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
#include <gphoto2.h>

#include "adc65.h"

/* colorspace structure */
typedef struct {
  unsigned char R;
  unsigned char G;
  unsigned char B;
} colorspaceRGB;

#define GP_MODULE "adc65"

static
int adc65_exchange (Camera *camera, char *cmd, int cmd_size, char *resp, int resp_size) {
	int ret;

	ret = gp_port_write (camera->port, cmd, cmd_size);
	if (ret < 0)
		return ret;
	return gp_port_read (camera->port, resp, resp_size);
}

int
adc65_ping(Camera *camera) {
	char cmd, resp[3];
	int ret;

	GP_LOG(GP_LOG_DEBUG, "Pinging the camera.\n");

	cmd = 0x30;
	ret = adc65_exchange (camera, &cmd, 1, resp, 3);
	if ( ret < GP_OK)
		return ret;

	if (resp[1] != 0x30)
		return GP_ERROR;

	GP_LOG (GP_LOG_DEBUG, "Ping answered!\n");
	return GP_OK;
}

int
adc65_file_count (Camera *camera) {
        char cmd, resp[65538];
	int ret;

        GP_LOG (GP_LOG_DEBUG, "Getting the number of pictures.\n");
        cmd = 0x00;
        ret = adc65_exchange(camera, &cmd, 1, resp, 65538);
	if (ret < 2) /* must have at least a 2 byte reply */
                return ret;
        return resp[1] - 1;
}

static char *
adc65_read_data (Camera *camera, char *cmd, int cmd_size, int data_type, int *size) {
	char resp[2], temp;
	int x, y, x1, y1, z;
	unsigned char ul, ur, bl, br, ret;
	colorspaceRGB rgb;
	char *us = NULL;
	char *s = NULL;
	char *ppmhead = "P6\n# test.ppm\n256 256\n255\n";

	switch (data_type) {
		case ADC65_DATA_PICTURE:
			GP_LOG (GP_LOG_DEBUG, "Getting Picture\n");

			/* get the picture */
			ret = adc65_exchange (camera, cmd, cmd_size, resp, 2);
			if (ret < 2)
			    return NULL;

			us = (char *)malloc (65536);
			if (!us)
				return NULL;
			if (gp_port_read (camera->port, us, 65536) < 0) {
				free(us);
				return NULL;
			}

			/* Turn right-side-up and invert*/
			for (x=0; x<32768; x++) {
			    temp = us[x];
				us[x] = 255 - us[65536-x];
				us[65536-x] = 255 - temp;
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
			GP_LOG(GP_LOG_DEBUG, "size=%i\n", *size);
			break;
		default:
			break;
	}
	free(us);
	return(s);
}

char *
adc65_read_picture(Camera *camera, int picture_number, int *size) {
	char cmd = picture_number + 1;

	return (adc65_read_data(camera, &cmd, 1, ADC65_DATA_PICTURE, size));
}

