/****************************************************************************
 *
 * File: serial.c
 *
 * Serial communication layer.
 *
 * $Id$
 ****************************************************************************/

/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <ctype.h>

#include <gphoto2.h>

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

#include "library.h"
#include "canon.h"
#include "serial.h"
#include "util.h"
#include "crc.h"

void
serial_flush_input (GPPort *gdev)
{
}

void
serial_flush_output (GPPort *gdev)
{
}

/*****************************************************************************
 *
 * canon_serial_change_speed
 *
 * change the speed of the communication.
 *
 * speed - the new speed
 *
 * Returns 1 on success.
 * Returns 0 on any error.
 *
 ****************************************************************************/

int
canon_serial_change_speed (GPPort *gdev, int speed)
{
	gp_port_settings settings;

	/* set speed */
	gp_port_get_settings (gdev, &settings);
	settings.serial.speed = speed;
	gp_port_set_settings (gdev, settings);

	usleep (70000);

	return 1;
}


/*****************************************************************************
 *
 * canon_serial_get_cts
 *
 * Gets the status of the CTS (Clear To Send) line on the serial port.
 *
 * CTS is "1" when the camera is ON, and "0" when it is OFF.
 *
 * Returns 1 on CTS high.
 * Returns 0 on CTS low.
 *
 ****************************************************************************/
int
canon_serial_get_cts (GPPort *gdev)
{
	GPLevel level;

	gp_port_get_pin (gdev, PIN_CTS, &level);
	return (level);
}

/*****************************************************************************
 *
 * canon_serial_init
 *
 * Initializes the given serial or USB device.
 *
 * devname - the name of the device to open
 *
 * Returns 0 on success.
 * Returns -1 on any error.
 *
 ****************************************************************************/

int
canon_serial_init (Camera *camera)
{
	GPPortSettings settings;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "Initializing the (serial) camera.\n");

	/* Get the current settings */
	gp_port_get_settings (camera->port, &settings);

	/* Adjust the current settings */
	settings.serial.speed = 9600;
	settings.serial.bits = 8;
	settings.serial.parity = 0;
	settings.serial.stopbits = 1;

	/* Set the new settings */
	gp_port_set_settings (camera->port, settings);

	return GP_OK;
}

/*****************************************************************************
 *
 * canon_serial_send
 *
 * Send the given buffer with given length over the serial line.
 *
 * buf   - the raw data buffer to send
 * len   - the length of the buffer
 * sleep - time in usec to wait between characters
 *
 * Returns 0 on success, -1 on error.
 *
 ****************************************************************************/

int
canon_serial_send (Camera *camera, const unsigned char *buf, int len, int sleep)
{
	int i;

	/* the A50 does not like to get too much data in a row at 115200
	 * The S10 and S20 do not have this problem */
	if (sleep > 0 && camera->pl->slow_send == 1) {
		for (i = 0; i < len; i++) {
			gp_port_write (camera->port, (char *) buf, 1);
			buf++;
			usleep (sleep);
		}
	} else {
		gp_port_write (camera->port, (char *) buf, len);
	}

	return 0;
}


/**
 * Sets the timeout, in miliseconds.
 */
void
serial_set_timeout (GPPort *gdev, int to)
{
	gp_port_set_timeout (gdev, to);
}

/*****************************************************************************
 *
 * canon_serial_get_byte
 *
 * Get the next byte from the serial line.
 * Actually the function reads chunks of data and keeps them in a cache.
 * Only one byte per call will be returned.
 *
 * Returns the byte on success, -1 on error.
 *
 ****************************************************************************/
int
canon_serial_get_byte (GPPort *gdev)
{
	static unsigned char cache[512];
	static unsigned char *cachep = cache;
	static unsigned char *cachee = cache;
	int recv;

	/* if still data in cache, get it */
	if (cachep < cachee) {
		return (int) *cachep++;
	}

	recv = gp_port_read (gdev, cache, 1);
	if (recv < 0)		/* An error occurred */
		return -1;

	cachep = cache;
	cachee = cache + recv;

	if (recv) {
		return (int) *cachep++;
	}

	return -1;
}

/* ------------------------- Frame-level processing ------------------------- */

int
canon_serial_send_frame (Camera *camera, const unsigned char *pkt, int len)
{
	static unsigned char buffer[2100];

	/* worst case: two maximum-sized packets (~1020 bytes, full of data
	   that needs to be escaped */
	unsigned char *p;

	p = buffer;
	*p++ = CANON_FBEG;
	while (len--) {
		if (p - buffer >= sizeof (buffer) - 1) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "FATAL ERROR: send buffer overflow\n");
			return -1;
		}
		if (*pkt != CANON_FBEG && *pkt != CANON_FEND && *pkt != CANON_ESC)
			*p++ = *pkt++;
		else {
			*p++ = CANON_ESC;
			*p++ = *pkt++ ^ CANON_XOR;
		}
	}
	*p++ = CANON_FEND;

	return !canon_serial_send (camera, buffer, p - buffer, USLEEP2);
}


unsigned char *
canon_serial_recv_frame (Camera *camera, int *len)
{
	static unsigned char buffer[5000];

	/* more than enough :-) (allow for a few run-together packets) */
	unsigned char *p = buffer;
	int c;

	while ((c = canon_serial_get_byte (camera->port)) != CANON_FBEG) {
		if (c == -1)
			return NULL;
	}
	while ((c = canon_serial_get_byte (camera->port)) != CANON_FEND) {
		if (c < 0)
			return NULL;
		if (c == CANON_ESC)
			c = canon_serial_get_byte (camera->port) ^ CANON_XOR;
		if (p - buffer >= sizeof (buffer)) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "FATAL ERROR: receive buffer overflow\n");
			return NULL;
		}
		*p++ = c;
	}

	/* If you don't want to see the data dumped, change the frontend to
	 * set a lower debug level
	 */
	gp_log (GP_LOG_DATA, "canon", "RECV (without CANON_FBEG and CANON_FEND bytes)");
	gp_log_data ("canon", buffer, p - buffer);

	if (len)
		*len = p - buffer;
	return buffer;
}

/* ------------------------ Packet-level processing ------------------------- */


int
canon_serial_send_packet (Camera *camera, unsigned char type,
			  unsigned char seq, unsigned char *pkt, int len)
{
	unsigned char *hdr = pkt - PKT_HDR_LEN;
	unsigned short crc;

	hdr[PKT_TYPE] = type;
	hdr[PKT_SEQ] = seq;
	hdr[PKT_LEN_LSB] = len & 0xff;
	hdr[PKT_LEN_MSB] = len >> 8;

	if (type == PKT_NACK) {
		hdr[PKT_TYPE] = PKT_ACK;
		hdr[PKT_TYPE + 1] = '\xff';	/* PKTACK_NACK; */
	}

	if (type == PKT_UPLOAD_EOT) {
		hdr[PKT_TYPE] = PKT_EOT;
		hdr[PKT_TYPE + 1] = 0x3;
		len = 2;
	}

	if (type == PKT_EOT || type == PKT_ACK || type == PKT_NACK)
		len = 2;	/* @@@ hack */
	crc = canon_psa50_gen_crc (hdr, len + PKT_HDR_LEN);
	pkt[len] = crc & 0xff;
	pkt[len + 1] = crc >> 8;

	return canon_serial_send_frame (camera, hdr, len + PKT_HDR_LEN + 2);
}

unsigned char *
canon_serial_recv_packet (Camera *camera, unsigned char *type, unsigned char *seq, int *len)
{
	unsigned char *pkt;
	unsigned short crc;
	int raw_length, length = 0;

	pkt = canon_serial_recv_frame (camera, &raw_length);
	if (!pkt)
		return NULL;
	if (raw_length < PKT_HDR_LEN) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: packet truncated\n");
		return NULL;
	}
	if (pkt[PKT_TYPE] == PKT_MSG) {
		length = pkt[PKT_LEN_LSB] | (pkt[PKT_LEN_MSB] << 8);
		if (length + PKT_HDR_LEN > raw_length - 2) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: invalid length\n");
			/*fprintf(stderr,"Sending NACK\n");
			   canon_serial_send_packet(PKT_NACK,camera->pl->seq_rx++,camera->pl->psa50_eot+PKT_HDR_LEN,0); */
			camera->pl->receive_error = ERROR_RECEIVED;
			return NULL;
		}
	}
	crc = pkt[raw_length - 2] | (pkt[raw_length - 1] << 8);
	if (!canon_psa50_chk_crc (pkt, raw_length - 2, crc)) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: CRC error\n");
		return NULL;
	}
	if (type)
		*type = pkt[PKT_TYPE];
	if (seq)
		*seq = pkt[PKT_SEQ];
	if (len)
		*len = length;
	if (*type == PKT_ACK || *type == PKT_EOT)
		return pkt;
	return pkt + PKT_HDR_LEN;
}

/* ----------------------- Message-level processing ------------------------ */


/**
 * Waits for an "ACK" from the camera.
 *
 * Return values:
 *  1 : ACK received
 *  0 : communication error (no reply received for example)
 * -1 : NACK received.
 */
int
canon_serial_wait_for_ack (Camera *camera)
{
	unsigned char *pkt;
	unsigned char type, seq, old_seq;
	int len;

	while (1) {
		pkt = canon_serial_recv_packet (camera, &type, &seq, &len);
		if (!pkt)
			return 0;
		if (seq == camera->pl->seq_tx && type == PKT_ACK) {
			if (pkt[2] == PKTACK_NACK) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: NACK received\n");
				return -1;
			}
			camera->pl->seq_tx++;
			return 1;
		}
		old_seq = '\0';
		if (type == PKT_EOT) {
			old_seq = pkt[0];
			if (camera->pl->receive_error == NOERROR) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "Old EOT received, sending corresponding ACK\n");
				if (!canon_serial_send_packet
				    (camera, PKT_ACK, old_seq,
				     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
					return 0;
				pkt = canon_serial_recv_packet (camera, &type, &seq, &len);
				if (!pkt)
					return 0;
				if (seq == old_seq && type == PKT_ACK) {
					if (pkt[2] == PKTACK_NACK) {
						gp_debug_printf (GP_DEBUG_LOW, "canon",
								 "Old EOT acknowledged\n");
						return -1;
					}
					return 1;
				}
			}
		}
		/* error already aknowledged, we skip the following ones */
		if (camera->pl->receive_error == ERROR_RECEIVED) {
			if (!canon_serial_send_packet
			    (camera, PKT_NACK, old_seq, camera->pl->psa50_eot + PKT_HDR_LEN,
			     0))
				return 0;
			return 1;
		}

		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "ERROR: ACK format or sequence error, retrying\n");
		gp_debug_printf (GP_DEBUG_LOW, "canon", "Sending NACK\n");
		canon_serial_send_packet (camera, PKT_NACK, camera->pl->seq_rx++,
					  camera->pl->psa50_eot + PKT_HDR_LEN, 0);
		camera->pl->receive_error = ERROR_RECEIVED;

/*
 * just keep on trying. protocol seems to retransmit EOTs, so we may get
 * some old EOTs when we're actually expecting ACKs.
 */
	}
}

/**
 * Sends a message to the camera.
 *
 * See the "Protocol" file for an explanation of the various
 * elements needed to create a message.
 *
 * Arguments:
 *  mtype : message type.
 *  dir   : direction.
 *  ap    : message payload (list of arguments, see 'man va_start'
 */
static int
canon_serial_send_msg (Camera *camera, unsigned char mtype, unsigned char dir, va_list * ap)
{
	unsigned char buffer[MAX_PKT_PAYLOAD + 2];	/* allow space for CRC */
	unsigned char upload_buffer[MAX_PKT_PAYLOAD + 2];
	unsigned char *pkt, *pkt2, *pos;
	int total, good_ack, try;

	memset (buffer, 0, PKT_HDR_LEN + MSG_HDR_LEN);

	pkt = buffer + PKT_HDR_LEN;
	pkt[MSG_02] = 2;
	pkt[MSG_MTYPE] = mtype;
	pkt[MSG_DIR] = dir;

	pos = pkt + MSG_HDR_LEN;
	total = 0;

	while (1) {
		const char *str;
		int len;

		str = va_arg (*ap, unsigned char *);

		if (!str)
			break;
		len = va_arg (*ap, int);

		if (pos + len - pkt > MAX_MSG_SIZE && camera->pl->uploading != 1) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "FATAL ERROR: message too big (%i)\n",
					 pos + len - pkt);
			return -1;
		}
		memcpy (pos, str, len);
		pos += len;
	}

	total = pos - pkt;

	pkt[MSG_LEN_LSB] = total & 0xff;
	pkt[MSG_LEN_MSB] = total >> 8;

	if (camera->pl->uploading == 1) {
		memset (upload_buffer, 0, PKT_HDR_LEN + MSG_HDR_LEN);
		pkt2 = upload_buffer;
		memcpy (pkt2, pkt + UPLOAD_DATA_BLOCK, total - UPLOAD_DATA_BLOCK);
		for (try = 0; try < MAX_TRIES; try++) {
			canon_serial_send_packet (camera, PKT_MSG, 0, pkt, UPLOAD_DATA_BLOCK);
			canon_serial_send_packet (camera, PKT_MSG, 0x1, pkt2,
						  total - UPLOAD_DATA_BLOCK);
			if (!canon_serial_send_packet
			    (camera, PKT_UPLOAD_EOT, camera->pl->seq_tx,
			     camera->pl->psa50_eot + PKT_HDR_LEN, 1))
				return 0;
			if (!canon_serial_send_packet
			    (camera, PKT_UPLOAD_EOT, camera->pl->seq_tx,
			     camera->pl->psa50_eot + PKT_HDR_LEN, 1))
				return 0;

			good_ack = canon_serial_wait_for_ack (camera);
			if (good_ack == 1)
				return good_ack;
		}
		return -1;
	} else {
		pkt[MSG_LEN_LSB] = total & 0xff;
		pkt[MSG_LEN_MSB] = total >> 8;
		for (try = 1; try < MAX_TRIES; try++) {
			if (!canon_serial_send_packet (camera, PKT_MSG, 0, pkt, total))
				return 0;
			if (!canon_serial_send_packet
			    (camera, PKT_EOT, camera->pl->seq_tx,
			     camera->pl->psa50_eot + PKT_HDR_LEN, 1))
				return 0;
			good_ack = canon_serial_wait_for_ack (camera);
			if (good_ack == -1) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "NACK received, retrying command\n");
			} else if (good_ack == 1) {
				return good_ack;
			} else {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "No ACK received, retrying command\n");
				if (try == 2) {
					/* is the camera still there? */
					if (!canon_serial_send_packet
					    (camera, PKT_EOT, camera->pl->seq_tx,
					     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
						return 0;
					good_ack = canon_serial_wait_for_ack (camera);
					if (good_ack == 0) {
						camera->pl->receive_error = FATAL_ERROR;
						gp_debug_printf (GP_DEBUG_LOW, "canon",
								 "ERROR: FATAL ERROR\n");
						clear_readiness (camera);
						return -1;
					}
				}
			}
		}
		return -1;
	}
}

/**
 * Receive a message from the camera.
 *
 * See the "Protocol" file for an explanation of the various
 * elements needed to handle a message.
 *
 * Arguments:
 *  mtype : message type.
 *  dir   : direction.
 *  total : payload length (set by this function).
 *
 * Returns:
 *  char* : pointer to the message payload.
 */
unsigned char *
canon_serial_recv_msg (Camera *camera, unsigned char mtype, unsigned char dir, int *total)
{
	static unsigned char *msg = NULL;
	static int msg_size = 512;	/* initial allocation/2 */
	unsigned char *frag;
	unsigned char type, seq;
	int len, length = 0, msg_pos = 0;

	while (1) {
		frag = canon_serial_recv_packet (camera, &type, NULL, &len);
		if (!frag)
			return NULL;
		if (type == PKT_MSG)
			break;
		/* uploading is special */
/*              if (type == PKT_ACK && mtype == 0x3 && dir == 0x21) break; */
		if (type == PKT_EOT) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "Old EOT received sending corresponding ACK\n");
			canon_serial_send_packet (camera, PKT_ACK, frag[0],
						  camera->pl->psa50_eot + PKT_HDR_LEN, 0);
		}
		gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: protocol error, retrying\n");
	}
	/* we keep the fragment only if there was no error */
	if (camera->pl->receive_error == NOERROR) {
		length = frag[MSG_LEN_LSB] | (frag[MSG_LEN_MSB] << 8);
		/* while uploading we expect 2 ACKs and a message 0x3 0x21
		 * not always in the same order */
/*
		if (type == PKT_ACK && mtype == 0x3 && dir == 0x21) {
			gp_debug_printf(GP_DEBUG_LOW,"canon","ignoring ACK received while waiting for MSG\n");
			return frag;
		} 
*/
		if (len < MSG_HDR_LEN || frag[MSG_02] != 2) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "ERROR: message format error\n");
			return NULL;
		}

		if (frag[MSG_MTYPE] != mtype || frag[MSG_DIR] != dir) {
			if (frag[MSG_MTYPE] == '\x01' && frag[MSG_DIR] == '\x00' &&
			    memcmp (frag + 12, "\x30\x00\x00\x30", 4)) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: Battery exhausted, camera off");
				gp_camera_status (camera, _("Battery exhausted, camera off."));
				camera->pl->receive_error = ERROR_LOWBATT;
			} else {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: unexpected message.\n");
				gp_camera_status (camera, _("ERROR: unexpected message"));
			}
			return NULL;
		}
		frag += MSG_HDR_LEN;
		len -= MSG_HDR_LEN;
	}
	while (1) {
		if (camera->pl->receive_error == NOERROR) {
			if (msg_pos + len > length) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: message overrun\n");
				gp_camera_status (camera, _("ERROR: message overrun"));
				return NULL;
			}
			if (msg_pos + len > msg_size || !msg) {
				msg_size *= 2;
				msg = realloc (msg, msg_size);
				if (!msg) {
					perror ("realloc");
					exit (1);
				}
			}
			memcpy (msg + msg_pos, frag, len);
			msg_pos += len;
		}
		frag = canon_serial_recv_packet (camera, &type, &seq, &len);
		if (!frag)
			return NULL;
		if (type == PKT_EOT) {
			/* in case of error we don't want to stop as the camera will send
			   the 1st packet of the sequence again */
			if (camera->pl->receive_error == ERROR_RECEIVED) {
				camera->pl->seq_rx = seq;
				canon_serial_send_packet (camera, PKT_NACK, camera->pl->seq_rx,
							  camera->pl->psa50_eot + PKT_HDR_LEN,
							  0);
				camera->pl->receive_error = ERROR_ADDRESSED;
			} else {
				if (seq == camera->pl->seq_rx)
					break;
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: out of sequence\n");
				gp_camera_status (camera, _("ERROR: out of sequence."));
				return NULL;
			}
		}
		if (type != PKT_MSG && camera->pl->receive_error == NOERROR) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "ERROR: unexpected packet type\n");
			gp_camera_status (camera, _("ERROR: unexpected packet type."));
			return NULL;
		}
		if (type == PKT_EOT && camera->pl->receive_error == ERROR_RECEIVED) {
			camera->pl->receive_error = ERROR_ADDRESSED;
		}
		if (type == PKT_MSG && camera->pl->receive_error == ERROR_ADDRESSED) {
			msg_pos = 0;
			length = frag[MSG_LEN_LSB] | (frag[MSG_LEN_MSB] << 8);
			if (len < MSG_HDR_LEN || frag[MSG_02] != 2) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: message format error\n");
				gp_camera_status (camera, _("ERROR: message format error."));
				return NULL;
			}

			if (frag[MSG_MTYPE] != mtype || frag[MSG_DIR] != dir) {
				if (frag[MSG_MTYPE] == '\x01' && frag[MSG_DIR] == '\x00' &&
				    memcmp (frag + 12, "\x30\x00\x00\x30", 4)) {
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "ERROR: Battery exhausted, camera off");
					gp_camera_status (camera,
							  _("Battery exhausted, camera off."));
					camera->pl->receive_error = ERROR_LOWBATT;
				} else {
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "ERROR: unexpected message2\n");
					gp_camera_status (camera,
							  _("ERROR: unexpected message2."));
				}
				return NULL;
			}
			frag += MSG_HDR_LEN;
			len -= MSG_HDR_LEN;
			camera->pl->receive_error = NOERROR;
		}
	}
	if (camera->pl->receive_error == ERROR_ADDRESSED) {
		camera->pl->receive_error = NOERROR;
	}
	if (camera->pl->receive_error == NOERROR) {
		/*we want to be sure the camera U N D E R S T A N D S our packets */
		if (camera->pl->uploading == 1 && camera->pl->md->model == CANON_PS_A50)
			camera->pl->slow_send = 1;
		if (!canon_serial_send_packet
		    (camera, PKT_ACK, camera->pl->seq_rx++,
		     camera->pl->psa50_eot + PKT_HDR_LEN, 0)) {
			if (camera->pl->uploading == 1
			    && camera->pl->md->model == CANON_PS_A50)
				camera->pl->slow_send = 0;
			return NULL;
		}
		if (camera->pl->uploading == 1 && camera->pl->md->model == CANON_PS_A50)
			camera->pl->slow_send = 0;
		if (total)
			*total = msg_pos;
		return msg;
	}

	return NULL;
}

/**
 * canon_serial_dialogue:
 *
 * @mtype : type
 * @dir   : direction
 * @len   : length of the received payload
 * @...   : The rest of the arguments will be put together to
 *          fill up the payload of the request message.
 *
 * Higher level function: sends a message and waits for a
 * reply from the camera.
 *
 * Payload: each argument after "len" goes by 2: the variable itself,
 * and the next argument has to be its length. You also have to finish
 * the list by a "NULL".
 *
 * Example: To send a string called "name" :
 * canon_serial_dialogue(0x05,0x12,&len,name,strlen(name)+1,NULL);
 *
 **/
unsigned char *
canon_serial_dialogue (Camera *camera, unsigned char mtype, unsigned char dir, int *len, ...)
{
	va_list ap;
	int okay, try;
	unsigned char *good_ack;

	for (try = 1; try < MAX_TRIES; try++) {
		va_start (ap, len);
		okay = canon_serial_send_msg (camera, mtype, dir, &ap);
		va_end (ap);
		if (!okay)
			return NULL;
		/* while uploading we receive 2 ACKs and 1 confirmation message
		 * The first ACK has already been received if we are here */
		if (camera->pl->uploading == 1) {
			camera->pl->seq_tx--;
			good_ack =
				canon_serial_recv_msg (camera, mtype, dir ^ DIR_REVERSE, len);
			if (!good_ack)
				return NULL;
			if (good_ack[0] == camera->pl->seq_tx && good_ack[1] == 0x5) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ACK received waiting for the confirmation message\n");
				good_ack =
					canon_serial_recv_msg (camera, mtype,
							       dir ^ DIR_REVERSE, len);
			} else {
				okay = canon_serial_wait_for_ack (camera);
				if (okay == 1)
					return good_ack;
			}
		} else
			good_ack =
				canon_serial_recv_msg (camera, mtype, dir ^ DIR_REVERSE, len);

		if (good_ack)
			return good_ack;
		if (camera->pl->receive_error == NOERROR) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "Resending message\n");
			camera->pl->seq_tx--;
		}
		if (camera->pl->receive_error == FATAL_ERROR)
			break;
	}
	return NULL;
}

/* ----------------------- Command-level processing ------------------------ */


/**
 * canon_serial_end:
 * @camera: the camera to switch off
 * @Returns: GP_OK
 *
 * Switches the #camera off
 */
int
canon_serial_end (Camera *camera)
{
	canon_serial_send (camera, "\xC0\x00\x02\x55\x2C\xC1", 6, USLEEP2);
	canon_serial_send (camera, "\xC0\x00\x04\x01\x00\x00\x00\x24\xC6\xC1", 8, USLEEP2);
	return GP_OK;
}

/**
 * canon_serial_off:
 * @camera: the camera to switch off
 * @Returns: GP_OK
 *
 * Switches the #camera off, and resets the serial driver to 9600 bauds,
 * in order to be ready to switch the camera back on again if wanted.
 * Should better be named psa50_serial_off
 */
int
canon_serial_off (Camera *camera)
{
	canon_serial_send (camera, "\xC0\x00\x02\x55\x2C\xC1", 6, USLEEP2);
	canon_serial_send (camera, "\xC0\x00\x04\x01\x00\x00\x00\x24\xC6\xC1", 8, USLEEP2);
	canon_serial_change_speed (camera->port, 9600);
	return GP_OK;
}



/*
 * print a message corresponding
 * to the error encountered
 */
void
canon_serial_error_type (Camera *camera)
{
	switch (camera->pl->receive_error) {
		case ERROR_LOWBATT:
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "ERROR: no battery left, Bailing out!\n");
			break;
		case FATAL_ERROR:
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "ERROR: camera connection lost!\n");
			break;
		default:
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: malformed message\n");
			break;
	}
}

/*
 * Upload to Camera via serial
 *
 */
int
canon_serial_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath, GPContext *context)
{
	unsigned char *msg;
	char filename[64];
	char buf[4096];
	int offset = 0;
	char offset2[4];
	int block_len;
	char block_len2[4];
	int sent = 0;
	int i, j = 0, len, hdr_len;
	long int size;
	const char *data, *name;
	unsigned int id;

	camera->pl->uploading = 1;
	gp_file_get_name (file, &name);
	for (i = 0; name[i]; i++)
		filename[i] = toupper (name[i]);
	filename[i] = '\0';

	hdr_len = HDR_FIXED_LEN + strlen (name) + strlen (destpath);

	gp_file_get_data_and_size (file, &data, &size);

	id = gp_context_progress_start (context, size, _("Uploading file..."));
	while (sent < size) {

		if (size < DATA_BLOCK)
			block_len = size;
		else if ((size - sent < DATA_BLOCK))
			block_len = size - sent;
		else
			block_len = DATA_BLOCK;

		offset = sent;

		for (i = 0; i < 4; i++) {
			offset2[i] = (offset >> (8 * i)) & 0xff;
			block_len2[i] = (block_len >> (8 * i)) & 0xff;
		}

		for (i = 0; i < DATA_BLOCK; i++) {
			buf[i] = data[j];
			j++;
		}

		msg = canon_serial_dialogue (camera, 0x3, 0x11, &len, "\x02\x00\x00\x00", 4,
					     offset2, 4, block_len2, 4,
					     destpath, strlen (destpath), destname,
					     strlen (destname) + 1, buf, block_len, NULL);
		if (!msg) {
			camera->pl->uploading = 0;
			return GP_ERROR;
		}
		sent += block_len;
		gp_context_progress_update (context, id, sent);
	}
	gp_context_progress_stop (context, id);
	camera->pl->uploading = 0;
	return GP_OK;
}

unsigned char *
canon_serial_get_file (Camera *camera, const char *name, int *length, GPContext *context)
{
	unsigned char *file = NULL;
	unsigned char *msg;
	unsigned char name_len;
	unsigned int total = 0, expect = 0, size, id;
	int len;

	if (camera->pl->receive_error == FATAL_ERROR) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "ERROR: can't continue a fatal error condition detected\n");
		return NULL;
	}
	name_len = strlen (name) + 1;
	msg = canon_serial_dialogue (camera, 0x1, 0x11, &len, "\x00\x00\x00\x00", 5,
				     &name_len, 1, "\x00", 2, name, strlen (name) + 1, NULL);
	if (!msg) {
		canon_serial_error_type (camera);
		return NULL;
	}
	id = gp_context_progress_start (context, le32atoh (msg + 4),
					_("Getting file..."));
	while (msg) {
		if (len < 20 || le32atoh (msg)) {
			break;
		}
		if (!file) {
			total = le32atoh (msg + 4);

			if (total > camera->pl->md->max_picture_size) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: %d is too big\n", total);
				break;
			}
			file = malloc (total);
			if (!file) {
				perror ("malloc");
				break;
			}
			if (length)
				*length = total;
		}
		size = le32atoh (msg + 12);
		if (le32atoh (msg + 8) != expect || expect + size > total || size > len - 20) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: doesn't fit\n");
			break;
		}
		memcpy (file + expect, msg + 20, size);
		expect += size;
		gp_context_progress_update (context, id, expect);
		if ((expect == total) != le32atoh (msg + 16)) {
			GP_DEBUG ("ERROR: end mark != end of data");
			break;
		}
		if (expect == total) {
			gp_context_progress_stop (context, id);
			return file;
		}
		msg = canon_serial_recv_msg (camera, 0x1, 0x21, &len);
	}
	free (file);
	return NULL;
}

int
canon_serial_get_dirents (Camera *camera, unsigned char **dirent_data,
			  unsigned int *dirents_length, const char *path,
			  GPContext *context)
{
	unsigned char *p, *temp_ch, *data = NULL;
	int mallocd_bytes, total_size;

	*dirent_data = NULL;

	/* fetch all directory entrys, the first one is a little special */
	p = canon_serial_dialogue (camera, 0xb, 0x11, dirents_length, "", 1, path,
				   strlen (path) + 1, "\x00", 2, NULL);
	if (p == NULL) {
		gp_context_error (context, "canon_serial_get_dirents: "
				     "canon_serial_dialogue failed to fetch directory entrys");
		return GP_ERROR;
	}

	/* In the RS232 implementation, we should never get less than 5 bytes */
	if (*dirents_length < 5) {
		gp_context_error (context, "canon_serial_get_dirents: "
				     "Initial dirent packet too short (only %i bytes)",
				     *dirents_length);
		return GP_ERROR;
	}

	/* don't use GP_DEBUG since we log this with GP_LOG_DATA */
	gp_log (GP_LOG_DATA, "canon", "canon_serial_get_dirents: "
		"dirent packet received from canon_serial_dialogue:");
	gp_log_data ("canon", p, *dirents_length);

	/* the first five bytes is only for the RS232 implementation
	 * of this command, we do not need to copy them so therefore
	 * we don't need to malloc() them either
	 */
	mallocd_bytes = MAX (1024, *dirents_length - 5);
	data = malloc (mallocd_bytes);
	if (!data) {
		gp_context_error (context, "canon_serial_get_dirents: "
				     "Could not allocate %i bytes of memory", mallocd_bytes);
		return GP_ERROR_NO_MEMORY;
	}

	/* the first five bytes is only for the RS232 implementation
	 * of this command, do not copy them
	 */
	memcpy (data, p + 5, (*dirents_length - 5));
	total_size = *dirents_length;

	/* p[4] indicates this is not the last packet,
	 * read additional packets until there are no more
	 * directory entrys to read
	 */
	while (!p[4]) {
		GP_DEBUG ("p[4] is %i", (int) p[4]);
		p = canon_serial_recv_msg (camera, 0xb, 0x21, dirents_length);
		if (p == NULL) {
			gp_context_error (context, "canon_serial_get_dirents: "
					     "Failed to read another directory entry");
			free (data);
			return GP_ERROR;
		}

		/* don't use GP_DEBUG since we log this with GP_LOG_DATA */
		gp_log (GP_LOG_DATA, "canon", "canon_serial_get_dirents: "
			"dirent packet received from canon_serial_recv_msg:");
		gp_log_data ("canon", p, *dirents_length);

		/* the first five bytes is only for the RS232 implementation,
		 * don't count them when checking dirent size
		 */
		if (*dirents_length - 5 < CANON_MINIMUM_DIRENT_SIZE) {
			gp_context_error (context, "canon_serial_get_dirents: "
					     "Truncated directory entry received");
			free (data);
			return GP_ERROR;
		}

		/* check if we need to allocate some more memory,
		 * the first five bytes is only for the RS232
		 * implementation of this command, don't need to
		 * malloc for them.
		 */
		if (total_size + (*dirents_length - 5) > mallocd_bytes) {
			/* we allocate 1024 bytes chunks instead
			 * of the exact number of bytes needed.
			 * this is OK since we will free this
			 * before returning from canon_int_list_directory
			 * (our caller).
			 */
			mallocd_bytes += MAX (1024, *dirents_length);

			/* check if we are reading unrealistic ammounts
			 * of directory entrys so that we don't loop
			 * forever. 1024 * 1024 is picked out of the blue.
			 */
			if (mallocd_bytes > 1024 * 1024) {
				gp_context_error (context, "canon_serial_get_dirents: "
						     "Too many dirents, we must be looping.");
				free (data);
				return GP_ERROR;
			}

			temp_ch = realloc (data, mallocd_bytes);
			if (!temp_ch) {
				gp_context_error (context,
						     "canon_serial_get_dirents: "
						     "Could not resize dirent buffer "
						     "to %i bytes", mallocd_bytes);
				free (data);
				return GP_ERROR;
			}
			data = temp_ch;
		}

		/* the first five bytes is only for the RS232
		 * implementation of this command, don't copy them.
		 */
		memcpy (data + total_size, p + 5, (*dirents_length - 5));
		total_size += (*dirents_length - 5);
	}
	GP_DEBUG ("OK - this was last dirent");

	*dirent_data = data;
	return GP_OK;
}

/**
 * canon_serial_ready:
 * @camera: camera to get ready
 * @Returns: gphoto2 error code
 *
 * serial part of canon_int_ready
 **/
int
canon_serial_ready (Camera *camera, GPContext *context)
{
	unsigned char type, seq;
	int good_ack, speed, try, len, i;
	char *pkt;
	int res;
	char cam_id_str[2000];
	unsigned int id;

	GP_DEBUG ("canon_int_ready()");

	serial_set_timeout (camera->port, 900);	/* 1 second is the delay for awakening the camera */
	serial_flush_input (camera->port);
	serial_flush_output (camera->port);

	camera->pl->receive_error = NOERROR;

	/* First of all, we must check if the camera is already on */
	/*      cts=canon_serial_get_cts();
	   gp_debug_printf(GP_DEBUG_LOW,"canon","cts : %i\n",cts);
	   if (cts==32) {  CTS == 32  when the camera is connected. */
	if (camera->pl->first_init == 0 && camera->pl->cached_ready == 1) {
		/* First case, the serial speed of the camera is the same as
		 * ours, so let's try to send a ping packet : */
		if (!canon_serial_send_packet
		    (camera, PKT_EOT, camera->pl->seq_tx,
		     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
			return GP_ERROR;
		good_ack = canon_serial_wait_for_ack (camera);
		gp_debug_printf (GP_DEBUG_LOW, "canon", "good_ack = %i\n", good_ack);
		if (good_ack == 0) {
			/* no answer from the camera, let's try
			 * at the speed saved in the settings... */
			speed = camera->pl->speed;
			if (speed != 9600) {
				if (!canon_serial_change_speed (camera->port, speed)) {
					gp_camera_status (camera, _("Error changing speed."));
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "speed changed.\n");
				}
			}
			if (!canon_serial_send_packet
			    (camera, PKT_EOT, camera->pl->seq_tx,
			     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
				return GP_ERROR;
			good_ack = canon_serial_wait_for_ack (camera);
			if (good_ack == 0) {
				gp_camera_status (camera, _("Resetting protocol..."));
				canon_serial_off (camera);
				sleep (3);	/* The camera takes a while to switch off */
				return canon_int_ready (camera, context);
			}
			if (good_ack == -1) {
				gp_debug_printf (GP_DEBUG_LOW, "canon", "Received a NACK !\n");
				return GP_ERROR;
			}
			gp_camera_status (camera, _("Camera OK.\n"));
			return 1;
		}
		if (good_ack == -1) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "Received a NACK !\n");
			return GP_ERROR;
		}
		gp_debug_printf (GP_DEBUG_LOW, "canon", "Camera replied to ping, proceed.\n");
		return GP_OK;
	}

	/* Camera was off... */

	gp_camera_status (camera, _("Looking for camera ..."));
	if (camera->pl->receive_error == FATAL_ERROR) {
		/* we try to recover from an error
		   we go back to 9600bps */
		if (!canon_serial_change_speed (camera->port, 9600)) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: Error changing speed");
			return GP_ERROR;
		}
		camera->pl->receive_error = NOERROR;
	}
	id = gp_context_progress_start (context, MAX_TRIES, _("Trying to "
		"contact camera..."));
	for (try = 0; try < MAX_TRIES; try++) {
		if (canon_serial_send
		    (camera, "\x55\x55\x55\x55\x55\x55\x55\x55", 8, USLEEP1) < 0) {
			gp_context_error (context, _("Communication error 1"));
			return GP_ERROR;
		}
		pkt = canon_serial_recv_frame (camera, &len);
		gp_context_progress_update (context, id, try + 1);
		if (pkt)
			break;
	}
	gp_context_progress_stop (context, id);
	if (try == MAX_TRIES) {
		gp_camera_status (camera, _("No response from camera"));
		return GP_ERROR;
	}
	if (!pkt) {
		gp_camera_status (camera, _("No response from camera"));
		return GP_ERROR;
	}
	if (len < 40 && strncmp (pkt + 26, "Canon", 5)) {
		gp_camera_status (camera, _("Unrecognized response"));
		return GP_ERROR;
	}
	strncpy (cam_id_str, pkt + 26, sizeof (cam_id_str) - 1);

	GP_DEBUG ("cam_id_str : '%s'", cam_id_str);

	camera->pl->first_init = 0;

	/* Compare what the camera identified itself as with our list of known models */
	for (i = 0; models[i].id_str != NULL; i++) {
		if (!strcmp (models[i].id_str, cam_id_str)) {
			GP_DEBUG ("canon_usb_identify: Serial ID string matches '%s'",
				  models[i].id_str);
			gp_camera_status (camera, "Detected a %s", models[i].id_str);
			camera->pl->md = (struct canonCamModelData *) &models[i];
			break;
		}
	}

	if (models[i].id_str == NULL) {
		gp_context_error (context, "Unknown model '%s'", cam_id_str);
		return GP_ERROR_MODEL_NOT_FOUND;
	}

	/* take care of some model specific things */
	switch (camera->pl->md->model) {
		case CANON_PS_A5:
		case CANON_PS_A5_ZOOM:
		case CANON_PS_A50:
			if (camera->pl->speed > 57600)
				camera->pl->slow_send = 1;
			break;
		default:
			break;
	}

	/* 5 seconds  delay should  be enough for   big flash cards.   By
	 * experience, one or two seconds is too  little, as a large flash
	 * card needs more access time. */
	serial_set_timeout (camera->port, 5000);
	(void) canon_serial_recv_packet (camera, &type, &seq, NULL);
	if (type != PKT_EOT || seq) {
		gp_camera_status (camera, _("Bad EOT"));
		return GP_ERROR;
	}
	camera->pl->seq_tx = 0;
	camera->pl->seq_rx = 1;
	if (!canon_serial_send_frame (camera, "\x00\x05\x00\x00\x00\x00\xdb\xd1", 8)) {
		gp_camera_status (camera, _("Communication error 2"));
		return GP_ERROR;
	}
	res = 0;
	switch (camera->pl->speed) {
		case 9600:
			res = canon_serial_send_frame (camera, SPEED_9600, 12);
			break;
		case 19200:
			res = canon_serial_send_frame (camera, SPEED_19200, 12);
			break;
		case 38400:
			res = canon_serial_send_frame (camera, SPEED_38400, 12);
			break;
		case 57600:
			res = canon_serial_send_frame (camera, SPEED_57600, 12);
			break;
		case 115200:
			res = canon_serial_send_frame (camera, SPEED_115200, 12);
			break;
	}

	if (!res || !canon_serial_send_frame (camera, "\x00\x04\x01\x00\x00\x00\x24\xc6", 8)) {
		gp_camera_status (camera, _("Communication error 3"));
		return GP_ERROR;
	}
	speed = camera->pl->speed;
	gp_camera_status (camera, _("Changing speed... wait..."));
	if (!canon_serial_wait_for_ack (camera))
		return GP_ERROR;
	if (speed != 9600) {
		if (!canon_serial_change_speed (camera->port, speed)) {
			gp_camera_status (camera, _("Error changing speed"));
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: Error changing speed");
		} else {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "speed changed\n");
		}

	}
	for (try = 1; try < MAX_TRIES; try++) {
		canon_serial_send_packet (camera, PKT_EOT, camera->pl->seq_tx,
					  camera->pl->psa50_eot + PKT_HDR_LEN, 0);
		if (!canon_serial_wait_for_ack (camera)) {
			gp_camera_status (camera,
					  _
					  ("Error waiting ACK during initialization retrying"));
		} else
			break;
	}

	if (try == MAX_TRIES) {
		gp_camera_status (camera, _("Error waiting ACK during initialization"));
		return GP_ERROR;
	}

	gp_camera_status (camera, _("Connected to camera"));
	/* Now is a good time to ask the camera for its owner
	 * name (and Model String as well)  */
	canon_int_identify_camera (camera, context);
	canon_int_get_time (camera, context);

	return GP_OK;
}

/**
 * canon_serial_get_thumbnail:
 *
 * @camera: camera to work on
 * @name: file name (complete canon path) of file to get thumbnail for
 * @data: pointer to data pointer
 * @length: pointer to data length
 * @Returns: GP_ERROR code
 *
 * This is just the serial specific part extracted from the older
 * canon_get_thumbnail() routine. 
 **/
int 
canon_serial_get_thumbnail (Camera *camera, const char *name, unsigned char **data, int *length, GPContext *context)
{
	unsigned int expect = 0, size, payload_length, total_file_size;
	unsigned int total = 0, id;
	unsigned char *msg;

	CON_CHECK_PARAM_NULL(length);
	CON_CHECK_PARAM_NULL(data);
	*length = 0;
	*data = NULL;

	if (camera->pl->receive_error == FATAL_ERROR) {
		gp_context_error (context, "ERROR: can't continue a fatal "
				     "error condition detected");
		return GP_ERROR;
	}

	payload_length = strlen (name) + 1;
	msg = canon_serial_dialogue (camera, 0x1, 0x11, &total_file_size,
				     "\x01\x00\x00\x00\x00", 5,
				     &payload_length, 1, "\x00", 2,
				     name, strlen (name) + 1, NULL);
	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	
	total = le32atoh (msg + 4);
	if (total > 2000000) {	/* 2 MB thumbnails ? unlikely ... */
		gp_context_error (context, "ERROR: %d is too big", total);
		return GP_ERROR;
	}
	*data = malloc (total);
	if (!*data) {
		perror ("malloc");
		return GP_ERROR;
	}
	*length = total;

	id = gp_context_progress_start (context, total, _("Getting thumbnail..."));
	while (msg) {
		if (total_file_size < 20 || le32atoh (msg)) {
			return GP_ERROR;
		}
		size = le32atoh (msg + 12);
		if (le32atoh (msg + 8) != expect || expect + size > total
		    || size > total_file_size - 20) {
			GP_DEBUG ("ERROR: doesn't fit");
			return GP_ERROR;
		}
		memcpy (*data + expect, msg + 20, size);
		expect += size;
		gp_context_progress_update (context, id, expect);
		if ((expect == total) != le32atoh (msg + 16)) {
			GP_DEBUG ("ERROR: end mark != end of data");
			return GP_ERROR;
		}
		if (expect == total) {
			/* We finished receiving the file. Parse the header and
			   return just the thumbnail */
			break;
		}
		msg = canon_serial_recv_msg (camera, 0x1, 0x21,
					     &total_file_size);
	}
	gp_context_progress_stop (context, id);
	return GP_OK;
}

/****************************************************************************
 *
 * End of file: serial.c
 *
 ****************************************************************************/

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
