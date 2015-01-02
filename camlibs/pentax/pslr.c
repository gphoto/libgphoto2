/*
    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _BSD_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

#ifndef LIBGPHOTO2
#include <sys/ioctl.h>
#include <linux/../scsi/sg.h>
#endif

#include <stdarg.h>
#include <dirent.h>

#ifdef LIBGPHOTO2
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-setting.h>
#endif

#include "pslr.h"

#define MAX_SEGMENTS 20
#define MAX_RESOLUTIONS 4
#define POLL_INTERVAL 100000 /* Number of us to wait when polling */
#define BLKSZ 65536 /* Block size for downloads; if too big, we get
                     * memory allocation error from sg driver */
#define BLOCK_RETRY 3 /* Number of retries, since we can occasionally
                       * get SCSI errors when downloading data */

#ifdef DEBUG
#define DPRINT(x...) printf(x)
#else
#define DPRINT(x...) do { } while (0)
#endif

#define CHECK(x) do {                           \
        int __r;                                \
        __r = (x);                                                      \
        if (__r != PSLR_OK) {                                           \
            fprintf(stderr, "%s:%d:%s failed: %d\n", __FILE__, __LINE__, #x, __r); \
            return __r;                                                 \
        }                                                               \
    } while (0)

#define CHECK_CLN(x,rval,label) do {            \
        int __r;                                                        \
        __r = (x);                                                      \
        if (__r != PSLR_OK) {                                           \
            fprintf(stderr, "%s:%d:%s failed: %d\n", __FILE__, __LINE__, #x, __r); \
            rval = __r;                                                 \
            goto label;                                                 \
        }                                                               \
    } while (0)

#define PSLR_SUPPORTED_IMAGE_FORMAT_JPEG (1 << PSLR_IMAGE_FORMAT_JPEG)
#define PSLR_SUPPORTED_IMAGE_FORMAT_RAW (1 << PSLR_IMAGE_FORMAT_RAW)
#define PSLR_SUPPORTED_IMAGE_FORMAT_RAW_PLUS (1 << PSLR_IMAGE_FORMAT_RAW_PLUS)

typedef enum {
    PSLR_BUFFER_SEGMENT_LAST = 2,
    PSLR_BUFFER_SEGMENT_INNER = 3,
    PSLR_BUFFER_SEGMENT_OFFS = 4,
} pslr_segment_type_t;

typedef struct {
    uint32_t a;
    uint32_t type;
    uint32_t addr;
    uint32_t length;
} pslr_buffer_segment_info;

typedef struct {
    uint32_t offset;
    uint32_t addr;
    uint32_t length;
} ipslr_segment_t;

typedef struct {
    uint32_t id1;
    uint32_t id2;
    const char *name;
    const char *resolution_steps[MAX_RESOLUTIONS];
    int supported_formats;
} ipslr_model_info_t;

typedef struct {
#ifdef LIBGPHOTO2
    GPPort *port;
#else
    int fd;
#endif
    pslr_status status;
    uint32_t id1;
    uint32_t id2;
    ipslr_model_info_t *model;
    char devname[256];
    ipslr_segment_t segments[MAX_SEGMENTS];
    uint32_t buffer_len;
    uint32_t segment_count;
    uint32_t offset;
} ipslr_handle_t;

static int ipslr_set_mode(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_00_09(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_10_0a(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_00_05(ipslr_handle_t *p);
static int ipslr_status(ipslr_handle_t *p, uint8_t *buf);
static int ipslr_status_full(ipslr_handle_t *p, pslr_status *status);
static int ipslr_press_shutter(ipslr_handle_t *p);
static int ipslr_select_buffer(ipslr_handle_t *p, int bufno, int buftype, int bufres);
static int ipslr_buffer_segment_info(ipslr_handle_t *p, pslr_buffer_segment_info *pInfo);
static int ipslr_read_buffer(pslr_handle_t h, int bufno, int buftype, int bufres,
                             uint8_t **ppData, uint32_t *pLen);
static int ipslr_next_segment(ipslr_handle_t *p);
static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf);
static int ipslr_identify(ipslr_handle_t *p);
static int ipslr_write_args(ipslr_handle_t *p, int n, ...);

/*static int ipslr_cmd_00_04(ipslr_handle_t *p, uint32_t mode);*/

static int command(ipslr_handle_t *, int a, int b, int c);
static int get_status(ipslr_handle_t *);
static int get_result(ipslr_handle_t *);
static int read_result(ipslr_handle_t *, uint8_t *buf, uint32_t n);

void hexdump(uint8_t *buf, uint32_t bufLen);

static int scsi_write(ipslr_handle_t *, uint8_t *cmd, uint32_t cmdLen,
                      uint8_t *buf, uint32_t bufLen);
static int scsi_read(ipslr_handle_t *, uint8_t *cmd, uint32_t cmdLen,
                     uint8_t *buf, uint32_t bufLen);


static uint32_t get_uint32(uint8_t *buf);

static bool is_k10d(ipslr_handle_t *p);
static bool is_k20d(ipslr_handle_t *p);
static bool is_k30(ipslr_handle_t *p);
static bool is_k100ds(ipslr_handle_t *p);
static bool is_istds(ipslr_handle_t *p);

static pslr_progress_callback_t progress_callback = NULL;

static ipslr_model_info_t camera_models[] = {
    { PSLR_ID1_K30, PSLR_ID2_K30, "K30", { "16", "12", "8", "5" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_K20D, PSLR_ID2_K20D, "K20D", { "14", "10", "6", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_K10D, PSLR_ID2_K10D, "K10D", { "10", "6", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_K110D, PSLR_ID2_K110D, "K110D", { "6", "4", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_K100D, PSLR_ID2_K100D, "K100D", { "6", "4", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_K100DS, PSLR_ID2_K100DS, "K100DS", {}, PSLR_SUPPORTED_IMAGE_FORMAT_RAW },
    { PSLR_ID1_IST_DS2, PSLR_ID2_IST_DS2, "*ist DS2", { "6", "4", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_IST_DL, PSLR_ID2_IST_DL, "*ist DL", { "6", "4", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_IST_DS, PSLR_ID2_IST_DS, "*ist DS", { "6", "4", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_IST_D, PSLR_ID2_IST_D, "*ist D", { "6", "4", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_GX10, PSLR_ID2_GX10, "GX10", { "10", "6", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
    { PSLR_ID1_GX20, PSLR_ID2_GX20, "GX20", { "14", "10", "6", "2" }, PSLR_SUPPORTED_IMAGE_FORMAT_JPEG },
};

#ifndef LIBGPHOTO2
static ipslr_handle_t pslr;

pslr_handle_t pslr_init()
{
    DIR *d;
    char nmbuf[256];
    char infobuf[64];
    struct dirent *ent;
    int fd;

    memset(&pslr.devname, 0, sizeof(pslr.devname));

    d = opendir("/sys/class/scsi_generic");

    if (!d)
        return NULL;

    while (1) {
        ent = readdir(d);
        if (!ent)
            break;
        if (strcmp(ent->d_name, ".")==0 || strcmp(ent->d_name, "..") == 0)
            continue;
        snprintf(nmbuf, sizeof(nmbuf), "/sys/class/scsi_generic/%s/device/vendor", ent->d_name);
        fd = open(nmbuf, O_RDONLY);
        if (fd == -1) {
            continue;
        }
        read(fd, infobuf, sizeof(infobuf));
        close(fd);

        if ((strncmp(infobuf, "PENTAX", 6) != 0) && (strncmp(infobuf, "SAMSUNG", 7) != 0))

            continue;

        snprintf(nmbuf, sizeof(nmbuf), "/sys/class/scsi_generic/%s/device/model", ent->d_name);
        fd = open(nmbuf, O_RDONLY);
        if (fd == -1) {
            continue;
        }
        read(fd, infobuf, sizeof(infobuf));
        close(fd);

        if (!(strncmp(infobuf, "DIGITAL_CAMERA", 14) == 0
	      || strncmp(infobuf, "DSC_K20D", 8) == 0)
              || strncmp(infobuf, "DSC_K-30", 8) == 0) {
            continue;
        }

        /* Found PENTAX DIGITAL_CAMERA */
        snprintf(pslr.devname, sizeof(pslr.devname), "/dev/%s", ent->d_name);
        pslr.devname[sizeof(pslr.devname)-1] = '\0';

        /* Only support first connected camera at this time. */
        break;

    }

    closedir(d);
    if (pslr.devname[0] == '\0')
        return NULL;

    pslr.fd = open(pslr.devname, O_RDWR);
    if (pslr.fd == -1) {
        return NULL;
    }

    return &pslr;
}
#else
pslr_handle_t
pslr_init(GPPort *port)
{
	ipslr_handle_t *p = calloc(sizeof(ipslr_handle_t),1);
	if (p)
		p->port = port;
	return p;
}
#endif

int pslr_connect(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    uint8_t statusbuf[28];
    CHECK(ipslr_status(p, statusbuf));
    CHECK(ipslr_set_mode(p, 1));
    CHECK(ipslr_status(p, statusbuf));
    CHECK(ipslr_identify(p));
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("init bufmask=0x%x\n", p->status.bufmask);
    if (is_k10d(p) || is_k20d(p) || is_k30(p) || is_k100ds(p))
        CHECK(ipslr_cmd_00_09(p, 2));
    CHECK(ipslr_status_full(p, &p->status));
    CHECK(ipslr_cmd_10_0a(p, 1));
    if (!is_k30(p) && is_istds(p)) {
        CHECK(ipslr_cmd_00_05(p));
    }

    CHECK(ipslr_status_full(p, &p->status));
    return 0;
}

int pslr_disconnect(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    uint8_t statusbuf[28];
    CHECK(ipslr_cmd_10_0a(p, 0));
    CHECK(ipslr_set_mode(p, 0));
    CHECK(ipslr_status(p, statusbuf));
    return PSLR_OK;
}

int pslr_shutdown(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
#ifdef LIBGPHOTO2
	/* FIXME: close camera? */
    gp_port_close (p->port);
#else
    close(p->fd);
#endif
    return PSLR_OK;
}

int pslr_shutter(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    ipslr_press_shutter(p);
    return PSLR_OK;
}

int pslr_focus(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 1, 1));
    CHECK(command(p, 0x10, 0x05, 0x04));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_get_status(pslr_handle_t h, pslr_status *ps)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_status_full(p, &p->status));
    memcpy(ps, &p->status, sizeof(pslr_status));
    return PSLR_OK;
}

int pslr_get_buffer(pslr_handle_t h, int bufno, int type, int resolution, 
                    uint8_t **ppData, uint32_t *pLen)
{
    CHECK(ipslr_read_buffer(h, bufno, type, resolution, ppData, pLen));
    return PSLR_OK;
}

int pslr_set_progress_callback(pslr_handle_t h, pslr_progress_callback_t cb, uintptr_t user_data)
{
    progress_callback = cb;
    return PSLR_OK;
}

int pslr_set_shutter(pslr_handle_t h, pslr_rational_t value)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 2, value.nom, value.denom));
    CHECK(command(p, 0x18, 0x16, 0x08));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_set_aperture(pslr_handle_t h, pslr_rational_t value)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 3, value.nom, value.denom, 0));
    CHECK(command(p, 0x18, 0x17, 0x0c));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_set_iso(pslr_handle_t h, uint32_t value)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    /* TODO: if cmd 00 09 fails? */
    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 3, value, 0, 0));
    CHECK(command(p, 0x18, 0x15, 0x0c));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));
    return PSLR_OK;
}

int pslr_set_ec(pslr_handle_t h, pslr_rational_t value)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    /* TODO: if cmd 00 09 fails? */
    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 3, value.nom, value.denom));
    CHECK(command(p, 0x18, 0x18, 0x08));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));
    return PSLR_OK;
}

int pslr_set_jpeg_quality(pslr_handle_t h, pslr_jpeg_quality_t quality)
{
    int hwqual;
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (quality >= PSLR_JPEG_QUALITY_MAX)
        return PSLR_PARAM;
    if (is_k20d(p))
    {
	hwqual = quality;
    }
    else if (is_k30(p))
    {
        // max_qual - hwqual = number of stars (on the lcd)
        hwqual = abs(quality-3);
    }
    else
    {
	hwqual = quality-1;
    }
    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 2, 1, hwqual));
    CHECK(command(p, 0x18, 0x13, 0x08));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));
    return PSLR_OK;
}

int pslr_set_jpeg_resolution(pslr_handle_t h, int resolution)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (resolution >= PSLR_MAX_RESOLUTIONS)
        return PSLR_PARAM;	

    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 2, 1, resolution));
    CHECK(command(p, 0x18, 0x14, 0x08));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));
    return PSLR_OK;
}

int pslr_set_jpeg_image_mode(pslr_handle_t h, pslr_jpeg_image_mode_t image_mode)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (image_mode < 0 || image_mode > PSLR_JPEG_IMAGE_MODE_MAX)
        return PSLR_PARAM;

    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 1, image_mode));
    CHECK(command(p, 0x18, 0x1b, 0x04));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));
    return PSLR_OK;
}

int pslr_set_jpeg_sharpness(pslr_handle_t h, int32_t sharpness)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (sharpness < 0 || sharpness > 6)
        return PSLR_PARAM;
    CHECK(ipslr_write_args(p, 2, 0, sharpness));
    CHECK(command(p, 0x18, 0x21, 0x08));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_set_jpeg_contrast(pslr_handle_t h, int32_t contrast)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (contrast < 0 || contrast > 6)
        return PSLR_PARAM;
    CHECK(ipslr_write_args(p, 2, 0, contrast));
    CHECK(command(p, 0x18, 0x22, 0x08));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_set_jpeg_saturation(pslr_handle_t h, int32_t saturation)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (saturation < 0 || saturation > 6)
        return PSLR_PARAM;
    CHECK(ipslr_write_args(p, 2, 0, saturation));
    CHECK(command(p, 0x18, 0x20, 0x08));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_set_image_format(pslr_handle_t h, pslr_image_format_t format)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format < 0 || format > PSLR_IMAGE_FORMAT_MAX)
        return PSLR_PARAM;
    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 2, 1, format));
    CHECK(command(p, 0x18, 0x12, 0x08));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));    
    return PSLR_OK;
}

int pslr_is_image_format_supported(pslr_handle_t h, pslr_image_format_t format)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format < 0 || format >= PSLR_IMAGE_FORMAT_MAX || !p->model)
        return false;
    return (1 << format) & p->model->supported_formats;
}

int pslr_set_raw_format(pslr_handle_t h, pslr_raw_format_t format)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format < 0 || format > PSLR_RAW_FORMAT_MAX)
        return PSLR_PARAM;
    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 2, 1, format));
    CHECK(command(p, 0x18, 0x1f, 0x08));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));    
    return PSLR_OK;
}

int pslr_delete_buffer(pslr_handle_t h, int bufno)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (bufno < 0 || bufno > 9)
        return PSLR_PARAM;
    CHECK(ipslr_write_args(p, 1, bufno));
    CHECK(command(p, 0x02, 0x03, 0x04));
    CHECK(get_status(p));
    return PSLR_OK;    
}

int pslr_green_button(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(command(p, 0x10, 0x07, 0x00));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_ae_lock(pslr_handle_t h, bool lock)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (lock)
        CHECK(command(p, 0x10, 0x06, 0x00));
    else
        CHECK(command(p, 0x10, 0x08, 0x00));
    CHECK(get_status(p));
    return PSLR_OK;
}

int pslr_set_exposure_mode(pslr_handle_t h, pslr_exposure_mode_t mode)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;

    if (mode < 0 || mode >= PSLR_EXPOSURE_MODE_MAX)
        return PSLR_PARAM;

    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 2, 1, mode));
    CHECK(command(p, 0x18, 0x01, 0x08));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));    
    return PSLR_OK;
}

int pslr_buffer_open(pslr_handle_t h, int bufno, int buftype, int bufres)
{
    pslr_buffer_segment_info info;
    uint16_t bufs;
    uint32_t buf_total = 0;
    int i, j;
    int ret;
    int retry = 0;
    int retry2 = 0;

    ipslr_handle_t *p = (ipslr_handle_t *) h;

    memset(&info, 0, sizeof(info));

    CHECK(ipslr_status_full(p, &p->status));
    bufs = p->status.bufmask;
    if ((bufs & (1<<bufno)) == 0) {
        DPRINT("No buffer data (%d)\n", bufno);
        return PSLR_READ_ERROR;
    }

    while (retry < 3) {
        /* If we get response 0x82 from the camera, there is a
         * desynch. We can recover by stepping through segment infos
         * until we get the last one (type = 2). Retry up to 3 times. */
        ret = ipslr_select_buffer(p, bufno, buftype, bufres);
        if (ret == PSLR_OK)
            break;

        retry++;
        retry2 = 0;
        do {
            CHECK(ipslr_buffer_segment_info(p, &info));
            CHECK(ipslr_next_segment(p));
            DPRINT("Recover: type=%d\n", info.type);
        } while (++retry2 < 100 && info.type != PSLR_BUFFER_SEGMENT_LAST);
    }

    if (retry == 3)
        return ret;

    i = 0;
    j = 0;
    do {
        CHECK(ipslr_buffer_segment_info(p, &info));
        DPRINT("%d: addr: 0x%x len: %d type=%d\n", i, info.addr, info.length, info.type);
        if (info.type == PSLR_BUFFER_SEGMENT_OFFS)
            p->segments[j].offset = info.length;
        else if (info.type == PSLR_BUFFER_SEGMENT_INNER) {
            if (j == MAX_SEGMENTS) {
                DPRINT("Too many segments.\n");
                return PSLR_NO_MEMORY;
            }
            p->segments[j].addr = info.addr;
            p->segments[j].length = info.length;
            j++;
        } else {
            DPRINT("Unknown segment type %d\n", info.type);
        }
        CHECK(ipslr_next_segment(p));
        buf_total += info.length;
        i++;
    } while (i < 100 && info.type != PSLR_BUFFER_SEGMENT_LAST);
    p->segment_count = j;
    p->buffer_len = buf_total;
    p->offset = 0;
    return PSLR_OK;
}

uint32_t pslr_buffer_read(pslr_handle_t h, uint8_t *buf, uint32_t size)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    unsigned int i;
    uint32_t pos = 0;
    uint32_t seg_offs;
    uint32_t addr;
    uint32_t blksz;
    int ret;

    /* Find current segment */
    for (i=0; i<p->segment_count; i++) {
        if (p->offset < pos + p->segments[i].length)
            break;
        pos += p->segments[i].length;
    }

    seg_offs = p->offset - pos;
    addr = p->segments[i].addr + seg_offs;

    /* Compute block size */
    blksz = size;
    if (blksz > p->segments[i].length - seg_offs)
        blksz = p->segments[i].length - seg_offs;
    if (blksz > BLKSZ)
        blksz = BLKSZ;

    /*printf("File offset %d segment: %d offset %d address 0x%x read size %d\n", p->offset, 
     *       i, seg_offs, addr, blksz); */
    
    ret = ipslr_download(p, addr, blksz, buf);
    if (ret != PSLR_OK)
        return 0;
    p->offset += blksz;
    return blksz;
}

uint32_t pslr_buffer_get_size(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    unsigned int i;
    uint32_t len = 0;
    for (i=0; i<p->segment_count; i++) {
        len += p->segments[i].length;
    }
    return len;
}

void pslr_buffer_close(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset(&p->segments[0], 0, sizeof(p->segments));
    p->offset = 0;
    p->segment_count = 0;
}

int pslr_select_af_point(pslr_handle_t h, uint32_t point)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 1, point));
    CHECK(command(p, 0x18, 0x07, 0x04));
    CHECK(get_status(p));
    CHECK(ipslr_cmd_00_09(p, 2));        
    return PSLR_OK;
}

const char *pslr_camera_name(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int ret;
    if (p->id1 == 0) {
        ret = ipslr_identify(p);
        if (ret != PSLR_OK)
            return NULL;
    }
    if (p->model)
        return p->model->name;
    else {
        static char unk_name[256];
        snprintf(unk_name, sizeof(unk_name), "ID#%x:%x", p->id1, p->id2);
        unk_name[sizeof(unk_name)-1] = '\0';
        return unk_name;
    }
}

const char **pslr_camera_resolution_steps(pslr_handle_t h)
{
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (p->model)
      return p->model->resolution_steps;
    return NULL;
}

/* ----------------------------------------------------------------------- */

static int ipslr_set_mode(ipslr_handle_t *p, uint32_t mode)
{
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p, 0, 0, 4));
    CHECK(get_status(p));
    return PSLR_OK;
}

static int ipslr_cmd_00_09(ipslr_handle_t *p, uint32_t mode)
{
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p, 0, 9, 4));
    CHECK(get_status(p));
    return PSLR_OK;
}

static int ipslr_cmd_10_0a(ipslr_handle_t *p, uint32_t mode)
{
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p, 0x10, 0x0a, 4));
    CHECK(get_status(p));
    return PSLR_OK;
}

static int ipslr_cmd_00_05(ipslr_handle_t *p)
{
    int n;
    uint8_t buf[0xb8];
    CHECK(command(p, 0x00, 0x05, 0x00));
    n = get_result(p);
    if (n != 0xb8) {
        DPRINT("only got %d bytes\n", n);
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p, buf, n));
    return PSLR_OK;
}

static int ipslr_status(ipslr_handle_t *p, uint8_t *buf)
{
    int n;
    CHECK(command(p, 0, 1, 0));
    n = get_result(p);
    if (n == 16 || n == 28) {
        return read_result(p, buf, n);
    } else {
        return PSLR_READ_ERROR;
    }
}

#define MAX_STATUS_BUF_SIZE 452
#ifdef DEBUG
static uint8_t lastbuf[MAX_STATUS_BUF_SIZE];
static int first = 1;

static void ipslr_status_diff(uint8_t *buf)
{
    int n;
    int diffs;
    if (first)
    {
	hexdump(buf, MAX_STATUS_BUF_SIZE);
	memcpy(lastbuf, buf, MAX_STATUS_BUF_SIZE);
	first = 0;
    }

    diffs = 0;
    for (n = 0; n < MAX_STATUS_BUF_SIZE; n++)
    {
	if (lastbuf[n] != buf[n])
	{
	    DPRINT("buf[%03X] last %02Xh %3d new %02Xh %3d\n", n, lastbuf[n], lastbuf[n], buf[n], buf[n]);
	    diffs++;
	}
    }
    if (diffs)
    {
	DPRINT("---------------------------\n");
	memcpy(lastbuf, buf, MAX_STATUS_BUF_SIZE);
    }
}
#endif

static int ipslr_status_full(ipslr_handle_t *p, pslr_status *status)
{
    int n;
    uint8_t buf[MAX_STATUS_BUF_SIZE];
    CHECK(command(p, 0, 8, 0));
    n = get_result(p);

    if (p->model && is_k10d(p)) {
        /* K10D status block */
        if (n != 392)  {
            DPRINT("only got %d bytes\n", n);
            return PSLR_READ_ERROR;
        }

        CHECK(read_result(p, buf, n));
        memset(status, 0, sizeof(*status));
        status->bufmask = buf[0x16] << 8 | buf[0x17];
        status->current_iso = get_uint32(&buf[0x11c]);
        status->current_shutter_speed.nom = get_uint32(&buf[0xf4]);
        status->current_shutter_speed.denom = get_uint32(&buf[0xf8]);
        status->current_aperture.nom = get_uint32(&buf[0xfc]);
        status->current_aperture.denom = get_uint32(&buf[0x100]);
        status->lens_min_aperture.nom = get_uint32(&buf[0x12c]);
        status->lens_min_aperture.denom = get_uint32(&buf[0x130]);
        status->lens_max_aperture.nom = get_uint32(&buf[0x134]);
        status->lens_max_aperture.denom = get_uint32(&buf[0x138]);
        status->current_zoom.nom = get_uint32(&buf[0x16c]);
        status->current_zoom.denom = get_uint32(&buf[0x170]);
        status->set_aperture.nom = get_uint32(&buf[0x34]);
        status->set_aperture.denom = get_uint32(&buf[0x38]);
        status->set_shutter_speed.nom = get_uint32(&buf[0x2c]);
        status->set_shutter_speed.denom = get_uint32(&buf[0x30]);
        status->set_iso = get_uint32(&buf[0x60]);
        status->jpeg_resolution = get_uint32(&buf[0x7c]);
        status->jpeg_contrast = get_uint32(&buf[0x94]);
        status->jpeg_sharpness = get_uint32(&buf[0x90]);
        status->jpeg_saturation = get_uint32(&buf[0x8c]);
        status->jpeg_quality = 1+get_uint32(&buf[0x80]);
        status->jpeg_image_mode = get_uint32(&buf[0x88]);
        status->zoom.nom = get_uint32(&buf[0x16c]);
        status->zoom.denom = get_uint32(&buf[0x170]);
        status->focus = get_uint32(&buf[0x174]);
        status->raw_format = get_uint32(&buf[0x84]);
        status->image_format = get_uint32(&buf[0x78]);
        status->light_meter_flags = get_uint32(&buf[0x124]);
        status->ec.nom = get_uint32(&buf[0x3c]);
        status->ec.denom = get_uint32(&buf[0x40]);
        status->custom_ev_steps = get_uint32(&buf[0x9c]);
        status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
        status->exposure_mode = get_uint32(&buf[0xe0]);
        status->user_mode_flag = get_uint32(&buf[0x1c]);
        status->af_point_select = get_uint32(&buf[0xbc]);
        status->selected_af_point = get_uint32(&buf[0xc0]);
        status->focused_af_point = get_uint32(&buf[0x150]);
        return PSLR_OK;
    }

    if (p->model && is_k20d(p)) {
        /* K20D status block */
        if (n != 412)  {
            DPRINT("only got %d bytes\n", n);
            return PSLR_READ_ERROR;
        }

        CHECK(read_result(p, buf, n));
#ifdef DEBUG
	ipslr_status_diff(buf);
#endif
        memset(status, 0, sizeof(*status));
        status->bufmask = buf[0x16] << 8 | buf[0x17]; 
	status->current_iso = get_uint32(&buf[0x130]); /*d*/
        status->current_shutter_speed.nom = get_uint32(&buf[0x108]); /*d*/
        status->current_shutter_speed.denom = get_uint32(&buf[0x10C]); /*d*/
        status->current_aperture.nom = get_uint32(&buf[0x110]); /*d*/
        status->current_aperture.denom = get_uint32(&buf[0x114]); /*d*/
        status->lens_min_aperture.nom = get_uint32(&buf[0x140]); /*d*/
        status->lens_min_aperture.denom = get_uint32(&buf[0x144]); /*d*/
        status->lens_max_aperture.nom = get_uint32(&buf[0x148]); /*d*/
        status->lens_max_aperture.denom = get_uint32(&buf[0x14B]); /*d*/
        status->current_zoom.nom = get_uint32(&buf[0x180]); /*d*/
        status->current_zoom.denom = get_uint32(&buf[0x184]); /*d*/
        status->set_aperture.nom = get_uint32(&buf[0x34]); /*d*/
        status->set_aperture.denom = get_uint32(&buf[0x38]); /*d*/
        status->set_shutter_speed.nom = get_uint32(&buf[0x2c]); /*d*/
        status->set_shutter_speed.denom = get_uint32(&buf[0x30]); /*d*/
        status->set_iso = get_uint32(&buf[0x60]); /*d*/
        status->jpeg_resolution = get_uint32(&buf[0x7c]); /*d*/
        status->jpeg_contrast = get_uint32(&buf[0x94]); /* commands do now work for it?*/
        status->jpeg_sharpness = get_uint32(&buf[0x90]); /* commands do now work for it?*/
        status->jpeg_saturation = get_uint32(&buf[0x8c]); /* commands do now work for it?*/
        status->jpeg_quality = get_uint32(&buf[0x80]); /*d*/
        status->jpeg_image_mode = get_uint32(&buf[0x88]); /*d*/
        status->zoom.nom = get_uint32(&buf[0x180]); /*d*/
        status->zoom.denom = get_uint32(&buf[0x184]); /*d*/
        status->focus = get_uint32(&buf[0x188]); /*d current focus ring position?*/
        status->raw_format = get_uint32(&buf[0x84]); /*d*/
        status->image_format = get_uint32(&buf[0x78]); /*d*/
        status->light_meter_flags = get_uint32(&buf[0x138]); /*d*/
        status->ec.nom = get_uint32(&buf[0x3c]); /*d*/
        status->ec.denom = get_uint32(&buf[0x40]); /*d*/
        status->custom_ev_steps = get_uint32(&buf[0x9c]);
        status->custom_sensitivity_steps = get_uint32(&buf[0xa0]);
        status->exposure_mode = get_uint32(&buf[0xe0]); /*d*/
        status->user_mode_flag = get_uint32(&buf[0x1c]); /*d*/
        status->af_point_select = get_uint32(&buf[0xbc]); /* not sure*/
        status->selected_af_point = get_uint32(&buf[0xc0]); /*d*/
        status->focused_af_point = get_uint32(&buf[0x160]); /*d, unsure about it, a lot is changing when the camera focuses*/
	/* 0x158 current ev?
	 / 0xB8 0 - MF, 1 - AF.S, 2 - AF.C
	 / 0xB4, 0xC4 - metering mode, 0 - matrix, 1 - center weighted, 2 - spot
	 / 0x160 and 0x164 change when AF
	 / 0xC0 changes when selecting focus point manually or from GUI
	 / 0xBC focus point selection 0 - auto, 1 - manual, 2 - center
         */
        return PSLR_OK;
    }

    if (p->model && is_k30(p)) { // might work with k01 too
        if (n != 452)  {
            DPRINT("only got %d bytes\n", n);
            return PSLR_READ_ERROR;
        }
        CHECK(read_result(p, buf, n));
#ifdef DEBUG
        ipslr_status_diff(buf);
#endif
        memset(status, 0, sizeof(*status));
        status->bufmask = get_uint32(&buf[0x1E]);
        status->current_iso = get_uint32(&buf[0x134]);
        status->current_shutter_speed.nom = get_uint32(&buf[0x10C]);
        status->current_shutter_speed.denom = get_uint32(&buf[0x110]);
        status->current_aperture.nom = get_uint32(&buf[0x114]);
        status->current_aperture.denom = get_uint32(&buf[0x118]);
        status->lens_min_aperture.nom = get_uint32(&buf[0x144]);
        status->lens_min_aperture.denom = get_uint32(&buf[0x148]);
        status->lens_max_aperture.nom = get_uint32(&buf[0x14C]);
        status->lens_max_aperture.denom = get_uint32(&buf[0x150]);
        status->current_zoom.nom = get_uint32(&buf[0x1A0]);
        status->current_zoom.denom = get_uint32(&buf[0x1A4]);
        status->set_aperture.nom = get_uint32(&buf[0x3C]);
        status->set_aperture.denom = get_uint32(&buf[0x40]);
        status->set_shutter_speed.nom = get_uint32(&buf[0x34]);
        status->set_shutter_speed.denom = get_uint32(&buf[0x38]);
        status->set_iso = get_uint32(&buf[0x14]);
        status->jpeg_resolution = get_uint32(&buf[0x84]);
        status->jpeg_contrast = get_uint32(&buf[0x9C]);
        status->jpeg_sharpness = get_uint32(&buf[0x98]);
        status->jpeg_saturation = get_uint32(&buf[0x94]);
        status->jpeg_quality = 3-get_uint32(&buf[0x88]); // Maximum quality is 3
        status->jpeg_image_mode = get_uint32(&buf[0x80]);
        status->zoom.nom = get_uint32(&buf[0x1A0]);
        status->zoom.denom = get_uint32(&buf[0x1A4]);
        status->focus = get_uint32(&buf[0x1A8]);
        status->raw_format = get_uint32(&buf[0x8C]);
        status->image_format = get_uint32(&buf[0x80]);
        status->light_meter_flags = get_uint32(&buf[0x13C]);
        status->ec.nom = get_uint32(&buf[0x44]);
        status->ec.denom = get_uint32(&buf[0x48]);
        status->custom_ev_steps = get_uint32(&buf[0x15C]);
        status->custom_sensitivity_steps = get_uint32(&buf[0xa8]);
        status->exposure_mode = get_uint32(&buf[0xb4]);
        status->user_mode_flag = get_uint32(&buf[0x24]);
        status->af_point_select = get_uint32(&buf[0xc4]);
        status->selected_af_point = get_uint32(&buf[0xc0]);
        status->focused_af_point = get_uint32(&buf[0x168]);
        return PSLR_OK;
    }

    if (p->model && is_k100ds(p)) {
        /* K100DS (Super) status block */
        if (n != 264)  {
            DPRINT("only got %d bytes\n", n);
            return PSLR_READ_ERROR;
        }

        CHECK(read_result(p, buf, n));
        memset(status, 0, sizeof(*status));

        status->bufmask = get_uint32(&buf[0x10]);
        status->image_format = PSLR_IMAGE_FORMAT_RAW;
        status->raw_format = PSLR_RAW_FORMAT_PEF;

        return PSLR_OK;
    }

    if (p->model && is_istds(p)) {
        /* *ist DS status block */
        if (n != 0x108) {
            DPRINT("only got %d bytes\n", n);
            return PSLR_READ_ERROR;
        }
        CHECK(read_result(p, buf, n));
#ifdef DEBUG
        ipslr_status_diff(buf);
#endif
        memset(status, 0, sizeof(*status));
        status->bufmask = get_uint32(&buf[0x10]);
        status->set_shutter_speed.nom = get_uint32(&buf[0x80]);
        status->set_shutter_speed.denom = get_uint32(&buf[0x84]);
        status->set_aperture.nom = get_uint32(&buf[0x88]);
        status->set_aperture.denom = get_uint32(&buf[0x8c]);
        status->lens_min_aperture.nom = get_uint32(&buf[0xb8]);
        status->lens_min_aperture.denom = get_uint32(&buf[0xbc]);
        status->lens_max_aperture.nom = get_uint32(&buf[0xc0]);
        status->lens_max_aperture.denom = get_uint32(&buf[0xc4]);

        return PSLR_OK;
    }
    /* Unknown camera */
    return PSLR_OK;
}

static int ipslr_press_shutter(ipslr_handle_t *p)
{
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("before: mask=0x%x\n", p->status.bufmask);
    CHECK(ipslr_write_args(p, 1, 2));
    CHECK(command(p, 0x10, 0x05, 0x04));
    get_status(p);
    DPRINT("shutter result code: 0x%x\n", r);
    return PSLR_OK;
}

static int ipslr_read_buffer(pslr_handle_t h, int bufno, int buftype, int bufres,
                             uint8_t **ppData, uint32_t *pLen)
{
    uint8_t *buf = 0;
    uint8_t *buf_ptr;
    uint32_t bytes;

    if (!ppData || !pLen)
        return PSLR_PARAM;

    ipslr_handle_t *p = (ipslr_handle_t *) h;

    CHECK(pslr_buffer_open(h, bufno, buftype, bufres));

    buf = malloc(p->buffer_len);
    if (!buf)
        return PSLR_NO_MEMORY;
    buf_ptr = buf;

    do {
        bytes = pslr_buffer_read(h, buf_ptr, p->buffer_len - (buf_ptr - buf));
        buf_ptr += bytes;
    } while(bytes);
    pslr_buffer_close(h);
    *ppData = buf;
    *pLen = buf_ptr - buf;

    return PSLR_OK;
}

static int ipslr_select_buffer(ipslr_handle_t *p, int bufno, int buftype, int bufres)
{
    int r;
    DPRINT("Select buffer %d,%d,%d,0\n", bufno, buftype,bufres);
    if (is_k20d(p)) {
        CHECK(ipslr_write_args(p, 4, bufno, buftype, bufres, 0));
        CHECK(command(p, 0x02, 0x01, 0x10));
    } else if (is_k10d(p)) {
        CHECK(ipslr_write_args(p, 4, bufno, buftype, bufres-1, 0));
        CHECK(command(p, 0x02, 0x01, 0x10));
    } else {
        /* older cameras: 3-arg select buffer */
        CHECK(ipslr_write_args(p, 4, bufno, buftype, bufres));
        CHECK(command(p, 0x02, 0x01, 0x0c));
    }
    r = get_status(p);
    if (r != 0)
        return PSLR_COMMAND_ERROR;
    return PSLR_OK;
}

static int ipslr_next_segment(ipslr_handle_t *p)
{
    int r;
    CHECK(ipslr_write_args(p, 1, 0));
    CHECK(command(p, 0x04, 0x01, 0x04));
    usleep(10000); /* needed !! 100 too short, 1000 not short enough for PEF */
    r = get_status(p);
    if (r == 0)
        return PSLR_OK;
    return PSLR_COMMAND_ERROR;
}

static int ipslr_buffer_segment_info(ipslr_handle_t *p, pslr_buffer_segment_info *pInfo)
{
    uint8_t buf[16];
    uint32_t n;

    CHECK(command(p, 0x04, 0x00, 0x00));
    n = get_result(p);
    if (n != 16)
        return PSLR_READ_ERROR;
    CHECK(read_result(p, buf, 16));
    pInfo->a = get_uint32(&buf[0]);
    pInfo->type = get_uint32(&buf[4]);
    pInfo->addr = get_uint32(&buf[8]);
    pInfo->length = get_uint32(&buf[12]);
    return PSLR_OK;
}

static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf)
{
    uint8_t downloadCmd[8] = { 0xf0, 0x24, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00 };
    uint32_t block;
    int n;
    int retry;
    uint32_t length_start = length;

    retry = 0;
    while (length > 0) {
        if (length > BLKSZ)
            block = BLKSZ;
        else
            block = length;

        /*DPRINT("Get 0x%x bytes from 0x%x\n", block, addr); */
        CHECK(ipslr_write_args(p, 2, addr, block));
        CHECK(command(p, 0x06, 0x00, 0x08));
        get_status(p);

        n = scsi_read(p, downloadCmd, sizeof(downloadCmd), buf, block);
	get_status(p);

        if (n < 0) {
            if (retry < BLOCK_RETRY) {
                retry++;
                continue;
            }
            return PSLR_READ_ERROR;
        }
        buf += n;
        length -= n;
        addr += n;
        retry = 0;
        if (progress_callback) {
            progress_callback(length_start-length, length_start);
        }
    }
    return PSLR_OK;
}


static int ipslr_identify(ipslr_handle_t *p)
{
    uint8_t idbuf[8];
    int n;
    unsigned int i;

    CHECK(command(p, 0, 4, 0));
    n = get_result(p);
    if (n != 8)
        return PSLR_READ_ERROR;
    CHECK(read_result(p, idbuf, 8));
    p->id1 = get_uint32(&idbuf[0]);
    p->id2 = get_uint32(&idbuf[4]);
    p->model = NULL;
    for (i=0; i<sizeof(camera_models)/sizeof(camera_models[0]); i++) {
        if (camera_models[i].id1 == p->id1) {
            p->model = &camera_models[i];
            break;
        }
    }
    return PSLR_OK;
}

static int ipslr_write_args(ipslr_handle_t *p, int n, ...)
{
    va_list ap;
    uint8_t cmd[8] = { 0xf0, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t buf[4*n];
    int res;
    int i;
    uint32_t data;

    va_start(ap, n);
    if (is_k10d(p) || is_k20d(p)) {
        /* All at once */
        for (i=0; i<n; i++) {
            data = va_arg(ap, uint32_t);
            buf[4*i+0] = data >> 24;
            buf[4*i+1] = data >> 16;
            buf[4*i+2] = data >> 8;
            buf[4*i+3] = data;
        }
        cmd[4] = 4*n;
        res = scsi_write(p, cmd, sizeof(cmd), buf, 4*n);
        if (res != PSLR_OK) {
    	    va_end(ap);
            return res;
	}
    } else {
        /* Arguments one by one */
        for (i=0; i<n; i++) {
            data = va_arg(ap, uint32_t);
            buf[0] = data >> 24;
            buf[1] = data >> 16;
            buf[2] = data >> 8;
            buf[3] = data;
            cmd[4] = 4;
            cmd[2] = i*4;
            res = scsi_write(p, cmd, sizeof(cmd), buf, 4);
            if (res != PSLR_OK) {
    	        va_end(ap);
                return res;
            }
        }
    }
    va_end(ap);
    return PSLR_OK;
}

/* ----------------------------------------------------------------------- */

static int command(ipslr_handle_t *p, int a, int b, int c)
{
    uint8_t cmd[8] = { 0xf0, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    cmd[2] = a;
    cmd[3] = b;
    cmd[4] = c;
    CHECK(scsi_write(p, cmd, sizeof(cmd), 0, 0));
    return PSLR_OK;
}

static int read_status(ipslr_handle_t *p, uint8_t *buf)
{
    uint8_t cmd[8] = { 0xf0, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    int n;

    n = scsi_read(p, cmd, 8, buf, 8);
    if (n != 8) {
        DPRINT("Only got %d bytes\n", n);
        /* The *ist DS doesn't know to return the correct number of
            read bytes for this command, so return PSLR_OK instead of
            PSLR_READ_ERROR */
        return PSLR_OK;
    }
    return PSLR_OK;
}

static int get_status(ipslr_handle_t *p)
{
    uint8_t statusbuf[8];
    while (1) {
        /*usleep(POLL_INTERVAL); */
        CHECK(read_status(p, statusbuf));
        /*DPRINT("get_status->\n"); */
        /*hexdump(statusbuf, 8); */
        if (!(statusbuf[7] & 0x01) || (statusbuf[7] & 0xfe))
            break;
        /*DPRINT("Waiting for ready - "); */
        /*hexdump(statusbuf, 8); */
        usleep(POLL_INTERVAL);
    }
    if ((statusbuf[7] & 0xff) != 0) {
        DPRINT("ERROR: 0x%x\n", statusbuf[7]);
    }
    return statusbuf[7];
}

static int get_result(ipslr_handle_t *p)
{
    uint8_t statusbuf[8];
    while (1) {
        /*DPRINT("read out status\n");*/
        CHECK(read_status(p, statusbuf));
        /*hexdump(statusbuf, 8);*/
        if (statusbuf[6] == 0x01)
            break;
        /*DPRINT("Waiting for result\n");*/
        /*hexdump(statusbuf, 8);*/
        usleep(POLL_INTERVAL);
    }
    if ((statusbuf[7] & 0xff) != 0) {
        DPRINT("ERROR: 0x%x\n", statusbuf[7]);
        return -1;
    }
    return statusbuf[0] | statusbuf[1] << 8 | statusbuf[2] << 16 | statusbuf[3];
}

static int read_result(ipslr_handle_t *p, uint8_t *buf, uint32_t n)
{
    uint8_t cmd[8] = { 0xf0, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned int r;
    cmd[4] = n;
    cmd[5] = n >> 8;
    cmd[6] = n >> 16;
    cmd[7] = n >> 24;
    r = scsi_read(p, cmd, sizeof(cmd), buf, n);
    if (r != n)
        return PSLR_READ_ERROR;
    return PSLR_OK;
}

void hexdump(uint8_t *buf, uint32_t bufLen)
{
    unsigned int i;

    for (i=0; i<bufLen; i++) {
        if (i%16 == 0)
            DPRINT("0x%04x | ", i);
        DPRINT("%02x ", buf[i]);
        if (i%8 == 7)
            DPRINT(" ");
        if (i%16 == 15)
            DPRINT("\n");
    }
    if (i%16 != 15)
        DPRINT("\n");
}


/* ----------------------------------------------------------------------- */
#ifdef LIBGPHOTO2
static int scsi_write(ipslr_handle_t *p, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen) {
	int ret;
	char sense_buffer[32];

	ret = gp_port_send_scsi_cmd (p->port, 1, (char*)cmd, cmdLen,
		sense_buffer, sizeof(sense_buffer), (char*)buf, bufLen);
	if (ret == GP_OK) return PSLR_OK;
	return PSLR_SCSI_ERROR;
}
static int scsi_read(ipslr_handle_t *p, uint8_t *cmd, uint32_t cmdLen,
              uint8_t *buf, uint32_t bufLen)
{
	int ret;
	char sense_buffer[32];

	ret = gp_port_send_scsi_cmd (p->port, 0, (char*)cmd, cmdLen,
		sense_buffer, sizeof(sense_buffer), (char*)buf, bufLen);
	if (ret == GP_OK) return bufLen;
	return -PSLR_SCSI_ERROR;
}

#else
static void print_scsi_error(sg_io_hdr_t *pIo, uint8_t *sense_buffer)
{
    int k;

    if (pIo->sb_len_wr > 0) {
        DPRINT("SCSI error: sense data: ");
        for (k = 0; k < pIo->sb_len_wr; ++k) {
            if ((k > 0) && (0 == (k % 10)))
                DPRINT("\n  ");
            DPRINT("0x%02x ", sense_buffer[k]);
        }
        DPRINT("\n");
    }
    if (pIo->masked_status)
        DPRINT("SCSI status=0x%x\n", pIo->status);
    if (pIo->host_status)
        DPRINT("host_status=0x%x\n", pIo->host_status);
    if (pIo->driver_status)
        DPRINT("driver_status=0x%x\n", pIo->driver_status);
}

static int scsi_write(ipslr_handle_t *p, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen)
{

    sg_io_hdr_t io;
    int sg_fd = p->fd;
    uint8_t sense[32];
    int r;

    memset(&io, 0, sizeof(io));

    io.interface_id = 'S';
    io.cmd_len = cmdLen;
    /* io.iovec_count = 0; */  /* memset takes care of this */
    io.mx_sb_len = sizeof(sense);
    io.dxfer_direction = SG_DXFER_TO_DEV;
    io.dxfer_len = bufLen;
    io.dxferp = buf;
    io.cmdp = cmd;
    io.sbp = sense;
    io.timeout = 20000;     /* 20000 millisecs == 20 seconds */
    /* io.flags = 0; */     /* take defaults: indirect IO, etc */
    /* io.pack_id = 0; */
    /* io.usr_ptr = NULL; */  

    r = ioctl(sg_fd, SG_IO, &io);

    if (r == -1) {
        perror("ioctl");
        return PSLR_DEVICE_ERROR;
    }

    if ((io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
        print_scsi_error(&io, sense);
        return PSLR_SCSI_ERROR;
    } else {
        return PSLR_OK;
    }
}

static int scsi_read(ipslr_handle_t *p, uint8_t *cmd, uint32_t cmdLen,
              uint8_t *buf, uint32_t bufLen)
{
    sg_io_hdr_t io;
    int sg_fd = p->fd;
    uint8_t sense[32];
    int r;

    memset(&io, 0, sizeof(io));

    io.interface_id = 'S';
    io.cmd_len = cmdLen;
    /* io.iovec_count = 0; */  /* memset takes care of this */
    io.mx_sb_len = sizeof(sense);
    io.dxfer_direction = SG_DXFER_FROM_DEV;
    io.dxfer_len = bufLen;
    io.dxferp = buf;
    io.cmdp = cmd;
    io.sbp = sense;
    io.timeout = 20000;     /* 20000 millisecs == 20 seconds */
    /* io.flags = 0; */     /* take defaults: indirect IO, etc */
    /* io.pack_id = 0; */
    /* io.usr_ptr = NULL; */  

    r = ioctl(sg_fd, SG_IO, &io);
    if (r == -1) {
        perror("ioctl");
        return -PSLR_DEVICE_ERROR;
    }

    if ((io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
        print_scsi_error(&io, sense);
        return -PSLR_SCSI_ERROR;
    } else {
        /* Older Pentax DSLR will report all bytes remaining, so make
         * a special case for this (treat it as all bytes read). */
        if (io.resid == bufLen)
            return bufLen;
        else
            return bufLen - io.resid;
    }
}
#endif /* LIBGPHOTO2 */
/* ----------------------------------------------------------------------- */

static uint32_t get_uint32(uint8_t *buf)
{
    uint32_t res;
    res = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
    return res;
}

/* ----------------------------------------------------------------------- */

static bool is_k10d(ipslr_handle_t *p)
{
    if (p->model && p->model->id1 == PSLR_ID1_K10D 
        && p->model->id2 == PSLR_ID2_K10D)
        return true;

    if (p->model && p->model->id1 == PSLR_ID1_GX10 
        && p->model->id2 == PSLR_ID2_GX10)
        return true;

    return false;
}

static bool is_k20d(ipslr_handle_t *p)
{
    if (p->model && p->model->id1 == PSLR_ID1_K20D 
        && p->model->id2 == PSLR_ID2_K20D)
        return true;
    if (p->model && p->model->id1 == PSLR_ID1_GX20
        && p->model->id2 == PSLR_ID2_GX20)
        return true;
    return false;
}

static bool is_k30(ipslr_handle_t *p)
{
    if (p->model && p->model->id1 == PSLR_ID1_K30
        && p->model->id2 == PSLR_ID2_K30)
        return true;
    return false;
}

static bool is_k100ds(ipslr_handle_t *p)
{
    if (p->model && p->model->id1 == PSLR_ID1_K100DS
        && p->model->id2 == PSLR_ID2_K100DS)
        return true;

    return false;
}

static bool is_istds(ipslr_handle_t *p)
{
    if (p->model && p->model->id1 == PSLR_ID1_IST_DS 
        && p->model->id2 == PSLR_ID2_IST_DS)
        return true;
    return false;
}
