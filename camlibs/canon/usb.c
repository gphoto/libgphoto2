/****************************************************************************
 *
 * File: usb.c
 *
 * USB communication layer.
 *
 * $Id$
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#ifdef OS2
#include <db.h>
#endif
#include <netinet/in.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include <gphoto2.h>

#include "usb.h"
#include "canon.h"
#include "util.h"


/*****************************************************************************
 *
 * canon_usb_camera_init
 *
 * Initializes the USB camera through a series of read/writes
 *
 * Returns GP_OK on success.
 * Returns GP_ERROR on any error.
 *
 ****************************************************************************/
int
canon_usb_camera_init (Camera *camera)
{
	unsigned char msg[0x58];
	unsigned char buffer[0x44];
	int i, read_bytes, *i_ptr;
	char *camstat_str = "NOT RECOGNIZED";
	unsigned char camstat;

	GP_DEBUG ("canon_usb_camera_init()");

	memset (msg, 0, sizeof (msg));
	memset (buffer, 0, sizeof (buffer));

	i = gp_port_usb_msg_read (camera->port, 0x0c, 0x55, 0, msg, 1);
	if (i != 1) {
		gp_camera_set_error (camera,
				     "Could not establish initial contact with camera");
		return GP_ERROR_IO_INIT;
	}
	camstat = msg[0];
	switch (camstat) {
		case 'A':
			camstat_str = "Camera was already active";
			break;
		case 'C':
			camstat_str = "Camera was woken up";
			break;
		case 'I':
		case 'E':
			camstat_str = "Unknown (some kind of error)";
			break;
	}
	if (camstat != 'A' && camstat != 'C') {
		gp_camera_set_error (camera, "Initial camera response %c/'%s' unrecognized",
				     camstat, camstat_str);
		return GP_ERROR_IO_INIT;
	}
	GP_DEBUG ("canon_usb_camera_init() "
		  "initial camera response: %c/'%s'", camstat, camstat_str);

	i = gp_port_usb_msg_read (camera->port, 0x04, 0x1, 0, msg, 0x58);
	if (i != 0x58) {
		gp_camera_set_error (camera,
				     "Step #2 of initialization failed! (returned %i, expected %i) "
				     "Camera not operational", i, 0x58);
		return GP_ERROR_IO_INIT;
	}

	i = gp_port_usb_msg_write (camera->port, 0x04, 0x11, 0, msg + 0x48, 0x10);
	if (i != 0x10) {
		gp_camera_set_error (camera, "Step #3 of initialization failed! "
				     "(returned %i, expected %i) Camera not operational", i,
				     0x10);
		return GP_ERROR_IO_INIT;
	}
	GP_DEBUG ("canon_usb_camera_init() "
		  "PC sign on LCD should be lit now (if your camera has a PC sign)");

	/* We expect to get 0x44 bytes here, but the camera is picky at this stage and
	 * we must read 0x40 bytes, look at position 0 and read 0x4 (or whatever) bytes
	 * more.
	 */
	i = gp_port_read (camera->port, buffer, 0x40);
	if ((i >= 4)
	    && (buffer[i - 4] == 0x54) && (buffer[i - 3] == 0x78)
	    && (buffer[i - 2] == 0x00) && (buffer[i - 1] == 0x00)) {

		/* We have some reports that sometimes the camera takes a long
		 * time to respond to this read request and then comes back with
		 * the 54 78 00 00 packet, instead of telling us to read four more
		 * bytes which is the normal 54 78 00 00 packet.
		 */

		GP_DEBUG ("canon_usb_camera_init() "
			  "expected %i bytes, got %i bytes with \"54 78 00 00\" "
			  "at the end, so we just ignore the whole bunch and call it a day",
			  0x40, i);

		return GP_OK;
	}
	if (i != 0x40) {
		gp_camera_set_error (camera, "Step #4.1 failed! "
				     "(returned %i, expected %i) Camera not operational", i,
				     0x40);
		return GP_ERROR_IO_INIT;
	}

	i_ptr = (int *) buffer;
	read_bytes = *i_ptr;
	if (read_bytes) {
		i = gp_port_read (camera->port, buffer, read_bytes);
		if (i != read_bytes) {
			GP_DEBUG ("canon_usb_camera_init() "
				  "Step #4.2 of initialization failed! (returned %i, expected %i) "
				  "Camera might still work though. Continuing.", i,
				  read_bytes);
		} else {
			if ((i >= 4)
			    && (buffer[i - 4] == 0x54) && (buffer[i - 3] == 0x78)
			    && (buffer[i - 2] == 0x00) && (buffer[i - 1] == 0x00)) {
				if (i != 4) {
					/* Only log this message about anomality if we did not
					 * get exactly four bytes back. We expect to get four
					 * bytes (54 78 00 00) and don't have to log it if we do.
					 */
					GP_DEBUG ("canon_usb_camera_init() "
						  "expected %i but got %i bytes with \"54 78 00 00\" "
						  "at the end, so we just ignore the whole bunch",
						  0x4, i);
				}
			} else {
				/* I don't think we can ever get here and the camera will still work
				 * anymore. That should be fixed by the other new code in this commit.
				 * I'll leave it here as a comment for now, but change the behaviour
				 * to return error instead.
				 * GP_DEBUG("canon_usb_camera_init() "
				 *       "Step #4 of initialization failed! (returned %i, expected %i) "
				 *       "Camera might still work though. Continuing.", i, 0x44);
				 */
				gp_camera_set_error (camera,
						     "Step #4 of initialization failed! "
						     "(returned %i, expected %i) Camera not operational",
						     i, read_bytes);
				return GP_ERROR_IO_INIT;
			}
		}
	}

	return GP_OK;
}

/*****************************************************************************
 *
 * canon_usb_init
 *
 * Initializes the given USB device.
 *
 * Returns GP_OK on success.
 *
 ****************************************************************************/

int
canon_usb_init (Camera *camera)
{
	int res;
	GPPortSettings settings;

	GP_DEBUG ("Initializing the (USB) camera.\n");

	/* Get the current settings */
	gp_port_get_settings (camera->port, &settings);

	/* Adjust the current settings */
	settings.usb.inep = 0x81;
	settings.usb.outep = 0x02;
	settings.usb.config = 1;
	settings.usb.altsetting = 0;

	/* Set the new settings */
	res = gp_port_set_settings (camera->port, settings);
	if (res != GP_OK) {
		gp_camera_set_error (camera, "Could not apply USB settings");
		return res;
	}

	return canon_usb_camera_init (camera);
}

/**
 * canon_usb_keylock:
 * @camera: camera to lock keys on
 * @Returns: gphoto2 error code
 *
 * Lock the keys on the camera and turn off the display
 **/
int
canon_usb_keylock (Camera *camera)
{
	unsigned char *c_res;
	int bytes_read;

	GP_DEBUG ("canon_usb_keylock()");

	/* Check for cameras that do not need this command */
	if (camera->pl->model == CANON_PS_S100) {
		GP_DEBUG ("Your camera model does not need the keylock.");
		return GP_OK;
	}

	c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_KEYLOCK, &bytes_read, NULL, 0);
	if (bytes_read == 0x4) {
		GP_DEBUG ("Got the expected number of bytes back, "
			  "unfortuntely we don't know what they mean.");
	} else {
		gp_camera_set_error (camera, "canon_usb_keylock: "
				     "Unexpected amount of data returned (%i bytes, expected %i)",
				     bytes_read, 0x4);
		return GP_ERROR_IO;
	}

	return GP_OK;
}

/**
 * canon_usb_dialogue:
 * @camera: the Camera to work with
 * @canon_funct: integer constant that identifies function we are execute
 * @return_length: number of bytes to read from the camera as response
 * @payload: data we are to send to the camera
 * @payload_length: length of #payload
 *
 * USB version of the #canon_serial_dialogue function.
 *
 * We construct a packet with the known command values (cmd{1,2,3}) of
 * the function requested (#canon_funct) to the camera. If #return_length
 * exists for this function, we read #return_length bytes back from the
 * camera and return this camera response to the caller.
 *
 * Example :
 *
 *	This function gets called with
 *		#canon_funct = CANON_USB_FUNCTION_SET_TIME
 *		#payload = already constructed payload with the new time
 *	we construct a complete command packet and send this to the camera.
 *	The canon_usb_cmdstruct indicates that command
 *	CANON_USB_FUNCTION_SET_TIME returns four bytes, so we read those
 *	four bytes into our buffer and return a pointer to the buffer to
 *	the caller.
 *
 *	This should probably be changed so that the caller supplies a
 *	unsigned char ** which can be pointed to our buffer and an int
 *	returned with GP_OK or some error code.
 *
 **/
unsigned char *
canon_usb_dialogue (Camera *camera, int canon_funct, int *return_length,
		    const char *payload, int payload_length)
{
	int msgsize, status, i;
	char cmd1 = 0, cmd2 = 0, *funct_descr = "";
	int cmd3 = 0, read_bytes = 0;
	unsigned char packet[1024];	// used for sending data to camera
	static unsigned char buffer[0x9c];	// used for receiving data from camera

	/* clear this to indicate that no data is there if we abort */
	if (return_length)
		*return_length = 0;

	/* clearing the receive buffer could be done right before the gp_port_read()
	 * but by clearing it here we eliminate the possibility that a caller thinks
	 * data in this buffer is a result of this particular canon_usb_dialogue() call
	 * if we return error but this is not checked for... good or bad I don't know.
	 */
	memset (buffer, 0x00, sizeof (buffer));

	/* search through the list of known canon commands (canon_usb_cmd)
	 * and look for parameters to be used for function 'canon_funct'
	 */
	i = 0;
	while (canon_usb_cmd[i].num != 0) {
		if (canon_usb_cmd[i].num == canon_funct) {
			funct_descr = canon_usb_cmd[i].description;
			cmd1 = canon_usb_cmd[i].cmd1;
			cmd2 = canon_usb_cmd[i].cmd2;
			cmd3 = canon_usb_cmd[i].cmd3;
			read_bytes = canon_usb_cmd[i].return_length;
			break;
		}
		i++;
	}
	if (canon_usb_cmd[i].num == 0) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_dialogue() "
				 "called for ILLEGAL function %i! Aborting.", canon_funct);
		return NULL;
	}
	GP_DEBUG ("canon_usb_dialogue() cmd 0x%x 0x%x 0x%x (%s), payload = %i bytes",
		  cmd1, cmd2, cmd3, funct_descr, payload_length);

	if (read_bytes > sizeof (buffer)) {
		/* If this message is ever printed, chances are that you just added
		 * a new command to canon_usb_cmd with a return_length greater than
		 * all the others and did not update the declaration of 'buffer' in
		 * this function.
		 */
		GP_DEBUG ("canon_usb_dialogue() "
			  "read_bytes %i won't fit in buffer of size %i!",
			  read_bytes, sizeof (buffer));
		return NULL;
	}

	if (payload_length) {
		GP_DEBUG ("Got payload.");
		gp_log_data ("canon", payload, payload_length);
	}

	if ((payload_length + 0x50) > sizeof (packet)) {
		GP_LOG (GP_LOG_VERBOSE,
			"canon_usb_dialogue: payload too big, won't fit into buffer (%i > %i)",
			(payload_length + 0x50), sizeof (packet));
		return NULL;
	}

	/* OK, we have now checked for all errors I could think of,
	 * proceed with the actual work.
	 */

	/* construct packet to send to camera, including the three
	 * commands, serial number and a payload if one has been supplied
	 */

	memset (packet, 0x00, sizeof (packet));	/* zero block */
	intatpos (packet, 0x0, 0x10 + payload_length);
	packet[0x40] = 0x2;
	packet[0x44] = cmd1;
	packet[0x47] = cmd2;
	intatpos (packet, 0x04, cmd3);
	intatpos (packet, 0x4c, 0x12345678);	/* fake serial number */
	intatpos (packet, 0x48, 0x10 + payload_length);

	msgsize = 0x50 + payload_length;	/* TOTAL msg size */

	if (payload_length > 0)
		memcpy (packet + 0x50, payload, payload_length);

	/* now send the packet to the camera */
	status = gp_port_usb_msg_write (camera->port, msgsize > 1 ? 0x04 : 0x0c, 0x10, 0,
					packet, msgsize);
	if (status != msgsize) {
		GP_DEBUG ("canon_usb_dialogue: write failed! (returned %i)\n", status);
		return NULL;
	}

	/* and, if this canon_funct is known to generate a response from the camera,
	 * read this response back.
	 */
	status = gp_port_read (camera->port, buffer, read_bytes);
	if (status != read_bytes) {
		GP_DEBUG ("canon_usb_dialogue: read failed! "
			  "(returned %i, expected %i)\n", status, read_bytes);
		return NULL;
	}


	/* if cmd3 equals to 0x202, this is a command that returns L (long) data
	 * and what we return here is the complete packet (ie. not skipping the
	 * first 0x50 bytes we otherwise would) so that the caller
	 * (which is canon_usb_long_dialogue()) can find out how much data to
	 * read from the USB port by looking at offset 6 in this packet.
	 */
	if (cmd3 == 0x202) {
		if (return_length)
			*return_length = read_bytes;
		return buffer;
	} else {
		if (return_length)
			*return_length = (read_bytes - 0x50);
		return buffer + 0x50;
	}
}

/**
 * canon_usb_long_dialogue:
 * @camera: the Camera to work with
 * @canon_funct: integer constant that identifies function we are execute
 * @payload: data we are to send to the camera
 * @payload_length: length of #payload
 *
 * This function is used to invoke camera commands which return L (long) data.
 * It calls #canon_usb_dialogue(), if it gets a good response it will malloc()
 * memory and read the entire returned data into this malloc'd memory and store
 * a pointer to the malloc'd memory in 'data'.
 **/
int
canon_usb_long_dialogue (Camera *camera, int canon_funct, unsigned char **data,
			 int *data_length, int max_data_size, const char *payload,
			 int payload_length, int display_status)
{
	int bytes_read;
	unsigned int total_data_size = 0, bytes_received = 0, read_bytes = USB_BULK_READ_SIZE;
	char *lpacket;		/* "length packet" */

	/* indicate there is no data if we bail out somewhere */
	*data_length = 0;

	if (display_status)
		gp_camera_progress (camera, 0);

	GP_DEBUG ("canon_usb_long_dialogue() function %i, payload = %i bytes",
		  canon_funct, payload_length);

	/* Call canon_usb_dialogue(), this will not return any data "the usual way"
	 * but after this we read 0x40 bytes from the USB port, the int at pos 6 in
	 * the returned data holds the total number of bytes we are to read.
	 *
	 */
	lpacket =
		canon_usb_dialogue (camera, canon_funct, &bytes_read, payload, payload_length);
	if (lpacket == NULL) {
		GP_DEBUG ("canon_usb_long_dialogue: canon_usb_dialogue " "returned error!");
		return GP_ERROR;
	}
	/* This check should not be needed since we check the return of canon_usb_dialogue()
	 * above, but as the saying goes: better safe than sorry.
	 */
	if (bytes_read != 0x40) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "canon_usb_long_dialogue: canon_usb_dialogue "
				 "did not return (%i bytes) the number of bytes "
				 "we expected (%i)!. Aborting.", bytes_read, 0x40);
		return GP_ERROR;
	}

	total_data_size = *(unsigned *) (lpacket + 0x6);

	if (max_data_size && (total_data_size > max_data_size)) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_long_dialogue: "
				 "ERROR: Packet of size %i is too big "
				 "(max reasonable size specified is %i)",
				 total_data_size, max_data_size);
		return GP_ERROR;
	}
	*data = malloc (total_data_size);
	if (!*data) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_long_dialogue: "
				 "ERROR: Could not allocate %i bytes of memory",
				 total_data_size);
		return GP_ERROR_NO_MEMORY;
	}

	bytes_received = 0;
	while (bytes_received < total_data_size) {
		if ((total_data_size - bytes_received) < read_bytes)
			read_bytes = (total_data_size - bytes_received);

		gp_debug_printf (GP_DEBUG_HIGH, "canon",
				 "calling gp_port_read(), total_data_size = %i, "
				 "bytes_received = %i, read_bytes = %i (0x%x)",
				 total_data_size, bytes_received, read_bytes, read_bytes);
		bytes_read = gp_port_read (camera->port, *data + bytes_received, read_bytes);
		if (bytes_read < 1) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "gp_port_read() returned error (%i) or no data\n",
					 bytes_read);
			free (*data);

			/* here, it is an error to get 0 bytes from gp_port_read()
			 * too, but 0 is GP_OK so if bytes_read is 0 return GP_ERROR
			 * instead, otherwise return bytes_read since that is the
			 * error code returned by gp_port_read()
			 */
			if (bytes_read < 0)
				return bytes_read;
			else
				return GP_ERROR;
		}

		if (bytes_read < read_bytes)
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "WARNING: gp_port_read() resulted in short read "
					 "(returned %i bytes, expected %i)",
					 bytes_read, read_bytes);
		bytes_received += bytes_read;

		if (display_status)
			gp_camera_progress (camera,
					    total_data_size ? (bytes_received /
							       (float) total_data_size) : 1.);
	}

	*data_length = total_data_size;

	return GP_OK;
}


int
canon_usb_get_file (Camera *camera, const char *name, unsigned char **data, int *length)
{
	char payload[100];
	int payload_length, maxfilesize, res;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_get_file() called for file '%s'",
			 name);

	/* 8 is strlen ("12111111") */
	if (8 + strlen (name) > sizeof (payload) - 1) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_get_file: ERROR: "
				 "Supplied file name '%s' does not fit in payload buffer.",
				 name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Construct payload containing file name, buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	sprintf (payload, "12111111%s", name);
	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_get_file: payload %s", payload);
	payload_length = strlen (payload) + 1;
	intatpos (payload, 0x0, 0x0);	// get picture
	intatpos (payload, 0x4, USB_BULK_READ_SIZE);

	if (camera->pl->model == CANON_PS_S10 || camera->pl->model == CANON_PS_S20
	    || camera->pl->model == CANON_PS_S30 || camera->pl->model == CANON_PS_S40
	    || camera->pl->model == CANON_PS_G2 || camera->pl->model == CANON_PS_G1
	    || camera->pl->model == CANON_PS_S300 || camera->pl->model == CANON_PS_S100
	    || camera->pl->model == CANON_PS_A10 || camera->pl->model == CANON_PS_A20
	    || camera->pl->model == CANON_EOS_D30 || camera->pl->model == CANON_PS_PRO90_IS) {
		maxfilesize = 10000000;
	} else {
		maxfilesize = 2000000;
	}

	/* the 1 is to show status */
	res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
				       maxfilesize, payload, payload_length, 1);
	if (res != GP_OK) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "canon_usb_get_file: canon_usb_long_dialogue() "
				 "returned error (%i).", res);
		return res;
	}

	return GP_OK;
}

int
canon_usb_get_thumbnail (Camera *camera, const char *name, unsigned char **data, int *length)
{
	char payload[100];
	int payload_length, res;

	/* 8 is strlen ("11111111") */
	if (8 + strlen (name) > sizeof (payload) - 1) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_get_thumbnail: ERROR: "
				 "Supplied file name '%s' does not fit in "
				 "payload buffer.", name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Construct payload containing file name, buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	sprintf (payload, "11111111%s", name);
	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_usb_get_thumbnail: "
			 "payload %s", payload);
	payload_length = strlen (payload) + 1;

	intatpos (payload, 0x0, 0x1);	// get thumbnail
	intatpos (payload, 0x4, USB_BULK_READ_SIZE);

	/* 0 is to not show status */
	res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
				       32 * 1024, payload, payload_length, 0);

	if (res != GP_OK) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "canon_usb_get_thumbnail: canon_usb_long_dialogue() "
				 "returned error (%i).", res);
		return res;
	}

	return GP_OK;
}

/*
 * Upload to USB Camera
 *
 */
int
canon_usb_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath)
{
	gp_camera_message (camera, _("Not implemented!"));
	return GP_ERROR_NOT_SUPPORTED;
}

int
canon_usb_get_dirents (Camera *camera, unsigned char **dirent_data,
		       unsigned int *dirents_length, const char *path)
{
	unsigned char payload[100];
	unsigned int payload_length;
	int res;

	*dirent_data = NULL;

	/* build payload :
	 *
	 * 0x00 dirname 0x00 0x00 0x00
	 *
	 * the 0x00 before dirname means 'no recursion'
	 * NOTE: the first 0x00 after dirname is the NULL byte terminating
	 * the string, so payload_length is strlen(path) + 4 
	 */
	if (strlen (path) + 4 > sizeof (payload)) {
		GP_DEBUG ("canon_usb_get_dirents: "
			  "Path '%s' too long (%i), won't fit in payload "
			  "buffer.", path, strlen (path));
		gp_camera_set_error (camera, "canon_usb_get_dirents: "
				     "Couldn't fit payload into buffer, "
				     "'%.96s' (truncated) too long.", path);
		return GP_ERROR_BAD_PARAMETERS;
	}
	memset (payload, 0x00, sizeof (payload));
	memcpy (payload + 1, path, strlen (path));
	payload_length = strlen (path) + 4;

	/* 1024 * 1024 is max realistic data size, out of the blue.
	 * 0 is to not show progress status
	 */
	res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_DIRENT,
				       dirent_data, dirents_length, 1024 * 1024, payload,
				       payload_length, 0);
	if (res != GP_OK) {
		gp_camera_set_error (camera, "canon_usb_get_dirents: "
				     "canon_usb_long_dialogue failed to fetch direntrys, "
				     "returned %i", res);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * canon_usb_ready:
 * @camera: camera to get ready
 * @Returns: gphoto2 error code
 *
 * USB part of canon_int_ready
 **/
int
canon_usb_ready (Camera *camera)
{
	int res;

	GP_DEBUG ("canon_usb_ready()");

	res = canon_int_identify_camera (camera);
	if (res != GP_OK) {
		gp_camera_set_error (camera, "Camera not ready, "
				     "identify camera request failed (returned %i)", res);
		return GP_ERROR;
	}
	if (!strcmp ("Canon PowerShot S20", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S20");
		camera->pl->model = CANON_PS_S20;
	} else if (!strcmp ("Canon PowerShot S10", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S10");
		camera->pl->model = CANON_PS_S10;
	} else if (!strcmp ("Canon PowerShot S30", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S30");
		camera->pl->model = CANON_PS_S30;
	} else if (!strcmp ("Canon PowerShot S40", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot S40");
		camera->pl->model = CANON_PS_S40;
	} else if (!strcmp ("Canon PowerShot G1", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot G1");
		camera->pl->model = CANON_PS_G1;
	} else if (!strcmp ("Canon PowerShot G2", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot G2");
		camera->pl->model = CANON_PS_G2;
	} else if ((!strcmp ("Canon DIGITAL IXUS", camera->pl->ident))
		   || (!strcmp ("Canon IXY DIGITAL", camera->pl->ident))
		   || (!strcmp ("Canon PowerShot S110", camera->pl->ident))
		   || (!strcmp ("Canon PowerShot S100", camera->pl->ident))
		   || (!strcmp ("Canon DIGITAL IXUS v", camera->pl->ident))) {
		gp_camera_status (camera,
				  "Detected a Digital IXUS series / IXY DIGITAL / PowerShot S100 series");
		camera->pl->model = CANON_PS_S100;
	} else if ((!strcmp ("Canon DIGITAL IXUS 300", camera->pl->ident))
		   || (!strcmp ("Canon IXY DIGITAL 300", camera->pl->ident))
		   || (!strcmp ("Canon PowerShot S300", camera->pl->ident))) {
		gp_camera_status (camera,
				  "Detected a Digital IXUS 300 / IXY DIGITAL 300 / PowerShot S300");
		camera->pl->model = CANON_PS_S300;
	} else if (!strcmp ("Canon PowerShot A10", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot A10");
		camera->pl->model = CANON_PS_A10;
	} else if (!strcmp ("Canon PowerShot A20", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot A20");
		camera->pl->model = CANON_PS_A20;
	} else if (!strcmp ("Canon EOS D30", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a EOS D30");
		camera->pl->model = CANON_EOS_D30;
	} else if (!strcmp ("Canon PowerShot Pro90 IS", camera->pl->ident)) {
		gp_camera_status (camera, "Detected a PowerShot Pro90 IS");
		camera->pl->model = CANON_PS_PRO90_IS;
	} else {
		gp_camera_set_error (camera, "Unknown camera! (%s)", camera->pl->ident);
		return GP_ERROR;
	}

	res = canon_usb_keylock (camera);
	if (res != GP_OK) {
		gp_camera_set_error (camera, "Camera not ready, "
				     "could not lock camera keys (returned %i)", res);
		return res;
	}

	res = canon_int_get_time (camera);
	if (res == GP_ERROR) {
		gp_camera_set_error (camera, "Camera not ready, "
				     "get time request failed (returned %i)", res);
		return GP_ERROR;
	}

	gp_camera_status (camera, _("Connected to camera"));

	return GP_OK;
}

