/****************************************************************************
 *
 * File: serial.h
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef _SERIAL_H
#define _SERIAL_H

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

int canon_serial_change_speed(GPPort *gdev, int speed);
int canon_serial_init(Camera *camera);
int canon_serial_send(Camera *camera, const unsigned char *buf, int len, int sleep);
int canon_serial_get_byte(GPPort *gdev);
int canon_serial_get_cts(GPPort *gdev);

void serial_flush_input(GPPort *gdev);
void serial_flush_output(GPPort *gdev);
void serial_set_timeout(GPPort *gdev, int to);

int canon_serial_send_frame (Camera *camera, const unsigned char *pkt, int len);
unsigned char *canon_serial_recv_frame (Camera *camera, int *len);
void canon_serial_error_type(Camera *camera);

unsigned char * canon_serial_dialogue (Camera *camera, unsigned char mtype, unsigned char dir, int *len, ...);
int canon_serial_send_packet (Camera *camera, unsigned char type, unsigned char seq, unsigned char *pkt, int len);
unsigned char *canon_serial_recv_packet (Camera *camera, unsigned char *type, unsigned char *seq, int *len);
int canon_serial_wait_for_ack (Camera *camera);
unsigned char *canon_serial_recv_msg (Camera *camera, unsigned char mtype, unsigned char dir, int *total);

unsigned char *canon_serial_get_file (Camera *camera, const char *name, int *length);
int canon_serial_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath);
int canon_serial_get_dirents (Camera *camera, unsigned char **dirent_data, unsigned int *dirents_length, const char *path);

int canon_serial_ready (Camera *camera);

#define MAX_TRIES 10

#define USLEEP1 0
#define USLEEP2 1

#define HDR_FIXED_LEN 30
#define DATA_BLOCK 1536


/* ------------------------- Frame-level processing ------------------------- */


#define CANON_FBEG      0xc0
#define CANON_FEND      0xc1
#define CANON_ESC       0x7e
#define CANON_XOR       0x20

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



#endif /* _SERIAL_H */

/****************************************************************************
 *
 * End of file: serial.h
 *
 ****************************************************************************/

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
