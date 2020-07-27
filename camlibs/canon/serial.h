/****************************************************************************
 *
 * File: serial.h
 *
 ****************************************************************************/

#ifndef _SERIAL_H
#define _SERIAL_H

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

int canon_serial_init(Camera *camera);

void canon_serial_error_type(Camera *camera);

unsigned char * canon_serial_dialogue (Camera *camera, GPContext *context, unsigned char mtype, unsigned char dir, unsigned int *len, ...);

unsigned char *canon_serial_get_file (Camera *camera, const char *name, unsigned int *length, GPContext *context);
int canon_serial_put_file (Camera *camera, CameraFile *file, const char *filename, const char *destname, const char *destpath, GPContext *context);
int canon_serial_get_dirents (Camera *camera, unsigned char **dirent_data, unsigned int *dirents_length, const char *path, GPContext *context);


int canon_serial_ready (Camera *camera, GPContext *context);
int canon_serial_get_thumbnail (Camera *camera, const char *name, unsigned char **data, unsigned int *length, GPContext *context);

/**
 * MAX_TRIES
 *
 * Maximum number of retries for a serial send operation.
 *
 */
#define MAX_TRIES 10

/**
 * USLEEP1
 *
 * Number of microseconds to wait between characters when trying to
 * contact the camera by sending "U" characters. Currently zero
 * (i.e. no pause between characters).
 *
 */
#define USLEEP1 0

/**
 * USLEEP2
 *
 * Number of microseconds to wait between characters under all other
 * circumstances. Currently 1.
 *
 */
#define USLEEP2 1

/**
 * HDR_FIXED_LEN
 *
 * Length of fixed part of header for uploading a file.
 *
 */
#define HDR_FIXED_LEN 30

/**
 * DATA_BLOCK
 *
 * Maximum length of a data block to upload through the serial port.
 *
 */
#define DATA_BLOCK 1536

/* Defines for error handling */
/* #define NOERROR		0 */
/* #define ERROR_RECEIVED	1 */
/* #define ERROR_ADDRESSED	2 */
/* #define FATAL_ERROR	3 */
/* #define ERROR_LOWBATT	4 */

/**
 * canonSerialErrorCode:
 * @NOERROR: No error
 * @ERROR_RECEIVED: Packet length doesn't match received packet
 * @ERROR_ADDRESSED: Problem receiving EOT
 * @FATAL_ERROR: Fatal error
 * @ERROR_LOWBATT: Battery is low
 *
 * Used within serial code to signal various error conditions.
 *
 */
#undef NOERROR /* happens in windows. is also 0 */
typedef enum {
	NOERROR		= 0,
	ERROR_RECEIVED	= 1,
	ERROR_ADDRESSED	= 2,
	FATAL_ERROR	= 3,
	ERROR_LOWBATT   = 4
} canonSerialErrorCode;

/* ------------------------- Frame-level processing ------------------------- */

/* #define CANON_FBEG      0xc0 */
/* #define CANON_FEND      0xc1 */
/* #define CANON_ESC       0x7e */
/* #define CANON_XOR       0x20 */
/**
 * canonSerialFramingByte:
 * @CANON_FBEG: Beginning of frame
 * @CANON_FEND: End of frame
 * @CANON_ESC: XOR next byte with 0x20
 * @CANON_XOR: value to use with %CANON_ESC
 *
 * Enumeration of all "special" byte codes on the frame level.
 *
 */
typedef enum {
	CANON_FBEG    = 0xc0,		     /* Beginning of frame */
	CANON_FEND    = 0xc1,		     /* End of frame */
	CANON_ESC     = 0x7e,		     /* XOR next byte with 0x20 */
	CANON_XOR     = 0x20
} canonSerialFramingByte;
/* ------------------------ Packet-level processing ------------------------- */

/**
 * MAX_PKT_PAYLOAD
 *
 * Maximum size of a packet payload; used to allocate buffers.
 *
 */
#define MAX_PKT_PAYLOAD 65535

/* #define MAX_PKT_SIZE    (MAX_PKT_PAYLOAD+4) */

/* #define PKT_HDR_LEN     4 */

/* #define PKT_SEQ         0 */
/* #define PKT_TYPE        1 */
/* #define PKT_LEN_LSB     2 */
/* #define PKT_LEN_MSB     3 */
/**
 * canonPacketOffset:
 * @PKT_SEQ: Offset in packet to message sequence number
 * @PKT_TYPE: Offset in packet to type code
 * @PKT_LEN_LSB: Offset in packet to least-significant byte of packet length.
 * @PKT_LEN_MSB: Offset in packet to most-significant byte of packet length.
 * @PKT_HDR_LEN: Length of complete header.
 *
 * Offsets to bytes in a serial packet header.
 *
 */
typedef enum {
	PKT_SEQ       = 0,
	PKT_TYPE      = 1,
	PKT_LEN_LSB   = 2,
	PKT_LEN_MSB   = 3,
	PKT_HDR_LEN   = 4
} canonPacketOffset;

/* #define PKT_MSG         0 */
/* #define PKT_EOT         4 */
/* #define PKT_ACK         5 */

/**
 * canonPacketType:
 * @PKT_MSG: Message fragment
 * @PKT_SPD: Speed message from computer sent once, early in the
 *           initialization for the computer to ask the camera to
 *           switch to a higher speed.
 * @PKT_EOT: EOT
 * @PKT_ACK: ACK (or NAK)
 *
 * Packet type for byte 2 of packet header.
 * Unfortunately, these are mixed with %canonPacketThirdByte
 * codes to tell %canon_serial_send_packet what to do.
 *
 */
typedef enum {
	PKT_MSG       = 0,
	PKT_SPD       = 3,
	PKT_EOT       = 4,
	PKT_ACK       = 5
} canonPacketType;



/* #define PKT_NACK        255 */
/* #define PKTACK_NACK     0x01 */
/* #define PKT_UPLOAD_EOT  3 */

/**
 * canonPacketThirdByte:
 * @PKTACK_NACK: This ACK is a NACK (not acknowledged) message
 * @PKT_UPLOAD_EOT: This EOT is to end an upload
 * @PKT_NACK: this ACK is a request to retransmit
 *
 * Codes to go in the third byte of an ACK or EOT message.
 * Unfortunately, these are mixed with %canonPacketType
 * codes to tell %canon_serial_send_packet what to do.
 *
 */
typedef enum {
	PKTACK_NACK    = 0x01,
	PKT_UPLOAD_EOT = 3,
	PKT_NACK       = 255
} canonPacketThirdByte;

/* ----------------------- Message-level processing ------------------------ */


/**
 * MAX_MSG_SIZE
 *
 * Maximum size of a message to fit within a packet.
 *
 */
#define MAX_MSG_SIZE    (MAX_PKT_PAYLOAD-12)

/* #define FRAG_NUM                0 */
/* #define FRAG_LEN_LSB    2 */
/* #define FRAG_LEN_MSB    3 */

/* #define MSG_HDR_LEN     16 */
/* #define MSG_02          0 */
/* #define MSG_MTYPE       4 */
/* #define MSG_DIR         7 */
/* #define MSG_LEN_LSB     8 */
/* #define MSG_LEN_MSB     9 */

/* #define MSG_FFFB     12 */

/**
 * canonSerialMsgHeader:
 * @MSG_02: offset to bytes "00 02" in header
 * @MSG_MTYPE: offset to message type byte in header
 * @MSG_DIR : offset to message direction byte in header: 0x11 or 0x12
 *            is output to camera, 0x21 or 0x22 is response from camera
 * @MSG_LEN_LSB: offset to least-significant byte of 16-but message length
 * @MSG_LEN_MSB: offset to most-significant byte of 16-but message length
 * @MSG_HDR_LEN: length of entire message header
 *
 *
 */
typedef enum {
	MSG_02        = 0,
	MSG_MTYPE     = 4,
	MSG_DIR       = 7,
	MSG_LEN_LSB   = 8,
	MSG_LEN_MSB   = 9,
/*	MSG_FFFB      = 12,*/
	MSG_HDR_LEN   = 16
} canonSerialMsgHeader;


/**
 * DIR_REVERSE
 *
 * Value to XOR with direction byte to reverse direction.
 * Converts 0x21 -> 0x11, 0x11 -> 0x21.
 *
 */
#define DIR_REVERSE     0x30

/**
 * UPLOAD_DATA_BLOCK
 *
 * Size of blocks to upload a file.
 */
#define UPLOAD_DATA_BLOCK 900

/* ----------------------- Command-level processing ------------------------ */


/**
 * SPEED_9600
 *
 * String to send to set camera speed to 9600 bits per second.
 *
 */
#define SPEED_9600   (unsigned char *)"\x00\x03\x02\x02\x01\x10\x00\x00\x00\x00\xc0\x39"

/**
 * SPEED_19200
 *
 * String to send to set camera speed to 19200 bits per second.
 *
 */
#define SPEED_19200  (unsigned char *)"\x00\x03\x08\x02\x01\x10\x00\x00\x00\x00\x13\x1f"

/**
 * SPEED_38400
 *
 * String to send to set camera speed to 38400 bits per second.
 *
 */
#define SPEED_38400  (unsigned char *)"\x00\x03\x20\x02\x01\x10\x00\x00\x00\x00\x5f\x84"

/**
 * SPEED_57600
 *
 * String to send to set camera speed to 57600 bits per second.
 *
 */
#define SPEED_57600  (unsigned char *)"\x00\x03\x40\x02\x01\x10\x00\x00\x00\x00\x5e\x57"

/**
 * SPEED_115200
 *
 * String to send to set camera speed to 115200 bits per second.
 *
 */
#define SPEED_115200 (unsigned char *)"\x00\x03\x80\x02\x01\x10\x00\x00\x00\x00\x4d\xf9"



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
