/*
    pkTriggerCord
    Copyright (C) 2011-2019 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    Support for K200D added by Jens Dreyer <jens.dreyer@udo.edu> 04/2011
    Support for K-r added by Vincenc Podobnik <vincenc.podobnik@gmail.com> 06/2011
    Support for K-30 added by Camilo Polymeris <cpolymeris@gmail.com> 09/2012
    Support for K-01 added by Ethan Queen <ethanqueen@gmail.com> 01/2013
    Support for K-3 added by Tao Wang <twang2218@gmail.com> 01/2016

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    PK-Remote for Windows
    Copyright (C) 2010 Tomasz Kos

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License
    and GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include <stdarg.h>
#include <dirent.h>
#include <math.h>

#include "pslr.h"
#include "pslr_scsi.h"
#include "pslr_lens.h"

#define POLL_INTERVAL 50000 /* Number of us to wait when polling */
#define BLKSZ 65536 /* Block size for downloads; if too big, we get
                     * memory allocation error from sg driver */
#define BLOCK_RETRY 3 /* Number of retries, since we can occasionally
                       * get SCSI errors when downloading data */

void sleep_sec(double sec) {
    int i;
    for (i=0; i<floor(sec); ++i) {
        usleep(999999); // 1000000 is not working for Windows
    }
    usleep(1000000*(sec-floor(sec)));
}

ipslr_handle_t pslr;

static int ipslr_set_mode(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_00_09(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_10_0a(ipslr_handle_t *p, uint32_t mode);
static int ipslr_cmd_00_05(ipslr_handle_t *p);
static int ipslr_status(ipslr_handle_t *p, uint8_t *buf);
static int ipslr_status_full(ipslr_handle_t *p, pslr_status *status);
static int ipslr_press_shutter(ipslr_handle_t *p, bool fullpress);
static int ipslr_select_buffer(ipslr_handle_t *p, int bufno, pslr_buffer_type buftype, int bufres);
static int ipslr_buffer_segment_info(ipslr_handle_t *p, pslr_buffer_segment_info *pInfo);
static int ipslr_next_segment(ipslr_handle_t *p);
static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf);
static int ipslr_identify(ipslr_handle_t *p);
static int _ipslr_write_args(uint8_t cmd_2, ipslr_handle_t *p, int n, ...);
#define ipslr_write_args(p,n,...) _ipslr_write_args(0,(p),(n),__VA_ARGS__)
#define ipslr_write_args_special(p,n,...) _ipslr_write_args(4,(p),(n),__VA_ARGS__)

static int command(FDTYPE fd, int a, int b, int c);
static int get_status(FDTYPE fd);
static int get_result(FDTYPE fd);
static int read_result(FDTYPE fd, uint8_t *buf, uint32_t n);

void hexdump(uint8_t *buf, uint32_t bufLen);

static pslr_progress_callback_t progress_callback = NULL;

user_file_format_t file_formats[3] = {
    { USER_FILE_FORMAT_PEF, "PEF", "pef"},
    { USER_FILE_FORMAT_DNG, "DNG", "dng"},
    { USER_FILE_FORMAT_JPEG, "JPEG", "jpg"},
};


const char* valid_vendors[3] = {"PENTAX", "SAMSUNG", "RICOHIMG"};
const char* valid_models[3] = {"DIGITAL_CAMERA", "DSC", "Digital Camera"};

// x18 subcommands to change camera properties
// X18_n: unknown effect
typedef enum {
    X18_00,
    X18_EXPOSURE_MODE,
    X18_02,
    X18_AE_METERING_MODE,
    X18_FLASH_MODE,
    X18_AF_MODE,
    X18_AF_POINT_SEL,
    X18_AF_POINT,
    X18_08,
    X18_09,
    X18_0A,
    X18_0B,
    X18_0C,
    X18_0D,
    X18_0E,
    X18_0F,
    X18_WHITE_BALANCE,
    X18_WHITE_BALANCE_ADJ,
    X18_IMAGE_FORMAT,
    X18_JPEG_STARS,
    X18_JPEG_RESOLUTION,
    X18_ISO,
    X18_SHUTTER,
    X18_APERTURE,
    X18_EC,
    X18_19,
    X18_FLASH_EXPOSURE_COMPENSATION,
    X18_JPEG_IMAGE_TONE,
    X18_DRIVE_MODE,
    X18_1D,
    X18_1E,
    X18_RAW_FORMAT,
    X18_JPEG_SATURATION,
    X18_JPEG_SHARPNESS,
    X18_JPEG_CONTRAST,
    X18_COLOR_SPACE,
    X18_24,
    X18_JPEG_HUE
} x18_subcommands_t;

// x10 subcommands for buttons
// X10_n: unknown effect
typedef enum {
    X10_00,
    X10_01,
    X10_02,
    X10_03,
    X10_04,
    X10_SHUTTER,
    X10_AE_LOCK,
    X10_GREEN,
    X10_AE_UNLOCK,
    X10_09,
    X10_CONNECT,
    X10_0B,
    X10_CONTINUOUS,
    X10_BULB,
    X10_0E,
    X10_0F,
    X10_10,
    X10_DUST
} x10_subcommands_t;

/* ************** Enabling/disabling debug mode *************/
/* Done by reverse engineering the USB communication between PK Tether and */
/* Pentax K-10D camera. The debug on/off should work without breaking the  */
/* camera, but you are invoking this subroutines without any warranties    */
/* Written by: Samo Penic, 2014 */

#define DEBUG_OFF 0
#define DEBUG_ON 1

/* a different write_args function needs to be done with slightly changed */
/* command sequence. Original function was ipslr_write_args(). */

static
int pslr_get_buffer_status(ipslr_handle_t *p, uint32_t *x, uint32_t *y) {
    //ipslr_handle_t *p = (ipslr_handle_t *) h;
    DPRINT("[C]\t\tipslr_get_buffer_status()\n");
    uint8_t buf[8];
    int n;

    CHECK(command(p->fd, 0x02, 0x00, 0));
    n = get_result(p->fd);
    DPRINT("[C]\t\tipslr_get_buffer_status() bytes: %d\n",n);
    if (n!= 8) {
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p->fd, buf, n));
    int i;
    for (i=0; i<n; ++i) {
        DPRINT("[C]\t\tbuf[%d]=%02x\n",i,buf[i]);
    }
    get_uint32_func get_uint32_func_ptr;
    if (p->model->is_little_endian) {
        get_uint32_func_ptr = get_uint32_le;
    } else {
        get_uint32_func_ptr = get_uint32_be;
    }
    *x = (*get_uint32_func_ptr)(buf);
    *y = (*get_uint32_func_ptr)(buf+4);
    return PSLR_OK;
}

/* Commands in form 23 XX YY. I know it is stupid, but ipslr_cmd functions  */
/* are sooooo handy.                                                        */
static int ipslr_cmd_23_XX(ipslr_handle_t *p, char XX, char YY, uint32_t mode) {
    DPRINT("[C]\t\tipslr_cmd_23_XX(%x, %x, mode=%x)\n", XX, YY, mode);
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p->fd, 0x23, XX, YY));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

/* First of two exceptions. Command 0x23 0x06 0x14 behaves differently than */
/* generic 23 XX YY commands                                                */
static int ipslr_cmd_23_06(ipslr_handle_t *p, char debug_on_off) {
    DPRINT("[C]\t\tipslr_cmd_23_06(debug=%d)\n", debug_on_off);
    CHECK(ipslr_write_args(p, 1, 3));
    if (debug_on_off==0) {
        CHECK(ipslr_write_args_special(p, 4,0,0,0,0));
    } else {
        CHECK(ipslr_write_args_special(p, 4,1,1,0,0));
    }
    CHECK(command(p->fd, 0x23, 0x06, 0x14));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

/* Second exception. Command 0x23 0x04 0x08 behaves differently than generic */
/* 23 XX YY commands                                                         */
static int ipslr_cmd_23_04(ipslr_handle_t *p) {
    DPRINT("[C]\t\tipslr_cmd_23_04()\n");
    CHECK(ipslr_write_args(p, 1, 3)); // posebni ARGS-i
    CHECK(ipslr_write_args_special(p, 1, 1)); // posebni ARGS-i
    CHECK(command(p->fd, 0x23, 0x04, 0x08));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

/* Function called to enable/disable debug mode. If debug_mode argument is 0 */
/* function disables debug mode, else debug mode is enabled                  */
int debug_onoff(ipslr_handle_t *p, char debug_mode) {
    DPRINT("[C]\tdebug_onoff(%d)\n", debug_mode);
    uint8_t buf[16]; /* buffer for storing statuses and read_results */

    ipslr_cmd_00_09(p,1);

    ipslr_cmd_23_XX(p,0x07,0x04,3);
    read_result(p->fd,buf,0x10);

    ipslr_cmd_23_XX(p,0x05,0x04,3);
    read_result(p->fd,buf,0x04);
    ipslr_status(p,buf);

    if (debug_mode==0) {
        ipslr_cmd_23_06(p,DEBUG_OFF);
    } else {
        ipslr_cmd_23_06(p,DEBUG_ON);
    }
    ipslr_status(p,buf);


    ipslr_cmd_23_04(p);

    ipslr_cmd_23_XX(p,0x00,0x04, 0);

    ipslr_cmd_00_09(p,2);
    ipslr_status(p,buf);

    return PSLR_OK;
}

/* ************* End enabling/disabling debug mode ************ */

user_file_format_t *get_file_format_t( user_file_format uff ) {
    unsigned int i;
    for (i = 0; i<sizeof(file_formats) / sizeof(file_formats[0]); i++) {
        if (file_formats[i].uff == uff) {
            return &file_formats[i];
        }
    }
    return NULL;
}

int pslr_set_user_file_format(pslr_handle_t h, user_file_format uff) {
    switch ( uff ) {
        case USER_FILE_FORMAT_PEF:
            pslr_set_image_format(h, PSLR_IMAGE_FORMAT_RAW);
            pslr_set_raw_format(h, PSLR_RAW_FORMAT_PEF);
            break;
        case USER_FILE_FORMAT_DNG:
            pslr_set_image_format(h, PSLR_IMAGE_FORMAT_RAW);
            pslr_set_raw_format(h, PSLR_RAW_FORMAT_DNG);
            break;
        case USER_FILE_FORMAT_JPEG:
            pslr_set_image_format(h, PSLR_IMAGE_FORMAT_JPEG);
            break;
        case USER_FILE_FORMAT_MAX:
            return PSLR_PARAM;
    }
    return PSLR_OK;
}

user_file_format get_user_file_format( pslr_status *st ) {
    int rawfmt = st->raw_format;
    int imgfmt = st->image_format;
    if (imgfmt == PSLR_IMAGE_FORMAT_JPEG) {
        return USER_FILE_FORMAT_JPEG;
    } else {
        if (rawfmt == PSLR_RAW_FORMAT_PEF) {
            return USER_FILE_FORMAT_PEF;
        } else {
            return USER_FILE_FORMAT_DNG;
        }
    }
}

// most of the cameras require this exposure mode conversion step
pslr_gui_exposure_mode_t exposure_mode_conversion( pslr_exposure_mode_t exp ) {
    switch ( exp ) {

        case PSLR_EXPOSURE_MODE_GREEN:
            return PSLR_GUI_EXPOSURE_MODE_GREEN;
        case PSLR_EXPOSURE_MODE_P:
            return PSLR_GUI_EXPOSURE_MODE_P;
        case PSLR_EXPOSURE_MODE_SV:
            return PSLR_GUI_EXPOSURE_MODE_SV;
        case PSLR_EXPOSURE_MODE_TV:
            return PSLR_GUI_EXPOSURE_MODE_TV;
        case PSLR_EXPOSURE_MODE_AV:
        case PSLR_EXPOSURE_MODE_AV_OFFAUTO:
            return PSLR_GUI_EXPOSURE_MODE_AV;
        case PSLR_EXPOSURE_MODE_TAV:
            return PSLR_GUI_EXPOSURE_MODE_TAV;
        case PSLR_EXPOSURE_MODE_M:
        case PSLR_EXPOSURE_MODE_M_OFFAUTO:
            return PSLR_GUI_EXPOSURE_MODE_M;
        case PSLR_EXPOSURE_MODE_B:
        case PSLR_EXPOSURE_MODE_B_OFFAUTO:
            return PSLR_GUI_EXPOSURE_MODE_B;
        case PSLR_EXPOSURE_MODE_X:
            return PSLR_GUI_EXPOSURE_MODE_X;
        case PSLR_EXPOSURE_MODE_MAX:
            return PSLR_GUI_EXPOSURE_MODE_MAX;
    }
    return 0;
}

pslr_handle_t pslr_init( char *model, char *device ) {
    FDTYPE fd;
    char vendorId[20];
    char productId[20];
    int driveNum;
    char **drives;
    const char *camera_name;

    DPRINT("[C]\tplsr_init()\n");

    if ( device == NULL ) {
        drives = get_drives(&driveNum);
    } else {
        driveNum = 1;
        drives = malloc( driveNum * sizeof(char*) );
        drives[0] = malloc( strlen( device )+1 );
        strncpy( drives[0], device, strlen( device ) );
        drives[0][strlen(device)]='\0';
    }
    DPRINT("driveNum:%d\n",driveNum);
    int i;
    for ( i=0; i<driveNum; ++i ) {
        pslr_result result = get_drive_info( drives[i], &fd, vendorId, sizeof(vendorId), productId, sizeof(productId));

        DPRINT("\tChecking drive:  %s %s %s\n", drives[i], vendorId, productId);
        if ( find_in_array( valid_vendors, sizeof(valid_vendors)/sizeof(valid_vendors[0]),vendorId) != -1
                && find_in_array( valid_models, sizeof(valid_models)/sizeof(valid_models[0]), productId) != -1 ) {
            if ( result == PSLR_OK ) {
                DPRINT("\tFound camera %s %s\n", vendorId, productId);
                pslr.fd = fd;
                if ( model != NULL ) {
                    // user specified the camera model
                    camera_name = pslr_camera_name( &pslr );
                    DPRINT("\tName of the camera: %s\n", camera_name);
                    if ( str_comparison_i( camera_name, model, strlen( camera_name) ) == 0 ) {
                        return &pslr;
                    } else {
                        DPRINT("\tIgnoring camera %s %s\n", vendorId, productId);
                        pslr_shutdown ( &pslr );
                        pslr.id = 0;
                        pslr.model = NULL;
                    }
                } else {
                    return &pslr;
                }
            } else {
                DPRINT("\tCannot get drive info of Pentax camera. Please do not forget to install the program using 'make install'\n");
                // found the camera but communication is not possible
                close_drive( &fd );
                continue;
            }
        } else {
            close_drive( &fd );
            continue;
        }
    }
    DPRINT("\tcamera not found\n");
    return NULL;
}

int pslr_connect(pslr_handle_t h) {
    DPRINT("[C]\tpslr_connect()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    uint8_t statusbuf[28];
    CHECK(ipslr_status(p, statusbuf));
    CHECK(ipslr_set_mode(p, 1));
    CHECK(ipslr_status(p, statusbuf));
    CHECK(ipslr_identify(p));
    if ( !p->model ) {
        DPRINT("\nUnknown Pentax camera.\n");
        return -1;
    }
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("\tinit bufmask=0x%x\n", p->status.bufmask);
    if ( !p->model->old_scsi_command ) {
        CHECK(ipslr_cmd_00_09(p, 2));
    }
    CHECK(ipslr_status_full(p, &p->status));
    CHECK(ipslr_cmd_10_0a(p, 1));
    if ( p->model->old_scsi_command ) {
        CHECK(ipslr_cmd_00_05(p));
    }
    CHECK(ipslr_status_full(p, &p->status));
    return 0;
}

int pslr_disconnect(pslr_handle_t h) {
    DPRINT("[C]\tpslr_disconnect()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    uint8_t statusbuf[28];
    CHECK(ipslr_cmd_10_0a(p, 0));
    CHECK(ipslr_set_mode(p, 0));
    CHECK(ipslr_status(p, statusbuf));
    return PSLR_OK;
}

int pslr_shutdown(pslr_handle_t h) {
    DPRINT("[C]\tpslr_shutdown()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    close_drive(&p->fd);
    return PSLR_OK;
}

int pslr_shutter(pslr_handle_t h) {
    DPRINT("[C]\tpslr_shutter()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_press_shutter(p, true);
}

int pslr_focus(pslr_handle_t h) {
    DPRINT("[C]\tpslr_focus()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_press_shutter(p, false);
}

int pslr_get_status(pslr_handle_t h, pslr_status *ps) {
    DPRINT("[C]\tpslr_get_status()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset( ps, 0, sizeof( pslr_status ));
    CHECK(ipslr_status_full(p, &p->status));
    memcpy(ps, &p->status, sizeof (pslr_status));
    return PSLR_OK;
}

char *format_rational( pslr_rational_t rational, char * fmt ) {
    char *ret = malloc(32);
    if ( rational.denom == 0 ) {
        snprintf( ret, 32, "unknown" );
    } else {
        snprintf( ret, 32, fmt, 1.0 * rational.nom / rational.denom );
    }
    return ret;
}

static
char *get_white_balance_single_adjust_str( uint32_t adjust, char negativeChar, char positiveChar ) {
    char *ret = malloc(4);
    if ( adjust < 7 ) {
        snprintf( ret, 4, "%c%d", negativeChar, 7-adjust);
    } else if ( adjust > 7 ) {
        snprintf( ret, 4, "%c%d", positiveChar, adjust-7);
    } else {
        ret = "";
    }
    return ret;
}

static
char *get_white_balance_adjust_str( uint32_t adjust_mg, uint32_t adjust_ba ) {
    char *ret = malloc(8);
    if ( adjust_mg != 7 || adjust_ba != 7 ) {
        snprintf(ret, 8, "%s%s", get_white_balance_single_adjust_str(adjust_mg, 'M', 'G'),get_white_balance_single_adjust_str(adjust_ba, 'B', 'A'));
    } else {
        ret = "0";
    }
    return ret;
}

char *pslr_get_af_name(pslr_handle_t h, uint32_t af_point) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (p->model->af_point_num==11) {
        return get_pslr_af11_point_str(af_point);
    } else {
        char *raw = malloc(11);
        sprintf(raw, "%d", af_point);
        return raw;
    }
}

char *collect_status_info( pslr_handle_t h, pslr_status status ) {
    char *strbuffer = malloc(8192);
    sprintf(strbuffer,"%-32s: %d\n", "current iso", status.current_iso);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d/%d\n", "current shutter speed", status.current_shutter_speed.nom, status.current_shutter_speed.denom);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d/%d\n", "camera max shutter speed", status.max_shutter_speed.nom, status.max_shutter_speed.denom);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "current aperture", format_rational( status.current_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "lens max aperture", format_rational( status.lens_max_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "lens min aperture", format_rational( status.lens_min_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d/%d\n", "set shutter speed", status.set_shutter_speed.nom, status.set_shutter_speed.denom);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "set aperture", format_rational( status.set_aperture, "%.1f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "fixed iso", status.fixed_iso);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d-%d\n", "auto iso", status.auto_iso_min,status.auto_iso_max);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg quality", status.jpeg_quality);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %dM\n", "jpeg resolution", pslr_get_jpeg_resolution( h, status.jpeg_resolution));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "jpeg image tone", get_pslr_jpeg_image_tone_str(status.jpeg_image_tone));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg saturation", status.jpeg_saturation);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg contrast", status.jpeg_contrast);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg sharpness", status.jpeg_sharpness);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "jpeg hue", status.jpeg_hue);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s mm\n", "zoom", format_rational(status.zoom, "%.2f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "focus", status.focus);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "color space", get_pslr_color_space_str(status.color_space));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "image format", get_pslr_image_format_str(status.image_format));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "raw format", get_pslr_raw_format_str(status.raw_format));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "light meter flags", status.light_meter_flags);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "ec", format_rational( status.ec, "%.2f" ) );
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s EV steps\n", "custom ev steps", get_pslr_custom_ev_steps_str(status.custom_ev_steps));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s EV steps\n", "custom sensitivity steps", get_pslr_custom_sensitivity_steps_str(status.custom_sensitivity_steps));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "exposure mode", status.exposure_mode);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "scene mode", get_pslr_scene_mode_str(status.scene_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "user mode flag", status.user_mode_flag);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "ae metering mode", get_pslr_ae_metering_str(status.ae_metering_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "af mode", get_pslr_af_mode_str(status.af_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "af point select", get_pslr_af_point_sel_str(status.af_point_select));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "selected af point", pslr_get_af_name( h, status.selected_af_point));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "focused af point", pslr_get_af_name( h, status.focused_af_point));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "drive mode", get_pslr_drive_mode_str(status.drive_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "auto bracket mode", status.auto_bracket_mode > 0 ? "on" : "off");
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "auto bracket picture count", status.auto_bracket_picture_count);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %d\n", "auto bracket picture counter", status.auto_bracket_picture_counter);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "auto bracket ev", format_rational(status.auto_bracket_ev, "%.2f"));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "shake reduction", status.shake_reduction > 0 ? "on" : "off");
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "white balance mode", get_pslr_white_balance_mode_str(status.white_balance_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "white balance adjust", get_white_balance_adjust_str(status.white_balance_adjust_mg, status.white_balance_adjust_ba));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "flash mode", get_pslr_flash_mode_str(status.flash_mode));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %.2f\n", "flash exposure compensation", (1.0 * status.flash_exposure_compensation/256));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %.2f\n", "manual mode ev", (1.0 * status.manual_mode_ev / 10));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "lens", get_lens_name(status.lens_id1, status.lens_id2));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %.2fV %.2fV %.2fV %.2fV\n", "battery", 0.01 * status.battery_1, 0.01 * status.battery_2, 0.01 * status.battery_3, 0.01 * status.battery_4);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %s\n", "buffer mask", int_to_binary(status.bufmask));
    return strbuffer;
}

static
char *get_hardwired_setting_bool_info( pslr_bool_setting setting) {
    char *strbuffer = malloc(32+1);
    sprintf(strbuffer,"%-32s", setting.pslr_setting_status == PSLR_SETTING_STATUS_HARDWIRED ? "\t[hardwired]" : "");
    return strbuffer;
}

static
char *get_special_setting_info( pslr_setting_status_t setting_status) {
    char *strbuffer = malloc(32);
    switch ( setting_status ) {
        case PSLR_SETTING_STATUS_NA:
            sprintf(strbuffer,"N/A");
            break;
        case PSLR_SETTING_STATUS_UNKNOWN:
            sprintf(strbuffer,"Unknown");
            break;
        default:
            return NULL;
    }
    return strbuffer;
}

static
char *get_hardwired_setting_uint16_info( pslr_uint16_setting setting) {
    char *strbuffer = malloc(32+1);
    sprintf(strbuffer,"%-32s", setting.pslr_setting_status == PSLR_SETTING_STATUS_HARDWIRED ? "\t[hardwired]" : "");
    return strbuffer;
}

char *collect_settings_info( pslr_handle_t h, pslr_settings settings ) {
    char *strbuffer = malloc(8192);
    sprintf(strbuffer,"%-32s: %-8s%s\n", "one push bracketing", get_special_setting_info(settings.one_push_bracketing.pslr_setting_status) ?: settings.one_push_bracketing.value ? "on" : "off", get_hardwired_setting_bool_info(settings.one_push_bracketing));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "bulb mode", get_special_setting_info(settings.bulb_mode_press_press.pslr_setting_status) ?: settings.bulb_mode_press_press.value ? "press-press" : "press-hold", get_hardwired_setting_bool_info(settings.bulb_mode_press_press));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "bulb timer", get_special_setting_info(settings.bulb_timer.pslr_setting_status) ?: settings.bulb_timer.value ? "on" : "off", get_hardwired_setting_bool_info(settings.bulb_timer));
    char *bulb_timer_sec = malloc(32);
    sprintf(bulb_timer_sec, "%d s", settings.bulb_timer_sec.value);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "bulb timer sec", get_special_setting_info(settings.bulb_timer_sec.pslr_setting_status) ?: bulb_timer_sec, get_hardwired_setting_uint16_info(settings.bulb_timer_sec));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "remote bulb mode", get_special_setting_info(settings.remote_bulb_mode_press_press.pslr_setting_status) ?: settings.remote_bulb_mode_press_press.value ? "press-press" : "press-hold", get_hardwired_setting_bool_info(settings.remote_bulb_mode_press_press));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "using aperture ring", get_special_setting_info(settings.using_aperture_ring.pslr_setting_status) ?: settings.using_aperture_ring.value ? "on" : "off", get_hardwired_setting_bool_info(settings.using_aperture_ring));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "shake reduction", get_special_setting_info(settings.shake_reduction.pslr_setting_status) ?: settings.shake_reduction.value ? "on" : "off", get_hardwired_setting_bool_info(settings.shake_reduction));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "astrotracer", get_special_setting_info(settings.astrotracer.pslr_setting_status) ?: settings.astrotracer.value ? "on" : "off", get_hardwired_setting_bool_info(settings.astrotracer));
    char *astrotracer_timer_sec = malloc(32);
    sprintf(astrotracer_timer_sec, "%d s", settings.astrotracer_timer_sec.value);
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "astrotracer timer sec", get_special_setting_info(settings.astrotracer_timer_sec.pslr_setting_status) ?: astrotracer_timer_sec, get_hardwired_setting_uint16_info(settings.astrotracer_timer_sec));
    sprintf(strbuffer+strlen(strbuffer),"%-32s: %-8s%s\n", "horizon correction", get_special_setting_info(settings.horizon_correction.pslr_setting_status) ?: settings.horizon_correction.value ? "on" : "off", get_hardwired_setting_bool_info(settings.horizon_correction));
    return strbuffer;
}


int pslr_get_status_buffer(pslr_handle_t h, uint8_t *st_buf) {
    DPRINT("[C]\tpslr_get_status_buffer()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset( st_buf, 0, MAX_STATUS_BUF_SIZE);
//    CHECK(ipslr_status_full(p, &p->status));
//    ipslr_status_full(p, &p->status);
    memcpy(st_buf, p->status_buffer, MAX_STATUS_BUF_SIZE);
    return PSLR_OK;
}

int pslr_get_settings_buffer(pslr_handle_t h, uint8_t *st_buf) {
    DPRINT("[C]\tpslr_get_settings_buffer()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset( st_buf, 0, SETTINGS_BUFFER_SIZE);
    memcpy(st_buf, p->settings_buffer, SETTINGS_BUFFER_SIZE);
    return PSLR_OK;
}

int pslr_get_buffer(pslr_handle_t h, int bufno, pslr_buffer_type type, int resolution,
                    uint8_t **ppData, uint32_t *pLen) {
    DPRINT("[C]\tpslr_get_buffer()\n");
    uint8_t *buf = 0;
    int ret;
    ret = pslr_buffer_open(h, bufno, type, resolution);
    if ( ret != PSLR_OK ) {
        return ret;
    }

    uint32_t size = pslr_buffer_get_size(h);
    buf = malloc(size);
    if (!buf) {
        return PSLR_NO_MEMORY;
    }

    uint32_t bufpos = 0;
    while (true) {
        uint32_t nextread = size - bufpos > 65536 ? 65536 : size - bufpos;
        if (nextread == 0) {
            break;
        }
        uint32_t bytes = pslr_buffer_read(h, buf+bufpos, nextread);
        if (bytes == 0) {
            break;
        }
        bufpos += bytes;
    }
    if ( bufpos != size ) {
        return PSLR_READ_ERROR;
    }
    pslr_buffer_close(h);
    if (ppData) {
        *ppData = buf;
    }
    if (pLen) {
        *pLen = size;
    }

    return PSLR_OK;
}

int pslr_set_progress_callback(pslr_handle_t h, pslr_progress_callback_t cb, uintptr_t user_data) {
    progress_callback = cb;
    return PSLR_OK;
}

static
int ipslr_handle_command_x18( ipslr_handle_t *p, bool cmd9_wrap, int subcommand, int argnum,  ...) {
    DPRINT("[C]\t\tipslr_handle_command_x18(0x%x, %d)\n", subcommand, argnum);
    if ( cmd9_wrap ) {
        CHECK(ipslr_cmd_00_09(p, 1));
    }
    // max 4 args
    va_list ap;
    int args[4];
    int i;
    for ( i = 0; i < 4; ++i ) {
        args[i] = 0;
    }
    va_start(ap, argnum);
    for (i = 0; i < argnum; i++) {
        args[i] = va_arg(ap, int);
    }
    va_end(ap);
    CHECK(ipslr_write_args(p, argnum, args[0], args[1], args[2], args[3]));
    CHECK(command(p->fd, 0x18, subcommand, 4 * argnum));
    CHECK(get_status(p->fd));
    if ( cmd9_wrap ) {
        CHECK(ipslr_cmd_00_09(p, 2));
    }
    return PSLR_OK;
}

int pslr_test( pslr_handle_t h, bool cmd9_wrap, int subcommand, int argnum,  int arg1, int arg2, int arg3, int arg4) {
    DPRINT("[C]\tpslr_test(wrap=%d, subcommand=0x%x, %x, %x, %x, %x)\n", cmd9_wrap, subcommand, arg1, arg2, arg3, arg4);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, cmd9_wrap, subcommand, argnum, arg1, arg2, arg3, arg4);
}

int pslr_set_shutter(pslr_handle_t h, pslr_rational_t value) {
    DPRINT("[C]\tpslr_set_shutter(%x %x)\n", value.nom, value.denom);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_SHUTTER, 2, value.nom, value.denom, 0);
}

int pslr_set_aperture(pslr_handle_t h, pslr_rational_t value) {
    DPRINT("[C]\tpslr_set_aperture(%x %x)\n", value.nom, value.denom);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, false, X18_APERTURE, 3, value.nom, value.denom, 0);
}

int pslr_set_iso(pslr_handle_t h, uint32_t value, uint32_t auto_min_value, uint32_t auto_max_value) {
    DPRINT("[C]\tpslr_set_iso(0x%X, auto_min=%X, auto_max=%X)\n", value, auto_min_value, auto_max_value);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_ISO, 3, value, auto_min_value, auto_max_value);
}

int pslr_set_ec(pslr_handle_t h, pslr_rational_t value) {
    DPRINT("[C]\tpslr_set_ec(0x%X 0x%X)\n", value.nom, value.denom);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_EC, 2, value.nom, value.denom, 0);
}

int pslr_set_white_balance(pslr_handle_t h, pslr_white_balance_mode_t wb_mode) {
    DPRINT("[C]\tpslr_set_white_balance(0x%X)\n", wb_mode);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_WHITE_BALANCE, 1, wb_mode);
}

int pslr_set_white_balance_adjustment(pslr_handle_t h, pslr_white_balance_mode_t wb_mode, uint32_t wbadj_mg, uint32_t wbadj_ba) {
    DPRINT("[C]\tpslr_set_white_balance_adjustment(mode=0x%X, tint=0x%X, temp=0x%X)\n", wb_mode, wbadj_mg, wbadj_ba);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_WHITE_BALANCE_ADJ, 3, wb_mode, wbadj_mg, wbadj_ba);
}


int pslr_set_flash_mode(pslr_handle_t h, pslr_flash_mode_t value) {
    DPRINT("[C]\tpslr_set_flash_mode(%X)\n", value);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_FLASH_MODE, 1, value, 0, 0);
}

int pslr_set_flash_exposure_compensation(pslr_handle_t h, pslr_rational_t value) {
    DPRINT("[C]\tpslr_set_flash_exposure_compensation(%X %X)\n", value.nom, value.denom);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_FLASH_EXPOSURE_COMPENSATION, 2, value.nom, value.denom, 0);
}

int pslr_set_drive_mode(pslr_handle_t h, pslr_drive_mode_t drive_mode) {
    DPRINT("[C]\tpslr_set_drive_mode(%X)\n", drive_mode);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_DRIVE_MODE, 1, drive_mode, 0, 0);
}

int pslr_set_ae_metering_mode(pslr_handle_t h, pslr_ae_metering_t ae_metering_mode) {
    DPRINT("[C]\tpslr_set_ae_metering_mode(%X)\n", ae_metering_mode);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_AE_METERING_MODE, 1, ae_metering_mode, 0, 0);
}

int pslr_set_af_mode(pslr_handle_t h, pslr_af_mode_t af_mode) {
    DPRINT("[C]\tpslr_set_af_mode(%X)\n", af_mode);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_AF_MODE, 1, af_mode, 0, 0);
}

int pslr_set_af_point_sel(pslr_handle_t h, pslr_af_point_sel_t af_point_sel) {
    DPRINT("[C]\tpslr_set_af_point_sel(%X)\n", af_point_sel);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_AF_POINT_SEL, 1, af_point_sel, 0, 0);
}

int pslr_set_jpeg_stars(pslr_handle_t h, int jpeg_stars ) {
    DPRINT("[C]\tpslr_set_jpeg_stars(%X)\n", jpeg_stars);
    int hwqual;
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if ( jpeg_stars > p->model->max_jpeg_stars ) {
        return PSLR_PARAM;
    }
    hwqual = get_hw_jpeg_quality( p->model, jpeg_stars );
    return ipslr_handle_command_x18( p, true, X18_JPEG_STARS, 2, 1, hwqual, 0);
}

static
int _get_user_jpeg_resolution( ipslr_model_info_t *model, int hwres ) {
    return model->jpeg_resolutions[hwres];
}

int pslr_get_jpeg_resolution(pslr_handle_t h, int hwres) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return _get_user_jpeg_resolution( p->model, hwres );
}

static
int _get_hw_jpeg_resolution( ipslr_model_info_t *model, int megapixel) {
    int resindex = 0;
    while ( resindex < MAX_RESOLUTION_SIZE && model->jpeg_resolutions[resindex] > megapixel ) {
        ++resindex;
    }
    return resindex < MAX_RESOLUTION_SIZE ? resindex : MAX_RESOLUTION_SIZE-1;
}

int pslr_set_jpeg_resolution(pslr_handle_t h, int megapixel) {
    DPRINT("[C]\tpslr_set_jpeg_resolution(%X)\n", megapixel);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hwres = _get_hw_jpeg_resolution( p->model, megapixel );
    return ipslr_handle_command_x18( p, true, X18_JPEG_RESOLUTION, 2, 1, hwres, 0);
}

int pslr_set_jpeg_image_tone(pslr_handle_t h, pslr_jpeg_image_tone_t image_tone) {
    DPRINT("[C]\tpslr_set_jpeg_image_tone(%X)\n", image_tone);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (image_tone < 0 || image_tone > PSLR_JPEG_IMAGE_TONE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_JPEG_IMAGE_TONE, 1, image_tone, 0, 0);
}

int pslr_set_jpeg_sharpness(pslr_handle_t h, int32_t sharpness) {
    DPRINT("[C]\tpslr_set_jpeg_sharpness(%X)\n", sharpness);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_sharpness = sharpness + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_sharpness < 0 || hw_sharpness >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, X18_JPEG_SHARPNESS, 2, 0, hw_sharpness, 0);
}

int pslr_set_jpeg_contrast(pslr_handle_t h, int32_t contrast) {
    DPRINT("[C]\tpslr_set_jpeg_contrast(%X)\n", contrast);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_contrast = contrast + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_contrast < 0 || hw_contrast >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, X18_JPEG_CONTRAST, 2, 0, hw_contrast, 0);
}

int pslr_set_jpeg_hue(pslr_handle_t h, int32_t hue) {
    DPRINT("[C]\tpslr_set_jpeg_hue(%X)\n", hue);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_hue = hue + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    DPRINT("hw_hue: %d\n", hw_hue);
    if (hw_hue < 0 || hw_hue >= p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    DPRINT("before return\n");
    return ipslr_handle_command_x18( p, false, X18_JPEG_HUE, 2, 0, hw_hue, 0);
}

int pslr_set_jpeg_saturation(pslr_handle_t h, int32_t saturation) {
    DPRINT("[C]\tpslr_set_jpeg_saturation(%X)\n", saturation);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int hw_saturation = saturation + (pslr_get_model_jpeg_property_levels( h )-1) / 2;
    if (hw_saturation < 0 || hw_saturation >=  p->model->jpeg_property_levels) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, false, X18_JPEG_SATURATION, 2, 0, hw_saturation, 0);
}

int pslr_set_image_format(pslr_handle_t h, pslr_image_format_t format) {
    DPRINT("[C]\tpslr_set_image_format(%X)\n", format);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format >= PSLR_IMAGE_FORMAT_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_IMAGE_FORMAT, 2, 1, format, 0);
}

int pslr_set_raw_format(pslr_handle_t h, pslr_raw_format_t format) {
    DPRINT("[C]\tpslr_set_raw_format(%X)\n", format);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (format >= PSLR_RAW_FORMAT_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_RAW_FORMAT, 2, 1, format, 0);
}

int pslr_set_color_space(pslr_handle_t h, pslr_color_space_t color_space) {
    DPRINT("[C]\tpslr_set_raw_format(%X)\n", color_space);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (color_space >= PSLR_COLOR_SPACE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_COLOR_SPACE, 1, color_space, 0, 0);
}


int pslr_delete_buffer(pslr_handle_t h, int bufno) {
    DPRINT("[C]\tpslr_delete_buffer(%X)\n", bufno);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (bufno < 0 || bufno > 9) {
        return PSLR_PARAM;
    }
    CHECK(ipslr_write_args(p, 1, bufno));
    CHECK(command(p->fd, 0x02, 0x03, 0x04));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_green_button(pslr_handle_t h) {
    DPRINT("[C]\tpslr_green_button()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(command(p->fd, 0x10, X10_GREEN, 0x00));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_dust_removal(pslr_handle_t h) {
    DPRINT("[C]\tpslr_dust_removal()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(command(p->fd, 0x10, X10_DUST, 0x00));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_bulb(pslr_handle_t h, bool on ) {
    DPRINT("[C]\tpslr_bulb(%d)\n", on);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 1, on ? 1 : 0));
    CHECK(command(p->fd, 0x10, X10_BULB, 0x04));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_button_test(pslr_handle_t h, int bno, int arg) {
    DPRINT("[C]\tpslr_button_test(%X, %X)\n", bno, arg);
    int r;
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    CHECK(ipslr_write_args(p, 1, arg));
    CHECK(command(p->fd, 0x10, bno, 4));
    r = get_status(p->fd);
    DPRINT("\tbutton result code: 0x%x\n", r);
    return PSLR_OK;
}


int pslr_ae_lock(pslr_handle_t h, bool lock) {
    DPRINT("[C]\tpslr_ae_lock(%X)\n", lock);
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    if (lock) {
        CHECK(command(p->fd, 0x10, X10_AE_LOCK, 0x00));
    } else {
        CHECK(command(p->fd, 0x10, X10_AE_UNLOCK, 0x00));
    }
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

int pslr_set_exposure_mode(pslr_handle_t h, pslr_exposure_mode_t mode) {
    DPRINT("[C]\tpslr_set_exposure_mode(%X)\n", mode);
    ipslr_handle_t *p = (ipslr_handle_t *) h;

    if (mode >= PSLR_EXPOSURE_MODE_MAX) {
        return PSLR_PARAM;
    }
    return ipslr_handle_command_x18( p, true, X18_EXPOSURE_MODE, 2, 1, mode, 0);
}

int pslr_buffer_open(pslr_handle_t h, int bufno, pslr_buffer_type buftype, int bufres) {
    DPRINT("[C]\tpslr_buffer_open(#%X, type=%X, res=%X)\n", bufno, buftype, bufres);
    pslr_buffer_segment_info info;
    uint16_t bufs;
    uint32_t buf_total = 0;
    int i, j;
    int ret;
    int retry = 0;
    int retry2 = 0;

    ipslr_handle_t *p = (ipslr_handle_t *) h;

    memset(&info, 0, sizeof (info));

    CHECK(ipslr_status_full(p, &p->status));
    bufs = p->status.bufmask;
    DPRINT("\tp->status.bufmask = %x\n", p->status.bufmask);

    if ( p->model->status_parser_function && (bufs & (1 << bufno)) == 0) {
        // do not check this for limited support cameras
        DPRINT("\tNo buffer data (%d)\n", bufno);
        return PSLR_READ_ERROR;
    }

    while (retry < 3) {
        /* If we get response 0x82 from the camera, there is a
         * desynch. We can recover by stepping through segment infos
         * until we get the last one (b = 2). Retry up to 3 times. */
        ret = ipslr_select_buffer(p, bufno, buftype, bufres);
        if (ret == PSLR_OK) {
            break;
        }

        retry++;
        retry2 = 0;
        /* Try up to 9 times to reach segment info type 2 (last
         * segment) */
        do {
            CHECK(ipslr_buffer_segment_info(p, &info));
            CHECK(ipslr_next_segment(p));
            DPRINT("\tRecover: b=%d\n", info.b);
        } while (++retry2 < 10 && info.b != 2);
    }

    if (retry == 3) {
        return ret;
    }

    i = 0;
    j = 0;
    do {
        CHECK(ipslr_buffer_segment_info(p, &info));
        DPRINT("\t%d: Addr: 0x%X Len: %d(0x%08X) B=%d\n", i, info.addr, info.length, info.length, info.b);
        if (info.b == 4) {
            p->segments[j].offset = info.length;
        } else if (info.b == 3) {
            if (j == MAX_SEGMENTS) {
                DPRINT("\tToo many segments.\n");
                return PSLR_NO_MEMORY;
            }
            p->segments[j].addr = info.addr;
            p->segments[j].length = info.length;
            j++;
        }
        CHECK(ipslr_next_segment(p));
        buf_total += info.length;
        i++;
    } while (i < 9 && info.b != 2);
    p->segment_count = j;
    p->offset = 0;
    return PSLR_OK;
}

uint32_t pslr_buffer_read(pslr_handle_t h, uint8_t *buf, uint32_t size) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int i;
    uint32_t pos = 0;
    uint32_t seg_offs;
    uint32_t addr;
    uint32_t blksz;
    int ret;

    DPRINT("[C]\tpslr_buffer_read(%d)\n", size);

    /* Find current segment */
    for (i = 0; i < p->segment_count; i++) {
        if (p->offset < pos + p->segments[i].length) {
            break;
        }
        pos += p->segments[i].length;
    }

    seg_offs = p->offset - pos;
    addr = p->segments[i].addr + seg_offs;

    /* Compute block size */
    blksz = size;
    if (blksz > p->segments[i].length - seg_offs) {
        blksz = p->segments[i].length - seg_offs;
    }
    if (blksz > BLKSZ) {
        blksz = BLKSZ;
    }

//    DPRINT("File offset %d segment: %d offset %d address 0x%x read size %d\n", p->offset,
//           i, seg_offs, addr, blksz);

    ret = ipslr_download(p, addr, blksz, buf);
    if (ret != PSLR_OK) {
        return 0;
    }
    p->offset += blksz;
    return blksz;
}

uint32_t pslr_fullmemory_read(pslr_handle_t h, uint8_t *buf, uint32_t offset, uint32_t size) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int ret;

    DPRINT("[C]\tpslr_fullmemory_read(%d)\n", size);

    ret = ipslr_download(p, offset, size, buf);
    if (ret != PSLR_OK) {
        return 0;
    }
    return size;
}

uint32_t pslr_buffer_get_size(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int i;
    uint32_t len = 0;
    for (i = 0; i < p->segment_count; i++) {
        len += p->segments[i].length;
    }
    DPRINT("\tbuffer get size:%d\n",len);
    return len;
}

void pslr_buffer_close(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset(&p->segments[0], 0, sizeof (p->segments));
    p->offset = 0;
    p->segment_count = 0;
}

int pslr_select_af_point(pslr_handle_t h, uint32_t point) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return ipslr_handle_command_x18( p, true, X18_AF_POINT, 1, point, 0, 0);
}

int pslr_get_model_max_jpeg_stars(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->max_jpeg_stars;
}

int pslr_get_model_status_buffer_size(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->status_buffer_size;
}

int pslr_get_model_jpeg_property_levels(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->jpeg_property_levels;
}

int *pslr_get_model_jpeg_resolutions(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->jpeg_resolutions;
}

bool pslr_get_model_only_limited(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->status_buffer_size == 0 && !p->model->status_parser_function;
}

bool pslr_get_model_has_jpeg_hue(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->has_jpeg_hue;
}

bool pslr_get_model_need_exposure_conversion(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->need_exposure_mode_conversion;
}

int pslr_get_model_fastest_shutter_speed(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->fastest_shutter_speed;
}

int pslr_get_model_base_iso_min(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->base_iso_min;
}

int pslr_get_model_base_iso_max(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->base_iso_max;
}

int pslr_get_model_extended_iso_min(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->extended_iso_min;
}

int pslr_get_model_extended_iso_max(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->extended_iso_max;
}

pslr_jpeg_image_tone_t pslr_get_model_max_supported_image_tone(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->max_supported_image_tone;
}

int pslr_get_model_af_point_num(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->af_point_num;
}

bool pslr_get_model_old_bulb_mode(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->old_bulb_mode;
}

bool pslr_get_model_bufmask_single(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return p->model->bufmask_single;
}

bool pslr_get_model_has_settings_parser(pslr_handle_t h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    char cameraid[10];
    sprintf(cameraid, "0x%0x", p->model->id);
    int def_num;
    setting_file_process(cameraid, &def_num);
    return def_num>0;
}

const char *pslr_camera_name(pslr_handle_t h) {
    DPRINT("[C]\tpslr_camera_name()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int ret;
    if (p->id == 0) {
        ret = ipslr_identify(p);
        if (ret != PSLR_OK) {
            return NULL;
        }
    }
    if (p->model) {
        return p->model->name;
    } else {
        static char unk_name[256];
        snprintf(unk_name, sizeof (unk_name), "ID#%x", p->id);
        unk_name[sizeof (unk_name) - 1] = '\0';
        return unk_name;
    }
}

pslr_buffer_type pslr_get_jpeg_buffer_type(pslr_handle_t h, int jpeg_stars) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    return 2 + get_hw_jpeg_quality( p->model, jpeg_stars );
}

static int ipslr_set_mode(ipslr_handle_t *p, uint32_t mode) {
    DPRINT("[C]\t\tipslr_set_mode(0x%x)\n", mode);
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p->fd, 0, 0, 4));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

static int ipslr_cmd_00_09(ipslr_handle_t *p, uint32_t mode) {
    DPRINT("[C]\t\tipslr_cmd_00_09(0x%x)\n", mode);
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p->fd, 0, 9, 4));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

static int ipslr_cmd_10_0a(ipslr_handle_t *p, uint32_t mode) {
    DPRINT("[C]\t\tipslr_cmd_10_0a(0x%x)\n", mode);
    CHECK(ipslr_write_args(p, 1, mode));
    CHECK(command(p->fd, 0x10, X10_CONNECT, 4));
    CHECK(get_status(p->fd));
    return PSLR_OK;
}

static int ipslr_cmd_00_05(ipslr_handle_t *p) {
    DPRINT("[C]\t\tipslr_cmd_00_05()\n");
    int n;
    uint8_t buf[0xb8];
    CHECK(command(p->fd, 0x00, 0x05, 0x00));
    n = get_result(p->fd);
    if (n != 0xb8) {
        DPRINT("\tonly got %d bytes\n", n);
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p->fd, buf, n));
    return PSLR_OK;
}

static int ipslr_status(ipslr_handle_t *p, uint8_t *buf) {
    int n;
    DPRINT("[C]\t\tipslr_status()\n");
    CHECK(command(p->fd, 0, 1, 0));
    n = get_result(p->fd);
    if (n == 16 || n == 28) {
        return read_result(p->fd, buf, n);
    } else {
        return PSLR_READ_ERROR;
    }
}

static int ipslr_status_full(ipslr_handle_t *p, pslr_status *status) {
    int n;
    DPRINT("[C]\t\tipslr_status_full()\n");
    CHECK(command(p->fd, 0, 8, 0));
    n = get_result(p->fd);
    DPRINT("\tread %d bytes\n", n);
    int expected_bufsize = p->model != NULL ? p->model->status_buffer_size : 0;
    if ( p->model == NULL ) {
        DPRINT("\tp model null\n");
    }
    DPRINT("\texpected_bufsize: %d\n",expected_bufsize);

    CHECK(read_result(p->fd, p->status_buffer, n > MAX_STATUS_BUF_SIZE ? MAX_STATUS_BUF_SIZE: n));

    if ( expected_bufsize == 0 || !p->model->status_parser_function ) {
        // limited support only
        return PSLR_OK;
    } else if ( expected_bufsize > 0 && expected_bufsize != n ) {
        DPRINT("\tWaiting for %d bytes but got %d\n", expected_bufsize, n);
        return PSLR_READ_ERROR;
    } else {
        // everything OK
        (*p->model->status_parser_function)(p, status);
        if ( p->model->need_exposure_mode_conversion ) {
            status->exposure_mode = exposure_mode_conversion( status->exposure_mode );
        }
        if ( p->model->bufmask_command ) {
            uint32_t x, y;
            int ret;

            ret = pslr_get_buffer_status(p, &x, &y);
            if (ret != PSLR_OK) {
                return ret;
            }
            status->bufmask = x;
        }
        return PSLR_OK;
    }
}

// fullpress: take picture
// halfpress: autofocus
static int ipslr_press_shutter(ipslr_handle_t *p, bool fullpress) {
    DPRINT("[C]\t\tipslr_press_shutter(fullpress = %s)\n", (fullpress ? "true" : "false"));
    int r;
    CHECK(ipslr_status_full(p, &p->status));
    DPRINT("\t\tbefore: mask=0x%x\n", p->status.bufmask);
    CHECK(ipslr_write_args(p, 1, fullpress ? 2 : 1));
    CHECK(command(p->fd, 0x10, X10_SHUTTER, 0x04));
    r = get_status(p->fd);
    DPRINT("\t\tshutter result code: 0x%x\n", r);
    return PSLR_OK;
}

static int ipslr_select_buffer(ipslr_handle_t *p, int bufno, pslr_buffer_type buftype, int bufres) {
    int r;
    DPRINT("\t\tSelect buffer %d,%d,%d,0\n", bufno, buftype, bufres);
    if ( !p->model->old_scsi_command ) {
        CHECK(ipslr_write_args(p, 4, bufno, buftype, bufres, 0));
        CHECK(command(p->fd, 0x02, 0x01, 0x10));
    } else {
        /* older cameras: 3-arg select buffer */
        CHECK(ipslr_write_args(p, 4, bufno, buftype, bufres));
        CHECK(command(p->fd, 0x02, 0x01, 0x0c));
    }
    r = get_status(p->fd);
    if (r != 0) {
        return PSLR_COMMAND_ERROR;
    }
    return PSLR_OK;
}

static int ipslr_next_segment(ipslr_handle_t *p) {
    DPRINT("[C]\t\tipslr_next_segment()\n");
    int r;
    CHECK(ipslr_write_args(p, 1, 0));
    CHECK(command(p->fd, 0x04, 0x01, 0x04));
    usleep(100000); // needed !! 100 too short, 1000 not short enough for PEF
    r = get_status(p->fd);
    if (r == 0) {
        return PSLR_OK;
    }
    return PSLR_COMMAND_ERROR;
}

static int ipslr_buffer_segment_info(ipslr_handle_t *p, pslr_buffer_segment_info *pInfo) {
    DPRINT("[C]\t\tipslr_buffer_segment_info()\n");
    uint8_t buf[16];
    uint32_t n;
    int num_try = 20;

    pInfo->b = 0;
    while ( pInfo->b == 0 && --num_try > 0 ) {
        CHECK(command(p->fd, 0x04, 0x00, 0x00));
        n = get_result(p->fd);
        if (n != 16) {
            return PSLR_READ_ERROR;
        }
        CHECK(read_result(p->fd, buf, 16));

        //  use the right function based on the endian.
        get_uint32_func get_uint32_func_ptr;
        if (p->model->is_little_endian) {
            get_uint32_func_ptr = get_uint32_le;
        } else {
            get_uint32_func_ptr = get_uint32_be;
        }

        pInfo->a = (*get_uint32_func_ptr)(&buf[0]);
        pInfo->b = (*get_uint32_func_ptr)(&buf[4]);
        pInfo->addr = (*get_uint32_func_ptr)(&buf[8]);
        pInfo->length = (*get_uint32_func_ptr)(&buf[12]);
        if ( pInfo-> b == 0 ) {
            DPRINT("\tWaiting for segment info addr: 0x%x len: %d B=%d\n", pInfo->addr, pInfo->length, pInfo->b);
            sleep_sec( 0.1 );
        }
    }
    return PSLR_OK;
}

static int ipslr_download(ipslr_handle_t *p, uint32_t addr, uint32_t length, uint8_t *buf) {
    DPRINT("[C]\t\tipslr_download(address = 0x%X, length = %d)\n", addr, length);
    uint8_t downloadCmd[8] = {0xf0, 0x24, 0x06, 0x02, 0x00, 0x00, 0x00, 0x00};
    uint32_t block;
    int n;
    int retry;
    uint32_t length_start = length;

    retry = 0;
    while (length > 0) {
        if (length > BLKSZ) {
            block = BLKSZ;
        } else {
            block = length;
        }

        //DPRINT("Get 0x%x bytes from 0x%x\n", block, addr);
        CHECK(ipslr_write_args(p, 2, addr, block));
        CHECK(command(p->fd, 0x06, 0x00, 0x08));
        get_status(p->fd);

        n = scsi_read(p->fd, downloadCmd, sizeof (downloadCmd), buf, block);
        get_status(p->fd);

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
            progress_callback(length_start - length, length_start);
        }
    }
    return PSLR_OK;
}

static int ipslr_identify(ipslr_handle_t *p) {
    DPRINT("[C]\t\tipslr_identify()\n");
    uint8_t idbuf[8];
    int n;

    CHECK(command(p->fd, 0, 4, 0));
    n = get_result(p->fd);
    if (n != 8) {
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p->fd, idbuf, 8));
    //  Check the camera endian, which affect ID
    if (idbuf[0] == 0) {
        p->id = get_uint32_be(&idbuf[0]);
    } else {
        p->id = get_uint32_le(&idbuf[0]);
    }
    DPRINT("\tid of the camera: %x\n", p->id);
    p->model = find_model_by_id( p->id );
    return PSLR_OK;
}

int pslr_read_datetime(pslr_handle_t *h, int *year, int *month, int *day, int *hour, int *min, int *sec) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    DPRINT("[C]\t\tipslr_read_datetime()\n");
    uint8_t idbuf[800];
    int n;

    CHECK(command(p->fd, 0x20, 0x06, 0));
    n = get_result(p->fd);
    DPRINT("[C]\t\tipslr_read_datetime() bytes: %d\n",n);
    if (n!= 24) {
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p->fd, idbuf, n));
    get_uint32_func get_uint32_func_ptr;

    if (p->model->is_little_endian) {
        get_uint32_func_ptr = get_uint32_le;
    } else {
        get_uint32_func_ptr = get_uint32_be;
    }
    *year = (*get_uint32_func_ptr)(idbuf);
    *month = (*get_uint32_func_ptr)(idbuf+4);
    *day = (*get_uint32_func_ptr)(idbuf+8);
    *hour = (*get_uint32_func_ptr)(idbuf+12);
    *min = (*get_uint32_func_ptr)(idbuf+16);
    *sec = (*get_uint32_func_ptr)(idbuf+20);
    return PSLR_OK;
}

int pslr_read_dspinfo(pslr_handle_t *h, char* firmware) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    DPRINT("[C]\t\tipslr_read_dspinfo()\n");
    uint8_t buf[4];
    int n;

    CHECK(command(p->fd, 0x01, 0x01, 0));
    n = get_result(p->fd);
    DPRINT("[C]\t\tipslr_read_dspinfo() bytes: %d\n",n);
    if (n!= 4) {
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p->fd, buf, n));
    if (p->model->is_little_endian) {
        snprintf( firmware, 16, "%d.%02d.%02d.%02d", buf[3], buf[2], buf[1], buf[0]);
    } else {
        snprintf( firmware, 16, "%d.%02d.%02d.%02d", buf[0], buf[1], buf[2], buf[3]);
    }
    return PSLR_OK;
}

int pslr_read_setting(pslr_handle_t *h, int offset, uint32_t *value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    DPRINT("[C]\t\tipslr_read_setting(%d)\n", offset);
    uint8_t buf[4];
    int n;

    CHECK(ipslr_write_args(p, 1, offset));
    CHECK(command(p->fd, 0x20, 0x09, 4));
    n = get_result(p->fd);
    DPRINT("[C]\t\tipslr_read_setting() bytes: %d\n",n);
    if (n!= 4) {
        return PSLR_READ_ERROR;
    }
    CHECK(read_result(p->fd, buf, n));
    get_uint32_func get_uint32_func_ptr;
    if (p->model->is_little_endian) {
        get_uint32_func_ptr = get_uint32_le;
    } else {
        get_uint32_func_ptr = get_uint32_be;
    }
    *value = (*get_uint32_func_ptr)(buf);
    return PSLR_OK;
}

int pslr_write_setting(pslr_handle_t *h, int offset, uint32_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    DPRINT("[C]\t\tipslr_write_setting(%d)=%d\n", offset, value);
    CHECK(ipslr_cmd_00_09(p, 1));
    CHECK(ipslr_write_args(p, 2, offset, value));
    CHECK(command(p->fd, 0x20, 0x08, 8));
    CHECK(ipslr_cmd_00_09(p, 2));
    return PSLR_OK;
}

int pslr_write_setting_by_name(pslr_handle_t *h, char *name, uint32_t value) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int def_num;
    char cameraid[10];
    sprintf(cameraid, "0x%0x", p->model->id);
    //    printf("cameraid: %s\n", cameraid);
    pslr_setting_def_t *defs = setting_file_process(cameraid, &def_num);
    pslr_setting_def_t *setting_def = find_setting_by_name(defs, def_num, name);
    if (setting_def != NULL) {
        if (strcmp(setting_def->type,"boolean") == 0) {
            pslr_write_setting(h, setting_def->address, value);
        } else if (strcmp(setting_def->type, "uint16") == 0) {
            pslr_write_setting(h, setting_def->address, value >> 8);
            pslr_write_setting(h, setting_def->address+1, value & 0xff);
        }
    }
    return PSLR_OK;
}

bool pslr_has_setting_by_name(pslr_handle_t *h, char *name) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int def_num;
    char cameraid[10];
    sprintf(cameraid, "0x%0x", p->model->id);
    pslr_setting_def_t *defs = setting_file_process(cameraid, &def_num);
    pslr_setting_def_t *setting_def = find_setting_by_name(defs, def_num, name);
//    printf("%d %d\n", def_num, (setting_def != NULL));
    return (setting_def != NULL);
}


int pslr_read_settings(pslr_handle_t *h) {
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    int index=0;
    uint32_t value;
    int ret;
    while (index<SETTINGS_BUFFER_SIZE) {
        if ( (ret = pslr_read_setting(h, index, &value)) != PSLR_OK ) {
            return ret;
        }
        p->settings_buffer[index] = value;
        ++index;
    }
    return PSLR_OK;
}

int pslr_get_settings_json(pslr_handle_t h, pslr_settings *ps) {
    DPRINT("[C]\tpslr_get_settings_json()\n");
    ipslr_handle_t *p = (ipslr_handle_t *) h;
    memset( ps, 0, sizeof( pslr_settings ));
    CHECK(pslr_read_settings(h));
    char cameraid[20];
    sprintf(cameraid, "0x%05x", p->id);
    DPRINT("cameraid:%s\n", cameraid);
    ipslr_settings_parser_json(cameraid, p, &p->settings);
    memcpy(ps, &p->settings, sizeof (pslr_settings));
    return PSLR_OK;
}


static int _ipslr_write_args(uint8_t cmd_2, ipslr_handle_t *p, int n, ...) {
    va_list ap;
    uint8_t cmd[8] = {0xf0, 0x4f, cmd_2, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t buf[4 * n];
    FDTYPE fd = p->fd;
    int res;
    int i;
    uint32_t data;

    // print debug info
    va_start(ap, n);
    DPRINT("[C]\t\t\t_ipslr_write_args(cmd_2 = 0x%x, {", cmd_2);
    for (i = 0; i < n; i++) {
        if (i > 0) {
            DPRINT(", ");
        }
        DPRINT("0x%X", va_arg(ap, uint32_t));
    }
    DPRINT("})\n");
    va_end(ap);

    va_start(ap, n);
    if ( p->model && !p->model->old_scsi_command ) {
        /* All at once */
        for (i = 0; i < n; i++) {
            data = va_arg(ap, uint32_t);

            if (p->model == NULL || !p->model->is_little_endian) {
                set_uint32_be(data, &buf[4*i]);
            } else {
                set_uint32_le(data, &buf[4*i]);
            }
        }
        cmd[4] = 4 * n;


        res = scsi_write(fd, cmd, sizeof (cmd), buf, 4 * n);
        if (res != PSLR_OK) {
            va_end(ap);
            return res;
        }
    } else {
        /* Arguments one by one */
        for (i = 0; i < n; i++) {
            data = va_arg(ap, uint32_t);

            if (p->model == NULL || !p->model->is_little_endian) {
                set_uint32_be(data, &buf[0]);
            } else {
                set_uint32_le(data, &buf[0]);
            }

            cmd[4] = 4;
            cmd[2] = i * 4;
            res = scsi_write(fd, cmd, sizeof (cmd), buf, 4);
            if (res != PSLR_OK) {
                va_end(ap);
                return res;
            }
        }
    }
    va_end(ap);
    return PSLR_OK;
}

static int command(FDTYPE fd, int a, int b, int c) {
    DPRINT("[C]\t\t\tcommand(fd=%x, %x, %x, %x)\n", fd, a, b, c);
    uint8_t cmd[8] = {0xf0, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    cmd[2] = a;
    cmd[3] = b;
    cmd[4] = c;

    CHECK(scsi_write(fd, cmd, sizeof (cmd), 0, 0));
    return PSLR_OK;
}

static int read_status(FDTYPE fd, uint8_t *buf) {
    uint8_t cmd[8] = {0xf0, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int n;

    n = scsi_read(fd, cmd, 8, buf, 8);
    if (n != 8) {
        DPRINT("\tOnly got %d bytes\n", n);
        /* The *ist DS doesn't know to return the correct number of
            read bytes for this command, so return PSLR_OK instead of
            PSLR_READ_ERROR */
        return PSLR_OK;
    }
    return PSLR_OK;
}

static int get_status(FDTYPE fd) {
    DPRINT("[C]\t\t\tget_status(0x%x)\n", fd);

    uint8_t statusbuf[8];
    memset(statusbuf,0,8);

    while (1) {
        CHECK(read_status(fd, statusbuf));
        DPRINT("[R]\t\t\t\t => ERROR: 0x%02X\n", statusbuf[7]);
        if (statusbuf[7] != 0x01) {
            break;
        }
        usleep(POLL_INTERVAL);
    }
    if (statusbuf[7] != 0) {
        DPRINT("\tERROR: 0x%x\n", statusbuf[7]);
    }
    return statusbuf[7];
}

static int get_result(FDTYPE fd) {
    DPRINT("[C]\t\t\tget_result(0x%x)\n", fd);
    uint8_t statusbuf[8];
    while (1) {
        //DPRINT("read out status\n");
        CHECK(read_status(fd, statusbuf));
        //hexdump_debug(statusbuf, 8);
        if (statusbuf[6] == 0x01) {
            break;
        }
        //DPRINT("Waiting for result\n");
        //hexdump_debug(statusbuf, 8);
        usleep(POLL_INTERVAL);
    }
    if ((statusbuf[7] & 0xff) != 0) {
        DPRINT("\tERROR: 0x%x\n", statusbuf[7]);
        return -1;
    } else {
        DPRINT("[R]\t\t\t\t => [%02X %02X %02X %02X]\n",
               statusbuf[0], statusbuf[1], statusbuf[2], statusbuf[3]);
    }
    return statusbuf[0] | statusbuf[1] << 8 | statusbuf[2] << 16 | statusbuf[3] << 24;
}

static int read_result(FDTYPE fd, uint8_t *buf, uint32_t n) {
    DPRINT("[C]\t\t\tread_result(0x%x, size=%d)\n", fd, n);
    uint8_t cmd[8] = {0xf0, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int r;
    int i;
    set_uint32_le(n, &cmd[4]);
    r = scsi_read(fd, cmd, sizeof (cmd), buf, n);
    if (r != n) {
        return PSLR_READ_ERROR;
    }  else {
        //  Print first 32 bytes of the result.
        DPRINT("[R]\t\t\t\t => [");
        for (i = 0; i < n && i < 32; ++i) {
            if (i > 0) {
                if (i % 16 == 0) {
                    DPRINT("\n\t\t\t\t    ");
                } else if ((i%4) == 0 ) {
                    DPRINT(" ");
                }
                DPRINT(" ");
            }
            DPRINT("%02X", buf[i]);
        }
        if (n > 32) {
            DPRINT(" ... (%d bytes more)", (n-32));
        }
        DPRINT("]\n");
    }
    return PSLR_OK;
}

char *copyright() {
    char *ret = malloc(sizeof(char)*1024);
    sprintf(ret, "Copyright (C) 2011-2019 Andras Salamon\n\
\n\
Based on:\n\
pslr-shoot (C) 2009 Ramiro Barreiro\n\
PK-Remote (C) 2008 Pontus Lidman \n\n");
    return ret;
}

void write_debug( const char* message, ... ) {

    // Be sure debug is really on as DPRINT doesn't know
    //
    if ( !debug ) {
        return;
    }

    // Write to stderr
    //
    va_list argp;
    va_start(argp, message);
    vfprintf( stderr, message, argp );
    va_end(argp);
    return;
}
