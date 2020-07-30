/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright 2000 Adam Harrison <adam@antispin.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef STV0680_H
#define STV0680_H

#define CMDID_CLEAR_COMMS_ERROR           0x80
struct stv680_error_info {
    unsigned char error;
#define		CAMERR_OK				0
#define		CAMERR_BUSY				1
#define		CAMERR_TIMEOUT				2
#define		CAMERR_V2W_ERROR			3
#define		CAMERR_COMMS_ERROR			4
#define		CAMERR_BAD_EXPOSURE			5
#define		CAMERR_BAD_INDEX			6
#define		CAMERR_CAMERA_FULL			7
#define		CAMERR_BAD_COMMAND			8
#define		CAMERR_BAD_PARAM			9
#define		CAMERR_BAD_DATALEN			10
#define		CAMERR_TASK_FAILED			11
#define		CAMERR_FLASH_PROGRAM_FAILED		12
#define		CAMERR_BAD_ADDRESS			13
#define		CAMERR_BAD_PAGE				14
#define		CAMERR_EXISTING_IMAGE_SMALLER		15
#define		CAMERR_COMMAND_NOT_ALLOWED		16
#define		CAMERR_NO_SENSOR_DETECTED		17
#define		CAMERR_COLOUR_MATRIX_UNAVAILABLE	18
    unsigned char info;
};

#define CMDID_PING			0x88

#define CMDID_GET_CAMERA_INFO		0x85
struct stv680_camera_info {
    unsigned char	firmware_revision[2];
    unsigned char	asic_revision[2];
    unsigned char	sensor_id[2];
    unsigned char	hardware_config;

#define HWCONFIG_COMMSLINK_MASK			0x01
#define HWCONFIG_COMMSLINK_USB			0x00
#define HWCONFIG_COMMSLINK_SERIAL		0x01

#define HWCONFIG_FLICKERFREQ_MASK		0x02
#define HWCONFIG_FLICKERFREQ_50HZ		0x00
#define HWCONFIG_FLICKERFREQ_60HZ		0x02

#define HWCONFIG_MEMSIZE_MASK			0x04
#define HWCONFIG_MEMSIZE_64MBIT			0x00
#define HWCONFIG_MEMSIZE_16MBIT			0x04

#define HWCONFIG_HAS_THUMBNAILS			0x08
#define HWCONFIG_HAS_VIDEO			0x10
#define HWCONFIG_STARTUP_COMPLETED		0x20
#define HWCONFIG_IS_MONOCHROME			0x40
#define HWCONFIG_MEM_FITTED			0x80

    unsigned char	capabilities;
#define CAP_CIF					1
#define CAP_VGA					2
#define CAP_QCIF				4
#define CAP_QVGA				8

    unsigned char	vendor_id[2];
    unsigned char	product_id[2];

    unsigned char	reserved[4];

};

#define CMDID_GET_CAMERA_MODE			0x87
struct stv680_camera_mode {
    unsigned char format;
#define FORMAT_CIF				0
#define FORMAT_VGA				1
#define FORMAT_QCIF				2
#define FORMAT_QVGA				3
    unsigned char reserved[7];
};

#define CMDID_GET_IMAGE_INFO			0x86
struct stv680_image_info {
    unsigned char index[2];
    unsigned char maximages[2];
    unsigned char width[2];
    unsigned char height[2];
    unsigned char size[4];
    unsigned char thumb_width;
    unsigned char thumb_height;
    unsigned char thumb_size[2];
};

#define CMDID_UPLOAD_IMAGE			0x83
#define CMDID_UPLOAD_THUMBNAIL			0x84
#define CMDID_GET_IMAGE_HEADER			0x8f
struct stv680_image_header { /* For all upload and get image header */
    unsigned char size[4];
    unsigned char width[2];
    unsigned char height[2];
    unsigned char fine_exposure[2];
    unsigned char coarse_exposure[2];
    unsigned char sensor_gain;
    unsigned char sensor_clkdiv;
    unsigned char avg_pixel_value;
    unsigned char flags;
#define IMAGE_IS_VALID		0x01
#define IMAGE_FROM_STREAM	0x04
#define IMAGE_FIRST_OF_MOVIE	0x10
};

#define CMDID_GRAB_UPLOAD			0x89
struct stv680_image_header_small {
    unsigned char size[4];
    unsigned char width[2];
    unsigned char height[2];
};

#define CMDID_GRAB_IMAGE			0x05
#define 	GRAB_UPDATE_INDEX	0x1000
#define 	GRAB_TO_LAST_LOCATION	0x2000
#define 	GRAB_SUPPRESS_BEEP	0x4000
#define 	GRAB_USE_CAMERA_INDEX	0x8000

#define CMDID_SET_IMAGE_INDEX			0x06
#define CMDID_START_VIDEO			0x09
#define CMDID_STOP_VIDEO			0x0a

#define CMDID_GET_USER_INFO			0x8d
struct stv680_user_info {
    unsigned char buttonmask;
    unsigned char camera_mode;
#define CAMERA_MODE_START			1
#define CAMERA_MODE_VIDEO			2
#define CAMERA_MODE_BUSY			4
#define CAMERA_MODE_IDLE			8
    unsigned char index[2];
    unsigned char maximages[2];
    unsigned char current_format;
    unsigned char reserved;
};

#define STV0680_QCIF_WIDTH	178
#define STV0680_QCIF_HEIGHT	146
#define STV0680_CIF_WIDTH	356
#define STV0680_CIF_HEIGHT	292
#define STV0680_QVGA_WIDTH	324
#define STV0680_QVGA_HEIGHT	244
#define STV0680_VGA_WIDTH	644
#define STV0680_VGA_HEIGHT	484

int stv0680_try_cmd(GPPort *port, unsigned char cmd,
                unsigned short data,
                unsigned char *response, unsigned char response_len);


#endif
