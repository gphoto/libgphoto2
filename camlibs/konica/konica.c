/* konica.c
 *
 * Copyright (C) 2001 Lutz Müller
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>

#include <gphoto2.h>
#include <gphoto2-port-log.h>

#include "library.h"
#include "lowlevel.h"
#include "konica.h"

#define GP_MODULE "konica"
#define CHECK_NULL(r)     {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(r,f) {			\
	int res = (r);				\
	if (res < 0) {				\
		if (f)				\
			free (f);		\
		return (res);			\
	}					\
	switch (((f)[3] << 8) | (f)[2]) {	\
	case 0x0000:				\
		break;				\
        case 0x0101:				\
                return (KONICA_ERROR_FOCUSING_ERROR);\
        case 0x0102:\
                return (KONICA_ERROR_IRIS_ERROR);\
        case 0x0201:\
                return (KONICA_ERROR_STROBE_ERROR);\
        case 0x0203:\
                return (KONICA_ERROR_EEPROM_CHECKSUM_ERROR);\
        case 0x0205:\
                return (KONICA_ERROR_INTERNAL_ERROR1);\
        case 0x0206:\
                return (KONICA_ERROR_INTERNAL_ERROR2);\
        case 0x0301:\
                return (KONICA_ERROR_NO_CARD_PRESENT);\
        case 0x0311:\
                return (KONICA_ERROR_CARD_NOT_SUPPORTED);\
        case 0x0321:\
                return (KONICA_ERROR_CARD_REMOVED_DURING_ACCESS);\
        case 0x0340:\
                return (KONICA_ERROR_IMAGE_NUMBER_NOT_VALID);\
        case 0x0341:\
                return (KONICA_ERROR_CARD_CAN_NOT_BE_WRITTEN);\
        case 0x0381:\
                return (KONICA_ERROR_CARD_IS_WRITE_PROTECTED);\
        case 0x0382:\
                return (KONICA_ERROR_NO_SPACE_LEFT_ON_CARD);\
        case 0x0390:\
                return (KONICA_ERROR_IMAGE_PROTECTED);\
        case 0x0401:\
                return (KONICA_ERROR_LIGHT_TOO_DARK);\
        case 0x0402:\
                return (KONICA_ERROR_AUTOFOCUS_ERROR);\
        case 0x0501:\
                return (KONICA_ERROR_SYSTEM_ERROR);\
        case 0x0800:\
                return (KONICA_ERROR_ILLEGAL_PARAMETER);\
        case 0x0801:\
                return (KONICA_ERROR_COMMAND_CANNOT_BE_CANCELLED);\
        case 0x0b00:\
                return (KONICA_ERROR_LOCALIZATION_DATA_EXCESS);\
        case 0x0bff:\
                return (KONICA_ERROR_LOCALIZATION_DATA_CORRUPT);\
        case 0x0c01:\
                return (KONICA_ERROR_UNSUPPORTED_COMMAND);\
        case 0x0c02:\
                return (KONICA_ERROR_OTHER_COMMAND_EXECUTING);\
        case 0x0c03:\
                return (KONICA_ERROR_COMMAND_ORDER_ERROR);\
        case 0x0fff:\
                return (KONICA_ERROR_UNKNOWN_ERROR);\
        default:\
                GP_DEBUG (\
                        "The camera has just sent an error that has not "\
                        "yet been discovered. Please report the following "\
                        "to the maintainer of this driver with some "\
                        "additional information how you got this error.\n"\
                        " - Byte 1: %i\n"\
                        " - Byte 2: %i\n"\
                        "Thank you very much!\n",\
			(f)[2],\
                        (f)[3]);\
                return (GP_ERROR);\
        }\
}

int
k_init (GPPort *device)
{
        return (l_init (device));
}

int
k_erase_image (GPPort *device, int image_id_long, unsigned long image_id)
{
        /************************************************/
        /* Command to erase one image.                  */
        /*                                              */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of device ID                    */
        /*              0x02: Flash memory card         */
        /* 0xXX: Byte 1 of device ID                    */
        /*              0x00: Flash memory card         */
        /* 0xXX: Byte 3 of image ID (QM200 only)        */
        /* 0xXX: Byte 4 of image ID (QM200 only)        */
        /* 0xXX: Byte 0 of image ID                     */
        /* 0xXX: Byte 1 of image ID                     */
        /*                                              */
        /* Return values:                               */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x00, 0x80, 0x00, 0x00, 0x02,
                       0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        if (!image_id_long) {
                sb[6] = image_id;
                sb[7] = image_id >> 8;
                CHECK_RESULT (l_send_receive (device, sb, 8, &rb, &rbs,
					      0, NULL, NULL), rb);
        } else {
                sb[6] = image_id >> 16;
                sb[7] = image_id >> 24;
                sb[8] = image_id;
                sb[9] = image_id >> 8;
		CHECK_RESULT (l_send_receive (device, sb, 10, &rb, &rbs,
					      0, NULL, NULL), rb);
        }

        free (rb);
        return (GP_OK);
}


int
k_format_memory_card (GPPort *device)
{
        /************************************************/
        /* Command to format the memory card.           */
        /*                                              */
        /* 0x10: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of device ID                    */
        /*              0x02: Flash memory card         */
        /* 0xXX: Byte 1 of device ID                    */
        /*              0x00: Flash memory card         */
        /*                                              */
        /* Return values:                               */
        /* 0x10: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x10, 0x80, 0x00, 0x00, 0x02, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_RESULT (l_send_receive (device, sb, 6, &rb, &rbs,
				      0, NULL, NULL), rb);

        free (rb);
        return (GP_OK);
}


int
k_erase_all (GPPort *device, unsigned int *number_of_images_not_erased)
{
        /************************************************/
        /* Command to erase all images in the camera,   */
        /* except the protected ones.                   */
        /*                                              */
        /* 0x20: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of device ID                    */
        /*              0x02: Flash memory card         */
        /* 0xXX: Byte 1 of device ID                    */
        /*              0x00: Flash memory card         */
        /*                                              */
        /* Return values:                               */
        /* 0x20: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Byte 0 of number of images not erased  */
        /* 0xXX: Byte 1 of number of images not erased  */
        /************************************************/
        unsigned char sb[] = {0x20, 0x80, 0x00, 0x00, 0x02, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (number_of_images_not_erased);

	CHECK_RESULT (l_send_receive (device, sb, 6, &rb, &rbs,
				      0, NULL, NULL), rb);

	*number_of_images_not_erased = (rb[5] << 8) | rb[4];
        free (rb);
        return (GP_OK);
}


int
k_set_protect_status (GPPort *device, int image_id_long,
		      unsigned long image_id, int protected)
{
        /************************************************/
        /* Command to set the protect status of one     */
        /* image.                                       */
        /*                                              */
        /* 0x30: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of device ID                    */
        /*              0x02: Flash memory card         */
        /* 0xXX: Byte 1 of device ID                    */
        /*              0x00: Flash memory card         */
        /* 0xXX: Byte 3 of image ID (QM200 only)        */
        /* 0xXX: Byte 4 of image ID (QM200 only)        */
        /* 0xXX: Byte 0 of image ID                     */
        /* 0xXX: Byte 1 of image ID                     */
        /* 0xXX: Byte 0 of protect status               */
        /*              0x00: not protected             */
        /*              0x01: protected                 */
        /* 0x00: Byte 1 of protect status               */
        /*                                              */
        /* Return values:                               */
        /* 0x30: Byte 0 of command identifier           */
        /* 0x80: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x30, 0x80, 0x00, 0x00, 0x02, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        if (!image_id_long) {
                if (protected) sb[8] = 0x01;
                sb[6] = image_id;
                sb[7] = image_id >> 8;
		CHECK_RESULT (l_send_receive (device, sb, 10, &rb, &rbs,
					      0, NULL, NULL), rb);
        } else {
                if (protected) sb[10] = 0x01;
                sb[6] = image_id >> 16;
                sb[7] = image_id >> 24;
                sb[8] = image_id;
                sb[9] = image_id >> 8;
		CHECK_RESULT (l_send_receive (device, sb, 12, &rb, &rbs,
					      0, NULL, NULL), rb);
        }

        free (rb);
        return (GP_OK);
}


int
k_get_image (
        GPPort *device,
        int image_id_long,
        unsigned long image_id,
	KImageType image_type,
        unsigned char **image_buffer,
        unsigned int *image_buffer_size)
{
        /************************************************/
        /* Commands to get an image from the camera.    */
        /*                                              */
        /* 0xXX: Byte 0 of command identifier           */
        /*              0x00: get thumbnail             */
        /*              0x10: get image (jpeg)          */
        /*              0x30: get image (exif)          */
        /* 0x88: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of device ID                    */
        /*              0x02: Flash memory card         */
        /* 0xXX: Byte 1 of device ID                    */
        /*              0x00: Flash memory card         */
        /* 0xXX: Byte 3 of image ID (QM200 only)        */
        /* 0xXX: Byte 4 of image ID (QM200 only)        */
        /* 0xXX: Byte 0 of image ID                     */
        /* 0xXX: Byte 1 of image ID                     */
        /*                                              */
        /* The data will be received first, followed by */
        /* the return values.                           */
        /*                                              */
        /* Return values:                               */
        /* 0xXX: Byte 0 of command identifier           */
        /*              0x00: get thumbnail             */
        /*              0x10: get image (jpeg)          */
        /*              0x30: get image (exif)          */
        /* 0x88: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x00, 0x88, 0x00, 0x00, 0x02, 0x00, 0x00,
		       0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (image_buffer && image_buffer_size);

        switch (image_type) {
        case K_THUMBNAIL:
                sb[0] = 0x00;
                break;
        case K_IMAGE_JPEG:
                sb[0] = 0x10;
                break;
        case K_IMAGE_EXIF:
                sb[0] = 0x30;
                break;
        }
        if (!image_id_long) {
                sb[6] = image_id;
                sb[7] = image_id >> 8;
		CHECK_RESULT (l_send_receive (device, sb, 8, &rb, &rbs, 5000,
					image_buffer, image_buffer_size), rb);
        } else {
                sb[6] = image_id >> 16;
                sb[7] = image_id >> 24;
                sb[8] = image_id;
                sb[9] = image_id >> 8;
		CHECK_RESULT (l_send_receive (device, sb, 10, &rb, &rbs, 5000,
					image_buffer, image_buffer_size), rb);
        }

        free (rb);
        return (GP_OK);
}


int
k_get_image_information (
        GPPort *device,
        int image_id_long,
        unsigned long image_number,
        unsigned long *image_id,
        unsigned int *exif_size,
        int *protected,
        unsigned char **information_buffer,
        unsigned int *information_buffer_size)
{
        /************************************************/
        /* Command to get the information about an      */
        /* image from the camera.                       */
        /*                                              */
        /* 0x20: Byte 0 of command identifier           */
        /* 0x88: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of device ID                    */
        /*              0x02: Flash memory card         */
        /* 0xXX: Byte 1 of device ID                    */
        /*              0x00: Flash memory card         */
        /* 0xXX: Byte 3 of image number (QM200 only)    */
        /* 0xXX: Byte 4 of image number (QM200 only)    */
        /* 0xXX: Byte 0 of image number                 */
        /* 0xXX: Byte 1 of image number                 */
        /*                                              */
        /* The data will be received first, followed by */
        /* the return values.                           */
        /*                                              */
        /* Return values:                               */
        /* 0x20: Byte 0 of command identifier           */
        /* 0x88: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Byte 3 of image ID (QM200 only)        */
        /* 0xXX: Byte 4 of image ID (QM200 only)        */
        /* 0xXX: Byte 0 of image ID                     */
        /* 0xXX: Byte 1 of image ID                     */
        /* 0xXX: Byte 0 of exif size                    */
        /* 0xXX: Byte 1 of exif size                    */
        /* 0xXX: Byte 0 of protect status               */
        /*              0x00: not protected             */
        /*              0x01: protected                 */
        /* 0x00: Byte 1 of protect status               */
        /************************************************/
        unsigned char sb[] = {0x20, 0x88, 0x00, 0x00, 0x02, 0x00,
			0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (image_id && exif_size && protected && information_buffer &&
		    information_buffer_size);

        if (!image_id_long) {
                sb[6] = image_number;
                sb[7] = image_number >> 8;
		CHECK_RESULT (l_send_receive (device, sb, 8, &rb, &rbs, 1000,
			information_buffer, information_buffer_size), rb);
		*image_id = (unsigned long) ((rb[5] << 8) | rb[4]);
		*exif_size = (rb[7] << 8) | rb[6];
		*protected = (rb[8] != 0x00);
        } else {
                sb[6] = image_number >> 16;
                sb[7] = image_number >> 24;
                sb[8] = image_number;
                sb[9] = image_number >> 8;
		CHECK_RESULT (l_send_receive (device, sb, 10, &rb, &rbs, 1000,
			information_buffer, information_buffer_size), rb);
		*image_id = (rb[5] << 24) | (rb[4] << 16) |
			    (rb[7] << 8 ) |  rb[6];
		*exif_size = (rb[9] << 8) | rb[8];
		*protected = (rb[10] != 0x00);
        }

        free (rb);
        return (GP_OK);
}

int
k_get_preview (GPPort *device, int thumbnail,
	       unsigned char **image_buffer, unsigned int *image_buffer_size)
{
        /************************************************/
        /* Command to get the preview from the camera.  */
        /*                                              */
        /* 0x40: Byte 0 of command identifier           */
        /* 0x88: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of preview mode                 */
        /*              0x00: no thumbnail              */
        /*              0x01: thumbnail                 */
        /* 0x00: Byte 1 of preview mode                 */
        /*                                              */
        /* The data will be received first, followed by */
        /* the return values.                           */
        /*                                              */
        /* Return values:                               */
        /* 0x40: Byte 0 of command identifier           */
        /* 0x88: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x40, 0x88, 0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (image_buffer && image_buffer_size);

        if (thumbnail)
		sb[4] = 0x01;
        CHECK_RESULT (l_send_receive (device, sb, 6, &rb, &rbs, 5000,
				      image_buffer, image_buffer_size), rb);

        free (rb);
        return (GP_OK);
}

int
k_get_io_capability (GPPort *device,
		     KBitRate *bit_rates, KBitFlag *bit_flags)
{
        /************************************************/
        /* Command to get the IO capability from the    */
        /* camera.                                      */
        /*                                              */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /*                                              */
        /* Return values:                               */
        /* 0x40: Byte 0 of command identifier           */
        /* 0x88: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Byte 0 of supported bit rates          */
        /* 0xXX: Byte 1 of supported bit rates          */
        /* 0xXX: Byte 0 of supported flags              */
        /* 0xXX: Byte 1 of supported flags              */
        /************************************************/
        unsigned char sb[] = {0x00, 0x90, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (bit_rates && bit_flags);

	CHECK_RESULT (l_send_receive (device, sb, 4, &rb, &rbs,
				      0, NULL, NULL), rb);

	*bit_rates = (rb[5] << 8) | rb[4];
	*bit_flags = (rb[6] << 8) | rb[5];

        free (rb);
        return (GP_OK);
}

int
k_get_information (
        GPPort *device,
        char **model,
        char **serial_number,
        unsigned char *hardware_version_major,
        unsigned char *hardware_version_minor,
        unsigned char *software_version_major,
        unsigned char *software_version_minor,
        unsigned char *testing_software_version_major,
        unsigned char *testing_software_version_minor,
        char **name,
        char **manufacturer)
{
        /************************************************/
        /* Command to get some information about the    */
        /* camera.                                      */
        /*                                              */
        /* 0x10: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* (...)                                        */
        /*                                              */
        /* You can add pairs of additional bytes with   */
        /* no effect on the command.                    */
        /*                                              */
        /* Return values:                               */
        /* 0x10: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Reserved                               */
        /* 0xXX: Reserved                               */
        /* 0xXX: Reserved                               */
        /* 0xXX: Reserved                               */
        /* 0xXX: Byte 0 of model                        */
        /* 0xXX: Byte 1 of model                        */
        /* 0xXX: Byte 2 of model                        */
        /* 0xXX: Byte 3 of model                        */
        /* 0xXX: Byte 0 of serial number                */
        /* 0xXX: Byte 1 of serial number                */
        /* 0xXX: Byte 2 serial number                   */
        /* 0xXX: Byte 3 serial number                   */
        /* 0xXX: Byte 4 serial number                   */
        /* 0xXX: Byte 5 serial number                   */
        /* 0xXX: Byte 6 serial number                   */
        /* 0xXX: Byte 7 serial number                   */
        /* 0xXX: Byte 8 serial number                   */
        /* 0xXX: Byte 9 serial number                   */
        /* 0xXX: Hardware version (major)               */
        /* 0xXX: Hardware version (minor)               */
        /* 0xXX: Software version (major)               */
        /* 0xXX: Software version (minor)               */
        /* 0xXX: Testing software version (major)       */
        /* 0xXX: Testing software version (minor)       */
        /* 0xXX: Byte 0 of name                         */
        /* 0xXX: Byte 1 of name                         */
        /* 0xXX: Byte 2 of name                         */
        /* 0xXX: Byte 3 of name                         */
        /* 0xXX: Byte 4 of name                         */
        /* 0xXX: Byte 5 of name                         */
        /* 0xXX: Byte 6 of name                         */
        /* 0xXX: Byte 7 of name                         */
        /* 0xXX: Byte 8 of name                         */
        /* 0xXX: Byte 9 of name                         */
        /* 0xXX: Byte 10 of name                        */
        /* 0xXX: Byte 11 of name                        */
        /* 0xXX: Byte 12 of name                        */
        /* 0xXX: Byte 13 of name                        */
        /* 0xXX: Byte 14 of name                        */
        /* 0xXX: Byte 15 of name                        */
        /* 0xXX: Byte 16 of name                        */
        /* 0xXX: Byte 17 of name                        */
        /* 0xXX: Byte 18 of name                        */
        /* 0xXX: Byte 19 of name                        */
        /* 0xXX: Byte 20 of name                        */
        /* 0xXX: Byte 21 of name                        */
        /* 0xXX: Byte 0 of manufacturer                 */
        /* 0xXX: Byte 1 of manufacturer                 */
        /* 0xXX: Byte 2 of manufacturer                 */
        /* 0xXX: Byte 3 of manufacturer                 */
        /* 0xXX: Byte 4 of manufacturer                 */
        /* 0xXX: Byte 5 of manufacturer                 */
        /* 0xXX: Byte 6 of manufacturer                 */
        /* 0xXX: Byte 7 of manufacturer                 */
        /* 0xXX: Byte 8 of manufacturer                 */
        /* 0xXX: Byte 9 of manufacturer                 */
        /* 0xXX: Byte 10 of manufacturer                */
        /* 0xXX: Byte 11 of manufacturer                */
        /* 0xXX: Byte 12 of manufacturer                */
        /* 0xXX: Byte 13 of manufacturer                */
        /* 0xXX: Byte 14 of manufacturer                */
        /* 0xXX: Byte 15 of manufacturer                */
        /* 0xXX: Byte 16 of manufacturer                */
        /* 0xXX: Byte 17 of manufacturer                */
        /* 0xXX: Byte 18 of manufacturer                */
        /* 0xXX: Byte 19 of manufacturer                */
        /* 0xXX: Byte 20 of manufacturer                */
        /* 0xXX: Byte 21 of manufacturer                */
        /* 0xXX: Byte 22 of manufacturer                */
        /* 0xXX: Byte 23 of manufacturer                */
        /* 0xXX: Byte 24 of manufacturer                */
        /* 0xXX: Byte 25 of manufacturer                */
        /* 0xXX: Byte 26 of manufacturer                */
        /* 0xXX: Byte 27 of manufacturer                */
        /* 0xXX: Byte 28 of manufacturer                */
        /* 0xXX: Byte 29 of manufacturer                */
        /************************************************/
        unsigned char sb[] = {0x10, 0x90, 0x00, 0x00};
        unsigned int i, j;
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (model && serial_number && hardware_version_major &&
		    hardware_version_minor && software_version_major &&
		    software_version_minor && testing_software_version_major &&
		    testing_software_version_minor && name && manufacturer);

	CHECK_RESULT (l_send_receive (device, sb, 4, &rb, &rbs, 0, NULL, NULL),
		      rb);
	
	/* Model */
	for (i = 0; i < 4; i++) if (rb[8 + i] == 0) break;
	*model = malloc (sizeof (char) * (i + 1));
	for (j = 0; j < i; j++)
		(*model)[j] = rb[8 + j];
	(*model)[j] = 0;

	/* Serial Number */
	for (i = 0; i < 10; i++) if (rb[12 + i] == 0) break;
	*serial_number = malloc (sizeof (char) * (i + 1));
	for (j = 0; j < i; j++)
		(*serial_number)[j] = rb[12 + j];
	(*serial_number)[j] = 0;

	/* Versions */
	*hardware_version_major         = rb[22];
	*hardware_version_minor         = rb[23];
	*software_version_major         = rb[24];
	*software_version_minor         = rb[25];
	*testing_software_version_major = rb[26];
	*testing_software_version_minor = rb[27];

	/* Name */
	for (i = 0; i < 22; i++) if (rb[28 + i] == 0) break;
	*name = malloc (sizeof (char) * (i + 1));
	for (j = 0; j < i; j++)
		(*name)[j] = rb[28 + j];
	(*name)[j] = 0;

	/* Manufacturer */
	for (i = 0; i < 30; i++) if (rb[50 + i] == 0) break;
	*manufacturer = malloc (sizeof (char) * (i + 1));
	for (j = 0; j < i; j++)
		(*manufacturer)[j] = rb[50 + j];
	(*manufacturer)[j] = 0;

        free (rb);
        return (GP_OK);
}

int
k_get_status (GPPort *device, KStatus *status)
{
        /************************************************/
        /* Command to get the status of the camera.     */
        /*                                              */
        /* 0x20: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* (...)                                        */
        /*                                              */
        /* You can add pairs of additional bytes. If    */
        /* those are all 0x00, then nothing will        */
        /* change. If at least one deviates, all        */
        /* individual pieces of the status information  */
        /* will be returned as being zero.              */
        /*                                              */
        /* Return values:                               */
        /* 0x20: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Result of self test                    */
        /*              0x00: Self test passed          */
        /*              other: Self test failed         */
        /* 0xXX: Power level                            */
        /*              0x00: Low                       */
        /*              0x01: Normal                    */
        /*              0x02: High                      */
        /* 0xXX: Power source                           */
        /*              0x00: Battery                   */
        /*              0x01: AC                        */
        /* 0xXX: Card status                            */
        /*              0x07: Card                      */
        /*              0x12: No card                   */
        /* 0xXX: Display                                */
        /*              0x00: built in                  */
        /*              0x02: TV                        */
        /* 0xXX: Byte 0 of card size                    */
        /* 0xXX: Byte 1 of card size                    */
        /* 0xXX: Byte 0 of pictures in camera           */
        /* 0xXX: Byte 1 of left pictures in camera      */
        /* 0xXX: Year                                   */
        /* 0xXX: Month                                  */
        /* 0xXX: Day                                    */
        /* 0xXX: Hour                                   */
        /* 0xXX: Minute                                 */
        /* 0xXX: Second                                 */
        /* 0xXX: Byte 0 of bit rates                    */
        /* 0xXX: Byte 1 of bit rates                    */
        /* 0xXX: Byte 0 of bit flags                    */
        /* 0xXX: Byte 1 of bit flags                    */
        /* 0xXX: Flash                                  */
        /* 0xXX: Resolution                             */
        /* 0xXX: Focus                                  */
        /* 0xXX: Exposure                               */
        /* 0xXX: Byte 0 of total pictures               */
        /* 0xXX: Byte 1 of total pictures               */
        /* 0xXX: Byte 0 of total strobes                */
        /* 0xXX: Byte 1 of total strobes                */
        /************************************************/
        unsigned char sb[] = {0x20, 0x90, 0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (status);

        CHECK_RESULT (l_send_receive (device, sb, 6, &rb, &rbs, 0, NULL, NULL),
		      rb);
	
	status->self_test_result = (rb[5] << 8) | rb[4];
	switch (rb[6]) {
	case 0x00:
		status->power_level = K_POWER_LEVEL_LOW;
		break;
	case 0x01:
		status->power_level = K_POWER_LEVEL_NORMAL;
		break;
	case 0x02:
		status->power_level = K_POWER_LEVEL_HIGH;
		break;
	default:
		GP_DEBUG ("Unknown power level %i!", rb[6]);
		break;
	}
	switch (rb[7]) {
	case 0x00:
		status->power_source = K_POWER_SOURCE_BATTERY;
		break;
	case 0x01:
		status->power_source = K_POWER_SOURCE_AC;
		break;
	default:
		GP_DEBUG ("Unknown power source %i!", rb[7]);
		break;
	}
	switch (rb[8]) {
	case 0x07:
		status->card_status = K_CARD_STATUS_CARD;
		break;
	case 0x12:
		status->card_status = K_CARD_STATUS_NO_CARD;
		break;
	default:
		GP_DEBUG ("Unknown card status %i!", rb[8]);
		break;
	}
	switch (rb[9]) {
	case 0x00:
		status->display = K_DISPLAY_BUILT_IN;
		break;
	case 0x02:
		status->display = K_DISPLAY_TV;
		break;
	default:
		GP_DEBUG ("Unkown display %i!", rb[9]);
		break;
	}
	status->card_size     = (rb[11] << 8) | rb[10];
	status->pictures      = (rb[13] << 8) | rb[12];
	status->pictures_left = (rb[15] << 8) | rb[14];
	status->date.year   = rb[16];
	status->date.month  = rb[17];
	status->date.day    = rb[18];
	status->date.hour   = rb[19];
	status->date.minute = rb[20];
	status->date.second = rb[21];
	status->bit_rate    = (rb[23] << 8) | rb[22];
	status->bit_flags   = (rb[25] << 8) | rb[24];
	status->flash       = rb[26];
	status->resolution  = rb[27];
	status->focus       = rb[28];
	status->exposure    = rb[29];
	status->total_pictures = (rb[31] << 8) | rb[30];
	status->total_strobes  = (rb[33] << 8) | rb[32];

        free (rb);
        return (GP_OK);
}

int
k_get_date_and_time (GPPort *device, KDate *date)
{
        /************************************************/
        /* Command to get the date and time from the    */
        /* camera.                                      */
        /*                                              */
        /* 0x30: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /*                                              */
        /* Return values:                               */
        /* 0x30: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Year                                   */
        /* 0xXX: Month                                  */
        /* 0xXX: Day                                    */
        /* 0xXX: Hour                                   */
        /* 0xXX: Minute                                 */
        /* 0xXX: Second                                 */
        /************************************************/
        unsigned char sb[] = {0x30, 0x90, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        CHECK_RESULT (l_send_receive (device, sb, 4, &rb, &rbs, 0, NULL, NULL),
		      rb);
	date->year   = rb[4];
	date->month  = rb[5];
	date->day    = rb[6];
	date->hour   = rb[7];
	date->minute = rb[8];
	date->second = rb[9];

        free (rb);
        return (GP_OK);
};

int
k_get_preferences (GPPort *device, KPreferences *preferences)
{
        /************************************************/
        /* Command to get the preferences from the      */
        /* camera.                                      */
        /*                                              */
        /* 0x40: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /*                                              */
        /* Return values:                               */
        /* 0x40: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Byte 0 of shutoff time                 */
        /* 0xXX: Byte 1 of shutoff time                 */
        /* 0xXX: Byte 0 of self timer time              */
        /* 0xXX: Byte 1 of self timer time              */
        /* 0xXX: Byte 0 of beep                         */
        /* 0xXX: Byte 1 of beep                         */
        /* 0xXX: Byte 0 of slide show interval          */
        /* 0xXX: Byte 1 of slide show interval          */
        /************************************************/
        unsigned char sb[] = {0x40, 0x90, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        CHECK_RESULT (l_send_receive (device, sb, 4, &rb, &rbs, 0, NULL, NULL),
		      rb);
	preferences->shutoff_time           = rb[4];
	preferences->self_timer_time        = rb[5];
	preferences->beep                   = rb[6];
	preferences->slide_show_interval    = rb[7];

        free (rb);
        return (GP_OK);
}

int
k_set_io_capability (GPPort *device, KBitRate bit_rate, KBitFlag bit_flags)
{
        /************************************************/
        /* Command to set the IO capability of the      */
        /* camera.                                      */
        /*                                              */
        /* 0x80: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of bit rates                    */
        /* 0xXX: Byte 1 of bit rates                    */
        /* 0xXX: Byte 0 of bit flags                    */
        /* 0xXX: Byte 1 of bit flags                    */
        /*                                              */
        /* Return values:                               */
        /* 0x80: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	sb[4] = bit_rate;
	sb[5] = bit_rate >> 8;
        sb[6] = bit_flags;

	CHECK_RESULT (l_send_receive (device, sb, 8, &rb, &rbs, 0, NULL, NULL),
		      rb);

        free (rb);
        return (GP_OK);
}


int
k_set_date_and_time (GPPort *device, KDate date)
{
        /************************************************/
        /* Command to set date and time of the camera.  */
        /*                                              */
        /* 0xb0: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Year   (0x00 to 0x25, 0x60 to 0x63)    */
        /* 0xXX: Month  (0x01 to 0x0C)                  */
        /* 0xXX: Day    (0x01 to 0x1F)                  */
        /* 0xXX: Hour   (0x00 to 0x17)                  */
        /* 0xXX: Minute (0x00 to 0x3b)                  */
        /* 0xXX: Second (0x00 to 0x3b)                  */
        /*                                              */
        /* Return values:                               */
        /* 0xb0: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0xb0, 0x90, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        sb[4] = date.year;
        sb[5] = date.month;
        sb[6] = date.day;
        sb[7] = date.hour;
        sb[8] = date.minute;
        sb[9] = date.second;
	CHECK_RESULT (l_send_receive (device, sb, 10, &rb, &rbs, 0, NULL, NULL),
		      rb);
        free (rb);
        return (GP_OK);
}


int
k_set_preference (GPPort *device, KPreference preference, unsigned int value)
{
        /************************************************/
        /* Command to set a preference of the camera.   */
        /*                                              */
        /* 0xc0: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0xXX: Byte 0 of preference                   */
        /* 0xXX: Byte 1 of preference                   */
        /*                                              */
        /* Return values:                               */
        /* 0xc0: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0xc0, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        switch (preference) {
	case K_PREFERENCE_RESOLUTION:
		sb[4] = 0x00;
		sb[5] = 0xc0;
		break;
	case K_PREFERENCE_EXPOSURE:
		sb[4] = 0x02;
		sb[5] = 0xc0;
		break;
	case K_PREFERENCE_SELF_TIMER_TIME:
		sb[4] = 0x04;
		sb[5] = 0xc0;
		break;
	case K_PREFERENCE_SLIDE_SHOW_INTERVAL:
		sb[4] = 0x06;
		sb[5] = 0xc0;
		break;
	case K_PREFERENCE_FLASH:
		sb[4] = 0x00;
		sb[5] = 0xd0;
		break;
	case K_PREFERENCE_FOCUS_SELF_TIMER:
		sb[4] = 0x02;
		sb[5] = 0xd0;
		break;
	case K_PREFERENCE_AUTO_OFF_TIME:
		sb[4] = 0x04;
		sb[5] = 0xd0;
		break;
	case K_PREFERENCE_BEEP:
		sb[4] = 0x06;
		sb[5] = 0xd0;
	}
	sb[6] = value;
	sb[7] = value >> 8;
	CHECK_RESULT (l_send_receive (device, sb, 8, &rb, &rbs, 0, NULL, NULL),
		      rb);
        free (rb);
        return (GP_OK);
}

int
k_reset_preferences (GPPort *device)
{
        /************************************************/
        /* Command to reset the preferences of the      */
        /* camera.                                      */
        /*                                              */
        /* 0xc1: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /*                                              */
        /* Return values:                               */
        /* 0xc1: Byte 0 of command identifier           */
        /* 0x90: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0xc1, 0x90, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_RESULT (l_send_receive (device, sb, 4, &rb, &rbs, 0, NULL, NULL),
		      rb);
        free (rb);
        return (GP_OK);
}


int
k_take_picture (
        GPPort *device,
        int image_id_long,
        unsigned long *image_id,
        unsigned int *exif_size,
        unsigned char **information_buffer,
        unsigned int *information_buffer_size,
        int *protected)
{
        /************************************************/
        /* Command to take a picture.                   */
        /*                                              */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x91: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0x02: Byte 0 of device ID                    */
        /* 0x00: Byte 1 of device ID                    */
        /*                                              */
        /* The data (thumbnail) will be received first, */
        /* followed by the return values.               */
        /*                                              */
        /* Return values:                               */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x91: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Byte 3 of image ID (QM200 only)        */
        /* 0xXX: Byte 4 of image ID (QM200 only)        */
        /* 0xXX: Byte 0 of image ID                     */
        /* 0xXX: Byte 1 of image ID                     */
        /* 0xXX: Byte 0 of exif size                    */
        /* 0xXX: Byte 1 of exif size                    */
        /* 0xXX: Byte 0 of protect status               */
        /*              0x00: not protected             */
        /*              0x01: protected                 */
        /* 0x00: Byte 1 of protect status               */
        /************************************************/
        unsigned char sb[] = {0x00, 0x91, 0x00, 0x00, 0x02, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (image_id && exif_size && protected && information_buffer &&
		    information_buffer_size);

	CHECK_RESULT (l_send_receive (device, sb, 6, &rb, &rbs, 60000,
		      information_buffer, information_buffer_size), rb);

	if (!image_id_long) {
		*image_id = 0x00 | 0x00 | (rb[5] << 8) | rb[4];
		*exif_size = (rb[7] << 8) | rb[6];
		*protected = (rb[8] != 0x00);
	} else {
		*image_id = (rb[5] << 24) | (rb[4] << 16) |
			    (rb[7] << 8 ) | (rb[6]);
		*exif_size = (rb[9] << 8) | rb[8];
		*protected = (rb[10] != 0x00);
	}
        free (rb);
        return (GP_OK);
}

int
k_localization_tv_output_format_set (GPPort *device,
				     KTVOutputFormat tv_output_format)
{
        /************************************************/
        /* Command for various localization issues.     */
        /* Here: Setting of tv output format.           */
        /*                                              */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x92: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0x01: Byte 0 of localization type identifier */
        /*              0x00: transmission of           */
        /*                    localization data         */
        /*              0x01: setting of tv output      */
        /*                    format                    */
        /*              0x02: setting of date format    */
        /* 0x00: Byte 1 of localization type identifier */
        /* 0xXX: Byte 0 of tv output format             */
        /*              0x00: NTSC                      */
        /*              0x01: PAL                       */
        /*              0x02: None                      */
        /* 0x00: Byte 1 of tv output format             */
        /*                                              */
        /* Return values:                               */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x92: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x00, 0x92, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        switch (tv_output_format) {
        case K_TV_OUTPUT_FORMAT_NTSC:
                sb[6] = 0x00;
                break;
        case K_TV_OUTPUT_FORMAT_PAL:
                sb[6] = 0x01;
                break;
        case K_TV_OUTPUT_FORMAT_HIDE:
                sb[6] = 0x02;
                break;
        default:
                return (GP_ERROR_BAD_PARAMETERS);
        }
        CHECK_RESULT (l_send_receive (device, sb, 8, &rb, &rbs, 0, NULL, NULL),
		      rb);
        free (rb);
        return (GP_OK);
}

int
k_localization_date_format_set (GPPort *device, KDateFormat date_format)
{
        /************************************************/
        /* Command for various localization issues.     */
        /* Here: Setting of date format.                */
        /*                                              */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x92: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0x02: Byte 0 of localization type identifier */
        /*              0x00: transmission of           */
        /*                    localization data         */
        /*              0x01: setting of tv output      */
        /*                    format                    */
        /*              0x02: setting of date format    */
        /* 0x00: Byte 1 of localization type identifier */
        /* 0xXX: Byte 0 of date format                  */
        /*              0x00: month/day/year            */
        /*              0x01: day/month/year            */
        /*              0x02: year/month/day            */
        /* 0x00: Byte 1 of date format                  */
        /*                                              */
        /* Return values:                               */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x92: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        unsigned char sb[] = {0x00, 0x92, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

        switch (date_format) {
        case K_DATE_FORMAT_MONTH_DAY_YEAR:
                sb[6] = 0x00;
                break;
        case K_DATE_FORMAT_DAY_MONTH_YEAR:
                sb[6] = 0x01;
                break;
        case K_DATE_FORMAT_YEAR_MONTH_DAY:
                sb[6] = 0x02;
                break;
        default:
                return (GP_ERROR_BAD_PARAMETERS);
        }
        CHECK_RESULT (l_send_receive (device, sb, 8, &rb, &rbs, 0, NULL, NULL),
		      rb);
        free (rb);
        return (GP_OK);
}

#define PACKET_SIZE 1024

int
k_localization_data_put (GPPort *device,
			 unsigned char *data, unsigned long data_size)
{
        /************************************************/
        /* Command for various localization issues.     */
        /* Here: Transmission of localization data.     */
        /*                                              */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x92: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /* 0x00: Byte 0 of localization type identifier */
        /*              0x00: transmission of           */
        /*                    localization data         */
        /*              0x01: setting of tv output      */
        /*                    format                    */
        /*              0x02: setting of date format    */
        /* 0x00: Byte 1 of localization type identifier */
        /* 0x00: Byte 0 of ?                            */
        /* 0x00: Byte 1 of ?                            */
        /* 0xXX: Byte 0 of number of bytes of packet    */
        /* 0xXX: Byte 1 of number of bytes of packet    */
        /* 0xXX: Byte 3 of memory address where to      */
        /*              write the packet to             */
        /* 0xXX: Byte 4 of memory address               */
        /* 0xXX: Byte 0 of memory address               */
        /* 0xXX: Byte 1 of memory address               */
        /* 0xXX: Byte 0 of last packet identifier       */
        /*              0x00: packets to follow         */
        /*              0x01: no more packets to follow */
        /* 0x00: Byte 1 of last packet identifier       */
        /*                                              */
        /* Then follows the packet (size see above).    */
        /*                                              */
        /*                                              */
        /* Return values:                               */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x92: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        int result;
        unsigned char *rb = NULL;
        unsigned int rbs;
        unsigned long i, j;
        unsigned char sb[16 + PACKET_SIZE];

	gp_log (GP_LOG_DEBUG, "konica", "Uploading %i bytes localization "
		"data...", data_size);

	CHECK_NULL (data);
	if (data_size < 512)
		return (GP_ERROR_BAD_PARAMETERS);

        sb[0] = 0x00;
        sb[1] = 0x92;
        sb[2] = 0x00;
        sb[3] = 0x00;
        sb[4] = 0x00;
        sb[5] = 0x00;
        sb[6] = 0x00;
        sb[7] = 0x00;
        sb[8] = (unsigned char) (PACKET_SIZE >> 0);
        sb[9] = (unsigned char) (PACKET_SIZE >> 8);
        sb[10] = 0x00;
        sb[11] = 0x00;
        sb[12] = 0x00;
        sb[13] = 0x00;
        sb[14] = 0x00;
        sb[15] = 0x00;
        i = 0;
        while (TRUE) {

                /* Set up the packet. */
                sb[10] = i >> 16;
                sb[11] = i >> 24;
                sb[12] = i;
                sb[13] = i >> 8;
                for (j = 0; j < PACKET_SIZE; j++) {
                        if ((i + j) < data_size) sb[16 + j] = data[i + j];
                        else sb[16 + j] = 0xFF;
                }

		/*
                 * We could wait until the camera sends us
		 * K_ERROR_LOCALIZATION_DATA_EXCESS, but it does that
		 * not until the 70000th byte or so. However, as we can
		 * provoke the message with sb[14] = 0x01, we do so as
		 * soon as we exceed the 65535th byte to shorten the
		 * transmission time. We can't do that before or the
		 * camera reports K_ERROR_LOCALIZATION_DATA_CORRUPT.
		 */
		if (i + PACKET_SIZE > 65536)
			sb[14] = 0x01;
                result = l_send_receive (device, sb, PACKET_SIZE + 16,
					 &rb, &rbs, 0, NULL, NULL);
                if (result == GP_OK) {
			if ((rb[3] == 0x0b) && (rb[2] == 0x00)) {

				/*
				 * The camera does no longer want to receive
				 * localization data. We are done.
				 */
				return (GP_OK);
			} else if ((rb[3] == 0x00) && (rb[2] == 0x00)) {

				/*
				 * Everything is fine. Continue sending
				 * packets but make sure we don't loop
				 * forever.
				 */
				if (i > 131072) 
					return (GP_ERROR);
			}
		}
		CHECK_RESULT (result, rb);
                free (rb);
                i += PACKET_SIZE;
        }
}

int
k_cancel (GPPort *device, KCommand* command)
{
        /************************************************/
        /* Command to cancel a command.                 */
        /*                                              */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x9e: Byte 1 of command identifier           */
        /* 0x00: Reserved                               */
        /* 0x00: Reserved                               */
        /*                                              */
        /* Return values:                               */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x9e: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /* Following bytes only in case of success.     */
        /* 0xXX: Byte 0 of cancelled command            */
        /* 0xXX: Byte 1 of cancelled command            */
        /************************************************/
        unsigned char sb[] = {0x00, 0x9e, 0x00, 0x00};
        unsigned char *rb = NULL;
        unsigned int rbs;

	CHECK_NULL (command);

	CHECK_RESULT (l_send_receive (device, sb, 4, &rb, &rbs, 0, NULL, NULL),
		      rb);
	*command = (rb[5] << 8) | rb[4];
        free (rb);
        return (GP_OK);
}
