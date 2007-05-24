#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bayer.h>
#include <gphoto2/gphoto2.h>
#include "barbie.h"

#define GP_MODULE "barbie"

/* Some simple packet templates */
static const char packet_1[4]                = {0x02, 0x01, 0x01, 0x03};
static char *barbie_read_data (GPPort *port, char *cmd, int cmd_size, int data_type, int *size);

/* Utility Functions
   =======================================================================
*/

static int
barbie_write_command(GPPort *port, char *command, int size) {
	int x;

	x = gp_port_write(port, command, size);
	return (x >= GP_OK);
}

static int
barbie_read_response(GPPort *port, char *response, int size) {

	int x;
	char ack = 0;

	/* Read the ACK */
	x=gp_port_read(port, &ack, 1);

	if ((ack != ACK)||(x<0))
		return (0);

	/* Read the Response */
	memset(response, 0, size);
	x=gp_port_read(port,response, size);
	return (x > 0);
}

static
int barbie_exchange (GPPort *port, char *cmd, int cmd_size, char *resp, int resp_size) {

	int count = 0;

	while (count++ < 10) {
		if (barbie_write_command(port, cmd, cmd_size) != 1)
			return (0);
		if (barbie_read_response(port, resp, resp_size) != 1)
			return (0);
		/* if it's not busy, return */
		if (resp[RESPONSE_BYTE] != '!')
			return (1);
		/* if busy, sleep 2 seconds */
		GP_SYSTEM_SLEEP(2000);
	}

	return (0);
}

int barbie_ping(GPPort *port) {
	char cmd[4], resp[4];

	GP_DEBUG( "Pinging the camera...");

	memcpy(cmd, packet_1, 4);
	cmd[COMMAND_BYTE] = 'E';
	cmd[DATA1_BYTE]   = 'x';

	if (barbie_exchange(port, cmd, 4, resp, 4) == 0)
		return (0);

	if (resp[DATA1_BYTE] != 'x')
		return (0);
	GP_DEBUG ("Ping answered!");
	return (1);
}

int barbie_file_count (GPPort *port) {
        char cmd[4], resp[4];

        GP_DEBUG ("Getting the number of pictures...");

        memcpy(cmd, packet_1, 4);

        cmd[COMMAND_BYTE] = 'I';
        cmd[DATA1_BYTE]   = 0;

        if (barbie_exchange(port, cmd, 4, resp, 4) != 1)
                return (0);

        return (resp[DATA1_BYTE]);
}

char *barbie_read_firmware(GPPort *port) {

	char cmd[4];
	int x;
	
	memcpy(cmd, packet_1, 4);
	cmd[COMMAND_BYTE] = 'V';
	cmd[DATA1_BYTE]   = '0';
	
	return (barbie_read_data(port, cmd, 4, BARBIE_DATA_FIRMWARE, &x));
}

char *barbie_read_picture(GPPort *port, int picture_number, int get_thumbnail, int *size) {

	char cmd[4], resp[4];

	memcpy(cmd, packet_1, 4);
	cmd[COMMAND_BYTE] = 'A';
	cmd[DATA1_BYTE]   = picture_number;

	if (barbie_exchange(port, cmd, 4, resp, 4) != 1)
		return (NULL);
	
	memcpy(cmd, packet_1, 4);
	if (get_thumbnail)
		cmd[COMMAND_BYTE] = 'M';
	   else
		cmd[COMMAND_BYTE] = 'U';

	cmd[DATA1_BYTE] = 0;

	return (barbie_read_data(port, cmd, 4, BARBIE_DATA_PICTURE, size));
}

char *barbie_read_data (GPPort *port, char *cmd, int cmd_size, int data_type, int *size) {
	char c, resp[4];
	int cols, visrows, blackrows, statusbytes;
	int x, y, z;
	char *s = NULL, *us = NULL, *rg = NULL, *t = NULL;
	char ppmhead[64];

	if (barbie_exchange(port, cmd, cmd_size, resp, 4) != 1)
		return (0);
	switch (data_type) {
		case BARBIE_DATA_FIRMWARE:
			GP_DEBUG ("Getting Firmware...");
			/* we're getting the firmware revision */
			*size = resp[2];
			s = (char *)malloc(sizeof(char)*(*size));
			memset(s, 0, *size);
			s[0] = resp[3];
			if (gp_port_read(port, &s[1], (*size)-1) < 0) {
				free(s);
				return (NULL);
			}
			break;
		case BARBIE_DATA_PICTURE:
			GP_DEBUG( "Getting Picture...");
			/* we're getting a picture */
			cols = (unsigned char)resp[2];
			blackrows = (unsigned char)resp[3];
			if (gp_port_read(port, &c, 1) < 0)
				return (NULL);
			visrows = (unsigned char)c;
			if (gp_port_read(port, &c, 1) < 0)
				return (NULL);
			statusbytes = (unsigned char)c;
			*size = PICTURE_SIZE(cols, blackrows, visrows, statusbytes);
			sprintf(ppmhead, "P6\n# test.ppm\n%i %i\n255\n", cols-4, visrows);
printf("\tcols=%i blackrows=%i visrows=%i statusbytes=%i size=%i\n", cols, blackrows ,visrows, statusbytes, *size);
			us = (char *)malloc(sizeof(char)*(*size));
			rg = (char *)malloc(sizeof(char)*(*size));
			s  = (char *)malloc(sizeof(char)*cols*(blackrows+visrows)*3+strlen(ppmhead));
			t  = (char *)malloc(sizeof(char)*(cols-4)*visrows*3+strlen(ppmhead));
			memset(us, 0, *size);
			memset(rg, 0, *size);
			memset(s , 0, *size+strlen(ppmhead));
			memset(t , 0, *size+strlen(ppmhead));
			if (gp_port_read(port, us, *size)<0) {
				free(us);
				free(rg);
				free(s);
				return (NULL);
			}
			/* Unshuffle the data */
			*size = *size - 16;
			for (x=0; x<blackrows+visrows; x++) {
				for (y=0; y<cols-4; y++) {
					z = x*cols + y/2 + (y%2)*(cols/2+2);
					rg[x*cols+(y^1)] = us[z];
				}
			}
			free (us);
			strcpy (t, ppmhead);
			z = strlen(t);
			/* Camera uses a Bayer CCD array:
			 *		gb  gb   ...
			 *		rg  rg   ...
			 */
			gp_bayer_decode (rg, cols, blackrows+visrows, s+z, BAYER_TILE_GBRG);
			free (rg);
			/* Shrink it a bit to avoid junk cols and black rows */
			for (x=0; x<visrows; x++)
				memcpy (t+z+x*(cols-4)*3,s+z+cols*(x+blackrows)*3,(cols-4)*3);
			z+= visrows*(cols-4)*3;
			*size = z;
			memcpy(s,t,z);
			free (t);
			GP_DEBUG( "size=%i", *size);
			break;
		case BARBIE_DATA_THUMBNAIL:
			break;
		default:
			break;
	}
	/* read the footer */
	if (gp_port_read(port, &c, 1) < 0) {
		free(s);
		return (0);
	}
	return(s);
}
