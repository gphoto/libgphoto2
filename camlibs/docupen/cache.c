/* cache.c
 *
 * Copyright 2020 Ondrej Zary <ondrej@zary.sk>
 * based on Docupen tools by Florian Heinz <fh@cronon-ag.de>
 */
#include <gphoto2/gphoto2-library.h>
#include <gphoto2-endian.h>
#include "docupen.h"

static char cmd_datalen[]  = { 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a };
static char cmd_unknown[]  = { 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a };
static char cmd_get_all[]  = { 0x51, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x1a };

static bool fill_cache(Camera *camera)
{
	char *buf;
	int ret;
	size_t pktsize = camera->pl->info.packetsize[2] |
			 camera->pl->info.packetsize[1] << 8|
			 camera->pl->info.packetsize[0] << 16;

	buf = malloc(pktsize);
	if (!buf)
		return false;

	/* truncate file */
	fclose(camera->pl->cache);
	camera->pl->cache = fopen(camera->pl->cache_file, "w+");
	if (!camera->pl->cache) {
		perror("fopen");
		GP_LOG_E("unable to trunate cache file %s", camera->pl->cache_file);
		free(buf);
		return false;
	}

	dp_cmd(camera->port, cmd_unknown);
	dp_cmd(camera->port, cmd_get_all);

	uint32_t done = 0;
	while (done < camera->pl->datalen) {
		ret = gp_port_read(camera->port, buf, (camera->pl->datalen - done < pktsize) ? camera->pl->datalen-done : pktsize);
		if (ret < 0)
			break;
		fwrite(buf, 1, ret, camera->pl->cache);
		done += ret;
		if (ret < pktsize)
			break;
	}
	free(buf);
	return true;
}

bool dp_init_cache(Camera *camera)
{
	if (camera->pl->cache_file)
		return true;

	camera->pl->cache_file = malloc(64 + strlen(getenv("HOME")));
	if (!camera->pl->cache_file)
		return false;
	sprintf(camera->pl->cache_file, "%s/.cache", getenv("HOME"));

	if (!gp_system_is_dir(camera->pl->cache_file) && gp_system_mkdir(camera->pl->cache_file) != GP_OK) {
		GP_LOG_E("unable to create directory %s", camera->pl->cache_file);
		goto err_free;
	}

	sprintf(camera->pl->cache_file, "%s/.cache/docupen-%s.bin", getenv("HOME"), camera->pl->info.serialno);
	camera->pl->cache = fopen(camera->pl->cache_file, "a+");
	if (!camera->pl->cache) {
		perror("fopen");
		GP_LOG_E("unable to open cache file %s", camera->pl->cache_file);
		goto err_free;
	}

	dp_cmd(camera->port, cmd_datalen);
	gp_port_read(camera->port, (void *)&camera->pl->datalen, sizeof(camera->pl->datalen));
	camera->pl->datalen = le32toh(camera->pl->datalen);
	fseek(camera->pl->cache, 0, SEEK_END);
	if (ftell(camera->pl->cache) == camera->pl->datalen) {	/* cache is valid */
		if (!dp_init_calibration(camera, false))
			goto err_free;
		return true;
	}

	if (!dp_init_calibration(camera, true)) /* if cache is invalid, update LUT too */
		goto err_free;

	if (fill_cache(camera))
		return true;

err_free:
	free(camera->pl->cache_file);
	camera->pl->cache_file = NULL;
	return false;
}

bool dp_delete_cache(Camera *camera)
{
	if (camera->pl->cache)
		fclose(camera->pl->cache);
	camera->pl->cache = NULL;

	return !unlink(camera->pl->cache_file);
}
