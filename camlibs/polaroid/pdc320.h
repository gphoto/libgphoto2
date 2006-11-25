#ifndef __GPHOTO2_PDC320_H__
#define __GPHOTO2_PDC320_H__

#include <jpeg.h>

static const jpeg_quantization_table chrominance = {
  3,  2,  2,  3,  5,  8, 10, 12,
  2,  2,  3,  4,  5, 12, 12, 11,
  3,  3,  3,  5,  8, 11, 14, 11,
  3,  3,  4,  6, 10, 17, 16, 12,
  4,  4,  7, 11, 14, 22, 21, 15,
  5,  7, 11, 13, 16, 21, 23, 18,
 10, 13, 16, 17, 21, 24, 24, 20,
 14, 18, 19, 20, 22, 20, 21, 20
};
static const jpeg_quantization_table luminance = {
  3,  4,  5,  9, 20, 20, 20, 20,
  4,  4,  5, 13, 20, 20, 20, 20,
  5,  5, 11, 20, 20, 20, 20, 20,
  9, 13, 20, 20, 20, 20, 20, 20,
 20, 20, 20, 20, 20, 20, 20, 20,
 20, 20, 20, 20, 20, 20, 20, 20,
 20, 20, 20, 20, 20, 20, 20, 20,
 20, 20, 20, 20, 20, 20, 20, 20
};

/* ID returns 0 , XIRLINK, and 4 byte more */
#define ID_PDC640SE    "\x0XIRLINK\x2\x1\x3\x1"
#define ID_PDC320      "\x0XIRLINK\x2\x1\x3\x0"

#define PDC320_INIT	0x00
#define PDC320_ID	0x01
#define PDC320_STATE	0x02
#define PDC320_NUM	0x03
#define PDC320_SIZE	0x04
#define PDC320_PIC	0x05
#define PDC320_DEL	0x07
#define PDC320_ENDINIT	0x0a
#define PDC320_UNKNOWN3	0x0c


#define RETRIES 3

#define CR(result) {int r = (result); if (r < 0) return (r);}
#define CR_FREE(result, data) {int r = (result); if (r < 0) {free (data); return (r);}}

static struct {
	const char *model;
	unsigned char id;
} models[] = {
	{"Polaroid:Fun! 320", 0x3f},
	{"Polaroid:640SE",    0x40},
	{NULL,                0x00}
};

#define HUFF_00 33, \
    "\xFF\xC4\x00\x1F\x00\x00\x01\x05" "\x01\x01\x01\x01\x01\x01\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x01\x02" "\x03\x04\x05\x06\x07\x08\x09\x0A\x0B"

#define HUFF_10 183, \
    "\xFF\xC4\x00\xB5\x10\x00\x02\x01" "\x03\x03\x02\x04\x03\x05\x05\x04" \
    "\x04\x00\x00\x01\x7D\x01\x02\x03" "\x00\x04\x11\x05\x12\x21\x31\x41" \
    "\x06\x13\x51\x61\x07\x22\x71\x14" "\x32\x81\x91\xA1\x08\x23\x42\xB1" \
    "\xC1\x15\x52\xD1\xF0\x24\x33\x62" "\x72\x82\x09\x0A\x16\x17\x18\x19" \
    "\x1A\x25\x26\x27\x28\x29\x2A\x34" "\x35\x36\x37\x38\x39\x3A\x43\x44" \
    "\x45\x46\x47\x48\x49\x4A\x53\x54" "\x55\x56\x57\x58\x59\x5A\x63\x64" \
    "\x65\x66\x67\x68\x69\x6A\x73\x74" "\x75\x76\x77\x78\x79\x7A\x83\x84" \
    "\x85\x86\x87\x88\x89\x8A\x92\x93" "\x94\x95\x96\x97\x98\x99\x9A\xA2" \
    "\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA" "\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9" \
    "\xBA\xC2\xC3\xC4\xC5\xC6\xC7\xC8" "\xC9\xCA\xD2\xD3\xD4\xD5\xD6\xD7" \
    "\xD8\xD9\xDA\xE1\xE2\xE3\xE4\xE5" "\xE6\xE7\xE8\xE9\xEA\xF1\xF2\xF3" \
    "\xF4\xF5\xF6\xF7\xF8\xF9\xFA"
#endif
