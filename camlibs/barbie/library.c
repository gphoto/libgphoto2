#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>
#include "barbie.h"

#define GP_MODULE "barbie"

/* packet headers/footers */
char packet_header[3]           = {0x02, 0x01};
char packet_header_data[6]      = {0x02, 0x01, 0x01, 0x01, 0x01}; 
char packet_header_firmware[4]  = {0x02, 'V', 0x01};
char packet_footer[2]           = {0x03};

/* Some simple packet templates */
char packet_1[4]                = {0x02, 0x01, 0x01, 0x03};
char packet_2[5]                = {0x02, 0x01, 0x01, 0x01, 0x03};


/* Utility Functions
   =======================================================================
*/

void barbie_packet_dump(GPPort *port, int direction, char *buf, int size) {
	int x;

	if (direction == 0)
		GP_DEBUG( "\tRead  Packet (%i): ", size);
	   else
		GP_DEBUG( "\tWrite Packet (%i): ", size);
	for (x=0; x<size; x++) {
		if (isalpha(buf[x]))
			GP_DEBUG ("[ '%c' ] ", (unsigned char)buf[x]);
		   else
			GP_DEBUG ("[ x%02x ] ", (unsigned char)buf[x]);
	}
	GP_DEBUG ("\n");
}

int barbie_write_command(GPPort *port, char *command, int size) {

	int x;

	barbie_packet_dump(port, 1, command, size);
	x=gp_port_write(port, command, size);
	return (x == GP_OK);
}

int barbie_read_response(GPPort *port, char *response, int size) {

	int x;
	char ack = 0;

	/* Read the ACK */
	x=gp_port_read(port, &ack, 1);
	barbie_packet_dump(port, 0, &ack, 1);

	if ((ack != ACK)||(x<0))
		return (0);

	/* Read the Response */
	memset(response, 0, size);
	x=gp_port_read(port,response, size);
	barbie_packet_dump(port, 0, response, x);
	return (x > 0);
}

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

	GP_DEBUG( "Pinging the camera\n");

	memcpy(cmd, packet_1, 4);
	cmd[COMMAND_BYTE] = 'E';
	cmd[DATA1_BYTE]   = 'x';

	if (barbie_exchange(port, cmd, 4, resp, 4) == 0)
		return (0);

	if (resp[DATA1_BYTE] != 'x')
		return (0);
	GP_DEBUG ("Ping answered!\n");
	return (1);
}

int barbie_file_count (GPPort *port) {

        char cmd[4], resp[4];

        GP_DEBUG ("Getting the number of pictures\n");

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
	int n1, n2, n3, n4, x, y, z;
	unsigned char r, g, b;
	char *s = NULL, *us = NULL, *rg = NULL;
	char *ppmhead_t = "P6\n# test.ppm\n%i %i\n255\n";
	char ppmhead[64];

	if (barbie_exchange(port, cmd, cmd_size, resp, 4) != 1)
		return (0);
	switch (data_type) {
		case BARBIE_DATA_FIRMWARE:
			GP_DEBUG ("Getting Firmware\n");
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
			GP_DEBUG( "Getting Picture\n");
			/* we're getting a picture */
			n1 = (unsigned char)resp[2];
			n2 = (unsigned char)resp[3];
			if (gp_port_read(port, &c, 1) < 0)
				return (NULL);
			n3 = (unsigned char)c;
			if (gp_port_read(port, &c, 1) < 0)
				return (NULL);
			n4 = (unsigned char)c;
			*size = PICTURE_SIZE(n1, n2, n3, n4);
printf("\tn1=%i n2=%i n3=%i n4=%i size=%i\n", n1, n2 ,n3, n4, *size);
			sprintf(ppmhead, ppmhead_t, n1-1, (n2+n3-1));
			us = (char *)malloc(sizeof(char)*(*size));
			rg = (char *)malloc(sizeof(char)*(*size));
			s  = (char *)malloc(sizeof(char)*(n1-1)*(n2+n3-1)*3+strlen(ppmhead));
			memset(us, 0, *size);
			memset(rg, 0, *size);
			memset(s , 0, *size+strlen(ppmhead));
			if (gp_port_read(port, us, *size)<0) {
				free(us);
				free(rg);
				free(s);
				return (NULL);
			}
			/* Unshuffle the data */
			*size = *size - 16;
			for (x=0; x<(n2+n3); x++) {
				for (y=0; y<n1; y++) {
					z = x*n1 + y/2 + y%2*(n1/2+2);
					rg[x*n1+y] = us[z];
				}
			}
			/* Camera uses Bayen array:
			 *		bg  bg   ...
			 *		gr  gr   ...
			 */
			strcpy(s, ppmhead);
			z = strlen(s);
			for (x=0; x<(n2+n3-1); x++) {
				for (y=0; y<(n1-1); y++) {
					b = (unsigned char)rg[x*n1+y];
					g = (((unsigned char)rg[(x+1)*n1+y] + 
					      (unsigned char)rg[x*n1+y+1]) / 2);
					r = (unsigned char)rg[(x+1)*n1+y+1];
					s[z++] = r;
					s[z++] = g;
					s[z++] = b;
				}
			}
			*size = z;
			GP_DEBUG( "size=%i\n", *size);
			break;
		case BARBIE_DATA_THUMBNAIL:
			break;
		default:
			break;
	}
	/* read the footer */
	if (gp_port_read(port, &c, 1) < 0) {
		free(us);
		free(rg);
		free(s);
		return (0);
	}
	free(us);
	free(rg);
	return(s);
}
