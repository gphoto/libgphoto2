/*
 * psa50.c - Canon PowerShot "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
 * Additions 2000 by Philippe Marzouk and Edouard Lafargue
 * USB support, 2000, by Mikael Nyström
 */

/**
 * This file now includes both USB and serial support. This is
 * experimental and not guaranteed to work 100%. Maybe a better
 * design would be to make serial and USB support completely separate,
 * but at the same time, both protocols/busses work almost the same
 * way.
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <netinet/in.h>
#include <gphoto2.h>
#include "../../libgphoto2/exif.h"

#include "serial.h"
#include "util.h"
#include "crc.h"
#include "psa50.h"

/************ new stuff ********/
#include <gpio.h>
gpio_device *gdev;
gpio_device_settings settings;
/************ new stuff ********/

#define MAX_TRIES 15

#define USLEEP1 0
#define USLEEP2 1

static unsigned char psa50_eot[8];

/* ------------------------- Frame-level processing ------------------------- */


#define CANON_FBEG      0xc0
#define CANON_FEND      0xc1
#define CANON_ESC       0x7e
#define CANON_XOR       0x20

int psa50_send_frame(Camera *camera, const unsigned char *pkt,int len)
{
    static unsigned char buffer[2100];
        /* worst case: two maximum-sized packets (~1020 bytes, full of data
           that needs to be escaped */
    unsigned char *p;

    p = buffer;
    *p++ = CANON_FBEG;
    while (len--) {
        if (p-buffer >= sizeof(buffer)-1) {
            debug_message(camera,"FATAL ERROR: send buffer overflow\n");
            exit(1);
        }
        if (*pkt != CANON_FBEG && *pkt != CANON_FEND && *pkt != CANON_ESC)
            *p++ = *pkt++;
        else {
            *p++ = CANON_ESC;
            *p++ = *pkt++ ^ CANON_XOR;
        }
    }
    *p++ = CANON_FEND;

	return !canon_serial_send(camera, buffer,p-buffer,USLEEP2);
}


unsigned char *psa50_recv_frame(Camera *camera, int *len)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    static unsigned char buffer[5000];
	/* more than enough :-) (allow for a few run-together packets) */
    unsigned char *p = buffer;
    int c;
	
    while ((c = canon_serial_get_byte(cs->gdev)) != CANON_FBEG)
	  if (c == -1) return NULL;
    while ((c = canon_serial_get_byte(cs->gdev)) != CANON_FEND) {
        if (c < 0) return NULL;
        if (c == CANON_ESC) c = canon_serial_get_byte(cs->gdev) ^ CANON_XOR;
        if (p-buffer >= sizeof(buffer)) {
            debug_message(camera,"FATAL ERROR: receive buffer overflow\n");
            exit(1);
        }
        *p++ = c;
    }
    dump_hex(camera,"RECV",buffer,p-buffer);
    if (len) *len = p-buffer;
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


static int psa50_send_packet(Camera *camera, unsigned char type,
							 unsigned char seq, unsigned char *pkt,int len)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *hdr = pkt-PKT_HDR_LEN;
    unsigned short crc;

    hdr[PKT_TYPE] = type;
    hdr[PKT_SEQ] = seq;
	hdr[PKT_LEN_LSB] = len & 0xff;
	hdr[PKT_LEN_MSB] = len >> 8;

	if (type == PKT_NACK) {
		hdr[PKT_TYPE] = PKT_ACK;
		hdr[PKT_TYPE+1] = '\xff';/* PKTACK_NACK; */
	}
	
	if (type == PKT_UPLOAD_EOT) {
		hdr[PKT_TYPE] = PKT_EOT;
		hdr[PKT_TYPE+1] = 0x3;
		len = 2;
	}
	/*
	if (pkt[4] == 0x3 && pkt[7] == 0x11) {
		hdr[PKT_LEN_LSB] = (len - 58) & 0xff;
		hdr[PKT_LEN_MSB] = (len - 58) >> 8;
	}
	*/
    if (type == PKT_EOT || type == PKT_ACK || type == PKT_NACK) len = 2; /* @@@ hack */
    crc = canon_psa50_gen_crc(hdr,len+PKT_HDR_LEN);
    pkt[len] = crc & 0xff;
    pkt[len+1] = crc >> 8;
	
    return psa50_send_frame(camera, hdr,len+PKT_HDR_LEN+2);
}


static unsigned char *psa50_recv_packet(Camera *camera, unsigned char *type,
										unsigned char *seq, int *len)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *pkt;
    unsigned short crc;
    int raw_length,length=0;
	
    pkt = psa50_recv_frame(camera, &raw_length);
    if (!pkt) return NULL;
    if (raw_length < PKT_HDR_LEN) {
        debug_message(camera,"ERROR: packet truncated\n");
        return NULL;
    }
    if (pkt[PKT_TYPE] == PKT_MSG) {
        length = pkt[PKT_LEN_LSB] | (pkt[PKT_LEN_MSB] << 8);
        if (length+PKT_HDR_LEN > raw_length-2) {
            debug_message(camera,"ERROR: invalid length\n");
			/*fprintf(stderr,"Sending NACK\n");
			 psa50_send_packet(PKT_NACK,seq_rx++,psa50_eot+PKT_HDR_LEN,0); */
			receive_error = ERROR_RECEIVED;
            return NULL;
        }
    }
    crc = pkt[raw_length-2] | (pkt[raw_length-1] << 8);
    if (!canon_psa50_chk_crc(pkt,raw_length-2,crc)) {
        debug_message(camera,"ERROR: CRC error\n");
        return NULL;
    }
    if (type) *type = pkt[PKT_TYPE];
    if (seq) *seq = pkt[PKT_SEQ];
    if (len) *len = length;
	if (*type == PKT_ACK || *type == PKT_EOT) return pkt;
    return pkt+PKT_HDR_LEN;
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




/**
 * Waits for an "ACK" from the camera.
 *
 * Return values:
 *  1 : ACK received
 *  0 : communication error (no reply received for example)
 * -1 : NACK received.
 */
static int psa50_wait_for_ack(Camera *camera)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *pkt;
    unsigned char type,seq,old_seq;
    int len;

    while (1) {
        pkt = psa50_recv_packet(camera, &type,&seq,&len);
        if (!pkt) return 0;
        if (seq == seq_tx && type == PKT_ACK) {
                if (pkt[2] == PKTACK_NACK) {
                        debug_message(camera,"ERROR: NACK received\n");
                        return -1;
                }
            seq_tx++;
            return 1;
        }
        old_seq='\0';
        if (type == PKT_EOT) {
			old_seq=pkt[0];
			if (receive_error == NOERROR) {
                debug_message(camera,"Old EOT received, sending corresponding ACK\n");
                if (!psa50_send_packet(camera,PKT_ACK,old_seq,psa50_eot+PKT_HDR_LEN,0)) return 0;
                pkt = psa50_recv_packet(camera,&type,&seq,&len);
                if (!pkt) return 0;
                if (seq == old_seq && type == PKT_ACK) {
					if (pkt[2] == PKTACK_NACK) {
						debug_message(camera,"Old EOT acknowledged\n");
						return -1;
					}
					return 1;
                }
			}
        }
        /* error already aknowledged, we skip the following ones */
        if (receive_error == ERROR_RECEIVED) {
                if (!psa50_send_packet(camera,PKT_NACK,old_seq,psa50_eot+PKT_HDR_LEN,0)) return 0;
                return 1;
        }

        debug_message(camera,"ERROR: ACK format or sequence error, retrying\n");
        debug_message(camera,"Sending NACK\n");
        psa50_send_packet(camera,PKT_NACK,seq_rx++,psa50_eot+PKT_HDR_LEN,0);
        receive_error = ERROR_RECEIVED;

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
static int psa50_send_msg(Camera *camera,unsigned char mtype,
						  unsigned char dir,va_list *ap)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char buffer[MAX_PKT_PAYLOAD+2]; /* allow space for CRC */
    unsigned char *pkt,*pos;
    int total, good_ack, try;

    memset(buffer,0,PKT_HDR_LEN+MSG_HDR_LEN);
    pkt = buffer+PKT_HDR_LEN;
    pkt[MSG_02] = 2;
    pkt[MSG_MTYPE] = mtype;
    pkt[MSG_DIR] = dir;
/*
	pkt[12] = 0xE2;
	pkt[13] = 0xC;
	pkt[14] = 0xA1;
	pkt[15] = 0x2;
*/		
    pos = pkt+MSG_HDR_LEN;
    total = 0;

    while (1) {
        const char *str;
        int len;
		
        str = va_arg(*ap,unsigned char *);
        if (!str) break;
        len = va_arg(*ap,int);
        if (pos+len-pkt > MAX_MSG_SIZE) {
			debug_message(camera,"FATAL ERROR: message too big (%i)\n",pos+len-pkt);
			exit(1);
        }
        memcpy(pos,str,len);
        pos += len;
    }

    total = pos-pkt;
	
	if (mtype == 0x3 && dir == 0x11) { // uploading
		pkt[MSG_LEN_LSB] = total & 0xff;
		pkt[MSG_LEN_MSB] = total >> 8;
		
		psa50_send_packet(camera, PKT_MSG,0,pkt,total);
//		psa50_send_packet(PKT_MSG,0x1,pkt+(total/2),total/2);
		if (!psa50_send_packet(camera, PKT_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,1))
		  return 0;
		return psa50_wait_for_ack(camera);
	}
	else {
		pkt[MSG_LEN_LSB] = total & 0xff;
		pkt[MSG_LEN_MSB] = total >> 8;
		for (try=1; try < MAX_TRIES; try++) {
			if (!psa50_send_packet(camera,PKT_MSG,0,pkt,total)) return 0;
			/* if upload packet (03 11) a second packet have to be sent before EOT. */
			if (mtype == 0x03 && dir == 0x11) {
				if (!psa50_send_packet(camera,PKT_UPLOAD_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,1))
				  return 0;
			} else
			  if (!psa50_send_packet(camera,PKT_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,1)) return 0;
			good_ack = psa50_wait_for_ack(camera);
			if (good_ack == -1) {
				debug_message(camera,"NACK received, retrying command\n");
			}
			else if (good_ack==1) {
				return good_ack;
			}
			else  {
				debug_message(camera,"No ACK received, retrying command\n");
				if (try==2) {
					//is the camera still there?
					if (!psa50_send_packet(camera,PKT_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,0)) return 0;
					good_ack = psa50_wait_for_ack(camera);
					if (good_ack==0) {
						receive_error = FATAL_ERROR;
						debug_message(camera,"ERROR: FATAL ERROR\n");
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
static unsigned char *psa50_recv_msg(Camera *camera, unsigned char mtype,
									 unsigned char dir,int *total)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    static unsigned char *msg = NULL;
    static int msg_size = 512; /* initial allocation/2 */
    unsigned char *frag;
    unsigned char type,seq;
    int len,length = 0,msg_pos = 0;

//	serial_set_timeout(5000);
    while (1) {
		frag = psa50_recv_packet(camera,&type,NULL,&len);
		if (!frag) return NULL;
		if (type == PKT_MSG) break;
		if (type == PKT_EOT) {
			debug_message(camera,"Old EOT received sending corresponding ACK\n");
			psa50_send_packet(camera,PKT_ACK,frag[0],psa50_eot+PKT_HDR_LEN,0);
		}
		debug_message(camera,"ERROR: protocol error, retrying\n");
    }
    /* we keep the fragment only if there was no error */
	if (receive_error == NOERROR) {
		length = frag[MSG_LEN_LSB] | (frag[MSG_LEN_MSB] << 8);
		if (len < MSG_HDR_LEN || frag[MSG_02] != 2) {
			debug_message(camera,"ERROR: message format error\n");
			return NULL;
		}

		if (frag[MSG_MTYPE] != mtype || frag[MSG_DIR] != dir) {
			if(frag[MSG_MTYPE] =='\x01' && frag[MSG_DIR] == '\x00' &&
			   memcmp(frag+12,"\x30\x00\x00\x30",4)) {
				debug_message(camera,"ERROR: Battery exhausted, camera off");
				gp_camera_status(camera, "Battery exhausted, camera off.");
				receive_error=ERROR_LOWBATT;
			}
			else {
				debug_message(camera,"ERROR: unexpected message.\n");
				gp_camera_status(camera, "ERROR: unexpected message");
			}
			return NULL;
		}
		frag += MSG_HDR_LEN;
		len -= MSG_HDR_LEN;
	}
    while (1) {
		if (receive_error == NOERROR) {
			if (msg_pos+len > length) {
				debug_message(camera,"ERROR: message overrun\n");
				gp_camera_status(camera, "ERROR: message overrun");
				return NULL;
			}
			if (msg_pos+len > msg_size || !msg) {
				msg_size *= 2;
				msg = realloc(msg,msg_size);
				if (!msg) {
					perror("realloc");
					exit(1);
				}
			}
			memcpy(msg+msg_pos,frag,len);
			msg_pos += len;
		}
        frag = psa50_recv_packet(camera,&type,&seq,&len);
        if (!frag) return NULL;
        if (type == PKT_EOT) {
			/* in case of error we don't want to stop as the camera will send
			 the 1st packet of the sequence again */
			if (receive_error == ERROR_RECEIVED) {
				seq_rx = seq;
				psa50_send_packet(camera,PKT_NACK,seq_rx,psa50_eot+PKT_HDR_LEN,0);
				receive_error = ERROR_ADDRESSED;
			} else {
				if (seq == seq_rx)  break;
				debug_message(camera,"ERROR: out of sequence\n");
				gp_camera_status(camera, "ERROR: out of sequence.");
				return NULL;
			}
		}
		if (type != PKT_MSG && receive_error == NOERROR) {
            debug_message(camera,"ERROR: unexpected packet type\n");
            gp_camera_status(camera, "ERROR: unexpected packet type.");
            return NULL;
        }
		if (type == PKT_EOT && receive_error == ERROR_RECEIVED) {
			receive_error = ERROR_ADDRESSED;
		}
		if (type == PKT_MSG && receive_error == ERROR_ADDRESSED) {
			msg_pos =0;
			length = frag[MSG_LEN_LSB] | (frag[MSG_LEN_MSB] << 8);
			if (len < MSG_HDR_LEN || frag[MSG_02] != 2) {
				debug_message(camera,"ERROR: message format error\n");
				gp_camera_status(camera, "ERROR: message format error.");
				return NULL;
			}
			
			if (frag[MSG_MTYPE] != mtype || frag[MSG_DIR] != dir) {
				if(frag[MSG_MTYPE] =='\x01' && frag[MSG_DIR] == '\x00' &&
				   memcmp(frag+12,"\x30\x00\x00\x30",4)) {
					debug_message(camera,"ERROR: Battery exhausted, camera off");
					gp_camera_status(camera, "Battery exhausted, camera off.");
					receive_error=ERROR_LOWBATT;
				}
				else {
					debug_message(camera,"ERROR: unexpected message2\n");
					gp_camera_status(camera, "ERROR: unexpected message2.");
				}
				return NULL;
			}
			frag += MSG_HDR_LEN;
			len -= MSG_HDR_LEN;
			receive_error = NOERROR;
		}
    }
	if (receive_error == ERROR_ADDRESSED) {
		receive_error = NOERROR;
	}
	if (receive_error == NOERROR) {
		if (!psa50_send_packet(camera,PKT_ACK,seq_rx++,psa50_eot+PKT_HDR_LEN,0))
		  return NULL;
		if (total) *total = msg_pos;
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
static unsigned char *psa50_serial_dialogue(Camera *camera,
											unsigned char mtype,
											unsigned char dir,int *len,...)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    va_list ap;
    int okay,try;
	unsigned char *good_ack;
	
	for ( try = 1; try < MAX_TRIES; try++) {
		va_start(ap,len);
		okay = psa50_send_msg(camera,mtype,dir,&ap);
		va_end(ap);
		if (!okay) return NULL;
//		if (mtype==0x03 && dir==0x11)
//		  return NULL; /*  no error but we want to quit at this point */

		good_ack = psa50_recv_msg(camera,mtype,dir ^ DIR_REVERSE,len);
		
		if (good_ack) return good_ack;
		if (receive_error == NOERROR) {
			debug_message(camera,"Resending message\n");
			seq_tx--;
        }
		if (receive_error == FATAL_ERROR) break;
	}
	return NULL;
}


/**
 * Utility function used by psa50_usb_dialogue
 */
void intatpos(unsigned char *block, int pos, int integer )
{
    *(unsigned *)(block+pos)=integer;
}

/**
 * USB version of the dialogue function.
 */
static unsigned char *psa50_usb_dialogue(Camera *camera,char cmd1, char cmd2, int cmd3, int *retlen, const char *payload, int pay_length)
{
#ifdef GPIO_USB
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	int msgsize;
	char packet[0x3000];
	static char buffer[0x3000];
//	int i;
	int lonlen;

    memset(packet,0x00,0x3000);  /* zero block */
    packet[0x40]=0x2;
    packet[0x44]=cmd1;
    packet[0x47]=cmd2;
    intatpos(packet,0x04,cmd3); /* cmd3 */
    intatpos(packet,0x4c,0x12345678);
    if (pay_length!=0)
      memcpy (packet+0x50,payload, pay_length);
    intatpos(packet,0x0,0x10+pay_length);
    intatpos(packet,0x48,0x10+pay_length);
    /*    fprintf(stderr,"%s\n",payload); */
    msgsize=0x50+pay_length;
    /*    for (i=0;i<0x54;i++)
     * {
     *  fprintf (stderr,"%x %x\n",i,packet[i]);
     * }
     */
	
    gpio_usb_msg_write(cs->gdev,0x10,packet,msgsize);
    if (cmd3==0x202)
      {
        gpio_read(cs->gdev, buffer, 0x40);
        lonlen=*(unsigned *)(buffer+0x6);
        //fprintf(stderr,"Internal lonlen: %x\n",lonlen);
        if (lonlen>0x3000)
          gpio_read(cs->gdev,buffer,0x2000);
        else
          gpio_read(cs->gdev,buffer,lonlen);
        *retlen=lonlen;
        return buffer;
      }
    else
      {
        //fprintf(stderr,"Internal retlen: %x\n",*retlen);
        gpio_read(cs->gdev, buffer, *retlen+0x50);
        return buffer+0x50;
      }

#else
    fprintf(stderr,"There is not USB support on this computer. Try 'serial', please.\n");
    return 0;
#endif

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

char psa50_id[200]; /* some models may have a lot to report */
//struct canon_info camera_data;



static unsigned int get_int(const unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}


/**
 * Switches the camera off, closes the serial driver and restores
 * the serial port to its previous settings.
 */
int psa50_end(Camera *camera)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	canon_serial_send(camera,"\xC0\x00\x02\x55\x2C\xC1",6,USLEEP2);
	canon_serial_send(camera,"\xC0\x00\x04\x01\x00\x00\x00\x24\xC6\xC1",8,USLEEP2);
	canon_serial_restore(camera);
	return 0;
}

/**
 * Switches the camera off, and resets the serial driver to 9600 bauds,
 * in order to be ready to switch the camera back on again if wanted.
 */
int psa50_off(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	canon_serial_send(camera,"\xC0\x00\x02\x55\x2C\xC1",6,USLEEP2);
	canon_serial_send(camera,"\xC0\x00\x04\x01\x00\x00\x00\x24\xC6\xC1",8,USLEEP2);
	canon_serial_change_speed(cs->gdev,9600);
	return 0;
}

/**
 * does operations on a directory based on the value
 * of action : DIR_CREATE, DIR_REMOVE
 * 
 */
int psa50_directory_operations(Camera *camera, char *path, int action)
{
	unsigned char *msg;
	int len;
	char type;
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	switch (action) {
	 case DIR_CREATE:
		type = 0x5;
		break;
	 case DIR_REMOVE:
		type = 0x6;
		break;
	 default:
		debug_message(camera,"Bad operation specified : %i\n", action);
		return GP_ERROR;
		break;
	}
	
	switch (canon_comm_method) {
	 case CANON_USB:
		msg = psa50_usb_dialogue(camera, type,0x11,0x201,&len,path,strlen(path)+1);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg = psa50_serial_dialogue(camera, type,0x11,&len,path,strlen(path)+1,NULL);
		break;
	}
	
	if (!msg) {
		psa50_error_type(camera);
		return 0;
	}

	if(msg[0] != 0x00) {
		if (action == DIR_CREATE)
		  debug_message(camera,"Could not create directory %s\n",path);
		else
		  debug_message(camera,"Could not remove directory %s\n",path);
		return 0;
	}
	
	return 1;
}

/**
 *  Gets   the   camera identification  string,  usually   the  owner
 * name.  This information is stored  in the "camera" structure, which
 * is a global variable for the driver.
 *  This function also gets the firmware revision in the camera struct.
 *
 */
int psa50_get_owner_name(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	unsigned char *msg;
	int len;
	
	switch (canon_comm_method) {
	 case CANON_USB:
		len=0x4c;
		msg = psa50_usb_dialogue(camera, 0x01,0x12,0x201,&len,0,0);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg =  psa50_serial_dialogue(camera, 0x01,0x12,&len,NULL);
		break;
		
	}
	
	if (!msg) {
		psa50_error_type(camera);
		return 0;
	}
	/* Store these values in our "camera" structure: */
	memcpy(cs->firmwrev,(char *) msg+8,4);
	strncpy(cs->ident,(char *) msg+12,30);
	strncpy(cs->owner,(char *) msg+44,30);
	
	return 1;
}

/**
 * Get battery status.
 */
int psa50_get_battery(Camera *camera, int *pwr_status, int *pwr_source)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	unsigned char *msg;
	int len;

	switch (canon_comm_method) {
	 case CANON_USB:
		len=0x8;
		msg = psa50_usb_dialogue(camera, 0x0A,0x12,0x201,&len,0,0);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg =  psa50_serial_dialogue(camera,0x0a,0x12,&len,NULL);
		break;
	}
	
	if (!msg) {
		psa50_error_type(camera);
		return -1;
	}
	
	if (pwr_status) *pwr_status = msg[4];
	if (pwr_source) *pwr_source = msg[7];
	debug_message(camera,"Status: %i / Source: %i\n",*pwr_status,*pwr_source);
	return 0;
}


/**
 * Sets a file's attributes. See the 'Protocol' file for details.
 */
int psa50_set_file_attributes(Camera *camera, const char *file, const char *dir, char attrs)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	unsigned char buf[300];
	unsigned char *msg;
	unsigned char attr[4];
	int len,name_len;
	
	attr[0] = attr[1] = attr[2] = 0;
	attr[3] = attrs;
	
	switch (canon_comm_method) {
	 case CANON_USB:
		memcpy(buf,attr,4);
		memcpy(buf+4,dir,strlen(dir)+1);
		memcpy(buf+4+strlen(dir)+1,file,strlen(file)+1);
		name_len=strlen(dir)+strlen(file)+2+4;
		len=0x4;
		msg = psa50_usb_dialogue(camera, 0x0e,0x11,0x201,&len,buf,name_len);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg =  psa50_serial_dialogue(camera, 0xe,0x11,&len,attr,4,dir,strlen(dir)+1,
									 file,strlen(file)+1,NULL);
		break;
	}
	
	if (!msg) {
		psa50_error_type(camera);
		return -1;
	}
	
	return 0;
}

/**
 *  Sets   the   camera owner name. The string should
 * not be more than 30 characters long. We call get_owner_name
 * afterwards in order to check that everything went fine.
 *
 */
int psa50_set_owner_name(Camera *camera, const char *name)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	unsigned char *msg;
	int len;

	if (strlen(name) > 30) {
		debug_message(camera,"Name too long (%i chars), could not store it !", strlen(name));
		gp_camera_status(camera, "Name too long, could not store it !");
		return 0;
	}
	debug_message(camera,"New owner: %s\n",name);

	switch (canon_comm_method) {
	 case CANON_USB:
		len=0x4;
		msg = psa50_usb_dialogue(camera, 0x05,0x12,0x201,&len,name,strlen(name)+1);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg =  psa50_serial_dialogue(camera, 0x05,0x12,&len,name,strlen(name)+1,NULL);
		break;
	}

	if (!msg) {
		psa50_error_type(camera);
		return 0;
	}
	return psa50_get_owner_name(camera);
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
time_t psa50_get_time(Camera *camera)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	unsigned char *msg;
	int len;
	time_t date;
	
	
	switch (canon_comm_method) {
	 case CANON_USB:
		len=0x10;
		msg = psa50_usb_dialogue(camera, 0x03,0x12,0x201,&len,0,0);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg=psa50_serial_dialogue(camera, 0x03,0x12,&len,NULL);
		break;
	}
	
	if (!msg) {
		psa50_error_type(camera);
		return 0;
	}
	
	/* Beware, this is very dirty, and might fail one day, if time_t
	 is not a 4-byte value anymore. */
	memcpy(&date,msg+4,4);
	
	return byteswap32(date);
}

int psa50_set_time(Camera *camera)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	unsigned char *msg;
	int len,i;
	time_t date;
	char pcdate[4];
	
	date = time(NULL);
	for (i=0;i<4;i++)
	  pcdate[i] = (date >> (8*i)) & 0xff;
	
	switch (canon_comm_method) {
	 case CANON_USB:
		len=0x10;
		msg = psa50_usb_dialogue(camera, 0x04,0x12,0x201,&len,0,0);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg = psa50_serial_dialogue(camera, 0x04, 0x12, &len, pcdate, 
									sizeof(pcdate),
									"\x00\x00\x00\x00\x00\x00\x00\x00",8,NULL);
		break;
	}
	
	if (!msg) {
		psa50_error_type(camera);
		return 0;
	}
	
	return 1;
}

/**
 * Switches the camera on, detects the model and sets its speed.
 */
int psa50_ready(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char type,seq;
    char *pkt;
    int try,len,speed,good_ack,res;
//    int cts;

	
    switch (canon_comm_method) {
	 case CANON_USB:
		psa50_get_owner_name(camera);
		psa50_get_time(camera);
		if (!strcmp("Canon PowerShot S20",cs->ident)) {
			gp_camera_status(camera, "Detected a Powershot S20");
			cs->model = CANON_PS_S20;
			A5 = 0;
			return 1;
		}
		else if (!strcmp("Canon PowerShot S10",cs->ident)) {
			gp_camera_status(camera, "Detected a Powershot S10");
			cs->model = CANON_PS_S10;
			A5 = 0;
			return 1;
		}
		else if ((!strcmp("Canon DIGITAL IXUS",cs->ident))
				 || (!strcmp("Canon PowerShot S100",cs->ident))) {
			gp_camera_status(camera, "Detected a Digital IXUS / Powershot S100");
			cs->model = CANON_PS_S100;
			A5=0;
			return 1;
		} else {
			printf ("Unknown camera!\n");
			return 0;
		}
		break;
	 case CANON_SERIAL_RS232:
	 default:
		
		serial_set_timeout(cs->gdev,900);  // 1 second is the delay for awakening the camera
		serial_flush_input(cs->gdev);
		serial_flush_output(cs->gdev);
		
		receive_error=NOERROR;
		
      /* First of all, we must check if the camera is already on */
		/*      cts=canon_serial_get_cts();
		 debug_message(camera,"cts : %i\n",cts);
		 if (cts==32) {  CTS == 32  when the camera is connected. */
			 if (cs->first_init==0) {
				 /* First case, the serial speed of the camera is the same as
				  * ours, so let's try to send a ping packet : */
				 if (!psa50_send_packet(camera, PKT_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,0))
				   return 0;
				 good_ack=psa50_wait_for_ack(camera);
				 debug_message(camera,"good_ack = %i\n",good_ack);
				 if (good_ack==0) {
					 /* no answer from the camera, let's try
					  * at the speed saved in the settings... */
					 speed=cs->speed;
					 if (speed!=9600) {
						 if(!canon_serial_change_speed(cs->gdev, speed)) {
							 gp_camera_status(camera, "Error changing speed.");
							 debug_message(camera,"speed changed.\n");
						 }
					 }
					 if (!psa50_send_packet(camera, PKT_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,0))
					   return 0;
					 good_ack=psa50_wait_for_ack(camera);
					 if (good_ack==0) {
						 gp_camera_status(camera, "Resetting protocol...");
						 psa50_off(camera);
						 sleep(3); /* The camera takes a while to switch off */
						 return psa50_ready(camera);
					 }
					 if (good_ack==-1) {
						 debug_message(camera,"Received a NACK !\n");
						 return 0;
					 }
					 gp_camera_status(camera, "Camera OK.\n");
					 return 1;
				 }
				 if (good_ack==-1) {
					 debug_message(camera,"Received a NACK !\n");
					 return 0;
				 }
				 debug_message(camera,"Camera replied to ping, proceed.\n");
				 return 1;
			 }
		
		/* Camera was off... */
		
		gp_camera_status(camera, "Looking for camera ...");
		gp_camera_progress(camera, NULL, 0);
		if(receive_error==FATAL_ERROR) {
			/* we try to recover from an error
			 we go back to 9600bps */
			if(!canon_serial_change_speed(cs->gdev, 9600 )) {
				debug_message(camera,"ERROR: Error changing speed");
				return 0;
			}
			receive_error=NOERROR;
		}
		for (try = 1; try < MAX_TRIES; try++) {
			gp_camera_progress(camera, NULL, (try/(float) MAX_TRIES)*100);
			if (canon_serial_send(camera,"\x55\x55\x55\x55\x55\x55\x55\x55",8,USLEEP1) < 0) {
				gp_camera_status(camera, "Communication error 1");
				return 0;
			}
			pkt = psa50_recv_frame(camera, &len);
			if (pkt) break;
		}
		if (try == MAX_TRIES) {
			gp_camera_status(camera, "No response from camera");
			return 0;
		}
		if (!pkt) {
			gp_camera_status(camera, "No response from camera");
			return 0;
		}
		if (len < 40 && strncmp(pkt+26,"Canon",5)) {
			gp_camera_status(camera, "Unrecognized response");
			return 0;
		}
		strcpy(psa50_id,pkt+26); /* @@@ check size */
		
		debug_message(camera,"psa50_id : '%s'\n",psa50_id);
		
		cs->first_init=0;
		
		if (!strcmp("DE300 Canon Inc.",psa50_id)) {
			gp_camera_status(camera, "Powershot A5");
			cs->model = CANON_PS_A5;
			A5 = 1;
		} else if (!strcmp("Canon PowerShot A5 Zoom",psa50_id)) {
			gp_camera_status(camera, "Powershot A5 Zoom");
			cs->model = CANON_PS_A5_ZOOM;
			A5 = 1;
		} else if (!strcmp("Canon PowerShot A50",psa50_id)) {
			gp_camera_status(camera, "Detected a Powershot A50");
			cs->model = CANON_PS_A50;
			A5 = 0;
		} else if (!strcmp("Canon PowerShot S20",psa50_id)) {
			gp_camera_status(camera, "Detected a Powershot S20");
			cs->model = CANON_PS_S20;
			A5 = 0;
        } else if ((!strcmp("Canon DIGITAL IXUS",psa50_id))
				   || (!strcmp("Canon PowerShot S100",psa50_id)))
                {
					gp_camera_status(camera, "Detected a Digital IXUS / Powershot S100");
					cs->model = CANON_PS_S100;
					A5=0;
				} else {
					gp_camera_status(camera, "Detected a Powershot S10");
					cs->model = CANON_PS_S10;
					A5 = 0;
				}
		
		//  5 seconds  delay should  be enough for   big flash cards.   By
		// experience, one or two seconds is too  little, as a large flash
		// card needs more access time.
		serial_set_timeout(cs->gdev,5000);
		(void) psa50_recv_packet(camera, &type,&seq,NULL);
		if (type != PKT_EOT || seq) {
			gp_camera_status(camera, "Bad EOT");
			return 0;
		}
		seq_tx = 0;
		seq_rx = 1;
		if (!psa50_send_frame(camera,"\x00\x05\x00\x00\x00\x00\xdb\xd1",8)) {
			gp_camera_status(camera, "Communication error 2");
			return 0;
		}
		res=0;
		switch (cs->speed) {
		 case 9600: res=psa50_send_frame(camera,SPEED_9600,12); break;
		 case 19200: res=psa50_send_frame(camera,SPEED_19200,12); break;
		 case 38400: res=psa50_send_frame(camera,SPEED_38400,12); break;
		 case 57600: res=psa50_send_frame(camera,SPEED_57600,12); break;
		 case 115200: res=psa50_send_frame(camera,SPEED_115200,12); break;
		}
		
		if( !res || !psa50_send_frame(camera,"\x00\x04\x01\x00\x00\x00\x24\xc6",8)) {
			gp_camera_status(camera, "Communication error 3");
			return 0;
		}
		speed=cs->speed;
		gp_camera_status(camera, "Changing speed... wait...");
		if (!psa50_wait_for_ack(camera)) return 0;
		if (speed!=9600) {
			if(!canon_serial_change_speed(cs->gdev,speed)) {
				gp_camera_status(camera, "Error changing speed");
				debug_message(camera,"ERROR: Error changing speed");
			}
			else {
				debug_message(camera,"speed changed\n");
			}
			
		}
		for (try=1; try < MAX_TRIES; try++) {
			psa50_send_packet(camera, PKT_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,0);
			if (!psa50_wait_for_ack(camera)) {
				gp_camera_status(camera, "Error waiting ACK during initialization retrying");
			} else
			  break;
		}
	}
	if (try==MAX_TRIES) {
        gp_camera_status(camera, "Error waiting ACK during initialization");
        return 0;
	}
	gp_camera_status(camera, "Connected to camera");
	/* Now is a good time to ask the camera for its owner
	 * name (and Model String as well)  */
	psa50_get_owner_name(camera);
	psa50_get_time(camera);
	return 1;
}

/**
 * Ask the camera for the name of the flash storage
 * device. Usually "D:" or somehting like that.
 */
char *psa50_get_disk(Camera *camera)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *msg;
    int len;

    switch (canon_comm_method) {
    case CANON_USB:
      msg = psa50_usb_dialogue(camera,0x0A,0x11,0x202,&len,0,0);
      break;
    case CANON_SERIAL_RS232:
    default:
      msg = psa50_serial_dialogue(camera,0x0a,0x11,&len,NULL);
      break;
    }

    if (!msg) {
      psa50_error_type(camera);
      return NULL;
    }
    if (canon_comm_method == CANON_SERIAL_RS232) {
      msg = strdup((char *) msg+4); /* @@@ should check length */
      if (!msg) perror("strdup");
    }
    return msg;
}

/**
 * Gets available room and max capacity of a given disk.
 */
int psa50_disk_info(Camera *camera, const char *name,int *capacity,int *available)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *msg;
    int len;

    switch (canon_comm_method) {
    case CANON_USB:
      len=0xc;
      msg = psa50_usb_dialogue(camera,0x09,0x11,0x201,&len,name,strlen(name)+1);
      break;
    case CANON_SERIAL_RS232:
    default:
      msg = psa50_serial_dialogue(camera,0x09,0x11,&len,name,strlen(name)+1,NULL);
      break;
    }

    if (!msg) {
		psa50_error_type(camera);
		return 0;
    }
    if (len < 12) {
        debug_message(camera,"ERROR: truncated message\n");
        return 0;
    }
    if (capacity) *capacity = get_int(msg+4);
    if (available) *available = get_int(msg+8);
    return 1;
}


void psa50_free_dir(Camera *camera, struct psa50_dir *list)
{
    struct psa50_dir *walk;

    for (walk = list; walk->name; walk++) free((char *) walk->name);
    free(list);
}


/**
 * Get the directory tree of a given flash device.
 */
struct psa50_dir *psa50_list_directory(Camera *camera, const char *name)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    struct psa50_dir *dir = NULL;
    int entries = 0;
    unsigned char *msg,*p;
    int len,namlen;
    char attributes;
    char newstr[100];

    /* Ask the camera for a full directory listing */
    switch (canon_comm_method) {
	 case CANON_USB:
		//    fprintf(stderr,"Path: %s\n",name);
		newstr[0] = 1;
		newstr[1] = 0;
		strcat(newstr,name);
		namlen=strlen(newstr)+3;
		newstr[0] = 0;
		
		msg = psa50_usb_dialogue(camera, 0x0b,0x11,0x202,&len,newstr,namlen);
		//    fprintf(stderr,"Dir length: %x\n",len);
		// 
		break;
	 case CANON_SERIAL_RS232:
	 default:
		msg = psa50_serial_dialogue(camera,0xb,0x11,&len,"",1,name,strlen(name)+1,
									"\x00",2,NULL);
		break;
    }
	
    if (!msg) {
		psa50_error_type(camera);
		return NULL;
    }
    if (len < 16) {
		return NULL;
    }
	
    switch (canon_comm_method) {
	 case CANON_USB:
		p=msg-1;
		if (p >= msg+len) goto error;
		break;
	 case CANON_SERIAL_RS232:
	 default:
		if (!msg[5]) return NULL;
		p = msg+15;
		if (p >= msg+len) goto error;
		for (; *p; p++)
		  if (p >= msg+len) goto error;
		break;
    }
	
    /* This is the main loop, for every entry in the structure */
    while (p[0xb] != 0x00) {
        unsigned char *start;
        int is_file;
		
        //fprintf(stderr,"p %p msg+len %p len %d\n",p,msg+len,len);
        if (p == msg+len-1) {
            if (msg[4]) break;
            if (canon_comm_method == CANON_SERIAL_RS232)
              msg = psa50_recv_msg(camera, 0xb,0x21,&len);
            else
              debug_message(camera,"USB Driver: this message (psa50.c line 1126) should never be printed");
            if (!msg) goto error;
            if (len < 5) goto error;
            p = msg+4;
        }
        if (p+2 >= msg+len) goto error;
        attributes = p[1];
        is_file = !((p[1] & 0x10) == 0x10);
        if (p[1] == 0x10 || is_file) p += 11;
        else break;
        if (p >= msg+len) goto error;
        for (start = p; *p; p++)
            if (p >= msg+len) goto error;
        dir = realloc(dir,sizeof(*dir)*(entries+2));
        if (!dir) {
            perror("realloc");
            exit(1);
        }
        dir[entries].name = strdup(start);
        if (!dir[entries].name) {
            perror("strdup");
            exit(1);
        }

        memcpy((unsigned char *) &dir[entries].size,start-8,4);
        dir[entries].size = byteswap32(dir[entries].size); /* re-order little/big endian */
        memcpy((unsigned char *) &dir[entries].date,start-4,4);
        dir[entries].date = byteswap32(dir[entries].date); /* re-order little/big endian */
        dir[entries].is_file = is_file;
        dir[entries].attrs = attributes;
        // Every directory contains a "null" file entry, so we skip it
        if (strlen(dir[entries].name) > 0) {
          // Debug output:
          if (is_file) debug_message(camera,"-"); else debug_message(camera,"d");
          if ((dir[entries].attrs & 0x1) == 0x1) debug_message(camera,"r- "); else debug_message(camera,"rw ");
          if ((dir[entries].attrs & 0x20) == 0x20) debug_message(camera,"  new   "); else debug_message(camera,"saved   ");
          debug_message(camera,"%#2x - %8i %.24s %s\n",dir[entries].attrs, dir[entries].size,
                  asctime(gmtime(&dir[entries].date)), dir[entries].name);
          entries++;
        }
    }
    if (dir) dir[entries].name = NULL;
    return dir;
error:
    debug_message(camera,"ERROR: truncated message\n");
    if (dir) psa50_free_dir(camera, dir);
    return NULL;
}


unsigned char *psa50_get_file_serial(Camera *camera, const char *name,int *length)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *file = NULL;
    unsigned char *msg;
    unsigned char name_len;
    unsigned int total = 0,expect = 0,size;
    int len,maxfilesize;

    gp_camera_progress(camera, NULL, 0);
    if(receive_error==FATAL_ERROR) {
      debug_message(camera,"ERROR: can't continue a fatal error condition detected\n");
      return NULL;
    }
    name_len = strlen(name)+1;
    msg = psa50_serial_dialogue(camera, 0x1,0x11,&len,"\x00\x00\x00\x00",5,
                                &name_len,1,"\x00",2,name,strlen(name)+1,NULL);
    if (!msg) {
      psa50_error_type(camera);
      return NULL;
    }
    while (msg) {
        if (len < 20 || get_int(msg)) {
            break;
        }
        if (!file) {
            total = get_int(msg+4);
                if(cs->model == CANON_PS_S20) {
                        maxfilesize=3000000;
                }
                else {
                        maxfilesize=2000000;
                }

            if (total > maxfilesize) {
                debug_message(camera,"ERROR: %d is too big\n",total);
                break;
            }
            file = malloc(total);
            if (!file) {
                perror("malloc");
                break;
            }
            if (length) *length = total;
        }
        size = get_int(msg+12);
        if (get_int(msg+8) != expect || expect+size > total || size > len-20) {
            debug_message(camera,"ERROR: doesn't fit\n");
            break;
        }
        memcpy(file+expect,msg+20,size);
        expect += size;
        gp_camera_progress(camera, NULL, total ? (expect/(float) total)*100 : 100);
        if ((expect == total) != get_int(msg+16)) {
            debug_message(camera,"ERROR: end mark != end of data\n");
            break;
        }
        if (expect == total) return file;
        msg = psa50_recv_msg(camera, 0x1,0x21,&len);
    }
    free(file);
    return NULL;
}

unsigned char *psa50_get_file_usb(Camera *camera, const char *name,int *length)
{
#ifdef GPIO_USB
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *file = NULL;
    unsigned char msg[0x3000];
    unsigned char *msgg;
    char newstr[100];
//    unsigned char *namn;
    unsigned int total = 0,expect = 0,size;
    int name_len,len,maxfilesize;

    gp_camera_progress(camera, NULL, 0);
    if(receive_error==FATAL_ERROR) {
      debug_message(camera,"ERROR: can't continue a fatal error condition detected\n");
      return NULL;
    }
        //      *newstr="12345678";
    newstr[0]=0x31;
    newstr[1]=0x32;
    newstr[2]=0x31;
    newstr[3]=0x31;
    newstr[4]=0x31;
    newstr[5]=0x31;
    newstr[6]=0x31;
    newstr[7]=0x31;
    newstr[8]=0x0;
    size=0x2000;
    strcat(newstr,name);
    debug_message(camera,"%s\n",newstr);
    name_len=strlen(newstr)+1;
    intatpos(newstr,0x0,0x0); // get picture
    intatpos(newstr,0x4,size);
    msgg = psa50_usb_dialogue(camera, 0x01,0x11,0x202,&len,newstr,name_len);
    memcpy(msg,msgg,size);
    *length=len;

    while (1) {

      if (!file) {
        total=len;
        if(cs->model == CANON_PS_S20) {
          maxfilesize=3000000;
        }
        else {
          maxfilesize=2000000;
        }

            if (total > maxfilesize) {
              debug_message(camera,"ERROR: %d is too big\n",total);
              break;
            }
            file = malloc(total);
            if (!file) {
              debug_message(camera,"ERROR: Alloc problems!\n");
              perror("malloc");
              break;
            }
      }

      memcpy(file+expect,msg,size);

      expect += size;

      gp_camera_progress(camera, NULL, total ? (expect/(float) total)*100 : 100);
      if (expect == total) return file;

      if ((total-expect) <= 0x2000)
        size=total-expect;
      else
        size=0x2000;

		/* FIXME: (pm) a direct call to gpio_read here ??????? */
		gpio_read(cs->gdev,msg,size);
    }
    free(file);
    return NULL;
#else
    debug_message(camera,"This computer does not support USB. Please use the RS-232 serial link.\n");
    return NULL;
#endif
}

unsigned char *psa50_get_file(Camera *camera, const char *name,int *length)
{
	switch (canon_comm_method) {
	 case CANON_USB:
		return psa50_get_file_usb(camera, name,length);
		break;
	 case CANON_SERIAL_RS232:
	 default:
		return psa50_get_file_serial(camera, name,length);
		break;
	}
}

/**
 * Returns the thumbnail of a picture.
 */
unsigned char *psa50_get_thumbnail(Camera *camera, const char *name,int *length)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char *file = NULL;
    unsigned char *msg;
    char newstr[100];
    exifparser exifdat;
    unsigned char name_len;
    unsigned int total = 0,expect = 0,size;
    int len;


    gp_camera_progress(camera, NULL, 0);
    if(receive_error==FATAL_ERROR) {
		debug_message(camera,"ERROR: can't continue a fatal error condition detected\n");
		return NULL;
    }

    switch (canon_comm_method) {
	 case CANON_USB:
		newstr[0]=0x31;
		newstr[1]=0x31;
		newstr[2]=0x31;
		newstr[3]=0x31;
		newstr[4]=0x31;
		newstr[5]=0x31;
		newstr[6]=0x31;
		newstr[7]=0x31;
		newstr[8]=0x0;

		strcat(newstr,name);
		debug_message(camera,"%s\n",newstr);
		name_len=strlen(newstr)+1;
		intatpos(newstr,0x0,0x1); // get picturethumbnail
		intatpos(newstr,0x4,0x3000);
		msg = psa50_usb_dialogue(camera, 0x01,0x11,0x202,&len,newstr,name_len);
		debug_message(camera,"File length: %x\n",len);
		file = malloc(len);
		memcpy(file,msg,len);
		*length=len;
		break;
	 case CANON_SERIAL_RS232:
	 default:
		name_len = strlen(name)+1;
		msg = psa50_serial_dialogue(camera,0x1,0x11,&len,
									"\x01\x00\x00\x00\x00",5,
									&name_len,1,"\x00",2,
									name,strlen(name)+1,NULL);
		if (!msg) {
			psa50_error_type(camera);
			return NULL;
		}
		while (msg) {
			if (len < 20 || get_int(msg)) {
				return NULL;
			}
			if (!file) {
				total = get_int(msg+4);
				if (total > 2000000) { /* 2 MB thumbnails ? unlikely ... */
					debug_message(camera,"ERROR: %d is too big\n",total);
					return NULL;
				}
				file = malloc(total);
				if (!file) {
					perror("malloc");
					return NULL;
				}
				if (length) *length = total;
			}
			size = get_int(msg+12);
			if (get_int(msg+8) != expect || expect+size > total || size > len-20) {
				debug_message(camera,"ERROR: doesn't fit\n");
				return NULL;
			}
			memcpy(file+expect,msg+20,size);
			expect += size;
			gp_camera_progress(camera, NULL, total ? (expect/(float) total)*100 : 100);
			if ((expect == total) != get_int(msg+16)) {
				debug_message(camera,"ERROR: end mark != end of data\n");
				return NULL;
			}
			if (expect == total) {
				/* We finished receiving the file. Parse the header and
				 return just the thumbnail */
				break;
			}
			msg = psa50_recv_msg(camera,0x1,0x21,&len);
		}
		break;
    }

    exifdat.header=file;
    exifdat.data=file+12;
	
    debug_message(camera,"Got thumbnail, extracting it with the EXIF lib.\n");
    if (exif_parse_data(&exifdat)>0){
		debug_message(camera,"Parsed exif data.\n");
		file = exif_get_thumbnail(&exifdat); // Extract Thumbnail
		if (file==NULL) {
			FILE *fp;
			
			debug_message(camera,"Thumbnail conversion error, saving data to \
canon-death-dump.dat\n");
			if ((fp=fopen("canon-death-dump.dat","w"))!=NULL){
				fwrite(file,1,total,fp);
				fclose(fp);
			}
			free(file);
			return NULL;
		}
		return file;
    }
    return NULL;
}

int psa50_delete_file(Camera *camera, const char *name, const char *dir)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
    unsigned char buf[300];
    unsigned char *msg;
    int len,name_len;

    switch (canon_comm_method) {
    case CANON_USB:
      memcpy(buf,dir,strlen(dir)+1);
      memcpy(buf+strlen(dir)+1,name,strlen(name)+1);
      name_len=strlen(dir)+strlen(name)+2;
      len=0x4;
      msg = psa50_usb_dialogue(camera,0x0d,0x11,0x201,&len,buf,name_len);
      break;
    case CANON_SERIAL_RS232:
    default:
      msg =  psa50_serial_dialogue(camera,0xd,0x11,&len,dir,strlen(dir)+1,
                                 name,strlen(name)+1,NULL);
      break;
    }
    if (!msg) {
		psa50_error_type(camera);
		return -1;
    }
	if (msg[0] == 0x29) {
		gp_camera_message(camera,"File protected");
		return -1;
	}

    return 0;
}

/*
 * Upload to USB Camera
 *
 */
int psa50_put_file_usb(Camera *camera, CameraFile *file, char *destname, char *destpath) 
{
    gp_camera_message(camera,"Not implemented!");
    return GP_ERROR;
}

/*
 * Upload to serial Camera
 *
 */

#define HDR_FIXED_LEN 30
#define DATA_BLOCK 900
int psa50_put_file_serial(Camera *camera, CameraFile *file, char *destname, char *destpath)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	unsigned char *msg;
	char filename[64];
	char buf[4096];
	int offset=0;
	char offset2[4];
	int block_len;
	char block_len2[4];
	int sent=0;
	int i,j=0,len, hdr_len;
//	int okay, good_ack;
	
	for(i=0;file->name[i];i++)
	  filename[i]=toupper(file->name[i]);
	filename[i]='\0';

	hdr_len = HDR_FIXED_LEN + strlen(file->name) + strlen(destpath);
	
	while(sent<file->size) {
		
		if(file->size < DATA_BLOCK)
		  block_len = file->size;
		else if ((file->size - sent < DATA_BLOCK))
		  block_len = file->size - sent;
		else
		  block_len = DATA_BLOCK;
		
		offset = sent;
		fprintf(stderr,"offset: %i, sent: %i\n",offset,sent);
		
		for(i=0;i<4;i++) {
			offset2[i] = (offset >> (8*i)) & 0xff;
			block_len2[i] = (block_len >> (8*i)) & 0xff;
		}

		for(i=0; i<DATA_BLOCK; i++) {
			buf[i] = file->data[j];
			j++;
		}
	
		msg = psa50_serial_dialogue(camera,0x3,0x11,&len,"\x02\x00\x00\x00",4,
				offset2,4,block_len2,4,
			    destpath,strlen(destpath),destname,strlen(destname)+1,
				buf,block_len,
				NULL);
		
		sent += block_len;
		
		fprintf(stderr,"Message sent!\n");
	}
	
/* 
    for(i=0; i<PUT_BLOCK1; i++) {
		buf[i] = file->data[j];
		j++;
	}
	okay = psa50_send_packet(PKT_MSG,0x01,buf,PUT_BLOCK2);
	if(!okay)
	  return GP_ERROR;
*/ 

/*	
	if (!psa50_send_packet(PKT_UPLOAD_EOT,seq_tx,psa50_eot+PKT_HDR_LEN,1)) 
	  return 0;
	good_ack = psa50_wait_for_ack();
	if (good_ack == -1) {
		debug_message(camera,"ERROR: NACK received\n");
		return GP_ERROR;
	}
	else if (good_ack==1) {
		debug_message(camera,"ACK received\n");
	}
*/	
	
	
	
	
    return GP_OK;
}

/*
 * Upload a file to the camera
 *
 */
int psa50_put_file(Camera *camera, CameraFile *file, char *destname, char *destpath)
{

  switch (canon_comm_method) {
  case CANON_USB:
    return psa50_put_file_usb(camera, file,destname,destpath);
    break;
  case CANON_SERIAL_RS232:
  default:
    return psa50_put_file_serial(camera, file,destname,destpath);
    break;
  }
}

/*
 * return to the calling function a number corresponding
 * to the error encountered
 */
void psa50_error_type(Camera *camera)
{
        switch(receive_error) {
                case ERROR_LOWBATT:
                        debug_message(camera,"ERROR: no battery left, Bailing out!\n");
                        break;
                case FATAL_ERROR:
                        debug_message(camera,"ERROR: camera connection lost!\n");
                        break;
                default:
                        debug_message(camera,"ERROR: malformed message\n");
                        break;
        }
}
