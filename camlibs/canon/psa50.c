/*
 * psa50.c - Canon PowerShot "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
 * Additions 2000 by Philippe Marzouk and Edouard Lafargue
 * USB support, 2000, by Mikael Nyström
 *
 * $Header$
 */

/**
 * This file now includes both USB and serial support. This is
 * experimental and not guaranteed to work 100%. Maybe a better
 * design would be to make serial and USB support completely separate,
 * but at the same time, both protocols/busses work almost the same
 * way.
 */

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

#include <gphoto2.h>
#include <gphoto2-port-log.h>

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
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "../../libgphoto2/exif.h"

#include "serial.h"
#include "util.h"
#include "crc.h"
#include "psa50.h"
#include "canon.h"

/* ========================================================================
 * function prototype declarations
 * move these to psa50.h to make them publicly available
 * ========================================================================
 */

int psa50_send_frame (Camera *camera, const unsigned char *pkt, int len);
unsigned char *psa50_recv_frame (Camera *camera, int *len);
void intatpos (unsigned char *block, int pos, int integer);
unsigned char *psa50_get_file_serial (Camera *camera, const char *name, int *length);
int psa50_put_file_usb (Camera *camera, CameraFile * file, char *destname, char *destpath);
int psa50_put_file_serial (Camera *camera, CameraFile * file, char *destname, char *destpath);


#define MAX_TRIES 10

#define USLEEP1 0
#define USLEEP2 1

#define USB_BULK_READ_SIZE 0x3000

/* ------------------------- Frame-level processing ------------------------- */


#define CANON_FBEG      0xc0
#define CANON_FEND      0xc1
#define CANON_ESC       0x7e
#define CANON_XOR       0x20

int
psa50_send_frame (Camera *camera, const unsigned char *pkt, int len)
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
psa50_recv_frame (Camera *camera, int *len)
{
	static unsigned char buffer[5000];

	/* more than enough :-) (allow for a few run-together packets) */
	unsigned char *p = buffer;
	int c;

	while ((c = canon_serial_get_byte (camera->port)) != CANON_FBEG)
		if (c == -1)
			return NULL;
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
	if (camera->pl->dump_packets == 1)
		dump_hex (camera, "RECV", buffer, p - buffer);
	if (len)
		*len = p - buffer;
	return buffer;
}


/* ------------------------ Packet-level processing ------------------------- */


#define MAX_PKT_PAYLOAD 65535
#define MAX_PKT_SIZE    (MAX_PKT_PAYLOAD+4)

#define PKT_HDR_LEN     4

#define PKT_SEQ         0
#define PKT_TYPE        1
#define PKT_LEN_LSB     2
#define PKT_LEN_MSB     3

#define PKT_MSG         0
#define PKT_EOT         4
#define PKT_ACK         5
#define PKT_NACK        255
#define PKTACK_NACK     0x01
#define PKT_UPLOAD_EOT  3


static unsigned char seq_tx = 1;
static unsigned char seq_rx = 1;


static int
psa50_send_packet (Camera *camera, unsigned char type,
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

	return psa50_send_frame (camera, hdr, len + PKT_HDR_LEN + 2);
}


static unsigned char *
psa50_recv_packet (Camera *camera, unsigned char *type, unsigned char *seq, int *len)
{
	unsigned char *pkt;
	unsigned short crc;
	int raw_length, length = 0;

	pkt = psa50_recv_frame (camera, &raw_length);
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
			   psa50_send_packet(PKT_NACK,seq_rx++,camera->pl->psa50_eot+PKT_HDR_LEN,0); */
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


#define MAX_MSG_SIZE    (MAX_PKT_PAYLOAD-12)

#define FRAG_NUM                0
#define FRAG_LEN_LSB    2
#define FRAG_LEN_MSB    3

#define MSG_HDR_LEN     16
#define MSG_02          0
#define MSG_MTYPE       4
#define MSG_DIR         7
#define MSG_LEN_LSB     8
#define MSG_LEN_MSB     9

// #define MSG_FFFB     12



#define DIR_REVERSE     0x30

#define UPLOAD_DATA_BLOCK 900


/**
 * Waits for an "ACK" from the camera.
 *
 * Return values:
 *  1 : ACK received
 *  0 : communication error (no reply received for example)
 * -1 : NACK received.
 */
static int
psa50_wait_for_ack (Camera *camera)
{
	unsigned char *pkt;
	unsigned char type, seq, old_seq;
	int len;

	while (1) {
		pkt = psa50_recv_packet (camera, &type, &seq, &len);
		if (!pkt)
			return 0;
		if (seq == seq_tx && type == PKT_ACK) {
			if (pkt[2] == PKTACK_NACK) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: NACK received\n");
				return -1;
			}
			seq_tx++;
			return 1;
		}
		old_seq = '\0';
		if (type == PKT_EOT) {
			old_seq = pkt[0];
			if (camera->pl->receive_error == NOERROR) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "Old EOT received, sending corresponding ACK\n");
				if (!psa50_send_packet
				    (camera, PKT_ACK, old_seq,
				     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
					return 0;
				pkt = psa50_recv_packet (camera, &type, &seq, &len);
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
			if (!psa50_send_packet
			    (camera, PKT_NACK, old_seq, camera->pl->psa50_eot + PKT_HDR_LEN,
			     0))
				return 0;
			return 1;
		}

		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "ERROR: ACK format or sequence error, retrying\n");
		gp_debug_printf (GP_DEBUG_LOW, "canon", "Sending NACK\n");
		psa50_send_packet (camera, PKT_NACK, seq_rx++,
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
psa50_send_msg (Camera *camera, unsigned char mtype, unsigned char dir, va_list * ap)
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
			psa50_send_packet (camera, PKT_MSG, 0, pkt, UPLOAD_DATA_BLOCK);
			psa50_send_packet (camera, PKT_MSG, 0x1, pkt2,
					   total - UPLOAD_DATA_BLOCK);
			if (!psa50_send_packet
			    (camera, PKT_UPLOAD_EOT, seq_tx,
			     camera->pl->psa50_eot + PKT_HDR_LEN, 1))
				return 0;
			if (!psa50_send_packet
			    (camera, PKT_UPLOAD_EOT, seq_tx,
			     camera->pl->psa50_eot + PKT_HDR_LEN, 1))
				return 0;

			good_ack = psa50_wait_for_ack (camera);
			if (good_ack == 1)
				return good_ack;
		}
		return -1;
	} else {
		pkt[MSG_LEN_LSB] = total & 0xff;
		pkt[MSG_LEN_MSB] = total >> 8;
		for (try = 1; try < MAX_TRIES; try++) {
			if (!psa50_send_packet (camera, PKT_MSG, 0, pkt, total))
				return 0;
			if (!psa50_send_packet
			    (camera, PKT_EOT, seq_tx, camera->pl->psa50_eot + PKT_HDR_LEN, 1))
				return 0;
			good_ack = psa50_wait_for_ack (camera);
			if (good_ack == -1) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "NACK received, retrying command\n");
			} else if (good_ack == 1) {
				return good_ack;
			} else {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "No ACK received, retrying command\n");
				if (try == 2) {
					//is the camera still there?
					if (!psa50_send_packet
					    (camera, PKT_EOT, seq_tx,
					     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
						return 0;
					good_ack = psa50_wait_for_ack (camera);
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
static unsigned char *
psa50_recv_msg (Camera *camera, unsigned char mtype, unsigned char dir, int *total)
{
	static unsigned char *msg = NULL;
	static int msg_size = 512;	/* initial allocation/2 */
	unsigned char *frag;
	unsigned char type, seq;
	int len, length = 0, msg_pos = 0;

	while (1) {
		frag = psa50_recv_packet (camera, &type, NULL, &len);
		if (!frag)
			return NULL;
		if (type == PKT_MSG)
			break;
		//uploading is special
//              if (type == PKT_ACK && mtype == 0x3 && dir == 0x21) break;
		if (type == PKT_EOT) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "Old EOT received sending corresponding ACK\n");
			psa50_send_packet (camera, PKT_ACK, frag[0],
					   camera->pl->psa50_eot + PKT_HDR_LEN, 0);
		}
		gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: protocol error, retrying\n");
	}
	/* we keep the fragment only if there was no error */
	if (camera->pl->receive_error == NOERROR) {
		length = frag[MSG_LEN_LSB] | (frag[MSG_LEN_MSB] << 8);
		/* while uploading we expect 2 ACKs and a message 0x3 0x21
		 * not always in the same order */
//              if (type == PKT_ACK && mtype == 0x3 && dir == 0x21) {
//                      gp_debug_printf(GP_DEBUG_LOW,"canon","ignoring ACK received while waiting for MSG\n");
//                      return frag;
//              }
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
		frag = psa50_recv_packet (camera, &type, &seq, &len);
		if (!frag)
			return NULL;
		if (type == PKT_EOT) {
			/* in case of error we don't want to stop as the camera will send
			   the 1st packet of the sequence again */
			if (camera->pl->receive_error == ERROR_RECEIVED) {
				seq_rx = seq;
				psa50_send_packet (camera, PKT_NACK, seq_rx,
						   camera->pl->psa50_eot + PKT_HDR_LEN, 0);
				camera->pl->receive_error = ERROR_ADDRESSED;
			} else {
				if (seq == seq_rx)
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
		if (camera->pl->uploading == 1 && camera->pl->model == CANON_PS_A50)
			camera->pl->slow_send = 1;
		if (!psa50_send_packet
		    (camera, PKT_ACK, seq_rx++, camera->pl->psa50_eot + PKT_HDR_LEN, 0)) {
			if (camera->pl->uploading == 1 && camera->pl->model == CANON_PS_A50)
				camera->pl->slow_send = 0;
			return NULL;
		}
		if (camera->pl->uploading == 1 && camera->pl->model == CANON_PS_A50)
			camera->pl->slow_send = 0;
		if (total)
			*total = msg_pos;
		return msg;
	}

	return NULL;
}

/**
 * Higher level function: sends a message and waits for a
 * reply from the camera.
 *
 * Arguments:
 *   mtype : type
 *   dir   : direction
 *   len   : length of the received payload
 *   ...   : The rest of the arguments will be put together to
 *           fill up the payload of the request message.
 *
 *
 *  Payload: each argument after "len" goes by 2: the variable itself,
 * and the next argument has to be its length. You also have to finish
 * the list by a "NULL".
 *
 * Example: To send a string called "name" :
 * psa50_serial_dialogue(0x05,0x12,&len,name,strlen(name)+1,NULL);
 *
 */
static unsigned char *
psa50_serial_dialogue (Camera *camera, unsigned char mtype, unsigned char dir, int *len, ...)
{
	va_list ap;
	int okay, try;
	unsigned char *good_ack;

	for (try = 1; try < MAX_TRIES; try++) {
		va_start (ap, len);
		okay = psa50_send_msg (camera, mtype, dir, &ap);
		va_end (ap);
		if (!okay)
			return NULL;
		/* while uploading we receive 2 ACKs and 1 confirmation message
		 * The first ACK has already been received if we are here */
		if (camera->pl->uploading == 1) {
			seq_tx--;
			good_ack = psa50_recv_msg (camera, mtype, dir ^ DIR_REVERSE, len);
			if (!good_ack)
				return NULL;
			if (good_ack[0] == seq_tx && good_ack[1] == 0x5) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ACK received waiting for the confirmation message\n");
				good_ack =
					psa50_recv_msg (camera, mtype, dir ^ DIR_REVERSE, len);
			} else {
				okay = psa50_wait_for_ack (camera);
				if (okay == 1)
					return good_ack;
			}
		} else
			good_ack = psa50_recv_msg (camera, mtype, dir ^ DIR_REVERSE, len);

		if (good_ack)
			return good_ack;
		if (camera->pl->receive_error == NOERROR) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "Resending message\n");
			seq_tx--;
		}
		if (camera->pl->receive_error == FATAL_ERROR)
			break;
	}
	return NULL;
}


/**
 * Utility function used by psa50_usb_dialogue
 */
void
intatpos (unsigned char *block, int pos, int integer)
{
	*(unsigned *) (block + pos) = integer;
}

/**
 * USB version of the dialogue function.
 *
 * We construct a packet with the known command values (cmd{1,2,3}) of
 * the function requested (canon_funct) to the camera. If return_length
 * exists for this function, we read return_length bytes back from the
 * camera an return this camera response to the caller.
 *
 * Example :
 *
 *	This function gets called with
 *		canon_funct = CANON_USB_FUNCTION_SET_TIME
 *		payload = allready constructed payload with the new time
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
 */
static unsigned char *
psa50_usb_dialogue (Camera *camera, int canon_funct, int *return_length,
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
	 * data in this buffer is a result of this particular psa50_usb_dialogue() call
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
		gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_usb_dialogue() "
				 "called for ILLEGAL function %i! Aborting.", canon_funct);
		return NULL;
	}
	gp_debug_printf (GP_DEBUG_LOW, "\ncanon",
			 "psa50_usb_dialogue() cmd 0x%x 0x%x 0x%x (%s), payload = %i bytes",
			 cmd1, cmd2, cmd3, funct_descr, payload_length);

	if (read_bytes > sizeof (buffer)) {
		/* If this message is ever printed, chances are that you just added
		 * a new command to canon_usb_cmd with a return_length greater than
		 * all the others and did not update the declaration of 'buffer' in
		 * this function.
		 */
		gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_usb_dialogue() "
				 "read_bytes %i won't fit in buffer of size %i!",
				 read_bytes, sizeof (buffer));
		return NULL;
	}

	if (payload_length) {
		gp_debug_printf (GP_DEBUG_HIGH, "canon", "Payload");
		gp_log_data ("canon", payload, payload_length);
	}

	if ((payload_length + 0x50) > sizeof (packet)) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_usb_dialogue: payload too big, won't fit into buffer (%i > %i)",
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
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_usb_dialogue: write failed! (returned %i)\n", status);
		return NULL;
	}

	/* and, if this canon_funct is known to generate a response from the camera,
	 * read this response back.
	 */
	status = gp_port_read (camera->port, buffer, read_bytes);
	if (status != read_bytes) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_usb_dialogue: read failed! "
				 "(returned %i, expected %i)\n", status, read_bytes);
		return NULL;
	}

	if (return_length)
		*return_length = read_bytes;

	/* if cmd3 equals to 0x202, this is a command that returns L (long) data
	 * and what we return here is the complete packet (ie. not skipping the
	 * first 0x50 bytes we otherwise would) so that the caller
	 * (which is psa50_usb_long_dialogue()) can find out how much data to
	 * read from the USB port by looking at offset 6 in this packet.
	 */
	if (cmd3 == 0x202)
		return buffer;
	else
		return buffer + 0x50;
}

/**
 *
 * This function is used to invoke camera commands which return L (long) data.
 * It calls psa50_usb_dialogue(), if it gets a good response it will malloc()
 * memory and read the entire returned data into this malloc'd memory and store
 * a pointer to the malloc'd memory in 'data'.
 *
 */
static int
psa50_usb_long_dialogue (Camera *camera, int canon_funct, unsigned char **data,
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

	gp_debug_printf (GP_DEBUG_LOW, "canon",
			 "psa50_usb_long_dialogue() function %i, payload = %i bytes",
			 canon_funct, payload_length);

	/* Call psa50_usb_dialogue(), this will not return any data "the usual way"
	 * but after this we read 0x40 bytes from the USB port, the int at pos 6 in
	 * the returned data holds the total number of bytes we are to read.
	 *
	 */
	lpacket =
		psa50_usb_dialogue (camera, canon_funct, &bytes_read, payload, payload_length);
	if (lpacket == NULL) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_usb_long_dialogue: psa50_usb_dialogue "
				 "returned error!");
		return GP_ERROR;
	}
	/* This check should not be needed since we check the return of psa50_usb_dialogue()
	 * above, but as the saying goes: better safe than sorry.
	 */
	if (bytes_read != 0x40) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_usb_long_dialogue: psa50_usb_dialogue "
				 "did not return (%i bytes) the number of bytes "
				 "we expected (%i)!. Aborting.", bytes_read, 0x40);
		return GP_ERROR;
	}

	total_data_size = *(unsigned *) (lpacket + 0x6);

	if (max_data_size && (total_data_size > max_data_size)) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_usb_long_dialogue: "
				 "ERROR: Packet of size %i is too big "
				 "(max reasonable size specified is %i)",
				 total_data_size, max_data_size);
		return GP_ERROR;
	}
	*data = malloc (total_data_size);
	if (!*data) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_usb_long_dialogue: "
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


/* ----------------------- Command-level processing ------------------------ */



#define JPEG_ESC        0xFF
#define JPEG_BEG        0xD8
#define JPEG_SOS        0xDB
#define JPEG_A50_SOS    0xC4
#define JPEG_END        0xD9

#define SPEED_9600   "\x00\x03\x02\x02\x01\x10\x00\x00\x00\x00\xc0\x39"
#define SPEED_19200  "\x00\x03\x08\x02\x01\x10\x00\x00\x00\x00\x13\x1f"
#define SPEED_38400  "\x00\x03\x20\x02\x01\x10\x00\x00\x00\x00\x5f\x84"
#define SPEED_57600  "\x00\x03\x40\x02\x01\x10\x00\x00\x00\x00\x5e\x57"
#define SPEED_115200 "\x00\x03\x80\x02\x01\x10\x00\x00\x00\x00\x4d\xf9"


static unsigned int
get_int (const unsigned char *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}


/**
 * Switches the camera off
 */
int
psa50_end (Camera *camera)
{
	canon_serial_send (camera, "\xC0\x00\x02\x55\x2C\xC1", 6, USLEEP2);
	canon_serial_send (camera, "\xC0\x00\x04\x01\x00\x00\x00\x24\xC6\xC1", 8, USLEEP2);
	return 0;
}

/**
 * Switches the camera off, and resets the serial driver to 9600 bauds,
 * in order to be ready to switch the camera back on again if wanted.
 */
int
psa50_off (Camera *camera)
{
	canon_serial_send (camera, "\xC0\x00\x02\x55\x2C\xC1", 6, USLEEP2);
	canon_serial_send (camera, "\xC0\x00\x04\x01\x00\x00\x00\x24\xC6\xC1", 8, USLEEP2);
	canon_serial_change_speed (camera->port, 9600);
	return 0;
}

/**
 * does operations on a directory based on the value
 * of action : DIR_CREATE, DIR_REMOVE
 *
 */
int
psa50_directory_operations (Camera *camera, const char *path, int action)
{
	unsigned char *msg;
	int len, canon_usb_funct;
	char type;

	switch (action) {
		case DIR_CREATE:
			type = 0x5;
			canon_usb_funct = CANON_USB_FUNCTION_MKDIR;
			break;
		case DIR_REMOVE:
			type = 0x6;
			canon_usb_funct = CANON_USB_FUNCTION_RMDIR;
			break;
		default:
			gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_directory_operations: "
					 "Bad operation specified : %i\n", action);
			return GP_ERROR_BAD_PARAMETERS;
			break;
	}

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_directory_operations() "
			 "called to %s the directory '%s'",
			 canon_usb_funct == CANON_USB_FUNCTION_MKDIR ? "create" : "remove",
			 path);
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			msg = psa50_usb_dialogue (camera, canon_usb_funct, &len, path,
						  strlen (path) + 1);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, type, 0x11, &len, path,
						     strlen (path) + 1, NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return GP_ERROR;
	}

	if (msg[0] != 0x00) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "Could not %s directory %s",
				 canon_usb_funct ==
				 CANON_USB_FUNCTION_MKDIR ? "create" : "remove", path);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * Gets the camera identification string, usually the owner name.
 * This information is stored in the "camera" structure, which
 * is a global variable for the driver.
 *  This function also gets the firmware revision in the camera struct.
 *
 */
int
psa50_get_owner_name (Camera *camera)
{
	unsigned char *msg;
	int len;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_owner_name() called");

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x4c;
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_IDENTIFY_CAMERA,
						  &len, NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0x01, 0x12, &len, NULL);
			break;

	}

	if (!msg) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_owner_name: " "msg error");
		psa50_error_type (camera);
		return GP_ERROR;
	}

	/* Store these values in our "camera" structure: */
	memcpy (camera->pl->firmwrev, (char *) msg + 8, 4);
	strncpy (camera->pl->ident, (char *) msg + 12, 30);
	strncpy (camera->pl->owner, (char *) msg + 44, 30);
	gp_debug_printf (GP_DEBUG_HIGH, "canon", "psa50_get_owner_name: "
			 "ident '%s' owner '%s'", camera->pl->ident, camera->pl->owner);

	return GP_OK;
}

/**
 * Get battery status.
 */
int
psa50_get_battery (Camera *camera, int *pwr_status, int *pwr_source)
{
	unsigned char *msg;
	int len;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x8;
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_POWER_STATUS,
						  &len, NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0x0a, 0x12, &len, NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return -1;
	}

	if (pwr_status)
		*pwr_status = msg[4];
	if (pwr_source)
		*pwr_source = msg[7];
	gp_debug_printf (GP_DEBUG_LOW, "canon", "Status: %i / Source: %i\n", *pwr_status,
			 *pwr_source);
	return 0;
}


/**
 * Sets a file's attributes. See the 'Protocol' file for details.
 */
int
psa50_set_file_attributes (Camera *camera, const char *file, const char *dir,
			   unsigned char attrs)
{
	unsigned char payload[300];
	unsigned char *msg;
	unsigned char attr[4];
	int len, payload_length;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_set_file_attributes() "
			 "called for '%s'/'%s', attributes 0x%x", attrs);

	attr[0] = attr[1] = attr[2] = 0;
	attr[3] = attrs;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			if ((4 + strlen (dir) + 1 + strlen (file) + 1) > sizeof (payload)) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "psa50_set_file_attributes: "
						 "dir '%s' + file '%s' too long, "
						 "won't fit in payload buffer.", dir, file);
				return GP_ERROR_BAD_PARAMETERS;
			}
			/* create payload (yes, path and filename are two different strings
			 * and not meant to be concatenated)
			 */
			memset (payload, 0, sizeof (payload));
			memcpy (payload, attr, 4);
			memcpy (payload + 4, dir, strlen (dir) + 1);
			memcpy (payload + 4 + strlen (dir) + 1, file, strlen (file) + 1);
			payload_length = 4 + strlen (dir) + 1 + strlen (file) + 1;
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_SET_ATTR, &len,
						  payload, payload_length);
			if (len == 4) {
				/* XXX check camera return value (not psa50_usb_dialogue return value
				 * but the bytes in the packet returned)
				 */
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "psa50_set_file_attributes: "
						 "returned four bytes as expected, "
						 "we should check if they indicate "
						 "error or not. Returned data :");
				gp_log_data ("canon", msg, 4);
			} else {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "psa50_set_file_attributes: "
						 "setting attribute failed!");
				return GP_ERROR;
			}

			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0xe, 0x11, &len, attr, 4, dir,
						     strlen (dir) + 1, file, strlen (file) + 1,
						     NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 *  Sets   the   camera owner name. The string should
 * not be more than 30 characters long. We call get_owner_name
 * afterwards in order to check that everything went fine.
 *
 */
int
psa50_set_owner_name (Camera *camera, const char *name)
{
	unsigned char *msg;
	int len;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_set_owner_name() "
			 "called, name = '%s'", name);
	if (strlen (name) > 30) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_set_owner_name: Name too long (%i chars), "
				 "max is 30 characters!", strlen (name));
		gp_camera_status (camera, _("Name too long, max is 30 characters!"));
		return 0;
	}

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_CAMERA_CHOWN,
						  &len, name, strlen (name) + 1);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0x05, 0x12, &len, name,
						     strlen (name) + 1, NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return GP_ERROR;
	}
	return psa50_get_owner_name (camera);
}


/**
 * Get camera's current time.
 *
 * The camera gives time in little endian format, therefore we need
 * to swap the 4 bytes on big-endian machines.
 *
 * Nota: the time returned is not GMT but local time. Therefore,
 * if you use functions like "ctime", it will be translated to local
 * time _a second time_, and the result will be wrong. Only use functions
 * that don't translate the date into localtime, like "gmtime".
 */
time_t
psa50_get_time (Camera *camera)
{
	unsigned char *msg;
	int len;
	int t;
	time_t date;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x10;
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_GET_TIME, &len,
						  NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0x03, 0x12, &len, NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return GP_ERROR;
	}

	/* XXX will fail when sizeof(int) != 4. Should use u_int32_t or
	 * something instead. Investigate portability issues.
	 */
	memcpy (&t, msg + 4, 4);

	date = (time_t) byteswap32 (t);

	/* XXX should strip \n at the end of asctime() return data */
	gp_debug_printf (GP_DEBUG_HIGH, "canon", "Camera time: %s ", asctime (gmtime (&date)));
	return date;
}


int
psa50_set_time (Camera *camera)
{
	unsigned char *msg;
	int len, i;
	time_t date;
	char pcdate[4];

	date = time (NULL);
	for (i = 0; i < 4; i++)
		pcdate[i] = (date >> (8 * i)) & 0xff;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			len = 0x10;
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_SET_TIME, &len,
						  NULL, 0);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0x04, 0x12, &len, pcdate,
						     sizeof (pcdate),
						     "\x00\x00\x00\x00\x00\x00\x00\x00", 8,
						     NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return 0;
	}

	return 1;
}

/**
 * Switches the camera on, detects the model and sets its speed.
 */
int
psa50_ready (Camera *camera)
{
	unsigned char type, seq;
	char *pkt;
	int try, len, speed, good_ack, res;

	//    int cts;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_ready()");

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			res = psa50_get_owner_name (camera);
			if (res != GP_OK) {
				gp_camera_status (camera, "Camera not ready "
						  "('get owner name' request failed (%d))\n",
						  res);
				return GP_ERROR;
			}
			res = psa50_get_time (camera);
			if (res == GP_ERROR) {
				gp_camera_status (camera, "Camera not ready "
						  "('get time' request failed (%d))\n", res);
				return GP_ERROR;
			}
			if (!strcmp ("Canon PowerShot S20", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a Powershot S20");
				camera->pl->model = CANON_PS_S20;
				return GP_OK;
			} else if (!strcmp ("Canon PowerShot S10", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a Powershot S10");
				camera->pl->model = CANON_PS_S10;
				return GP_OK;
			} else if (!strcmp ("Canon PowerShot G1", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a Powershot G1");
				camera->pl->model = CANON_PS_G1;
				return GP_OK;
			} else if (!strcmp ("Canon PowerShot G2", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a Powershot G2");
				camera->pl->model = CANON_PS_G2;
				return GP_OK;
			} else if ((!strcmp ("Canon DIGITAL IXUS", camera->pl->ident))
				   || (!strcmp ("Canon IXY DIGITAL", camera->pl->ident))
				   || (!strcmp ("Canon PowerShot S110", camera->pl->ident))
				   || (!strcmp ("Canon PowerShot S100", camera->pl->ident))
				   || (!strcmp ("Canon DIGITAL IXUS v", camera->pl->ident))) {
				gp_camera_status (camera,
						  "Detected a Digital IXUS series / IXY DIGITAL / Powershot S100 series");
				camera->pl->model = CANON_PS_S100;
				return GP_OK;
			} else if ((!strcmp ("Canon DIGITAL IXUS 300", camera->pl->ident))
				   || (!strcmp ("Canon IXY DIGITAL 300", camera->pl->ident))
				   || (!strcmp ("Canon PowerShot S300", camera->pl->ident))) {
				gp_camera_status (camera,
						  "Detected a Digital IXUS 300 / IXY DIGITAL 300 / Powershot S300");
				camera->pl->model = CANON_PS_S300;
				return GP_OK;
			} else if (!strcmp ("Canon PowerShot A10", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a Powershot A10");
				camera->pl->model = CANON_PS_A10;
				return GP_OK;
			} else if (!strcmp ("Canon PowerShot A20", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a Powershot A20");
				camera->pl->model = CANON_PS_A20;
				return GP_OK;
			} else if (!strcmp ("Canon EOS D30", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a EOS D30");
				camera->pl->model = CANON_EOS_D30;
				return GP_OK;
			} else if (!strcmp ("Canon PowerShot Pro90 IS", camera->pl->ident)) {
				gp_camera_status (camera, "Detected a PowerShot Pro90 IS");
				camera->pl->model = CANON_PS_PRO90_IS;
				return GP_OK;
			} else {
				gp_debug_printf (GP_DEBUG_NONE, "Unknown camera! (%s)\n",
						 camera->pl->ident);
				return GP_ERROR;
			}
			break;
		case CANON_SERIAL_RS232:
		default:

			serial_set_timeout (camera->port, 900);	// 1 second is the delay for awakening the camera
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
				if (!psa50_send_packet
				    (camera, PKT_EOT, seq_tx,
				     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
					return GP_ERROR;
				good_ack = psa50_wait_for_ack (camera);
				gp_debug_printf (GP_DEBUG_LOW, "canon", "good_ack = %i\n",
						 good_ack);
				if (good_ack == 0) {
					/* no answer from the camera, let's try
					 * at the speed saved in the settings... */
					speed = camera->pl->speed;
					if (speed != 9600) {
						if (!canon_serial_change_speed
						    (camera->port, speed)) {
							gp_camera_status (camera,
									  _
									  ("Error changing speed."));
							gp_debug_printf (GP_DEBUG_LOW, "canon",
									 "speed changed.\n");
						}
					}
					if (!psa50_send_packet
					    (camera, PKT_EOT, seq_tx,
					     camera->pl->psa50_eot + PKT_HDR_LEN, 0))
						return GP_ERROR;
					good_ack = psa50_wait_for_ack (camera);
					if (good_ack == 0) {
						gp_camera_status (camera,
								  _("Resetting protocol..."));
						psa50_off (camera);
						sleep (3);	/* The camera takes a while to switch off */
						return psa50_ready (camera);
					}
					if (good_ack == -1) {
						gp_debug_printf (GP_DEBUG_LOW, "canon",
								 "Received a NACK !\n");
						return GP_ERROR;
					}
					gp_camera_status (camera, _("Camera OK.\n"));
					return 1;
				}
				if (good_ack == -1) {
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "Received a NACK !\n");
					return GP_ERROR;
				}
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "Camera replied to ping, proceed.\n");
				return GP_OK;
			}

			/* Camera was off... */

			gp_camera_status (camera, _("Looking for camera ..."));
			gp_camera_progress (camera, 0);
			if (camera->pl->receive_error == FATAL_ERROR) {
				/* we try to recover from an error
				   we go back to 9600bps */
				if (!canon_serial_change_speed (camera->port, 9600)) {
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "ERROR: Error changing speed");
					return GP_ERROR;
				}
				camera->pl->receive_error = NOERROR;
			}
			for (try = 1; try < MAX_TRIES; try++) {
				gp_camera_progress (camera, (try / (float) MAX_TRIES));
				if (canon_serial_send
				    (camera, "\x55\x55\x55\x55\x55\x55\x55\x55", 8,
				     USLEEP1) < 0) {
					gp_camera_status (camera, _("Communication error 1"));
					return GP_ERROR;
				}
				pkt = psa50_recv_frame (camera, &len);
				if (pkt)
					break;
			}
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
			strncpy (camera->pl->psa50_id, pkt + 26,
				 sizeof (camera->pl->psa50_id) - 1);

			gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_id : '%s'",
					 camera->pl->psa50_id);

			camera->pl->first_init = 0;

			if (!strcmp ("DE300 Canon Inc.", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Powershot A5");
				camera->pl->model = CANON_PS_A5;
				if (camera->pl->speed > 57600)
					camera->pl->slow_send = 1;
				camera->pl->A5 = 1;
			} else if (!strcmp ("Canon PowerShot A5 Zoom", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Powershot A5 Zoom");
				camera->pl->model = CANON_PS_A5_ZOOM;
				if (camera->pl->speed > 57600)
					camera->pl->slow_send = 1;
				camera->pl->A5 = 1;
			} else if (!strcmp ("Canon PowerShot A50", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot A50");
				camera->pl->model = CANON_PS_A50;
				if (camera->pl->speed > 57600)
					camera->pl->slow_send = 1;
			} else if (!strcmp ("Canon PowerShot S20", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot S20");
				camera->pl->model = CANON_PS_S20;
			} else if (!strcmp ("Canon PowerShot G1", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot G1");
				camera->pl->model = CANON_PS_G1;
			} else if (!strcmp ("Canon PowerShot G2", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot G2");
				camera->pl->model = CANON_PS_G2;
			} else if (!strcmp ("Canon PowerShot A10", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot A10");
				camera->pl->model = CANON_PS_A10;
			} else if (!strcmp ("Canon PowerShot A20", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot A20");
				camera->pl->model = CANON_PS_A20;
			} else if (!strcmp ("Canon EOS D30", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a EOS D30");
				camera->pl->model = CANON_EOS_D30;
			} else if (!strcmp ("Canon PowerShot Pro90 IS", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot Pro90 IS");
				camera->pl->model = CANON_PS_PRO90_IS;
			} else if (!strcmp ("Canon PowerShot Pro70", camera->pl->psa50_id)) {
				gp_camera_status (camera, "Detected a Powershot Pro70");
				camera->pl->model = CANON_PS_A70;
			} else if ((!strcmp ("Canon DIGITAL IXUS", camera->pl->psa50_id))
				   || (!strcmp ("Canon IXY DIGITAL", camera->pl->psa50_id))
				   || (!strcmp ("Canon PowerShot S100", camera->pl->psa50_id))
				   || (!strcmp ("Canon DIGITAL IXUS v", camera->pl->psa50_id))) {
				gp_camera_status (camera,
						  "Detected a Digital IXUS series / IXY DIGITAL / Powershot S100 series");
				camera->pl->model = CANON_PS_S100;
			} else if ((!strcmp ("Canon DIGITAL IXUS 300", camera->pl->psa50_id))
				   || (!strcmp ("Canon IXY DIGITAL 300", camera->pl->psa50_id))
				   || (!strcmp ("Canon PowerShot S300", camera->pl->psa50_id))) {
				gp_camera_status (camera,
						  "Detected a Digital IXUS 300 / IXY DIGITAL 300 / Powershot S300");
				camera->pl->model = CANON_PS_S300;
			} else {
				gp_camera_status (camera, "Detected a Powershot S10");
				camera->pl->model = CANON_PS_S10;
			}

			//  5 seconds  delay should  be enough for   big flash cards.   By
			// experience, one or two seconds is too  little, as a large flash
			// card needs more access time.
			serial_set_timeout (camera->port, 5000);
			(void) psa50_recv_packet (camera, &type, &seq, NULL);
			if (type != PKT_EOT || seq) {
				gp_camera_status (camera, _("Bad EOT"));
				return GP_ERROR;
			}
			seq_tx = 0;
			seq_rx = 1;
			if (!psa50_send_frame (camera, "\x00\x05\x00\x00\x00\x00\xdb\xd1", 8)) {
				gp_camera_status (camera, _("Communication error 2"));
				return GP_ERROR;
			}
			res = 0;
			switch (camera->pl->speed) {
				case 9600:
					res = psa50_send_frame (camera, SPEED_9600, 12);
					break;
				case 19200:
					res = psa50_send_frame (camera, SPEED_19200, 12);
					break;
				case 38400:
					res = psa50_send_frame (camera, SPEED_38400, 12);
					break;
				case 57600:
					res = psa50_send_frame (camera, SPEED_57600, 12);
					break;
				case 115200:
					res = psa50_send_frame (camera, SPEED_115200, 12);
					break;
			}

			if (!res
			    || !psa50_send_frame (camera, "\x00\x04\x01\x00\x00\x00\x24\xc6",
						  8)) {
				gp_camera_status (camera, _("Communication error 3"));
				return GP_ERROR;
			}
			speed = camera->pl->speed;
			gp_camera_status (camera, _("Changing speed... wait..."));
			if (!psa50_wait_for_ack (camera))
				return GP_ERROR;
			if (speed != 9600) {
				if (!canon_serial_change_speed (camera->port, speed)) {
					gp_camera_status (camera, _("Error changing speed"));
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "ERROR: Error changing speed");
				} else {
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "speed changed\n");
				}

			}
			for (try = 1; try < MAX_TRIES; try++) {
				psa50_send_packet (camera, PKT_EOT, seq_tx,
						   camera->pl->psa50_eot + PKT_HDR_LEN, 0);
				if (!psa50_wait_for_ack (camera)) {
					gp_camera_status (camera,
							  _
							  ("Error waiting ACK during initialization retrying"));
				} else
					break;
			}
	}
	if (try == MAX_TRIES) {
		gp_camera_status (camera, _("Error waiting ACK during initialization"));
		return GP_ERROR;
	}
	gp_camera_status (camera, _("Connected to camera"));
	/* Now is a good time to ask the camera for its owner
	 * name (and Model String as well)  */
	psa50_get_owner_name (camera);
	psa50_get_time (camera);
	return GP_OK;
}

/**
 * Ask the camera for the name of the flash storage
 * device. Usually "D:" or somehting like that.
 */
char *
psa50_get_disk (Camera *camera)
{
	unsigned char *msg;
	int len, res;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_disk()");

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			res = psa50_usb_long_dialogue (camera,
						       CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,
						       &msg, &len, 1024, NULL, 0, 0);
			if (res != GP_OK) {
				gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_disk: "
						 "psa50_usb_long_dialogue failed! returned %i",
						 res);
				return NULL;
			}
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0x0a, 0x11, &len, NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return NULL;
	}
	if (camera->pl->canon_comm_method == CANON_SERIAL_RS232) {
		char *t;

		t = strdup ((char *) msg + 4);	/* @@@ should check length */
		free (msg);
		if (!t) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_disk: "
					 "could not allocate %i bytes of memory to hold response",
					 strlen ((char *) msg + 4));
			return NULL;
		}
		msg = t;
	}

	gp_debug_printf (GP_DEBUG_HIGH, "canon", "psa50_get_disk: disk '%s'", msg);

	return msg;
}

/**
 * Gets available room and max capacity of a given disk.
 */
int
psa50_disk_info (Camera *camera, const char *name, int *capacity, int *available)
{
	unsigned char *msg;
	int len, cap, ava;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_disk_info() name '%s'", name);

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_DISK_INFO, &len,
						  name, strlen (name) + 1);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0x09, 0x11, &len, name,
						     strlen (name) + 1, NULL);
			break;
	}

	if (!msg) {
		psa50_error_type (camera);
		return 0;
	}
	if (len < 12) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: truncated message");
		return 0;
	}
	cap = get_int (msg + 4);
	ava = get_int (msg + 8);
	if (capacity)
		*capacity = cap;
	if (available)
		*available = ava;

	gp_debug_printf (GP_DEBUG_HIGH, "canon", "psa50_disk_info: "
			 "capacity %i kb, available %i kb",
			 cap > 0 ? (cap / 1024) : 0, ava > 0 ? (ava / 1024) : 0);

	return 1;
}


void
psa50_free_dir (Camera *camera, struct canon_dir *list)
{
	struct canon_dir *walk;

	for (walk = list; walk->name; walk++)
		free ((char *) walk->name);
	free (list);
}

/**
 * Get the directory tree of a given flash device.
 */
struct canon_dir *
psa50_list_directory (Camera *camera, const char *name)
{
	struct canon_dir *dir = NULL;
	int entries = 0, len, res;
	unsigned int payload_length;
	unsigned char *data, *p, *end_of_data;
	char attributes;
	unsigned char payload[100];

	/* Ask the camera for a full directory listing */
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			/* build payload :
			 *
			 * 0x00 dirname 0x00 0x00 0x00
			 *
			 * the 0x00 before dirname means 'no recursion'
			 * NOTE: the first 0x00 after dirname is the NULL byte terminating
			 * the string, so payload_length is strlen(name) + 4 
			 */
			if (strlen (name) + 4 > sizeof (payload)) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "psa50_list_directory: "
						 "Name '%s' too long (%i), won't fit in payload "
						 "buffer.", name, strlen (name));
				return NULL;	// XXX for now
				//return GP_ERROR_BAD_PARAMETERS;
			}
			memset (payload, 0x00, sizeof (payload));
			memcpy (payload + 1, name, strlen (name));
			payload_length = strlen (name) + 4;

			/* 1024 * 1024 is max realistic data size, out of the blue.
			 * 0 is to not show progress status
			 */
			res = psa50_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_DIRENT,
						       &data, &len, 1024 * 1024, payload,
						       payload_length, 0);
			if (res != GP_OK) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "psa50_list_directory: "
						 "psa50_usb_long_dialogue() failed to fetch direntrys, "
						 "returned %i", res);
				return NULL;
			}
			break;
		case CANON_SERIAL_RS232:
		default:
			data = psa50_serial_dialogue (camera, 0xb, 0x11, &len, "", 1, name,
						      strlen (name) + 1, "\x00", 2, NULL);
			break;
	}

	if (!data) {
		psa50_error_type (camera);
		return NULL;
	}
	if (len < 16) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: truncated message\n");
		return NULL;
	}

	end_of_data = data + len;
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			p = data - 1;
			if (p >= end_of_data)
				goto error;
			break;
		case CANON_SERIAL_RS232:
		default:
			if (!data[5])
				return NULL;
			p = data + 15;
			if (p >= end_of_data)
				goto error;
			for (; *p; p++)
				if (p >= end_of_data)
					goto error;
			break;
	}

	/* This is the main loop, for every entry in the structure */
	while (p[0xb] != 0x00) {
		unsigned char *start;
		int is_file;

		//fprintf(stderr,"p %p end_of_data %p len %d\n",p,end_of_data,len);
		if (p == end_of_data - 1) {
			if (data[4])
				break;
			if (camera->pl->canon_comm_method == CANON_SERIAL_RS232)
				data = psa50_recv_msg (camera, 0xb, 0x21, &len);
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "USB Driver: this message (%s line %i) "
						 "should never be printed", __FILE__,
						 __LINE__);
			if (!data)
				goto error;
			if (len < 5)
				goto error;
			p = data + 4;
		}
		if (p + 2 >= end_of_data)
			goto error;
		attributes = p[1];
		is_file = !((p[1] & 0x10) == 0x10);
		if (p[1] == 0x10 || is_file)
			p += 11;
		else
			break;
		if (p >= end_of_data)
			goto error;
		for (start = p; *p; p++)
			if (p >= end_of_data)
				goto error;
		dir = realloc (dir, sizeof (*dir) * (entries + 2));
		if (!dir) {
			perror ("realloc");
			exit (1);
		}
		dir[entries].name = strdup (start);
		if (!dir[entries].name) {
			perror ("strdup");
			exit (1);
		}

		memcpy ((unsigned char *) &dir[entries].size, start - 8, 4);
		dir[entries].size = byteswap32 (dir[entries].size);	/* re-order little/big endian */
		memcpy ((unsigned char *) &dir[entries].date, start - 4, 4);
		dir[entries].date = byteswap32 (dir[entries].date);	/* re-order little/big endian */
		dir[entries].is_file = is_file;
		dir[entries].attrs = attributes;
		// Every directory contains a "null" file entry, so we skip it
		if (strlen (dir[entries].name) > 0) {
			// Debug output:
			if (is_file)
				gp_debug_printf (GP_DEBUG_LOW, "canon", "-");
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon", "d");
			if ((dir[entries].attrs & 0x1) == 0x1)
				gp_debug_printf (GP_DEBUG_LOW, "canon", "r- ");
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon", "rw ");
			if ((dir[entries].attrs & 0x20) == 0x20)
				gp_debug_printf (GP_DEBUG_LOW, "canon", "  new   ");
			else
				gp_debug_printf (GP_DEBUG_LOW, "canon", "saved   ");
			gp_debug_printf (GP_DEBUG_LOW, "canon", "%#2x - %8i %.24s %s\n",
					 dir[entries].attrs, dir[entries].size,
					 asctime (gmtime (&dir[entries].date)),
					 dir[entries].name);
			entries++;
		}
	}
	if (camera->pl->canon_comm_method == CANON_USB)
		free (data);
	if (dir)
		dir[entries].name = NULL;
	return dir;
      error:
	gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: truncated message\n");
	if (dir)
		psa50_free_dir (camera, dir);
	return NULL;
}

unsigned char *
psa50_get_file_serial (Camera *camera, const char *name, int *length)
{
	unsigned char *file = NULL;
	unsigned char *msg;
	unsigned char name_len;
	unsigned int total = 0, expect = 0, size;
	int len, maxfilesize;

	gp_camera_progress (camera, 0);
	if (camera->pl->receive_error == FATAL_ERROR) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "ERROR: can't continue a fatal error condition detected\n");
		return NULL;
	}
	name_len = strlen (name) + 1;
	msg = psa50_serial_dialogue (camera, 0x1, 0x11, &len, "\x00\x00\x00\x00", 5,
				     &name_len, 1, "\x00", 2, name, strlen (name) + 1, NULL);
	if (!msg) {
		psa50_error_type (camera);
		return NULL;
	}
	while (msg) {
		if (len < 20 || get_int (msg)) {
			break;
		}
		if (!file) {
			total = get_int (msg + 4);
			if (camera->pl->model == CANON_PS_S20
			    || camera->pl->model == CANON_PS_G1
			    || camera->pl->model == CANON_PS_G2
			    || camera->pl->model == CANON_PS_S10) {
				maxfilesize = 10000000;
			} else {
				maxfilesize = 2000000;
			}

			if (total > maxfilesize) {
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
		size = get_int (msg + 12);
		if (get_int (msg + 8) != expect || expect + size > total || size > len - 20) {
			gp_debug_printf (GP_DEBUG_LOW, "canon", "ERROR: doesn't fit\n");
			break;
		}
		memcpy (file + expect, msg + 20, size);
		expect += size;
		gp_camera_progress (camera, total ? (expect / (float) total) : 1.);
		if ((expect == total) != get_int (msg + 16)) {
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "ERROR: end mark != end of data\n");
			break;
		}
		if (expect == total)
			return file;
		msg = psa50_recv_msg (camera, 0x1, 0x21, &len);
	}
	free (file);
	return NULL;
}

static int
psa50_get_file_usb (Camera *camera, const char *name, unsigned char **data, int *length)
{
	char payload[100];
	int payload_length, maxfilesize, res;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_file_usb() called for file '%s'",
			 name);

	/* 8 is strlen ("12111111") */
	if (8 + strlen (name) > sizeof (payload) - 1) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_file_usb: ERROR: "
				 "Supplied file name '%s' does not fit in payload buffer.",
				 name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Construct payload containing file name, buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	sprintf (payload, "12111111%s", name);
	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_file_usb: payload %s", payload);
	payload_length = strlen (payload) + 1;
	intatpos (payload, 0x0, 0x0);	// get picture
	intatpos (payload, 0x4, USB_BULK_READ_SIZE);

	if (camera->pl->model == CANON_PS_S10 || camera->pl->model == CANON_PS_S20
	    || camera->pl->model == CANON_PS_G2 || camera->pl->model == CANON_PS_G1
	    || camera->pl->model == CANON_PS_S300 || camera->pl->model == CANON_PS_S100
	    || camera->pl->model == CANON_PS_A10 || camera->pl->model == CANON_PS_A20
	    || camera->pl->model == CANON_EOS_D30 || camera->pl->model == CANON_PS_PRO90_IS) {
		maxfilesize = 10000000;
	} else {
		maxfilesize = 2000000;
	}

	/* the 1 is to show status */
	res = psa50_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
				       maxfilesize, payload, payload_length, 1);
	if (res != GP_OK) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_get_file_usb: psa50_usb_long_dialogue() "
				 "returned error (%i).", res);
		return res;
	}

	return GP_OK;
}

int
psa50_get_file (Camera *camera, const char *name, unsigned char **data, int *length)
{
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			return psa50_get_file_usb (camera, name, data, length);
			break;
		case CANON_SERIAL_RS232:
		default:
			*data = psa50_get_file_serial (camera, name, length);
			if (*data)
				return GP_OK;
			return GP_ERROR;
			break;
	}
}

static int
psa50_get_thumbnail_usb (Camera *camera, const char *name, unsigned char **data, int *length)
{
	char payload[100];
	int payload_length, res;

	/* 8 is strlen ("11111111") */
	if (8 + strlen (name) > sizeof (payload) - 1) {
		gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_thumbnail_usb: ERROR: "
				 "Supplied file name '%s' does not fit in "
				 "payload buffer.", name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* Construct payload containing file name, buffer size and function request.
	 * See the file Protocol in this directory for more information.
	 */
	sprintf (payload, "11111111%s", name);
	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_thumbnail_usb: "
			 "payload %s", payload);
	payload_length = strlen (payload) + 1;

	intatpos (payload, 0x0, 0x1);	// get thumbnail
	intatpos (payload, 0x4, USB_BULK_READ_SIZE);

	/* 0 is to not show status */
	res = psa50_usb_long_dialogue (camera, CANON_USB_FUNCTION_GET_FILE, data, length,
				       32 * 1024, payload, payload_length, 0);

	if (res != GP_OK) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "psa50_get_thumbnail_usb: psa50_usb_long_dialogue() "
				 "returned error (%i).", res);
		return res;
	}

	return GP_OK;
}

/**
 * Returns the thumbnail of a picture.
 */
unsigned char *
psa50_get_thumbnail (Camera *camera, const char *name, int *length)
{
	unsigned char *data = NULL;
	unsigned char *msg;
	exifparser exifdat;
	unsigned int total = 0, expect = 0, size, payload_length, total_file_size;
	int i, j, in;
	unsigned char *thumb;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "psa50_get_thumbnail() "
			 "called for file '%s'", name);

	gp_camera_progress (camera, 0);
	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			i = psa50_get_thumbnail_usb (camera, name, &data, length);
			if (i != GP_OK) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "psa50_get_thumbnail_usb() failed, returned %i",
						 i);
				return NULL;	// XXX for now
			}
			break;
		case CANON_SERIAL_RS232:
		default:
			if (camera->pl->receive_error == FATAL_ERROR) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: can't continue a fatal error condition detected");
				return NULL;
			}

			payload_length = strlen (name) + 1;
			msg = psa50_serial_dialogue (camera, 0x1, 0x11, &total_file_size,
						     "\x01\x00\x00\x00\x00", 5,
						     &payload_length, 1, "\x00", 2,
						     name, strlen (name) + 1, NULL);
			if (!msg) {
				psa50_error_type (camera);
				return NULL;
			}

			total = get_int (msg + 4);
			if (total > 2000000) {	/* 2 MB thumbnails ? unlikely ... */
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "ERROR: %d is too big\n", total);
				return NULL;
			}
			data = malloc (total);
			if (!data) {
				perror ("malloc");
				return NULL;
			}
			if (length)
				*length = total;

			while (msg) {
				if (total_file_size < 20 || get_int (msg)) {
					return NULL;
				}
				size = get_int (msg + 12);
				if (get_int (msg + 8) != expect || expect + size > total
				    || size > total_file_size - 20) {
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "ERROR: doesn't fit\n");
					return NULL;
				}
				memcpy (data + expect, msg + 20, size);
				expect += size;
				gp_camera_progress (camera,
						    total ? (expect / (float) total) : 1.);
				if ((expect == total) != get_int (msg + 16)) {
					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "ERROR: end mark != end of data\n");
					return NULL;
				}
				if (expect == total) {
					/* We finished receiving the file. Parse the header and
					   return just the thumbnail */
					break;
				}
				msg = psa50_recv_msg (camera, 0x1, 0x21, &total_file_size);
			}
			break;
	}

	switch (camera->pl->model) {
		case CANON_PS_A70:	/* pictures are JFIF files */
			/* we skip the first FF D8 */
			i = 3;
			j = 0;
			in = 0;

			/* we want to drop the header to get the thumbnail */

			thumb = malloc (total);
			if (!thumb) {
				perror ("malloc");
				break;
			}

			while (i < total) {
				if (data[i] == JPEG_ESC) {
					if (data[i + 1] == JPEG_BEG &&
					    ((data[i + 3] == JPEG_SOS)
					     || (data[i + 3] == JPEG_A50_SOS))) {
						in = 1;
					} else if (data[i + 1] == JPEG_END) {
						in = 0;
						thumb[j++] = data[i];
						thumb[j] = data[i + 1];
						return thumb;
					}
				}

				if (in == 1)
					thumb[j++] = data[i];
				i++;

			}
			return NULL;
			break;

		default:	/* Camera supports EXIF */
			exifdat.header = data;
			exifdat.data = data + 12;

			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "Got thumbnail, extracting it with the EXIF lib.\n");
			if (exif_parse_data (&exifdat) > 0) {
				gp_debug_printf (GP_DEBUG_LOW, "canon", "Parsed exif data.\n");
				data = exif_get_thumbnail (&exifdat);	// Extract Thumbnail
				if (data == NULL) {
					int f;
					char fn[255];

					if (rindex (name, '\\') != NULL)
						snprintf (fn, sizeof (fn) - 1,
							  "canon-death-dump.dat-%s",
							  rindex (name, '\\') + 1);
					else
						snprintf (fn, sizeof (fn) - 1,
							  "canon-death-dump.dat-%s", name);
					fn[sizeof (fn) - 1] = 0;

					gp_debug_printf (GP_DEBUG_LOW, "canon",
							 "psa50_get_thumbnail: "
							 "Thumbnail conversion error, saving "
							 "%i bytes to '%s'", total, fn);
					/* create with O_EXCL and 0600 for security */
					if ((f =
					     open (fn, O_CREAT | O_EXCL | O_RDWR,
						   0600)) == -1) {
						gp_debug_printf (GP_DEBUG_LOW, "canon",
								 "psa50_get_thumbnail: "
								 "error creating '%s': %m",
								 fn);
						break;
					}
					if (write (f, data, total) == -1) {
						gp_debug_printf (GP_DEBUG_LOW, "canon",
								 "psa50_get_thumbnail: "
								 "error writing to file '%s': %m",
								 fn);
					}

					close (f);
					break;
				}
				return data;
			}
			break;
	}

	free (data);
	return NULL;
}

int
psa50_delete_file (Camera *camera, const char *name, const char *dir)
{
	unsigned char payload[300];
	unsigned char *msg;
	int len, payload_length;

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			memcpy (payload, dir, strlen (dir) + 1);
			memcpy (payload + strlen (dir) + 1, name, strlen (name) + 1);
			payload_length = strlen (dir) + strlen (name) + 2;
			len = 0x4;
			msg = psa50_usb_dialogue (camera, CANON_USB_FUNCTION_DELETE_FILE, &len,
						  payload, payload_length);
			break;
		case CANON_SERIAL_RS232:
		default:
			msg = psa50_serial_dialogue (camera, 0xd, 0x11, &len, dir,
						     strlen (dir) + 1, name, strlen (name) + 1,
						     NULL);
			break;
	}
	if (!msg) {
		psa50_error_type (camera);
		return -1;
	}
	if (msg[0] == 0x29) {
		gp_camera_message (camera, _("File protected"));
		return -1;
	}

	return 0;
}

/*
 * Upload to USB Camera
 *
 */
int
psa50_put_file_usb (Camera *camera, CameraFile * file, char *destname, char *destpath)
{
	gp_camera_message (camera, _("Not implemented!"));
	return GP_ERROR_NOT_SUPPORTED;
}

/*
 * Upload to Camera via serial
 *
 */

#define HDR_FIXED_LEN 30
#define DATA_BLOCK 1536
int
psa50_put_file_serial (Camera *camera, CameraFile * file, char *destname, char *destpath)
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

	camera->pl->uploading = 1;
	gp_file_get_name (file, &name);
	for (i = 0; name[i]; i++)
		filename[i] = toupper (name[i]);
	filename[i] = '\0';

	hdr_len = HDR_FIXED_LEN + strlen (name) + strlen (destpath);

	gp_camera_progress (camera, 0);
	gp_file_get_data_and_size (file, &data, &size);

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

		msg = psa50_serial_dialogue (camera, 0x3, 0x11, &len, "\x02\x00\x00\x00", 4,
					     offset2, 4, block_len2, 4,
					     destpath, strlen (destpath), destname,
					     strlen (destname) + 1, buf, block_len, NULL);
		if (!msg) {
			camera->pl->uploading = 0;
			return GP_ERROR;
		}
		sent += block_len;
		gp_camera_progress (camera, size ? (sent / (float) size) : 1.);
	}
	camera->pl->uploading = 0;
	return GP_OK;
}

/*
 * Upload a file to the camera
 *
 */
int
psa50_put_file (Camera *camera, CameraFile * file, char *destname, char *destpath)
{

	switch (camera->pl->canon_comm_method) {
		case CANON_USB:
			return psa50_put_file_usb (camera, file, destname, destpath);
			break;
		case CANON_SERIAL_RS232:
		default:
			return psa50_put_file_serial (camera, file, destname, destpath);
			break;
	}
}

/*
 * print a message corresponding
 * to the error encountered
 */
void
psa50_error_type (Camera *camera)
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
