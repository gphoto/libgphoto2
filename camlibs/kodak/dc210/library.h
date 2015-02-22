/* library.h
 *
 * Copyright (C) 2002 Michel Koltan <koltan@gmx.de>
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


#ifndef _DC210_LIBRARY_H_
#define _DC210_LIBRARY_H_

#define DC210_CMD_OKAY         0
#define DC210_WRITE_ERROR     -1
#define DC210_READ_ERROR      -2
#define DC210_TIMEOUT_ERROR   -3
#define DC210_NAK_ERROR       -4
#define DC210_GARBAGE_ERROR   -5

#define THUMBHEIGHT 72
#define THUMBWIDTH 96
#define RED 0
#define GREEN 1
#define BLUE 2

static void cfa2ppm (CameraFile * file);
static void dc210_cmd_init (char *cmd, unsigned char command_byte);
static void dc210_cmd_packet_init (char * cmd_packet, const char * filename);
static int dc210_write_single_char (Camera *camera, unsigned char response);
static int dc210_write_command_packet(Camera * camera, char * data);
static int dc210_execute_command (Camera *camera, char *cmd);
static int dc210_read_single_char (Camera *camera, unsigned char * p);
static int dc210_wait_for_response (Camera *camera, int expect_busy, GPContext *context);
static int dc210_read_to_file (Camera *camera, CameraFile * f, int blocksize, long int expectsize, GPContext *context);
static int dc210_read_single_block (Camera *camera, unsigned char * b, int blocksize);
static int dc210_set_option (Camera * camera, char command, unsigned int value, int valuesize);
static int dc210_check_battery (Camera *camera);
static int dc210_get_card_status (Camera * camera, dc210_card_status * card_status);
static void dc210_picinfo_from_block (dc210_picture_info * picinfo, unsigned char * data);

#ifdef DEBUG
static int dc210_read_dummy_packet(Camera * camera);
static int dc210_test_command(Camera * camera, unsigned char cmdbyte, unsigned char *databytes);
#endif

#endif /* _DC210_LIBRARY_H_ */


