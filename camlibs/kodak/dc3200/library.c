/* library.c
 *
 * Copyright (C) 2000,2001,2002 donn morrison - dmorriso@gulf.uvic.ca
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

/****************************************************
 * kodak dc3200 digital camera driver library       *
 * for gphoto2                                      *
 *                                                  *
 * author: donn morrison - dmorriso@gulf.uvic.ca    *
 * date: dec 2000 - jan 2002                        *
 * license: gpl                                     *
 * version: 1.6                                     *
 *                                                  *
 ****************************************************/

#include "config.h"
#include "library.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "dc3200.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

/*
 * FIXME: Use properly sized integer types. The dc3200 code used lots
 *        of u_char, u_long, u_int, which may be wrong on non-32bit systems.
 */

/* #define DEBUG */

/*
 * dc3200_set_speed
 *
 * handshakes a baud rate with the camera
 *
 */
int dc3200_set_speed(Camera *camera, int baudrate)
{
	int baudcode = C9600;
	unsigned char resp[INI_PACKET_LEN], msg[INI_PACKET_LEN];
	int resp_len = INI_PACKET_LEN, msg_len = INI_PACKET_LEN;

	msg[0] = 0xAF;
	msg[1] = 0x00;

	switch(baudrate) {
	case 9600:
		baudcode = C9600;
		break;
	case 19200:
		baudcode = C19200;
		break;
	case 38400:
		baudcode = C38400;
		break;
	case 57600:
		baudcode = C57600;
		break;
	case 115200:
		baudcode = C115200;
		break;
	default:
		printf("unsupported baudrate: %d.\n", baudrate);
		return GP_ERROR;
	}

	msg[2] = baudcode;

	/* send our preferred baud rate */
	if(dc3200_send_command(camera, msg, msg_len, resp, &resp_len) == GP_ERROR)
		return GP_ERROR;
	/* the camera responds with our preferred baud rate or its
	 * maximum baud rate, whichever is lowest
	 */

	/* compile a confirmation packet */
	msg[0] = 0x9F;
	msg[1] = 0x00;
	if(resp_len == 2) {
		msg_len = 2;
	} else {
		/* calculate reply baud handshake */
		msg[2] = (resp[2] + 1) / 2;
		msg_len = INI_PACKET_LEN;
	}

	if(dc3200_send_command(camera, msg, msg_len, resp, &resp_len) == GP_ERROR)
		return GP_ERROR;

	if(resp_len == 2) {
		/* set the handshaked baudrate
		 * supports only 9600
		 */
		return C9600;
	} else if(resp[1] == 0x01) {
		/* set the handshaked baudrate */
		return (resp[2]*2) - 1;
	} else {
		/* unknown baud */
		return GP_ERROR;
	}

	return GP_ERROR;
}

/*
 * dc3200_setup
 *
 * sends two commands essential for survival
 *
 */
int dc3200_setup(Camera *camera)
{
	/*
	 * some strange commands that must be
	 * completed after initialization and
	 * before any camera access
	 *
	 * version info? ...
	 *
	 */

	unsigned char cmd1[5] = {0x01, 0, 0x01, 0x00, 0x0F};
	unsigned char cmd2[8] = {0x01, 0, 0x80, 0x00, 0x01, 0x81, 0x00, 0x03};

	unsigned char ack[ACK_PACKET_LEN], resp[DEF_PACKET_LEN];
	int ack_len = ACK_PACKET_LEN, resp_len = DEF_PACKET_LEN;

	cmd1[1] = dc3200_calc_seqnum(camera);
	cmd2[1] = dc3200_calc_seqnum(camera);

	if(dc3200_send_command(camera, cmd1, sizeof(cmd1), ack, &ack_len) == GP_ERROR)
		return GP_ERROR;
	if(dc3200_check_ack(camera, ack, ack_len) == GP_ERROR)
		return GP_ERROR;
	if(dc3200_recv_response(camera, resp, &resp_len) == GP_ERROR)
		return GP_ERROR;
	if(dc3200_send_ack(camera, resp[1]) == GP_ERROR)
		return GP_ERROR;

	if(dc3200_send_command(camera, cmd2, sizeof(cmd2), ack, &ack_len) == GP_ERROR)
		return GP_ERROR;
	if(dc3200_check_ack(camera, ack, ack_len) == GP_ERROR)
		return GP_ERROR;
	if(dc3200_recv_response(camera, resp, &resp_len) == GP_ERROR)
		return GP_ERROR;
	if(dc3200_send_ack(camera, resp[1]) == GP_ERROR)
		return GP_ERROR;

	/* set the cmd sequence number */
	camera->pl->cmd_seqnum = 0;
	camera->pl->rec_seqnum = 0;

	return GP_OK;
}


/*
 * dc3200_get_data
 *
 * retrieves data from the camera as either file
 * data or file/folder list data
 *
 */

	/* command to retrieve directory listing
	unsigned char list_pkt[DEF_PACKET_LEN] = {0x01, dc3200_calc_seqnum(camera), 0x80, 0x00, 0x20, 0x03, 26 + strlen(filename),
							0xc1, 0x50, 0x00, 0x00, 0x00, 0x00, 19 + strlen(filename),
							0x04, 0x01, 0x00, 0x01, 0x00, 0x01, 0x60, 0x01, 0x00, 0x01, 0x00, 0x05,
							0x00, 0x00, 0x00, 2 + strlen(filename), 0x01};
	*/
	/* command to get the picture
	unsigned char get_pkt[DEF_PACKET_LEN] = {0x01, calc_seqnum(), 0x80, 0x00, 0x20, 0x03, 26 + strlen(filename),
	                        0xC1, 0x50, 0x00, 0x00, 0x00, 0x00, 19 + strlen(filename),
							0x04, 0x01,	0x00, 0x01, 0x01, 0x04, 0x60, 0x10, 0x00, 0x01, 0x00, 0x05,
							0x00, 0x00, 0x00, 2 + strlen(filename), 0x01};
	*/

int dc3200_get_data(Camera *camera, unsigned char **data, unsigned long *data_len, int command, const char *folder, const char *filename)
{
	unsigned char ack[ACK_PACKET_LEN], resp[DEF_PACKET_LEN];
	int ack_len = ACK_PACKET_LEN, resp_len = DEF_PACKET_LEN;
	unsigned long num_left = 0;
	int data_start_pos = 0;
	unsigned char *ptr_data = NULL;
	unsigned int pid = 0;

	/* command bytes */
	unsigned char cb1=0x00,cb2=0x00,cb3=0x00;

	/* packet parts */
	unsigned char pkt_hdr[7];
	unsigned char pkt_typ[7];
	unsigned char pkt_cmd[12];
	unsigned char pkt_len[5];

	unsigned char *packet;
	int packet_len;
	char *file = NULL;
	char *ptr = NULL;

	if(folder) {
		if(filename) {
			file = malloc(strlen(folder) + strlen(filename) + 2);
			if(!file)
				return GP_ERROR;

			/* concatenate the folder + filename */
			strncpy(file, folder, strlen(folder));
			file[strlen(folder)] = 0;
			/* append the filename */
			if(folder[strlen(folder)-1] != '\\')
			strcat(file, "\\");
			strncat(file, filename, strlen(filename));

		} else {
			file = malloc(strlen(folder) + 2);
			if(!file)
				return GP_ERROR;

			strncpy(file, folder, strlen(folder));
			file[strlen(folder)] = 0;
		}
	} else {
		return GP_ERROR;
	}

	/* change all /'s to \'s */
	while((ptr = strchr(file, '/'))) {
		ptr[0] = '\\';
	}

	/* there should not be a trailing '\', except for root folder */
	if(strlen(file) > 1 && file[strlen(file)-1] == '\\')
		file[strlen(file)-1] = 0;

	switch (command) {
	case CMD_GET_PREVIEW:
		cb1 = 0x02; cb2 = 0x70; cb3 = 0x11;
		break;
	case CMD_GET_FILE:
		cb1 = 0x01; cb2 = 0x60; cb3 = 0x10;
		break;
	case CMD_LIST_FILES:
		cb1 = 0x01; cb2 = 0x60; cb3 = 0x01;
		break;
	}

	camera->pl->cmd_seqnum++;

	pkt_hdr[0] = 0x01;
	pkt_hdr[1] = dc3200_calc_seqnum(camera);
	pkt_hdr[2] = 0x80; pkt_hdr[3] = 0x00; pkt_hdr[4] = 0x20; pkt_hdr[5] = 0x03;
	pkt_hdr[6] = 26 + strlen(file);

	pkt_typ[0] = 0xC1;
	pkt_typ[1] = 0x50;
	pkt_typ[2] = 0x00; pkt_typ[3] = 0x00; pkt_typ[4] = 0x00; pkt_typ[5] = 0x00;
	pkt_typ[6] = 19 + strlen(file);

	pkt_cmd[0]  = 0x04; pkt_cmd[1]  =  cb1; /* cmd */
	pkt_cmd[2]  = 0x00; pkt_cmd[3]  = 0x01;	/* ??? */
	pkt_cmd[4]  = (camera->pl->cmd_seqnum >> 8) & 0xff; pkt_cmd[5]  = camera->pl->cmd_seqnum & 0xff; /* SEQ */
	pkt_cmd[6]  =  cb2; pkt_cmd[7]  =  cb3;	/* cmd */
	pkt_cmd[8]  = 0x00; pkt_cmd[9]  = 0x01;	/* ??? */
	pkt_cmd[10] = 0x00; pkt_cmd[11] = 0x05;	/* ??? */

	pkt_len[0] = 0x00; pkt_len[1] = 0x00; pkt_len[2] = 0x00;
	pkt_len[3] = strlen(file) + 2;
	pkt_len[4] = 0x01;

	packet_len = sizeof(pkt_hdr) + sizeof(pkt_typ) + sizeof(pkt_cmd) + sizeof(pkt_len) + strlen(file) + 2;
	packet = malloc(packet_len);
	if(!packet) {
		free(file);
		return GP_ERROR;
	}

	/* clear packet */
	memset(packet, 0, packet_len);

	/* copy pkt parts into packet */
	memcpy(packet, pkt_hdr, sizeof(pkt_hdr));
	memcpy(packet + sizeof(pkt_hdr), pkt_typ, sizeof(pkt_typ));
	memcpy(packet + sizeof(pkt_hdr) + sizeof(pkt_typ), pkt_cmd, sizeof(pkt_cmd));
	memcpy(packet + sizeof(pkt_hdr) + sizeof(pkt_typ) + sizeof(pkt_cmd), pkt_len, sizeof(pkt_len));
	memcpy(packet + sizeof(pkt_hdr) + sizeof(pkt_typ) + sizeof(pkt_cmd) + sizeof(pkt_len), file, strlen(file));

	free(file);

	/* send the command to list files */
	if(dc3200_send_command(camera, packet, packet_len, ack, &ack_len) == GP_ERROR){
		free(packet);
		return GP_ERROR;
	}

	free(packet);

	do {
		resp_len = DEF_PACKET_LEN;
		if(dc3200_recv_response(camera, resp, &resp_len) == GP_ERROR) {
			return GP_ERROR;
		}

		if(dc3200_send_ack(camera, resp[1]) == GP_ERROR) {
			return GP_ERROR;
		}

		if(resp[1] == camera->pl->rec_seqnum)
			continue;

		camera->pl->rec_seqnum = resp[1];

		/*
		 * we decide what action to take based on
		 * the number of packets we are expecting
		 *
		 * resp[7] holds this info
		 *
		 */
		switch(resp[7]) {
		case 0xC1:
			/*
			 * SINGLE PACKET
			 */

			/* get the total list length from the data header */
			*data_len = bytes_to_l(resp[26], resp[27], resp[28], resp[29]);
			num_left = 0;
			data_start_pos = 31;
			*data_len -= 1;

			/* allocate a buffer to store all this data */
			*data = malloc(*data_len);
			if(*data == NULL) {
				return GP_ERROR;
			}

			ptr_data = *data;

			/* copy this portion of the data */
			memcpy(ptr_data, resp + data_start_pos, resp_len - data_start_pos);
			/* incr the buff ptr */
			ptr_data += resp_len - data_start_pos;
			break;

		case 0x41:
			/*
			 * MULTIPACKET, START
			 */

			/* get the total list length from the data header */
			*data_len = bytes_to_l(resp[34], resp[35], resp[36], resp[37]);
			num_left = bytes_to_l(resp[12], resp[13], resp[14], resp[15]);
			data_start_pos = 39;
			*data_len -= 1;

			/* allocate a buffer to store all this data */
			*data = malloc(*data_len);
			if(*data == NULL) {
				return GP_ERROR;
			}

			ptr_data = *data;

			/* copy this portion of the data */
			memcpy(ptr_data, resp + data_start_pos, resp_len - data_start_pos);
			/* incr the data ptr */
			ptr_data += resp_len - data_start_pos;

			/* update gphoto2 frontend */
			if(command == CMD_GET_FILE || command == CMD_GET_PREVIEW)
			{
				pid = gp_context_progress_start(camera->pl->context,(int)*data_len,"%s", filename);
				gp_context_progress_update(camera->pl->context, pid, ptr_data - *data);
				if(gp_context_cancel(camera->pl->context) == GP_CONTEXT_FEEDBACK_CANCEL) {
					free(*data);
					dc3200_cancel_get_data(camera);
					return GP_ERROR_CANCEL;
				}
			}
			break;
		case 0x01:
			/*
			 * MULTIPACKET, MIDDLE
			 */
			/* calculate the number of packets remaining */
			num_left = bytes_to_l(resp[12], resp[13], resp[14], resp[15]);
			data_start_pos = 16;

			if(*data == NULL) {
				return GP_ERROR;
			}

			/* copy this portion of the data */
			memcpy(ptr_data, resp + data_start_pos, resp_len - data_start_pos);
			/* incr the buff ptr */
			ptr_data += resp_len - data_start_pos;

			/* update gphoto2 frontend */
			if(command == CMD_GET_FILE || command == CMD_GET_PREVIEW)
			{
				gp_context_progress_update(camera->pl->context, pid, ptr_data - *data);
				if(gp_context_cancel(camera->pl->context) == GP_CONTEXT_FEEDBACK_CANCEL) {
					free(*data);
					dc3200_cancel_get_data(camera);
					return GP_ERROR_CANCEL;
				}
			}
			break;
		case 0x81:
			/*
			 * MULTIPACKET, END
			 */
			/* calculate the number of packets remaining */
			num_left = bytes_to_l(resp[12], resp[13], resp[14], resp[15]);
			data_start_pos = 16;

			if(*data == NULL) {
				return GP_ERROR;
			}

			/* copy this portion of the data */
			memcpy(ptr_data, resp + data_start_pos, resp_len - data_start_pos);
			/* incr the buff ptr */
			ptr_data += resp_len - data_start_pos;

			/* update gphoto2 frontend */
			if(command == CMD_GET_FILE || command == CMD_GET_PREVIEW)
			{
				gp_context_progress_update(camera->pl->context, pid, ptr_data - *data);
				if(gp_context_cancel(camera->pl->context) == GP_CONTEXT_FEEDBACK_CANCEL) {
					free(*data);
					dc3200_cancel_get_data(camera);
					return GP_ERROR_CANCEL;
				}
			}
			break;
		default:
			return GP_ERROR;
		}
	} while(num_left > 1);

	if(pid != 0 && (command == CMD_GET_FILE || command == CMD_GET_PREVIEW))
		gp_context_progress_stop(camera->pl->context, pid);

	return GP_OK;
}

/*
 * dc3200_cancel_get_data
 *
 * stops a get data command
 *
 */
int dc3200_cancel_get_data(Camera *camera)
{
	unsigned char pkt[20] = {0x01, 0, 0x80, 0x00, 0x20, 0x03, 0x0d, 0xc1, 0x50, 0xc0,
		0x00, 0x00, 0x00, 0x06, 0x04, 0x01, 0x00, 0x01, 0, 0};
	unsigned char ack[ACK_PACKET_LEN], resp[DEF_PACKET_LEN];
	int ack_len = ACK_PACKET_LEN, resp_len = DEF_PACKET_LEN;

	pkt[1] = dc3200_calc_seqnum(camera);
	pkt[18] = (camera->pl->cmd_seqnum >> 8) & 0xff;
	pkt[19] = camera->pl->cmd_seqnum & 0xff;

	/* wait a bit ... */
	sleep(1);

	/* clear the buffer */
	dc3200_clear_read_buffer(camera);

	if(dc3200_send_command(camera, (unsigned char *)pkt, 20, ack, &ack_len) == GP_ERROR)
		return GP_ERROR;

	if(dc3200_recv_response(camera, resp, &resp_len) == GP_ERROR)
		return GP_ERROR;
	else
		dc3200_send_ack(camera, resp[1]);

	resp_len = DEF_PACKET_LEN;
	if(dc3200_recv_response(camera, resp, &resp_len) == GP_ERROR)
		return GP_ERROR;
	else
		dc3200_send_ack(camera, resp[1]);

	return GP_OK;
}

/*
 * dc3200_send_command
 *
 * if cmd == NULL || cmd_len == 0, send_command waits for a response
 * otherwise, it sends a command and waits for an acknowledgement
 *
 */
int dc3200_send_command(Camera *camera, unsigned char *cmd, int cmd_len, unsigned char *ack, int *ack_len)
{
	int sends, reads;
	int buff_len = *ack_len;
	int received = 0;
	unsigned char *buff = NULL;

	buff = malloc(sizeof(unsigned char) * *ack_len);
	if(buff == NULL) {
		return GP_ERROR;
	}

	sends = SEND_RETRIES;

	while(sends > 0) {
		reads = READ_RETRIES;
		/* check that we are acutally sending a command
		 * and not just waiting for data
		 */
		if(cmd != NULL && cmd_len > 0) {
			/* clear the read buffer */
			dc3200_clear_read_buffer(camera);

			/* send the command */
			dc3200_send_packet(camera, cmd, cmd_len);
		}

		/* wait for response */
		while(reads > 0) {
			if(dc3200_recv_packet(camera, buff, &buff_len) != GP_ERROR) {
				received = 1;
				break;
			}
			reads--;
		}

		if(received) {
			if(buff_len > *ack_len) {
				/* buffer size too small */
				free(buff);
				return GP_ERROR;
			}

			/* sukksess */
			*ack_len = buff_len;
			memcpy(ack, buff, buff_len);
			free(buff);
			return GP_OK;
		} else {
			sends--;
		}
	}

	/* send failed */
	free(buff);
	return GP_ERROR;
}

/*
 * dc3200_recv_response
 *
 * receives a response from a previous command
 *
 */
int dc3200_recv_response(Camera *camera, unsigned char *resp, int *resp_len)
{
	return dc3200_send_command(camera, NULL, 0, resp, resp_len);
}

/*
 * dc3200_send_ack
 *
 * sends an ack packet
 *
 */
int dc3200_send_ack(Camera *camera, int seqnum)
{
	unsigned char ack[ACK_PACKET_LEN];
	ack[0] = 0x01;
	ack[1] = seqnum + 16;

	return dc3200_send_packet(camera, ack, sizeof(ack));
}

/*
 * dc3200_check_ack
 *
 * checks an ack packet (well ok not really)
 *
 */
int dc3200_check_ack(Camera *camera, unsigned char *ack, int ack_len)
{
	return GP_OK;
}

/*
 * dc3200_send_packet
 *
 * compiles a packet from data and sends it
 *
 */
int dc3200_send_packet(Camera *camera, unsigned char *data, int data_len)
{
	int res;

	int buff_len = data_len;
	unsigned char *buff = NULL;

	buff = malloc(sizeof(unsigned char) * buff_len);
	if(!buff)
		return GP_ERROR;

	memcpy(buff, data, buff_len);

	/* compile this into a packet */
	res = dc3200_compile_packet(camera, &buff, &buff_len);
	if(res == GP_ERROR)
		return res;

#ifdef DEBUG
	dump_buffer(buff, buff_len, "s", 16);
#endif
	res = gp_port_write(camera->port, (char *)buff, data_len + 3);
	free(buff);
	return res;
}

/*
 * dc3200_recv_packet
 *
 * waits for a packet to arrive, checks it, and returns it
 *
 */
int dc3200_recv_packet(Camera *camera, unsigned char *data, int *data_len)
{
	int complete = 0;
	int num_read = 0, res = 0, fails = 0;

	/* allocate storage for size, checksum, and EOP */
	unsigned char *buff = NULL;

	buff = malloc(sizeof(unsigned char) * (*data_len + 3));
	if(buff == NULL)
		return GP_ERROR;

	memset(buff, 0, *data_len + 3);

	/*
	 * - read data until we get an 0xFF
	 * - process packet
	 *
	 */

	res = gp_port_read(camera->port, (char *)&buff[num_read], 1);

	while(res >= 0 && fails < READ_RETRIES) {
		if(res == 0) {
			/* read nothing */
			fails++;
		} else {
			/* reset # of fails */
			fails = 0;
			num_read++;
			if(buff[num_read-1] == 0xFF) {
				complete = 1;
				break;
			}
			if(num_read == *data_len + 3) {
				/* we've reached the buffer limit */
				complete = 0;
				break;
			}
		}
		res = gp_port_read(camera->port, (char *)&buff[num_read], 1);
	}

	if(!complete) {
		/* packet incomplete */
		free(buff);
		return GP_ERROR;
	}

#ifdef DEBUG
	dump_buffer(buff, num_read, "r", 16);
#endif

	if(dc3200_process_packet(camera, buff, &num_read) == GP_ERROR) {
		free(buff);
		return GP_ERROR;
	}

	/* save the last successful packet received time */
	time(&(camera->pl->last));

	memcpy(data, buff, *data_len);

	/* return with new packet size */
	*data_len = num_read;

	free(buff);
	return GP_OK;
}

/*
 * dc3200_compile_packet
 *
 * changes 0xFF -> 0xFE 0x01
 *         0xFE -> 0xFE 0x00
 * adds packet length, checksum, and EOP (0xFF)
 *
 */
int dc3200_compile_packet(Camera *camera, unsigned char **data, int *data_len)
{
	int count, i, j;
	unsigned char *new_data = NULL;
	unsigned char *tmp_ptr  = NULL;

	/* realloc + 2 for len and checksum */
	*data_len += 2;

	tmp_ptr = (unsigned char*)realloc(*data, *data_len);
	if(tmp_ptr == NULL)
		return GP_ERROR;

	*data = tmp_ptr;

	/* add length, checksum */
	(*data)[*data_len - 2] = *data_len - 2;
	(*data)[*data_len - 1] = dc3200_calc_checksum(camera, *data, *data_len - 1);

	/* this is a hack to fix the 0xFF/0xFE problem.
	 * we adjust a "don't care" byte so the checksum
	 * is not 0xFF or 0xFE
	 */
	if((*data)[*data_len - 1] >= 0xFE) {
		if(*data_len > 19) {
			(*data)[19] += 2;
			(*data)[*data_len - 1] = dc3200_calc_checksum(camera, *data, *data_len - 1);
			printf("adjusting checksum to %02x\n", (*data)[*data_len - 1]);
		}
	}

	/* count number of 0xfe's and 0xff's */
	count = 0;
	for(i=0;i<*data_len; i++) {
		if((*data)[i] == 0xFE || (*data)[i] == 0xFF)
			count++;
	}

	new_data = (unsigned char*)malloc(*data_len + count + 3); /* 3 for length, checksum, EOP */
	if(!new_data)
		return GP_ERROR;

	j = 0;
	for(i=0;i<*data_len; i++) {
		if((*data)[i] == 0xFE || (*data)[i] == 0xFF) {
			printf("(*data)[i]        == %02x\n", (*data)[i]);
			printf("(*data)[i] - 0xFE == %02x\n", (*data)[i] - 0xFE);
			new_data[j++] = 0xFE;
			new_data[j++] = (*data)[i] - 0xFE;
		} else {
			new_data[j++] = (*data)[i];
		}
	}

	/* adjust the new packet count */
	*data_len += count + 1;

	new_data[*data_len - 1] = 0xFF;

	/* free the old packet data */
	free(*data);

	/* set the ptr */
	*data = new_data;

	return GP_OK;
}

/*
 * dc3200_process_packet
 *
 * changes 0xFE 0x01 -> 0xFF
 *         0xFE 0x00 -> 0xFE
 * checks & removes packet length, checksum, EOP (0xFF)
 *
 */
int dc3200_process_packet(Camera *camera, unsigned char *data, int *data_len)
{
	int count = 0, i;
	int length, checksum;
	unsigned char *buff;

	if(data == NULL || *data_len < 1) {
		return GP_ERROR;
	}

	/* allocate a new buffer */
	buff = (unsigned char*)malloc(sizeof(unsigned char) * *data_len);
	if(!buff)
		return GP_ERROR;

	/* change 0xFE 0x00 | 0x01 -> 0xFF | 0xFE */
	for(i=0; i<*data_len; i++) {
		if(data[i] == 0xFE) {
			if(i<*data_len-1) { /* so we don't run out of array bounds */
				if(data[i+1] == 0x00) {
					buff[count] = 0xFE;
					/* advance i so we don't pick up the trailing byte */
					i++;
					/* incr count */
					count++;
				} else if(data[i+1] == 0x01) {
					buff[count] = 0xFF;
					i++;
					count++;
				}
			} else {
				/* we've found an 0xFE but there is no trailing byte */
				free(buff);
				return GP_ERROR;
			}
		} else {
			buff[count] = data[i];
			count++;
		}
	}

	/* copy over the old buffer */
	memcpy(data, buff, count);

	length = data[count - 3];
	checksum = data[count - 2];

	/* make sure its a valid packet */
	if(length != count - 3 || checksum != dc3200_calc_checksum(camera, data, count - 2)) {
		printf("%02x=%02x %02x=%02x\n", length, count - 3, checksum, dc3200_calc_checksum(camera, data, count - 2));
		free(buff);
		return GP_ERROR;
	}

	/* adjust packet size, strip length, checksum, EOP */
	*data_len = count - 3;

	free(buff);
	return GP_OK;
}

/*
 * dc3200_calc_checksum
 *
 * calculates the checksum for a byte array
 *
 */
int dc3200_calc_checksum(Camera *camera, unsigned char * buffer, int len)
{
	int sum = 0, i;

	for(i=0; i<len; i++) {
		sum += buffer[i];
	}

	sum = -sum + 0xFF;
	if(sum < 0) sum = sum & 0xFF;

	return sum;
}

/*
 * dc3200_calc_seqnum
 *
 * calculates the seqnum for a packet
 *
 */
int dc3200_calc_seqnum(Camera *camera)
{
	if(camera->pl->pkt_seqnum >= 0x1F || camera->pl->pkt_seqnum < 0x10) {
		camera->pl->pkt_seqnum = 0x10;
		return camera->pl->pkt_seqnum;
	} else {
		camera->pl->pkt_seqnum++;
		return camera->pl->pkt_seqnum;
	}
}

/*
 * dc3200_keep_alive
 *
 * sends and receives a ping packet to keep the
 * camera awake
 *
 */
int dc3200_keep_alive(Camera *camera)
{
	unsigned char ka[ACK_PACKET_LEN]; /* keepalive */
	unsigned char ak[ACK_PACKET_LEN]; /* keepalive ack */
	int ak_len = ACK_PACKET_LEN;

	ka[0] = 0xCF;
	ka[1] = 0x01;

	/* send keep alive packet */
	if(dc3200_send_command(camera, ka, sizeof(ka), ak, &ak_len) == GP_ERROR) {
		return GP_ERROR;
	}

	/* check the ack (should be same packet) */
	if(memcmp(ak,ka,ak_len) == 0) {
		return GP_OK;
	}

	return GP_ERROR;
}

/*
 * dc3200_clear_read_buffer
 *
 * clears data in the read buffer by reading it out
 *
 * there is definitely a better way to accomplish this...
 *
 */
int dc3200_clear_read_buffer(Camera *camera)
{
	unsigned char byte;
	int count = 0;

	gp_port_set_timeout(camera->port, 0);

	while(gp_port_read(camera->port, (char *)&byte, 1) > 0)
		count++;

	if(count > 0)
		printf("cleared %d bytes from read buffer\n", count);

	gp_port_set_timeout(camera->port, TIMEOUT);

	return GP_OK;
}

/*
 * dump_buffer
 *
 * displays a byte array nicely formatted in hex format
 *
 */
int dump_buffer(unsigned char * buffer, int len, char * title, int bytesperline)
{
	char spacer[80];
	int i;

	memset(spacer, 0, sizeof(spacer));
	memset(spacer, ' ', strlen(title)+2);

	printf("%s: ", title);

	for(i=0; i<len; i++) {
		if(i%bytesperline==0 && i > 0) {
			printf("\n%s", spacer);
		}
		printf("%02x ", buffer[i]);
	}

	printf("\n");

	printf("%s: ", title);

	for(i=0; i<len; i++) {
		if(i%bytesperline==0 && i > 0) {
			printf("\n%s", spacer);
		}
		if(buffer[i] >= 0x20 && buffer[i] < 0x7F) {
			printf("%c", buffer[i]);
		} else {
			printf(".");
		}
	}

	printf("\n");

	return GP_OK;
}

unsigned long bytes_to_l(int a, int b, int c, int d)
{
	unsigned long res = a;
	res = res << 24 | b << 16 | c << 8 | d;
	return res;
}
