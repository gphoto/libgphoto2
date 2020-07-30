/* ADC-65(s) camera driver
 * Released under the GPL version 2
 *
 * Copyright 2001
 * Benjamin Moos
 * <benjamin@psnw.com>
 * http://www.psnw.com/~smokeserpent/code/
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-abilities-list.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define GP_MODULE "adc65"

#define ACK		0x15

#define ADC65_DATA_FIRMWARE	0
#define ADC65_DATA_THUMBNAIL	1
#define ADC65_DATA_PICTURE	2

/* colorspace structure */
typedef struct {
  unsigned char R;
  unsigned char G;
  unsigned char B;
} colorspaceRGB;

static int
adc65_exchange (Camera *camera, const unsigned char *cmd, int cmd_size, unsigned char *resp, int resp_size) {
	int ret;

	ret = gp_port_write (camera->port, (char*)cmd, cmd_size);
	if (ret < 0)
		return ret;
	return gp_port_read (camera->port, (char*)resp, resp_size);
}

static int
adc65_ping(Camera *camera) {
	unsigned char cmd, resp[3];
	int ret;

	GP_DEBUG("Pinging the camera.");

	cmd = 0x30;
	ret = adc65_exchange (camera, &cmd, 1, resp, 3);
	if ( ret < GP_OK)
		return ret;

	if (resp[1] != 0x30)
		return GP_ERROR;

	GP_DEBUG("Ping answered!");
	return GP_OK;
}

static int
adc65_file_count (Camera *camera) {
        unsigned char cmd, resp[65538];
	int ret;

        GP_DEBUG("Getting the number of pictures.");
        cmd = 0x00;
        ret = adc65_exchange(camera, &cmd, 1, resp, 65538);
	if (ret < 2) /* must have at least a 2 byte reply */
                return ret;
        return resp[1] - 1;
}

static char *
adc65_read_data (Camera *camera, unsigned char *cmd, int cmd_size, int data_type, int *size) {
	unsigned char resp[2], temp;
	int x, y, x1, y1, z;
	unsigned char ul, ur, bl, br, ret;
	colorspaceRGB rgb;
	unsigned char *us = NULL;
	char *s = NULL;
	char *ppmhead = "P6\n# test.ppm\n256 256\n255\n";

	switch (data_type) {
		case ADC65_DATA_PICTURE:
			GP_DEBUG("Getting Picture");

			/* get the picture */
			ret = adc65_exchange (camera, cmd, cmd_size, resp, 2);
			if (ret < 2)
			    return NULL;

			us = malloc (65536);
			if (!us)
				return NULL;
			if (gp_port_read (camera->port, (char*)us, 65536) < 0) {
				free(us);
				return NULL;
			}

			/* Turn right-side-up and invert*/
			for (x=0; x<32768; x++) {
				temp = us[x];
				us[x] = 255 - us[65535-x];
				us[65535-x] = 255 - temp;
			}
			s  = (char *)malloc (sizeof(char)*65536*3+strlen(ppmhead));
			/* Camera uses this color array (upside-down):
			 *		bgbgb
			 *		grgrg
			 *      bgbgb
			 */
			strcpy(s, ppmhead);
			z = strlen(s);

			for (y=0; y<256; y++) {
			  if (y == 255) {
				y1 = y - 1;
			  }else{
				y1 = y + 1;
			  }
			  for (x=0; x<256; x++) {
				if (x == 255) {
				  x1 = x - 1;
				}else{
				  x1 = x + 1;
				}
				ul = (unsigned char)us[y * 256 + x];
				bl = (unsigned char)us[y1 * 256 + x];
				ur = (unsigned char)us[y * 256 + x1];
				br = (unsigned char)us[y1 * 256 + x1];

				  switch ( ((y & 1) << 1) | (x & 1) ) {
				  case 0: /* even row, even col, B */
					rgb.R = br;
					rgb.G = (ur + bl) / 2;
					rgb.B = ul;
					break;
				  case 1: /* even row, odd col, G */
					rgb.R = bl;
					rgb.G = ul;
					rgb.B = ur;
					break;
				  case 2:	/* odd row, even col, G */
					rgb.R = ur;
					rgb.G = ul;
					rgb.B = bl;
					break;
				  case 3:	/* odd row, odd col, R */
				  default:
					rgb.R = ul;
					rgb.G = (ur + bl) / 2;
					rgb.B = br;
					break;
				  }
				  s[z++] = rgb.R;
				  s[z++] = rgb.G;
				  s[z++] = rgb.B;
				}
			}
			*size = z;
			GP_DEBUG("size=%i", *size);
			break;
		default:
			break;
	}
	free(us);
	return(s);
}

static char *
adc65_read_picture(Camera *camera, int picture_number, int *size) {
	unsigned char cmd = picture_number + 1;

	return (adc65_read_data(camera, &cmd, 1, ADC65_DATA_PICTURE, size));
}


int camera_id (CameraText *id) {
	strcpy(id->text, "adc65");
	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	/* Fill in the appropriate flags/data */
	strcpy(a.model, "Achiever Digital:Adc65");
	a.port      = GP_PORT_SERIAL;
	a.speed[0]  = 115200;
	a.speed[1]  = 230400;
	a.speed[2]  = 0;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_NONE;
	a.folder_operations = GP_FOLDER_OPERATION_NONE;
	return gp_abilities_list_append(list, a);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	return gp_list_populate (list, "adc65%02i.ppm", adc65_file_count((Camera*)data));
}

static int
get_file_func (	CameraFilesystem *fs, const char *folder,
		const char *filename, CameraFileType type,
		CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
        int size, num;
	char *data;

        gp_file_set_mime_type (file, GP_MIME_PPM);
        num = gp_filesystem_number (fs, folder, filename, context);
	if (num < 0)
		return num;
        data = adc65_read_picture (camera, num, &size);
        if (!data)
		return GP_ERROR;
	return gp_file_append (file,data,size);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context) {
        strcpy (about->text, _("Adc65\nBenjamin Moos <benjamin@psnw.com>"));
        return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func
};

int
camera_init(Camera *camera, GPContext *context) {
	int ret;
	gp_port_settings settings;

	camera->functions->about        = camera_about;
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	ret = gp_port_set_timeout (camera->port, 5000);
	if (ret < GP_OK)
		return ret;

	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < GP_OK)
		return ret;
	settings.serial.bits    = 8;
	settings.serial.parity  = 0;
	settings.serial.stopbits= 1;
	ret = gp_port_set_settings (camera->port, settings);
	if (ret < GP_OK)
		return ret;

	return adc65_ping(camera);
}
