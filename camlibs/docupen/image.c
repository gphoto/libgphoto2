/* image.c
 *
 * Copyright 2020 Ondrej Zary <ondrej@zary.sk>
 * based on Docupen tools by Florian Heinz <fh@cronon-ag.de>
 */

#include "config.h"

#include <gphoto2/gphoto2-library.h>
#include <libgphoto2/gphoto2-endian.h>
#include "docupen.h"
#include "huffman.h"

gdImagePtr dp_get_image_mono(struct dp_imagehdr *dp, void *data)
{
	gdImagePtr img, fixed;
	int black, white;
	int type, len;
	int x = 0, y = 0;
	struct decoder decoder;
	int palette[256];
	unsigned int tmp = 0;
	int nr_marks = 0, mark = 0, offset = 0, last_mark = 0;
	int dist = dp->dpi == RES_400DPI ? 26 : 13;
	int out_width = dp->dpi == RES_400DPI ? 3072 : 1536;

	img = gdImageCreate(le16toh(dp->sizex), le16toh(dp->sizey));
	if (!img)
		return NULL;

	for (int i = 0; i <= 255; i++)
		palette[i] = gdImageColorAllocate(img, i, i, i);
	black = palette[0];
	white = palette[255];

	decoder_init(&decoder, data, le32toh(dp->payloadlen));
	while (decoder_token(&decoder, &type, &len) >= 0) {
		switch (type) {
		case DECODER_EOL:
			if (y > 0 && gdImagePalettePixel(img, 0, y) > 0xf0) {
				nr_marks++;
				last_mark = y;
			}
			y++;
			x = 0;
			tmp = 0;
			break;
		case DECODER_BLACK:
		case DECODER_WHITE:
			while (len && (x++ < le16toh(dp->sizex))) {
				/* decode first 32 bits(pixels) as four 8bpp pixels */
				if (x > le16toh(dp->sizex) - 32)
					if (type == DECODER_WHITE)
						tmp |= (1 << (le16toh(dp->sizex) - x));
				if (x == le16toh(dp->sizex)) {
					gdImageSetPixel(img, 0, y, tmp & 0xff);
					gdImageSetPixel(img, 1, y, (tmp >> 8) & 0xff);
					gdImageSetPixel(img, 2, y, (tmp >> 16) & 0xff);
					gdImageSetPixel(img, 3, y, (tmp >> 24) & 0xff);
				}
				if (x <= le16toh(dp->sizex) - 32)
					gdImageSetPixel(img, le16toh(dp->sizex) - x, y, (type == DECODER_WHITE) ? white : black);
				len--;
			}
			break;
		}
		if (y >= le16toh(dp->sizey))
			break;
	}
	/* don't lose the image bottom below the last mark */
	if (last_mark < le16toh(dp->sizey) - 1) {
		nr_marks++;
		gdImageSetPixel(img, 0, le16toh(dp->sizey) - 1, 0xff);
	}

	fixed = gdImageCreate(out_width, nr_marks * dist);
	if (!fixed) {
		gdImageDestroy(img);
		return NULL;
	}
	black = gdImageColorAllocate(fixed, 0, 0, 0);
	white = gdImageColorAllocate(fixed, 255, 255, 255);

	for (y = 0; y < img->sy; y++) {
		if (y > 0 && gdImagePalettePixel(img, 0, y) > 0xf0) {
			gdImageCopyResampled(fixed, img, 0, offset, 32, mark, out_width, dist, 1536, y - mark);
			mark = y;
			offset += dist;
		}
	}

	gdImageDestroy(img);

	return fixed;
}

struct line_grey4 {
	unsigned char val[800];
};

struct line_grey8 {
	unsigned char val[1600];
};

gdImagePtr dp_get_image_grey(struct dp_imagehdr *dp, void *data, struct lut *lut)
{
	gdImagePtr img, fixed;
	int palette[256], palette2[256];
	int i, j, odd, val;
	struct line_grey4 *line4 = data;
	struct line_grey8 *line8 = data;
	int nr_marks = 0, mark = 0, offset = 0, last_mark = 0;
	int pos_x;
	int dist = dp->dpi == RES_400DPI ? 26 : 13;
	int out_width = dp->dpi == RES_400DPI ? 3180 : 1590;

	img = gdImageCreate(le16toh(dp->sizex), le16toh(dp->sizey));
	if (!img)
		return NULL;
	for (i = 0; i <= 255; i++)
		palette[i] = gdImageColorAllocate(img, i, i, i);

	for (i = 0; i < le16toh(dp->sizey); i++) {
		odd = 0;
		for (j = 0; j < le16toh(dp->sizex); j++) {
			if (le16toh(dp->type) == TYPE_GREY4) {
				if (odd)
					val = line4->val[j >> 1] & 0xf0;
				else
					val = (line4->val[j >> 1] & 0x0f) << 4;
				odd = !odd;
			} else
				val = line8->val[j];
			pos_x = le16toh(dp->sizex) - j - 1;
			if (pos_x != 1599)
				val = lut[pos_x * 3 + 2].data[val]; /* use red LUT */
			gdImageSetPixel(img, pos_x, i, palette[val]);
		}
		if (i > 0 && gdImagePalettePixel(img, 1599, i) < 0xf0) {
			nr_marks++;
			last_mark = i;
		}
		line4++;
		line8++;
	}
	/* don't lose the image bottom below the last mark */
	if (last_mark < le16toh(dp->sizey) - 1) {
		nr_marks++;
		gdImageSetPixel(img, 1599, le16toh(dp->sizey) - 1, 0x80);
	}

	fixed = gdImageCreate(out_width, nr_marks * dist);
	if (!fixed) {
		gdImageDestroy(img);
		return NULL;
	}
	for (i = 0; i <= 255; i++)
		palette2[i] = gdImageColorAllocate(fixed, i, i, i);

	for (i = 0; i < img->sy; i++) {
		if (i > 0 && gdImagePalettePixel(img, 1599, i) < 0xf0) {
			gdImageCopyResampled(fixed, img, 0, offset, 0, mark, out_width, dist, 1590, i - mark);
			mark = i;
			offset += dist;
		}
	}

	gdImageDestroy(img);

	return fixed;
}

struct line12 {
	unsigned char r[800];
	unsigned char g[800];
	unsigned char b[800];
};

struct line24 {
	unsigned char r[1600];
	unsigned char g[1600];
	unsigned char b[1600];
};

gdImagePtr dp_get_image_color(struct dp_imagehdr *dp, void *data, struct lut *lut)
{
	gdImagePtr img, fixed;
	int i, j, odd;
	unsigned char r, g, b;
	struct line12 *line12 = data;
	struct line24 *line24 = data;
	int nr_marks = 0, mark = 0, offset = 0, last_mark = 0;
	int pos_x;
	int dist = dp->dpi == RES_400DPI ? 26 : 13;
	int out_width = dp->dpi == RES_400DPI ? 3180 : 1590;

	if ((le16toh(dp->sizex) <= 0) || (le16toh(dp->sizey) <= 0) ||
		 ((le16toh(dp->sizex) * le16toh(dp->sizey) * 3) / (le16toh(dp->type) == TYPE_COLOR12 ? 2 : 1) > le32toh(dp->payloadlen)))
		return NULL;

	img = gdImageCreateTrueColor(le16toh(dp->sizex), le16toh(dp->sizey));
	if (!img)
		return NULL;

	for (i = 0; i < le16toh(dp->sizey); i++) {
		odd = 0;
		for (j = 0; j < le16toh(dp->sizex); j++) {
			if (le16toh(dp->type) == TYPE_COLOR12) {
				if (odd) {
					r = line12->r[j >> 1] & 0xf0;
					g = line12->g[j >> 1] & 0xf0;
					b = line12->b[j >> 1] & 0xf0;
				} else {
					r = (line12->r[j >> 1] & 0x0f) << 4;
					g = (line12->g[j >> 1] & 0x0f) << 4;
					b = (line12->b[j >> 1] & 0x0f) << 4;
				}
				odd = !odd;
			} else {
				r = line24->r[j];
				g = line24->g[j];
				b = line24->b[j];
			}
			pos_x = le16toh(dp->sizex) - j - 1;
			if (pos_x != 1599) {
				r = lut[pos_x * 3 + 2].data[r];
				g = lut[pos_x * 3 + 1].data[g];
				b = lut[pos_x * 3 + 0].data[b];
			}
			gdImageSetPixel(img, pos_x, i, (r << 16) | (g << 8) | b);
		}
		if (gdTrueColorGetRed(gdImageTrueColorPixel(img, 1599, i)) < 0xf0) {
			nr_marks++;
			last_mark = i;
		}
		line12++;
		line24++;
	}
	/* don't lose the image bottom below the last mark */
	if (last_mark < le16toh(dp->sizey) - 1) {
		nr_marks++;
		gdImageSetPixel(img, 1599, le16toh(dp->sizey) - 1, 0x80 << 16);
	}

	fixed = gdImageCreateTrueColor(out_width, nr_marks * dist);
	if (!fixed) {
		gdImageDestroy(img);
		return NULL;
	}

	for (i = 0; i < img->sy; i++) {
		if (gdTrueColorGetRed(gdImageTrueColorPixel(img, 1599, i)) < 0xf0) {
			gdImageCopyResampled(fixed, img, 0, offset, 0, mark, out_width, dist, 1590, i - mark);
			mark = i;
			offset += dist;
		}
	}

	gdImageDestroy(img);

	return fixed;
}
