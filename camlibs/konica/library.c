/*************************/
/* Included header files */
/*************************/
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <zlib.h>
#include <string.h>
#include <time.h>
#include <gphoto2.h>
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif
#include "library.h"
#include "konica.h"


/*************/
/* Variables */
/*************/
static gchar* konica_results[] = {
	/* KONICA_ERROR_FOCUSING_ERROR				*/	N_("Focusing error"),
	/* KONICA_ERROR_IRIS_ERROR				*/	N_("Iris error"),
	/* KONICA_ERROR_STROBE_ERROR				*/	N_("Strobe error"),
	/* KONICA_ERROR_EEPROM_CHECKSUM_ERROR			*/	N_("EEPROM checksum error"),
	/* KONICA_ERROR_INTERNAL_ERROR1				*/	N_("Internal error (1)"),
	/* KONICA_ERROR_INTERNAL_ERROR2				*/	N_("Internal error (2)"),
	/* KONICA_ERROR_NO_CARD_PRESENT				*/	N_("No card present"),
	/* KONICA_ERROR_CARD_NOT_SUPPORTED			*/	N_("Card not supported"),
	/* KONICA_ERROR_CARD_REMOVED_DURING_ACCESS		*/	N_("Card removed during access"),
	/* KONICA_ERROR_IMAGE_NUMBER_NOT_VALID			*/	N_("Image number not valid"),
	/* KONICA_ERROR_CARD_CAN_NOT_BE_WRITTEN			*/	N_("Card cannot be written"),
	/* KONICA_ERROR_CARD_IS_WRITE_PROTECTED			*/	N_("Card is write protected"),
	/* KONICA_ERROR_NO_SPACE_LEFT_ON_CARD			*/	N_("No space left on card"),
	/* KONICA_ERROR_NO_IMAGE_ERASED_AS_IMAGE_PROTECTED	*/	N_("No image erased as image protected"),
	/* KONICA_ERROR_LIGHT_TOO_DARK				*/	N_("Light too dark"),
	/* KONICA_ERROR_AUTOFOCUS_ERROR				*/	N_("Autofocus error"),
	/* KONICA_ERROR_SYSTEM_ERROR				*/	N_("System error"),
	/* KONICA_ERROR_ILLEGAL_PARAMETER			*/	N_("Illegal parameter"),
	/* KONICA_ERROR_COMMAND_CANNOT_BE_CANCELLED		*/	N_("Command cannot be cancelled"),
	/* KONICA_ERROR_LOCALIZATION_DATA_EXCESS		*/	N_("Localization data excess"),
	/* KONICA_ERROR_LOCALIZATION_DATA_CORRUPT		*/	N_("Localization data corrupt"),
	/* KONICA_ERROR_UNSUPPORTED_COMMAND			*/	N_("Unsupported command"),
	/* KONICA_ERROR_OTHER_COMMAND_EXECUTING			*/	N_("Other command executing"),
	/* KONICA_ERROR_COMMAND_ORDER_ERROR			*/	N_("Command order error"),
	/* KONICA_ERROR_UNKNOWN_ERROR				*/	N_("Unknown error")
};



gchar* models_qm100[] = {"Konica Q-EZ", "Konica Q-M100", "Konica Q-M100V", "HP PhotoSmart C20", "HP PhotoSmart C30", NULL};


gchar* models_qm200[] = {"Konica Q-M200", "HP PhotoSmart C200",	"HP PhotoSmart 215", "HP PhotoSmart 315", NULL};


gchar** models[2] = {models_qm100, models_qm200};


/**************/
/* Prototypes */
/**************/
gboolean localization_file_read (Camera* camera, gchar* file_name, guchar** data, gulong* data_size);

gint erase_all_unprotected_images (Camera* camera, CameraWidget* widget);


/*************/
/* Functions */
/*************/
gint
erase_all_unprotected_images (Camera* camera, CameraWidget* widget)
{
	konica_data_t*	konica_data;
	gint		not_erased;
	gint		result;
	gchar*		tmp;

	konica_data = (konica_data_t *) camera->camlib_data;

	result = k_erase_all (konica_data->device, &not_erased);
	if ((result == GP_OK) && (not_erased > 0)) {
		tmp = g_strdup_printf (_("%i images were protected and have not been erased."), not_erased);
		gp_frontend_status (camera, tmp);
		g_free (tmp);
	}
	return (result);
}


gint 
camera_id (CameraText* id)
{
	g_return_val_if_fail (id != NULL, GP_ERROR_BAD_PARAMETERS);
	strcpy (id->text, "konica");
	return (GP_OK);
}


gint 
camera_abilities (CameraAbilitiesList* list)
{
	gint 			i, j;
	CameraAbilities* 	a;

	g_return_val_if_fail (list != NULL, GP_ERROR_BAD_PARAMETERS);
	for (i = 0; i < 2; i++) {
		for (j = 0; ; j++) {
			if (models[i][j] == NULL) break;
			a = gp_abilities_new ();
			strcpy (a->model, models[i][j]);
			a->port		= GP_PORT_SERIAL;
			a->speed[0]	= 300;
			a->speed[1] 	= 600;
			a->speed[2]	= 1200;
			a->speed[3]	= 2400;
			a->speed[4]	= 4800;
			a->speed[5]	= 9600;
			a->speed[6]	= 19200;
			a->speed[7]	= 38400;
			a->speed[8]	= 57600;
			a->speed[9]	= 115200;
			a->speed[10]	= 0;
			a->capture	= GP_CAPTURE_IMAGE | GP_CAPTURE_PREVIEW;
			a->config	= 1;
			a->file_delete	= 1;
			a->file_preview	= 1;
			a->file_put	= 0;
			gp_abilities_list_append (list, a);
		}
        }
	return (GP_OK);
}


gint 
camera_init (Camera* camera, CameraInit* init)
{
	gint 		i, j;
	guint 		test_bit_rate[10] 	= { 9600, 115200, 57600, 38400, 19200,  4800,  2400,  1200,   600,    300};
	guint 		bit_rate[10] 		= {  300,    600,  1200,  2400,  4800,  9600, 19200, 38400, 57600, 115200};
	gboolean 	bit_rate_supported[10] 	= {FALSE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,  FALSE}; 
	gboolean 	bit_flag_8_bits;
	gboolean 	bit_flag_stop_2_bits;
	gboolean 	bit_flag_parity_on;
	gboolean 	bit_flag_parity_odd;
	gboolean 	bit_flag_use_hw_flow_control;
	gint			result;
	guint 			speed;
	konica_data_t*		konica_data;
	gpio_device_settings 	device_settings;
	gpio_device*		device;

	g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (init, 	GP_ERROR_BAD_PARAMETERS);

	/* First, set up all the function pointers. */
	camera->functions->id 			= camera_id;
	camera->functions->abilities 		= camera_abilities;
	camera->functions->init 		= camera_init;
	camera->functions->exit 		= camera_exit;
	camera->functions->folder_list  	= camera_folder_list;
	camera->functions->file_list		= camera_file_list;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	camera->functions->file_delete 		= camera_file_delete;
	camera->functions->file_config_get	= camera_file_config_get;
	camera->functions->file_config_set	= camera_file_config_set;
	camera->functions->folder_config_get	= camera_folder_config_get;
	camera->functions->folder_config_set	= camera_folder_config_set;
	camera->functions->config_get		= camera_config_get;
	camera->functions->config_set		= camera_config_set;
	camera->functions->config 	  	= camera_config;
	camera->functions->capture 		= camera_capture;
	camera->functions->summary		= camera_summary;
	camera->functions->manual 		= camera_manual;
	camera->functions->about 		= camera_about;
	camera->functions->result_as_string	= camera_result_as_string;

        /* Create device. */
        device = gpio_new (GPIO_DEVICE_SERIAL);

	/* Store some data we constantly need. */
	konica_data = g_new (konica_data_t, 1);
	camera->camlib_data = konica_data;
	konica_data->filesystem = gp_filesystem_new ();
	konica_data->device = device;
	konica_data->image_id_long = FALSE;
	for (i = 0; i < 2; i++) {
		for (j = 0; ; j++) {
			if (models[i][j] == NULL) break;
			if (strcmp (models[i][j], camera->model) == 0) {
				if (i == 1) konica_data->image_id_long = TRUE;
				break;
			}
		}
	}
	/* Speed: We are given 0: We'll try all possibilities, starting with  */
	/*          9600, then trying all other values. Once we found the     */
	/*          camera's current speed, we'll set it to the highest value */
	/*          supported by the camera.                                  */
        /*        We are not given 0: We will first try to communicate with   */
	/*          the camera with the given speed. If we do not succeed, we */
	/*          will try all possibilities. If the given speed does not   */
	/*          equal the current speed, we will change the current speed */
	/*          to the given one.                                         */

	strcpy (device_settings.serial.port, init->port.path);
	device_settings.serial.bits = 8;
	device_settings.serial.parity = 0;
	device_settings.serial.stopbits = 1;

	/* In case we got a speed not 0, let's first 	*/
	/* test if we do already have the given speed.	*/
	if (init->port.speed != 0) {
		gp_debug_printf (GP_DEBUG_LOW, "konica", "-> Quick test for given speed %i.\n", init->port.speed);
		device_settings.serial.speed = init->port.speed;
		gpio_set_settings (device, device_settings);
		if (k_init (device) == GP_OK) return (GP_OK);
	}

	/* We either do have a speed of 0 or are not	*/
	/* communicating in the given speed. We'll test	*/
	/* what speed the camera currently uses...	*/
	for (i = 0; i < 10; i++) {
		gp_debug_printf (GP_DEBUG_LOW, "konica", "-> Testing speed %i.\n", test_bit_rate[i]);
		device_settings.serial.speed = test_bit_rate[i];
		gpio_set_settings (device, device_settings); 
		if (k_init (device) == GP_OK) break; 
	}
	if ((i == 1) && (init->port.speed == 0)) {

		/* We have a speed of 0 and are already communicating at the  */
		/* highest bit rate possible. What else do we want?           */
		return (GP_OK);

	}
	if (i == 10) {
		gpio_free (device);
		return (GP_ERROR_IO);
	}

	/* ... and what speed it is able to handle.	*/
	gp_debug_printf (GP_DEBUG_LOW, "konica", "-> Getting IO capabilities.\n");
	result = k_get_io_capability (
		device,
		&bit_rate_supported[0], 
		&bit_rate_supported[1], 
		&bit_rate_supported[2],
		&bit_rate_supported[3],
		&bit_rate_supported[4],
		&bit_rate_supported[5],
		&bit_rate_supported[6],
		&bit_rate_supported[7],
		&bit_rate_supported[8],
		&bit_rate_supported[9],
		&bit_flag_8_bits,
		&bit_flag_stop_2_bits,
		&bit_flag_parity_on,
		&bit_flag_parity_odd,
		&bit_flag_use_hw_flow_control);
	if (result != GP_OK) {
		gpio_free (device);
		return (result);
	}
	if (init->port.speed == 0) {

		/* We are given 0. Set the highest speed. */
		for (i = 9; i >= 0; i--) if (bit_rate_supported[i]) break;
		if (i < 0) {
			gpio_free (device);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		gp_debug_printf (GP_DEBUG_LOW, "konica", "-> Setting speed to %i.\n", bit_rate[i]);
		if ((result = k_set_io_capability (device, bit_rate[i], TRUE, FALSE, FALSE, FALSE, FALSE)) != GP_OK) {
			gpio_free (device);
			return (result);
		}
		if ((result = k_exit (device)) != GP_OK) {
			gpio_free (device);
			return (result);
		}
                device_settings.serial.speed = bit_rate[i];
                gpio_set_settings (device, device_settings);
                if ((result = k_init (device)) != GP_OK) {
			gpio_free (device);
			return (result);
		}

		/* We were successful! */
		return (GP_OK);

	} else {
		/* Does the camera allow us to set the bit rate to given speed?	*/
		speed = (guint) init->port.speed;
		if (    ((speed ==    300) && (!bit_rate_supported[0])) ||
			((speed ==    600) && (!bit_rate_supported[1])) ||
			((speed ==   1200) && (!bit_rate_supported[2])) ||
			((speed ==   2400) && (!bit_rate_supported[3])) ||
			((speed ==   4800) && (!bit_rate_supported[4])) ||
			((speed ==   9600) && (!bit_rate_supported[5])) ||
			((speed ==  19200) && (!bit_rate_supported[6])) ||
			((speed ==  38400) && (!bit_rate_supported[7])) ||
			((speed ==  57600) && (!bit_rate_supported[8])) ||
			((speed == 115200) && (!bit_rate_supported[9])) ||
			((speed !=    300) && 
			 (speed !=    600) &&
			 (speed !=   1200) &&
			 (speed !=   2400) &&
			 (speed !=   4800) &&
			 (speed !=   9600) &&
			 (speed !=  19200) &&
			 (speed !=  38400) &&
			 (speed !=  57600) &&
			 (speed != 115200))) {
			gpio_free (device);
			return (GP_ERROR_NOT_SUPPORTED);
		}
		/* Now we can set the given speed. */
		if ((result = k_set_io_capability (device, init->port.speed, TRUE, FALSE, FALSE, FALSE, FALSE)) != GP_OK) {
			gpio_free (device);
			return (result);
		}
		if ((result = k_exit (device)) != GP_OK) {
			gpio_free (device);
			return (result);
		}
		if ((result = k_init (device)) != GP_OK) {
			gpio_free (device);
			return (result);
		}

		/* We were successful! */
		return (GP_OK);
	}
}


gint
camera_exit (Camera* camera)
{
	gint		result;
        konica_data_t* 	konica_data;

	g_return_val_if_fail (camera, 							GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (konica_data = (konica_data_t *) camera->camlib_data, 	GP_ERROR_BAD_PARAMETERS);
	
	if ((result = k_exit (konica_data->device)) != GP_OK) return (result);
	if ((result = gp_filesystem_free (konica_data->filesystem)) != GP_OK) return (result);
        if ((result = gpio_free (konica_data->device)) != GPIO_OK) return (GP_ERROR);
	return (GP_OK);
}


gint
camera_folder_list (Camera* camera, CameraList* list, gchar* folder)
{
	g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (list, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (folder, 	GP_ERROR_BAD_PARAMETERS);

	/* The camera does not support folders. */
	return (GP_OK);
}


gint 
camera_file_list (Camera* camera, CameraList* list, gchar* folder)
{
	guint 			self_test_result;
	k_power_level_t 	power_level;
	k_power_source_t 	power_source;
	k_card_status_t 	card_status;
	k_display_t 		display;
	guint 		card_size;
	guint 		pictures = 0;
	guint 		pictures_left;
	guchar 		year;
	guchar 		month;
	guchar 		day;
	guchar 		hour;
	guchar 		minute;
	guchar 		second;
	guint 		io_setting_bit_rate;
	guint 		io_setting_flags;
	guchar 		flash;
	guchar 		quality;
	guchar 		focus;
	guchar 		exposure;
	guint 		total_pictures;
	guint 		total_strobes;
	guint 		i;
	guchar* 	information_buffer = NULL;
	guint		information_buffer_size;
	guint		exif_size;
	gboolean	protected;
	gulong		image_id;
	konica_data_t*	konica_data;
	gchar*		filename;
	gint		result;

        gp_debug_printf (GP_DEBUG_LOW, "konica", "*** Entering camera_file_list ***");
	g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (list, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (folder, 	GP_ERROR_BAD_PARAMETERS);

	konica_data = (konica_data_t *) camera->camlib_data;
	if ((result = k_get_status (
		konica_data->device,
		&self_test_result, 
		&power_level,
		&power_source,
		&card_status,
		&display, 
		&card_size,
		&pictures, 
		&pictures_left, 
		&year, 
		&month, 
		&day,
		&hour,
		&minute,
		&second,
		&io_setting_bit_rate,
		&io_setting_flags,
		&flash,
		&quality,
		&focus,
		&exposure,
		&total_pictures,
		&total_strobes)) != GP_OK) return (result);

	/* We can't get the filename from the camera. 	*/
	/* But we decide to call the images		*/
	/* 'image%6i.jpeg', with the image id as 	*/
	/* parameter. Therefore, let's get the image	*/
	/* ids.						*/
	for (i = 0; i < pictures; i++) {
	        if (k_get_image_information (
	                konica_data->device,
	                konica_data->image_id_long,
	                i + 1,
	                &image_id,
	                &exif_size,
	                &protected,
	                &information_buffer,
	                &information_buffer_size) == GP_OK) filename = g_strdup_printf ("%06i.jpg", (int) image_id);
		else filename = g_strdup ("??????.jpg");
		g_free (information_buffer);
		information_buffer = NULL;
		if ((result = gp_list_append (list, filename, GP_LIST_FILE)) != GP_OK) return (result);
		gp_filesystem_append (konica_data->filesystem, "/", filename);
		g_free (filename);
	}

	return (GP_OK);
}


gint 
camera_file_get_generic (Camera* camera, CameraFile* file, gchar* folder, gchar* filename, k_image_type_t image_type)
{
	gulong 		image_id;
	gchar		image_id_string[] = {0, 0, 0, 0, 0, 0, 0};
	konica_data_t*	konica_data;
	gchar*		tmp;
	gint		result;

	g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (file, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (folder, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (filename, GP_ERROR_BAD_PARAMETERS);

	konica_data = (konica_data_t *) camera->camlib_data;

	/* Check if we can get the image id from the filename. */
	g_return_val_if_fail (filename[0] != '?', GP_ERROR);
	memcpy (image_id_string, filename, 6);
	image_id = atol (image_id_string);
	
	/* Get the image. */
	result = k_get_image (
                konica_data->device,
                konica_data->image_id_long,
                image_id,
                image_type,
                (guchar **) &file->data,
                (guint *) &file->size);

	if (result == GP_OK) {
		strcpy (file->type, "image/jpg");
		if (image_type == K_THUMBNAIL) {
			tmp = g_strdup_printf ("%06i-thumbnail.jpg", (int) image_id);
			strcpy (file->name, tmp);
			g_free (tmp);
		} else {
			strcpy (file->name, filename);
		}
	}
	return (result);
}


gint 
camera_file_get (Camera* camera, CameraFile* file, gchar* folder, gchar* filename)
{
	return (camera_file_get_generic (camera, file, folder, filename, K_IMAGE_EXIF));
}


gint 
camera_file_get_preview (Camera* camera, CameraFile* file, gchar* folder, gchar* filename)
{
	return (camera_file_get_generic (camera, file, folder, filename, K_THUMBNAIL));
}


gint 
camera_file_delete (Camera* camera, gchar* folder, gchar* filename)
{
	gulong 		image_id; 
	gchar		image_id_string[] = {0, 0, 0, 0, 0, 0, 0};
	konica_data_t*	konica_data;

        gp_debug_printf (GP_DEBUG_LOW, "konica", "*** camera_file_delete ***");
        g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
        g_return_val_if_fail (folder, 	GP_ERROR_BAD_PARAMETERS);
        g_return_val_if_fail (filename, GP_ERROR_BAD_PARAMETERS);

	konica_data = (konica_data_t *) camera->camlib_data;

	/* Check if we can get the image id from the filename. */
	g_return_val_if_fail (filename[0] != '?', GP_ERROR);
	memcpy (image_id_string, filename, 6);
	image_id = atol (image_id_string);
	
	return (k_erase_image (konica_data->device, konica_data->image_id_long, image_id));
}


gint 
camera_summary (Camera* camera, CameraText* summary)
{
	gchar*		model = NULL;
	gchar*		serial_number = NULL;
	guchar 		hardware_version_major;
	guchar 		hardware_version_minor;
	guchar 		software_version_major;
	guchar 		software_version_minor;
	guchar 		testing_software_version_major;
	guchar 		testing_software_version_minor;
	gchar*		name = NULL;
	gchar*		manufacturer = NULL;
        konica_data_t*	konica_data;
	gint		result;

        g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
        g_return_val_if_fail (summary, 	GP_ERROR_BAD_PARAMETERS);

        konica_data = (konica_data_t *) camera->camlib_data;
	result = k_get_information (
		konica_data->device,
		&model, 
		&serial_number, 
		&hardware_version_major, 
		&hardware_version_minor, 
		&software_version_major, 
		&software_version_minor, 
		&testing_software_version_major,
		&testing_software_version_minor,
		&name, 
		&manufacturer);

	if (result == GP_OK) 
		sprintf (summary->text, 
			"About the camera:\n"
			"  Model: %s\n"
			"  Serial Number: %s,\n"
			"  Hardware Version: %i.%i\n"
			"  Software Version: %i.%i\n"
			"  Testing Software Version: %i.%i\n"
			"  Name: %s,\n"
			"  Manufacturer: %s\n",
			model,
			serial_number,
			hardware_version_major, 
			hardware_version_minor,
			software_version_major, 
			software_version_minor,
			testing_software_version_major, 
			testing_software_version_minor,
			name,
			manufacturer);
			
	return (result);
}


gint 
camera_capture (Camera* camera, CameraFile* file, CameraCaptureInfo* info) 
{
	konica_data_t*	konica_data;
	gulong 		image_id;
	gint 		exif_size;
	guchar*		information_buffer = NULL;
	guint 		information_buffer_size;
	gboolean 	protected;
	gint		result;

        gp_debug_printf (GP_DEBUG_LOW, "konica", "*** Entering camera_capture ***");
	g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (file, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (info, 	GP_ERROR_BAD_PARAMETERS);

        konica_data = (konica_data_t *) camera->camlib_data;

	/* What are we supposed to do? */
	switch (info->type) {
	case GP_CAPTURE_IMAGE:

		/* Take the picture. */
		result = k_take_picture (
			konica_data->device, 
			konica_data->image_id_long, 
			&image_id, 
			&exif_size, 
			&information_buffer, 
			&information_buffer_size, 
			&protected);
		g_free (information_buffer);
		if (result != GP_OK) return (result);
	
		/* Get the image. */
	        result = k_get_image (
			konica_data->device, 
			konica_data->image_id_long, 
			image_id, 
			K_IMAGE_JPEG, 
			(guchar **) &file->data, 
			(guint *) &file->size);
		if (result == GP_OK) {
			strcpy (file->type, "image/jpg");
			strcpy (file->name, "image.jpg");
		} else return (result);

		/* Delete this image. */
		result = k_erase_image (konica_data->device, konica_data->image_id_long, image_id);
		
		return (result);

	case GP_CAPTURE_VIDEO:

		/* Our cameras can't do that. */
		return (GP_ERROR_NOT_SUPPORTED);

	case GP_CAPTURE_PREVIEW:

		/* Get the preview. */
		if ((result = k_get_preview (konica_data->device, TRUE, (guchar**) &file->data, (guint*) &file->size)) == GP_OK) {
			strcpy (file->type, "image/jpg");
			strcpy (file->name, "preview.jpg");
		}
		return (result);

	default:

		/* Should not be reached. */
		g_warning (_("Unknown capture type (%i)!\n"), info->type);
		return (GP_ERROR_BAD_PARAMETERS);
	}
}


gint 
camera_manual (Camera* camera, CameraText* manual)
{
	strcpy(manual->text, "No manual available.");

	return (GP_OK);
}


gint 
camera_about (Camera* camera, CameraText* about) 
{
	g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (about, 	GP_ERROR_BAD_PARAMETERS);

	strcpy (about->text, 
		"Konica library\n"
		"Lutz Müller <urc8@rz.uni-karlsruhe.de>\n"
		"Support for all Konica and several HP cameras.");

	return (GP_OK);
}

gint 
camera_file_config_get (Camera* camera, CameraWidget** window, gchar* folder, gchar* file)
{
	CameraWidget*	widget;
	konica_data_t*	konica_data;
	gulong		image_id;
	guint		exif_size;
	gboolean 	protected;
	guchar*		information_buffer = NULL;
	guint		information_buffer_size;
	gint		result;
	gint		value_int = 0;
	
	g_return_val_if_fail (camera,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (window,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (!*window,	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (folder,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (file,	GP_ERROR_BAD_PARAMETERS);
	
	/* Get some information about the picture. */
	konica_data = (konica_data_t*) camera->camlib_data;
	result = k_get_image_information (
		konica_data->device, 
		konica_data->image_id_long, 
		gp_filesystem_number (konica_data->filesystem, folder, file),
		&image_id, &exif_size, &protected, &information_buffer, &information_buffer_size);
	if (result != GP_OK) return (result);

	/* Construct the window. */
	*window = gp_widget_new (GP_WIDGET_WINDOW, file);
	widget = gp_widget_new (GP_WIDGET_TOGGLE, "Protect");
	if (protected) value_int = 1;
	gp_widget_value_set (widget, &value_int);
	gp_widget_append (*window, widget);

	return (GP_OK);
}

int
camera_file_config_set (Camera* camera, CameraWidget* window, gchar* folder, gchar* file)
{
	CameraWidget* 	widget;
	gint		result = GP_OK;
	gchar		image_id_string[] = {0, 0, 0, 0, 0, 0, 0};
	glong		image_id;
	gint		value_int;
	konica_data_t*	konica_data;

	g_return_val_if_fail (camera,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (window,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (folder,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (file, 	GP_ERROR_BAD_PARAMETERS);
	
	/* Some information we need. */
	konica_data = (konica_data_t*) camera->camlib_data;
	g_return_val_if_fail (file[0] != '?', GP_ERROR);
	memcpy (image_id_string, file, 6);
	image_id = atol (image_id_string);
	
	/* Protect status? */
	g_return_val_if_fail (widget = gp_widget_child_by_label (window, _("Protect")), GP_ERROR_BAD_PARAMETERS);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &value_int);
		result = k_set_protect_status (konica_data->device, konica_data->image_id_long, image_id, (value_int != 0));
	}
		
	return (result);
}

int
camera_folder_config_get (Camera* camera, CameraWidget** window, gchar* folder)
{
	CameraWidget*	widget;

	g_return_val_if_fail (camera,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (window, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (!*window,	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (folder,	GP_ERROR_BAD_PARAMETERS);

	/* Construct the window. */
	*window = gp_widget_new (GP_WIDGET_WINDOW, folder);
	widget = gp_widget_new (GP_WIDGET_BUTTON, _("Erase all unprotected images"));
	gp_widget_callback_set (widget, erase_all_unprotected_images);
	gp_widget_append (*window, widget);

	return (GP_OK);
}

int
camera_folder_config_set (Camera* camera, CameraWidget* window, gchar* folder)
{
	g_return_val_if_fail (camera,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (window,   GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (folder,   GP_ERROR_BAD_PARAMETERS);
	
	return (GP_OK);
}

int 
camera_config_get (Camera* camera, CameraWidget** window)
{
        CameraWidget*	widget;
	CameraWidget*	section;
        guint 		shutoff_time, self_timer_time, beep, slide_show_interval;
        guint 		self_test_result;
        k_power_level_t 	power_level;
        k_power_source_t 	power_source;
        k_card_status_t 	card_status;
        k_display_t 	display;
        guint 		card_size;
        guint 		pictures = 0;
        guint 		pictures_left;
        guchar 		year, month, day, hour, minute, second;
        guint 		io_setting_bit_rate, io_setting_flags;
        guchar 		flash;
        guchar 		resolution;
        guchar 		focus_self_timer;
        guchar 		exposure;
        guint 		total_pictures;
        guint 		total_strobes;
	konica_data_t*	konica_data;
	gint		year_4_digits;
	struct tm	tm_struct;
	time_t		t;
	gint		result;
	gfloat		value_float;

        gp_debug_printf (GP_DEBUG_LOW, "konica", "*** Entering camera_config_get ***");
	g_return_val_if_fail (camera, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (window, 	GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (!*window,	GP_ERROR_BAD_PARAMETERS);

	/* Get the current settings. */
	konica_data = (konica_data_t *) camera->camlib_data;
	if ((result = k_get_status (
		konica_data->device, 
                &self_test_result,
                &power_level,
                &power_source,
                &card_status,
                &display,
                &card_size,
                &pictures,
                &pictures_left,
                &year,
                &month,
                &day,
                &hour,
                &minute,
                &second,
                &io_setting_bit_rate,
                &io_setting_flags,
                &flash,
                &resolution,
                &focus_self_timer,
                &exposure,
                &total_pictures,
                &total_strobes)) != GP_OK) return (result);
	if ((result = k_get_preferences (
		konica_data->device,
                &shutoff_time,
                &self_timer_time,
                &beep,
                &slide_show_interval)) != GP_OK) return (result);

	/* Create the window. */
        *window = gp_widget_new (GP_WIDGET_WINDOW, "Konica Configuration");

        /************************/
        /* Persistent Settings  */
        /************************/
        section = gp_widget_new (GP_WIDGET_SECTION, "Persistent Settings");
        gp_widget_append (*window, section);

	/* Date */
	widget = gp_widget_new (GP_WIDGET_DATE, "Date & Time");
	gp_widget_append (section, widget);
	if (year > 80) year_4_digits = year + 1900;
	else year_4_digits = year + 2000;
	tm_struct.tm_year = year_4_digits - 1900;
	tm_struct.tm_mon = month;
	tm_struct.tm_mday = day;
	tm_struct.tm_hour = hour;
	tm_struct.tm_min = minute;
	tm_struct.tm_sec = second;
	t = mktime (&tm_struct);
	gp_widget_value_set (widget, &t);

        /* Beep */
        widget = gp_widget_new (GP_WIDGET_RADIO, "Beep");
        gp_widget_append (section, widget);
	gp_widget_choice_add (widget, "On");
	gp_widget_choice_add (widget, "Off");
        switch (beep) {
        case 0:
		gp_widget_value_set (widget, "Off");
		break;
        default:
		gp_widget_value_set (widget, "On");
                break;
        }

        /* Self Timer Time */
        widget = gp_widget_new (GP_WIDGET_RANGE, "Self Timer Time");
        gp_widget_append (section, widget);
        gp_widget_range_set (widget, 3, 40, 1);
	value_float = self_timer_time;
	gp_widget_value_set (widget, &value_float);

        /* Auto Off Time */
        widget = gp_widget_new (GP_WIDGET_RANGE, "Auto Off Time");
        gp_widget_append (section, widget);
        gp_widget_range_set (widget, 1, 255, 1);
	value_float = shutoff_time;
	gp_widget_value_set (widget, &value_float);

        /* Slide Show Interval */
        widget = gp_widget_new (GP_WIDGET_RANGE, "Slide Show Interval");
        gp_widget_append (section, widget);
        gp_widget_range_set (widget, 1, 30, 1);
	value_float = slide_show_interval;
	gp_widget_value_set (widget, &value_float);

        /* Resolution */
        widget = gp_widget_new (GP_WIDGET_RADIO, "Resolution");
        gp_widget_append (section, widget);
        gp_widget_choice_add (widget, "Low (576 x 436)");
        gp_widget_choice_add (widget, "Medium (1152 x 872)");
        gp_widget_choice_add (widget, "High (1152 x 872)");
        switch (resolution) {
        case 1:
		gp_widget_value_set (widget, "High (1152 x 872)");
                break;
        case 3:
		gp_widget_value_set (widget, "Low (576 x 436)"); 
                break;
        default:
		gp_widget_value_set (widget, "Medium (1152 x 872)");
                break;
        }

	/****************/
	/* Localization */
	/****************/
        section = gp_widget_new (GP_WIDGET_SECTION, "Localization");
        gp_widget_append (*window, section);

        /* Language */
        widget = gp_widget_new (GP_WIDGET_TEXT, "Localization File");
        gp_widget_append (section, widget);

	/* TV output format */
	widget = gp_widget_new (GP_WIDGET_RADIO, "TV Output Format");
	gp_widget_append (section, widget);
	gp_widget_choice_add (widget, "NTSC");
	gp_widget_choice_add (widget, "PAL");
	gp_widget_choice_add (widget, "Do not display TV menu");

	/* Date format */
	widget = gp_widget_new (GP_WIDGET_RADIO, "Date Format");
	gp_widget_append (section, widget);
	gp_widget_choice_add (widget, "Month/Day/Year");
	gp_widget_choice_add (widget, "Day/Month/Year");
	gp_widget_choice_add (widget, "Year/Month/Day");

        /********************************/
        /* Session-persistent Settings  */
        /********************************/
        section = gp_widget_new (GP_WIDGET_SECTION, "Session-persistent Settings");
        gp_widget_append (*window, section);

        /* Flash */
        widget = gp_widget_new (GP_WIDGET_RADIO, "Flash");
        gp_widget_append (section, widget);
        gp_widget_choice_add (widget, "Off");
        gp_widget_choice_add (widget, "On");
        gp_widget_choice_add (widget, "On, red-eye reduction");
        gp_widget_choice_add (widget, "Auto");
        gp_widget_choice_add (widget, "Auto, red-eye reduction");
        switch (flash) {
        case 0:
		gp_widget_value_set (widget, "Off");
                break;
        case 1:
		gp_widget_value_set (widget, "On");
                break;
        case 5:
		gp_widget_value_set (widget, "On, red-eye reduction");
                break;
        case 6:
		gp_widget_value_set (widget, "Auto, red-eye reduction");
                break;
        default:
		gp_widget_value_set (widget, "Auto");
                break;
        }

        /* Exposure */
        widget = gp_widget_new (GP_WIDGET_RANGE, "Exposure");
        gp_widget_append (section, widget);
        gp_widget_range_set (widget, 0, 255, 1);
	value_float = exposure;
	gp_widget_value_set (widget, &value_float);

        /* Focus */
        widget = gp_widget_new (GP_WIDGET_RADIO, "Focus");
        gp_widget_append (section, widget);
        gp_widget_choice_add (widget, "Fixed");
        gp_widget_choice_add (widget, "Auto");
        switch ((guint) (focus_self_timer / 2)) {
        case 1:
		gp_widget_value_set (widget, "Auto");
                break;
        default:
		gp_widget_value_set (widget, "Fixed");
                break;
        }

        /************************/
        /* Volatile Settings    */
        /************************/
        section = gp_widget_new (GP_WIDGET_SECTION, "Volatile Settings");
        gp_widget_append (*window, section);

        /* Self Timer */
        widget = gp_widget_new (GP_WIDGET_RADIO, "Self Timer");
        gp_widget_append (section, widget);
        gp_widget_choice_add (widget, "Self Timer (only next picture)");
        gp_widget_choice_add (widget, "Normal");
        switch (focus_self_timer % 2) {
        case 1:
		gp_widget_value_set (widget, "Self Timer (only next picture)");
                break;
        default:
		gp_widget_value_set (widget, "Normal");
                break;
        }

	/* That's it. */
	return (GP_OK);
}
 
int camera_config_set (Camera *camera, CameraWidget *window)
{
	CameraWidget*	section;
	CameraWidget*	widget_focus;
	CameraWidget*	widget_self_timer;
	CameraWidget*	widget;
	k_date_format_t date_format = K_DATE_FORMAT_YEAR_MONTH_DAY;
	k_tv_output_format_t tv_output_format = K_TV_OUTPUT_FORMAT_HIDE;
	guint		beep = 0;
        gint 		j = 0;
        guchar*		data;
        gulong 		data_size;
	guchar 		focus_self_timer = 0;
        konica_data_t*	konica_data;
	gint		i;
	gfloat		f;
	gchar*		c;
	struct tm*	tm_struct;
	gint		result;

        gp_debug_printf (GP_DEBUG_LOW, "konica", "*** Entering camera_config_set ***");
	g_return_val_if_fail (camera, GP_ERROR_BAD_PARAMETERS);
	g_return_val_if_fail (window, GP_ERROR_BAD_PARAMETERS);

        konica_data = (konica_data_t *) camera->camlib_data;

        /************************/
        /* Persistent Settings  */
        /************************/
	g_assert ((section = gp_widget_child_by_label (window, "Persistent Settings")) != NULL);

	/* Date & Time */
	g_assert ((widget = gp_widget_child_by_label (section, "Date & Time")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &i);
		tm_struct = localtime ((time_t*) &i);
		if ((result = k_set_date_and_time (
			konica_data->device, 
			tm_struct->tm_year - 100, 
			tm_struct->tm_mon, 
			tm_struct->tm_mday, 
			tm_struct->tm_hour, 
			tm_struct->tm_min, 
			tm_struct->tm_sec)) != GP_OK) return (result); 
	}

	/* Beep */
	g_assert ((widget = gp_widget_child_by_label (section, "Beep")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &c);
		if (strcmp (c, "Off") == 0) beep = 0;
		else if (strcmp (c, "On") == 0) beep = 1;
		else g_assert_not_reached ();
		if ((result = k_set_preference (konica_data->device, K_PREFERENCE_BEEP, beep)) != GP_OK) return (result);
	}

	/* Self Timer Time */
	g_assert ((widget = gp_widget_child_by_label (section, "Self Timer Time")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &f);
		if ((result = k_set_preference (konica_data->device, K_PREFERENCE_SELF_TIMER_TIME, (gint) f)) != GP_OK) return (result);
	}

	/* Auto Off Time */
	g_assert ((widget = gp_widget_child_by_label (section, "Auto Off Time")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &f);
		if ((result = k_set_preference (konica_data->device, K_PREFERENCE_AUTO_OFF_TIME, (gint) f)) != GP_OK) return (result);
	}

	/* Slide Show Interval */
	g_assert ((widget = gp_widget_child_by_label (section, "Slide Show Interval")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &f);
		if ((result = k_set_preference (konica_data->device, K_PREFERENCE_SLIDE_SHOW_INTERVAL, (gint) f)) != GP_OK) return (result);
	}

	/* Resolution */
	g_assert ((widget = gp_widget_child_by_label (section, "Resolution")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &c);
		if (strcmp (c, "High (1152 x 872)") == 0) j = 1;
                else if (strcmp (c, "Low (576 x 436)") == 0) j = 3;
                else j = 0;
        	if ((result = k_set_preference (konica_data->device, K_PREFERENCE_RESOLUTION, j)) != GP_OK) return (result);
	}

        /****************/
        /* Localization */
        /****************/
	g_assert ((section = gp_widget_child_by_label (window, "Localization")) != NULL);

	/* Localization File */
	g_assert ((widget = gp_widget_child_by_label (section, "Localization File")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &c);
	        if (strcmp (c, "") != 0) {
                	data = NULL;
			data_size = 0;

			/* Read localization file */
			if (!localization_file_read (camera, c, &data, &data_size)) {
				g_free (data);
				return (GP_ERROR);
			}
	
			/* Go! */
			result = k_localization_data_put (konica_data->device, data, data_size);
			g_free (data);
			if (result != GP_OK) return (result);
		}
	}

	/* TV Output Format */
	g_assert ((widget = gp_widget_child_by_label (section, "TV Output Format")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &c);
		if (strcmp (c, "NTSC") == 0) tv_output_format = K_TV_OUTPUT_FORMAT_NTSC;
		else if (strcmp (c, "PAL") == 0) tv_output_format = K_TV_OUTPUT_FORMAT_PAL;
		else if (strcmp (c, "Do not display TV menu") == 0) tv_output_format = K_TV_OUTPUT_FORMAT_HIDE;
		else g_assert_not_reached ();
		if ((result = k_localization_tv_output_format_set (konica_data->device, tv_output_format)) != GP_OK) return (result);
	}

	/* Date Format */
	g_assert ((widget = gp_widget_child_by_label (section, "Date Format")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &c);
		if (strcmp (c, "Month/Day/Year") == 0) date_format = K_DATE_FORMAT_MONTH_DAY_YEAR;
                else if (strcmp (c, "Day/Month/Year") == 0) date_format = K_DATE_FORMAT_DAY_MONTH_YEAR;
		else if (strcmp (c, "Year/Month/Day") == 0) date_format = K_DATE_FORMAT_YEAR_MONTH_DAY;
		else g_assert_not_reached ();
		if ((result = k_localization_date_format_set (konica_data->device, date_format)) != GP_OK) return (result);
	}

        /********************************/
        /* Session-persistent Settings  */
        /********************************/
	g_assert ((section = gp_widget_child_by_label (window, "Session-persistent Settings")) != NULL);

	/* Flash */
	g_assert ((widget = gp_widget_child_by_label (section, "Flash")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &c);
		if (strcmp (c, "Off") == 0) j = 0;
		else if (strcmp (c, "On") == 0) j = 1;
                else if (strcmp (c, "On, red-eye reduction") == 0) j = 5;
		else if (strcmp (c, "Auto") == 0) j = 2;
		else if (strcmp (c, "Auto, red-eye reduction") == 0) j = 6;
		else g_assert_not_reached ();
		if ((result = k_set_preference (konica_data->device, K_PREFERENCE_FLASH, j)) != GP_OK) return (result);
	}

	/* Exposure */
	g_assert ((widget = gp_widget_child_by_label (section, "Exposure")) != NULL);
	if (gp_widget_changed (widget)) {
		gp_widget_value_get (widget, &f);
		if ((result = k_set_preference (konica_data->device, K_PREFERENCE_EXPOSURE, (gint) f)) != GP_OK) return (result);
	}

	/* Focus will be set together with self timer. */
	g_assert ((widget_focus = gp_widget_child_by_label (section, "Focus")) != NULL);

        /************************/
        /* Volatile Settings    */
        /************************/
	g_assert ((section = gp_widget_child_by_label (window, "Volatile Settings")) != NULL);

	/* Self Timer (and Focus) */
	g_assert ((widget_self_timer = gp_widget_child_by_label (section, "Self Timer")) != NULL);
	if (gp_widget_changed (widget_focus) && gp_widget_changed (widget_self_timer)) {
		gp_widget_value_get (widget_focus, &c);
		if (strcmp (c, "Auto") == 0) focus_self_timer = 2;
		else if (strcmp (c, "Fixed") == 0) focus_self_timer = 0;
		else g_assert_not_reached ();
		gp_widget_value_get (widget_self_timer, &c);
		if (strcmp (c, "Self Timer (only next picture)") == 0) focus_self_timer++;
		else if (strcmp (c, "Normal") == 0);
		else g_assert_not_reached ();
		if ((result = k_set_preference (konica_data->device, K_PREFERENCE_FOCUS_SELF_TIMER, focus_self_timer)) != GP_OK) return (result);
	}

	/* We are done. */
	return (GP_OK);
}


gint 
camera_config (Camera *camera)
{
	gint		result;
	CameraWidget*	window = NULL;

	if ((result = camera_config_get (camera, &window)) == GP_OK) {

	        /* Prompt the user with the config window. */
	        if (gp_frontend_prompt (camera, window) == GP_PROMPT_CANCEL) {
			gp_widget_unref (window);
	                return (GP_OK);
	        }
		result = camera_config_set (camera, window);
		gp_widget_unref (window);
		return (result);
	} else {
		if (window) gp_widget_unref (window);
		return (result);
	}
}


gchar*
camera_result_as_string (Camera* camera, gint result)
{
	/* Really an error? */
	g_return_val_if_fail (result < 0, _("Unknown error"));

	/* libgphoto2 error? */
	if (-result < 100) return (gp_result_as_string (result));

	/* Our error? */
	if ((0 - result - 100) < (guint) (sizeof (konica_results) / sizeof (*konica_results))) return _(konica_results [0 - result - 100]);
	
	return _("Unknown error");
}


gboolean localization_file_read (Camera *camera, gchar *file_name, guchar **data, gulong *data_size)
{
	FILE *file;
	gulong j;
	gchar f;
	guchar c[] = "\0\0";
	gulong line_number;
	konica_data_t *konica_data;
	guchar checksum;
	gulong fcs;
	guint d;
	gchar *message;

	g_return_val_if_fail (camera != NULL, FALSE);
	g_return_val_if_fail (file_name != NULL, FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (*data == NULL, FALSE);
	g_return_val_if_fail (data_size != NULL, FALSE);

	konica_data = (konica_data_t *) camera->camlib_data;
	if ((file = fopen (file_name, "r")) == NULL) {
		gp_frontend_message (camera, "Could not open requested localization file!");
		return (FALSE);
	}
	*data_size = 0;
	*data = g_new (guchar, 65536);
	j = 0; 
	line_number = 1;
        do {
                f = fgetc (file);
                switch (f) {
                case '\n': 
			line_number++;
			continue;
		case EOF:
			break;
                case '#':
			/************************/
                        /* Comment: Discard.    */
			/************************/
                        do {
                                f = fgetc (file);
                        } while ((f != '\n') && (f != EOF)); 
			if (f == '\n') line_number++;
                        continue;
                case '\t': 
			continue;
                case ' ': 
			continue;
                default: 
                        /****************************************/
                        /* j == 0: We have to read the second   */
                        /*         half of the byte to send.    */
                        /* j == 1: We'll compose our byte.      */
                        /****************************************/
			if ((f != '0') && (f != '1') && (f != '2') &&
			    (f != '3') && (f != '4') && (f != '5') &&
			    (f != '6') && (f != '7') && (f != '8') &&
			    (f != '9') && (f != 'A') && (f != 'B') &&
			    (f != 'C') && (f != 'D') && (f != 'E') &&
			    (f != 'F')) {
				message = g_strdup_printf (
					"Error in localization file!\n"
					"\"%c\" in line %i is not allowed.", 
					f,
					(gint) line_number);
				gp_frontend_message (camera, message);
				g_free (message);
				fclose (file);
				return (FALSE);
			}
			c[j] = f;
			if (j == 1) {
				if (sscanf (&c[0], "%X", &d) != 1) {
					gp_frontend_message (
						camera,
						"There seems to be an error in"
						" the localization file.");
						fclose (file);
						return (FALSE);
				}
				(*data)[*data_size] = d;
				(*data_size)++;
				if (*data_size == 65536) {
					gp_frontend_message (
						camera, 
						"Localization file too long!");
					fclose (file);
					return (FALSE);
				}
			}
			j = 1 - j;
			continue;
                } 
	} while (f != EOF);
	fclose (file);
	/*********************************/
	/* Calculate and check checksum. */
	/*********************************/
	checksum = 0;
	g_warning ("Checksum not implemented!");
	//FIXME: There's a checksum at (*data)[100]. I could not figure out how it is calculated.
	/*********************************************/
	/* Calculate and check frame check sequence. */
	/*********************************************/
	fcs = 0;
	g_warning ("Frame check sequence not implemented!");
	//FIXME: There's a frame check sequence at (*data)[108] and (*data)[109]. I could not figure out how it is calculated.
	gp_debug_printf (GP_DEBUG_LOW, "konica", "-> %i bytes read.\n", (gint) *data_size);
	return (TRUE);
}
