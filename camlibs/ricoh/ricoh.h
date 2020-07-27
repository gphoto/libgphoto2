/* ricoh.h
 *
 * Copyright 2002 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __RICOH_H__
#define __RICOH_H__

#include <time.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-context.h>

enum _RicohSpeed {
	RICOH_SPEED_2400   = 0x00,
	RICOH_SPEED_4800   = 0x01,
	RICOH_SPEED_9600   = 0x02,
	RICOH_SPEED_19200  = 0x03,
	RICOH_SPEED_38400  = 0x04,
	RICOH_SPEED_57600  = 0x05,
	RICOH_SPEED_115200 = 0x07
};
typedef enum _RicohSpeed RicohSpeed;

int ricoh_set_speed (Camera *camera, GPContext *context, RicohSpeed speed);

/* We don't know the numbers for the models marked as 'dummy'. */
enum _RicohModel {
	RICOH_MODEL_1        = 0x001, /* dummy */
	RICOH_MODEL_2        = 0x002, /* dummy */
	RICOH_MODEL_2E       = 0x003, /* dummy */
	RICOH_MODEL_100G     = 0x005, /* dummy */
	RICOH_MODEL_300      = 0x300,
	RICOH_MODEL_300Z     = 0x301,
	RICOH_MODEL_4200     = 0x402,
	RICOH_MODEL_4300     = 0x403,
	RICOH_MODEL_5000     = 0x004, /* dummy */
	RICOH_MODEL_ESP2     = 0x006, /* dummy */
	RICOH_MODEL_ESP50    = 0x007, /* dummy */
	RICOH_MODEL_ESP60    = 0x008, /* dummy */
	RICOH_MODEL_ESP70    = 0x009, /* dummy */
	RICOH_MODEL_ESP80    = 0x010, /* dummy */
	RICOH_MODEL_ESP80SXG = 0x400
};
typedef enum _RicohModel RicohModel;

int ricoh_connect    (Camera *camera, GPContext *context, RicohModel *model);
int ricoh_disconnect (Camera *camera, GPContext *context);

enum _RicohMode {
	RICOH_MODE_PLAY   = 0x00,
	RICOH_MODE_RECORD = 0x01
};
typedef enum _RicohMode RicohMode;

int ricoh_get_mode  (Camera *camera, GPContext *context, RicohMode *mode);
int ricoh_set_mode  (Camera *camera, GPContext *context, RicohMode  mode);

int ricoh_get_num   (Camera *camera, GPContext *context, unsigned int *n);

int ricoh_get_pic_size  (Camera *, GPContext *, unsigned int, uint64_t *);
int ricoh_get_pic_date  (Camera *, GPContext *, unsigned int, time_t *);
int ricoh_get_pic_name  (Camera *, GPContext *, unsigned int, const char **);
int ricoh_get_pic_memo  (Camera *, GPContext *, unsigned int, const char **);
int ricoh_del_pic       (Camera *, GPContext *, unsigned int);

enum _RicohFileType {
	RICOH_FILE_TYPE_NORMAL  = 0xa0,
	RICOH_FILE_TYPE_PREVIEW = 0xa4
};
typedef enum _RicohFileType RicohFileType;
int ricoh_get_pic   (Camera *camera, GPContext *context, unsigned int n,
		     RicohFileType type,
		     unsigned char **data, unsigned int *size);

int ricoh_take_pic  (Camera *, GPContext *);
int ricoh_put_file  (Camera *, GPContext *, const char *name,
		     const unsigned char *data, unsigned int size);

int ricoh_get_date  (Camera *camera, GPContext *context, time_t *time);
int ricoh_set_date  (Camera *camera, GPContext *context, time_t  time);

int ricoh_get_cam_mem   (Camera *camera, GPContext *context, int *mem);
int ricoh_get_cam_amem  (Camera *camera, GPContext *context, int *mem);

int ricoh_get_copyright (Camera *camera, GPContext *context,
		         const char **copyright);
int ricoh_set_copyright (Camera *camera, GPContext *context,
			 const char *copyright);

enum _RicohResolution {
	RICOH_RESOLUTION_640_480  = 0x01,
	RICOH_RESOLUTION_1280_960 = 0x04
};
typedef enum _RicohResolution RicohResolution;

int ricoh_get_resolution (Camera *, GPContext *, RicohResolution *);
int ricoh_set_resolution (Camera *, GPContext *, RicohResolution);

enum _RicohExposure {
	RICOH_EXPOSURE_M20  = 0x01, /* -2.0 */
	RICOH_EXPOSURE_M15  = 0x02,
	RICOH_EXPOSURE_M10  = 0x03,
	RICOH_EXPOSURE_M05  = 0x04,
	RICOH_EXPOSURE_00   = 0x05,
	RICOH_EXPOSURE_05   = 0x06,
	RICOH_EXPOSURE_10   = 0x07,
	RICOH_EXPOSURE_15   = 0x08,
	RICOH_EXPOSURE_20   = 0x09, /* +2.0 */
	RICOH_EXPOSURE_AUTO = 0xff
};
typedef enum _RicohExposure RicohExposure;

int ricoh_get_exposure (Camera *, GPContext *, RicohExposure *);
int ricoh_set_exposure (Camera *, GPContext *, RicohExposure);

enum _RicohWhiteLevel {
	RICOH_WHITE_LEVEL_AUTO		= 0x00,
	RICOH_WHITE_LEVEL_OUTDOOR	= 0x01,
	RICOH_WHITE_LEVEL_FLUORESCENT	= 0x02,
	RICOH_WHITE_LEVEL_INCANDESCENT	= 0x03,
	RICOH_WHITE_LEVEL_BLACK_WHITE	= 0x04,
	RICOH_WHITE_LEVEL_SEPIA		= 0x05
};
typedef enum _RicohWhiteLevel RicohWhiteLevel;

int ricoh_get_white_level (Camera *, GPContext *, RicohWhiteLevel *);
int ricoh_set_white_level (Camera *, GPContext *, RicohWhiteLevel);

enum _RicohMacro {
	RICOH_MACRO_OFF = 0x00,
	RICOH_MACRO_ON  = 0x01
};
typedef enum _RicohMacro RicohMacro;

int ricoh_get_macro (Camera *, GPContext *, RicohMacro *);
int ricoh_set_macro (Camera *, GPContext *, RicohMacro);

enum _RicohZoom {
	RICOH_ZOOM_OFF = 0x00,
	RICOH_ZOOM_1   = 0x01,
	RICOH_ZOOM_2   = 0x02,
	RICOH_ZOOM_3   = 0x03,
	RICOH_ZOOM_4   = 0x04,
	RICOH_ZOOM_5   = 0x05,
	RICOH_ZOOM_6   = 0x06,
	RICOH_ZOOM_7   = 0x07,
	RICOH_ZOOM_8   = 0x08
};
typedef enum _RicohZoom RicohZoom;

int ricoh_get_zoom (Camera *, GPContext *, RicohZoom *);
int ricoh_set_zoom (Camera *, GPContext *, RicohZoom);

enum _RicohFlash {
	RICOH_FLASH_AUTO = 0x00,
	RICOH_FLASH_OFF  = 0x01,
	RICOH_FLASH_ON   = 0x02
};
typedef enum _RicohFlash RicohFlash;

int ricoh_get_flash (Camera *, GPContext *, RicohFlash *);
int ricoh_set_flash (Camera *, GPContext *, RicohFlash);

enum _RicohRecMode {
	RICOH_REC_MODE_IMAGE           = 0x00,
	RICOH_REC_MODE_CHARACTER       = 0x01,
	RICOH_REC_MODE_SOUND           = 0x03,
	RICOH_REC_MODE_IMAGE_SOUND     = 0x04,
	RICOH_REC_MODE_CHARACTER_SOUND = 0x06
};
typedef enum _RicohRecMode RicohRecMode;

int ricoh_get_rec_mode (Camera *, GPContext *, RicohRecMode *);
int ricoh_set_rec_mode (Camera *, GPContext *, RicohRecMode);

enum _RicohCompression {
	RICOH_COMPRESSION_NONE = 0x00,
	RICOH_COMPRESSION_MAX  = 0x01,
	RICOH_COMPRESSION_NORM = 0x02,
	RICOH_COMPRESSION_MIN  = 0x03
};
typedef enum _RicohCompression RicohCompression;

int ricoh_get_compression (Camera *, GPContext *, RicohCompression *);
int ricoh_set_compression (Camera *, GPContext *, RicohCompression);

#endif /* __RICOH_H__ */
