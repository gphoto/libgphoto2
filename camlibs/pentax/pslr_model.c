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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#ifndef RAD10
#include <unistd.h>
#endif
#include "js0n.h"
#include <sys/types.h>
#include <sys/stat.h>

#include "pslr_model.h"
#include "pslr_log.h"
#include "pslr.h"

static uint8_t lastbuf[MAX_STATUS_BUF_SIZE];
static int first = 1;
static char *jsontext=NULL;
static int jsonsize;

static int dir_exists(char *dir) {
    int res = 0;
    struct stat info;

    if ( (stat(dir, &info) == 0) && (info.st_mode & S_IFDIR) ) {
        res = 1;
    }
    return res;
}

static void ipslr_status_diff(uint8_t *buf) {
    int n;
    int diffs;
    if (first) {
        hexdump(buf, MAX_STATUS_BUF_SIZE);
        memcpy(lastbuf, buf, MAX_STATUS_BUF_SIZE);
        first = 0;
    }

    diffs = 0;
    for (n = 0; n < MAX_STATUS_BUF_SIZE; n++) {
        if (lastbuf[n] != buf[n]) {
            DPRINT("\t\tbuf[%03X] last %02Xh %3d new %02Xh %3d\n", n, lastbuf[n], lastbuf[n], buf[n], buf[n]);
            diffs++;
        }
    }
    if (diffs) {
        DPRINT("---------------------------\n");
        memcpy(lastbuf, buf, MAX_STATUS_BUF_SIZE);
    }
}

static
uint16_t get_uint16_be(const uint8_t *buf) {
    uint16_t res;
    res = buf[0] << 8 | buf[1];
    return res;
}

uint32_t get_uint32_be(uint8_t *buf) {
    uint32_t res;
    res = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
    return res;
}

static
int32_t get_int32_be(uint8_t *buf) {
    int32_t res;
    res = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
    return res;
}

static
uint16_t get_uint16_le(const uint8_t *buf) {
    uint16_t res;
    res = buf[1] << 8 | buf[0];
    return res;
}

uint32_t get_uint32_le(uint8_t *buf) {
    uint32_t res;
    res = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
    return res;
}

static
int32_t get_int32_le(uint8_t *buf) {
    int32_t res;
    res = buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0];
    return res;
}

void set_uint32_le(uint32_t v, uint8_t *buf) {
    buf[0] = v;
    buf[1] = v >> 8;
    buf[2] = v >> 16;
    buf[3] = v >> 24;
}

void set_uint32_be(uint32_t v, uint8_t *buf) {
    buf[0] = v >> 24;
    buf[1] = v >> 16;
    buf[2] = v >> 8;
    buf[3] = v;
}

char *pslr_hexdump(uint8_t *buf, uint32_t bufLen) {
    char *ret = malloc(4*bufLen);
    uint32_t i;
    sprintf(ret,"%s","");
    for (i = 0; i < bufLen; i++) {
        if (i % 16 == 0) {
            sprintf(ret+strlen(ret),"0x%04x | ", i);
        }
        sprintf(ret+strlen(ret), "%02x ", buf[i]);
        if (i % 8 == 7) {
            sprintf(ret+strlen(ret), " ");
        }
        if (i % 16 == 15) {
            sprintf(ret+strlen(ret), "\n");
        }
    }
    if (i % 16 != 15) {
        sprintf(ret+strlen(ret), "\n");
    }
    return ret;
}

void hexdump(uint8_t *buf, uint32_t bufLen) {
    char *dmp = pslr_hexdump(buf, bufLen);
    printf("%s",dmp);
    free(dmp);
}

void hexdump_debug(uint8_t *buf, uint32_t bufLen) {
    char *dmp = pslr_hexdump(buf, bufLen);
    DPRINT("%s",dmp);
    free(dmp);
}


// based on http://stackoverflow.com/a/657202/21348

const char* int_to_binary( uint16_t x ) {
    static char b[sizeof(uint16_t)*8+1] = {0};
    int y;
    long long z;
    for (z=(1LL<<sizeof(uint16_t)*8)-1,y=0; z>0; z>>=1,y++) {
        b[y] = ( ((x & z) == z) ? '1' : '0');
    }
    b[y] = 0;
    return b;
}


static
int _get_user_jpeg_stars( ipslr_model_info_t *model, int hwqual ) {
    if ( model->id == 0x12f71 ) {
        // K5IIs hack
        // TODO: test it
        if ( hwqual == model->max_jpeg_stars -1 ) {
            return model->max_jpeg_stars;
        } else {
            return model->max_jpeg_stars - 1 - hwqual;
        }
    } else {
        return model->max_jpeg_stars - hwqual;
    }
}

int pslr_get_hw_jpeg_quality( ipslr_model_info_t *model, int user_jpeg_stars) {
    if ( model->id == 0x12f71 ) {
        // K5IIs hack
        // TODO: test it
        if ( user_jpeg_stars == model->max_jpeg_stars ) {
            return model->max_jpeg_stars-1;
        } else {
            return model->max_jpeg_stars - 1 - user_jpeg_stars;
        }
    } else {
        return model->max_jpeg_stars - user_jpeg_stars;
    }
}


static
void ipslr_status_parse_k10d(ipslr_handle_t  *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }
    memset(status, 0, sizeof (*status));
    status->bufmask = get_uint16_be(&buf[0x16]);
    status->user_mode_flag = get_uint32_be(&buf[0x1c]);
    status->set_shutter_speed.nom = get_uint32_be(&buf[0x2c]);
    status->set_shutter_speed.denom = get_uint32_be(&buf[0x30]);
    status->set_aperture.nom = get_uint32_be(&buf[0x34]);
    status->set_aperture.denom = get_uint32_be(&buf[0x38]);
    status->ec.nom = get_uint32_be(&buf[0x3c]);
    status->ec.denom = get_uint32_be(&buf[0x40]);
    status->fixed_iso = get_uint32_be(&buf[0x60]);
    status->image_format = get_uint32_be(&buf[0x78]);
    status->jpeg_resolution = get_uint32_be(&buf[0x7c]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32_be(&buf[0x80]));
    status->raw_format = get_uint32_be(&buf[0x84]);
    status->jpeg_image_tone = get_uint32_be(&buf[0x88]);
    status->jpeg_saturation = get_uint32_be(&buf[0x8c]);
    status->jpeg_sharpness = get_uint32_be(&buf[0x90]);
    status->jpeg_contrast = get_uint32_be(&buf[0x94]);
    status->custom_ev_steps = get_uint32_be(&buf[0x9c]);
    status->custom_sensitivity_steps = get_uint32_be(&buf[0xa0]);
    status->af_point_select = get_uint32_be(&buf[0xbc]);
    status->selected_af_point = get_uint32_be(&buf[0xc0]);
    status->exposure_mode = get_uint32_be(&buf[0xac]);
    status->current_shutter_speed.nom = get_uint32_be(&buf[0xf4]);
    status->current_shutter_speed.denom = get_uint32_be(&buf[0xf8]);
    status->current_aperture.nom = get_uint32_be(&buf[0xfc]);
    status->current_aperture.denom = get_uint32_be(&buf[0x100]);
    status->current_iso = get_uint32_be(&buf[0x11c]);
    status->light_meter_flags = get_uint32_be(&buf[0x124]);
    status->lens_min_aperture.nom = get_uint32_be(&buf[0x12c]);
    status->lens_min_aperture.denom = get_uint32_be(&buf[0x130]);
    status->lens_max_aperture.nom = get_uint32_be(&buf[0x134]);
    status->lens_max_aperture.denom = get_uint32_be(&buf[0x138]);
    status->focused_af_point = get_uint32_be(&buf[0x150]);
    status->zoom.nom = get_uint32_be(&buf[0x16c]);
    status->zoom.denom = get_uint32_be(&buf[0x170]);
    status->focus = get_int32_be(&buf[0x174]);
}

static
void ipslr_status_parse_k20d(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }
    memset(status, 0, sizeof (*status));
    status->bufmask = get_uint16_be( &buf[0x16]);
    status->user_mode_flag = get_uint32_be(&buf[0x1c]);
    status->set_shutter_speed.nom = get_uint32_be(&buf[0x2c]);
    status->set_shutter_speed.denom = get_uint32_be(&buf[0x30]);
    status->set_aperture.nom = get_uint32_be(&buf[0x34]);
    status->set_aperture.denom = get_uint32_be(&buf[0x38]);
    status->ec.nom = get_uint32_be(&buf[0x3c]);
    status->ec.denom = get_uint32_be(&buf[0x40]);
    status->fixed_iso = get_uint32_be(&buf[0x60]);
    status->image_format = get_uint32_be(&buf[0x78]);
    status->jpeg_resolution = get_uint32_be(&buf[0x7c]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32_be(&buf[0x80]));
    status->raw_format = get_uint32_be(&buf[0x84]);
    status->jpeg_image_tone = get_uint32_be(&buf[0x88]);
    status->jpeg_saturation = get_uint32_be(&buf[0x8c]); // commands do now work for it?
    status->jpeg_sharpness = get_uint32_be(&buf[0x90]); // commands do now work for it?
    status->jpeg_contrast = get_uint32_be(&buf[0x94]); // commands do now work for it?
    status->custom_ev_steps = get_uint32_be(&buf[0x9c]);
    status->custom_sensitivity_steps = get_uint32_be(&buf[0xa0]);
    status->ae_metering_mode = get_uint32_be(&buf[0xb4]); // same as c4
    status->af_mode = get_uint32_be(&buf[0xb8]);
    status->af_point_select = get_uint32_be(&buf[0xbc]); // not sure
    status->selected_af_point = get_uint32_be(&buf[0xc0]);
    status->exposure_mode = get_uint32_be(&buf[0xac]);
    status->current_shutter_speed.nom = get_uint32_be(&buf[0x108]);
    status->current_shutter_speed.denom = get_uint32_be(&buf[0x10C]);
    status->current_aperture.nom = get_uint32_be(&buf[0x110]);
    status->current_aperture.denom = get_uint32_be(&buf[0x114]);
    status->current_iso = get_uint32_be(&buf[0x130]);
    status->light_meter_flags = get_uint32_be(&buf[0x138]);
    status->lens_min_aperture.nom = get_uint32_be(&buf[0x140]);
    status->lens_min_aperture.denom = get_uint32_be(&buf[0x144]);
    status->lens_max_aperture.nom = get_uint32_be(&buf[0x148]);
    status->lens_max_aperture.denom = get_uint32_be(&buf[0x14B]);
    status->focused_af_point = get_uint32_be(&buf[0x160]); // unsure about it, a lot is changing when the camera focuses
    status->zoom.nom = get_uint32_be(&buf[0x180]);
    status->zoom.denom = get_uint32_be(&buf[0x184]);
    status->focus = get_int32_be(&buf[0x188]); // current focus ring position?
    // 0x158 current ev?
    // 0x160 and 0x164 change when AF
}

static
void ipslr_status_parse_istds(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
    /* *ist DS status block */
    memset(status, 0, sizeof (*status));
    status->bufmask = get_uint16_be(&buf[0x12]);
    status->set_shutter_speed.nom = get_uint32_be(&buf[0x80]);
    status->set_shutter_speed.denom = get_uint32_be(&buf[0x84]);
    status->set_aperture.nom = get_uint32_be(&buf[0x88]);
    status->set_aperture.denom = get_uint32_be(&buf[0x8c]);
    status->lens_min_aperture.nom = get_uint32_be(&buf[0xb8]);
    status->lens_min_aperture.denom = get_uint32_be(&buf[0xbc]);
    status->lens_max_aperture.nom = get_uint32_be(&buf[0xc0]);
    status->lens_max_aperture.denom = get_uint32_be(&buf[0xc4]);

    // no DNG support so raw format is PEF
    status->raw_format = PSLR_RAW_FORMAT_PEF;
}

// some of the cameras share most of the status fields
// this method is used for K-x, K-7, K-5, K-r
//
// some cameras also have this data block, but it's shifted a bit
static
void ipslr_status_parse_common(ipslr_handle_t *p, pslr_status *status, int shift) {

    uint8_t *buf = p->status_buffer;
    get_uint32_func get_uint32_func_ptr;
    get_int32_func get_int32_func_ptr;
    get_uint16_func get_uint16_func_ptr;

    if (p->model->is_little_endian) {
        get_uint32_func_ptr = get_uint32_le;
        get_int32_func_ptr = get_int32_le;
        get_uint16_func_ptr = get_uint16_le;
    } else {
        get_uint32_func_ptr = get_uint32_be;
        get_int32_func_ptr = get_int32_be;
        get_uint16_func_ptr = get_uint16_be;
    }

    // 0x0C: 0x85 0xA5
    // 0x0F: beginning 0 sometime changes to 1
    // 0x14: LCD panel 2: turned off 3: on?
    status->bufmask = (*get_uint16_func_ptr)( &buf[0x1E + shift]);
    status->user_mode_flag = (*get_uint32_func_ptr)(&buf[0x24 + shift]);
    status->flash_mode = (*get_uint32_func_ptr)(&buf[0x28 + shift]);
    status->flash_exposure_compensation = (*get_int32_func_ptr)(&buf[0x2C + shift]);
    status->set_shutter_speed.nom = (*get_uint32_func_ptr)(&buf[0x34 + shift]);
    status->set_shutter_speed.denom = (*get_uint32_func_ptr)(&buf[0x38 + shift]);
    status->set_aperture.nom = (*get_uint32_func_ptr)(&buf[0x3C + shift]);
    status->set_aperture.denom = (*get_uint32_func_ptr)(&buf[0x40 + shift]);
    status->ec.nom = (*get_uint32_func_ptr)(&buf[0x44 + shift]);
    status->ec.denom = (*get_uint32_func_ptr)(&buf[0x48 + shift]);
    status->auto_bracket_mode = (*get_uint32_func_ptr)(&buf[0x4C + shift]);
    status->auto_bracket_ev.nom = (*get_uint32_func_ptr)(&buf[0x50 + shift]);
    status->auto_bracket_ev.denom = (*get_uint32_func_ptr)(&buf[0x54 + shift]);
    status->auto_bracket_picture_count = (*get_uint32_func_ptr)(&buf[0x58 + shift]);
    status->drive_mode = (*get_uint32_func_ptr)(&buf[0x5C + shift]);
    status->fixed_iso = (*get_uint32_func_ptr)(&buf[0x68 + shift]);
    status->auto_iso_min = (*get_uint32_func_ptr)(&buf[0x6C + shift]);
    status->auto_iso_max = (*get_uint32_func_ptr)(&buf[0x70 + shift]);
    status->white_balance_mode = (*get_uint32_func_ptr)(&buf[0x74 + shift]);
    status->white_balance_adjust_mg = (*get_uint32_func_ptr)(&buf[0x78 + shift]); // 0: M7 7: 0 14: G7
    status->white_balance_adjust_ba = (*get_uint32_func_ptr)(&buf[0x7C + shift]); // 0: B7 7: 0 14: A7
    status->image_format = (*get_uint32_func_ptr)(&buf[0x80 + shift]);
    status->jpeg_resolution = (*get_uint32_func_ptr)(&buf[0x84 + shift]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, (*get_uint32_func_ptr)(&buf[0x88 + shift]));
    status->raw_format = (*get_uint32_func_ptr)(&buf[0x8C + shift]);
    status->jpeg_image_tone = (*get_uint32_func_ptr)(&buf[0x90 + shift]);
    status->jpeg_saturation = (*get_uint32_func_ptr)(&buf[0x94 + shift]);
    status->jpeg_sharpness = (*get_uint32_func_ptr)(&buf[0x98 + shift]);
    status->jpeg_contrast = (*get_uint32_func_ptr)(&buf[0x9C + shift]);
    status->color_space = (*get_uint32_func_ptr)(&buf[0xA0 + shift]);
    status->custom_ev_steps = (*get_uint32_func_ptr)(&buf[0xA4 + shift]);
    status->custom_sensitivity_steps = (*get_uint32_func_ptr)(&buf[0xa8 + shift]);
    status->exposure_mode = (*get_uint32_func_ptr)(&buf[0xb4 + shift]);
    status->scene_mode = (*get_uint32_func_ptr)(&buf[0xb8 + shift]);
    status->ae_metering_mode = (*get_uint32_func_ptr)(&buf[0xbc + shift]); // same as cc
    status->af_mode = (*get_uint32_func_ptr)(&buf[0xC0 + shift]);
    status->af_point_select = (*get_uint32_func_ptr)(&buf[0xc4 + shift]);
    status->selected_af_point = (*get_uint32_func_ptr)(&buf[0xc8 + shift]);
    status->shake_reduction = (*get_uint32_func_ptr)(&buf[0xE0 + shift]);
    status->auto_bracket_picture_counter = (*get_uint32_func_ptr)(&buf[0xE4 + shift]);
    status->jpeg_hue = (*get_uint32_func_ptr)(&buf[0xFC + shift]);
    status->current_shutter_speed.nom = (*get_uint32_func_ptr)(&buf[0x10C + shift]);
    status->current_shutter_speed.denom = (*get_uint32_func_ptr)(&buf[0x110 + shift]);
    status->current_aperture.nom = (*get_uint32_func_ptr)(&buf[0x114 + shift]);
    status->current_aperture.denom = (*get_uint32_func_ptr)(&buf[0x118 + shift]);
    status->max_shutter_speed.nom = (*get_uint32_func_ptr)(&buf[0x12C + shift]);
    status->max_shutter_speed.denom = (*get_uint32_func_ptr)(&buf[0x130 + shift]);
    status->current_iso = (*get_uint32_func_ptr)(&buf[0x134 + shift]);
    status->light_meter_flags = (*get_uint32_func_ptr)(&buf[0x13C + shift]);
    status->lens_min_aperture.nom = (*get_uint32_func_ptr)(&buf[0x144 + shift]);
    status->lens_min_aperture.denom = (*get_uint32_func_ptr)(&buf[0x148 + shift]);
    status->lens_max_aperture.nom = (*get_uint32_func_ptr)(&buf[0x14C + shift]);
    status->lens_max_aperture.denom = (*get_uint32_func_ptr)(&buf[0x150 + shift]);
    status->manual_mode_ev = (*get_int32_func_ptr)(&buf[0x15C + shift]);
    status->focused_af_point = (*get_uint32_func_ptr)(&buf[0x168 + shift]); //d, unsure about it, a lot is changing when the camera focuses
    // probably voltage*100
    // battery_1 > battery2 ( noload vs load voltage?)
    status->battery_1 = (*get_uint32_func_ptr)( &buf[0x170 + shift] );
    status->battery_2 = (*get_uint32_func_ptr)( &buf[0x174 + shift] );
    status->battery_3 = (*get_uint32_func_ptr)( &buf[0x180 + shift] );
    status->battery_4 = (*get_uint32_func_ptr)( &buf[0x184 + shift] );

}

static
void ipslr_status_parse_kx(ipslr_handle_t *p, pslr_status *status) {

    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0);
    status->zoom.nom = get_uint32_be(&buf[0x198]);
    status->zoom.denom = get_uint32_be(&buf[0x19C]);
    status->focus = get_int32_be(&buf[0x1A0]);
    status->lens_id1 = get_uint32_be(&buf[0x188]) & 0x0F;
    status->lens_id2 = get_uint32_be( &buf[0x194]);
    // selected_af_point: cannot find the field, 0xc8 is always zero
}

// Vince: K-r support 2011-06-22
//
static
void ipslr_status_parse_kr(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    status->zoom.nom = get_uint32_be(&buf[0x19C]);
    status->zoom.denom = get_uint32_be(&buf[0x1A0]);
    status->focus = get_int32_be(&buf[0x1A4]);
    status->lens_id1 = get_uint32_be(&buf[0x18C]) & 0x0F;
    status->lens_id2 = get_uint32_be( &buf[0x198]);
}

static
void ipslr_status_parse_k5(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    status->zoom.nom = get_uint32_be(&buf[0x1A0]);
    status->zoom.denom = get_uint32_be(&buf[0x1A4]);
    status->focus = get_int32_be(&buf[0x1A8]); // ?
    status->lens_id1 = get_uint32_be(&buf[0x190]) & 0x0F;
    status->lens_id2 = get_uint32_be( &buf[0x19C]);

// TODO: check these fields
//status.focused = getInt32(statusBuf, 0x164);
}

static
void ipslr_status_parse_k30(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    //~ status->jpeg_contrast -= 4;
    //~ status->jpeg_hue -= 4;
    //~ status->jpeg_sharpness -= 4;
    //~ status->jpeg_saturation -= 4;
    status->zoom.nom = get_uint32_be(&buf[0x1A0]);
    status->zoom.denom = 100;
    status->focus = get_int32_be(&buf[0x1A8]); // ?
    status->lens_id1 = get_uint32_be(&buf[0x190]) & 0x0F;
    status->lens_id2 = get_uint32_be( &buf[0x19C]);
}

// status check seems to be the same as K30
static
void ipslr_status_parse_k01(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    //~ status->jpeg_contrast -= 4;
    //~ status->jpeg_hue -= 4;
    //~ status->jpeg_sharpness -= 4;
    //~ status->jpeg_saturation -= 4;
    status->zoom.nom = get_uint32_be(&buf[0x1A0]); // - good for K01
    status->zoom.denom = 100; // good for K-01
    status->focus = get_int32_be(&buf[0x1A8]); // ? - good for K01
    status->lens_id1 = get_uint32_be(&buf[0x190]) & 0x0F; // - good for K01
    status->lens_id2 = get_uint32_be( &buf[0x19C]); // - good for K01
}

static
void ipslr_status_parse_k50(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    status->zoom.nom = get_uint32_be(&buf[0x1A0]);
    status->zoom.denom = get_uint32_be(&buf[0x1A4]);
    //    status->focus = get_int32_be(&buf[0x1A8]); // ?
    status->lens_id1 = get_uint32_be(&buf[0x190]) & 0x0F;
    status->lens_id2 = get_uint32_be( &buf[0x19C]);
}

static
void ipslr_status_parse_k500(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    status->zoom.nom = get_uint32_be(&buf[0x1A0]);
    status->zoom.denom = get_uint32_be(&buf[0x1A4]);
    //    status->focus = get_int32_be(&buf[0x1A8]); // ?
    status->lens_id1 = get_uint32_be(&buf[0x190]) & 0x0F;
    status->lens_id2 = get_uint32_be( &buf[0x19C]);
    // cannot read max_shutter_speed from status buffer, hardwire the values here
    status->max_shutter_speed.nom = 1;
    status->max_shutter_speed.denom = 6000;
}

static
void ipslr_status_parse_km(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, -4);
    status->zoom.nom = get_uint32_be(&buf[0x180]);
    status->zoom.denom = get_uint32_be(&buf[0x184]);
    status->lens_id1 = get_uint32_be(&buf[0x170]) & 0x0F;
    status->lens_id2 = get_uint32_be( &buf[0x17c]);
// TODO
// status.focused = getInt32(statusBuf, 0x164);
}

// K-3 returns data in little-endian
static
void ipslr_status_parse_k3(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    status->bufmask = get_uint16_le( &buf[0x1C]);
    status->zoom.nom = get_uint32_le(&buf[0x1A0]);
    status->zoom.denom = get_uint32_le(&buf[0x1A4]);
    status->focus = get_int32_le(&buf[0x1A8]);
    status->lens_id1 = get_uint32_le(&buf[0x190]) & 0x0F;
    status->lens_id2 = get_uint32_le( &buf[0x19C]);
    // cannot read max_shutter_speed from status buffer, hardwire the values here
    status->max_shutter_speed.nom = 1;
    status->max_shutter_speed.denom = 8000;
}

static
void ipslr_status_parse_ks1(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
    status->bufmask = get_uint16_le( &buf[0x0C]);
    status->zoom.nom = get_uint32_le(&buf[0x1A0]);
    status->zoom.denom = get_uint32_le(&buf[0x1A4]);
    status->focus = get_int32_le(&buf[0x1A8]);
    status->lens_id1 = get_uint32_le(&buf[0x190]) & 0x0F;
    status->lens_id2 = get_uint32_le( &buf[0x19C]);
}


static
void ipslr_status_parse_k1(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
// parse_common returns invalid values for the following fields. Fixing the fields:

    status->jpeg_hue = get_uint32_le(&buf[0x100]);
    status->current_shutter_speed.nom = get_uint32_le(&buf[0x110]);
    status->current_shutter_speed.denom = get_uint32_le(&buf[0x114]);
    status->current_aperture.nom = get_uint32_le(&buf[0x118]);
    status->current_aperture.denom = get_uint32_le(&buf[0x11c]);
    status->max_shutter_speed.nom = get_uint32_le(&buf[0x130]);
    status->max_shutter_speed.denom = get_uint32_le(&buf[0x134]);
    status->current_iso = get_uint32_le(&buf[0x138]);
    status->light_meter_flags = get_uint32_le(&buf[0x140]); // ?
    status->lens_min_aperture.nom = get_uint32_le(&buf[0x148]);
    status->lens_min_aperture.denom = get_uint32_le(&buf[0x14c]);
    status->lens_max_aperture.nom = get_uint32_le(&buf[0x150]);
    status->lens_max_aperture.denom = get_uint32_le(&buf[0x154]);
    status->manual_mode_ev = get_uint32_le(&buf[0x160]); // ?
    status->focused_af_point = get_uint32_le(&buf[0x16c]); // ?
    status->battery_1 = get_uint32_le(&buf[0x174]);
    status->battery_2 = get_uint32_le(&buf[0x178]);

    // selected_af_point
    // toprow left: 0x04000000
    // toprow leftmiddle: 0x02000000
    // toprow middle: 0x01000000
    // toprow rightmiddle: 0x00800000
    // bottomright: 0x00000004

    status->bufmask = get_uint16_le( &buf[0x0C]);
    status->zoom.nom = get_uint32_le(&buf[0x1A4]);
    status->zoom.denom = get_uint32_le(&buf[0x1A8]);
//    status->focus = get_int32_le(&buf[0x1A8]);
    status->lens_id1 = get_uint32_le(&buf[0x194]) & 0x0F;
    status->lens_id2 = get_uint32_le( &buf[0x1A0]);
}

static
void ipslr_status_parse_k70(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    ipslr_status_parse_common( p, status, 0 );
// parse_common returns invalid values for the following fields. Fixing the fields:

    status->auto_bracket_picture_counter = get_uint32_le(&buf[0xE8]);
    status->jpeg_hue = get_uint32_le(&buf[0x100]);
    status->current_shutter_speed.nom = get_uint32_le(&buf[0x110]);
    status->current_shutter_speed.denom = get_uint32_le(&buf[0x114]);
    status->current_aperture.nom = get_uint32_le(&buf[0x118]);
    status->current_aperture.denom = get_uint32_le(&buf[0x11c]);
    status->max_shutter_speed.nom = get_uint32_le(&buf[0x130]);
    status->max_shutter_speed.denom = get_uint32_le(&buf[0x134]);
    status->current_iso = get_uint32_le(&buf[0x138]);
    status->light_meter_flags = get_uint32_le(&buf[0x140]); // ?
    status->lens_min_aperture.nom = get_uint32_le(&buf[0x148]);
    status->lens_min_aperture.denom = get_uint32_le(&buf[0x14c]);
    status->lens_max_aperture.nom = get_uint32_le(&buf[0x150]);
    status->lens_max_aperture.denom = get_uint32_le(&buf[0x154]);
    status->manual_mode_ev = get_uint32_le(&buf[0x160]); // ?
    status->focused_af_point = get_uint32_le(&buf[0x16c]); // ?

    switch ( status->af_point_select) {
        case 0:
            status->af_point_select=PSLR_AF_POINT_SEL_SPOT;
            break;
        case 1:
            status->af_point_select=PSLR_AF_POINT_SEL_SELECT;
            break;
        case 2:
            status->af_point_select=PSLR_AF_POINT_SEL_EXPANDED;
            break;
        case 5:
            status->af_point_select=PSLR_AF_POINT_SEL_AUTO_5;
            break;
        case 6:
            status->af_point_select=PSLR_AF_POINT_SEL_AUTO_11;
            break;
    }

    status->battery_1 = get_uint32_le(&buf[0x174]);
    status->battery_2 = get_uint32_le(&buf[0x178]);
    status->battery_3 = 0;
    status->battery_4 = 0;

    uint32_t converted_selected_af_point=0;
    int convert_bit_index[11] = { 26, 24, 22, 1, 16, 14, 12, 0, 6, 4, 2};
    int bitidx=0;
    for (bitidx=0; bitidx<11; ++bitidx) {
        if (status->selected_af_point & 1<<convert_bit_index[bitidx]) {
            converted_selected_af_point |= 1 << bitidx;
        }
    }
    status->selected_af_point = converted_selected_af_point;

    status->bufmask = get_uint16_le( &buf[0x0C]);
    status->zoom.nom = get_uint32_le(&buf[0x1A4]);
    status->zoom.denom = get_uint32_le(&buf[0x1A8]);
//    status->focus = get_int32_le(&buf[0x1A8]);
    status->lens_id1 = get_uint32_le(&buf[0x194]) & 0x0F;
    status->lens_id2 = get_uint32_le( &buf[0x1A0]);
    status->shake_reduction = get_uint32_le(&buf[0xe4]);
}

static
void ipslr_status_parse_k200d(ipslr_handle_t *p, pslr_status *status) {
    uint8_t *buf = p->status_buffer;
    if ( PSLR_DEBUG_ENABLED ) {
        ipslr_status_diff(buf);
    }

    memset(status, 0, sizeof (*status));
    status->bufmask = get_uint16_be(&buf[0x16]);
    status->user_mode_flag = get_uint32_be(&buf[0x1c]);
    status->set_shutter_speed.nom = get_uint32_be(&buf[0x2c]);
    status->set_shutter_speed.denom = get_uint32_be(&buf[0x30]);
    status->current_aperture.nom = get_uint32_be(&buf[0x034]);
    status->current_aperture.denom = get_uint32_be(&buf[0x038]);
    status->set_aperture.nom = get_uint32_be(&buf[0x34]);
    status->set_aperture.denom = get_uint32_be(&buf[0x38]);
    status->ec.nom = get_uint32_be(&buf[0x3c]);
    status->ec.denom = get_uint32_be(&buf[0x40]);
    status->current_iso = get_uint32_be(&buf[0x060]);
    status->fixed_iso = get_uint32_be(&buf[0x60]);
    status->auto_iso_min = get_uint32_be(&buf[0x64]);
    status->auto_iso_max = get_uint32_be(&buf[0x68]);
    status->image_format = get_uint32_be(&buf[0x78]);
    status->jpeg_resolution = get_uint32_be(&buf[0x7c]);
    status->jpeg_quality = _get_user_jpeg_stars( p->model, get_uint32_be(&buf[0x80]));
    status->raw_format = get_uint32_be(&buf[0x84]);
    status->jpeg_image_tone = get_uint32_be(&buf[0x88]);
    status->jpeg_saturation = get_uint32_be(&buf[0x8c]);
    status->jpeg_sharpness = get_uint32_be(&buf[0x90]);
    status->jpeg_contrast = get_uint32_be(&buf[0x94]);
    //status->custom_ev_steps = get_uint32_be(&buf[0x9c]);
    //status->custom_sensitivity_steps = get_uint32_be(&buf[0xa0]);
    status->exposure_mode = get_uint32_be(&buf[0xac]);
    status->af_mode = get_uint32_be(&buf[0xb8]);
    status->af_point_select = get_uint32_be(&buf[0xbc]);
    status->selected_af_point = get_uint32_be(&buf[0xc0]);
    status->drive_mode = get_uint32_be(&buf[0xcc]);
    status->shake_reduction = get_uint32_be(&buf[0xda]);
    status->jpeg_hue = get_uint32_be(&buf[0xf4]);
    status->current_shutter_speed.nom = get_uint32_be(&buf[0x0104]);
    status->current_shutter_speed.denom = get_uint32_be(&buf[0x108]);
    status->light_meter_flags = get_uint32_be(&buf[0x124]);
    status->lens_min_aperture.nom = get_uint32_be(&buf[0x13c]);
    status->lens_min_aperture.denom = get_uint32_be(&buf[0x140]);
    status->lens_max_aperture.nom = get_uint32_be(&buf[0x144]);
    status->lens_max_aperture.denom = get_uint32_be(&buf[0x148]);
    status->focused_af_point = get_uint32_be(&buf[0x150]);
    status->zoom.nom = get_uint32_be(&buf[0x17c]);
    status->zoom.denom = get_uint32_be(&buf[0x180]);
    status->focus = get_int32_be(&buf[0x184]);
    // Drive mode: 0=Single shot, 1= Continous Hi, 2= Continous Low or Self timer 12s, 3=Self timer 2s
    // 4= remote, 5= remote 3s delay
}

pslr_setting_def_t *pslr_find_setting_by_name (pslr_setting_def_t *array, int array_length, char *name) {
    if (array == NULL || array_length == 0) {
        return NULL;
    }
    int i;
    for ( i = 0; i<array_length; i++) {
        if ( strncmp(array[i].name, name, strlen(name))==0 ) {
            return &array[i];
        }
    }
    return NULL;
}

static
char *read_json_file(int *jsonsize) {
    int jsonfd = open("pentax_settings.json", O_RDONLY);
    if (jsonfd == -1) {
        // cannot find in the current directory, also checking PKTDATADIR
        if (dir_exists(PKTDATADIR)) {
            jsonfd = open(PKTDATADIR "/pentax_settings.json", O_RDONLY);
        }
        if (jsonfd == -1) {
            pslr_write_log(PSLR_ERROR, "Cannot open pentax_settings.json file\n");
            return NULL;
        }
    }
    *jsonsize = lseek(jsonfd, 0, SEEK_END);
    lseek(jsonfd, 0, SEEK_SET);
    char *jsontext=malloc(*jsonsize);
    ssize_t ret = read(jsonfd, jsontext, *jsonsize);
    if (ret < *jsonsize) {
        fprintf(stderr, "Could not read pentax_settings.json file\n");
        free(jsontext);
        return NULL;
    }
    DPRINT("json text:\n%.*s\n", *jsonsize, jsontext);
    return jsontext;
}

pslr_setting_def_t *setting_file_process(const char *cameraid, int *def_num) {
    pslr_setting_def_t defs[128];
    *def_num=0;
    if (jsontext == NULL) {
        jsontext = read_json_file(&jsonsize);
    }
    size_t json_part_length;
    const char *json_part;
    if (!(json_part = js0n(cameraid, strlen(cameraid), jsontext, jsonsize, &json_part_length))) {
        pslr_write_log(PSLR_ERROR, "JSON: Cannot find camera model\n");
        return NULL;
    }

    if (!(json_part = js0n("fields", strlen("fields"), json_part, json_part_length, &json_part_length))) {
        pslr_write_log(PSLR_ERROR, "JSON: No fields defined for the camera model\n");
        return NULL;
    }
    int ai=0;
    const char *json_array_part;
    size_t json_array_part_length;
    size_t name_length, type_length, value_length, address_length;
    while ((json_array_part = js0n(NULL, ai, json_part, json_part_length, &json_array_part_length))) {
        const char *camera_field_name_ptr;
        char *camera_field_name;
        if (!(camera_field_name_ptr=js0n( "name", strlen("name"), json_array_part, json_array_part_length, &name_length))) {
            pslr_write_log(PSLR_ERROR, "No name is defined\n");
            return NULL;
        } else {
            camera_field_name=malloc(name_length+1);
            memcpy(camera_field_name, camera_field_name_ptr, name_length);
            camera_field_name[name_length]='\0';
        }
        const char *camera_field_type_ptr;
        char *camera_field_type;
        if (!(camera_field_type_ptr=js0n( "type", strlen("type"), json_array_part, json_array_part_length, &type_length))) {
            pslr_write_log(PSLR_ERROR, "No type is defined\n");
            return NULL;
        } else {
            camera_field_type=malloc(type_length+1);
            memcpy(camera_field_type, camera_field_type_ptr, type_length);
            camera_field_type[type_length]='\0';
        }
        const char *camera_field_value_ptr;
        char *camera_field_value;
        if (!(camera_field_value_ptr=js0n( "value", strlen("value"), json_array_part, json_array_part_length, &value_length))) {
            camera_field_value=NULL;
        } else {
            camera_field_value=malloc(value_length+1);
            memcpy(camera_field_value, camera_field_value_ptr, value_length);
            camera_field_value[value_length]='\0';
        }

        const char *camera_field_address_ptr;
        char *camera_field_address;
        if (!(camera_field_address_ptr=js0n( "address", strlen("address"), json_array_part, json_array_part_length, &address_length))) {
            camera_field_address=NULL;
        } else {
            camera_field_address=malloc(address_length+1);
            memcpy(camera_field_address, camera_field_address_ptr, address_length);
            camera_field_address[address_length]='\0';
        }
        DPRINT("name: %.*s %.*s %.*s %.*s\n", (int)name_length, camera_field_name, (int)address_length, camera_field_address, (int)value_length, camera_field_value, (int)type_length, camera_field_type);
        pslr_setting_def_t setting_def = { camera_field_name, camera_field_address==NULL?0:strtoul(camera_field_address,NULL,16), camera_field_value, camera_field_type };
        defs[(*def_num)++]=setting_def;
        ++ai;
    }
    pslr_setting_def_t *ret=malloc(*def_num*sizeof(pslr_setting_def_t));
    //        printf("return %d defs\n",*def_num);
    memcpy(ret, defs, *def_num*sizeof(pslr_setting_def_t));
    return ret;
}

pslr_bool_setting ipslr_settings_parse_bool(const uint8_t *buf, const pslr_setting_def_t *def) {
    pslr_bool_setting bool_setting;
    if (def->value != NULL) {
        bool_setting = (pslr_bool_setting) {
            PSLR_SETTING_STATUS_HARDWIRED, strcmp("false", def->value) == 0 ? false : true
        };
    } else if (def->address != 0) {
        uint8_t target = strcmp(def->type, "boolean!") == 0 ? 0 : 1;
        bool_setting = (pslr_bool_setting) {
            PSLR_SETTING_STATUS_READ, buf[def->address] == target
        };
    } else {
        bool_setting = (pslr_bool_setting) {
            PSLR_SETTING_STATUS_NA, false
        };
    }
    return bool_setting;
}

pslr_uint16_setting ipslr_settings_parse_uint16(const uint8_t *buf, const pslr_setting_def_t *def) {
    pslr_uint16_setting uint16_setting;
    if (def->value != NULL) {
        uint16_setting = (pslr_uint16_setting) {
            PSLR_SETTING_STATUS_HARDWIRED, atoi(def->value)
        };
    } else if (def->address != 0) {
        uint16_setting = (pslr_uint16_setting) {
            PSLR_SETTING_STATUS_READ, get_uint16_be(&buf[def->address])
        };
    } else {
        uint16_setting = (pslr_uint16_setting) {
            PSLR_SETTING_STATUS_NA, 0
        };
    }
    return uint16_setting;
}

void ipslr_settings_parser_json(const char *cameraid, ipslr_handle_t *p, pslr_settings *settings) {
    uint8_t *buf = p->settings_buffer;
    memset(settings, 0, sizeof (*settings));
    int def_num;

    pslr_setting_def_t *defs = setting_file_process(cameraid,&def_num);
    int def_index=0;
    while (def_index < def_num) {
        pslr_bool_setting bool_setting;
        pslr_uint16_setting uint16_setting;
        pslr_setting_def_t def = defs[def_index];
        if (strncmp(def.type, "boolean", 7) == 0) {
            bool_setting = ipslr_settings_parse_bool(buf, &def);
        } else if (strcmp(def.type, "uint16") == 0) {
            uint16_setting = ipslr_settings_parse_uint16(buf, &def);
        } else {
            pslr_write_log(PSLR_ERROR, "Invalid json type: %s\n", def.type);
        }
        if (strcmp(def.name, "bulb_mode_press_press") == 0) {
            settings->bulb_mode_press_press = bool_setting;
        } else if (strcmp(def.name, "remote_bulb_mode_press_press") == 0) {
            settings->remote_bulb_mode_press_press = bool_setting;
        } else if (strcmp(def.name, "one_push_bracketing") == 0) {
            settings->one_push_bracketing = bool_setting;
        } else if (strcmp(def.name, "bulb_timer") == 0) {
            settings->bulb_timer = bool_setting;
        } else if (strcmp(def.name, "bulb_timer_sec") == 0) {
            settings->bulb_timer_sec = uint16_setting;
        } else if (strcmp(def.name, "using_aperture_ring") == 0) {
            settings->using_aperture_ring = bool_setting;
        } else if (strcmp(def.name, "shake_reduction") == 0) {
            settings->shake_reduction = bool_setting;
        } else if (strcmp(def.name, "astrotracer") == 0) {
            settings->astrotracer = bool_setting;
        } else if (strcmp(def.name, "astrotracer_timer_sec") == 0) {
            settings->astrotracer_timer_sec = uint16_setting;
        } else if (strcmp(def.name, "horizon_correction") == 0) {
            settings->horizon_correction = bool_setting;
        }
        ++def_index;
    }
}


ipslr_model_info_t camera_models[] = {
    { 0x12aa2, "*ist DS",     true,  true,  true,  false, false, false, 264, 3, {6, 4, 2},       5, 4000, 200, 3200, 200,  3200,  PSLR_JPEG_IMAGE_TONE_BRIGHT,           false, 11, ipslr_status_parse_istds},
    { 0x12cd2, "K20D",        false, true,  true,  false, false, false, 412, 4, {14, 10, 6, 2},  7, 4000, 100, 3200, 100,  6400,  PSLR_JPEG_IMAGE_TONE_MONOCHROME,       true,  11, ipslr_status_parse_k20d},
    { 0x12c1e, "K10D",        false, true,  true,  false, false, false, 392, 3, {10, 6, 2},      7, 4000, 100, 1600, 100,  1600,  PSLR_JPEG_IMAGE_TONE_BRIGHT,           false, 11, ipslr_status_parse_k10d},
    { 0x12c20, "GX10",        false, true,  true,  false, false, false, 392, 3, {10, 6, 2},      7, 4000, 100, 1600, 100,  1600,  PSLR_JPEG_IMAGE_TONE_BRIGHT,           false, 11, ipslr_status_parse_k10d},
    { 0x12cd4, "GX20",        false, true,  true,  false, false, false, 412, 4, {14, 10, 6, 2},  7, 4000, 100, 3200, 100,  6400,  PSLR_JPEG_IMAGE_TONE_MONOCHROME,       true,  11, ipslr_status_parse_k20d},
    { 0x12dfe, "K-x",         false, true,  true,  false, false, false, 436, 3, {12, 10, 6, 2},  9, 6000, 200, 6400, 100, 12800,  PSLR_JPEG_IMAGE_TONE_MONOCHROME,       true,  11, ipslr_status_parse_kx}, //muted: bug
    { 0x12cfa, "K200D",       false, true,  true,  false, false, false, 408, 3, {10, 6, 2},      9, 4000, 100, 1600, 100,  1600,  PSLR_JPEG_IMAGE_TONE_MONOCHROME,       true,  11, ipslr_status_parse_k200d},
    { 0x12db8, "K-7",         false, true,  true,  false, false, false, 436, 4, {14, 10, 6, 2},  9, 8000, 100, 3200, 100,  6400,  PSLR_JPEG_IMAGE_TONE_MUTED,            true,  11, ipslr_status_parse_kx},
    { 0x12e6c, "K-r",         false, true,  true,  false, false, false, 440, 3, {12, 10, 6, 2},  9, 6000, 200,12800, 100, 25600,  PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  11, ipslr_status_parse_kr},
    { 0x12e76, "K-5",         false, true,  true,  false, false, false, 444, 4, {16, 10, 6, 2},  9, 8000, 100,12800,  80, 51200,  PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  11, ipslr_status_parse_k5},
    { 0x12d72, "K-2000",      false, true,  true,  false, false, false, 412, 3, {10, 6, 2},      9, 4000, 100, 3200, 100,  3200,  PSLR_JPEG_IMAGE_TONE_MONOCHROME,       true,  11, ipslr_status_parse_km},
    { 0x12d73, "K-m",         false, true,  true,  false, false, false, 412, 3, {10, 6, 2},      9, 4000, 100, 3200, 100,  3200,  PSLR_JPEG_IMAGE_TONE_MONOCHROME,       true,  11, ipslr_status_parse_km},
    { 0x12f52, "K-30",        false, true,  false, false, false, false, 452, 3, {16, 12, 8, 5},  9, 6000, 100,12800, 100, 25600,  PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  11, ipslr_status_parse_k30},
    { 0x12ef8, "K-01",        false, true,  true,  false, false, false, 452, 3, {16, 12, 8, 5},  9, 4000, 100,12800, 100, 25600,  PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  11, ipslr_status_parse_k01},
    { 0x12f70, "K-5II",       false, true,  true,  false, false, false, 444,  4, {16, 10, 6, 2}, 9, 8000, 100, 12800, 80, 51200,  PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  11, ipslr_status_parse_k5},
    { 0x12f71, "K-5IIs",      false, true,  true,  false, false, false, 444,  4, {16, 10, 6, 2}, 9, 8000, 100, 12800, 80, 51200,  PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  11, ipslr_status_parse_k5},
    { 0x12fb6, "K-50",        false, true,  true,  false, false, false, 452,  4, {16, 12, 8, 5}, 9, 6000, 100, 51200, 100, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  11, ipslr_status_parse_k50},
    { 0x12fc0, "K-3",         false, true,  true,  false, false, true,  452,  4, {24, 14, 6, 2}, 9, 8000, 100, 51200, 100, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  27, ipslr_status_parse_k3},
    { 0x1309c, "K-3II",       false, false, true,  true,  false, true,  452,  4, {24, 14, 6, 2}, 9, 8000, 100, 51200, 100, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS,    true,  27, ipslr_status_parse_k3},
    { 0x12fca, "K-500",       false, true,  true,  false, false, false, 452,  3, {16, 12, 8, 5}, 9, 6000, 100, 51200, 100, 51200, PSLR_JPEG_IMAGE_TONE_CROSS_PROCESSING, true,  11, ipslr_status_parse_k500},
    // only limited support from here
    { 0x12994, "*ist D",      true,  true,  true,  false, false, false, 0,   3, {6, 4, 2}, 3, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_NONE,   false, 11, NULL},   // buffersize: 264
    { 0x12b60, "*ist DS2",    true,  true,  true,  false, false, false, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, false, 11, NULL},
    { 0x12b1a, "*ist DL",     true,  true,  true,  false, false, false, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, false, 11, NULL},
    { 0x12b80, "GX-1L",       true,  true,  true,  false, false, false, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, false, 11, NULL},
    { 0x12b9d, "K110D",       false, true,  true,  false, false, false, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, false, 11, NULL},
    { 0x12b9c, "K100D",       true,  true,  true,  false, false, false, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, false, 11, NULL},
    { 0x12ba2, "K100D Super", true,  true,  true,  false, false, false, 0,   3, {6, 4, 2}, 5, 4000, 200, 3200, 200, 3200, PSLR_JPEG_IMAGE_TONE_BRIGHT, false, 11, NULL},
    { 0x1301a, "K-S1",        false, true,  true,  false, false, true,  452,  3, {20, 12, 6, 2}, 9, 6000, 100, 51200, 100, 51200, PSLR_JPEG_IMAGE_TONE_CROSS_PROCESSING, true,  11, ipslr_status_parse_ks1},
    { 0x13024, "K-S2",        false, true,  true,  false, false, true,  452,  3, {20, 12, 6, 2}, 9, 6000, 100, 51200, 100, 51200, PSLR_JPEG_IMAGE_TONE_CROSS_PROCESSING, true,  11, ipslr_status_parse_k3},
    { 0x13092, "K-1",         false, false, true,  true,  false, true,  456,  3, {36, 22, 12, 2}, 9, 8000, 100, 204800, 100, 204800, PSLR_JPEG_IMAGE_TONE_FLAT, true,  33, ipslr_status_parse_k1 },
    { 0x13240, "K-1 II",      false, false, true,  true,  false, true,  456,  3, {36, 22, 12, 2}, 9, 8000, 100, 819200, 100, 819200, PSLR_JPEG_IMAGE_TONE_FLAT, true,  33, ipslr_status_parse_k1 },
    { 0x13222, "K-70",        false, false, true,  true,  true,  true,  456,  3, {24, 14, 6, 2}, 9, 6000, 100, 102400, 100, 102400, PSLR_JPEG_IMAGE_TONE_AUTO, true,  11, ipslr_status_parse_k70},
    { 0x1322c, "KP",          false, false, true,  true,  false, true,  456,   3, {24, 14, 6, 2}, 9, 6000, 100, 819200, 100, 819200, PSLR_JPEG_IMAGE_TONE_AUTO, true,  27, ipslr_status_parse_k70},
    { 0x13010, "645Z",        false, false, true,  true,  false, false,  0,   3, {51, 32, 21, 3}, 9, 4000, 100, 204800, 100, 204800, PSLR_JPEG_IMAGE_TONE_CROSS_PROCESSING, true,  35, NULL},
    { 0x13254, "K-3III",      false, false, true,  true,  false, true,  452,  4, {24, 14, 6, 2}, 9, 8000, 100, 51200, 100, 51200, PSLR_JPEG_IMAGE_TONE_BLEACH_BYPASS, true,  27, ipslr_status_parse_k3}
};

ipslr_model_info_t *pslr_find_model_by_id( uint32_t id ) {
    unsigned int i;
    for ( i = 0; i<sizeof (camera_models) / sizeof (camera_models[0]); i++) {
        if ( camera_models[i].id == id ) {
            //    DPRINT("found %d\n",i);
            return &camera_models[i];
        }
    }
    //    DPRINT("not found\n");
    return NULL;
}
