/****************************************************************************
 *
 * File: usb.c
 *
 * USB communication layer.
 *
 * $Id$
 ****************************************************************************/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#ifdef CANON_EXPERIMENTAL_UPLOAD
/* For filestat to get file time for upload */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif /* CANON_EXPERIMENTAL_UPLOAD */
#ifdef OS2
#include <db.h>
#endif
#include <netinet/in.h>

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

/* WARNING: This destroys reentrancy of the code. Better to put this
 * in the camera descriptor somewhere. */
static int serial_code = 0;

/**
 * canon_usb_camera_init:
 * @camera: camera to initialize
 * @context: context for error reporting
 *  GP_OK on success.
 *  GP_ERROR on any error.
 *
 * Initializes the USB camera through a series of read/writes
 *
 * Returns: gphoto2 error code
 *
 */
int
canon_usb_camera_init (Camera *camera, GPContext *context)
{
	unsigned char msg[0x58];
	unsigned char buffer[0x44];
	int i, read_bytes;
	char *camstat_str = _("NOT RECOGNIZED");
	unsigned char camstat;

	GP_DEBUG ("canon_usb_camera_init()");

	memset (msg, 0, sizeof (msg));
	memset (buffer, 0, sizeof (buffer));

	i = canon_usb_identify (camera, context);
	if (i != GP_OK)
		return i;

	i = gp_port_usb_msg_read (camera->port, 0x0c, 0x55, 0, msg, 1);
	if (i != 1) {
		gp_context_error (context, _("Could not establish initial contact with camera"));
		return GP_ERROR_CORRUPTED_DATA;
	}
	camstat = msg[0];
	switch (camstat) {
		case 'A':
			camstat_str = _("Camera was already active");
			break;
		case 'C':
			camstat_str = _("Camera was woken up");
			break;
		case 'I':
		case 'E':
			camstat_str = _("Unknown (some kind of error))");
			break;
	}
	if (camstat != 'A' && camstat != 'C') {
		gp_context_error (context, _("Initial camera response %c/'%s' unrecognized)"),
				  camstat, camstat_str);
		return GP_ERROR_CORRUPTED_DATA;
	}
	GP_DEBUG ("canon_usb_camera_init() initial camera response: %c/'%s'", camstat,
		  camstat_str);

	/* The 50-byte message seems not to arrive from a D30 */
	if ( !IS_EOS(camera->pl->md->model) ) {
		i = gp_port_usb_msg_read (camera->port, 0x04, 0x1, 0, msg, 0x58);
		if (i != 0x58) {
			gp_context_error (context,
					  _("Step #2 of initialization failed for PowerShot camera! (returned %i, expected %i) "
					  "Camera not operational"), i, 0x58);
			return GP_ERROR_CORRUPTED_DATA;
		}
	}

	if (camstat == 'A') {
		/* read another 0x50 bytes, except from EOS cameras */
		if ( !IS_EOS(camera->pl->md->model) ) {
			i = gp_port_usb_msg_read (camera->port, 0x04, 0x4, 0, msg, 0x50);
			if (i != 0x50) {
				gp_context_error (context,
						  _("EOS Step #3 of initialization failed! "
						  "(returned %i, expected %i) "
						  "Camera not operational"), i, 0x50);
				return GP_ERROR_CORRUPTED_DATA;
			}
		}
		return GP_OK;
	}
	else {
		/* Read this from EOS camera only if camera wasn't active. */
		if ( IS_EOS(camera->pl->md->model) ) {
			i = gp_port_usb_msg_read (camera->port, 0x04, 0x1, 0, msg, 0x58);
			if (i != 0x58) {
				gp_context_error (context,
						  _("Step #2 of initialization failed for EOS camera! (returned %i, expected %i) "
						  "Camera not operational"), i, 0x58);
				return GP_ERROR_CORRUPTED_DATA;
			}
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
					  _("Step #3 of initialization failed! "
					  "(returned %i, expected %i) Camera not operational"), i,
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
		}
		else {
			if (i != 0x40) {
				gp_context_error (context,
						  _("Step #4.1 failed! "
						  "(returned %i, expected %i) Camera not operational"), i,
						  0x40);
				return GP_ERROR_CORRUPTED_DATA;
			}
		}

		/* just check if (int) buffer[0] says 0x4 or not, log a warning if it doesn't. */
		read_bytes = le32atoh (buffer);
		if (read_bytes != 4)
			GP_DEBUG ("canon_usb_camera_init() camera says to read %i more bytes, ",
				  "we would have expected 4 - overriding since some cameras are "
				  "known not to give correct numbers of bytes.", read_bytes);

		i = gp_port_read (camera->port, buffer, 4);
		if (i != 4)
			GP_DEBUG ("canon_usb_camera_init() "
				  "Step #4.2 of initialization failed! (returned %i, expected %i) "
				  "Camera might still work though. Continuing.", i, 4);

		read_bytes = 0;
		do {
			i = gp_port_check_int_fast ( camera->port, buffer, 0x10 );
			if ( i > 0 )
				read_bytes += i;
		} while ( read_bytes < 0x10 && i >= 0 );
		if ( read_bytes!= 0x10 ) {
			GP_DEBUG ( "canon_usb_camera_init() interrupt read failed, status=%d", i );
			return GP_ERROR_CORRUPTED_DATA;
		}
	}

	if ( !IS_EOS(camera->pl->md->model) )
		if(canon_usb_lock_keys(camera,context) < 0) {
			gp_context_error (context, _("lock keys failed."));
			return GP_ERROR_CORRUPTED_DATA;
		}

	return GP_OK;
}

/**
 * canon_usb_init:
 * @camera: camera to initialize
 * @context: context for error reporting
 *
 * Initializes the given USB device.
 *
 * Returns: gphoto2 error code (%GP_OK on success).
 *
 */
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

	res = canon_int_get_battery(camera, NULL, NULL, context);
	if (res != GP_OK) {
		gp_context_error (context, _("Camera not ready, get_battery failed: %s"),
				  gp_result_as_string (res));
		return res; 
	}

	return GP_OK;
}

/**
 * canon_usb_lock_keys:
 * @camera: camera to lock keys on
 * @context: context for error reporting
 *
 * Lock the keys on the camera and turn off the display
 *
 * Returns: gphoto2 error code
 *
 */
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
		case CANON_EOS_D60:
                case CANON_EOS_10D:
		case CANON_PS_S230:
		case CANON_PS_S400:
			GP_DEBUG ("Locking camera keys and turning off LCD using 'EOS' locking code...");

			memset (payload, 0, sizeof (payload));
			payload[0] = 0x06;

			c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_LOCK_KEYS,
						    &bytes_read, payload, 4);
			if (!c_res)
				return GP_ERROR;
			if (bytes_read == 0x4) {
				GP_DEBUG ("canon_usb_lock_keys: Got the expected number of bytes back.");
			} else {
				gp_context_error (context,
						  _("canon_usb_lock_keys: "
						  "Unexpected amount of data returned (%i bytes, expected %i)"),
						  bytes_read, 0x4);
				return GP_ERROR;
			}

			break;

		case CANON_PS_S45:
		case CANON_PS_G3:
		default:
			/* Special case: doesn't implement "get
                           picture abilities", but isn't an EOS
                           camera, so we have to use the "normal" key
                           lock command. Since the S45 is a relatively
                           new model (in Jan. 2003), I suspect that we
                           will find more cameras in the future that
                           work this way. */
			GP_DEBUG ("Locking camera keys and turning off LCD using special-case S45 locking code...");
			c_res = canon_usb_dialogue (camera,
						    CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,
						    &bytes_read, NULL, 0);
			if (bytes_read == 0x4) {
				GP_DEBUG ("canon_usb_lock_keys: Got the expected number of bytes back.");
			} else {
				gp_context_error (context,
						  _("canon_usb_lock_keys: "
						  "Unexpected amount of data returned (%i bytes, expected %i)"),
						  bytes_read, 0x4);
				return GP_ERROR;
			}
			break;

		case CANON_PS_A5:
		case CANON_PS_A5_ZOOM:
		case CANON_PS_A50:
		case CANON_PS_S30:
		case CANON_PS_S40:
		case CANON_PS_PRO70:
		case CANON_PS_S300:
		case CANON_PS_G2:
		case CANON_PS_A10:
		case CANON_PS_A20:
		case CANON_PS_A30:
		case CANON_PS_A40:
		case CANON_PS_S330:
		case CANON_PS_S200:
		case CANON_PS_A100:
		case CANON_PS_A200:
		case CANON_OPT_200:
			/* Previous default; I doubt that any new
			 * cameras will work this way, so it now is
			 * explicit for the older cameras that use
			 * it. */
			GP_DEBUG ("Locking camera keys and turning off LCD using 'normal' locking code...");

			c_res = canon_usb_dialogue (camera,
						    CANON_USB_FUNCTION_GET_PIC_ABILITIES,
						    &bytes_read, NULL, 0);
			if ( bytes_read == 0x334 ) {
				GP_DEBUG ( "canon_usb_lock_keys: Got the expected number of bytes back from \"get picture abilities.\"" );
			} else {
				gp_context_error ( context,
						   _("canon_usb_lock_keys: "
						   "Unexpected return of %i bytes (expected %i) from \"get picture abilities.\""),
						   bytes_read, 0x334 );
				return GP_ERROR;
			}
			c_res = canon_usb_dialogue (camera,
						    CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,
						    &bytes_read, NULL, 0);
			if (bytes_read == 0x4) {
				GP_DEBUG ("canon_usb_lock_keys: Got the expected number of bytes back.");
			} else {
				gp_context_error (context,
						  _("canon_usb_lock_keys: "
						  "Unexpected amount of data returned (%i bytes, expected %i)"),
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
 * @context: context for error reporting
 *
 * Unlocks the keys on cameras that support this
 *
 * Returns: gphoto2 error code
 *
 */
int
canon_usb_unlock_keys (Camera *camera, GPContext *context)
{
	unsigned char *c_res;
	int bytes_read;

	GP_DEBUG ("canon_usb_unlock_keys()");

	switch (camera->pl->md->model) {
		case CANON_EOS_D30:
		case CANON_EOS_D60:
	        case CANON_EOS_10D:
		case CANON_PS_S230:
		case CANON_PS_S400:
			c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,
						    &bytes_read, NULL, 0);
			if (!c_res)
				return GP_ERROR;
			if (bytes_read == 0x4) {
				GP_DEBUG ("canon_usb_unlock_keys: Got the expected number of bytes back.");
			} else {
				gp_context_error (context,
						  _("canon_usb_unlock_keys: "
						  "Unexpected amount of data returned (%i bytes, expected %i)"),
						  bytes_read, 0x4);
				return GP_ERROR;
			}

			break;
		default:
			/* Your camera model does not need unlocking, cannot do unlocking or
			 * we don't know how to unlock it's keys. 
			 */
			GP_DEBUG ("canon_usb_unlock_keys: Not unlocking the kind of camera you have.\n"
				  "If unlocking works when using the Windows software with your camera,\n"
				  "please contact %s.", MAIL_GPHOTO_DEVEL);
			break;
	}

	return GP_OK;
}

/**
 * canon_usb_poll_interrupt_pipe:
 * @camera: the Camera to work with
 * @buf: buffer to receive data read from the pipe.
 * @n_tries: number of times to try
 *
 * Reads the interrupt pipe repeatedly until either
 * 1. a non-zero length is returned,
 * 2. an error code is returned, or
 * 3. the number of read attempts reaches @n_tries.
 *
 * Returns:
 *  length of read, or
 *  zero if n_tries has been exceeded, or
 *  gphoto2 error code from read that results in an I/O error.
 *
 */
static int canon_usb_poll_interrupt_pipe ( Camera *camera, unsigned char *buf, int n_tries )
{
	int i, status = 0;

	memset ( buf, 0x81, 0x40 ); /* Put weird stuff in buffer */
	/* Read repeatedly until we get either an
	   error or a non-zero size. */
	for ( i=0; i<n_tries; i++ ) {
		status = gp_port_check_int_fast ( camera->port,
						  buf, 0x40 );
		if ( status != 0 ) /* Either some real data, or failure */
			break;
	}
	if ( status <= 0 )
		GP_LOG ( GP_LOG_ERROR, _("canon_usb_poll_interrupt_pipe:"
			 " interrupt read failed after %i tries, \"%s\""),
			 i, gp_result_as_string(status) );
	else
		GP_DEBUG ( "canon_usb_poll_interrupt_pipe:"
			   " interrupt packet took %d tries\n", i+1 );

	return status;
}

/**
 * canon_usb_poll_interrupt_multiple:
 * @camera: Array of Camera's to work with
 * @n_cameras: Length of #camera
 * @camera_flags: array of int's corresponding to entries in #camera.
 *                Non-zero (true) if that camera is to be polled, zero
 *                (false) if that camera is to be ignored.
 * @buf: buffer to receive data read from the pipe.
 * @n_tries: number of times to try
 * @which: returns index of camera that responded
 *
 * Polls the interrupt pipes of several cameras either until one
 *  responds, or for a specified number of tries.
 *  We will return when:
 * 1. a non-zero length is returned,
 * 2. an error code is returned, or
 * 3. the number of read attempts reaches @n_tries.
 *
 * Returns:
 *  length of read, or
 *  zero if n_tries has been exceeded, or
 *  gphoto2 error code from read that results in an I/O error.
 *
 */
int canon_usb_poll_interrupt_multiple ( Camera *camera[], int n_cameras,
					int camera_flags[],
					unsigned char *buf, int n_tries,
					int *which )
{
	int i = 0, status = 0;

	memset ( buf, 0x81, 0x40 ); /* Put weird stuff in buffer */
	*which = 0;			     /* Start with the first camera */
	/* Read repeatedly until we get either an
	   error or a non-zero size. */
	while ( status == 0 && i < n_tries ) {
		while ( !camera_flags[*which] )
			*which = (*which+1) % n_cameras;

		status = gp_port_check_int_fast ( camera[*which]->port,
						  buf, 0x40 );
	}
	if ( status <= 0 )
		GP_LOG ( GP_LOG_ERROR, _("canon_usb_poll_interrupt_multiple:"
			 " interrupt read failed after %i tries, \"%s\""),
			 i, gp_result_as_string(status) );
	else
		GP_DEBUG ( "canon_usb_poll_interrupt_multiple:"
			   " interrupt packet took %d tries\n", i+1 );

	return status;
}

/**
 * canon_usb_capture_dialogue:
 * @camera: the Camera to work with
 * @return_length: number of bytes to read from the camera as response
 * @context: context for error reporting
 *
 * Handles the "capture image" command, where we must read the
 *  interrupt pipe before getting the normal command response.
 *
 * We call canon_usb_dialogue() for the actual "release shutter"
 *  command, then handle the interrupt pipe here.
 *
 * Returns: a char * that points to the data read from the camera (or
 * NULL on failure), and sets what @return_length points to to the number
 * of bytes read.
 *
 */
unsigned char *
canon_usb_capture_dialogue (Camera *camera, int *return_length, GPContext *context )
{
	int status, i;
	unsigned char payload[8]; /* used for sending data to camera */
	static unsigned char *buffer; /* used for receiving data from camera */
	unsigned char buf2[0x40]; /* for reading from interrupt endpoint */

	int mstimeout = -1;		     /* To save original timeout after shutter release */

	/* clear this to indicate that no data is there if we abort */
	if (return_length)
		*return_length = 0;

	GP_DEBUG ("canon_usb_capture_dialogue()");

	/* Build payload for command, which contains subfunction code */
	memset (payload, 0x00, sizeof (payload));
	payload[0] = 4;


	/* First, let's try to make sure the interrupt pipe is clean. */
	while ( (status = canon_usb_poll_interrupt_pipe ( camera, buf2, 10 )) > 0 );

	/* Shutter release can take a long time sometimes on EOS
	 * cameras; perhaps buffer is full and needs to be flushed? */
	gp_port_get_timeout (camera->port, &mstimeout);
	GP_DEBUG("canon_usb_capture_dialogue: usb port timeout starts at %dms", mstimeout);
	gp_port_set_timeout (camera->port, 15000);

	/* now send the packet to the camera */
	buffer = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_CONTROL_CAMERA,
				      return_length,
				      payload, sizeof payload );

	if ( buffer == NULL )
		return NULL;

	gp_port_set_timeout (camera->port, mstimeout);
	GP_DEBUG("canon_usb_capture_dialogue:"
		 " set camera port timeout back to %d seconds...", mstimeout / 1000 );

	/* Check the status code from the camera. */
	if ( le32atoh ( buffer ) != 0 ) {
		GP_DEBUG ( "canon_usb_capture_dialogue:"
			   " got nonzero camera status code"
			   " %08x in response to capture command", le32atoh ( buffer ) );
		goto FAIL;
	}

	/* Now we need to read from the interrupt pipe. Since we have
	   to use the short timeout (50 ms), we need to try several
	   times.
	   Read until we have completion signaled (0x0a for PowerShot,
	   0x0f for EOS) */
	camera->pl->capture_step = 0;
	camera->pl->thumb_length = 0; camera->pl->image_length = 0;
	camera->pl->image_key = 0x81818181;

	while ( buf2[4] != 0x0f ) {
		status = canon_usb_poll_interrupt_pipe ( camera, buf2, MAX_INTERRUPT_TRIES );
		if ( status > 0x17 )
			GP_DEBUG ( "canon_usb_capture_dialogue:"
				   " interrupt read too long (length=%i)", status );
		else if (  status == 0 )
			goto FAIL;

		switch ( buf2[4] ) {
		case 0x08:
			/* Thumbnail size */
			if ( status != 0x17 )
				GP_LOG ( GP_LOG_ERROR, _("canon_usb_capture_dialogue:"
					 " bogus length 0x%04x"
					 " for thumbnail size packet"), status );
			camera->pl->thumb_length = le32atoh ( buf2+0x11 );
			camera->pl->image_key = le32atoh ( buf2+0x0c );
			GP_DEBUG ( "canon_usb_capture_dialogue: thumbnail size %i, tag=0x%08x",
				   camera->pl->thumb_length, camera->pl->image_key );
			break;
		case 0x0c:
			/* Full image size */
			if ( status != 0x17 )
				GP_LOG ( GP_LOG_ERROR, _("canon_usb_capture_dialogue:"
					 " bogus length 0x%04x"
					 " for full image size packet"), status );
			camera->pl->image_length = le32atoh ( buf2+0x11 );
			camera->pl->image_key = le32atoh ( buf2+0x0c );
			GP_DEBUG ( "canon_usb_capture_dialogue: full image size: 0x%08x, tag=0x%08x",
				   camera->pl->image_length, camera->pl->image_key );
			break;
		case 0x0a:
			if ( buf2[12] == 0x1c ) {
				GP_DEBUG ( "canon_usb_capture_dialogue: first interrupt read" );
				if ( camera->pl->capture_step == 0 )
					camera->pl->capture_step++;
				else if ( camera->pl->capture_step == 2 ) {
					/* I think this may be a signal that this is the last shot
					   that will fit on the CF card. */
					GP_DEBUG ( "canon_usb_capture_dialogue: redundant \"1c\" code; full CF card?" );
					camera->pl->capture_step = 1;
				}
				else {
					GP_LOG ( GP_LOG_ERROR, _("canon_usb_capture_dialogue:"
						 " first interrupt read out of sequence") );
					goto FAIL;
				}
			}
			else if ( buf2[12] == 0x1d ) {
				GP_DEBUG ( "canon_usb_capture_dialogue: second interrupt read (after image sizes)" );
				if ( camera->pl->capture_step != 1 ) {
					GP_LOG ( GP_LOG_ERROR, _("canon_usb_capture_dialogue:"
						 " second interrupt read out of sequence") );
					goto FAIL;
				}
				camera->pl->capture_step++;
				/* PowerShot cameras end with this message */
				if ( !IS_EOS(camera->pl->md->model) )
					goto EXIT;
			}
			else if ( buf2[12] == 0x0a ) {
				GP_LOG ( GP_LOG_ERROR, _("canon_usb_capture_dialogue:"
					 " photographic failure signaled, code = 0x%08x"),
					 le32atoh ( buf2+16 ) );
				goto FAIL;
			}
			else {
				GP_DEBUG ( "canon_usb_capture_dialogue: unknown subcode 0x%08x in 0x0a interrupt read",
					   le32atoh ( buf2+12 ) );
			}
			break;
		case 0x0e:
			/* The image has been written to local
			 * storage. Never seen if the transfer
			 * mode didn't include any local
			 * storage. */
			GP_DEBUG ( "canon_usb_capture_dialogue:"
				   " EOS flash write complete from interrupt read" );
			if ( camera->pl->capture_step != 2 ) {
				GP_LOG ( GP_LOG_ERROR, _("canon_usb_capture_dialogue:"
					 " third EOS interrupt read out of sequence") );
				goto FAIL;
			}
			camera->pl->capture_step++;
			/* Canon SDK unlocks the keys here for EOS cameras. */
			if ( canon_usb_unlock_keys ( camera, context ) < 0 ) {
				GP_DEBUG ( "canon_usb_capture_dialogue: couldn't unlock keys after capture." );
				goto FAIL;
			}
			break;
		case 0x0f:
			/* We allow this message as third or
			 * fourth, since we may or may not be
			 * writing to camera storage. */
			if ( camera->pl->capture_step == 2 ) {
				/* Keys were never unlocked, as that message never came. */
				if ( canon_usb_unlock_keys ( camera, context ) < 0 ) {
					GP_DEBUG ( "canon_usb_capture_dialogue: couldn't unlock keys after capture." );
					goto FAIL;
				}
			}
			else if ( camera->pl->capture_step == 3 ) {
				GP_DEBUG ( "canon_usb_capture_dialogue:"
					   " final EOS interrupt read" );
			}
			else {
				GP_LOG ( GP_LOG_ERROR, _("canon_usb_capture_dialogue:"
					 " fourth EOS interrupt read out of sequence") );
				goto FAIL;
			}
			break;
		default:
			/* Unknown */
			GP_DEBUG ( "canon_usb_capture_dialogue:"
				   " unknown code 0x%02x in interrupt read",
				   buf2[4] );
			goto FAIL;
		}
	}

 EXIT:
	*return_length = 0x1c;
	return buffer;
 FAIL:
	/* Try to purge interrupt pipe, which was left in an unknown state. */
	for ( i=0; i<5; i++ )
		status = canon_usb_poll_interrupt_pipe ( camera, buf2, 1000 );
	canon_usb_unlock_keys ( camera, context );    /* Ignore status code, as we can't fix it anyway. */
	return NULL;
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
 *		#canon_funct = %CANON_USB_FUNCTION_SET_TIME
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
 * Returns: a char * that points to the data read from the camera (or
 * NULL on failure), and sets what @return_length points to to the number
 * of bytes read.
 *
 */
unsigned char *
canon_usb_dialogue (Camera *camera, canonCommandIndex canon_funct, int *return_length, const char *payload,
		    int payload_length)
{
	int msgsize, status, i;
	char cmd1 = 0, cmd2 = 0, *funct_descr = "";
	int cmd3 = 0, read_bytes = 0, read_bytes1 = 0, read_bytes2 = 0;
	unsigned char packet[1024];	/* used for sending data to camera */
	static unsigned char buffer[0x384];	/* used for receiving data from camera */

	int j, canon_subfunc = 0;
	char subcmd = 0, *subfunct_descr = "";
	int additional_read_bytes = 0;

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

	/*
	 * The CONTROL_CAMERA function is special in that its payload specifies a
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
			_("canon_usb_dialogue:"
			" payload too big, won't fit into buffer (%i > %i)"),
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
	htole32a (packet + 0x4c, serial_code++);	/* serial number */
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
		if ( status >= 0 )
			GP_DEBUG ("canon_usb_dialogue: read 1 of %i bytes failed! (returned %i)",
				  read_bytes1, status);
		else			     /* Error code */
			GP_DEBUG ("canon_usb_dialogue: read 1 of %i bytes failed! (%s)",
				  read_bytes1, gp_result_as_string ( status ) );
		return NULL;
	}

	if (read_bytes2) {
		status = gp_port_read (camera->port, buffer + read_bytes1, read_bytes2);
		if (status != read_bytes2) {
			if ( status >= 0 )
				GP_DEBUG ("canon_usb_dialogue: read 2 of %i bytes failed! (returned %i)",
					  read_bytes2, status);
			else			     /* Error code */
				GP_DEBUG ("canon_usb_dialogue: read 2 of %i bytes failed! (%s)",
				  read_bytes2, gp_result_as_string ( status ) );
			return NULL;
		}
	}

	/* if cmd3 is 0x202, this is a command that returns L (long) data
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
		if ( le32atoh ( buffer+0x50 ) != 0 ) {
			GP_DEBUG ( "canon_usb_dialogue: got nonzero camera status code"
				 " %08x in response to command 0x%x 0x%x 0x%x (%s)",
				 le32atoh ( buffer+0x50 ), cmd1, cmd2, cmd3, funct_descr );
		}
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
 * @context: context for error reporting
 *
 * This function is used to invoke camera commands which return L (long) data.
 * It calls #canon_usb_dialogue(), if it gets a good response it will malloc()
 * memory and read the entire returned data into this malloc'd memory and store
 * a pointer to the malloc'd memory in 'data'.
 *
 * Returns: gphoto2 error code
 *
 */
int
canon_usb_long_dialogue (Camera *camera, canonCommandIndex canon_funct, unsigned char **data,
			 int *data_length, int max_data_size, const char *payload,
			 int payload_length, int display_status, GPContext *context)
{
	int bytes_read;
	unsigned int total_data_size = 0, bytes_received = 0, read_bytes = USB_BULK_READ_SIZE;
	unsigned char *lpacket;		/* "length packet" */
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
		if ((total_data_size - bytes_received) > USB_BULK_READ_SIZE )
			/* Limit max transfer length */
			read_bytes = USB_BULK_READ_SIZE;
		else if ((total_data_size - bytes_received) > 0x040 )
			/* Round longer transfers down to nearest 0x40 */
			read_bytes = (total_data_size - bytes_received) / 0x40 * 0x40;
		else
			/* Final transfer; use true length */
			read_bytes = (total_data_size - bytes_received);

		GP_DEBUG ("canon_usb_long_dialogue: calling gp_port_read(), total_data_size = %i, "
			  "bytes_received = %i, read_bytes = %i (0x%x)", total_data_size,
			  bytes_received, read_bytes, read_bytes);
		bytes_read = gp_port_read (camera->port, *data + bytes_received, read_bytes);
		if (bytes_read < 1) {
			GP_DEBUG ("canon_usb_long_dialogue: gp_port_read() returned error (%i) or no data\n",
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
			GP_DEBUG ("canon_usb_long_dialogue: WARNING: gp_port_read() resulted in short read "
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

/**
 * canon_usb_get_file:
 * @camera: camera to lock keys on
 * @name: name of file to fetch
 * @data: to receive image data
 * @length: to receive length of image data
 * @context: context for error reporting
 *
 * Get a file from a USB_connected Canon camera.
 *
 * Returns: gphoto2 error code, length in @length, and image data in @data.
 *
 */
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

/**
 * canon_usb_get_thumbnail:
 * @camera: camera to lock keys on
 * @name: name of thumbnail to fetch
 * @data: to receive image data
 * @length: to receive length of image data
 * @context: context for error reporting
 *
 * Gets a thumbnail from a USB_connected Canon camera.
 *
 * Returns: gphoto2 error code, length in @length, and image data in
 *    @data.
 *
 */
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

/**
 * canon_usb_get_captured_image:
 * @camera: camera to lock keys on
 * @key: index of image to fetch, returned from canon_usb_capture_dialogue()
 * @data: to receive image data
 * @length: to receive length of image data
 * @context: context for error reporting
 *
 * Gets the just-captured image from a USB-connected Canon
 * camera. This function must be called soon after an image capture,
 * and needs the image key returned by canon_usb_capture_dialogue().
 *
 * Returns: gphoto2 error code, length in @length, and image data in @data.
 *
 */
int
canon_usb_get_captured_image (Camera *camera, const int key, unsigned char **data, int *length,
			      GPContext *context)
{
	char payload[16];
	int payload_length = 16, result;

	GP_DEBUG ("canon_usb_get_captured_image() called");

	/* Construct payload containing buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	htole32a (payload, 0x0);	/* always zero */
	htole32a (payload + 0x4, USB_BULK_READ_SIZE);
	htole32a (payload + 0x8, CANON_DOWNLOAD_FULL);
	htole32a (payload + 0xc, key);

	/* the 1 is to show status */
	result = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_RETRIEVE_CAPTURE, data, length,
					  0, payload,
					  payload_length, 1, context);
	if (result != GP_OK) {
		GP_DEBUG ("canon_usb_get_captured_image: canon_usb_long_dialogue() "
			  "returned error (%i).", result);
		return result;
	}

	return GP_OK;
}

/**
 * canon_usb_get_captured_thumbnail:
 * @camera: camera to lock keys on
 * @key: index of image to fetch, returned from canon_usb_capture_dialog()
 * @data: to receive thumbnail data
 * @length: to receive length of thumbnail data
 * @context: context for error reporting
 *
 * Gets the just-captured thumbnail from a USB-connected Canon
 * camera. This function must be called after a remote image capture,
 * before leaving remote capture mode, and needs the image key
 * returned by canon_usb_capture_dialog().
 *
 * Returns: gphoto2 error code, length in @length, and thumbnail data in @data.
 *
 */
int
canon_usb_get_captured_thumbnail (Camera *camera, const int key, unsigned char **data, int *length,
			      GPContext *context)
{
	char payload[16];
	int payload_length = 16, result;

	GP_DEBUG ("canon_usb_get_captured_thumbnail() called");

	/* Construct payload containing buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	htole32a (payload, 0x0);	/* get picture */
	htole32a (payload + 0x4, USB_BULK_READ_SIZE);
	htole32a (payload + 0x8, CANON_DOWNLOAD_THUMB);
	htole32a (payload + 0xc, key);

	/* the 1 is to show status */
	result = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_RETRIEVE_CAPTURE, data, length,
					  0, payload,
					  payload_length, 1, context);
	if (result != GP_OK) {
		GP_DEBUG ("canon_usb_get_captured_thumbnail: canon_usb_long_dialogue() "
			  "returned error (%i).", result);
		return result;
	}

	return GP_OK;
}

/**
 * canon_usb_set_file_time:
 * @camera: camera to unlock keys on
 * @camera_filename: Full pathname to use on camera
 * @time: Unix time to store in camera directory
 * @context: context for error reporting
 *
 * Finishes the upload of an image to a Canon camera. Must be called
 * after the last data transfer completes.
 *
 * Returns: gphoto2 error code
 *
 */
int canon_usb_set_file_time ( Camera *camera, char *camera_filename, time_t time, GPContext *context)
{
	unsigned char *result_buffer;
	int payload_size =  0x4 + strlen(camera_filename) + 2,
		bytes_read;
	char *payload = malloc ( payload_size );

	if ( payload == NULL ) {
		GP_DEBUG ( "canon_usb_set_file_time:"
			   " failed to allocate payload block." );
		gp_context_error(context, _("Out of memory: %d bytes needed."),
				 payload_size );
		return GP_ERROR_NO_MEMORY;
	}
	memset ( payload, 0, payload_size );

	strncpy ( payload + 0x4, camera_filename, strlen(camera_filename) );
	htole32a ( payload, time );	     /* Load specified time for camera directory. */
	result_buffer = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_SET_FILE_TIME,
				      &bytes_read, payload, payload_size );
	if ( result_buffer == NULL ) {
		GP_DEBUG ( "canon_usb_set_file_time:"
			   " dialogue failed." );
		return GP_ERROR;
	}

	free ( payload );
	return GP_OK;
}

/**
 * canon_usb_put_file:
 * @camera: camera to lock keys on
 * @file: CameraFile object to upload
 * @destname: name file should have on camera
 * @destpath: pathname for directory to put file
 * @context: context for error reporting
 * 
 * Uploads file to USB camera
 *
 * Bugs:
 * at the moment, files bigger than 65572-length(filename), while being
 * correctly uploaded, cause the camera to not accept any more uploads.
 * Smaller files work fine.
 * s10sh (http://www.kyuzz.org/antirez/s10sh.html) has the same problem.
 * The problem only appears when USB deinitialisation and initialisation
 * is performed between uploads. You can call this function more than
 * once with big files during one session without encountering the problem 
 * described. <kramm@quiss.org>
 * 
 * Returns: gphoto2 error code
 *
 */
int
canon_usb_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath,
		    GPContext *context)
{
#ifndef CANON_EXPERIMENTAL_UPLOAD
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
	long int size;			     /* Total size of file to upload */
	FILE *fi;
	const char *srcname;
	char *packet;
	struct stat filestat;		     /* To get time from Unix file */

	GP_DEBUG ( "canon_put_file_usb()" );

	sprintf(filename, "%s%s", destpath, destname);
	filename_len = strlen(filename);

	packet = malloc(packet_size + filename_len + 0x5d);

	if(!packet) {
	    int len = packet_size + filename_len + 0x5d;
	    GP_DEBUG ("canon_put_file_usb: Couldn't reserve %d bytes of memory", len);
	    gp_context_error(context, _("Out of memory: %d bytes needed."), len);
	    return GP_ERROR_NO_MEMORY;
	}

	GP_DEBUG ( "canon_put_file_usb: converting file name" );
	CHECK_RESULT (gp_file_get_name (file, &srcname));

	/* Open input file and read all its data into a buffer. */
	if(!gp_file_get_data_and_size (file, &data, &size)) {
	    fi = fopen(srcname, "rb");
	    if(!fi) {
		gp_context_error(context, _("Couldn't read from file \"%s\""), srcname);
		free(packet);
		return GP_ERROR;
	    }
	    fstat ( fileno(fi), &filestat );
	    fseek(fi, 0, SEEK_END);
	    size = ftell(fi);
	    fseek(fi, 0, SEEK_SET);
	    newdata = data = malloc(size);
	    if(!newdata) {
		gp_context_error(context, _("Out of memory: %d bytes needed."), size);
		free(packet);
		return GP_ERROR_NO_MEMORY;
	    }
	    fread(newdata, size, 1, fi);
	    fclose(fi);
	}

	GP_DEBUG ( "canon_put_file_usb:"
		   " finished reading file, %d bytes (0x%x)",
		   size, size );

	/* Take the buffer and send it to the camera, breaking it into
	 * packets of suitable size. */
	while(offs < size) {
	    len2 = packet_size;
	    if(size - offs < len2)
		len2 = size - offs;
	    len1 = len2 + 0x1c + filename_len + 1;

	    GP_DEBUG ( "canon_put_file_usb: len1=%x, len2=%x", len1, len2 );

	    memset(packet, 0, 0x40);
	    packet[4]=3;
	    packet[5]=2;
	    htole32a ( packet + 0x6, len1 + 0x40 );
	    htole32a (packet + 0x4c, serial_code++); /* Serial number */

	    /* now send the packet to the camera */
	    status = gp_port_usb_msg_write (camera->port, 0x04, 0x10, 0,
					    packet, 0x40);
	    if (status != 0x40) {
		    GP_DEBUG ("canon_put_file_usb: write 1 failed! (returned %i)\n", status);
		    gp_context_error(context, _("File upload failed."));
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }

	    status = gp_port_read (camera->port, buffer, 0x40);
	    if (status != 0x40) {
		    GP_DEBUG ("canon_put_file_usb: read 1 failed! "
			      "(returned %i, expected %i)", status, 0x40);
		    gp_context_error(context, _("File upload failed."));
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }

	    memset(packet, 0, len1 + 0x40);
	    /* Length of this block (minus 0x40) */
	    htole32a ( packet, len1 );

	    /* Fill in codes */
	    packet[0x04] = 3;
	    packet[0x05] = 4;
	    packet[0x40] = 2;
	    packet[0x44] = 3;
	    packet[0x47] = 0x11;

	    /* Copy of bytes 0:3 */
	    htole32a ( packet+0x48, len1 );

	    /* Serial number */
	    htole32a (packet + 0x4c, serial_code++);

	    htole32a ( packet+0x54, offs );

	    /* Max length of data block */
	    htole32a ( packet+0x58, len2 );

	    strcpy(&packet[0x5c], filename); 
	    memcpy(&packet[0x5c+filename_len+1], &data[offs], len2);

	    status = gp_port_write (camera->port, packet, len1+0x40);
	    if (status != len1+0x40) {
		    GP_DEBUG ("canon_put_file_usb: write 2 failed! "
			      "(returned %i, expected %i)", status, len1+0x40);
		    gp_context_error(context, _("File upload failed."));
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }

	    status = gp_port_read (camera->port, buffer, 0x40);
	    if (status != 0x40) {
		    GP_DEBUG ("canon_put_file_usb: read 2 failed! "
			      "(returned %i, expected %i)", status, 0x5c);
		    gp_context_error(context, _("File upload failed."));
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }

	    status = gp_port_read (camera->port, buffer, 0x1c);
	    if (status != 0x1c) {
		    GP_DEBUG ("canon_put_file_usb: read 3 failed! "
			      "(returned %i, expected %i)", status, 0x5c);
		    gp_context_error(context, _("File upload failed."));
		    if(newdata)
			free(newdata);
		    free(packet);
		    return GP_ERROR_CORRUPTED_DATA;
	    }
	    else if ( le32atoh ( buffer ) != 0 ) {
		    /* Got a status message back from the camera */
		    GP_DEBUG ( "canon_put_file_usb: got camera status 0x%08x from read 3",
			       le32atoh ( buffer ) );
	    }
	    offs += len2;
	}

	/* Now we finish the job. */
	canon_usb_set_file_time ( camera, filename, filestat.st_mtime, context );

	canon_usb_set_file_attributes ( camera, (unsigned short)0,
					filename, context );

	if(size > 65572-filename_len) {
	    gp_context_message(context, _("File was too big. You may have to turn your camera off and back on before uploading more files."));
	}

	if(newdata)
	    free(newdata);
	free(packet);
	return GP_OK;
#endif /* CANON_EXPERIMENTAL_UPLOAD */
}

/**
 * canon_usb_get_dirents:
 * @camera: camera to initialize
 * @dirent_data: to receive directory data
 * @dirents_length: to receive length of @dirent_data
 * @path: pathname of directory to list
 * @context: context for error reporting
 *
 * Lists a directory.
 *
 * Returns: gphoto2 error code
 *
 */
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
				  _("canon_usb_get_dirents: "
				  "Couldn't fit payload into buffer, "
				  "'%.96s' (truncated) too long."), path);
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
				  _("canon_usb_get_dirents: "
				  "canon_usb_long_dialogue failed to fetch direntries, "
				  "returned %i"), res);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * canon_usb_list_all_dirs:
 * @camera: camera to initialize
 * @dirent_data: to receive directory data
 * @dirents_length: to receive length of @dirent_data
 * @context: context for error reporting
 *
 * Lists all directories on camera. This can be executed before and
 *  after a "capture image" operation. By comparing the results, we can
 *  see where the new image went. Unfortunately, this seems to be the
 *  only way to find out, as Canon doesn't return that information from
 *  a capture command.
 *
 * Returns: gphoto2 error code
 *
 */
int
canon_usb_list_all_dirs (Camera *camera, unsigned char **dirent_data,
		       unsigned int *dirents_length, GPContext *context)
{
	unsigned char payload[100];
	unsigned int payload_length;
	unsigned char *disk_name = canon_int_get_disk_name (camera, context);
	int res;

	*dirent_data = NULL;

	/* build payload :
	 *
	 * 0x0f dirname 0x00 0x00 0x00
	 *
	 * the 0x0f before dirname means 'recurse to level 15', which
	 * should get all the directories on the camera.
	 * NOTE: the first 0x00 after dirname is the NULL byte terminating
	 * the string, so payload_length is strlen(dirname) + 4 
	 */
	if (strlen (disk_name) + 4 > sizeof (payload)) {
		GP_DEBUG ("canon_usb_list_all_dirs: "
			  "Path '%s' too long (%i), won't fit in payload buffer.", disk_name,
			  strlen (disk_name));
		gp_context_error (context,
				  _("canon_usb_list_all_dirs: "
				  "Couldn't fit payload into buffer, "
				  "'%.96s' (truncated) too long."), disk_name);
		return GP_ERROR_BAD_PARAMETERS;
	}
	memset (payload, 0x00, sizeof (payload));
	memcpy (payload + 1, disk_name, strlen (disk_name));
	payload[0] = 0x0f;		     /* Recursion depth */
	payload_length = strlen (disk_name) + 4;
	free ( disk_name );

	/* Give no max data size.
	 * 0 is to not show progress status
	 */
	res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_DIRENT, dirent_data,
				       dirents_length, 0, payload, payload_length, 0,
				       context);
	if (res != GP_OK) {
		gp_context_error (context,
				  _("canon_usb_list_all_dirs: "
				  "canon_usb_long_dialogue failed to fetch direntries, "
				  "returned %i"), res);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * canon_usb_set_file_attributes:
 * @camera: camera to initialize
 * @attr_bits: bits to write in the camera directory entry
 * @pathname: pathname of file whose attributes are to be changed.
 * @context: context for error reporting
 *
 * Changes the attribute bits in a specified directory entry.
 *
 * Returns: gphoto2 error code
 *
 */
int
canon_usb_set_file_attributes (Camera *camera, unsigned short attr_bits,
			       const char *pathname, GPContext *context)
{
	/* Pathname string plus 32-bit parameter plus two NULs. */
	unsigned int payload_length = strlen(pathname) + 6;
	unsigned char *payload = malloc ( payload_length );
	int bytes_read;
	unsigned char *res;

	GP_DEBUG ( "canon_usb_set_file_attributes()" );

	/* build payload :
	 *
	 * <attr bits> pathname 0x00 0x00
	 *
	 * The attribute bits will be a 32-bit little-endian integer.
	 * NOTE: the first 0x00 after dirname is the NULL byte terminating
	 * the string, so payload_length is strlen(dirname) + 4 
	 */
	memset (payload, 0x00, payload_length);
	memcpy (payload + 4, pathname, strlen (pathname));
	htole32a ( payload, (unsigned long)attr_bits );

	/* 1024 * 1024 is max realistic data size, out of the blue.
	 * 0 is to not show progress status
	 */
	res = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_SET_ATTR, &bytes_read,
				   payload, payload_length );
	if ( res == NULL ) {
		gp_context_error (context,
				  _("canon_usb_set_file_attributes: "
				  "canon_usb_dialogue failed"));
		free ( payload );
		return GP_ERROR;
	}
	else if ( le32atoh ( res+0x50 ) != 0 ) {
		gp_context_error (context,
				  _("canon_usb_set_file_attributes: "
				  "canon_usb_dialogue returned error status 0x%08x from camera"),
				  le32atoh ( res+0x50 ) );
		free ( payload );
		return GP_ERROR;
	}

	free ( payload );
	return GP_OK;
}

/**
 * canon_usb_ready:
 * @camera: camera to get ready
 *
 * USB part of canon_int_ready
 *
 * Returns: gphoto2 error code
 *
 */
int
canon_usb_ready (Camera *camera)
{
	GP_DEBUG ("canon_usb_ready()");

	/* XXX send a 'ping' packet and check that the camera is
	 * still alive.
	 */

	return GP_OK;
}

/**
 * canon_usb_identify:
 * @camera: camera to identify
 * @context: context for error reporting
 *  GP_ERROR_MODEL_NOT_FOUND if not found in our list;
 *    this is a code inconsistency.
 *  GP_OK otherwise
 *
 * Identify a supported camera by looking through our list of models.
 *
 * Returns: gphoto2 error code
 *
 */
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

	/* ahem. isn't this a tautology? */

	/* Formerly, we matched strings here.
	 * Now we just match USB Vendor/Product IDs to work around
	 * the colon/space problem. (FIXME)
	 */
	for (i = 0; models[i].id_str != NULL; i++) {
		if (models[i].usb_vendor 
		    && models[i].usb_product
		    && (a.usb_vendor  == models[i].usb_vendor) 
		    && (a.usb_product == models[i].usb_product)) {
			GP_DEBUG ("canon_usb_identify: model name match '%s'",
				  models[i].id_str);
			gp_context_status (context, _("Detected a '%s'."), models[i].id_str);
			camera->pl->md = (struct canonCamModelData *) &models[i];
			return GP_OK;
		}
	}

	gp_context_error (context, _("Name \"%s\" from camera does not match any known camera"), a.model);

	return GP_ERROR_MODEL_NOT_FOUND;
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
