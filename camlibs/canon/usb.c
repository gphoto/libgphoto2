/****************************************************************************
 *
 * File: usb.c
 *
 * USB communication layer.
 *
 * $Id$
 ****************************************************************************/

#include <config.h>

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

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}


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
canon_usb_camera_init (Camera *camera, GPContext *context)
{
	unsigned char msg[0x58];
	unsigned char buffer[0x44];
	int i, read_bytes;
	char *camstat_str = "NOT RECOGNIZED";
	unsigned char camstat;

	GP_DEBUG ("canon_usb_camera_init()");

	memset (msg, 0, sizeof (msg));
	memset (buffer, 0, sizeof (buffer));

	i = canon_usb_identify (camera, context);
	if (i != GP_OK)
		return i;

	i = gp_port_usb_msg_read (camera->port, 0x0c, 0x55, 0, msg, 1);
	if (i != 1) {
		gp_context_error (context, "Could not establish initial contact with camera");
		return GP_ERROR_CORRUPTED_DATA;
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
		gp_context_error (context, "Initial camera response %c/'%s' unrecognized",
				  camstat, camstat_str);
		return GP_ERROR_CORRUPTED_DATA;
	}
	GP_DEBUG ("canon_usb_camera_init() initial camera response: %c/'%s'", camstat,
		  camstat_str);

	i = gp_port_usb_msg_read (camera->port, 0x04, 0x1, 0, msg, 0x58);
	if (i != 0x58) {
		gp_context_error (context,
				  "Step #2 of initialization failed! (returned %i, expected %i) "
				  "Camera not operational", i, 0x58);
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (camstat == 'A') {
		/* read another 0x50 bytes */
		i = gp_port_usb_msg_read (camera->port, 0x04, 0x4, 0, msg, 0x50);
		if (i != 0x50) {
			gp_context_error (context,
					  "EOS D30 Step #3 of initialization failed! "
					  "(returned %i, expected %i) "
					  "Camera not operational", i, 0x50);
			return GP_ERROR_CORRUPTED_DATA;
		}
		return GP_OK;
	}

	/* set byte 0 in msg to new "canon length" (0x10) (which is total
	 * packet size - 0x40) and then move the last 0x10 bytes of msg to
	 * offset 0x40 and write it back to the camera.
	 */
	msg[0] = 0x10;
	memmove (msg + 0x40, msg + 0x48, 0x10);

	i = gp_port_usb_msg_write (camera->port, 0x04, 0x11, 0, msg, 0x50);
	if (i != 0x50) {
		gp_context_error (context,
				  "Step #3 of initialization failed! "
				  "(returned %i, expected %i) Camera not operational", i,
				  0x50);
		return GP_ERROR_IO_INIT;
	}
	GP_DEBUG ("canon_usb_camera_init() "
		  "PC sign on LCD should be lit now (if your camera has a PC sign)");

	/* We expect to get 0x44 bytes here, but the camera is picky at this stage and
	 * we must read 0x40 bytes and then read 0x4 bytes more.
	 */
	i = gp_port_read (camera->port, buffer, 0x40);
	if ((i >= 4)
	    && (buffer[i - 4] == 0x54) && (buffer[i - 3] == 0x78)
	    && (buffer[i - 2] == 0x00) && (buffer[i - 1] == 0x00)) {

		/* We have some reports that sometimes the camera takes a long
		 * time to respond to the above read request and then comes back with
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
		gp_context_error (context,
				  "Step #4.1 failed! "
				  "(returned %i, expected %i) Camera not operational", i,
				  0x40);
		return GP_ERROR_CORRUPTED_DATA;
	}

	/* just check if (int) buffer[0] says 0x4 or not, log a warning if it doesn't. */
	read_bytes = le32atoh (buffer);
	if (read_bytes != 4)
		GP_DEBUG ("canon_usb_camera_init() camera says to read %i more bytes, ",
			  "we wold have expected 4 - overriding since some cameras are "
			  "known not to give correct numbers of bytes.", read_bytes);

	i = gp_port_read (camera->port, buffer, 4);
	if (i != 4)
		GP_DEBUG ("canon_usb_camera_init() "
			  "Step #4.2 of initialization failed! (returned %i, expected %i) "
			  "Camera might still work though. Continuing.", i, 4);

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
canon_usb_init (Camera *camera, GPContext *context)
{
	int res, id_retry;
	GPPortSettings settings;

	GP_DEBUG ("Initializing the (USB) camera.\n");

	/* Get the current settings */
	gp_port_get_settings (camera->port, &settings);

	/* Use the default settings the core parsed */

	/* Set the new settings */
	res = gp_port_set_settings (camera->port, settings);
	if (res != GP_OK) {
		gp_context_error (context, _("Could not apply USB settings"));
		return res;
	}

	res = canon_usb_camera_init (camera, context);
	if (res != GP_OK)
		return res;

	/* We retry the identify camera because sometimes (camstat == 'A'
	 * in canon_usb_camera_init()) this is necessary to get the camera
	 * back in sync, and the windows driver actually executes four of
	 * these in a row before downloading thumbnails.
	 */
	res = GP_ERROR;
	for (id_retry = 1; id_retry <= 4; id_retry++) {
		res = canon_int_identify_camera (camera, context);
		if (res != GP_OK)
			GP_DEBUG ("Identify camera try %i/%i failed %s", id_retry, 4,
				  id_retry <
				  4 ? "(this is OK)" : "(now it's not OK any more)");
		else
			break;
	}
	if (res != GP_OK) {
		gp_context_error (context,
				  _("Camera not ready, "
				    "multiple 'Identify camera' requests failed: %s"),
				  gp_result_as_string (res));
		return GP_ERROR;
	}

	res = canon_usb_lock_keys (camera, context);
	if (res != GP_OK) {
		gp_context_error (context,
				  _("Camera not ready, could not lock camera keys: %s"),
				  gp_result_as_string (res));
		return res;
	}

	return GP_OK;
}

/**
 * canon_usb_lock_keys:
 * @camera: camera to lock keys on
 * @Returns: gphoto2 error code
 *
 * Lock the keys on the camera and turn off the display
 **/
int
canon_usb_lock_keys (Camera *camera, GPContext *context)
{
	unsigned char *c_res;
	int bytes_read;
	char payload[4];

	GP_DEBUG ("canon_usb_lock_keys()");

	switch (camera->pl->md->model) {
		case CANON_PS_G1:
		case CANON_PS_S100:
		case CANON_PS_S20:
		case CANON_PS_S10:
			GP_DEBUG ("canon_usb_lock_keys: Your camera model does not need the keylock.");
			break;
		case CANON_PS_PRO90_IS:
			GP_DEBUG ("canon_usb_lock_keys: Your camera model does not support keylocking.");
			break;
		case CANON_EOS_D30:
			memset (payload, 0, sizeof (payload));
			payload[0] = 0x06;

			c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_LOCK_KEYS,
						    &bytes_read, payload, 4);
			if (!c_res)
				return GP_ERROR;

			break;
		default:
			c_res = canon_usb_dialogue (camera,
						    CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,
						    &bytes_read, NULL, 0);
			if (bytes_read == 0x4) {
				GP_DEBUG ("canon_usb_lock_keys: Got the expected number of bytes back, unfortuntely we don't know what they mean.");
			} else {
				gp_context_error (context,
						  "canon_usb_lock_keys: "
						  "Unexpected amount of data returned (%i bytes, expected %i)",
						  bytes_read, 0x4);
				return GP_ERROR;
			}
			break;
	}

	return GP_OK;
}

/**
 * canon_usb_unlock_keys:
 * @camera: camera to unlock keys on
 * @Returns: gphoto2 error code
 *
 * Unlocks the keys on cameras that support this
 **/
int
canon_usb_unlock_keys (Camera *camera)
{
	unsigned char *c_res;
	int bytes_read;

	GP_DEBUG ("canon_usb_unlock_keys()");

	switch (camera->pl->md->model) {
		case CANON_EOS_D30:
			c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,
						    &bytes_read, NULL, 0);
			/* Should look at the bytes returned, but I don't know what they mean */
			if (!c_res)
				return GP_ERROR;

			break;
		default:
			/* Your camera model does not need unlocking, cannot do unlocking or
			 * we don't know how to unlock it's keys. If unlocking works when
			 * using the Windows software with your camera, please contact
			 * <gphoto-devel@gphoto.net>
			 */
			GP_DEBUG ("canon_usb_unlock_keys: Not unlocking the kind of camera you have.");
			break;
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
 * @Returns: a char * that points to the data read from the camera (or
 * NULL on failure), and sets what @return_length points to to the number
 * of bytes read.
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
canon_usb_dialogue (Camera *camera, int canon_funct, int *return_length, const char *payload,
		    int payload_length)
{
	int msgsize, status, i;
	char cmd1 = 0, cmd2 = 0, *funct_descr = "";
	int cmd3 = 0, read_bytes = 0, read_bytes1 = 0, read_bytes2 = 0;
	unsigned char packet[1024];	/* used for sending data to camera */
	static unsigned char buffer[0x9c];	/* used for receiving data from camera */

#ifdef EXPERIMENTAL_CAPTURE
	int j, canon_subfunc = 0;
	char subcmd = 0, *subfunct_descr = "";
	int additional_read_bytes = 0, returned_read_bytes = 0;
#endif

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
		GP_DEBUG ("canon_usb_dialogue() called for ILLEGAL function %i! Aborting.",
			  canon_funct);
		return NULL;
	}
	GP_DEBUG ("canon_usb_dialogue() cmd 0x%x 0x%x 0x%x (%s)", cmd1, cmd2, cmd3,
		  funct_descr);

#ifdef EXPERIMENTAL_CAPTURE
	/*
	 * The CONTROL_CAMERA function is special in that it's payload specifies a 
	 * subcommand, and the size of the return data is dependent on which
	 * subcommand we're sending the camera.  See "Protocol" file for details.
	 */
	if (canon_usb_cmd[i].num == CANON_USB_FUNCTION_CONTROL_CAMERA) {
		canon_subfunc = le32atoh (payload);
		j = 0;
		while (canon_usb_control_cmd[j].num != 0) {
			if (canon_usb_control_cmd[j].subcmd == canon_subfunc) {
				subfunct_descr = canon_usb_control_cmd[j].description;
				subcmd = canon_usb_control_cmd[j].subcmd;
				additional_read_bytes = canon_usb_control_cmd[j].additional_return_length;
				break;
			}
			j++;
		}
		if (canon_usb_control_cmd[j].num == 0) {
			GP_DEBUG("canon_usb_dialogue(): CONTROL_CAMERA called for ILLEGAL "
				 "sub function %i! Aborting.", canon_subfunc);
			return NULL;
		}
		read_bytes += additional_read_bytes;

		GP_DEBUG ("canon_usb_dialogue() called with CONTROL_CAMERA, %s",
			  canon_usb_control_cmd[j].description);
	}
#endif /* EXPERIMENTAL_CAPTURE */

	if (read_bytes > sizeof (buffer)) {
		/* If this message is ever printed, chances are that you just added
		 * a new command to canon_usb_cmd with a return_length greater than
		 * all the others and did not update the declaration of 'buffer' in
		 * this function.
		 */
		GP_DEBUG ("canon_usb_dialogue() "
			  "read_bytes %i won't fit in buffer of size %i!", read_bytes,
			  sizeof (buffer));
		return NULL;
	}

	if (payload_length) {
		GP_DEBUG ("Payload :");
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
	htole32a (packet, 0x10 + payload_length);
	packet[0x40] = 0x2;
	packet[0x44] = cmd1;
	packet[0x47] = cmd2;
	htole32a (packet + 0x04, cmd3);
	htole32a (packet + 0x4c, 0x12345678);	/* fake serial number */
	htole32a (packet + 0x48, 0x10 + payload_length);

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

#ifdef EXPERIMENTAL_CAPTURE
	// TESTING

	if (camera->pl->capturing)
		sleep(2);

	// OTHER POSSIBLE TEST
	//if (cmd1 == 0x13 && cmd2 == 0x12 && cmd3 == 0x201) {
	//	GP_DEBUG ("sleeping 2 sec...");
	//	sleep(3);
	//	GP_DEBUG ("...done");
	//}

	//if (cmd1 == 0x17 && cmd2 == 0x12 && cmd3 == 0x202) {
	//	GP_DEBUG ("capture command, sleeping 5 sec...");
	//	sleep(5);
	//	GP_DEBUG("...done");
	//}
#endif


	/* Divide read_bytes into two parts (two reads), one that is the highest
	 * ammount of 0x40 byte blocks we can get, and one that is the modulus (the rest).
	 * This is done because it is how the windows driver does it, and some cameras
	 * (EOS D30 for example) seem to not like it if we were to read read_bytes
	 * in a single read instead.
	 */
	read_bytes1 = read_bytes - (read_bytes % 0x40);
	read_bytes2 = read_bytes - read_bytes1;

	status = gp_port_read (camera->port, buffer, read_bytes1);
	if (status != read_bytes1) {
		GP_DEBUG ("canon_usb_dialogue: read 1 failed! (returned %i, expected %i)",
			  status, read_bytes1);
		return NULL;
	}

	if (read_bytes2) {
		status = gp_port_read (camera->port, buffer + read_bytes1, read_bytes2);
		if (status != read_bytes2) {
			GP_DEBUG ("canon_usb_dialogue: read 2 failed! "
				  "(returned %i, expected %i)", status, read_bytes2);
			return NULL;
		}
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
 * @data: Pointer to pointer to allocated memory holding the data returned from the camera
 * @data_length: Pointer to where you want the number of bytes read from the camera
 * @max_data_size: Max realistic data size so that we can abort if something goes wrong
 * @payload: data we are to send to the camera
 * @payload_length: length of #payload
 * @display_status: Whether you want progress bar for this operation or not
 *
 * This function is used to invoke camera commands which return L (long) data.
 * It calls #canon_usb_dialogue(), if it gets a good response it will malloc()
 * memory and read the entire returned data into this malloc'd memory and store
 * a pointer to the malloc'd memory in 'data'.
 **/
int
canon_usb_long_dialogue (Camera *camera, int canon_funct, unsigned char **data,
			 int *data_length, int max_data_size, const char *payload,
			 int payload_length, int display_status, GPContext *context)
{
	int bytes_read;
	unsigned int total_data_size = 0, bytes_received = 0, read_bytes = USB_BULK_READ_SIZE;
	char *lpacket;		/* "length packet" */
	unsigned int id = 0;

	/* indicate there is no data if we bail out somewhere */
	*data_length = 0;

	GP_DEBUG ("canon_usb_long_dialogue() function %i, payload = %i bytes", canon_funct,
		  payload_length);

	/* Call canon_usb_dialogue(), this will not return any data "the usual way"
	 * but after this we read 0x40 bytes from the USB port, the int at pos 6 in
	 * the returned data holds the total number of bytes we are to read.
	 */
	lpacket =
		canon_usb_dialogue (camera, canon_funct, &bytes_read, payload, payload_length);
	if (lpacket == NULL) {
		GP_DEBUG ("canon_usb_long_dialogue: canon_usb_dialogue returned error!");
		return GP_ERROR;
	}
	/* This check should not be needed since we check the return of canon_usb_dialogue()
	 * above, but as the saying goes: better safe than sorry.
	 */
	if (bytes_read != 0x40) {
		GP_DEBUG ("canon_usb_long_dialogue: canon_usb_dialogue "
			  "did not return (%i bytes) the number of bytes "
			  "we expected (%i)!. Aborting.", bytes_read, 0x40);
		return GP_ERROR_CORRUPTED_DATA;
	}

	total_data_size = le32atoh (lpacket + 0x6);

	if (display_status)
		id = gp_context_progress_start (context, total_data_size,
						_("Receiving data..."));

	if (max_data_size && (total_data_size > max_data_size)) {
		GP_DEBUG ("canon_usb_long_dialogue: ERROR: Packet of size %i is too big "
			  "(max reasonable size specified is %i)", total_data_size,
			  max_data_size);
		return GP_ERROR_CORRUPTED_DATA;
	}
	*data = malloc (total_data_size);
	if (!*data) {
		GP_DEBUG ("canon_usb_long_dialogue: "
			  "ERROR: Could not allocate %i bytes of memory", total_data_size);
		return GP_ERROR_NO_MEMORY;
	}

	bytes_received = 0;
	while (bytes_received < total_data_size) {
		if ((total_data_size - bytes_received) < read_bytes)
			read_bytes = (total_data_size - bytes_received);

		GP_DEBUG ("calling gp_port_read(), total_data_size = %i, "
			  "bytes_received = %i, read_bytes = %i (0x%x)", total_data_size,
			  bytes_received, read_bytes, read_bytes);
		bytes_read = gp_port_read (camera->port, *data + bytes_received, read_bytes);
		if (bytes_read < 1) {
			GP_DEBUG ("gp_port_read() returned error (%i) or no data\n",
				  bytes_read);
			free (*data);
			*data = NULL;

			/* here, it is an error to get 0 bytes from gp_port_read()
			 * too, but 0 is GP_OK so if bytes_read is 0 return GP_ERROR_CORRUPTED_DATA
			 * instead, otherwise return bytes_read since that is the
			 * error code returned by gp_port_read()
			 */
			if (bytes_read < 0)
				return bytes_read;
			else
				return GP_ERROR_CORRUPTED_DATA;
		}

		if (bytes_read < read_bytes)
			GP_DEBUG ("WARNING: gp_port_read() resulted in short read "
				  "(returned %i bytes, expected %i)", bytes_read, read_bytes);
		bytes_received += bytes_read;

		if (display_status)
			gp_context_progress_update (context, id, bytes_received);
	}
	if (display_status)
		gp_context_progress_stop (context, id);

	*data_length = total_data_size;

	return GP_OK;
}


int
canon_usb_get_file (Camera *camera, const char *name, unsigned char **data, int *length,
		    GPContext *context)
{
	char payload[100];
	int payload_length, res;

	GP_DEBUG ("canon_usb_get_file() called for file '%s'", name);

	/* 8 is strlen ("12111111") */
	if (8 + strlen (name) > sizeof (payload) - 1) {
		GP_DEBUG ("canon_usb_get_file: ERROR: "
			  "Supplied file name '%s' does not fit in payload buffer.", name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Construct payload containing file name, buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	sprintf (payload, "12111111%s", name);
	GP_DEBUG ("canon_usb_get_file: payload %s", payload);
	payload_length = strlen (payload) + 1;
	htole32a (payload, 0x0);	/* get picture */
	htole32a (payload + 0x4, USB_BULK_READ_SIZE);

	/* the 1 is to show status */
	res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
				       camera->pl->md->max_picture_size, payload,
				       payload_length, 1, context);
	if (res != GP_OK) {
		GP_DEBUG ("canon_usb_get_file: canon_usb_long_dialogue() "
			  "returned error (%i).", res);
		return res;
	}

	return GP_OK;
}

int
canon_usb_get_thumbnail (Camera *camera, const char *name, unsigned char **data, int *length,
			 GPContext *context)
{
	char payload[100];
	int payload_length, res;

	/* 8 is strlen ("11111111") */
	if (8 + strlen (name) > sizeof (payload) - 1) {
		GP_DEBUG ("canon_usb_get_thumbnail: ERROR: "
			  "Supplied file name '%s' does not fit in payload buffer.", name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Construct payload containing file name, buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	sprintf (payload, "11111111%s", name);
	GP_DEBUG ("canon_usb_get_thumbnail: payload %s", payload);
	payload_length = strlen (payload) + 1;

	htole32a (payload, 0x1);	/* get thumbnail */
	htole32a (payload + 0x4, USB_BULK_READ_SIZE);

	/* 0 is to not show status */
	res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
				       camera->pl->md->max_thumbnail_size, payload,
				       payload_length, 0, context);

	if (res != GP_OK) {
		GP_DEBUG ("canon_usb_get_thumbnail: canon_usb_long_dialogue() "
			  "returned error (%i).", res);
		return res;
	}

	return GP_OK;
}

/*
 * Upload to USB Camera
 * 
 * at the moment, files bigger than 65572-length(filename), while being
 * correctly uploaded, cause the camera to not accept any more uploads.
 * Smaller files work fine.
 * s10sh (http://www.kyuzz.org/antirez/s10sh.html) has the same problem.
 * The problem only appears when USB deinitialisation and initialisation
 * is performed between uploads. You can call this function more than
 * once with big files during one session without encountering the problem 
 * described. <kramm@quiss.org>
 * 
 */
int
canon_usb_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath,
		    GPContext *context)
{
#ifndef EXPERIMENTAL_UPLOAD
	return GP_ERROR_NOT_SUPPORTED;
#else
	long int packet_size = USB_BULK_WRITE_SIZE;
	char buffer[0x80];
	long int len1,len2;
	int status;
	long int offs = 0;
	char filename[256];
	int filename_len;
	const char *data;
	char *newdata = NULL;
	long int size;
	FILE *fi;
	const char *srcname;
	char *packet; 

	sprintf(filename, "%s%s", destpath, destname);
	filename_len = strlen(filename);
	
	packet = malloc(packet_size + filename_len + 0x5d);
	
	if(!packet) {
	    int len = packet_size + filename_len + 0x5d;
	    GP_DEBUG ("canon_put_file_usb: Couldn't reserve %d bytes of memory", len);
	    gp_context_error(context, "Out of memory: %d bytes needed.", len);
	    return GP_ERROR_NO_MEMORY;
	}
	
	CHECK_RESULT (gp_file_get_name (file, &srcname));

	if(!gp_file_get_data_and_size (file, &data, &size)) {
	    fi = fopen(srcname, "rb");
	    if(!fi) {
		GP_DEBUG ("canon_put_file_usb: Couldn't read from %s", srcname);
		gp_context_error(context, "Couldn't read from %s", srcname);
		free(packet);
		return GP_ERROR;
	    }
	    fseek(fi, 0, SEEK_END);
	    size = ftell(fi);
	    fseek(fi, 0, SEEK_SET);
	    newdata = data = malloc(size);
	    if(!newdata) {
		GP_DEBUG ("canon_put_file_usb: Couldn't reserve memory for %s", srcname);
		gp_context_error(context, "Out of memory: %d bytes needed.", size);
		free(packet);
		return GP_ERROR_NO_MEMORY;
	    }
	    fread(newdata, size, 1, fi);
	    fclose(fi);
	}
	while(offs < size) {
	    len2 = packet_size;
	    if(size - offs < len2)
		len2 = size - offs;
	    len1 = len2 + 0x1c + filename_len + 1;

	    memset(packet, 0, 0x40);
	    packet[4]=3;
	    packet[5]=2;
	    packet[6]=(0x40+len1)&255;
	    packet[7]=(0x40+len1)>>8;
	    packet[8]=(0x40+len1)>>16;
	    packet[9]=(0x40+len1)>>24;

	    /* now send the packet to the camera */
	    status = gp_port_usb_msg_write (camera->port, 0x04, 0x10, 0,
					    packet, 0x40);
	    if (status != 0x40) {
		    GP_DEBUG ("canon_put_file_usb: write 1 failed! (returned %i)\n", status);
		    gp_context_error(context, "File upload failed.");
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }

	    status = gp_port_read (camera->port, buffer, 0x40);
	    if (status != 0x40) {
		    GP_DEBUG ("canon_put_file_usb: read 1 failed! "
			      "(returned %i, expected %i)", status, 0x40);
		    gp_context_error(context, "File upload failed.");
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }

	    memset(packet, 0, len1 + 0x40);
	    packet[0x00] = len1&255;
	    packet[0x01] = len1>>8;
	    packet[0x02] = len1>>16;
	    packet[0x03] = len1>>24;
	    packet[0x04] = 3;
	    packet[0x05] = 4;
	    packet[0x40] = 2;
	    packet[0x44] = 3;
	    packet[0x47] = 0x11;
	    packet[0x48] = len1&255;
	    packet[0x49] = len1>>8;
	    packet[0x4a] = len1>>16;
	    packet[0x4b] = len1>>24;
	    packet[0x4c] = 0x78;
	    packet[0x4d] = 0x56;
	    packet[0x4e] = 0x34;
	    packet[0x4f] = 0x12;
	    packet[0x50] = 2;
	    packet[0x54] = offs&255;
	    packet[0x55] = offs>>8;
	    packet[0x56] = offs>>16;
	    packet[0x57] = offs>>24;
	    packet[0x58] = len2&255;
	    packet[0x59] = len2>>8;
	    packet[0x5a] = len2>>16;
	    packet[0x5b] = len2>>24;
	    strcpy(&packet[0x5c], filename); 
	    memcpy(&packet[0x5c+filename_len+1], &data[offs], len2);

	    status = gp_port_write (camera->port, packet, len1+0x40);
	    if (status != len1+0x40) {
		    GP_DEBUG ("canon_put_file_usb: write 2 failed! "
			      "(returned %i, expected %i)", status, len1+0x40);
		    gp_context_error(context, "File upload failed.");
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }

	    status = gp_port_read (camera->port, buffer, 0x5c);
	    if (status != 0x5c) {
		    GP_DEBUG ("canon_put_file_usb: read 2 failed! "
			      "(returned %i, expected %i)", status, 0x5c);
		    gp_context_error(context, "File upload failed.");
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }
	    offs += len2;
	}

	if(size > 65572-filename_len) {
	    gp_context_message(context, _("File was too big. You may have to turn your camera off and back on before uploading more files."));
	}

	if(newdata)
	    free(newdata);
	free(packet);
	return GP_OK;
#endif /* EXPERIMENTAL_UPLOAD */
}

int
canon_usb_get_dirents (Camera *camera, unsigned char **dirent_data,
		       unsigned int *dirents_length, const char *path, GPContext *context)
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
	 * the string, so payload_length is strlen(dirname) + 4 
	 */
	if (strlen (path) + 4 > sizeof (payload)) {
		GP_DEBUG ("canon_usb_get_dirents: "
			  "Path '%s' too long (%i), won't fit in payload buffer.", path,
			  strlen (path));
		gp_context_error (context,
				  "canon_usb_get_dirents: "
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
	res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_DIRENT, dirent_data,
				       dirents_length, 1024 * 1024, payload, payload_length, 0,
				       context);
	if (res != GP_OK) {
		gp_context_error (context,
				  "canon_usb_get_dirents: "
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
	GP_DEBUG ("canon_usb_ready()");

	/* XXX send a 'ping' packet and check that the camera is
	 * still alive.
	 */

	return GP_OK;
}

int
canon_usb_identify (Camera *camera, GPContext *context)
{
	CameraAbilities a;
	int i, res;

	res = gp_camera_get_abilities (camera, &a);
	if (res != GP_OK) {
		GP_DEBUG ("canon_usb_identify: Could not get camera abilities: %s",
			  gp_result_as_string (res));
		return res;
	}

	for (i = 0; models[i].id_str != NULL; i++) {
		if (models[i].usb_vendor && models[i].usb_product
		    && !strcmp (models[i].id_str, a.model)) {
			GP_DEBUG ("canon_usb_identify: model name match '%s'",
				  models[i].id_str);
			gp_context_status (context, _("Detected a '%s'."), models[i].id_str);
			camera->pl->md = (struct canonCamModelData *) &models[i];
			return GP_OK;
		}
	}

	gp_context_error (context, "Could not identify camera based on name '%s'", a.model);

	return GP_ERROR_MODEL_NOT_FOUND;
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
