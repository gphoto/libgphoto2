/* calibration.c
 *
 * Copyright 2020 Ondrej Zary <ondrej@zary.sk>
 */

#include "config.h"

#include <math.h>
#include <gphoto2/gphoto2-library.h>
#include "docupen.h"

#define NUM_POINTS 5
struct pixel_cal {
	unsigned char data[NUM_POINTS];
};

#define CAL_SIZE (1600 * 3)
#define LUT_FILE_DUMMY_END 0x800
#define LUT_FILE_SIZE (2 * (CAL_SIZE * NUM_POINTS + CAL_SIZE * 2 + CAL_SIZE * sizeof(struct lut)) + LUT_FILE_DUMMY_END + SERIALNO_LEN + 2)	/* 2526866 */

static char cmd_get_cal[]  = { 0x56, 0x00, 0x00, 0xc0, 0x5d, 0x00, 0x00, 0x1a };
static char cmd_get_cal1[] = { 0x56, 0x01, 0x00, 0xc0, 0x61, 0x00, 0x00, 0x1a };

/* VALUES (y) at which data points (x) are stored */
static unsigned char cal_points[] = { 0, 32, 38, 106, 192, 252, 255 };

static unsigned char empty[] = { 0x00, 0x00, 0x00, 0x00, 0x00 };

/* build look-up table (LUT) from calibration data */
static void make_lut(unsigned char lut[], unsigned char p[]) {
	unsigned char points[] = { 0, p[0], p[1], p[2], p[3], p[4], p[4] + 8 };
	float slope = (float)cal_points[1] / points[1];
	int cur_point = 0;
	for (unsigned int i = 0; i < sizeof(struct lut); i++) {
		unsigned int val = floorf((i - points[cur_point]) * slope + cal_points[cur_point]);
		lut[i] = (val <= 255) ? val : 255;
		if (lut[i] < 255 && lut[i] >= cal_points[cur_point + 1]) {
			cur_point++;
			slope = (float)(cal_points[cur_point + 1] - cal_points[cur_point]) / (points[cur_point + 1] - points[cur_point]);
		}
	}
}

static bool lut_from_cal(struct pixel_cal cal[], FILE *out) {
	/* input data */
	if (fwrite(cal, 1, CAL_SIZE * sizeof(struct pixel_cal), out) != CAL_SIZE * sizeof(struct pixel_cal))
		return false;

	/* fake ??? table */
	unsigned char table2[CAL_SIZE * 2];
	memset(table2, 0x00, sizeof(table2));
	table2[0] = 0x18;
	if (fwrite(table2, 1, sizeof(table2), out) != sizeof(table2))
		return false;

	/* lookup tables */
	for (int i = 0; i < CAL_SIZE; i++) {
		unsigned char lut[sizeof(struct lut)];
		if (i == 0) {
			memset(lut, 0xff, sizeof(lut));
			lut[0] = 0x18;
		} else if (memcmp(cal[i].data, empty, NUM_POINTS) == 0)
			memset(lut, 0xff, sizeof(lut));
		else
			make_lut(lut, cal[i].data);
		if (fwrite(lut, 1, sizeof(lut), out) != sizeof(lut))
			return false;
	}

	return true;
}

/* create a calibration LUT file, same format as PenLut.raw in Windows SW */
static bool make_lut_file(Camera *camera, FILE *lut)
{
	struct pixel_cal cal[CAL_SIZE];
	char dummy[0x400];

	/* first calibration data */
	dp_cmd(camera->port, cmd_get_cal);
	if (gp_port_read(camera->port, (void *)cal, sizeof(cal)) != sizeof(cal))
		return false;
	if (!lut_from_cal(cal, lut))
		return false;
	/* second calibration data */
	dp_cmd(camera->port, cmd_get_cal1);
	if (gp_port_read(camera->port, (void *)cal, sizeof(cal)) != sizeof(cal))
		return false;
	if (gp_port_read(camera->port, dummy, sizeof(dummy)) != sizeof(dummy))
		return false;
	if (!lut_from_cal(cal, lut))
		return false;
	/* 0x800 zero bytes (LUT_FILE_DUMMY_END) */
	memset(dummy, 0, sizeof(dummy));
	if (fwrite(dummy, 1, sizeof(dummy), lut) != sizeof(dummy))
		return false;
	if (fwrite(dummy, 1, sizeof(dummy), lut) != sizeof(dummy))
		return false;
	/* serial number */
	if (fwrite(camera->pl->info.serialno, 1, sizeof(camera->pl->info.serialno), lut) != sizeof(camera->pl->info.serialno))
		return false;
	/* 2 zero bytes */
	if (fwrite(dummy, 1, 2, lut) != 2)
		return false;

	return true;
}

bool dp_init_calibration(Camera *camera, bool force)
{
	char *lut_file;
	bool ret = false;
	int i, j;

	if (camera->pl->lut)
                return true;

	lut_file = malloc(64 + strlen(getenv("HOME")));
	if (!lut_file)
		return false;
	sprintf(lut_file, "%s/.cache/docupen-%s.lut", getenv("HOME"), camera->pl->info.serialno);

	FILE *lut = fopen(lut_file, "a+");
	if (!lut) {
		perror("fopen");
		GP_LOG_E("unable to open LUT file %s", lut_file);
		goto err_free;
	}
	fseek(lut, 0, SEEK_END);
	if (force || ftell(lut) != LUT_FILE_SIZE) {
		/* truncate file */
		fclose(lut);
		lut = fopen(lut_file, "w+");
		if (!lut) {
			perror("fopen");
			GP_LOG_E("unable to trunate cache file %s", lut_file);
			goto err_free;
		}
		if (!make_lut_file(camera, lut))
			goto err_close;
		fflush(lut);
	}
	/* read first LUT into memory, the 2nd one seems unused? */
	fseek(lut, CAL_SIZE * NUM_POINTS + CAL_SIZE * 2, SEEK_SET); /* 0x8340 */
	camera->pl->lut = malloc(CAL_SIZE * sizeof(struct lut));
	if (!camera->pl->lut)
		goto err_close;

	if (fread(camera->pl->lut, 1, CAL_SIZE * sizeof(struct lut), lut) != (CAL_SIZE * sizeof(struct lut))) {
		GP_LOG_E("error reading LUT from file");
		goto err_close;
	}
	/**
	 * There are empty (0xff) LUTs at the beginning and end (left and right edge of the scanner).
	 * Windows SW crops the image there. We can use full width of the scanner at the cost of
	 * edges not having properly calibrated colors.
	 * Find first and last non-empty LUTs and copy over the empty ones.
	 */
	for (i = 0; i < CAL_SIZE; i++)
		if (camera->pl->lut[i].data[1] != 0xff)	/* [1] because first byte of first LUT is always 0x18 */
			break;
	for (j = 0; j < i; j += 3)	/* copy in 3-LUT (RGB) chunks */
		memcpy(camera->pl->lut[j].data, camera->pl->lut[i].data, 3 * sizeof(struct lut));

	for (i = CAL_SIZE - 1; i >= 0; i--)
		if (camera->pl->lut[i].data[1] != 0xff)
			break;
	i -= 2;
	for (j = i + 3; j < CAL_SIZE; j += 3) /* copy in 3-LUT (RGB) chunks */
		memcpy(camera->pl->lut[j].data, camera->pl->lut[i].data, 3 * sizeof(struct lut));

	ret = true;

err_close:
	fclose(lut);
err_free:
	free(lut_file);
	return ret;
}
