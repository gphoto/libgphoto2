/* library.c
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2/gphoto2.h>

#include "libgphoto2/i18n.h"

#include "dc210.h"
#include "library.h"


static int dc210_cmd_error = 0;
static const char ppmheader[] = "P6\n96 72\n255\n";

#ifdef DEBUG
static int firststatus = 1;
static char oldstatus[DC210_STATUS_SIZE];
#endif

/***************************************************************************

 Global in- and output procedures

****************************************************************************/

static void cfa2ppm
(CameraFile * file)
{
	/* this is a very quick and dirty hack to convert cfa into ppm */
	unsigned char buf[THUMBHEIGHT][THUMBWIDTH];
	unsigned char rgb[THUMBHEIGHT][THUMBWIDTH][3];
	unsigned char val;
	unsigned int addval;
	const char * data;
	unsigned long datasize;
	int i, x, y;

	/* get data */

	DC210_DEBUG("Converting CFA to PPM\n");

	gp_file_get_data_and_size(file, &data, &datasize);

	/* split 8 bit values into to 4 bit values and blow up again to 8 bit*/

	i = 0;
	for(y = 0; y < THUMBHEIGHT; y++ )
		for(x = 0; x < THUMBWIDTH; x+=2 ){
			val = (unsigned char) data[i] >> 4;
			buf[y][x] = (val << 4) | val;
			val = (unsigned char) data[i++] & 0x0F;
			buf[y][x+1] = (val << 4) | val;
		};

	/* fill in the exact values */
	for( y = 0; y < THUMBHEIGHT; y += 2 )
		for( x = 0; x < THUMBWIDTH; x += 2 )
		{
			rgb[y][x][GREEN] = buf[y][x];
			rgb[y][x+1][GREEN] = buf[y][x];
			rgb[y+1][x][GREEN] = buf[y+1][x+1];
			rgb[y+1][x+1][GREEN] = buf[y+1][x+1];
			rgb[y][x][RED] = buf[y][x+1];
			rgb[y][x+1][RED] = buf[y][x+1];
			rgb[y+1][x][RED] = buf[y][x+1];
			rgb[y+1][x+1][RED] = buf[y][x+1];
			rgb[y][x][BLUE] = buf[y+1][x];
			rgb[y][x+1][BLUE] = buf[y+1][x];
			rgb[y+1][x][BLUE] = buf[y+1][x];
			rgb[y+1][x+1][BLUE] = buf[y+1][x];
		};

	/* fill in the interpolated values, don't care about borders */
	for ( y = 1; y < THUMBHEIGHT - 2; y+= 2)
		for ( x = 0; x < THUMBWIDTH - 2; x +=2){
			/* 2 missing green values */
			addval = rgb[y][x][GREEN] + rgb[y][x+2][GREEN]
				+ rgb[y-1][x+1][GREEN] + rgb[y+1][x+1][GREEN];
			rgb[y][x+1][GREEN] = addval / 4;

			addval = rgb[y+1][x-1][GREEN] + rgb[y+1][x+1][GREEN]
				+ rgb[y][x][GREEN] + rgb[y+2][x][GREEN];
			rgb[y+1][x][GREEN] = addval / 4;

			/* 3 missing red values */
			addval = rgb[y-1][x][RED] + rgb[y+1][x][RED];
			rgb[y][x][RED] = addval / 2;

			addval = rgb[y-1][x][RED] + rgb[y-1][x+2][RED]
				+ rgb[y+1][x][RED] + rgb[y+1][x+2][RED];
			rgb[y][x+1][RED] = addval / 4;

			addval = rgb[y+1][x][RED] + rgb[y+1][x+2][RED];
			rgb[y+1][x+1][RED] = addval / 2;

			/* 3 missing blue values */
			addval = rgb[y][x-1][BLUE] + rgb[y][x+1][BLUE];
			rgb[y][x][BLUE] = addval / 2;

			addval = rgb[y][x-1][BLUE] + rgb[y][x+1][BLUE]
				+ rgb[y+2][x-1][BLUE] + rgb[y+2][x+1][BLUE];
			rgb[y+1][x][BLUE] = addval / 4;

			addval = rgb[y][x+1][BLUE] + rgb[y+2][x+1][BLUE];
			rgb[y+1][x+1][BLUE] = addval / 2;
		};

	gp_file_clean(file);
	gp_file_append(file, ppmheader, sizeof(ppmheader) - 1);
	gp_file_append(file, (char *) rgb, THUMBWIDTH * THUMBHEIGHT * 3);
	gp_file_set_mime_type(file, GP_MIME_PPM);

};

/* Utility procedures for command execution */

static void dc210_cmd_init
(char * cmd, unsigned char command_byte)
{
	/* utility procedure to initialize a command string */

	memset (cmd, 0, 8);

	cmd[0] = command_byte;
	cmd[7] = 0x1a;

};

static void dc210_cmd_packet_init
(char * cmd_packet, const char * filename)
{
	/* prepare command packet */
	memset (cmd_packet, 0x00, DC210_CMD_DATA_SIZE);
	memset (cmd_packet + 48, 0xFF, 8);
	strcpy(cmd_packet, "\\PCCARD\\DCIMAGES\\");
	strcpy(cmd_packet + 17, filename);

	DC210_DEBUG("Complete filename is %s\n", cmd_packet);

};

static int dc210_write_single_char
(Camera *camera, unsigned char response)
{
	/* utility procedure to write a single character */

	int i;

	for (i=0; i< RETRIES; i++){
		if (gp_port_write(camera->port, (char *)&response, 1) >= 0)
			return (GP_OK);
	};

	return GP_ERROR;

};

static int dc210_read_single_char
(Camera *camera, unsigned char * response)
{

	/* utility procedure to read a single character */

	int i;
	signed char error;

	for (i = 0; i < RETRIES; i++){

		error = gp_port_read(camera->port, (char *)response, 1);

		if (error < 0){
			if (error == GP_ERROR_TIMEOUT)
				continue;
			else{
				DC210_DEBUG("Real bad error reading single character. Errornumber: %d\n", error);
				return GP_ERROR;
			};
		};

		return GP_OK;

	};

	return GP_ERROR_TIMEOUT;

};

/* Command execution */

static int dc210_execute_command
(Camera *camera, char *cmd)
{

	/* This procedure sends a command to the camera and
	   waits for acknowledgement. Calling procedures have
	   to check the returncode; if the command succeeded
	   there are three possible follow ups:
	   - Read a DC210_COMMAND_COMPLETE in case of
	     simple commands
           - Send an additional command package in case of
	     sophisticated commands
	   - Read data from the camera
	   But all this depends on the command and should not
	   be mixed in this procedure. */

	int i,k ;
	unsigned char response;
	signed char error;

	dc210_cmd_error = DC210_CMD_OKAY;

	for (i = 0; i < RETRIES; i++){

		/* write command */
		if (gp_port_write(camera->port, cmd, 8) < 0){
			DC210_DEBUG("Could not write to port.\n");
			dc210_cmd_error = DC210_WRITE_ERROR;
			continue; /* try again */
		};

		for (k = 0; k < RETRIES; k++){
			error = gp_port_read(camera->port, (char *)&response, 1);
			if (error != 1){
				if (error == GP_ERROR_TIMEOUT){
					dc210_cmd_error = DC210_TIMEOUT_ERROR;
					continue; /* wait a little bit longer */
				}
				else{
					DC210_DEBUG("Real bad error reading answer. Errornumber: %d\n", error);
					dc210_cmd_error = DC210_READ_ERROR;
					return error;
				};
			};

			switch (response){
			case DC210_COMMAND_ACK:
				DC210_DEBUG("Command 0x%02X acknowledged.\n", (unsigned char) cmd[0]);
				return GP_OK; break; /* that's fine, just what we expected */
			case DC210_COMMAND_NAK:
				DC210_DEBUG("Sorry, but the camera seems not to understand the command 0x%02X\n", (unsigned char) cmd[0]);
				dc210_cmd_error = DC210_NAK_ERROR;
				break; /* anyway, send the command again */
			default:
				DC210_DEBUG("Strange. Unexpected response for command 0x%02X: 0x%02X\n", (unsigned char) cmd[0], response);
				dc210_cmd_error = DC210_GARBAGE_ERROR;
				return GP_ERROR;
				break;
			};
			break;
		};

	};

	/* command does definitely not work */

	DC210_DEBUG("Command definitely didn't work.\n");

	return GP_ERROR;

};

/* appropriate actions to a command */

static int dc210_write_command_packet
(Camera * camera, char * data)
{

	unsigned char checksum;
	int i, error;
	unsigned char answer;

	/* calculating checksum */
	checksum = 0;
	for (i = 0; i < DC210_CMD_DATA_SIZE; i ++){
		checksum ^= data[i];
	};

	for (i = 0; i < RETRIES; i++){

		/* write the startbyte */
		dc210_write_single_char(camera, DC210_CMD_PACKET_FOLLOWING);

		/* write packet */
		gp_port_write(camera->port, data, DC210_CMD_DATA_SIZE);

		/* write checksum */
		dc210_write_single_char(camera, checksum);

		/* read answer */

		error = gp_port_read(camera->port, (char *)&answer, 1);

		if (error < 0) return GP_ERROR;

		if (answer == DC210_CORRECT_PACKET) return GP_OK;

		if (answer != DC210_ILLEGAL_PACKET) {
			DC210_DEBUG("Strange answer to command packet: 0x%02X.\n", answer);
			return GP_ERROR;
		};

	};

	DC210_DEBUG("Could not send command packet.\n");

	return GP_ERROR;

};

static int dc210_wait_for_response
(Camera *camera, int expect_busy, GPContext *context)
{
	/* Waits for a command to finish.
	   Expects either a timeout, a DC210_BUSY,
	   a DC210_COMMAND_COMPLETE or a
	   DC210_PACKET_FOLLOWING; all other answers
	   are considered errors */

	unsigned char response;
	int error = 0;
	int counter = 0;
	int progress_id = 0;

	if (context)
	  progress_id = gp_context_progress_start (context, expect_busy, _("Waiting..."));

	while (1){

	  error = dc210_read_single_char(camera, &response);
	  if (error < 0){
	          if (context)
			  gp_context_progress_stop (context, progress_id);
		  return error;
	  };

	  switch (response){
	  case DC210_BUSY:
		  /* wait a little bit longer */
		  if (context && counter <= expect_busy)
			  gp_context_progress_update (context, progress_id, counter++);
		  break;
	  case DC210_COMMAND_COMPLETE:
	  case DC210_PACKET_FOLLOWING:
		  if (context)
			  gp_context_progress_stop (context, progress_id);
		  return response;
		  break;
	  default:
		  if (context)
			  gp_context_progress_stop (context, progress_id);
		  DC210_DEBUG("Command terminated with errorcode 0x%02X.\n", response);
		  return GP_ERROR;
	  };
	};

};

static int dc210_read_single_block
(Camera *camera, unsigned char * b, int blocksize)
{

  int i, k, error;
  char cs_read, cs_computed;

  for (i = 0; i < RETRIES; i++){

    if (dc210_wait_for_response(camera, 0, NULL) != DC210_PACKET_FOLLOWING)
      return GP_ERROR;

    error = 1;
    for (k = 0; k < RETRIES; k++){
      if (gp_port_read(camera->port, (char *)b, blocksize) < 0){
	continue;
      };
      error = 0;
      break;
    };

    if (error) return GP_ERROR;

    if (dc210_read_single_char(camera, (unsigned char *)&cs_read) < 0)
      return GP_ERROR;

    cs_computed = 0;
    for (k = 0 ; k < blocksize; k++)
      cs_computed ^= b[k];
    if (cs_computed == cs_read){
      dc210_write_single_char(camera, DC210_CORRECT_PACKET);
      return GP_OK;
    };

    dc210_write_single_char(camera, DC210_ILLEGAL_PACKET);

  };

  return GP_ERROR;

};

static int dc210_read_to_file
(Camera *camera, CameraFile * f, int blocksize, long int expectsize, GPContext *context)
{

  int packets, k, l, fatal_error, packet_following;
  unsigned int progress_id = 0;
  unsigned char cs_read, cs_computed;
  unsigned char * b;

  int blocks = expectsize / blocksize;
  int remaining = expectsize % blocksize;

  if (NULL == (b = malloc(blocksize))) return GP_ERROR;

  if (remaining) blocks++;

  if (context)
	  progress_id = gp_context_progress_start (context, blocks, _("Getting data..."));

  fatal_error = 0;
  packets = 0;

  packet_following = dc210_wait_for_response(camera, 0, NULL);
  while (DC210_PACKET_FOLLOWING == packet_following){
	  fatal_error = 1;
	  for (k = 0; k < RETRIES; k++){
		  /* read packet */
	          if (gp_port_read(camera->port, (char *)b, blocksize) < 0){
		          dc210_write_single_char(camera, DC210_ILLEGAL_PACKET);
			  packet_following = dc210_wait_for_response(camera, 0, NULL);
			  continue;
		  };
		  /* read checksum */
		  if (dc210_read_single_char(camera, &cs_read) == GP_ERROR){
			  free(b);
			  return GP_ERROR;
		  };
		  /* test checksum */
		  cs_computed = 0;
		  for (l = 0 ; l < blocksize; l++)
			  cs_computed ^= b[l];
		  if (cs_computed != cs_read){
			  dc210_write_single_char(camera, DC210_ILLEGAL_PACKET);
			  packet_following = dc210_wait_for_response(camera, 0, NULL);
			  continue;
		  };
		  /* append to file */
		  if (packets == blocks - 1 && remaining)
			  gp_file_append(f, (char *)b, remaining);
		  else
			  gp_file_append(f, (char *)b, blocksize);
		  /* request next packet */
		  dc210_write_single_char(camera, DC210_CORRECT_PACKET);
		  packet_following = dc210_wait_for_response(camera, 0, NULL);
		  fatal_error = 0;
		  if (context)
			  gp_context_progress_update (context, progress_id, packets);
		  packets++;
		  break;
	  };
	  if (fatal_error) break;
  };

  if (packet_following < 0)
	  fatal_error = 1;

  if (context)
	  gp_context_progress_stop (context, progress_id);

  free(b);

  if (fatal_error) return GP_ERROR;

  return GP_OK;

};

/***************************************************************************

 Working procedures

****************************************************************************/

/***** Set procedures *****/

static int dc210_set_option (Camera * camera, char command, unsigned int value, int valuesize){

	char cmd[8];
	dc210_cmd_init(cmd, command);

	switch(valuesize){
	case 0:
		break;
	case 1:
		cmd[2] = value & 0xFF;
		break;
	case 2:
		cmd[3] = value & 0xFF;
		cmd[2] = (value >> 8) & 0xFF;
		break;
	case 4:
		cmd[5] = value & 0xFF;
		cmd[4] = (value >> 8) & 0xFF;
		cmd[3] = (value >> 16) & 0xFF;
		cmd[2] = (value >> 24) & 0xFF;
		break;
	default:
		return GP_ERROR;
	};

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;

	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

	return GP_OK;

};

int dc210_set_compression (Camera * camera, dc210_compression_type compression){

	return dc210_set_option(camera, DC210_SET_QUALITY, compression, 1);

};

int dc210_set_flash (Camera * camera, dc210_flash_type flash, char preflash){

	if (flash != DC210_FLASH_NONE && preflash){
		return dc210_set_option(camera, DC210_SET_FLASH, flash + 3, 1);
	}
	else
		return dc210_set_option(camera, DC210_SET_FLASH, flash, 1);

};

int dc210_set_zoom (Camera * camera, dc210_zoom_type zoom){

	return dc210_set_option(camera, DC210_SET_ZOOM, zoom, 1);

};

int dc210_set_file_type (Camera * camera, dc210_file_type_type file_type){

	return dc210_set_option(camera, DC210_SET_FILE_TYPE, file_type, 1);

};

int dc210_set_resolution (Camera * camera, dc210_resolution_type res){

	return dc210_set_option(camera, DC210_SET_RESOLUTION, res, 1);

};

int dc210_set_delay (Camera * camera){

	/* This function works, but I do no have the slightest idea,
	   what the effect is / should be.
	   There is *no* delay, when you execute this function before
	   capturing. */

	return dc210_set_option(camera, DC210_SET_DELAY, 10, 4);

};

int dc210_set_exp_compensation (Camera * camera, signed int compensation){

	unsigned char param = abs(compensation);

	if (compensation < 0) param |= 0x80;

	return dc210_set_option(camera, DC210_SET_EXPOSURE, param, 1);

};

int dc210_system_time_callback (Camera * camera, CameraWidget * widget, GPContext * context){

	return dc210_set_option(camera, DC210_SET_TIME, (time(NULL) - CAMERA_SET_EPOC) << 1, 4);

};

static int dc210_format_card (Camera * camera, char * album_name, GPContext * context){

  char data[DC210_CMD_DATA_SIZE];
  char cmd[8];
  char * subst;
  unsigned char answer[16];
  unsigned char checksum_read, checksum;
  int i;

  memset (data, 0, DC210_CMD_DATA_SIZE);

  /* Make the album name acceptable - we need at least 8 characters
     which are not blank and there are no blanks allowed before the
     last character.
  */
  if (album_name != NULL && strlen(album_name) > 0){
	  strncpy(data, album_name, 11);
	  /* substitute blanks in the first 8 characters*/
	  while (NULL != (subst = strchr(data, ' '))) *subst = '_';
	  if (strlen(data) < 8) strncat(data, "________", 8 - strlen(data));
  };

  DC210_DEBUG("Album name is '%s'\n", data);

  dc210_cmd_init(cmd, DC210_CARD_FORMAT);

  dc210_execute_command(camera, cmd);
  dc210_write_command_packet(camera, data);
  if (dc210_wait_for_response(camera, 3, context) != DC210_PACKET_FOLLOWING) return GP_ERROR;

  gp_port_read(camera->port, (char *)answer, 16);
  gp_port_read(camera->port, (char *)&checksum_read, 1);
  checksum = 0;

  for (i = 0; i < 16; i++) checksum ^= answer[i];
  if (checksum_read != checksum) return GP_ERROR;

  DC210_DEBUG("Flash card formatted.\n");

  if (dc210_write_single_char(camera, DC210_CORRECT_PACKET) == GP_ERROR) return GP_ERROR;
  if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

  gp_filesystem_reset(camera->fs);

  return GP_OK;

};

static int dc210_get_card_status (Camera * camera, dc210_card_status * card_status){

  char cmd[8];
  unsigned char answer[16];
  unsigned char checksum_read, checksum;
  int i;

  card_status->open = 0;
  card_status->program = 0;
  card_status->space = 0;

  dc210_cmd_init(cmd, DC210_CARD_STATUS);
  dc210_execute_command(camera, cmd);
  if (dc210_wait_for_response(camera, 0, NULL) != DC210_PACKET_FOLLOWING) return GP_ERROR;

  gp_port_read(camera->port, (char *)answer, 16);
  gp_port_read(camera->port, (char *)&checksum_read, 1);

  checksum = 0;
  for (i = 0; i < 16; i++) checksum ^= answer[i];

  if (checksum_read != checksum)
	  DC210_DEBUG("Error reading card status.\n");
  else
	  DC210_DEBUG("Card status correctly read.\n");

  if (answer[0] & 0x08) card_status->open = 1;
  DC210_DEBUG("Card status open is %d\n", card_status->open);
  card_status->program = answer[1] * 0x100 + answer[2];
  /* set the space in kb */
  card_status->space = (answer[3] * 0x1000000 + answer[4] * 0x10000 + answer[5] * 0x100 + answer[6]) / 1024;

  if (dc210_write_single_char(camera, DC210_CORRECT_PACKET) == GP_ERROR) return GP_ERROR;
  if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

  return GP_OK;

};

int dc210_format_callback(Camera * camera, CameraWidget * widget, GPContext * context){

	CameraWidget * window;
	char * album_name;

	gp_widget_get_root(widget, &window);

	gp_widget_get_child_by_label (window, _("Album name"), &widget);
	gp_widget_get_value (widget, &album_name);

	return dc210_format_card(camera, album_name, context);

};

static int dc210_check_battery (Camera *camera){

  char cmd[8];
  dc210_cmd_init(cmd, DC210_CHECK_BATTERY);

  if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
  if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

  return GP_OK;

};

int dc210_init_port (Camera *camera){

	/* sending a break command resets the
	   speed to 9600 */
	int camera_speeds[] = {115200, 19200, 38400, 57600};
	int i, desired_speed;
	GPPortSettings settings;

	gp_port_get_settings (camera->port, &settings);
	gp_port_set_timeout (camera->port, TIMEOUT);
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;

	desired_speed = settings.serial.speed;
	if (! desired_speed) desired_speed = 115200;

	DC210_DEBUG("Desired port speed is %d.\n", desired_speed);

	/* only set the speed if nothing was specified on the command line */
	if (! settings.serial.speed) settings.serial.speed = 9600;
	gp_port_set_settings (camera->port, settings);

	/* ok, run a dummy command; if the camera was off,
	   this will activate it and set the speed to 9600;
	   but the command will return an error;
	   It makes no sense to shorten the timeout time,
	   because the camera may need up to 10 seconds (information
	   of the manual) to startup */
	if (dc210_check_battery(camera) == GP_OK) return GP_OK;

	/* Fine, the last command didn't work, but we are now
	   sure, the camera is working, if it is connected
	   and has enough power.
	   This is the time to send a break to reset the speed
	   to 9600 */

	gp_camera_set_port_speed(camera, 9600);
	gp_port_send_break(camera->port, 300);
	usleep(300 * 1000);

	/* Excellent. Now our dummy command should work */

	if (dc210_check_battery(camera) == GP_OK){
	  return dc210_set_speed(camera, desired_speed);
	};

	/* My, my, this is not nice. We change the timeout
	   and try other speeds as last resort */

	gp_port_set_timeout (camera->port, 100);

	for (i = 0; i < 4; i++){
		settings.serial.speed = camera_speeds[i];
		gp_port_set_settings (camera->port, settings);

		if (dc210_check_battery(camera) == GP_OK){
			gp_port_set_timeout (camera->port, TIMEOUT);
			return dc210_set_speed(camera, desired_speed);
		};

		DC210_DEBUG("What a pity. Speed %d does not work.\n", camera_speeds[i]);
	};

	/* reset the timeout */
	gp_port_set_timeout (camera->port, TIMEOUT);

	return GP_ERROR;

};

int dc210_set_speed (Camera *camera, int speed) {

	unsigned char cmd[8];
	GPPortSettings settings;

	dc210_cmd_init((char*)cmd, DC210_SET_SPEED);

	switch (speed) {
	case 9600:
		cmd[2] = 0x96; cmd[3] = 0x00; break;
	case 19200:
		cmd[2] = 0x19; cmd[3] = 0x20; break;
	case 38400:
		cmd[2] = 0x38; cmd[3] = 0x40; break;
	case 57600:
		cmd[2] = 0x57; cmd[3] = 0x60; break;
	case 115200:
		cmd[2] = 0x11; cmd[3] = 0x52; break;
	default:
		return (GP_ERROR);
	};

	if (dc210_execute_command(camera, (char*)cmd) == GP_ERROR) return GP_ERROR;

	gp_port_get_settings (camera->port, &settings);
	settings.serial.speed = speed;
	gp_port_set_settings (camera->port, settings);

	DC210_DEBUG("Port speed set to %d.\n", speed);

	return GP_OK;

};

/****** Picture actions *****/

int dc210_take_picture
(Camera * camera, GPContext * context)
{
	char cmd[8];
	dc210_cmd_init(cmd, DC210_TAKE_PICTURE);

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	switch (dc210_wait_for_response(camera, 5, context)){
	  /* we allow a timeout error here and check the result via status */
	  case DC210_COMMAND_COMPLETE:
	  case GP_ERROR_TIMEOUT: return GP_OK; break;
  	  default: return GP_ERROR; break;
	};

	return GP_ERROR;

};

int dc210_delete_picture
(Camera * camera, unsigned int picno)
{

	char cmd[8];
	unsigned int pic_offset = picno - 1;

	dc210_cmd_init(cmd, DC210_DELETE_PICTURE);
	cmd[3] = pic_offset & 0xFF;
	cmd[2] = (pic_offset >> 8) & 0xFF;

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

	return GP_OK;

};

int dc210_delete_last_picture
(Camera * camera )
{

  dc210_status status;

  if (dc210_get_status(camera, &status) == GP_ERROR)
	  return GP_ERROR;

  if (0 == status.numPicturesInCamera)
	  return GP_ERROR;

  return dc210_delete_picture(camera, status.numPicturesInCamera);

};

int dc210_delete_picture_by_name
(Camera * camera, const char * filename )
{

        char cmd[8];
	char cmd_packet[DC210_CMD_DATA_SIZE];

	dc210_cmd_init(cmd, DC210_CARD_FILE_DEL);
	dc210_cmd_packet_init(cmd_packet, filename);

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_write_command_packet(camera, cmd_packet) == GP_ERROR) return GP_ERROR;
	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

	return GP_OK;

};

int dc210_download_last_picture
(Camera * camera, CameraFile *file, dc210_picture_type type, GPContext *context)
{

	dc210_status status;
	dc210_picture_info picinfo;

	if (dc210_get_status(camera, &status) == GP_ERROR) return GP_ERROR;
	if (status.numPicturesInCamera == 0) return GP_ERROR;

	if (dc210_get_picture_info(camera, &picinfo, status.numPicturesInCamera) == GP_ERROR)
		return GP_ERROR;

	if (dc210_get_picture_info(camera, &picinfo, status.numPicturesInCamera) == GP_ERROR)
		return GP_ERROR;

	return dc210_download_picture_by_name(camera, file, picinfo.image_name, type, context);

};

int dc210_open_card
(Camera * camera)
{

	char cmd[8];
	dc210_card_status card_status;

	/* check if card is already opened */
	dc210_get_card_status(camera, &card_status);
	if (card_status.open) return GP_OK;

	dc210_cmd_init(cmd, DC210_OPEN_CARD);

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

	return GP_OK;
};

#if 0 /* This function is not used by anybody */
static int dc210_close_card
(Camera * camera)
{

	char cmd[8];
	dc210_cmd_init(cmd, DC210_CLOSE_CARD);

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

	return GP_OK;
};
#endif

int dc210_download_picture_by_name
(Camera * camera, CameraFile *file, const char *filename, dc210_picture_type type, GPContext *context)
{

        char cmd[8];
	char cmd_packet[DC210_CMD_DATA_SIZE];
	dc210_picture_info picinfo;

	/* prepare command */
	if (type == DC210_FULL_PICTURE){
		/* we need the file size for the progress bar */
		if (dc210_get_picture_info_by_name(camera, &picinfo, filename) == GP_ERROR)
			return GP_ERROR;
		DC210_DEBUG("Picture size is %d\n", picinfo.picture_size);
		dc210_cmd_init(cmd, DC210_CARD_READ_FILE);
	}
	else
		dc210_cmd_init(cmd, DC210_CARD_READ_THUMB);

	if (type == DC210_RGB_THUMB) cmd[4] = 1;

	dc210_cmd_packet_init(cmd_packet, filename);

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_write_command_packet(camera, cmd_packet) == GP_ERROR) return GP_ERROR;

	switch (type){
	case DC210_FULL_PICTURE:
		if (picinfo.file_type == DC210_FILE_TYPE_JPEG)
			gp_file_set_mime_type(file, GP_MIME_JPEG);
		if (dc210_read_to_file(camera, file,
				       DC210_CARD_BLOCK_SIZE,
				       picinfo.picture_size, context) == GP_ERROR)
			return GP_ERROR;
		break;
	case DC210_CFA_THUMB:
		/* this thumb type is much faster to download, because it has just
		   a 6th of the size of rgb data */
		if (dc210_read_to_file(camera, file,
				       DC210_DOWNLOAD_BLOCKSIZE, 48 * 72, NULL) == GP_ERROR)
			return GP_ERROR;
		/* convert cfa data to rgb: */
		cfa2ppm(file);
		break;
	  break;
	case DC210_RGB_THUMB:
		/* if you really need nice thumbs, you can use this, but I don't think it's
		   worth the 6 times longer download time */
		gp_file_set_mime_type(file, GP_MIME_PPM);
		gp_file_append(file, ppmheader, sizeof(ppmheader) - 1);
		if (dc210_read_to_file(camera, file,
				       DC210_DOWNLOAD_BLOCKSIZE, 96 * 72 * 3, NULL) == GP_ERROR)
			return GP_ERROR;
		break;
	};

	return GP_OK;
};

int dc210_capture (Camera *camera, CameraFilePath *path, GPContext *context)
{

	dc210_status status;
	dc210_picture_info picinfo;
	int pictures_in_camera;

	/* dc210_take_picture no longer returns an error on timeout; check
	   the working of the command via status */

	if (dc210_get_status(camera, &status) == GP_ERROR) return GP_ERROR;
	pictures_in_camera = status.numPicturesInCamera;

	if (dc210_take_picture(camera, context) == GP_ERROR) return GP_ERROR;

	if (dc210_get_status(camera, &status) == GP_ERROR) return GP_ERROR;
	if (pictures_in_camera == status.numPicturesInCamera) return GP_ERROR;

	if (dc210_get_picture_info(camera, &picinfo, status.numPicturesInCamera) == GP_ERROR) return GP_ERROR;

	strcpy(path->folder, "/");
	strcpy(path->name, picinfo.image_name);

	return GP_OK;

};

/***** Retrieve information *****/

int dc210_get_picture_number (Camera *camera, const char * filename){

	/* This function is terrible slow if you have a lot of pictures
	   in the camera; therefor it should not be used */

	dc210_status status;
	dc210_picture_info picinfo;
	int i;

	if (dc210_get_status(camera, &status) == GP_ERROR) return -1;

	for (i = 1; i <= status.numPicturesInCamera; i++){

		if (dc210_get_picture_info(camera, &picinfo, i) == GP_ERROR) return -1;

		if (strcmp(picinfo.image_name, filename) == 0){
			return i;
		};

	};

	return -1;

};

int dc210_get_filenames (Camera *camera, CameraList *list, GPContext *context)
{
	char cmd[8];
	CameraFile *file;
	char filename[13];
	const char * data;
	unsigned long datasize;
	int pictures, i;

	gp_file_new(&file);

	dc210_cmd_init(cmd, DC210_GET_ALBUM_FILENAMES);

	dc210_execute_command(camera, cmd);

	dc210_read_to_file(camera, file, DC210_DIRLIST_SIZE, 0, NULL);

	gp_file_get_data_and_size(file, &data, &datasize);

	pictures = (unsigned char) data[0] * 0x100 + (unsigned char) data[1];
	DC210_DEBUG("There are %d pictures in the camera\n", pictures);

	filename[8] = '.';
	filename[12] = '\0';
	for (i = 0; i < pictures; i++){
		strncpy(filename, data + 2 + i*20, 8);
		strncpy(filename + 9, data + 10 + i*20, 3);
		DC210_DEBUG("Adding filename %s to list\n", filename);
		gp_list_append(list, filename, NULL);
	};

	gp_file_free(file);

	return GP_OK;

};

static void dc210_picinfo_from_block (dc210_picture_info * picinfo, unsigned char * data){

	picinfo->camera_type = data[1];
	picinfo->file_type = data[2];
	picinfo->resolution = data[3];
	picinfo->compression = data[4];
	picinfo->picture_number = data[6] * 0x100 + data[7];
	picinfo->picture_size = data[8] * 0x1000000 + data[9] * 0x10000 + data[10] * 0x100 + data[11];
	picinfo->preview_size = 96 * 72 * 3 + sizeof(ppmheader);
	picinfo->picture_time = CAMERA_GET_EPOC + ((data[12] * 0x1000000 +
						    data[13] * 0x10000 +
						    data[14] * 0x100 +
						    data[15]) >> 1);
	picinfo->flash_used = data[16];
	picinfo->flash = data[17];
	picinfo->preflash = (data[17] > 2);
	if (picinfo->preflash) picinfo->flash -= 3;
	picinfo->zoom = data[21];
	picinfo->f_number = data[26];
	picinfo->battery = data[27];
	picinfo->exposure_time = data[28] * 0x1000000 + data[29] * 0x10000 + data[30] * 0x100 + data[31];
	strncpy(picinfo->image_name, (char *)&data[32], 12);
	picinfo->image_name[12] = 0;

}

int dc210_get_picture_info (Camera *camera, dc210_picture_info *picinfo, unsigned int picno) {

	char cmd[8];
	unsigned char data[DC210_PICINFO_SIZE];
	unsigned int pic_offset = picno - 1;

	dc210_cmd_init(cmd, DC210_GET_PICINFO);
	cmd[3] = pic_offset & 0xFF;
	cmd[2] = (pic_offset >> 8) & 0xFF;

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_read_single_block(camera, data, DC210_PICINFO_SIZE) == GP_ERROR) return GP_ERROR;
	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

	dc210_picinfo_from_block(picinfo, data);

	return GP_OK;

};

int dc210_get_picture_info_by_name (Camera *camera, dc210_picture_info *picinfo, const char * filename) {

	char cmd[8];
	char cmd_packet[DC210_CMD_DATA_SIZE];
	unsigned char data[DC210_CARD_BLOCK_SIZE];

	dc210_cmd_init(cmd, DC210_CARD_GET_PICINFO);
	dc210_cmd_packet_init(cmd_packet, filename);

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_write_command_packet(camera, cmd_packet) == GP_ERROR) return GP_ERROR;
	if (dc210_read_single_block(camera, data, DC210_CARD_BLOCK_SIZE) == GP_ERROR) return GP_ERROR;
	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

	dc210_picinfo_from_block(picinfo, data);

	return GP_OK;

};

int dc210_get_status (Camera *camera, dc210_status *status) {

#ifdef DEBUG
	int i;
#endif

	char data[DC210_STATUS_SIZE];
	char cmd[8];

	/* you have to check the battery status explicitly
	   before reading the status table*/
	dc210_check_battery(camera);

	/* check the card */
	dc210_get_card_status(camera, &(status->card_status));

	dc210_cmd_init(cmd, DC210_GET_STATUS);

	if (dc210_execute_command(camera, cmd) == GP_ERROR) return GP_ERROR;
	if (dc210_read_single_block(camera, (unsigned char *)data, DC210_STATUS_SIZE) == GP_ERROR) return GP_ERROR;
	if (dc210_wait_for_response(camera, 0, NULL) != DC210_COMMAND_COMPLETE) return GP_ERROR;

#ifdef DEBUG
	if (firststatus){
	  firststatus = 0;
	}
	else{
		for (i = 0; i < DC210_STATUS_SIZE; i++){
			if (oldstatus[i] != data[i] && (i < 12 || i > 15)){
				DC210_DEBUG("Statusdata differs at offset %03d (old: 0x%02X, new: 0x%02X)\n",
					    i, (unsigned char) oldstatus[i], (unsigned char) data[i]);
			};
		};
	};
	memcpy(oldstatus, data, DC210_STATUS_SIZE);
#endif

	status->firmwareMajor         = data[2];
	status->firmwareMinor         = data[3];
	status->battery               = data[8];
	status->acstatus              = data[9];

	/* seconds since unix epoc */
	status->time = CAMERA_GET_EPOC + ((unsigned char) data[12] * 0x1000000 +
					  (unsigned char) data[13] * 0x10000 +
					  (unsigned char) data[14] * 0x100 +
					  (unsigned char) data[15]) / 2;

	status->zoom                  = data[16];
	status->compression_type      = data[19];
	status->flash                 = data[20];
	status->exp_compensation      = data[21] & 0x7F;
	if (data[21] & 0x80) status->exp_compensation *= -1;
	status->preflash = (data[20] > 2);
	if (status->preflash) status->flash -= 3;
	status->resolution            = data[22];
	status->file_type             = data[23];
	status->totalPicturesTaken    = (unsigned char) data[25] * 0x100 + (unsigned char) data[26];
	status->totalFlashesFired     = (unsigned char) data[27] * 0x100 + (unsigned char) data[28];
	status->numPicturesInCamera   = (unsigned char) data[56] * 0x100 + (unsigned char) data[57];
	status->remainingLow          = (unsigned char) data[68] * 0x100 + (unsigned char) data[69];
	status->remainingMedium       = (unsigned char) data[70] * 0x100 + (unsigned char) data[71];
	status->remainingHigh         = (unsigned char) data[72] * 0x100 + (unsigned char) data[73];
	strncpy(status->album_name, &data[77], 11);
	status->album_name[11] = 0;

	return GP_OK;

}

/* Debugging stuff, not really needed in the driver */

#ifdef DEBUG

static int dc210_read_dummy_packet(Camera * camera){

	int i;
	signed char error;
	unsigned char answer, lastanswer, checksum;

	checksum = 0;
	lastanswer = 0;
	for ( i = 0; i < 2048; i++){

		error = gp_port_read(camera->port, &answer, 1);

		if (error < 0){
			DC210_DEBUG("Packet read exhausted. Packet size: %d\n", i-1);
			if (checksum == lastanswer)
			  DC210_DEBUG("Checksum is okay. Sending okay.\n");
			else
			  DC210_DEBUG("Checksum is not okay. Sending correct packet anyway.\n");
			answer = DC210_CORRECT_PACKET;
			gp_port_write(camera->port, &answer, 1);
			return GP_OK;
		};
		DC210_DEBUG(" Byte %03d: 0x%02X", i, answer);
		if (answer > 32)
		  DC210_DEBUG(" (%c)", answer);
		DC210_DEBUG("\n");
		checksum ^= lastanswer;
		lastanswer = answer;
	};

	DC210_DEBUG("Packet too long. Sending cancel.\n");
	answer = DC210_CANCEL;
	gp_port_write(camera->port, &answer, 1);

	return GP_OK;

};

static int dc210_test_command
(Camera * camera, unsigned char cmdbyte, unsigned char *databytes){

	/* this is a debug command to test the behaviour
	   of certain camera commands */
	unsigned char cmd[8];
	unsigned char answer;
	signed char error;
	int packetcount = 0;
	char dummy_cmd_packet[DC210_CMD_DATA_SIZE];

	memset (dummy_cmd_packet, 0, DC210_CMD_DATA_SIZE);

	dc210_cmd_init(cmd, cmdbyte);

	memcpy(cmd + 2, databytes, 4);

	/* okay, write the command and wait for acknowledge */

	gp_port_write(camera->port, cmd, 8);

	error = gp_port_read(camera->port, &answer, 1);

	if (error < 0){
		DC210_DEBUG("Command 0x%02X produced port read error %d",
		       cmdbyte, error);
		return GP_OK;
	};

	switch (answer){
	case DC210_COMMAND_ACK:
		DC210_DEBUG("Okay, command 0x%02X acknowledged.\n", cmdbyte);
		break;
	case DC210_COMMAND_NAK:
		DC210_DEBUG("Sorry, command 0x%02X not acknowledged.\n", cmdbyte);
		return GP_OK;
	default:
		DC210_DEBUG("Unexpected response 0x%02X to command 0x%02X.\n",
		       answer, cmdbyte);
		return GP_OK;
	};

 wait_finish:
	error = gp_port_read(camera->port, &answer, 1);

	if (error < 0){
		DC210_DEBUG("Command 0x%02X produced port read error %d\n",
		       cmdbyte, error);
		DC210_DEBUG("Probably the command expects a command package.\n");
		DC210_DEBUG("Sending one...\n");
		dc210_write_command_packet(camera, dummy_cmd_packet);
		goto wait_finish;
	};

	switch(answer){
	case DC210_COMMAND_COMPLETE:
		DC210_DEBUG("Okay. Command has been accepted and executed.\n");
		return GP_OK;
	case DC210_EXECUTION_ERROR:
		DC210_DEBUG("Sorry. An error occurred while executing command.\n");
		return GP_OK;
	case DC210_CORRECT_PACKET:
		DC210_DEBUG("Okay. Packet successfully received.\n");
		goto wait_finish;
	case DC210_PACKET_FOLLOWING:
		DC210_DEBUG("My, my, you got more than you bargained for. ");
		DC210_DEBUG("Awaiting packet...\n");
		dc210_read_dummy_packet(camera);
		DC210_DEBUG("Read %d packets.\n", ++packetcount);
		goto wait_finish;
	case DC210_BUSY:
		DC210_DEBUG("Camera is busy.\n");
		goto wait_finish;
	default:
		DC210_DEBUG("Unexpected response 0x%02X to command 0x%02X.\n",
		       answer, cmdbyte);
		return GP_OK;
	};
};

int dc210_debug_callback(Camera * camera, CameraWidget * widget, GPContext * context){

	/*CameraFile *file;*/
	CameraWidget * window;
	char * s_param1, *s_param2, *s_param3;
	int param1, param2, param3;
	int value;
	unsigned char params[4];
	dc210_status status;

	memset(params, 0, 4);

	gp_widget_get_root(widget, &window);

	gp_widget_get_child_by_label (window, _("Parameter 1"), &widget);
	gp_widget_get_value (widget, &s_param1);
	param1 = atoi(s_param1);

	gp_widget_get_child_by_label (window, _("Parameter 2"), &widget);
	gp_widget_get_value (widget, &s_param2);
	param2 = atoi(s_param2);

	gp_widget_get_child_by_label (window, _("Parameter 3"), &widget);
	gp_widget_get_value (widget, &s_param3);
	param3 = atoi(s_param3);

	/* Put the stuff you want to test here */
	dc210_get_status(camera, &status);
	dc210_test_command(camera, 0x85, params);
	dc210_get_status(camera, &status);

	return GP_OK;

};

#endif
