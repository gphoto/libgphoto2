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

#define PSLR_MAX_RESOLUTIONS 4

#define PSLR_LIGHT_METER_AE_LOCK 0x8

#define PSLR_AF_POINT_TOP_LEFT   0x1
#define PSLR_AF_POINT_TOP_MID    0x2
#define PSLR_AF_POINT_TOP_RIGHT  0x4
#define PSLR_AF_POINT_FAR_LEFT   0x8
#define PSLR_AF_POINT_MID_LEFT   0x10
#define PSLR_AF_POINT_MID_MID    0x20
#define PSLR_AF_POINT_MID_RIGHT  0x40
#define PSLR_AF_POINT_FAR_RIGHT  0x80
#define PSLR_AF_POINT_BOT_LEFT   0x100
#define PSLR_AF_POINT_BOT_MID    0x200
#define PSLR_AF_POINT_BOT_RIGHT  0x400

#define PSLR_ID1_K30     0x12f52
#define PSLR_ID2_K30     0x20c
#define PSLR_ID1_K20D    0x12cd2
#define PSLR_ID2_K20D    0x1ba
#define PSLR_ID1_K10D    0x12c1e
#define PSLR_ID2_K10D    0x1a5
#define PSLR_ID1_K110D   0x12b9d
#define PSLR_ID2_K110D   0x1ac
#define PSLR_ID1_K100D   0x12b9c
#define PSLR_ID2_K100D   0x189
#define PSLR_ID1_K100DS  0x12ba2
#define PSLR_ID2_K100DS  0x189
#define PSLR_ID1_IST_D   0x12994
#define PSLR_ID2_IST_D   0x141
#define PSLR_ID1_IST_DS  0x12aa2
#define PSLR_ID2_IST_DS  0x177
#define PSLR_ID1_IST_DS2 0x12b60
#define PSLR_ID2_IST_DS2 0x19a
#define PSLR_ID1_IST_DL  0x12b1a
#define PSLR_ID2_IST_DL  0x188
#define PSLR_ID1_GX10    0x12c20
#define PSLR_ID2_GX10    0x1ad
#define PSLR_ID1_GX20    0x12cd4
#define PSLR_ID2_GX20    0x1c6

typedef enum {
    PSLR_OK = 0,
    PSLR_DEVICE_ERROR,
    PSLR_SCSI_ERROR,
    PSLR_COMMAND_ERROR,
    PSLR_READ_ERROR,
    PSLR_NO_MEMORY,
    PSLR_PARAM,                 /* Invalid parameters to API */
    PSLR_ERROR_MAX
} pslr_result;

typedef enum {
    PSLR_BUF_THUMBNAIL,
    PSLR_BUF_PREVIEW,
    PSLR_BUF_JPEG,
    PSLR_BUF_PEF
} pslr_buffer_type;

typedef enum {
    PSLR_JPEG_QUALITY_4, /* K20D only */
    PSLR_JPEG_QUALITY_3,
    PSLR_JPEG_QUALITY_2,
    PSLR_JPEG_QUALITY_1,
    PSLR_JPEG_QUALITY_MAX
} pslr_jpeg_quality_t;

typedef enum {
    PSLR_JPEG_IMAGE_MODE_NATURAL,
    PSLR_JPEG_IMAGE_MODE_BRIGHT,
    PSLR_JPEG_IMAGE_MODE_MAX
} pslr_jpeg_image_mode_t;

typedef enum {
    PSLR_RAW_FORMAT_PEF,
    PSLR_RAW_FORMAT_DNG,
    PSLR_RAW_FORMAT_MAX
} pslr_raw_format_t;

typedef enum {
    PSLR_IMAGE_FORMAT_JPEG,
    PSLR_IMAGE_FORMAT_RAW,
    PSLR_IMAGE_FORMAT_RAW_PLUS,
    PSLR_IMAGE_FORMAT_MAX
} pslr_image_format_t;

typedef enum {
    PSLR_CUSTOM_EV_STEPS_1_2,
    PSLR_CUSTOM_EV_STEPS_1_3,
    PSLR_CUSTOM_EV_STEPS_MAX
} pslr_custom_ev_steps_t;

typedef enum {
    PSLR_CUSTOM_SENSITIVITY_STEPS_1EV,
    PSLR_CUSTOM_SENSITIVITY_STEPS_AS_EV,
    PSLR_CUSTOM_SENSITIVITY_STEPS_MAX
} pslr_custom_sensitivity_steps_t;

typedef enum {
    PSLR_EXPOSURE_MODE_GREEN,
    PSLR_EXPOSURE_MODE_P,
    PSLR_EXPOSURE_MODE_SV,
    PSLR_EXPOSURE_MODE_TV,
    PSLR_EXPOSURE_MODE_AV,
    PSLR_EXPOSURE_MODE_TAV,
    PSLR_EXPOSURE_MODE_M,
    PSLR_EXPOSURE_MODE_B,
    PSLR_EXPOSURE_MODE_X,
    PSLR_EXPOSURE_MODE_MAX
} pslr_exposure_mode_t;

typedef enum {
    PSLR_AF_POINT_SEL_AUTO,
    PSLR_AF_POINT_SEL_SEL,
    PSLR_AF_POINT_SEL_CENTER,
    PSLR_AF_POINT_SEL_MAX
} pslr_af_point_sel_t;

typedef struct {
    int32_t nom;
    int32_t denom;
} pslr_rational_t;

typedef void *pslr_handle_t;

typedef enum {
    PSLR_STREAM_RAW,
    PSLR_STREAM_JPEG
} pslr_stream_t;

typedef void *pslr_buffer_handle_t;

typedef struct {
    uint16_t power;
} pslr_status_brief;

typedef struct {
    pslr_status_brief brief;
    uint16_t bufmask;                      /* 0x16 */
    uint32_t current_iso;                  /* 0x11c */
    pslr_rational_t current_shutter_speed; /* 0xf4 */
    pslr_rational_t current_aperture;      /* 0xfc */
    pslr_rational_t lens_max_aperture;     /* 0x12c */
    pslr_rational_t lens_min_aperture;     /* 0x134 */
    pslr_rational_t current_zoom;          /* 0x16c */
    pslr_rational_t set_shutter_speed;     /* 0x2c */
    pslr_rational_t set_aperture;          /* 0x34 */
    uint32_t set_iso;                      /* 0x60 */
    uint32_t jpeg_resolution;              /* 0x7c */
    uint32_t jpeg_saturation;              /* 0x8c */
    uint32_t jpeg_quality;                 /* 0x80 */
    uint32_t jpeg_contrast;                /* 0x94 */
    uint32_t jpeg_sharpness;               /* 0x90 */
    uint32_t jpeg_image_mode;              /* 0x88 */
    pslr_rational_t zoom;                  /* 0x16c */
    uint32_t focus;                        /* 0x174 */
    uint32_t image_format;                 /* 0x78 */
    uint32_t raw_format;                   /* 0x84 */
    uint32_t light_meter_flags;            /* 0x124 */
    pslr_rational_t ec;                    /* 0x3c */
    uint32_t custom_ev_steps;              /* 0x9c */
    uint32_t custom_sensitivity_steps;     /* 0xa0 */
    uint32_t exposure_mode;                /* 0xe0 */
    uint32_t user_mode_flag;               /* 0x1c */
    uint32_t af_point_select;              /* 0xbc */
    uint32_t selected_af_point;            /* 0xc0 */
    uint32_t focused_af_point;             /* 0x150 */
} pslr_status;

typedef void (*pslr_progress_callback_t)(uint32_t current, uint32_t total);

#ifdef LIBGPHOTO2
pslr_handle_t pslr_init(GPPort *port);
#else
pslr_handle_t pslr_init(void);
#endif
int pslr_connect(pslr_handle_t h);
int pslr_disconnect(pslr_handle_t h);
int pslr_shutdown(pslr_handle_t h);
const char *pslr_model(uint32_t id);

int pslr_shutter(pslr_handle_t h);
int pslr_focus(pslr_handle_t h);

int pslr_get_status(pslr_handle_t h, pslr_status *sbuf);
int pslr_get_buffer(pslr_handle_t h, int bufno, int type, int resolution,
                    uint8_t **pdata, uint32_t *pdatalen);

int pslr_set_progress_callback(pslr_handle_t h, pslr_progress_callback_t cb, 
                               uintptr_t user_data);

int pslr_set_shutter(pslr_handle_t h, pslr_rational_t value);
int pslr_set_aperture(pslr_handle_t h, pslr_rational_t value);
int pslr_set_iso(pslr_handle_t h, uint32_t value);
int pslr_set_ec(pslr_handle_t h, pslr_rational_t value);

int pslr_set_jpeg_quality(pslr_handle_t h, pslr_jpeg_quality_t quality);
int pslr_set_jpeg_resolution(pslr_handle_t h, int resolution);
int pslr_set_jpeg_image_mode(pslr_handle_t h, pslr_jpeg_image_mode_t image_mode);

int pslr_set_jpeg_sharpness(pslr_handle_t h, int32_t sharpness);
int pslr_set_jpeg_contrast(pslr_handle_t h, int32_t contrast);
int pslr_set_jpeg_saturation(pslr_handle_t h, int32_t saturation);

int pslr_set_image_format(pslr_handle_t h, pslr_image_format_t format);
int pslr_is_image_format_supported(pslr_handle_t h, pslr_image_format_t format);
int pslr_set_raw_format(pslr_handle_t h, pslr_raw_format_t format);

int pslr_delete_buffer(pslr_handle_t h, int bufno);

int pslr_green_button(pslr_handle_t h);
int pslr_ae_lock(pslr_handle_t h, bool lock);

int pslr_buffer_open(pslr_handle_t h, int bufno, int type, int resolution);
uint32_t pslr_buffer_read(pslr_handle_t h, uint8_t *buf, uint32_t size);
void pslr_buffer_close(pslr_buffer_handle_t h);
uint32_t pslr_buffer_get_size(pslr_handle_t h);

int pslr_set_exposure_mode(pslr_handle_t h, pslr_exposure_mode_t mode);
int pslr_select_af_point(pslr_handle_t h, uint32_t point);

const char *pslr_camera_name(pslr_handle_t h);
const char **pslr_camera_resolution_steps(pslr_handle_t h);
