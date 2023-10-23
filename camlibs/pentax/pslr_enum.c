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
#include "pslr_enum.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

const char* pslr_color_space_str[PSLR_COLOR_SPACE_MAX] = {
    "sRGB",
    "AdobeRGB"
};

const char* pslr_af_mode_str[PSLR_AF_MODE_MAX] = {
    "MF",
    "AF.S",
    "AF.C",
    "AF.A"
};

const char* pslr_ae_metering_str[PSLR_AE_METERING_MAX] = {
    "Multi",
    "Center",
    "Spot"
};

const char* pslr_flash_mode_str[PSLR_FLASH_MODE_MAX] = {
    "Manual",
    "Manual-RedEye",
    "Slow",
    "Slow-RedEye",
    "TrailingCurtain",
    "Auto",
    "Auto-RedEye",
    "TrailingCurtain", /* maybe in manual mode?? */
    "Wireless"
};

const char* pslr_drive_mode_str[PSLR_DRIVE_MODE_MAX] = {
    "Single", /* Bracketing also returns Single */
    "Continuous-HI",
    "SelfTimer-12",
    "SelfTimer-2",
    "Remote",
    "Remote-3",
    "Continuous-LO"
};

const char*  pslr_af_point_sel_str[PSLR_AF_POINT_SEL_MAX] = {
    "Auto-5",
    "Select",
    "Spot",
    "Auto-11",
    "Expanded"
};

const char*  pslr_af11_point_str[11] = {
    "topleft",
    "topmiddle",
    "topright",
    "farleft",
    "middleleft",
    "middlemiddle",
    "middleright",
    "farright",
    "bottomleft",
    "bottommiddle",
    "bottomright"
};

const char* pslr_jpeg_image_tone_str[PSLR_JPEG_IMAGE_TONE_MAX] = {
    "Natural",
    "Bright",
    "Portrait",
    "Landscape",
    "Vibrant",
    "Monochrome",
    "Muted",
    "ReversalFilm",
    "BleachBypass",
    "Radiant",
    "CrossProcessing",
    "Flat",
    "Auto"
};

const char* pslr_white_balance_mode_str[PSLR_WHITE_BALANCE_MODE_MAX] = {
    "Auto",
    "Daylight",
    "Shade",
    "Cloudy",
    "Fluorescent_D",
    "Fluorescent_N",
    "Fluorescent_W",
    "Tungsten",
    "Flash",
    "Manual", /* sometimes called Manual1 */
    "Manual2",
    "Manual3",
    "Kelvin1",
    "Kelvin2",
    "Kelvin3",
    "Fluorescent_L",
    "CTE",
    "MultiAuto"
};

const char* pslr_custom_ev_steps_str[PSLR_CUSTOM_EV_STEPS_MAX] = {
    "1/2",
    "1/3"
};

const char* pslr_custom_sensitivity_steps_str[PSLR_CUSTOM_SENSITIVITY_STEPS_MAX] = {
    "1",
    "As"
};

const char* pslr_image_format_str[PSLR_IMAGE_FORMAT_MAX] = {
    "JPEG",
    "RAW",
    "RAW+"
};

const char* pslr_raw_format_str[PSLR_RAW_FORMAT_MAX] = {
    "PEF",
    "DNG"
};

const char* pslr_scene_mode_str[PSLR_SCENE_MODE_MAX] = {
    "NONE",
    "HISPEED",
    "DOF",
    "MTF",
    "STANDARD",
    "PORTRAIT",
    "LANDSCAPE",
    "MACRO",
    "SPORT",
    "NIGHTSCENEPORTRAIT",
    "NOFLASH",
    "NIGHTSCENE",
    "SURFANDSNOW",
    "TEXT",
    "SUNSET",
    "KIDS",
    "PET",
    "CANDLELIGHT",
    "MUSEUM",
    "19",
    "FOOD",
    "STAGE",
    "NIGHTSNAP",
    "SWALLOWDOF",
    "24",
    "NIGHTSCENEHDR",
    "BLUESKY",
    "FOREST",
    "28",
    "BLACKLIGHTSILHOUETTE"
};


/* case insenstive comparison - strnicmp */
int str_comparison_i (const char *s1, const char *s2, int n) {
    if ( s1 == NULL ) {
        return s2 == NULL ? 0 : -(*s2);
    }
    if (s2 == NULL) {
        return *s1;
    }

    char c1='\0', c2='\0';
    int length=0;
    while ( length<n && (c1 = tolower (*s1)) == (c2 = tolower (*s2))) {
        if (*s1 == '\0') {
            break;
        }
        ++s1;
        ++s2;
        ++length;
    }
    return c1 - c2;
}

int find_in_array( const char** array, int length, char* str ) {
    int i;
    int found_index=-1;
    size_t found_index_length=0;
    size_t string_length;
    for ( i = 0; i<length; ++i ) {
        string_length = strlen(array[i]);
        if ( (str_comparison_i( array[i], str, string_length ) == 0) && (string_length > found_index_length) ) {
            found_index_length = string_length;
            found_index = i;
        }
    }
    return found_index;
}

static
const char *get_pslr_str( const char** array, int length, int value ) {
    if (value >=0 && value < length) {
        return array[value];
    } else {
        char *ret = malloc(128);
        sprintf (ret, "Unknown value: %d", value);
        return ret;
    }
}


pslr_color_space_t pslr_get_color_space( char *str ) {
    return find_in_array( pslr_color_space_str, sizeof(pslr_color_space_str)/sizeof(pslr_color_space_str[0]),str);
}

const char *pslr_get_color_space_str( pslr_color_space_t value ) {
    return get_pslr_str( pslr_color_space_str, sizeof(pslr_color_space_str)/sizeof(pslr_color_space_str[0]),value);
}

pslr_af_mode_t pslr_get_af_mode( char *str ) {
    return find_in_array( pslr_af_mode_str, sizeof(pslr_af_mode_str)/sizeof(pslr_af_mode_str[0]),str);
}

const char *pslr_get_af_mode_str( pslr_af_mode_t value ) {
    return get_pslr_str( pslr_af_mode_str, sizeof(pslr_af_mode_str)/sizeof(pslr_af_mode_str[0]),value);
}

pslr_ae_metering_t pslr_get_ae_metering( char *str ) {
    return find_in_array( pslr_ae_metering_str, sizeof(pslr_ae_metering_str)/sizeof(pslr_ae_metering_str[0]),str);
}

const char *pslr_get_ae_metering_str( pslr_ae_metering_t value ) {
    return get_pslr_str( pslr_ae_metering_str, sizeof(pslr_ae_metering_str)/sizeof(pslr_ae_metering_str[0]),value);
}

pslr_flash_mode_t pslr_get_flash_mode( char *str ) {
    return find_in_array( pslr_flash_mode_str, sizeof(pslr_flash_mode_str)/sizeof(pslr_flash_mode_str[0]),str);
}

const char *pslr_get_flash_mode_str( pslr_flash_mode_t value ) {
    return get_pslr_str( pslr_flash_mode_str, sizeof(pslr_flash_mode_str)/sizeof(pslr_flash_mode_str[0]),value);
}

pslr_drive_mode_t pslr_get_drive_mode( char *str ) {
    return find_in_array( pslr_drive_mode_str, sizeof(pslr_drive_mode_str)/sizeof(pslr_drive_mode_str[0]),str);
}

const char *pslr_get_drive_mode_str( pslr_drive_mode_t value ) {
    return get_pslr_str( pslr_drive_mode_str, sizeof(pslr_drive_mode_str)/sizeof(pslr_drive_mode_str[0]),value);
}

pslr_af_point_sel_t pslr_get_af_point_sel( char *str ) {
    return find_in_array( pslr_af_point_sel_str, sizeof(pslr_af_point_sel_str)/sizeof(pslr_af_point_sel_str[0]),str);
}

const char *pslr_get_af_point_sel_str( pslr_af_point_sel_t value ) {
    return get_pslr_str( pslr_af_point_sel_str, sizeof(pslr_af_point_sel_str)/sizeof(pslr_af_point_sel_str[0]),value);
}

char *pslr_get_af11_point_str( uint32_t value ) {
    if (value==0) {
        return "none";
    }
    int bitidx=0;
    char *ret = malloc(1024);
    int pos = sprintf(ret, "%s", "");
    while (value>0 && bitidx<11) {
        if ((value & 0x01) == 1) {
            int written = sprintf(ret + pos, "%s%s", pos == 0 ? "" : ",", pslr_af11_point_str[bitidx]);
            if (written < 0) {
                return ret;
            }
            pos += written;
        }
        value >>= 1;
        ++bitidx;
    }
    if (value>0) {
        sprintf(ret, "%s", "invalid");
    }
    return ret;
}


pslr_jpeg_image_tone_t pslr_get_jpeg_image_tone( char *str ) {
    return find_in_array( pslr_jpeg_image_tone_str, sizeof(pslr_jpeg_image_tone_str)/sizeof(pslr_jpeg_image_tone_str[0]),str);
}

const char *pslr_get_jpeg_image_tone_str( pslr_jpeg_image_tone_t value ) {
    return get_pslr_str( pslr_jpeg_image_tone_str, sizeof(pslr_jpeg_image_tone_str)/sizeof(pslr_jpeg_image_tone_str[0]),value);
}

pslr_white_balance_mode_t pslr_get_white_balance_mode( char *str ) {
    return find_in_array( pslr_white_balance_mode_str, sizeof(pslr_white_balance_mode_str)/sizeof(pslr_white_balance_mode_str[0]),str);
}

const char *pslr_get_white_balance_mode_str( pslr_white_balance_mode_t value ) {
    return get_pslr_str( pslr_white_balance_mode_str, sizeof(pslr_white_balance_mode_str)/sizeof(pslr_white_balance_mode_str[0]),value);
}

const char *pslr_get_custom_ev_steps_str( pslr_custom_ev_steps_t value ) {
    return get_pslr_str( pslr_custom_ev_steps_str, sizeof(pslr_custom_ev_steps_str)/sizeof(pslr_custom_ev_steps_str[0]),value);
}

const char *pslr_get_custom_sensitivity_steps_str( pslr_custom_sensitivity_steps_t value ) {
    return get_pslr_str( pslr_custom_sensitivity_steps_str, sizeof(pslr_custom_sensitivity_steps_str)/sizeof(pslr_custom_sensitivity_steps_str[0]),value);
}

const char *pslr_get_image_format_str( pslr_image_format_t value ) {
    return get_pslr_str( pslr_image_format_str, sizeof(pslr_image_format_str)/sizeof(pslr_image_format_str[0]),value);
}

const char *pslr_get_raw_format_str( pslr_raw_format_t value ) {
    return get_pslr_str( pslr_raw_format_str, sizeof(pslr_raw_format_str)/sizeof(pslr_raw_format_str[0]),value);
}

const char *pslr_get_scene_mode_str( pslr_scene_mode_t value ) {
    return get_pslr_str( pslr_scene_mode_str, sizeof(pslr_scene_mode_str)/sizeof(pslr_scene_mode_str[0]),value);
}
