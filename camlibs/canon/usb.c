/****************************************************************************
 *
 * File: usb.c
 *
 * USB communication layer.
 *
 ****************************************************************************/

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
#ifdef CANON_EXPERIMENTAL_UPLOAD
/* For filestat to get file time for upload */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif /* CANON_EXPERIMENTAL_UPLOAD */

#include "libgphoto2/i18n.h"


#include <gphoto2/gphoto2.h>

#include "usb.h"
#include "canon.h"
#include "util.h"

#ifdef __GNUC__
# define __unused__ __attribute__((unused))
#else
# define __unused__
#endif

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

/* IDENTIFY_INIT_TIMEOUT: the starting timeout (in milliseconds)
 * to wait for the camera to respond to an "identify camera" request.
 * This timeout is doubled for each retry */
/* It was originally 10, but this made the communication fail, so
 * bump up to 100. */
#define IDENTIFY_INIT_TIMEOUT 100

/* IDENTIFY_MAX_ATTEMPTS: how many "identify camera" requests to send
 * to the camera, until we give up for lack of response. */
#define IDENTIFY_MAX_ATTEMPTS 5

/* CANON_FAST_TIMEOUT: how long (in milliseconds) we should wait for
 * an URB to come back on an interrupt endpoint */
#define CANON_FAST_TIMEOUT 500

/* WARNING: This destroys reentrancy of the code. Better to put this
 * in the camera descriptor somewhere. */
static int serial_code = 0;

/* Map camera status codes (from offset 0x50 in reply block) to
 * messages. */
static struct canon_usb_status canon_usb_status_table[] = {
        {0x00000000, NULL},
        {0x02000022, "File not found"},
        {0x02000029, "File was protected"},
        {0x0200002a, "Compact Flash card full"},
        {0x02000081, "Failed to lock EOS keys"},
        {0x02000082, "Failed to unlock EOS keys"},
        {0x02000085, "Could not switch to capture mode"},
        {0x02000086, "Invalid command parameters"},
        {0x00000086, "Can't unlock EOS keys (new)"},
        {0x02000087, "No storage card in camera"},
	{0x82200040, "Unknown error (new protocol)"},
	{0x82220040, "Unknown error (new protocol)"}
};


/*
 * Command codes are structured:
 *   cmd2=11 -> camera control,
 *   cmd2=12 -> storage control.
 *
 *   cmd3=201 -> fixed length response
 *   cmd3=202 -> variable length response
 */

static const struct canon_usb_cmdstruct canon_usb_cmd[] = {
	{CANON_USB_FUNCTION_GET_FILE,		"Get file",			0x01, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_MKDIR,		"Make directory",		0x05, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_RMDIR,		"Remove directory",		0x06, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_DISK_INFO,		"Disk info request",		0x09, 0x11, 0x201,	0x5c},
	/* 0x0a is overloaded: "flash device ident" and "delete file",
	 * with different responses */
	{CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,	"Flash device ident",	0x0a, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_DELETE_FILE_2,	"Delete file",		0x0a, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_GET_DIRENT,		"Get directory entries",	0x0b, 0x11, 0x202,	0x40},
	/* Command code 0x0d is overloaded: delete file (old),
	 * disk info request ID (new). */
	{CANON_USB_FUNCTION_DELETE_FILE,	"Delete file",		0x0d, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_DISK_INFO_2,	"Disk info request (new)",	0x0d, 0x11, 0x201,	0x5c},
	/* Command code 0x0e is overloaded: set file attribute (old),
	 * flash device ID (new). And the response is different: fixed
	 * length in old, variable length in new. */
	{CANON_USB_FUNCTION_SET_ATTR,		"Set file attributes",	0x0e, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_FLASH_DEVICE_IDENT_2, "Flash device ident (new)", 0x0e, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_SET_FILE_TIME,	"Set file time",		0x0f, 0x11, 0x201,	0x54},

	{CANON_USB_FUNCTION_IDENTIFY_CAMERA,	"Identify camera",		0x01, 0x12, 0x201,	0x9c},
	{CANON_USB_FUNCTION_GET_TIME,		"Get time",			0x03, 0x12, 0x201,	0x60},
	{CANON_USB_FUNCTION_SET_TIME,		"Set time",			0x04, 0x12, 0x201,	0x54},
	/* 0x05 is overloaded: "change owner" and "get owner", with
	 * different responses */
	{CANON_USB_FUNCTION_CAMERA_CHOWN,	"Change camera owner",		0x05, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_GET_OWNER,		"Get owner name (new)",		0x05, 0x12, 0x201,	0x74},
	{CANON_USB_FUNCTION_CAMERA_CHOWN_2,	"Change owner (new)",	0x06, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_POWER_STATUS,	"Power supply status",		0x0a, 0x12, 0x201,	0x58},
	/* 0x13 is overloaded: remote camera control in the original
	   protocol, power status in the new protocol. */
	{CANON_USB_FUNCTION_CONTROL_CAMERA,	"Remote camera control",	0x13, 0x12, 0x201,      0x40},
	{CANON_USB_FUNCTION_POWER_STATUS_2,	"Power supply status (new)",	0x13, 0x12, 0x201,      0x58},
	{CANON_USB_FUNCTION_RETRIEVE_CAPTURE,	"Download a captured image",	0x17, 0x12, 0x202,      0x40},
	{CANON_USB_FUNCTION_RETRIEVE_PREVIEW,	"Download a captured preview",	0x18, 0x12, 0x202,      0x40},
	{CANON_USB_FUNCTION_UNKNOWN_FUNCTION,	"Unknown function",		0x1a, 0x12, 0x201,	0x80},
	{CANON_USB_FUNCTION_EOS_LOCK_KEYS,	"EOS lock keys",		0x1b, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,	"EOS unlock keys",		0x1c, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_GET_BODY_ID,	"EOS get body ID",		0x1d, 0x12, 0x201,	0x58},
	{CANON_USB_FUNCTION_GET_PIC_ABILITIES,	"Get picture abilities",	0x1f, 0x12, 0x201,	0x384},
	{CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,	"Lock keys and turn off LCD",	0x20, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_20D_UNKNOWN_1,	"Unknown EOS 20D function",	0x21, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_20D_UNKNOWN_2,	"Unknown EOS 20D function",	0x22, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_GET_BODY_ID_2,	"Get body ID (new)",		0x23, 0x12, 0x201,	0x58},
	{CANON_USB_FUNCTION_GET_PIC_ABILITIES_2, "Get picture abilities (new)",	0x24, 0x12, 0x201,	0x474},
	{CANON_USB_FUNCTION_CONTROL_CAMERA_2,	"Remote camera control (new)",	0x25, 0x12, 0x201,	0x40},
	{CANON_USB_FUNCTION_RETRIEVE_CAPTURE_2, "Download captured image (new)", 0x26, 0x12, 0x202,	0x40},
	{CANON_USB_FUNCTION_LOCK_KEYS_2,	"Lock keys (new)",		0x35, 0x12, 0x201,	0x5c},
	{CANON_USB_FUNCTION_UNLOCK_KEYS_2,	"Unlock keys (new)",		0x36, 0x12, 0x201,	0x54},
	/* WARNING: I don't think this is really the right value, but
	 * it gives no error on EOS 20D -- swestin 22-Mar-05 */
	{CANON_USB_FUNCTION_SET_ATTR_2,		"Set file attributes (new))", 0x07, 0x11, 0x201,	0x54},
	{ 0, NULL, 0, 0, 0, 0 }
};


const struct canon_usb_control_cmdstruct canon_usb_control_cmd[] = {
	/* COMMAND NAME                         Description            Value   CmdLen ReplyLen */
	{CANON_USB_CONTROL_INIT,                "Camera control init",  0x00,  0x18,  0x1c},  /* load 0x00, 0x00 */
	{CANON_USB_CONTROL_SHUTTER_RELEASE,     "Release shutter",      0x04,  0x18,  0x1c},  /* load 0x04, 0x00 */
	{CANON_USB_CONTROL_SET_PARAMS,          "Set release params",   0x07,  0x3c,  0x1c},  /* ?? */
	{CANON_USB_CONTROL_SET_TRANSFER_MODE,   "Set transfer mode",    0x09,  0x1c,  0x1c},  /* load (0x09, 0x04, 0x03) or (0x09, 0x04, 0x02000003) */
	{CANON_USB_CONTROL_GET_PARAMS,          "Get release params",   0x0a,  0x18,  0x4c},  /* load 0x0a, 0x00 */
	{CANON_USB_CONTROL_GET_ZOOM_POS,        "Get zoom position",    0x0b,  0x18,  0x20},  /* load 0x0b, 0x00 */
	{CANON_USB_CONTROL_SET_ZOOM_POS,        "Set zoom position",    0x0c,  0x1c,  0x1c},  /* load 0x0c, 0x04, 0x01 (or 0x0c, 0x04, 0x0b) (or 0x0c, 0x04, 0x0a) or (0x0c, 0x04, 0x09) or (0x0c, 0x04, 0x08) or (0x0c, 0x04, 0x07) or (0x0c, 0x04, 0x06) or (0x0c, 0x04, 0x00) */
	{CANON_USB_CONTROL_GET_AVAILABLE_SHOT,  "Get available shot",   0x0d,  0x18,  0x20},
	{CANON_USB_CONTROL_GET_CUSTOM_FUNC,     "Get custom func.",     0x0f,  0x22,  0x26},
	{CANON_USB_CONTROL_GET_EXT_PARAMS_SIZE, "Get ext. release params size",
	 0x10,  0x1c,  0x20},  /* load 0x10, 0x00 */
	{CANON_USB_CONTROL_GET_EXT_PARAMS,      "Get ext. release params",
	 0x12,  0x1c,  0x2c},  /* load 0x12, 0x04, 0x10 */
	{CANON_USB_CONTROL_SET_EXT_PARAMS,      "Set extended params",  0x13,  0x15,  0x1c}, /* based on EOS 20D */

	{CANON_USB_CONTROL_EXIT,                "Exit release control", 0x01,  0x18,  0x1c},
	/* New subcodes for new version of protocol */
	{CANON_USB_CONTROL_UNKNOWN_1,		"Unknown remote subcode",
	 0x1b,  0x08,  0x5e},
	{CANON_USB_CONTROL_UNKNOWN_2,		"Unknown remote subcode",
	 0x1c,  0x00,  0x00},
	/* unobserved, commands present in canon headers defines, but need more usb snoops to get reply lengths */
	{CANON_USB_CONTROL_VIEWFINDER_START,    "Start viewfinder",     0x02,  0x00,  0x00},
	{CANON_USB_CONTROL_VIEWFINDER_STOP,     "Stop viewfinder",      0x03,  0x00,  0x00},
	{CANON_USB_CONTROL_SET_CUSTOM_FUNC,     "Set custom func.",     0x0e,  0x00,  0x00},
	{CANON_USB_CONTROL_GET_EXT_PARAMS_VER,  "Get extended params version",
	 0x11,  0x00,  0x00},
	{CANON_USB_CONTROL_SELECT_CAM_OUTPUT,   "Select camera output", 0x14,  0x00,  0x00}, /* LCD (0x1), Video out (0x2), or OFF (0x3) */
	{CANON_USB_CONTROL_DO_AE_AF_AWB,        "Do AE, AF, and AWB",   0x15,  0x00,  0x00},
	{ 0, NULL, 0, 0, 0 }
};

static int canon_usb_identify (Camera *camera, GPContext *context);

/**
 * canon_usb_camera_init:
 * @camera: camera to initialize
 * @context: context for error reporting
 *
 * Initializes the USB camera through a series of read/writes
 *
 * Returns: gphoto2 error code
 *  @GP_OK on success.
 *  @GP_ERROR_OS_FAILURE if it seems to be an OS error
 *  @GP_ERROR_CORRUPTED_DATA if communication was completed, but the
 *               response was other than expected.
 *
 */
static int
canon_usb_camera_init (Camera *camera, GPContext *context)
{
        unsigned char msg[0x58];
        unsigned char buffer[0x44];
        int i, read_bytes, timeout;
        char *camstat_str = _("NOT RECOGNIZED");
        unsigned char camstat;

        GP_DEBUG ("canon_usb_camera_init()");

        memset (msg, 0, sizeof (msg));
        memset (buffer, 0, sizeof (buffer));

        i = canon_usb_identify (camera, context);
        if (i != GP_OK)
                return i;

        /* Read one byte from the control pipe. */
        i = gp_port_usb_msg_read (camera->port, 0x0c, 0x55, 0, (char *)msg, 1);
        if (i != 1) {
                gp_context_error (context, _("Could not establish initial contact with camera"));
		if (i < GP_OK)
			return i;
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
	default:
		gp_context_error (context, _("Initial camera response '%c' unrecognized"),
				  camstat);
		return GP_ERROR_CORRUPTED_DATA;
        }

        GP_DEBUG ("canon_usb_camera_init() initial camera response: %c/'%s'",
                  camstat, camstat_str);

        i = gp_port_usb_msg_read (camera->port, 0x04, 0x1, 0, (char *)msg, 0x58);
        if (i != 0x58) {
                if ( i < 0 ) {
			gp_context_error (context,
				_("Step #2 of initialization failed: (\"%s\" on read of %i). "
				"Camera not operational"), gp_result_as_string(i), 0x58);
                        return GP_ERROR_OS_FAILURE;
		} else {
			gp_context_error (context,
				_("Step #2 of initialization failed! (returned %i bytes, expected %i). "
				"Camera not operational"), i, 0x58);
			return GP_ERROR_CORRUPTED_DATA;
		}
        }
        /* Get maximum download transfer length from camera, if
         * provided */
        camera->pl->xfer_length = le32atoh (msg+0x4c);
        if ( camera->pl->xfer_length == 0xFFFFFFFF )
                camera->pl->xfer_length = USB_BULK_READ_SIZE; /* Use default */
        GP_DEBUG ("canon_usb_camera_init() set transfer length to 0x%x",
                  camera->pl->xfer_length );

        if (camstat == 'A') {
                /* read another 0x50 bytes */
                i = gp_port_usb_msg_read (camera->port, 0x04, 0x4, 0, (char *)msg, 0x50);
                if (i != 0x50) {
                        if ( i < 0 ) {
				gp_context_error (context,
						  _("Step #3 of initialization failed: "
						    "\"%s\" on read of %i. "
						    "Camera not operational"), gp_result_as_string(i), 0x50);
                                return GP_ERROR_OS_FAILURE;
			}
                        else {
				gp_context_error (context,
						  _("Step #3 of initialization failed! "
						    "(returned %i, expected %i). "
						    "Camera not operational"), i, 0x50);
                                return GP_ERROR_CORRUPTED_DATA;
			}
                }

        }
        else {
                /* set byte 0 in msg to new "canon length" (0x10) (which is total
                 * packet size - 0x40) and then move the last 0x10 bytes of msg to
                 * offset 0x40 and write it back to the camera.
                 */
                memset ( msg, 0, 0x40 );
                msg[0] = 0x10;
                memmove (msg + 0x40, msg + 0x48, 0x10);
                i = gp_port_usb_msg_write (camera->port, 0x04, 0x11, 0, (char *)msg, 0x50);
                if (i != 0x50) {
                        if ( i < 0 ) {
				gp_context_error (context,
						  _("Step #3 of initialization failed: "
						    "\"%s\" on write of %i. "
						    "Camera not operational"), gp_result_as_string(i), 0x50);
                                return GP_ERROR_OS_FAILURE;
			}
                        else {
				gp_context_error (context,
						  _("Step #3 of initialization failed! "
						    "(returned %i, expected %i). "
						    "Camera not operational"), i, 0x50);
                                return GP_ERROR_CORRUPTED_DATA;
			}
                }
		/* Temp hack for SD500/IXUS 700/IXY 600 for testing */
                if ( camera->pl->md->model != CANON_CLASS_6 && camera->pl->md->usb_product != 0x30f2 ) {
                        /* We expect to get 0x44 bytes here, but the camera is picky at this stage and
                         * we must read 0x40 bytes and then read 0x4 bytes more.
                         */
                        i = gp_port_read (camera->port, (char *)buffer, 0x40);
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
                                        if ( i < 0 ) {
						gp_context_error (context,
								  _("Step #4 failed: "
								    "\"%s\" on read of %i. Camera not operational"),
								  gp_result_as_string(i), 0x40);
                                                return GP_ERROR_OS_FAILURE;
					}
                                        else {
						gp_context_error (context,
								  _("Step #4 failed "
								    "(returned %i, expected %i). Camera not operational"),
								  i, 0x40);
                                                return GP_ERROR_CORRUPTED_DATA;
					}
                                }
                        }
                        /* just check if (int) buffer[0] says 0x4 or not, log a warning if it doesn't. */
                        read_bytes = le32atoh (buffer);
                        if (read_bytes != 4)
                                GP_DEBUG ("canon_usb_camera_init() camera says to read %i more bytes, "
                                          "we would have expected 4 - overriding since some cameras are "
                                          "known not to give correct numbers of bytes.", read_bytes);
                        i = gp_port_read (camera->port, (char *)buffer, 4);
                        if (i != 4) {
				if ( i < 0 )
					GP_DEBUG ("canon_usb_camera_init() "
						  "Step #5 of initialization failed: \"%s\" from read of %i. "
						  "Camera might still work though. Continuing.",
						  gp_result_as_string(i), 4);
				else
					GP_DEBUG ("canon_usb_camera_init() "
						  "Step #5 of initialization failed! (returned %i, expected %i) "
						  "Camera might still work though. Continuing.",
						  i, 4);
			}
                }
                else {
                        /* Newer cameras can give us all 0x44 bytes at
                         * once; some insist on it. */
                        i = gp_port_read (camera->port, (char *)buffer, 0x44);
                        if (i != 0x44) {
				if ( i < 0 ) {
					gp_context_error (context,
							  _("Step #4 failed: "
							    "\"%s\" on read of %i. Camera not operational"),
							  gp_result_as_string(i), 0x44);
					return GP_ERROR_OS_FAILURE;
				}
				else {
					gp_context_error (context,
							  _("Step #4 failed "
							    "(returned %i, expected %i). Camera not operational"),
							  i, 0x44);
					return GP_ERROR_CORRUPTED_DATA;
				}
                        }
                }

                read_bytes = 0;

		gp_port_get_timeout ( camera->port, &timeout );
		gp_port_set_timeout ( camera->port, CANON_FAST_TIMEOUT );

                do {
                        GP_DEBUG ( "canon_usb_camera_init() read_bytes=0x%x", read_bytes );
                        i = gp_port_check_int ( camera->port, (char *)buffer, 0x10 );
                        if ( i > 0 )
                                read_bytes += i;
                } while ( read_bytes < 0x10 && i >= 0 );

		gp_port_set_timeout ( camera->port, timeout );

                if ( read_bytes < 0x10 ) {
                        GP_DEBUG ( "canon_usb_camera_init() interrupt read returned only %d bytes, status=%d", read_bytes, i );
                        if ( i < 0 )
                                return GP_ERROR_OS_FAILURE;
                        else
                                return GP_ERROR_CORRUPTED_DATA;
                }
                else if ( i < 0 ) {
                        GP_DEBUG ( "canon_usb_camera_init() interrupt read failed, status=%d", i );
                        return GP_ERROR_CORRUPTED_DATA;
                }
                else if ( i > 0x10 )
                        GP_DEBUG ( "canon_usb_camera_init() interrupt read %d bytes, expected 16", read_bytes );
                else
                        GP_DEBUG ( "canon_usb_camera_init() interrupt read OK" );
        }

        GP_DEBUG ("canon_usb_camera_init() "
                  "PC sign on LCD should be lit now (if your camera has a PC sign)");

        read_bytes = 0;
        return camstat;
}

/* Forward reference for use within canon_usb_init() */
static int canon_usb_get_body_id (Camera *camera, GPContext *context);

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
        /* unsigned char buffer[0x44]; */
        int res, id_retry, i, camstat;
        /* int read_bytes; */
	int orig_mstimeout = -1;
	int id_mstimeout = IDENTIFY_INIT_TIMEOUT; /* separate timeout for
						     identify camera */

        GP_DEBUG ("Initializing the (USB) camera.");

#if 0
	/* FIXME: as raspberry user confirmed the need for those...
 	 * I am currently not sure why I added them.
	 */
	gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_IN);
	gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_OUT);
	gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_INT);
#endif
        camstat = canon_usb_camera_init (camera, context);
        if ( camstat < 0 )
                return camstat;

	/* Save the original timeout value and set up the initial
	   identify camera timeout value */
	gp_port_get_timeout (camera->port, &orig_mstimeout);
	gp_port_set_timeout (camera->port, id_mstimeout);

        /* We retry the identify camera because sometimes (camstat == 'A'
         * in canon_usb_camera_init()) this is necessary to get the camera
         * back in sync, and the windows driver actually executes four of
         * these in a row before downloading thumbnails.
         */
        res = GP_ERROR;
        for (id_retry = 1; id_retry <= IDENTIFY_MAX_ATTEMPTS; id_retry++) {
                res = canon_int_identify_camera (camera, context);
                if (res != GP_OK) {
                        GP_DEBUG ("Identify camera try %i/%i failed %s", id_retry, IDENTIFY_MAX_ATTEMPTS,
                                  id_retry <
                                  IDENTIFY_MAX_ATTEMPTS ? "(this is OK)" : "(now it's not OK any more)");
			id_mstimeout *= 2;
			gp_port_set_timeout (camera->port, id_mstimeout);
		}
                else
                        break;
        }

	/* Restore the original timeout value */
	gp_port_set_timeout (camera->port, orig_mstimeout);

        if (res != GP_OK) {
                gp_context_error (context,
                                  _("Camera not ready, "
                                    "multiple 'Identify camera' requests failed: %s"),
                                  gp_result_as_string (res));
                if ( res < 0 )
                        return GP_ERROR_OS_FAILURE;
                else
                        return GP_ERROR_CORRUPTED_DATA;
        }

        if ( camera->pl->md->model == CANON_CLASS_6 ) {
                unsigned char *c_res;
                unsigned int bytes_read = 0;

                /* Get body ID here */
                GP_DEBUG ( "canon_usb_init: camera uses newer protocol, so we get body ID" );
                res = canon_usb_get_body_id ( camera, context );
                if ( res < 0 ) {
                        GP_DEBUG ( "canon_usb_init: \"Get body ID\" failed, code %d", res );
                        return ( res );
                }

                GP_DEBUG ( "canon_usb_init: camera uses newer protocol, so we get camera abilities" );
                c_res = canon_usb_dialogue (camera,
                                            CANON_USB_FUNCTION_GET_PIC_ABILITIES_2,
                                            &bytes_read, NULL, 0);

                if ( c_res == NULL ) {
                        GP_DEBUG ( "canon_usb_init: \"get picture abilities\" failed; continuing anyway." );
                }
                else if ( bytes_read == 0x424 ) {
                        GP_DEBUG ( "canon_usb_init: Got the expected length back from \"get picture abilities.\"" );
                } else {
                        GP_DEBUG ( "canon_usb_init: "
				   "Unexpected return of %i bytes (expected %i) from \"get picture abilities.\" We will continue.",
				   bytes_read, 0x424 );
                }
                res = canon_int_get_battery(camera, NULL, NULL, context);
                if (res != GP_OK) {
                        gp_context_error (context, _("Camera not ready, get_battery failed: %s"),
                                          gp_result_as_string (res));
                        return res;
                }
        }
        else {
                if ( camera->pl->md->model != CANON_CLASS_4 ) {
                        i = canon_usb_lock_keys(camera,context);
                        if( i < 0 ) {
                                gp_context_error (context, _("lock keys failed."));
                                return i;
                        }
                }

                res = canon_int_get_battery(camera, NULL, NULL, context);
                if (res != GP_OK) {
                        gp_context_error (context, _("Camera not ready, get_battery failed: %s"),
                                          gp_result_as_string (res));
                        return res;
                }

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
        unsigned int bytes_read;
        unsigned char payload[4];

        GP_DEBUG ("canon_usb_lock_keys()");

        switch (camera->pl->md->model) {
	case CANON_CLASS_NONE:
	case CANON_CLASS_0:
		GP_DEBUG ("canon_usb_lock_keys: Your camera model does not need the keylock.");
		break;

	case CANON_CLASS_1:
	case CANON_CLASS_2:
	case CANON_CLASS_3:
		/* Previous default; I doubt that any new
		 * cameras will work this way. */
		GP_DEBUG ("canon_usb_lock_keys: Locking camera and turning off LCD using 'normal' locking code...");

		c_res = canon_usb_dialogue (camera,
					    CANON_USB_FUNCTION_GET_PIC_ABILITIES,
					    &bytes_read, NULL, 0);

		if ( c_res == NULL )
			return GP_ERROR_OS_FAILURE;
		if ( bytes_read == 0x334 ) {
			GP_DEBUG ( "canon_usb_lock_keys: Got the expected length back from \"get picture abilities.\"" );
		} else {
			GP_DEBUG ( "canon_usb_lock_keys: "
				   "Unexpected return of %i bytes (expected %i) from "
				   "\"get picture abilities.\" We will continue.",
				   bytes_read, 0x334 );
		}
		c_res = canon_usb_dialogue (camera,
					    CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,
					    &bytes_read, NULL, 0);
		if ( c_res == NULL )
			return GP_ERROR_OS_FAILURE;
		if (bytes_read == 0x4) {
			GP_DEBUG ("canon_usb_lock_keys: Got the expected length back.");
		} else {
			gp_context_error (context,
					  _("canon_usb_lock_keys: "
					    "Unexpected length returned from \"lock keys\" function (%i bytes, expected %i)"),
					  bytes_read, 0x4);
			return GP_ERROR_CORRUPTED_DATA;
		}
		camera->pl->keys_locked = TRUE;
		break;

	case CANON_CLASS_4:
		GP_DEBUG ("canon_usb_lock_keys: Locking camera and turning off LCD using 'EOS' locking code...");

		memset (payload, 0, sizeof (payload));
		payload[0] = 0x06;

		c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_LOCK_KEYS,
					    &bytes_read, payload, 4);
		if ( c_res == NULL )
			return GP_ERROR_OS_FAILURE;
		if (bytes_read == 0x4) {
			GP_DEBUG ("canon_usb_lock_keys: Got the expected length back.");
		} else {
			gp_context_error (context,
					  _("canon_usb_lock_keys: "
					    "Unexpected length returned (%i bytes, expected %i)"),
					  bytes_read, 0x4);
			return GP_ERROR_CORRUPTED_DATA;
		}
		camera->pl->keys_locked = TRUE;
		break;

	case CANON_CLASS_5:
		/* Doesn't implement "get picture abilities",
		   but isn't an EOS camera, so we have to use
		   the "normal" key lock command. Since the
		   S45 is a relatively new model (in
		   Jan. 2003), I suspect that we will find
		   more cameras in the future that work this
		   way. */
		GP_DEBUG ("canon_usb_lock_keys: Locking camera and turning off LCD using class 5 locking code...");
		c_res = canon_usb_dialogue (camera,
					    CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,
					    &bytes_read, NULL, 0);
		if ( c_res == NULL )
			return GP_ERROR_OS_FAILURE;
		if (bytes_read == 0x4) {
			GP_DEBUG ("canon_usb_lock_keys: Got the expected length back.");
		} else {
			gp_context_error (context,
					  _("canon_usb_lock_keys: "
					    "Unexpected length returned (%i bytes, expected %i)"),
					  bytes_read, 0x4);
			return GP_ERROR_CORRUPTED_DATA;
		}
		camera->pl->keys_locked = TRUE;
		break;
	case CANON_CLASS_6:
		/* Newest variation of protocol, and quite
		 * different. "Get picture abilities" is
		 * implemented, but with a different command
		 * code and a longer buffer returned. */
		GP_DEBUG ("Camera uses newer protocol: Locking camera keys and turning off LCD...");

		c_res = canon_usb_dialogue (camera,
					    CANON_USB_FUNCTION_GET_PIC_ABILITIES_2,
					    &bytes_read, NULL, 0);

		if ( c_res == NULL ) {
			GP_DEBUG ( "canon_usb_lock_keys: \"get picture abilities\" failed; continuing anyway." );
		}
		else if ( bytes_read == 0x424 ) {
			GP_DEBUG ( "canon_usb_lock_keys: Got the expected length back from \"get picture abilities.\"" );
		} else {
			GP_DEBUG ( "canon_usb_lock_keys: "
				   "Unexpected return of %i bytes (expected %i) from \"get picture abilities.\" We will continue.",
				   bytes_read, 0x424 );
		}

		memset (payload, 0, sizeof (payload));
		payload[0] = 0x06;

		c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_LOCK_KEYS_2,
					    &bytes_read, payload, 4);
		if ( c_res == NULL )
			return GP_ERROR_OS_FAILURE;
		if (bytes_read == 0xc) {
			GP_DEBUG ("canon_usb_lock_keys: Got the expected length back.");
		} else {
			gp_context_error (context,
					  _("canon_usb_lock_keys: "
					    "Unexpected length returned (%i bytes, expected %i)"),
					  bytes_read, 0xc);
			return GP_ERROR_CORRUPTED_DATA;
		}
		camera->pl->keys_locked = TRUE;
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
        unsigned int bytes_read;

        GP_DEBUG ("canon_usb_unlock_keys()");

        if ( !camera->pl->keys_locked )
                GP_DEBUG ("canon_usb_unlock_keys: keys aren't locked" );
        else
                if ( camera->pl->md->model == CANON_CLASS_4 ) {
                        c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,
                                                    &bytes_read, NULL, 0);
                        if ( c_res == NULL )
                                return GP_ERROR_OS_FAILURE;
                        if (bytes_read == 0x4) {
                                GP_DEBUG ("canon_usb_unlock_keys: Got the expected length back.");
                        } else {
                                gp_context_error (context,
                                                  _("canon_usb_unlock_keys: "
                                                    "Unexpected length returned (%i bytes, expected %i)"),
                                                  bytes_read, 0x4);
                                return GP_ERROR_CORRUPTED_DATA;
                        }
                        camera->pl->keys_locked = FALSE;
                }
                else if ( camera->pl->md->model == CANON_CLASS_6 ) {
                        c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_UNLOCK_KEYS_2,
                                                    &bytes_read, NULL, 0);
                        if ( c_res == NULL )
                                return GP_ERROR_OS_FAILURE;
                        if (bytes_read == 0x4) {
                                GP_DEBUG ("canon_usb_unlock_keys: Got the expected length back.");
                        } else {
                                gp_context_error (context,
                                                  _("canon_usb_unlock_keys: "
                                                    "Unexpected length returned (%i bytes, expected %i)"),
                                                  bytes_read, 0x4);
                                return GP_ERROR_CORRUPTED_DATA;
                        }
                        camera->pl->keys_locked = FALSE;
                }
                else {
                        /* Your camera model does not need unlocking, cannot do unlocking or
                         * we don't know how to unlock its keys.
                         */
                        GP_DEBUG ("canon_usb_unlock_keys: Key unlocking not implemented for this camera model. "
                                  "If unlocking works when using the Windows software with your camera, "
                                  "please contact %s.", MAIL_GPHOTO_DEVEL);
                }

        return GP_OK;
}

/**
 * canon_usb_get_body_id:
 * @camera: camera to query
 * @context: context for error reporting
 *
 * Gets the body ID (hardware serial number) from an EOS camera.
 *
 * Returns: body ID (hardware serial number of camera) or gphoto2
 *          error code.
 *
 */
static int
canon_usb_get_body_id (Camera *camera, GPContext *context)
{
        unsigned char *c_res;
        unsigned int bytes_read;

        GP_DEBUG ("canon_usb_get_body_id()");

        switch ( camera->pl->md->model ) {
        case CANON_CLASS_4:
                c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_GET_BODY_ID,
                                            &bytes_read, NULL, 0);
                if ( c_res == NULL )
                        return GP_ERROR_OS_FAILURE;
                else if (bytes_read == 0x8) {
                        int body_id = le32atoh ( c_res+0x4 );
                        GP_DEBUG ("canon_usb_get_body_id: Got the expected length back.");
                        if ( camera->pl->md->usb_product == 0x3044 )
                                /* EOS D30 is a special case */
                                GP_DEBUG ("canon_usb_get_body_id: body ID is %04x%05d", (body_id>>16)&0xffff, body_id&0xffff );
                        else
                                GP_DEBUG ("canon_usb_get_body_id: body ID is %u", body_id );
                        camera->pl->body_id = body_id;
                        return GP_OK;
                } else {
                        gp_context_error (context,
                                          _("canon_usb_get_body_id: "
                                            "Unexpected data length returned (%i bytes, expected %i)"),
                                          bytes_read, 0x58);
                        return GP_ERROR_CORRUPTED_DATA;
                }
                break;
        case CANON_CLASS_6:
                c_res = canon_usb_dialogue (camera, CANON_USB_FUNCTION_EOS_GET_BODY_ID_2,
                                            &bytes_read, NULL, 0);
                if ( c_res == NULL )
                        return GP_ERROR_OS_FAILURE;
                else if (bytes_read == 0x8) {
                        unsigned int body_id = le32atoh ( c_res+0x4 );
                        GP_DEBUG ("canon_usb_get_body_id: Got the expected length back.");
                        GP_DEBUG ("canon_usb_get_body_id: body ID is %010u", body_id );
                        camera->pl->body_id = body_id;
                        return GP_OK;
                } else {
                        gp_context_error (context,
                                          _("canon_usb_get_body_id: "
                                            "Unexpected data length returned (%i bytes, expected %i)"),
                                          bytes_read, 0x58);
                        return GP_ERROR_CORRUPTED_DATA;
                }
                break;
        default:
                /* As far as we know, only EOS models implement the "get body ID" function. */
                GP_DEBUG ("canon_usb_get_body_id: \"Get body ID\" not implemented for this camera model. "
                          "If the Windows software can read a body ID (hardware serial number) from your camera, "
                          "please contact %s.", MAIL_GPHOTO_DEVEL);
                break;
        }

        return GP_OK;
}

/**
 * canon_usb_poll_interrupt_pipe:
 * @camera: the Camera to work with
 * @buf: buffer to receive data read from the pipe.
 * @timeout: time in 1/1000 of a second
 *
 * Reads the interrupt pipe repeatedly until either
 * 1. a non-zero length is returned,
 * 2. an error code is returned, or
 * 3. the timeout is reached
 *
 * Returns:
 *  length of read, or
 *  zero if the timeout is reached
 *  gphoto2 error code from read that results in an I/O error.
 *
 */
static int canon_usb_poll_interrupt_pipe ( Camera *camera, unsigned char *buf, unsigned int timeout )
{
        int i = 0, status = 0, oldtimeout;
        struct timeval start, end, cur;
        double duration;

        memset ( buf, 0x81, 0x40 ); /* Put weird stuff in buffer */

	gp_port_get_timeout ( camera->port, &oldtimeout );
	gp_port_set_timeout ( camera->port, CANON_FAST_TIMEOUT );

        /* Read repeatedly until we get either an
           error, a non-zero size or hit the timeout. */
        gettimeofday ( &start, NULL );
	while (1) {
		unsigned long curduration;

		i++;
                status = gp_port_check_int ( camera->port, (char *)buf, 0x40 );
                /* Either some real data, or failure */
                if ( status != 0 && status != GP_ERROR_TIMEOUT)
                        break;
        	gettimeofday ( &cur, NULL );
		curduration =	(cur.tv_sec-start.tv_sec)*1000 +
				(cur.tv_usec-start.tv_usec)/1000;
		if (curduration >= timeout) {
			/* Timeout is not an error */
			status = 0;
			break;
		}
        }
        gettimeofday ( &end, NULL );

	gp_port_set_timeout ( camera->port, oldtimeout );

        duration  =   (double)end.tv_sec +   end.tv_usec/1e6;
        duration -= (double)start.tv_sec + start.tv_usec/1e6;
        if ( status <= 0 )
                GP_DEBUG ( "canon_usb_poll_interrupt_pipe:"
			   " interrupt read failed after %i tries, %6.3f sec \"%s\"",
                           i, duration, gp_result_as_string(status) );
        else
                GP_DEBUG ( "canon_usb_poll_interrupt_pipe:"
                           " interrupt packet took %d tries, %6.3f sec",
                           i+1, duration );

        return status;
}


int
canon_usb_wait_for_event (Camera *camera, int timeout,
		CameraEventType *eventtype, void **eventdata,
		GPContext *context)
{
        unsigned char buf2[0x40]; /* for reading from interrupt endpoint */
	unsigned char *final_state = NULL; /* For comparing
					     * before/after
					     * directories */
	unsigned int final_state_len;
        int status = GP_OK;

	if (!camera->pl->directory_state)
		status = canon_usb_list_all_dirs ( camera, &camera->pl->directory_state, &camera->pl->directory_state_length, context );
	if (status < GP_OK) {
		GP_DEBUG ("canon_usb_wait_for_event: status %d", status);
		return status;
	}

	*eventtype = GP_EVENT_TIMEOUT;
	*eventdata = NULL;
	status = canon_usb_poll_interrupt_pipe ( camera, buf2, timeout );
	GP_DEBUG ("canon_usb_wait_for_event: status %d", status);
	if (status <= GP_OK)
		return status;
	*eventtype = GP_EVENT_UNKNOWN;
	GP_DEBUG ("canon_usb_wait_for_event: bytes %x %x %x %x %x", buf2[0],buf2[1],buf2[2],buf2[3],buf2[4]);
	switch (buf2[4]) {
	case 0x0e: {
		CameraFilePath *path;
		*eventtype = GP_EVENT_FILE_ADDED;
		*eventdata = path = malloc(sizeof(CameraFilePath));
		status = canon_usb_list_all_dirs ( camera, &final_state, &final_state_len, context );
		if (status < GP_OK)
			return status;
		/* Find new file name in camera directory */
		canon_int_find_new_image ( camera, camera->pl->directory_state, camera->pl->directory_state_length, final_state, final_state_len, path );
		if (path->folder[0] != '/') {
			free (path);
			*eventtype = GP_EVENT_UNKNOWN;
			*eventdata = malloc(strlen("Failed to get added filename?")+1);
			strcpy (*eventdata, "Failed to get added filename?");
		}
		free ( camera->pl->directory_state );
		camera->pl->directory_state = final_state;
		camera->pl->directory_state_length = final_state_len;
		return GP_OK;
	}
	default:
		*eventtype = GP_EVENT_UNKNOWN;
		*eventdata = malloc(strlen("Unknown CANON event 0x01 0x02 0x03 0x04 0x05")+1);
		sprintf (*eventdata,"Unknown CANON event 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",buf2[0],buf2[1],buf2[2],buf2[3],buf2[4]);
		return GP_OK;
	}
	return GP_OK;
}
/**
 * canon_usb_capture_dialogue:
 * @camera: the Camera to work with
 * @return_length: number of bytes to read from the camera as response
 * @photo_status: a pointer to an int that returns a photographic status
 *                error codes from the camera (if any) if this function
 *                returns NULL
 * @context: context for error reporting
 *
 * Handles the "capture image" command, where we must read the
 *  interrupt pipe before getting the normal command response.
 *
 * We call canon_usb_dialogue() for the actual "release shutter"
 *  command, then handle the interrupt pipe here.
 *
 * Returns: a char * that points to the data read from the camera (or
 * NULL on failure), and sets what @return_length points to the number
 * of bytes read. If this function returns NULL, return any photographic
 * failure error codes in what @photo_status points to.
 *
 */
unsigned char *
canon_usb_capture_dialogue (Camera *camera, unsigned int *return_length, int *photo_status, GPContext *context )
{
        int status;
        unsigned char payload[9]; /* used for sending data to camera */
        static unsigned char *buffer; /* used for receiving data from camera */
        unsigned char buf2[0x40]; /* for reading from interrupt endpoint */

        int mstimeout = -1;                  /* To save original timeout after shutter release */

        /* clear this to indicate that no data is there if we abort */
        if (return_length)
                *return_length = 0;

	/* for later wait_event */
	if (!camera->pl->directory_state)
		status = canon_usb_list_all_dirs ( camera, &camera->pl->directory_state, &camera->pl->directory_state_length, context );

        GP_DEBUG ("canon_usb_capture_dialogue()");

	*photo_status = 0; /* This should only be checked by the caller
			      if canon_usb_capture_dialogue() returns null */

        /* Build payload for command, which contains subfunction code */
        memset (payload, 0x00, sizeof (payload));
        payload[0] = 4;


        /* First, let's try to make sure the interrupt pipe is clean. */
        while ( (status = canon_usb_poll_interrupt_pipe ( camera, buf2, CANON_FAST_TIMEOUT )) > 0 );

        /* Shutter release can take a long time sometimes on EOS
         * cameras; perhaps buffer is full and needs to be flushed? */
        gp_port_get_timeout (camera->port, &mstimeout);
        GP_DEBUG("canon_usb_capture_dialogue: usb port timeout starts at %dms", mstimeout);
        gp_port_set_timeout (camera->port, 15000);

        /* now send the packet to the camera */
        if ( camera->pl->md->model != CANON_CLASS_6 )
                buffer = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_CONTROL_CAMERA,
                                              return_length,
                                              payload, 8 );
        else {
                buffer = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_CONTROL_CAMERA_2,
                                              return_length,
                                              payload, 9 );
        }

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

	if ( camera->pl->md->model == CANON_CLASS_6 ) {
		/* Don't know what this command does, but the Windows
		 * software issues it here. */
		htole32a ( payload, 0x0f );
		GP_DEBUG ( "canon_usb_capture_dialogue: Issuing unknown command 0x22 for class 6 camera." );
                buffer = canon_usb_dialogue ( camera,
					      CANON_USB_FUNCTION_20D_UNKNOWN_2,
					      return_length, payload, 4 );

                if ( buffer == NULL )
                        GP_DEBUG ( "canon_usb_capture_dialogue: Unknown command 0x22 returned null buffer; continuing anyway." );
		else if ( *return_length != 0x4 )
                        GP_DEBUG ( "canon_usb_capture_dialogue: Unknown command 0x22 returned buffer of unexpected size 0x%x; continuing anyway.",
				   *return_length );
		else if ( le32atoh ( buffer+0x50 ) != 0 )
                        GP_DEBUG ( "canon_usb_capture_dialogue: Unknown command 0x22 returned status code 0x%x; continuing anyway.",
				   le32atoh ( buffer+0x50 ) );
	}
        /* Now we need to read from the interrupt pipe. Since we have
           to use the short timeout (50 ms), we need to try several
           times.
           Read until we have completion signaled (0x0a for PowerShot,
           0x0f for EOS, 0x0e for newer EOS) */
        camera->pl->capture_step = 0;
        camera->pl->thumb_length = 0; camera->pl->image_length = 0;
        camera->pl->image_key = 0x81818181;

        while ( buf2[4] != 0x0f ) {
		/* Give the camera a long time ... (even covering bulb exposures) */
                status = canon_usb_poll_interrupt_pipe ( camera, buf2, MAX_INTERRUPT_TRIES*CANON_FAST_TIMEOUT );
                if ( status > 0x17 )
                        GP_DEBUG ( "canon_usb_capture_dialogue:"
                                   " interrupt read too long (length=%i)", status );
                else if (  status <= 0 )
                        goto FAIL;

                switch ( buf2[4] ) {
                case 0x08:
                        /* Thumbnail size */
                        if ( status != 0x17 )
                                gp_log (GP_LOG_ERROR, "canon/usb.c",
					"canon_usb_capture_dialogue:"
					  " bogus length 0x%04x"
					  " for thumbnail size packet", status );
                        camera->pl->thumb_length = le32atoh ( buf2+0x11 );
                        camera->pl->image_key = le32atoh ( buf2+0x0c );
                        GP_DEBUG ( "canon_usb_capture_dialogue: thumbnail size %ld, tag=0x%08lx",
                                   camera->pl->thumb_length, camera->pl->image_key );
			camera->pl->transfer_mode &= ~REMOTE_CAPTURE_THUMB_TO_PC;
			/* Special case for class 6 (newer protocol) and the EOS 300D */
			if ( camera->pl->transfer_mode == 0
			     && ( camera->pl->md->model == CANON_CLASS_6 || camera->pl->md->usb_product == 0x3084 ) )
				goto EXIT;
                        break;
                case 0x0c:
                        /* Full image size */
                        if ( status != 0x17 )
                                gp_log (GP_LOG_ERROR, "canon/usb.c",
					"canon_usb_capture_dialogue:"
					  " bogus length 0x%04x"
					  " for full image size packet", status );
                        camera->pl->image_length = le32atoh ( buf2+0x11 );
                        camera->pl->image_key = le32atoh ( buf2+0x0c );
                        GP_DEBUG ( "canon_usb_capture_dialogue: full image size: 0x%08lx, tag=0x%08lx",
                                   camera->pl->image_length, camera->pl->image_key );
			camera->pl->transfer_mode &= ~REMOTE_CAPTURE_FULL_TO_PC;
			/* Special case for class 6 (newer protocol) and the EOS 300D */
			if (!camera->pl->secondary_image &&
			    camera->pl->transfer_mode == 0
			     && ( camera->pl->md->model == CANON_CLASS_6 || camera->pl->md->usb_product == 0x3084 ) )
				goto EXIT;

                        break;
		case 0x10:
                        /* Secondary image size, key */
			/* (only for RAW + JPEG modes) */
			GP_DEBUG ( "canon_usb_capture_dialogue: "
				   "secondary image descriptor received");

			camera->pl->image_b_length = le32atoh ( buf2+0x11 );
			camera->pl->image_b_key = le32atoh ( buf2+0x0c );

			GP_DEBUG ( "canon_usb_capture_dialogue: secondary image size: 0x%08lx, tag=0x%08lx",
				   camera->pl->image_b_length, camera->pl->image_b_key);
			camera->pl->transfer_mode &= ~REMOTE_CAPTURE_FULL_TO_PC;
			/* Special case for class 6 (newer protocol) and the EOS 300D */
			if ( camera->pl->transfer_mode == 0
			     && ( camera->pl->md->model == CANON_CLASS_6 || camera->pl->md->usb_product == 0x3084 ) )
				goto EXIT;
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
                                        gp_log (GP_LOG_ERROR, "canon/usb.c",
						"canon_usb_capture_dialogue:"
						  " first interrupt read out of sequence");
                                        goto FAIL;
                                }
                        }
                        else if ( buf2[12] == 0x1d ) {
                                GP_DEBUG ( "canon_usb_capture_dialogue: second interrupt read (after image sizes)" );
                                if ( camera->pl->capture_step != 1 ) {
                                        gp_log (GP_LOG_ERROR, "canon/usb.c",
						"canon_usb_capture_dialogue:"
						  " second interrupt read out of sequence");
                                        goto FAIL;
                                }
                                camera->pl->capture_step++;
                                /* PowerShot cameras end with this message */
                                if ( camera->pl->md->model != CANON_CLASS_4 )
                                        goto EXIT;
                        }
                        else if ( buf2[12] == 0x0a ) {
                                gp_log (GP_LOG_ERROR, "canon/usb.c",
					"canon_usb_capture_dialogue:"
					  " photographic failure signaled, code = 0x%08x",
                                         le32atoh ( buf2+16 ) );
				*photo_status = le32atoh ( buf2+16 );
                                goto FAIL2;
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
                        if ( camera->pl->capture_step != 2 && camera->pl->md->model != CANON_CLASS_6 ) {
                                gp_log (GP_LOG_ERROR, "canon/usb.c",
					"canon_usb_capture_dialogue:"
					  " third EOS interrupt read out of sequence");
                                goto FAIL;
                        }
                        camera->pl->capture_step++;
                        /* Canon SDK unlocks the keys here for EOS cameras. */
                        if ( canon_usb_unlock_keys ( camera, context ) < 0 ) {
                                GP_DEBUG ( "canon_usb_capture_dialogue: couldn't unlock keys after capture, ignoring." );
                                /* goto FAIL; ... known to not work on EOS 10D and 20D. -Marcus */
                        }
			camera->pl->transfer_mode &= ~(REMOTE_CAPTURE_THUMB_TO_DRIVE|REMOTE_CAPTURE_FULL_TO_DRIVE);
			/* Special case for class 6 (newer protocol) and the EOS 300D */
			if (	camera->pl->md->model == CANON_CLASS_6 ||
				camera->pl->md->usb_product == 0x3083 ||/* EOS 10D */
				camera->pl->md->usb_product == 0x3084 	/* EOS 300D */
			) {
				GP_DEBUG ( "canon_usb_capture_dialogue:"
					   " final interrupt read at step %i", camera->pl->capture_step );
				goto EXIT;
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
                                gp_log (GP_LOG_ERROR, "canon/usb.c",
					"canon_usb_capture_dialogue:"
					  " fourth EOS interrupt read out of sequence");
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
FAIL:  /* Try to purge interrupt pipe, which was left in an unknown state. */
        status = canon_usb_poll_interrupt_pipe ( camera, buf2, 1000 );
FAIL2:	/* After "photographic error" is signaled, we know pipe is clean. */
        canon_usb_unlock_keys ( camera, context );    /* Ignore status code, as we can't fix it anyway. */
        return NULL;
}

/**
 * canon_usb_decode_status
 * @code: status code returned in offset 0x50 in the response
 *
 * Returns: a string with the error message, or NUL if status is OK.
 */
static char *canon_usb_decode_status ( int code ) {
        unsigned int i;
        static char message[100];

        for ( i=0;
              i<sizeof(canon_usb_status_table)/sizeof(struct canon_usb_status);
              i++ )
                if ( canon_usb_status_table[i].code == code )
                        return canon_usb_status_table[i].message;

        sprintf ( message, "Unknown status code 0x%08x from camera", code );
        return message;
}

/**
 * canon_usb_dialogue_full:
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
 *      This function gets called with
 *              #canon_funct = %CANON_USB_FUNCTION_SET_TIME
 *              #payload = already constructed payload with the new time
 *      we construct a complete command packet and send this to the camera.
 *      The canon_usb_cmdstruct indicates that command
 *      CANON_USB_FUNCTION_SET_TIME returns four bytes, so we read those
 *      four bytes into our buffer and return a pointer to the buffer to
 *      the caller.
 *
 *      This should probably be changed so that the caller supplies a
 *      unsigned char ** which can be pointed to our buffer and an int
 *      returned with GP_OK or some error code.
 *
 * Returns: a char * that points to all of the packet data read from
 * the camera (or NULL on failure), and sets what @return_length
 * points to the number of bytes read.  Note that, unlike this
 * function, canon_usb_dialogue() chops off the first 0x50 bytes to
 * generate its output.
 *
 */
unsigned char *
canon_usb_dialogue_full (Camera *camera, canonCommandIndex canon_funct, unsigned int *return_length, const unsigned char *payload,
                    unsigned int payload_length)
{
        int msgsize, status, i;
        char cmd1 = 0, cmd2 = 0, *funct_descr = "";
        int cmd3 = 0, read_bytes1 = 0, read_bytes2 = 0;
	unsigned int read_bytes = 0;
        unsigned char packet[1024];     /* used for sending data to camera */
        static unsigned char buffer[0x474];     /* used for receiving data from camera */
	char *msg;
        int j, canon_subfunc = 0;
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
                GP_DEBUG ("canon_usb_dialogue_full() called for ILLEGAL function %i! Aborting.",
                          canon_funct);
                return NULL;
        }
        GP_DEBUG ("canon_usb_dialogue_full() cmd 0x%x 0x%x 0x%x (%s)", cmd1, cmd2, cmd3,
                  funct_descr);

        /*
         * The CONTROL_CAMERA function is special in that its payload specifies a
         * subcommand, and the size of the return data is dependent on which
         * subcommand we're sending the camera.  See "Protocol" file for details.
         */
        if ( ( camera->pl->md->model != CANON_CLASS_6
               && canon_usb_cmd[i].num == CANON_USB_FUNCTION_CONTROL_CAMERA )
             || ( camera->pl->md->model == CANON_CLASS_6
		  && canon_usb_cmd[i].num == CANON_USB_FUNCTION_CONTROL_CAMERA_2 ) ) {
                canon_subfunc = le32atoh (payload);
                j = 0;
                while (canon_usb_control_cmd[j].num != 0) {
                        if (canon_usb_control_cmd[j].subcmd == canon_subfunc) {
                                additional_read_bytes = canon_usb_control_cmd[j].additional_return_length;
                                break;
                        }
                        j++;
                }
                if (canon_usb_control_cmd[j].num == 0) {
                        GP_DEBUG("canon_usb_dialogue_full(): CONTROL_CAMERA called for ILLEGAL "
                                 "sub function %i! Aborting.", canon_subfunc);
                        return NULL;
                }
                read_bytes += additional_read_bytes;

                GP_DEBUG ("canon_usb_dialogue_full() called with CONTROL_CAMERA, %s",
                          canon_usb_control_cmd[j].description);
		if ( !strcmp ( "Set transfer mode", canon_usb_control_cmd[j].description ) ) {
			/* We need to remember the transfer mode, as with
			 * newer cameras it changes capture
			 * completion. */
			camera->pl->transfer_mode = payload[8];
			GP_DEBUG ( "canon_usb_dialogue_full() setting transfer mode to %d",
				   camera->pl->transfer_mode );
		}
        }

        if (read_bytes > sizeof (buffer)) {
                /* If this message is ever printed, chances are that you just added
                 * a new command to canon_usb_cmd with a return_length greater than
                 * all the others and did not update the declaration of 'buffer' in
                 * this function.
                 */
                GP_DEBUG ("canon_usb_dialogue_full() "
                          "read_bytes %i won't fit in buffer of size %li!", read_bytes,
                          (long)sizeof (buffer));
                return NULL;
        }

        if (payload_length)
                GP_LOG_DATA ((char *)payload, (long)payload_length, "Payload:");

        if ((payload_length + 0x50) > sizeof (packet)) {
                gp_log (GP_LOG_DEBUG, "canon/usb.c",
                        "canon_usb_dialogue:"
			  " payload too big, won't fit into buffer (%i > %i)",
                        (payload_length + 0x50), (int)sizeof (packet));
                return NULL;
        }

        /* OK, we have now checked for all errors I could think of,
         * proceed with the actual work.
         */

        /* construct packet to send to camera, including the three
         * commands, serial number and a payload if one has been supplied
         */

        memset (packet, 0x00, sizeof (packet)); /* zero block */
        htole32a (packet, 0x10 + payload_length);
        packet[0x40] = 0x2;
        packet[0x44] = cmd1;
        packet[0x47] = cmd2;
        htole32a (packet + 0x04, cmd3);
        if ( camera->pl->md->model == CANON_CLASS_6 ) {
                /* New style of protocol is picky about this byte. */
                if ( cmd3 == 0x202 )
                        packet[0x46] = 0x20;
                else
                        packet[0x46] = 0x10;
        }

        htole32a (packet + 0x4c, serial_code++);        /* serial number */
        htole32a (packet + 0x48, 0x10 + payload_length);
        msgsize = 0x50 + payload_length;        /* TOTAL msg size */

        if (payload_length > 0)
                memcpy (packet + 0x50, payload, payload_length);

        /* now send the packet to the camera */
        status = gp_port_usb_msg_write (camera->port, msgsize > 1 ? 0x04 : 0x0c, 0x10, 0,
                                        (char *)packet, msgsize);
        if (status != msgsize) {
                GP_DEBUG ("canon_usb_dialogue_full: write failed! (returned %i)", status);
                return NULL;
        }

        /* and, if this canon_funct is known to generate a response from the camera,
         * read this response back.
         */

        /* Even the new cameras use multiple reads sometimes,
         * e.g. "Get camera abilities". */
        if ( camera->pl->md->model != CANON_CLASS_6 && read_bytes <= 0x400 ) {
                /*
                 * Divide read_bytes into two parts.
                 *
                 * - The first read is for the length rounded down to a length
                 *   divisible by 0x40. The 32 bits starting at the beginning
                 *   of the buffer may contain the length information; if not,
                 *   the 32 bits starting at offset 0x48 will contain the
                 *   length information, which we check against the expected
                 *   length and log a warning if the two do not agree. We then
                 *   read the rest of the data, if any, in a single chunk.
                 *
                 * - The final read is for the remainder of the data. This
                 *   can never be longer than 0x37 bytes.
                 *
                 * Read two is optional. If the requested length is only
                 * 0x40, only the first read will be issued.  If the length is
                 * divisible by 0x40, the second read will be skipped.
                 *
                 * We read like this because the Windows driver reads in two
                 * operations, and some cameras (EOS D30 for example) seem to
                 * not like it if we were to read read_bytes in a single read
                 * instead.
                 *
                 * We check the size a safety measure: if we are mistaken
                 * about the length of data returned for some command, we can
                 * work around it with just a warning. The "get camera
                 * abilities" command, in particular, seems to vary from
                 * camera to camera.
                 */
                read_bytes1 = read_bytes - (read_bytes % 0x40);
                status = gp_port_read (camera->port, (char *)buffer, read_bytes1);
                if (status != read_bytes1) {
                        if ( status >= 0 )
                                GP_DEBUG ("canon_usb_dialogue_full: read 1 of 0x%x bytes failed! (returned %i)",
                                          read_bytes1, status);
                        else                         /* Error code */
                                GP_DEBUG ("canon_usb_dialogue_full: read 1 of 0x%x bytes failed! (%s)",
                                          read_bytes1, gp_result_as_string ( status ) );
                        return NULL;
                }

                if ( cmd3 != 0x202 ) {
                        unsigned int reported_length = le32atoh (buffer);
                        if ( reported_length == 0 ) {
                                /* No length at start of packet. Did we read enough to
                                 * see the length later on? */
                                GP_DEBUG ( "canon_usb_dialogue_full: no length at start of packet." );
                                if ( read_bytes1 >= 0x50 ) {
                                        reported_length = le32atoh (buffer+0x48);
                                        GP_DEBUG ( "canon_usb_dialogue_full: got length from offset 0x48." );
                                }
                        }
                        GP_DEBUG ("canon_usb_dialogue_full: camera reports 0x%x bytes (0x%x total)",
                                  reported_length, reported_length+0x40 );

                        if ( reported_length > 0 && reported_length+0x40 != read_bytes ) {
                                gp_log (GP_LOG_DEBUG, "canon/usb.c",
					"canon_usb_dialogue:"
					  " expected 0x%x bytes, but camera reports 0x%x",
                                         read_bytes, reported_length+0x40 );
                                read_bytes = reported_length+0x40;
                        }
                }

                read_bytes2 = read_bytes - read_bytes1;

                if ( read_bytes2 > 0 ) {
                        status = gp_port_read (camera->port, (char *)buffer + read_bytes1, read_bytes2);
                        if (status != read_bytes2) {
                                if ( status >= 0 )
                                        GP_DEBUG ("canon_usb_dialogue_full: read 2 of %i bytes failed! (returned %i)",
                                                  read_bytes2, status);
                                else                         /* Error code */
                                        GP_DEBUG ("canon_usb_dialogue_full: read 2 of %i bytes failed! (%s)",
                                                  read_bytes2, gp_result_as_string ( status ) );
                                return NULL;
                        }
                }
        }
        else {
                /* Newer protocol: these cameras allow (and some
                 * demand) the host software to read the entire
                 * response in one operation. */
                status = gp_port_read (camera->port, (char *)buffer, read_bytes );
                if ( status != (int)read_bytes ) {
                        if ( status >= 0 )
                                GP_DEBUG ("canon_usb_dialogue_full: single read of %i bytes failed! (returned %i)",
                                          read_bytes, status);
                        else                         /* Error code */
                                GP_DEBUG ("canon_usb_dialogue_full: single read of %i bytes failed! (%s)",
                                          read_bytes, gp_result_as_string ( status ) );
                        return NULL;
                }
        }

                msg = canon_usb_decode_status ( le32atoh ( buffer+0x50 ) );
                if ( msg != NULL ) {
                        GP_DEBUG ( "canon_usb_dialogue_full: camera status \"%s\""
				   " in response to command 0x%x 0x%x 0x%x (%s)",
				   msg, cmd1, cmd2, cmd3, funct_descr );
			return NULL;
                }

	if (return_length)
		*return_length = read_bytes;
	return buffer;
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
 *      This function gets called with
 *              #canon_funct = %CANON_USB_FUNCTION_SET_TIME
 *              #payload = already constructed payload with the new time
 *      we construct a complete command packet and send this to the camera.
 *      The canon_usb_cmdstruct indicates that command
 *      CANON_USB_FUNCTION_SET_TIME returns four bytes, so we read those
 *      four bytes into our buffer and return a pointer to the buffer to
 *      the caller.
 *
 *      This should probably be changed so that the caller supplies a
 *      unsigned char ** which can be pointed to our buffer and an int
 *      returned with GP_OK or some error code.
 *
 * Returns: a char * that points to the data read from the camera (or
 * NULL on failure), and sets what @return_length points to the number
 * of bytes read.
 *
 */
unsigned char *
canon_usb_dialogue (Camera *camera, canonCommandIndex canon_funct, unsigned int *return_length, const unsigned char *payload,
                    unsigned int payload_length)
{
	unsigned char *buffer;

	buffer = canon_usb_dialogue_full (camera, canon_funct, return_length,
					  payload, payload_length);

	/* Remove the packet header from the response */
	if (return_length)
		*return_length = *return_length - 0x50;
	if (buffer)
		return buffer + 0x50;
	else
		return NULL;
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
                         unsigned int *data_length, unsigned int max_data_size, const unsigned char *payload,
                         unsigned int payload_length, int display_status, GPContext *context)
{
        int bytes_read;
	unsigned int dialogue_len;
        unsigned int total_data_size = 0, bytes_received = 0, read_bytes = camera->pl->xfer_length;
        unsigned char *lpacket;         /* "length packet" */
        unsigned int id = 0;

        /* indicate there is no data if we bail out somewhere */
        *data_length = 0;

        GP_DEBUG ("canon_usb_long_dialogue() function %i, payload = %i bytes", canon_funct,
                  payload_length);

        /* Call canon_usb_dialogue_full(), this will not return any data "the usual way"
         * but after this we read 0x40 bytes from the USB port, the int at pos 6 in
         * the returned data holds the total number of bytes we are to read.
         */
        lpacket =
                canon_usb_dialogue_full (camera, canon_funct, &dialogue_len, payload, payload_length);
        if (lpacket == NULL) {
                GP_DEBUG ("canon_usb_long_dialogue: canon_usb_dialogue returned error!");
                return GP_ERROR_OS_FAILURE;
        }
        /* This check should not be needed since we check the return of canon_usb_dialogue()
         * above, but as the saying goes: better safe than sorry.
         */
        if (dialogue_len != 0x40) {
                GP_DEBUG ("canon_usb_long_dialogue: canon_usb_dialogue "
                          "returned %i bytes, not the length "
                          "we expected (%i)!. Aborting.", dialogue_len, 0x40);
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
                if ((total_data_size - bytes_received) > camera->pl->xfer_length )
                        /* Limit max transfer length */
                        read_bytes = camera->pl->xfer_length;
                else if ((total_data_size - bytes_received) > 0x040 && camera->pl->md->model != CANON_CLASS_6 )
                        /* Round longer transfers down to nearest 0x40 */
                        read_bytes = (total_data_size - bytes_received) / 0x40 * 0x40;
                else
                        /* Final transfer; use true length */
                        read_bytes = (total_data_size - bytes_received);

                GP_DEBUG ("canon_usb_long_dialogue: total_data_size = %i, "
                          "bytes_received = %i, read_bytes = %i (0x%x)", total_data_size,
                          bytes_received, read_bytes, read_bytes);
                bytes_read = gp_port_read (camera->port, (char *)*data + bytes_received, read_bytes);
                if (bytes_read < 1) {
                        GP_DEBUG ("canon_usb_long_dialogue: gp_port_read() returned error (%i) or no data",
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
                } else if ((unsigned int)bytes_read < read_bytes)
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
 * @camera: camera to use
 * @name: name of file to fetch
 * @data: to receive image data
 * @length: to receive length of image data
 * @context: context for error reporting
 *
 * Get a file from a USB-connected Canon camera.
 *
 * Returns: gphoto2 error code, length in @length, and image data in @data.
 *
 */
int
canon_usb_get_file (Camera *camera, const char *name, unsigned char **data, unsigned int *length,
                    GPContext *context)
{
        char payload[100];
        int payload_length, res, offset;

        GP_DEBUG ("canon_usb_get_file() called for file '%s'", name);

        /* Construct payload containing file name, buffer size and
         * function request.  See the file Protocol.xml in the doc/
         * directory for more information.
         */
        if ( camera->pl->md->model == CANON_CLASS_6 ) {
                offset = 4;
                if ( offset + strlen (name) > sizeof (payload) - 2 ) {
                        GP_DEBUG ("canon_usb_get_file: ERROR: "
                                  "Supplied file name '%s' does not fit in payload buffer.", name);
                        return GP_ERROR_BAD_PARAMETERS;
                }
                htole32a (payload, 0x0);        /* get picture */
                strncpy ( payload+offset, name, sizeof(payload)-offset-1 );
                payload[offset + strlen (payload+offset)] = 0;
                payload_length = offset + strlen (payload+offset) + 2;
                GP_DEBUG ( "canon_usb_get_file: payload 0x%08x:%s",
                           le32atoh(payload), payload+offset );
        }
        else {
                offset = 8;
                if ( offset + strlen (name) > sizeof (payload) - 1 ) {
                        GP_DEBUG ("canon_usb_get_file: ERROR: "
                                  "Supplied file name '%s' does not fit in payload buffer.", name);
                        return GP_ERROR_BAD_PARAMETERS;
                }
                htole32a (payload, 0x0);        /* get picture */
                htole32a (payload + 0x4, camera->pl->xfer_length);
                strncpy ( payload+offset, name, sizeof(payload)-offset );
                payload_length = offset + strlen (payload+offset) + 1;
                GP_DEBUG ( "canon_usb_get_file: payload 0x%08x:0x%08x:%s",
                           le32atoh(payload), le32atoh(payload+4), payload+offset );
        }

        /* the 1 is to show status */
        res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
                                       camera->pl->md->max_movie_size, (unsigned char *)payload,
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
 * @camera: camera to use
 * @name: name of thumbnail to fetch
 * @data: to receive image data
 * @length: to receive length of image data
 * @context: context for error reporting
 *
 * Gets a thumbnail, including EXIF info, from a USB_connected Canon
 *   camera.
 *
 * Returns: gphoto2 error code, length in @length, and image data in
 *    @data.
 *
 */
int
canon_usb_get_thumbnail (Camera *camera, const char *name, unsigned char **data, unsigned int *length,
                         GPContext *context)
{
        char payload[100];
        unsigned int payload_length;
	int res, offset;

        GP_DEBUG ("canon_usb_get_thumbnail() called for file '%s'", name);

        if ( camera->pl->md->model == CANON_CLASS_6 ) {
		/* Construct payload containing file name and function
		 * request.  See the file Protocol.xml in directory
		 * camlibs/canon/doc for more information.
		 */

                offset = 4;
                if ( offset + strlen (name) > sizeof (payload) - 2 ) {
                        GP_DEBUG ("canon_usb_get_thumbnail: ERROR: "
                                  "Supplied file name '%s' does not fit in payload buffer.", name);
                        return GP_ERROR_BAD_PARAMETERS;
                }
                strncpy ( payload+offset, name, sizeof(payload)-offset-1 );
                payload[offset + strlen (payload+offset)] = 0;
                payload_length = offset + strlen (payload+offset) + 2;
		htole32a (payload, 0x1);        /* get thumbnail */
                GP_DEBUG ( "canon_usb_get_thumbnail: payload 0x%08x:%s",
                           le32atoh(payload), payload+offset );
        }
        else {
		/* Construct payload containing file name, buffer size
		 * and function request.  See the file Protocol.xml in
		 * directory camlibs/canon/doc for more information.
		 */

                offset = 8;
                if ( offset + strlen (name) > sizeof (payload) - 1 ) {
                        GP_DEBUG ("canon_usb_get_thumbnail: ERROR: "
                                  "Supplied file name '%s' does not fit in payload buffer.", name);
                        return GP_ERROR_BAD_PARAMETERS;
                }
		htole32a (payload, 0x1);        /* get thumbnail */
                htole32a (payload + 0x4, camera->pl->xfer_length);
                strncpy ( payload+offset, name, sizeof(payload)-offset );
                payload_length = offset + strlen (payload+offset) + 1;
                GP_DEBUG ( "canon_usb_get_thumbnail: payload 0x%08x:0x%08x:%s",
                           le32atoh(payload), le32atoh(payload+4), payload+offset );
        }

        /* 0 is to not show status */
        res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
                                       camera->pl->md->max_thumbnail_size, (unsigned char *)payload,
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
canon_usb_get_captured_image (Camera *camera, const int key, unsigned char **data, unsigned int *length,
                              GPContext *context)
{
        unsigned char payload[16];
        int payload_length = 16, result;

        GP_DEBUG ("canon_usb_get_captured_image() called");

        /* Construct payload containing buffer size and function request.
         * See the file Protocol in this directory for more information.
         */
        htole32a (payload, 0x0);        /* always zero */
        htole32a (payload + 0x4, camera->pl->xfer_length);
        htole32a (payload + 0x8, CANON_DOWNLOAD_FULL);
        htole32a (payload + 0xc, key);

        /* the 1 is to show status */
	if ( camera->pl->md->model == CANON_CLASS_6 )
		result = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_RETRIEVE_CAPTURE_2, data, length,
						  0, payload,
						  payload_length, 1, context);
	else
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
 * canon_usb_get_captured_secondary_image:
 * @camera: camera to work on
 * @key: index of image to fetch, returned from canon_usb_capture_dialogue()
 * @data: to receive image data
 * @length: to receive length of image data
 * @context: context for error reporting
 *
 * Gets the just-captured image from a USB-connected Canon
 * camera. This function must be called soon after an image capture,
 * and needs the image key returned by canon_usb_capture_dialogue().
 * This function is only used for "secondary images," e.g., the JPEGs
 * downloaded when the camera's capture image format mode is set to "RAW + JPEG"
 *
 * Returns: gphoto2 error code, length in @length, and image data in @data.
 *
 */
int
canon_usb_get_captured_secondary_image (Camera *camera, const int key, unsigned char **data, unsigned int *length,
					GPContext *context)
{
        unsigned char payload[16];
        int payload_length = 16, result;

        GP_DEBUG ("canon_usb_get_captured_secondary_image() called");

        /* Construct payload containing buffer size and function request.
         * See the file Protocol in this directory for more information.
         */
        htole32a (payload, 0x0);        /* always zero */
        htole32a (payload + 0x4, camera->pl->xfer_length);
        htole32a (payload + 0x8, CANON_DOWNLOAD_SECONDARY);
        htole32a (payload + 0xc, key);

        /* the 1 is to show status */
	if ( camera->pl->md->model == CANON_CLASS_6 )
		result = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_RETRIEVE_CAPTURE_2, data, length,
						  0, payload,
						  payload_length, 1, context);
	else
		result = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_RETRIEVE_CAPTURE, data, length,
						  0, payload,
						  payload_length, 1, context);
        if (result != GP_OK) {
                GP_DEBUG ("canon_usb_get_captured_secondary_image: canon_usb_long_dialogue() "
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
canon_usb_get_captured_thumbnail (Camera *camera, const int key, unsigned char **data, unsigned int *length,
				  GPContext *context)
{
        unsigned char payload[16];
        int payload_length = 16, result;

        GP_DEBUG ("canon_usb_get_captured_thumbnail() called");

        /* Construct payload containing buffer size and function request.
         * See the file Protocol in this directory for more information.
         */
        htole32a (payload, 0x0);        /* get picture */
        htole32a (payload + 0x4, camera->pl->xfer_length);
        htole32a (payload + 0x8, CANON_DOWNLOAD_THUMB);
        htole32a (payload + 0xc, key);

        /* the 1 is to show status */
	if ( camera->pl->md->model == CANON_CLASS_6 )
		result = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_RETRIEVE_CAPTURE_2, data, length,
						  0, payload,
						  payload_length, 1, context);
	else
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
        unsigned int payload_size =  0x4 + strlen(camera_filename) + 2,
                bytes_read;
        unsigned char *payload = malloc ( payload_size );

        if ( payload == NULL ) {
                GP_DEBUG ( "canon_usb_set_file_time:"
                           " failed to allocate payload block." );
                gp_context_error(context, _("Out of memory: %d bytes needed."),
                                 payload_size );
                return GP_ERROR_NO_MEMORY;
        }
        memset ( payload, 0, payload_size );

        strcpy ( (char *)payload + 0x4, camera_filename );
        htole32a ( payload, time );          /* Load specified time for camera directory. */
        result_buffer = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_SET_FILE_TIME,
					     &bytes_read, payload, payload_size );
        free ( payload );
        if ( result_buffer == NULL ) {
                GP_DEBUG ( "canon_usb_set_file_time:"
                           " dialogue failed." );
                return GP_ERROR_OS_FAILURE;
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
canon_usb_set_file_attributes (Camera *camera, unsigned int attr_bits,
                               const char *dir, const char *file,
                               GPContext *context)
{
        /* Pathname string plus 32-bit parameter plus two NULs. */
        unsigned int payload_length = 4 + strlen (dir) + 1 + strlen (file) + 2;
        unsigned char *payload = calloc ( payload_length, sizeof ( unsigned char ) );
        unsigned int bytes_read;
        unsigned char *res;

        GP_DEBUG ( "canon_usb_set_file_attributes()" );
        GP_DEBUG ( "canon_usb_set_file_attributes(): payload is %d=0x%x bytes; string length is %d=0x%x",
                   payload_length, payload_length, (int)strlen(dir), (int)strlen(dir) );
        /* build payload :
         *
         * <attr bits> directory 0x00 file 0x00 0x00
         *
         * The attribute bits will be a 32-bit little-endian integer.
         */
        memset (payload, 0x00, payload_length);
        memcpy (payload + 4, dir, strlen (dir));
        memcpy (payload + 4 + strlen(dir) + 1, file, strlen (file) );
        htole32a ( payload, (unsigned long)attr_bits );

        if ( camera->pl->md->model == CANON_CLASS_6 )
                res = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_SET_ATTR_2, &bytes_read,
                                           payload, payload_length );
        else
                res = canon_usb_dialogue ( camera, CANON_USB_FUNCTION_SET_ATTR, &bytes_read,
                                           payload, payload_length );
        if ( res == NULL ) {
                gp_context_error (context,
                                  _("canon_usb_set_file_attributes: "
				    "canon_usb_dialogue failed"));
                free ( payload );
                return GP_ERROR_OS_FAILURE;
        }
        else if ( le32atoh ( res+0x50 ) != 0 ) {
                gp_context_message (context,
				    _("Warning in canon_usb_set_file_attributes: "
				      "canon_usb_dialogue returned error status 0x%08x from camera"),
				    le32atoh ( res+0x50 ) );
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
 * The problem only appears when USB deinitialization and initialization
 * is performed between uploads. You can call this function more than
 * once with big files during one session without encountering the problem
 * described. <kramm@quiss.org>
 *
 * Returns: gphoto2 error code
 *
 */

#ifndef CANON_EXPERIMENTAL_UPLOAD
int
canon_usb_put_file (Camera __unused__ *camera, CameraFile __unused__ *file,
		    const char __unused__ *filename, const char __unused__ *destname, const char __unused__ *destpath,
                    GPContext __unused__ *context)
{
        return GP_ERROR_NOT_SUPPORTED;
}

#else /* else ifdef CANON_EXPERIMENTAL_UPLOAD */

int
canon_usb_put_file (Camera *camera, CameraFile *file,
		    const char *xfilename, const char *destname, const char *destpath,
                    GPContext *context)
{
        long int packet_size = USB_BULK_WRITE_SIZE;
        char buffer[0x80];
        long int len1,len2;
        int status;
        long int offs = 0;
        char filename[256];
        int filename_len;
        char *data;
        char *newdata = NULL;
        long int size;                       /* Total size of file to upload */
        FILE *fi;
        const char *srcname;
        char *packet;
        struct stat filestat;                /* To get time from Unix file */

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
        status = gp_file_get_name (file, &srcname)
	if (status < GP_OK) {
		free (packet);
		return status;
	}

        /* Open input file and read all its data into a buffer. */
        if(!gp_file_get_data_and_size (file, (const char **)&data, &size)) {
		fi = fopen(srcname, "rb");
		if(!fi) {
			gp_context_error(context, _("Couldn't read from file \"%s\""), srcname);
			free(packet);
			return GP_ERROR_OS_FAILURE;
		}
		fstat ( fileno(fi), &filestat );
		fseek(fi, 0, SEEK_END);
		size = ftell(fi);
		fseek(fi, 0, SEEK_SET);
		newdata = data = malloc(size);
		if(!newdata) {
			gp_context_error(context, _("Out of memory: %ld bytes needed."), size);
			free(packet);
			return GP_ERROR_NO_MEMORY;
		}
		fread(newdata, size, 1, fi);
		fclose(fi);
        }

        GP_DEBUG ( "canon_put_file_usb:"
                   " finished reading file, %ld bytes (0x%lx)",
                   size, size );

        /* Take the buffer and send it to the camera, breaking it into
         * packets of suitable size. */
        while(offs < size) {
		len2 = packet_size;
		if(size - offs < len2)
			len2 = size - offs;
		len1 = len2 + 0x1c + filename_len + 1;

		GP_DEBUG ( "canon_put_file_usb: len1=%lx, len2=%lx", len1, len2 );

		memset(packet, 0, 0x40);
		packet[4]=3;
		packet[5]=2;
		htole32a ( packet + 0x6, len1 + 0x40 );
		htole32a (packet + 0x4c, serial_code++); /* Serial number */

		/* now send the packet to the camera */
		status = gp_port_usb_msg_write (camera->port, 0x04, 0x10, 0,
						packet, 0x40);
		if (status != 0x40) {
			GP_DEBUG ("canon_put_file_usb: write 1 failed! (returned %i)", status);
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
				  "(returned %i, expected %li)", status, len1+0x40);
			gp_context_error(context, _("File upload failed."));
			if(newdata)
				free(newdata);
			free(packet);
			if ( status < 0 )
				return GP_ERROR_OS_FAILURE;
			else
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
			if ( status < 0 )
				return GP_ERROR_OS_FAILURE;
			else
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
			if ( status < 0 )
				return GP_ERROR_OS_FAILURE;
			else
				return GP_ERROR_CORRUPTED_DATA;
		}
		else {
			char *msg = canon_usb_decode_status ( le32atoh ( buffer+0x50 ) );
			if ( msg != NULL ) {
				GP_DEBUG ( "canon_put_file_usb: camera status \"%s\" during upload",
					   msg );
			}
		}
		offs += len2;
        }

        /* Now we finish the job. */
        canon_usb_set_file_time ( camera, filename, filestat.st_mtime, context );

        canon_usb_set_file_attributes ( camera, (unsigned int)0,
                                        destpath, destname, context );

        if(size > 65572-filename_len) {
		gp_context_message(context, _("File was too big. You may have to turn your camera off and back on before uploading more files."));
        }

        if(newdata)
		free(newdata);
        free(packet);
        return GP_OK;
}
#endif /* CANON_EXPERIMENTAL_UPLOAD */


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
                          "Path '%s' too long (%li), won't fit in payload buffer.", path,
                          (long)strlen (path));
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
                return res;

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
        char *disk_name = canon_int_get_disk_name (camera, context);
        int res;

        *dirent_data = NULL;

	if (!disk_name) /* should only happen on IO error */
		return GP_ERROR_IO;

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
                          "Path '%s' too long (%li), won't fit in payload buffer.",
                          disk_name, (long)strlen (disk_name));
                gp_context_error (context,
                                  _("canon_usb_list_all_dirs: "
				    "Couldn't fit payload into buffer, "
				    "'%.96s' (truncated) too long."), disk_name);
                return GP_ERROR_BAD_PARAMETERS;
        }
        memset (payload, 0x00, sizeof (payload));
        memcpy (payload + 1, disk_name, strlen (disk_name));
        payload[0] = 0x0f;                   /* Recursion depth */
        payload_length = strlen (disk_name) + 4;
        free ( disk_name );

        /* Give no max data size.
         * 0 is to not show progress status
         */
        res = canon_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_DIRENT, (unsigned char **)dirent_data,
                                       dirents_length, 0, payload, payload_length, 0,
                                       context);
        if (res != GP_OK) {
                gp_context_error (context,
                                  _("canon_usb_list_all_dirs: "
				    "canon_usb_long_dialogue failed to fetch direntries, "
				    "returned %i"), res);
                return res;
        }

        return GP_OK;
}

/**
 * canon_usb_ready:
 * @camera: camera to check
 *
 * Checks whether camera is available for communication.
 *
 * Returns: gphoto2 error code
 *          GP_OK if camera is ready
 *
 */
int
canon_usb_ready (Camera *camera, GPContext __unused__ *context)
{
	unsigned char *msg;
        unsigned int len;

        GP_DEBUG ("canon_usb_ready()");

        /* "Identify camera" seems to be a command compatible with all
	 * cameras, and one that doesn't change the camera state. */
	msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_IDENTIFY_CAMERA,
				  &len, NULL, 0);
	if ( msg == NULL )
		return GP_ERROR_OS_FAILURE;

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
static int
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
                        GP_DEBUG ("canon_usb_identify: USB ID match 0x%04x:0x%04x (model name \"%s\")",
                                  models[i].usb_vendor, models[i].usb_product, models[i].id_str);
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
