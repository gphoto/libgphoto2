/*****************************************/
/* See konica.h for licence and details. */
/*****************************************/


/*************************/
/* Included Header Files */
/*************************/
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>
#include "lowlevel.h"
#include "konica.h"


/**************/
/* Prototypes */
/**************/
k_return_status_t K_RETURN_STATUS (l_return_status_t l_return_status);


k_return_status_t return_status_translation (guchar byte1, guchar byte2);


/*************/
/* Functions */
/*************/
k_return_status_t K_RETURN_STATUS (l_return_status_t l_return_status)
{
	switch (l_return_status) {
	case L_IO_ERROR:
		return (K_L_IO_ERROR);
	case L_TRANSMISSION_ERROR:
		return (K_L_TRANSMISSION_ERROR);
	default:
		return (K_L_SUCCESS);
	}
}


k_return_status_t return_status_translation (guchar byte1, guchar byte2)
{
	gchar buffer[1024];

	switch ((byte2 << 8 ) | byte1) {
	case 0x0000:
		return (K_SUCCESS);
	case 0x0101:
		return (K_ERROR_FOCUSING_ERROR);
	case 0x0102:
		return (K_ERROR_IRIS_ERROR);
	case 0x0201:
		return (K_ERROR_STROBE_ERROR);
	case 0x0203:
		return (K_ERROR_EEPROM_CHECKSUM_ERROR);
	case 0x0205:
		return (K_ERROR_INTERNAL_ERROR1);
	case 0x0206:
		return (K_ERROR_INTERNAL_ERROR2);
	case 0x0301:
		return (K_ERROR_NO_CARD_PRESENT);
	case 0x0311:
		return (K_ERROR_CARD_NOT_SUPPORTED);
	case 0x0321:
		return (K_ERROR_CARD_REMOVED_DURING_ACCESS);
	case 0x0340:
		return (K_ERROR_IMAGE_NUMBER_NOT_VALID);
	case 0x0341:
		return (K_ERROR_CARD_CAN_NOT_BE_WRITTEN);
	case 0x0381:
		return (K_ERROR_CARD_IS_WRITE_PROTECTED);
	case 0x0382:
		return (K_ERROR_NO_SPACE_LEFT_ON_CARD);
	case 0x0390:
		return (K_ERROR_NO_PICTURE_ERASED_AS_IMAGE_PROTECTED);
	case 0x0401:
		return (K_ERROR_LIGHT_TOO_DARK);
	case 0x0402:
		return (K_ERROR_AUTOFOCUS_ERROR);
	case 0x0501:
		return (K_ERROR_SYSTEM_ERROR);
	case 0x0800:
		return (K_ERROR_ILLEGAL_PARAMETER);
	case 0x0801:
		return (K_ERROR_COMMAND_CANNOT_BE_CANCELLED);
	case 0x0b00:
		return (K_ERROR_LOCALIZATION_DATA_EXCESS);
	case 0x0bff:
		return (K_ERROR_LOCALIZATION_DATA_CORRUPT);
	case 0x0c01:
		return (K_ERROR_UNSUPPORTED_COMMAND);
	case 0x0c02:
		return (K_ERROR_OTHER_COMMAND_EXECUTING);
	case 0x0c03:
		return (K_ERROR_COMMAND_ORDER_ERROR);
	case 0x0fff:
		return (K_ERROR_UNKNOWN_ERROR);
	default:
		sprintf (
			buffer, 
			"The camera has just sent an error that has not "
			"yet been discovered. Please report the following "
			"to the maintainer of this driver with some "
			"additional information how you got this error.\n"
			" - Byte 1: %i\n"
			" - Byte 2: %i\n"
			"Thank you very much!\n",
			byte1,
			byte2);
		gp_frontend_message (NULL, buffer);
		return (K_PROGRAM_ERROR);
	}
}


k_return_status_t k_init (gpio_device *device)
{
	return (K_RETURN_STATUS (l_init (device)));
}


k_return_status_t k_exit (gpio_device *device) 
{
	return (K_RETURN_STATUS (l_exit (device)));
}


k_return_status_t k_erase_image (gpio_device *device, gboolean image_id_long, gulong image_id)
{
	/************************************************/
	/* Command to erase one image.		 	*/
	/*						*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of device ID			*/
	/*		0x02: Flash memory card		*/
	/* 0xXX: Byte 1 of device ID			*/
	/*		0x00: Flash memory card		*/
	/* 0xXX: Byte 3 of image ID (QM200 only)	*/
	/* 0xXX: Byte 4 of image ID (QM200 only)	*/
	/* 0xXX: Byte 0 of image ID			*/
	/* 0xXX: Byte 1 of image ID			*/
	/*						*/
	/* Return values:				*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
	guchar sb[] = {0x00, 0x80, 0x00, 0x00, 0x02, 
		       0x00, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	if (!image_id_long) {
		sb[6] = image_id;
		sb[7] = image_id >> 8;
		l_return_status = l_send_receive (device, sb, 8, &rb, &rbs);
	} else {
		sb[6] = image_id >> 16;
		sb[7] = image_id >> 24;
		sb[8] = image_id;
		sb[9] = image_id >> 8;
		l_return_status = l_send_receive (device, sb, 10, &rb, &rbs);
	}
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_format_memory_card (gpio_device *device)
{
	/************************************************/
	/* Command to format the memory card.		*/
	/*						*/
	/* 0x10: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of device ID			*/
	/*		0x02: Flash memory card		*/
	/* 0xXX: Byte 1 of device ID			*/
	/*		0x00: Flash memory card		*/
	/*						*/
	/* Return values:				*/
	/* 0x10: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
	guchar sb[] = {0x10, 0x80, 0x00, 0x00, 0x02, 0x00}; 
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

	l_return_status = l_send_receive (device, sb, 6, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_erase_all (gpio_device *device, guint *number_of_images_not_erased)
{
	/************************************************/
	/* Command to erase all images in the camera, 	*/
	/* except the protected ones.			*/
	/*						*/
	/* 0x20: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of device ID			*/
	/*		0x02: Flash memory card		*/
	/* 0xXX: Byte 1 of device ID			*/
	/*		0x00: Flash memory card		*/
	/*						*/
	/* Return values:				*/
	/* 0x20: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/* Following bytes only in case of success.	*/
	/* 0xXX: Byte 0 of number of images not erased	*/
	/* 0xXX: Byte 1 of number of images not erased	*/
	/************************************************/
	unsigned char sb[] = {0x20, 0x80, 0x00, 0x00, 0x02, 0x00}; 
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;
	
        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	g_return_val_if_fail (number_of_images_not_erased != NULL, K_PROGRAM_ERROR);

	l_return_status = l_send_receive (device, sb, 6, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb);
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) 
			*number_of_images_not_erased = (rb[5] << 8) | rb[4];
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_set_protect_status (gpio_device *device, gboolean image_id_long, gulong image_id, gboolean protected)
{
	/************************************************/
	/* Command to set the protect status of one 	*/
	/* image.					*/
	/*						*/
	/* 0x30: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of device ID			*/
	/*		0x02: Flash memory card		*/
	/* 0xXX: Byte 1 of device ID			*/
	/*		0x00: Flash memory card		*/
	/* 0xXX: Byte 3 of image ID (QM200 only)	*/
	/* 0xXX: Byte 4 of image ID (QM200 only)	*/
	/* 0xXX: Byte 0 of image ID			*/
	/* 0xXX: Byte 1 of image ID			*/
	/* 0xXX: Byte 0 of protect status		*/
	/*		0x00: not protected		*/
	/*		0x01: protected			*/
	/* 0x00: Byte 1 of protect status		*/
	/*						*/
	/* Return values:				*/
	/* 0x30: Byte 0 of command identifier		*/
	/* 0x80: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
	guchar sb[] = {0x30, 0x80, 0x00, 0x00, 0x02, 0x00, 
		       0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; 
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	if (!image_id_long) {
		if (protected) sb[8] = 0x01;
		sb[6] = image_id;
		sb[7] = image_id >> 8;
		l_return_status = l_send_receive (device, sb, 10, &rb, &rbs);
	} else {
		if (protected) sb[10] = 0x01;
		sb[6] = image_id >> 16;
		sb[7] = image_id >> 24;
		sb[8] = image_id;
		sb[9] = image_id >> 8;
		l_return_status = l_send_receive (device, sb, 12, &rb, &rbs);
	}
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_get_image (
	gpio_device *device, 
	gboolean image_id_long,
	gulong image_id, 
	k_image_type_t image_type, 
	guchar **image_buffer, 
	guint *image_buffer_size)
{
	/************************************************/
	/* Commands to get an image from the camera.	*/
	/*						*/
	/* 0xXX: Byte 0 of command identifier		*/
	/*		0x00: get thumbnail		*/
	/*		0x10: get image (jpeg)		*/
	/*		0x30: get image (exif)		*/
	/* 0x88: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of device ID			*/
	/*		0x02: Flash memory card		*/
	/* 0xXX: Byte 1 of device ID			*/
	/*		0x00: Flash memory card		*/
	/* 0xXX: Byte 3 of image ID (QM200 only)	*/
	/* 0xXX: Byte 4 of image ID (QM200 only)	*/
	/* 0xXX: Byte 0 of image ID			*/
	/* 0xXX: Byte 1 of image ID			*/
	/*						*/
	/* The data will be received first, followed by	*/
	/* the return values.				*/
	/*						*/
	/* Return values:				*/
	/* 0xXX: Byte 0 of command identifier		*/
	/*		0x00: get thumbnail		*/
	/*		0x10: get image (jpeg)		*/
	/*		0x30: get image (exif)		*/
	/* 0x88: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
	guchar sb[] = {0x00, 0x88, 0x00, 0x00, 0x02, 
		       0x00, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	g_return_val_if_fail (image_buffer != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (*image_buffer == NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (image_buffer_size != NULL, K_PROGRAM_ERROR);

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
		l_return_status = l_send_receive_receive (device, sb, 8, image_buffer, image_buffer_size, &rb, &rbs, 2000);
	} else {
		sb[6] = image_id >> 16;
		sb[7] = image_id >> 24;
		sb[8] = image_id;
		sb[9] = image_id >> 8;
		l_return_status = l_send_receive_receive (device, sb, 10, image_buffer, image_buffer_size, &rb, &rbs, 2000);
	}
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return (k_return_status);
	}
}


k_return_status_t k_get_image_information (
	gpio_device *device,
	gboolean image_id_long,
	gulong image_number,
	gulong *image_id, 
	guint *exif_size, 
	gboolean *protected, 
	guchar **information_buffer, 
	guint *information_buffer_size)
{
	/************************************************/
	/* Command to get the information about an 	*/
	/* image from the camera.			*/
	/*						*/
	/* 0x20: Byte 0 of command identifier		*/
	/* 0x88: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of device ID			*/
	/*		0x02: Flash memory card		*/
	/* 0xXX: Byte 1 of device ID			*/
	/*		0x00: Flash memory card		*/
	/* 0xXX: Byte 3 of image number (QM200 only)	*/
	/* 0xXX: Byte 4 of image number (QM200 only)	*/
	/* 0xXX: Byte 0 of image number			*/
	/* 0xXX: Byte 1 of image number			*/
	/*						*/
	/* The data will be received first, followed by	*/
	/* the return values.				*/
	/*						*/
	/* Return values:				*/
	/* 0x20: Byte 0 of command identifier		*/
	/* 0x88: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/* Following bytes only in case of success.     */
	/* 0xXX: Byte 3 of image ID (QM200 only)	*/
	/* 0xXX: Byte 4 of image ID (QM200 only)	*/
	/* 0xXX: Byte 0 of image ID			*/
	/* 0xXX: Byte 1 of image ID			*/
	/* 0xXX: Byte 0 of exif size			*/
	/* 0xXX: Byte 1 of exif size			*/
	/* 0xXX: Byte 0 of protect status		*/
	/*		0x00: not protected		*/
	/*		0x01: protected			*/
	/* 0x00: Byte 1 of protect status		*/
	/************************************************/
	guchar sb[] = {0x20, 0x88, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	g_return_val_if_fail (image_id != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (exif_size != NULL, K_PROGRAM_ERROR); 
	g_return_val_if_fail (protected != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (information_buffer != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (*information_buffer == NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (information_buffer_size != NULL, K_PROGRAM_ERROR);

        if (!image_id_long) {
		sb[6] = image_number;
		sb[7] = image_number >> 8;
		l_return_status = l_send_receive_receive (device, sb, 8, information_buffer, information_buffer_size, &rb, &rbs, 1000);
		if (l_return_status != L_SUCCESS) 
			return K_RETURN_STATUS (l_return_status);
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			*image_id = (gulong) ((rb[5] << 8) | rb[4]);
			*exif_size = (rb[7] << 8) | rb[6];
			*protected = (rb[8] != 0x00);
		}
	} else {
		sb[6] = image_number >> 16;
		sb[7] = image_number >> 24;
		sb[8] = image_number;
		sb[9] = image_number >> 8;
		l_return_status = l_send_receive_receive (device, sb, 10, information_buffer, information_buffer_size, &rb, &rbs, 1000);
		if (l_return_status != L_SUCCESS) 
			return K_RETURN_STATUS (l_return_status);
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			*image_id = (rb[5] << 24) | (rb[4] << 16) | (rb[7] << 8) | rb[6];
			*exif_size = (rb[9] << 8) | rb[8];
			*protected = (rb[10] != 0x00);
		}
	}
	g_free (rb);
	return (k_return_status);
}


k_return_status_t k_get_preview (gpio_device *device, gboolean thumbnail, guchar **image_buffer, guint *image_buffer_size)
{
	/************************************************/
	/* Command to get the preview from the camera.	*/
	/*						*/
	/* 0x40: Byte 0 of command identifier		*/
	/* 0x88: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of preview mode			*/
	/*		0x00: no thumbnail		*/
	/*		0x01: thumbnail			*/
	/* 0x00: Byte 1 of preview mode			*/
	/*						*/
	/* The data will be received first, followed by	*/
	/* the return values.				*/
	/*						*/
	/* Return values:				*/
	/* 0x40: Byte 0 of command identifier		*/
	/* 0x88: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
	guchar sb[] = {0x40, 0x88, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
        g_return_val_if_fail (image_buffer != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (*image_buffer == NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (image_buffer_size != NULL, K_PROGRAM_ERROR);
	if (thumbnail) sb[4] = 0x01;
	l_return_status = l_send_receive_receive (device, sb, 6, image_buffer, image_buffer_size, &rb, &rbs, 5000);
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_get_io_capability (
	gpio_device *device,
	gboolean *bit_rate_300,
	gboolean *bit_rate_600,
	gboolean *bit_rate_1200,
	gboolean *bit_rate_2400,
	gboolean *bit_rate_4800,
	gboolean *bit_rate_9600,
	gboolean *bit_rate_19200,
	gboolean *bit_rate_38400,
	gboolean *bit_rate_57600,
	gboolean *bit_rate_115200, 
	gboolean *bit_flag_8_bits,
	gboolean *bit_flag_stop_2_bits,
	gboolean *bit_flag_parity_on,
	gboolean *bit_flag_parity_odd,
	gboolean *bit_flag_hw_flow_control)
{
	/************************************************/
	/* Command to get the IO capability from the 	*/
	/* camera.					*/
	/*						*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/*						*/
	/* Return values:				*/
	/* 0x40: Byte 0 of command identifier		*/
	/* 0x88: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
        /* Following bytes only in case of success.     */
	/* 0xXX: Byte 0 of supported bit rates		*/
	/* 0xXX: Byte 1 of supported bit rates		*/
	/* 0xXX: Byte 0 of supported flags		*/
	/* 0xXX: Byte 1 of supported flags		*/
	/************************************************/
	guchar sb[] = {0x00, 0x90, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	g_return_val_if_fail (bit_rate_300 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_600 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_1200 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_2400 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_4800 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_9600 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_19200 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_38400 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_57600 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_rate_115200 != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_flag_8_bits != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_flag_stop_2_bits != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_flag_parity_on != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_flag_parity_odd != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (bit_flag_hw_flow_control != NULL, K_PROGRAM_ERROR);
	l_return_status = (l_send_receive (device, sb, 4, &rb, &rbs));
	if (l_return_status != L_SUCCESS) {
		g_free (rb);
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			*bit_rate_300    = ((rb[4] ^ (1 << 0)) && ~rb[4]);
			*bit_rate_600    = ((rb[4] ^ (1 << 1)) && ~rb[4]);
			*bit_rate_1200   = ((rb[4] ^ (1 << 2)) && ~rb[4]);
			*bit_rate_2400   = ((rb[4] ^ (1 << 3)) && ~rb[4]);
			*bit_rate_4800   = ((rb[4] ^ (1 << 4)) && ~rb[4]);
			*bit_rate_9600   = ((rb[4] ^ (1 << 5)) && ~rb[4]);
			*bit_rate_19200  = ((rb[4] ^ (1 << 6)) && ~rb[4]);
			*bit_rate_38400  = ((rb[4] ^ (1 << 7)) && ~rb[4]);
			*bit_rate_57600  = ((rb[5] ^ (1 << 0)) && ~rb[5]);
			*bit_rate_115200 = ((rb[5] ^ (1 << 1)) && ~rb[5]);
			*bit_flag_8_bits       = ((rb[6] ^ (1 << 0)) && ~rb[6]);
			*bit_flag_stop_2_bits  = ((rb[6] ^ (1 << 1)) && ~rb[6]);
			*bit_flag_parity_on    = ((rb[6] ^ (1 << 2)) && ~rb[6]);
			*bit_flag_parity_odd   = ((rb[6] ^ (1 << 3)) && ~rb[6]);
			*bit_flag_hw_flow_control= ((rb[6] ^ (1 << 4)) && ~rb[6]);
		}
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_get_information (
	gpio_device *device,
	gchar **model, 
	gchar **serial_number,
	guchar *hardware_version_major, 
	guchar *hardware_version_minor, 
	guchar *software_version_major, 
	guchar *software_version_minor,
	guchar *testing_software_version_major,
	guchar *testing_software_version_minor,
	gchar **name,
	gchar **manufacturer) 
{
	/************************************************/
	/* Command to get some information about the  	*/
	/* camera.					*/
	/*						*/
	/* 0x10: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* (...)					*/
	/* 						*/
	/* You can add pairs of additional bytes with	*/
	/* no effect on the command.			*/
	/*						*/
	/* Return values:				*/
	/* 0x10: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
        /* Following bytes only in case of success.     */
	/* 0xXX: Reserved				*/
	/* 0xXX: Reserved				*/
	/* 0xXX: Reserved				*/
	/* 0xXX: Reserved				*/
	/* 0xXX: Byte 0 of model			*/
	/* 0xXX: Byte 1 of model			*/
	/* 0xXX: Byte 2 of model			*/
	/* 0xXX: Byte 3 of model			*/
	/* 0xXX: Byte 0 of serial number		*/
	/* 0xXX: Byte 1 of serial number		*/
	/* 0xXX: Byte 2 serial number			*/
	/* 0xXX: Byte 3 serial number			*/
	/* 0xXX: Byte 4 serial number			*/
	/* 0xXX: Byte 5 serial number			*/
	/* 0xXX: Byte 6 serial number			*/
	/* 0xXX: Byte 7 serial number			*/
	/* 0xXX: Byte 8 serial number			*/
	/* 0xXX: Byte 9 serial number			*/
	/* 0xXX: Hardware version (major)		*/
	/* 0xXX: Hardware version (minor)		*/
	/* 0xXX: Software version (major)		*/
	/* 0xXX: Software version (minor)		*/
	/* 0xXX: Testing software version (major)	*/
	/* 0xXX: Testing software version (minor)	*/
	/* 0xXX: Byte 0 of name				*/
	/* 0xXX: Byte 1 of name				*/
	/* 0xXX: Byte 2 of name				*/
	/* 0xXX: Byte 3 of name				*/
	/* 0xXX: Byte 4 of name				*/
	/* 0xXX: Byte 5 of name				*/
	/* 0xXX: Byte 6 of name				*/
	/* 0xXX: Byte 7 of name				*/
	/* 0xXX: Byte 8 of name				*/
	/* 0xXX: Byte 9 of name				*/
	/* 0xXX: Byte 10 of name			*/
	/* 0xXX: Byte 11 of name			*/
	/* 0xXX: Byte 12 of name			*/
	/* 0xXX: Byte 13 of name			*/
	/* 0xXX: Byte 14 of name			*/
	/* 0xXX: Byte 15 of name			*/
	/* 0xXX: Byte 16 of name			*/
	/* 0xXX: Byte 17 of name			*/
	/* 0xXX: Byte 18 of name			*/
	/* 0xXX: Byte 19 of name			*/
	/* 0xXX: Byte 20 of name			*/
	/* 0xXX: Byte 21 of name			*/
	/* 0xXX: Byte 0 of manufacturer			*/
	/* 0xXX: Byte 1 of manufacturer			*/
	/* 0xXX: Byte 2 of manufacturer			*/
	/* 0xXX: Byte 3 of manufacturer			*/
	/* 0xXX: Byte 4 of manufacturer			*/
	/* 0xXX: Byte 5 of manufacturer			*/
	/* 0xXX: Byte 6 of manufacturer			*/
	/* 0xXX: Byte 7 of manufacturer			*/
	/* 0xXX: Byte 8 of manufacturer			*/
	/* 0xXX: Byte 9 of manufacturer			*/
	/* 0xXX: Byte 10 of manufacturer		*/
	/* 0xXX: Byte 11 of manufacturer		*/
	/* 0xXX: Byte 12 of manufacturer		*/
	/* 0xXX: Byte 13 of manufacturer		*/
	/* 0xXX: Byte 14 of manufacturer		*/
	/* 0xXX: Byte 15 of manufacturer		*/
	/* 0xXX: Byte 16 of manufacturer		*/
	/* 0xXX: Byte 17 of manufacturer		*/
	/* 0xXX: Byte 18 of manufacturer		*/
	/* 0xXX: Byte 19 of manufacturer		*/
	/* 0xXX: Byte 20 of manufacturer		*/
	/* 0xXX: Byte 21 of manufacturer		*/
	/* 0xXX: Byte 22 of manufacturer		*/
	/* 0xXX: Byte 23 of manufacturer		*/
	/* 0xXX: Byte 24 of manufacturer		*/
	/* 0xXX: Byte 25 of manufacturer		*/
	/* 0xXX: Byte 26 of manufacturer		*/
	/* 0xXX: Byte 27 of manufacturer		*/
	/* 0xXX: Byte 28 of manufacturer		*/
	/* 0xXX: Byte 29 of manufacturer		*/
	/************************************************/
	guchar sb[] = {0x10, 0x90, 0x00, 0x00};
	guint i, j;
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
 	g_return_val_if_fail (model != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (*model == NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (serial_number != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (*serial_number == NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (hardware_version_major != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (hardware_version_minor != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (software_version_major != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (software_version_minor != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (testing_software_version_major != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (testing_software_version_minor != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (name != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (*name == NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (manufacturer != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (*manufacturer == NULL, K_PROGRAM_ERROR);
	l_return_status = l_send_receive (device, sb, 4, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb);
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			/****************/
			/* Model	*/
			/****************/
			for (i = 0; i < 4; i++) if (rb[8 + i] == 0) break;
			*model = g_new0 (gchar, i + 1);
			for (j = 0; j < i; j++) (*model)[j] = rb[8 + j];
			/************************/
			/* Serial Number	*/
			/************************/
			for (i = 0; i < 10; i++) if (rb[12 + i] == 0) break;
			*serial_number = g_new0 (gchar, i + 1);
			for (j = 0; j < i; j++) 
				(*serial_number)[j] = rb[12 + j];
			/****************/
			/* Versions	*/
			/****************/
			*hardware_version_major		= rb[22];
			*hardware_version_minor		= rb[23];
			*software_version_major		= rb[24];
			*software_version_minor		= rb[25];
			*testing_software_version_major	= rb[26];
			*testing_software_version_minor = rb[27];
			/****************/
			/* Name		*/
			/****************/
			for (i = 0; i < 22; i++) if (rb[28 + i] == 0) break;
			*name = g_new0 (gchar, i + 1);
			for (j = 0; j < i; j++) (*name)[j] = rb[28 + j];
			/****************/
			/* Manufacturer	*/
			/****************/
			for (i = 0; i < 30; i++) if (rb[50 + i] == 0) break;
			*manufacturer = g_new0 (gchar, i + 1);
			for (j = 0; j < i; j++) (*manufacturer)[j] = rb[50 + j];
		}
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_get_status (
	gpio_device *device,
	guint *self_test_result, 
	k_power_level_t *power_level,
	k_power_source_t *power_source,
	k_card_status_t *card_status, 
	k_display_t *display, 
	guint *card_size,
	guint *pictures, 
	guint *pictures_left, 
	guchar *year, 
	guchar *month, 
	guchar *day,
	guchar *hour,
	guchar *minute,
	guchar *second,
	guint *io_setting_bit_rate,
	guint *io_setting_flags,
	guchar *flash,
	guchar *resolution,
	guchar *focus,
	guchar *exposure,
	guint *total_pictures,
	guint *total_strobes)
{
	/************************************************/
	/* Command to get the status of the camera.	*/
	/*						*/
	/* 0x20: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* (...)					*/
	/* 						*/
	/* You can add pairs of additional bytes. If 	*/
	/* those are all 0x00, then nothing will	*/
	/* change. If at least one deviates, all 	*/
	/* individual pieces of the status information	*/
	/* will be returned as being zero.		*/
	/*						*/
	/* Return values:				*/
	/* 0x20: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
        /* Following bytes only in case of success.     */
	/* 0xXX: Result of self test			*/
	/*		0x00: Self test passed		*/
	/*		other: Self test failed		*/
	/* 0xXX: Power level				*/
	/*		0x00: Low			*/
	/*		0x01: Normal			*/
	/*		0x02: High			*/
	/* 0xXX: Power source				*/
	/*		0x00: Battery			*/
	/*		0x01: AC			*/
	/* 0xXX: Card status				*/
	/*		0x07: Card			*/
	/*		0x12: No card			*/
	/* 0xXX: Display				*/
	/*		0x00: built in			*/
	/*		0x02: TV			*/
	/* 0xXX: Byte 0 of card size			*/
	/* 0xXX: Byte 1 of card size			*/
	/* 0xXX: Byte 0 of pictures in camera		*/
	/* 0xXX: Byte 1 of left pictures in camera	*/
	/* 0xXX: Year					*/
	/* 0xXX: Month					*/
	/* 0xXX: Day					*/
	/* 0xXX: Hour					*/
	/* 0xXX: Minute					*/
	/* 0xXX: Second					*/
	/* 0xXX: Byte 0 of bit rates			*/
	/* 0xXX: Byte 1 of bit rates			*/
	/* 0xXX: Byte 0 of bit flags			*/
	/* 0xXX: Byte 1 of bit flags			*/
	/* 0xXX: Flash					*/
	/* 0xXX: Resolution				*/
	/* 0xXX: Focus					*/
	/* 0xXX: Exposure				*/
	/* 0xXX: Byte 0 of total pictures		*/
	/* 0xXX: Byte 1 of total pictures		*/
	/* 0xXX: Byte 0 of total strobes		*/
	/* 0xXX: Byte 1 of total strobes		*/
	/************************************************/
	guchar sb[] = {0x20, 0x90, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	g_return_val_if_fail (self_test_result != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (power_level != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (power_source != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (card_status != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (display != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (card_size != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (pictures != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (pictures_left != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (year != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (month != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (day != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (hour != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (minute != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (second != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (io_setting_bit_rate != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (io_setting_flags != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (flash != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (resolution != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (focus != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (exposure != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (total_pictures != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (total_strobes != NULL, K_PROGRAM_ERROR);

	l_return_status = l_send_receive (device, sb, 6, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb);
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			*self_test_result	= (rb[5] << 8) | rb[4];
			switch (rb[6]) {
				case 0x00: 
					*power_level = K_POWER_LEVEL_LOW;
					break;
				case 0x01:
					*power_level = K_POWER_LEVEL_NORMAL;
					break;
				case 0x02:
					*power_level = K_POWER_LEVEL_HIGH;
					break;
				default:
					power_level = NULL;
					break;
			}
			switch (rb[7]) {
				case 0x00:
					*power_source = K_POWER_SOURCE_BATTERY;
					break;
				case 0x01:
					*power_source = K_POWER_SOURCE_AC;
					break;
				default:
					power_source = NULL;
					break;
			}
			switch (rb[8]) {
				case 0x07:
					*card_status = K_CARD_STATUS_CARD;
					break;
				case 0x12:
					*card_status = K_CARD_STATUS_NO_CARD;
					break;
				default:
					card_status = NULL;
					break;
			}
			switch (rb[9]) {
				case 0x00:
					*display = K_DISPLAY_BUILT_IN;
					break;
				case 0x02:
					*display = K_DISPLAY_TV;
					break;
				default:
					display = NULL;
					break;
			}
			*card_size		= (rb[11] << 8) | rb[10];
			*pictures		= (rb[13] << 8) | rb[12];
			*pictures_left		= (rb[15] << 8) | rb[14];
			*year			= rb[16];
			*month			= rb[17];
			*day			= rb[18];
			*hour			= rb[19];
			*minute			= rb[20];
			*second			= rb[21];
			*io_setting_bit_rate	= (rb[23] << 8) | rb[22];
			*io_setting_flags	= (rb[25] << 8) | rb[24];
			*flash			= rb[26];
			*resolution		= rb[27];
			*focus			= rb[28];
			*exposure		= rb[29];
			*total_pictures		= (rb[31] << 8) | rb[30];
			*total_strobes		= (rb[33] << 8) | rb[32];
		}
		g_free (rb);
		return (k_return_status);
	}
}


k_return_status_t k_get_date_and_time (
	gpio_device *device,
	guchar *year, 
	guchar *month, 
	guchar *day, 
	guchar *hour, 
	guchar *minute, 
	guchar *second) 
{
	/************************************************/
	/* Command to get the date and time from the	*/
	/* camera.					*/
	/*						*/
	/* 0x30: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/*						*/
	/* Return values:				*/
	/* 0x30: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
        /* Following bytes only in case of success.     */
	/* 0xXX: Year					*/
	/* 0xXX: Month					*/
	/* 0xXX: Day					*/
	/* 0xXX: Hour					*/
	/* 0xXX: Minute					*/
	/* 0xXX: Second					*/
	/************************************************/
	guchar sb[] = {0x30, 0x90, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
        g_return_val_if_fail (year != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (month != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (day != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (hour != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (minute != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (second != NULL, K_PROGRAM_ERROR);

	l_return_status = l_send_receive (device, sb, 4, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			*year	= rb[4];
			*month	= rb[5];
			*day	= rb[6];
			*hour	= rb[7];
			*minute	= rb[8];
			*second	= rb[9];
		}
		g_free (rb);
		return (k_return_status);
	}
};


k_return_status_t k_get_preferences (
	gpio_device *device,
	guint *shutoff_time, 
	guint *self_timer_time, 
	guint *beep, 
	guint *slide_show_interval) 
{
	/************************************************/
	/* Command to get the preferences from the	*/
	/* camera.					*/
	/*						*/
	/* 0x40: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/*						*/
	/* Return values:				*/
	/* 0x40: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
        /* Following bytes only in case of success.     */
	/* 0xXX: Byte 0 of shutoff time			*/
	/* 0xXX: Byte 1 of shutoff time			*/
	/* 0xXX: Byte 0 of self timer time		*/
	/* 0xXX: Byte 1 of self timer time		*/
	/* 0xXX: Byte 0 of beep				*/
	/* 0xXX: Byte 1 of beep				*/
	/* 0xXX: Byte 0 of slide show interval		*/
	/* 0xXX: Byte 1 of slide show interval		*/
	/************************************************/
	unsigned char sb[] = {0x40, 0x90, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
       	g_return_val_if_fail (shutoff_time != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (self_timer_time != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (beep != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (slide_show_interval != NULL, K_PROGRAM_ERROR);
	l_return_status = l_send_receive (device, sb, 4, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb);
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			*shutoff_time		= rb[4];
			*self_timer_time	= rb[5];
			*beep			= rb[6];
			*slide_show_interval	= rb[7];
		}
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_set_io_capability (
	gpio_device *device,
	guint bit_rate, 
	gboolean bit_flag_7_or_8_bits,
	gboolean bit_flag_stop_2_bits,
	gboolean bit_flag_parity_on,
	gboolean bit_flag_parity_odd,
	gboolean bit_flag_use_hw_flow_control)
{
	/************************************************/
	/* Command to set the IO capability of the	*/
	/* camera.					*/
	/*						*/
	/* 0x80: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of bit rates			*/
	/* 0xXX: Byte 1 of bit rates			*/
	/* 0xXX: Byte 0 of bit flags			*/
	/* 0xXX: Byte 1 of bit flags			*/
	/*						*/
	/* Return values:				*/
	/* 0x80: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
	guchar sb[] = {0x80, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;
	
        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
	switch (bit_rate) {
		case 300:
			sb[4] = (1 << 0);
			break;
		case 600:
			sb[4] = (1 << 1);
			break;
		case 1200:
			sb[4] = (1 << 2);
			break;
		case 2400:
			sb[4] = (1 << 3);
			break;
		case 4800:
			sb[4] = (1 << 4);
			break;
		case 9600:
			sb[4] = (1 << 5);
			break;
		case 19200:
			sb[4] = (1 << 6);
			break;
		case 38400:
			sb[4] = (1 << 7);
			break;
		case 57600:
			sb[5] = (1 << 0);
			break;
		case 115200:
			sb[5] = (1 << 1);
			break;
		default:
			return (K_UNSUPPORTED_BITRATE);
			break;
	}
	sb[6] = (bit_flag_7_or_8_bits | (bit_flag_stop_2_bits << 1) | (bit_flag_parity_on << 2) | (bit_flag_parity_odd << 3) | 
		(bit_flag_use_hw_flow_control << 4));
	l_return_status = l_send_receive (device, sb, 8, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_set_date_and_time (
	gpio_device *device,
	guchar year, 
	guchar month, 
	guchar day, 
	guchar hour, 
	guchar minute, 
	guchar second)
{
	/************************************************/
	/* Command to set date and time of the camera.	*/
	/*						*/
	/* 0xb0: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Year 	(0x00 to 0x25, 0x60 to 0x63)	*/
	/* 0xXX: Month 	(0x01 to 0x0C)			*/
	/* 0xXX: Day 	(0x01 to 0x1F)			*/
	/* 0xXX: Hour	(0x00 to 0x17)			*/
	/* 0xXX: Minute	(0x00 to 0x3b)			*/
	/* 0xXX: Second (0x00 to 0x3b)			*/
	/*						*/
	/* Return values:				*/
	/* 0xb0: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
        guchar sb[] = {0xb0, 0x90, 0x00, 0x00, 0x00, 
		       0x00, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        sb[4] = year;
        sb[5] = month;
        sb[6] = day;
        sb[7] = hour;
        sb[8] = minute;
        sb[9] = second;
        l_return_status = l_send_receive (device, sb, 10, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return k_return_status;
	}
}


k_return_status_t k_set_preference (
	gpio_device *device,
	k_preference_t preference, guint value)
{
	/************************************************/
	/* Command to set a preference of the camera.	*/
	/*						*/
	/* 0xc0: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0xXX: Byte 0 of preference			*/
	/* 0xXX: Byte 1 of preference			*/
	/*						*/
	/* Return values:				*/
	/* 0xc0: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
        guchar sb[] = {0xc0, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;
        
        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
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
        l_return_status = l_send_receive (device, sb, 8, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return (k_return_status);
	}
}


k_return_status_t k_reset_preferences (gpio_device *device)
{
	/************************************************/
	/* Command to reset the preferences of the	*/
	/* camera.					*/
	/*						*/
	/* 0xc1: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/*						*/
	/* Return values:				*/
	/* 0xc1: Byte 0 of command identifier		*/
	/* 0x90: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
        guchar sb[] = {0xc1, 0x90, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

	g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
        l_return_status = l_send_receive (device, sb, 4, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb); 
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		return (k_return_status);
	}
}


k_return_status_t k_take_picture (
	gpio_device *device,
	gboolean image_id_long,
	gulong *image_id, 
	guint *exif_size, 
	unsigned char **information_buffer, 
	unsigned int *information_buffer_size, 
	gboolean *protected)
{
	/************************************************/
	/* Command to take a picture.			*/
	/*						*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x91: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/* 0x02: Byte 0 of device ID			*/
	/* 0x00: Byte 1 of device ID			*/
	/*						*/
	/* The data (thumbnail) will be received first, */
	/* followed by the return values.		*/
	/*						*/
	/* Return values:				*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x91: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
        /* Following bytes only in case of success.     */
	/* 0xXX: Byte 3 of image ID (QM200 only)	*/
	/* 0xXX: Byte 4 of image ID (QM200 only)	*/
	/* 0xXX: Byte 0 of image ID			*/
	/* 0xXX: Byte 1 of image ID			*/
	/* 0xXX: Byte 0 of exif size			*/
	/* 0xXX: Byte 1 of exif size			*/
	/* 0xXX: Byte 0 of protect status		*/
	/*		0x00: not protected		*/
	/*		0x01: protected			*/
	/* 0x00: Byte 1 of protect status		*/
	/************************************************/
        guchar sb[] = {0x00, 0x91, 0x00, 0x00, 0x02, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

	g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (image_id != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (exif_size != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (protected != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (information_buffer != NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (*information_buffer == NULL, K_PROGRAM_ERROR);
        g_return_val_if_fail (information_buffer_size != NULL, K_PROGRAM_ERROR);
	l_return_status = l_send_receive_receive (device, sb, 6, information_buffer, information_buffer_size, &rb, &rbs, 60000);
	if (l_return_status != L_SUCCESS) {
		g_free (rb);
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) {
			if (!image_id_long) {
		                *image_id = 0x00 | 0x00 | (rb[5] << 8) | rb[4];
		                *exif_size = (rb[7] << 8) | rb[6];
				*protected = (rb[8] != 0x00); 
			} else {
				*image_id = (rb[5] << 24) | (rb[4] << 16) | (rb[7] << 8) | (rb[6]);
		                *exif_size = (rb[9] << 8) | rb[8];
				*protected = (rb[10] != 0x00); 
			}
		}
		g_free (rb);
		return (k_return_status);
        }
}


k_return_status_t k_localization_tv_output_format_set (gpio_device *device, k_tv_output_format_t tv_output_format)
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
        /*              0x01: setting of tv output 	*/
	/*                    format 			*/
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
        guchar sb[] = {0x00, 0x92, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
        l_return_status_t l_return_status;
        k_return_status_t k_return_status;
        guchar *rb = NULL;
        guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
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
		return (K_PROGRAM_ERROR);
        }
        l_return_status = l_send_receive (device, sb, 8, &rb, &rbs);
        if (l_return_status != L_SUCCESS) {
                g_free (rb);
                return K_RETURN_STATUS (l_return_status);
        }
        k_return_status = return_status_translation (rb[2], rb[3]);
        g_free (rb);
        return (k_return_status);
}


k_return_status_t k_localization_date_format_set (gpio_device *device, k_date_format_t date_format)
{
        /************************************************/
        /* Command for various localization issues.     */
	/* Here: Setting of date format.		*/
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
	/* 0xXX: Byte 0 of date format			*/
	/*		0x00: month/day/year		*/
	/*		0x01: day/month/year		*/
	/*		0x02: year/month/day		*/
        /* 0x00: Byte 1 of date format                  */
	/*						*/
        /* Return values:                               */
        /* 0x00: Byte 0 of command identifier           */
        /* 0x92: Byte 1 of command identifier           */
        /* 0xXX: Byte 0 of return status                */
        /* 0xXX: Byte 1 of return status                */
        /************************************************/
        guchar sb[] = {0x00, 0x92, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00};
        l_return_status_t l_return_status;
        k_return_status_t k_return_status;
        guchar *rb = NULL;
        guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
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
		return (K_PROGRAM_ERROR);
	}
        l_return_status = l_send_receive (device, sb, 8, &rb, &rbs);
        if (l_return_status != L_SUCCESS) {
                g_free (rb);
                return K_RETURN_STATUS (l_return_status);
        }
        k_return_status = return_status_translation (rb[2], rb[3]);
        g_free (rb);
        return (k_return_status);
}


k_return_status_t k_localization_data_put (gpio_device *device, guchar *data, gulong data_size)
{
	/************************************************/
        /* Command for various localization issues.     */
	/* Here: Transmission of localization data.	*/
	/*						*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x92: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
        /* 0x00: Byte 0 of localization type identifier */
        /*              0x00: transmission of           */
        /*                    localization data         */
        /*              0x01: setting of tv output      */
        /*                    format                    */
        /*              0x02: setting of date format    */
        /* 0x00: Byte 1 of localization type identifier */
        /* 0x00: Byte 0 of ?                            */
        /* 0x00: Byte 1 of ?                            */
	/* 0xXX: Byte 0 of number of bytes of packet	*/
	/* 0xXX: Byte 1 of number of bytes of packet	*/
	/* 0xXX: Byte 3 of memory address where to 	*/
	/*		write the packet to		*/
	/* 0xXX: Byte 4 of memory address		*/
	/* 0xXX: Byte 0 of memory address		*/
	/* 0xXX: Byte 1 of memory address		*/
	/* 0xXX: Byte 0 of last packet identifier	*/
	/*		0x00: packets to follow		*/
	/*		0x01: no more packets to follow	*/
	/* 0x00: Byte 1 of last packet identifier	*/
	/* 						*/
	/* Then follows the packet (size see above).	*/
	/* 						*/
	/*						*/
	/* Return values:				*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x92: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
	/************************************************/
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;
	gulong i, j;
	guint packet_size = 1024;
	guchar sb[16 + packet_size];

	g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR);
	g_return_val_if_fail (data_size >= 512, K_PROGRAM_ERROR);
	g_return_val_if_fail (data != NULL, K_PROGRAM_ERROR);
	sb[0] = 0x00;
	sb[1] = 0x92;
	sb[2] = 0x00;
	sb[3] = 0x00;
	sb[4] = 0x00;
	sb[5] = 0x00;
	sb[6] = 0x00;
	sb[7] = 0x00;
	sb[8] = packet_size;
	sb[9] = packet_size >> 8;
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
                for (j = 0; j < packet_size; j++) {
                        if ((i + j) < data_size) sb[16 + j] = data[i + j];
                        else sb[16 + j] = 0xFF;
                }
		/********************************************************/
		/* We could wait until the camera sends us 		*/
		/* K_ERROR_LOCALIZATION_DATA_EXCESS, but it does that 	*/
		/* not until the 70000th byte or so. However, as we can */
		/* provoke the message with sb[14] = 0x01, we do so as	*/
		/* soon as we exceed the 65535th byte to shorten the 	*/
		/* transmission time. We can't do that before or the 	*/
		/* camera reports K_ERROR_LOCALIZATION_DATA_CORRUPT.	*/
		/********************************************************/
		if (i + packet_size > 65536) sb[14] = 0x01;
		l_return_status = l_send_receive (device, sb, packet_size + 16, &rb, &rbs);
		if (l_return_status != L_SUCCESS) {
			g_free (rb);
			return (K_RETURN_STATUS (l_return_status));
		}
		k_return_status = return_status_translation (rb[2], rb[3]);
		g_free (rb);
		switch (k_return_status) {
		case K_SUCCESS:
			/* Everything is fine. Continue sending packets. However, make sure we don't loop forever. */
			if (i > 131072) return (K_PROGRAM_ERROR);
			break;
		case K_ERROR_LOCALIZATION_DATA_EXCESS:
			/* The camera does no longer want to receive localization data. We are done. */
			return (K_SUCCESS);
		default:
			/* Something went wrong. */
			return (k_return_status);
		}
		i += packet_size;
	}
}


k_return_status_t k_cancel (gpio_device *device, k_command_t *command)
{
	/************************************************/
	/* Command to cancel a command.			*/
	/*						*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x9e: Byte 1 of command identifier		*/
	/* 0x00: Reserved				*/
	/* 0x00: Reserved				*/
	/*						*/
	/* Return values:				*/
	/* 0x00: Byte 0 of command identifier		*/
	/* 0x9e: Byte 1 of command identifier		*/
	/* 0xXX: Byte 0 of return status		*/
	/* 0xXX: Byte 1 of return status		*/
        /* Following bytes only in case of success.     */
	/* 0xXX: Byte 0 of cancelled command		*/
	/* 0xXX: Byte 1 of cancelled command		*/
	/************************************************/
        guchar sb[] = {0x00, 0x9e, 0x00, 0x00};
	l_return_status_t l_return_status;
	k_return_status_t k_return_status;
	guchar *rb = NULL;
	guint rbs;

        g_return_val_if_fail (device != NULL, K_PROGRAM_ERROR); 
	g_return_val_if_fail (command != NULL, K_PROGRAM_ERROR);
        l_return_status = l_send_receive (device, sb, 4, &rb, &rbs);
	if (l_return_status != L_SUCCESS) {
		g_free (rb);
		return K_RETURN_STATUS (l_return_status);
	} else {
		k_return_status = return_status_translation (rb[2], rb[3]);
		if (k_return_status == K_SUCCESS) 
			*command = (rb[5] << 8) | rb[4];
		return (k_return_status);
	}
}
