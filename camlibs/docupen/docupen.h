#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <gd.h>

#define MAX_CALIBVALUES	8
#define SERIALNO_LEN	16

struct dp_info {
	uint8_t  s;
	uint16_t version;
	uint8_t  len;
	uint32_t memwrite;
	uint32_t pagestored;
	uint16_t res;
	uint32_t datacountbyte;
	uint8_t  flashmemcfg;
	uint16_t flashmemid;
	uint32_t flashmemmax;
	uint32_t errorcode;
	uint32_t memwritestart;
	uint8_t  packetsize[3];
	uint8_t  notch[MAX_CALIBVALUES];
	uint32_t ucorrectedcountbyte;
	uint8_t  calibvalue[MAX_CALIBVALUES];
	uint16_t checksum;
	uint16_t m_badcols[10];
	uint8_t  serialno[SERIALNO_LEN];
	uint8_t  assignedserialno;
	uint8_t  modelname[8];
	uint32_t modelwidth;
	uint8_t  encryptionsupport;
	uint8_t  maxdpi;
	uint8_t  dummy;
	uint8_t  unused[250];
} __attribute__ ((packed));

#define DP_MAGIC	0x2049

/* Vertical resolutions. Horizontal resolution is always 200dpi */
#define RES_100DPI	0x19
#define RES_200DPI	0x32
#define RES_400DPI	0x64

#define TYPE_FIRST	0x0000
#define TYPE_MONO	0x4700
#define TYPE_GREY4	0x0001
#define TYPE_GREY8	0x0002
#define TYPE_COLOR12	0x0004
#define TYPE_COLOR24	0x0008

#define PROFILE_LEN	0x400

struct dp_imagehdr {
	uint16_t magic;
	uint16_t type;
	uint8_t  unknown;	/* header size = always 0x32 */
	uint8_t  dpi;
	uint16_t sizex;
	uint16_t sizey;
	uint32_t payloadlen;
	uint32_t payloadlen2; /* ???  is payloadlen-0x20 */
	uint32_t payloadaddr;
	uint32_t nextchunk_raw;
	uint32_t nextchunk;  /* nextchunk-0x200, for USB-Mode */
	uint16_t nr;
	uint8_t  data[0];
} __attribute__ ((packed));

struct _CameraPrivateLibrary {
	struct dp_info info;
	uint32_t datalen;
	char *cache_file;
	FILE *cache;
	struct lut *lut;
	char *profile;
};

struct lut {
    unsigned char data[256];
};

bool dp_cmd(GPPort *port, const char *cmd);
bool dp_init_cache(Camera *camera);
bool dp_delete_cache(Camera *camera);
bool dp_init_calibration(Camera *camera, bool force);
gdImagePtr dp_get_image_mono(struct dp_imagehdr *dp, void *data);
gdImagePtr dp_get_image_grey(struct dp_imagehdr *dp, void *data, struct lut *lut);
gdImagePtr dp_get_image_color(struct dp_imagehdr *dp, void *data, struct lut *lut);
