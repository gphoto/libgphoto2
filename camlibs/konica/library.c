/*************************/
/* Included header files */
/*************************/
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <zlib.h>
#include <gphoto2.h>
#include "library.h"
#include "konica.h"


/*************/
/* Variables */
/*************/
char *models_qm100[] = {
	"Konica Q-EZ",	
	"Konica Q-M100", 
	"Konica Q-M100V",
	"HP PhotoSmart C20", 
	"HP PhotoSmart C30", 
	NULL};


char *models_qm200[] = {
	"Konica Q-M200", 
	"HP PhotoSmart C200",	
	NULL};


char **models[2] = {models_qm100, models_qm200};


/**************/
/* Prototypes */
/**************/
gboolean error_happened (Camera *camera, k_return_status_t return_status);
gboolean localization_file_read (
	Camera *camera, 
	gchar *file_name, 
	guchar **data,
	gulong *data_size);


/*************/
/* Functions */
/*************/
gboolean error_happened (Camera *camera, k_return_status_t return_status)
{
	g_return_val_if_fail (camera != NULL, GP_ERROR);
        switch (return_status) {
		case K_PROGRAM_ERROR:
			gp_camera_message (camera, "Program error!");
			return (TRUE);
                case K_L_IO_ERROR:
                        gp_camera_message (camera, "IO error!");
                        return (TRUE);
                case K_L_TRANSMISSION_ERROR:
                        gp_camera_message (camera, "Transmission error!\n");
                        return (TRUE);
                case K_L_SUCCESS:
                        return (FALSE);
                case K_SUCCESS:
                        return (FALSE);
                case K_ERROR_FOCUSING_ERROR:
                        gp_camera_message (camera, "Focussing error!");
                case K_ERROR_IRIS_ERROR:
                        gp_camera_message (camera, "Iris error!");
                        return (TRUE);
                case K_ERROR_STROBE_ERROR:
                        gp_camera_message (camera, "Strobe error!");
                        return (TRUE);
                case K_ERROR_EEPROM_CHECKSUM_ERROR:
                        gp_camera_message (camera, "Eeprom checksum error!");
                        return (TRUE);
                case K_ERROR_INTERNAL_ERROR1:
                        gp_camera_message (camera, "Internal error (1)!");
                        return (TRUE);
                case K_ERROR_INTERNAL_ERROR2:
                        gp_camera_message (camera, "Internal error (2)!");
                        return (TRUE);
                case K_ERROR_NO_CARD_PRESENT:
                        gp_camera_message (camera, "No card present!");
                        return (TRUE);
                case K_ERROR_CARD_NOT_SUPPORTED:
                        gp_camera_message (camera, "Card not supported!");
                        return (TRUE);
                case K_ERROR_CARD_REMOVED_DURING_ACCESS:
                        gp_camera_message (camera, "Card removed during access!");
                        return (TRUE);
                case K_ERROR_IMAGE_NUMBER_NOT_VALID:
                        gp_camera_message (camera, "Image number not valid!");
                        return (TRUE);
                case K_ERROR_CARD_CAN_NOT_BE_WRITTEN:
                        gp_camera_message (camera, "Card can not be written!");
                        return (TRUE);
                case K_ERROR_CARD_IS_WRITE_PROTECTED:
                        gp_camera_message (camera, "Card is write protected!");
                        return (TRUE);
                case K_ERROR_NO_SPACE_LEFT_ON_CARD:
                        gp_camera_message (camera, "ERROR!\n");
                        return (TRUE);
                case K_ERROR_NO_PICTURE_ERASED_AS_IMAGE_PROTECTED:
                        gp_camera_message (camera, "No picture erased as image protected!");
                        return (TRUE);
                case K_ERROR_LIGHT_TOO_DARK:
                        gp_camera_message (camera, "Light too dark!");
                        return (TRUE);
                case K_ERROR_AUTOFOCUS_ERROR:
                        gp_camera_message (camera, "Autofocus error!");
                        return (TRUE);
                case K_ERROR_SYSTEM_ERROR:
                        gp_camera_message (camera, "System Error!");
                        return (TRUE);
                case K_ERROR_ILLEGAL_PARAMETER:
                        gp_camera_message (camera, "Illegal parameter!");
                        return (TRUE);
                case K_ERROR_COMMAND_CANNOT_BE_CANCELLED:
                        gp_camera_message (camera, "Command cannot be cancelled!");
                        return (TRUE);
		case K_ERROR_LOCALIZATION_DATA_EXCESS:
			gp_camera_message (camera, "Too much localization data!\n");
			return (TRUE);
		case K_ERROR_LOCALIZATION_DATA_CORRUPT:
			gp_camera_message (camera, "Localization data corrupt!\n");
			return (TRUE);
                case K_ERROR_UNSUPPORTED_COMMAND:
                        gp_camera_message (camera, "Unsupported command!");
                        return (TRUE);
                case K_ERROR_OTHER_COMMAND_EXECUTING:
                        gp_camera_message (camera, "Other command executing!");
                        return (TRUE);
                case K_ERROR_COMMAND_ORDER_ERROR:
                        gp_camera_message (camera, "Command order error!");
                        return (TRUE);
                case K_ERROR_UNKNOWN_ERROR:
                        gp_camera_message (camera, "Unknown error!");
                        return (TRUE);
                default:
                        return (TRUE);
        }	
}


int camera_id (CameraText *id)
{
	g_return_val_if_fail (id != NULL, GP_ERROR);
	strcpy (id->text, "konica");
	return (GP_OK);
}


int camera_abilities (CameraAbilitiesList *list)
{
	gint i, j;
	CameraAbilities *a;

	g_return_val_if_fail (list != NULL, GP_ERROR);
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
			a->capture	= 1;
			a->config	= 1;
			a->file_delete	= 1;
			a->file_preview	= 1;
			a->file_put	= 0;
			gp_abilities_list_append (list, a);
		}
        }
	return (GP_OK);
}


int camera_init (Camera *camera, CameraInit *init)
{
	gint i, j;
	guint test_bit_rate[10] = {
		9600, 
		115200, 
		57600, 
		38400, 
		19200, 
		4800, 
		2400, 
		1200, 
		600, 
		300};
	guint bit_rate[10] = {
		300, 
		600, 
		1200, 
		2400, 
		4800, 
		9600, 
		19200, 
		38400, 
		57600, 
		115200};
	gboolean bit_rate_supported[10] = {
		FALSE, 
		FALSE, 
		FALSE, 
		FALSE, 
		FALSE, 
		FALSE, 
		FALSE, 
		FALSE, 
		FALSE, 
		FALSE}; 
	gboolean bit_flag_8_bits;
	gboolean bit_flag_stop_2_bits;
	gboolean bit_flag_parity_on;
	gboolean bit_flag_parity_odd;
	gboolean bit_flag_use_hw_flow_control;
	guint speed;
	konica_data_t *konica_data;

	g_return_val_if_fail (camera != NULL, GP_ERROR);
	g_return_val_if_fail (init != NULL, GP_ERROR);
	if (camera->debug == 1) printf ("*** Entering camera_init.\n");
	/* First, set up all the function pointers. */
	camera->functions->id 			= camera_id;
	camera->functions->abilities 		= camera_abilities;
	camera->functions->init 		= camera_init;
	camera->functions->exit 		= camera_exit;
	camera->functions->folder_list  	= camera_folder_list;
	camera->functions->file_list		= camera_file_list;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	camera->functions->file_put 		= camera_file_put;
	camera->functions->file_delete 		= camera_file_delete;
	camera->functions->config_get   	= camera_config_get;
	camera->functions->config_set   	= camera_config_set;
	camera->functions->capture 		= camera_capture;
	camera->functions->summary		= camera_summary;
	camera->functions->manual 		= camera_manual;
	camera->functions->about 		= camera_about;
	/* Store some data we constantly need. */
	konica_data = g_new (konica_data_t, 1);
	camera->camlib_data = konica_data;
	konica_data->port_settings = init->port_settings;
	konica_data->debug_flag = (camera->debug == 1);
konica_data->debug_flag = TRUE;
	konica_data->filesystem = gp_filesystem_new ();
	konica_data->image_id_long = FALSE;
	for (i = 0; i < 2; i++) {
		for (j = 0; ; j++) {
			if (models[i][j] == NULL) break;
			if (strcmp (models[i][j], init->model) == 0) {
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
	/* In case we got a speed not 0, let's first 	*/
	/* test if we do already have the given speed.	*/
	if (init->port_settings.speed != 0) {
		if (k_init (konica_data) == K_L_SUCCESS) {
			if (konica_data->debug_flag) 
				printf ("*** Leaving camera_init.\n");
			return (GP_OK);
		}
	}
	/* We either do have a speed of 0 or are not	*/
	/* communicating in the given speed. We'll test	*/
	/* what speed the camera currently uses...	*/
	for (i = 0; i < 10; i++) {
		if (konica_data->debug_flag) 
			printf ("-> Testing speed %i.\n", test_bit_rate[i]);
		konica_data->port_settings.speed = test_bit_rate[i]; 
		if (k_init (konica_data) == K_L_SUCCESS) break; 
	}
	if ((i == 1) && (init->port_settings.speed == 0)) {
		/* We have a speed of 0 and are already communicating at the  */
		/* highest bit rate possible. What else do we want?           */
		if (konica_data->debug_flag) 
			printf ("*** Leaving camera_init.\n");
		return (GP_OK);
	}
	if (i == 10) {
		gp_camera_message (
			camera, 
			"Could not communicate with camera!");
		return (GP_ERROR);
	}
	/* ... and what speed it is able to handle.	*/
	if (konica_data->debug_flag) printf ("-> Getting IO capabilities.\n");
	if (error_happened (camera, k_get_io_capability (
		konica_data,
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
		&bit_flag_use_hw_flow_control))) return (GP_ERROR);
	if (init->port_settings.speed == 0) {
		/* We are given 0. Set 	*/
		/* the highest speed.	*/
		for (i = 9; i >= 0; i--) if (bit_rate_supported[i]) break;
		if (i < 0) return (GP_ERROR);
		if (konica_data->debug_flag) 
			printf ("-> Setting speed to %i.\n", bit_rate[i]);
		if (error_happened (camera, k_set_io_capability (
			konica_data,
			bit_rate[i], 
			TRUE, 
			FALSE, 
			FALSE, 
			FALSE, 
			FALSE))) return (GP_ERROR);
		if (error_happened (camera, k_exit (konica_data)))
			return (GP_ERROR);
                konica_data->port_settings.speed = bit_rate[i];
                if (error_happened (camera, k_init (konica_data))) 
			return (GP_ERROR);
		if (konica_data->debug_flag) 
			printf ("*** Leaving camera_init.\n");
		return (GP_OK);
	} else {
		/* Does the camera allow us to set */
		/* the bit rate to given speed?	   */
		speed = (guint) konica_data->port_settings.speed;
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
			gp_camera_message (camera, "Unsupported speed!");
			return (GP_ERROR);
		}
		/* Now we can set the given speed. */
		if (error_happened (camera, k_set_io_capability (
			konica_data,
			konica_data->port_settings.speed, 
			TRUE, 
			FALSE, 
			FALSE, 
			FALSE, 
			FALSE))) return (GP_ERROR);
		if (error_happened (camera, k_exit (konica_data)))
			return (GP_ERROR);
		if (error_happened (camera, k_init (konica_data))) 
			return (GP_ERROR);
		if (konica_data->debug_flag) 
			printf ("*** Leaving camera_init.\n");
		return (GP_OK);
	}
}


int camera_exit (Camera *camera)
{
        konica_data_t *konica_data;

	g_return_val_if_fail (camera != NULL, GP_ERROR);
        konica_data = (konica_data_t *) camera->camlib_data;
	if (error_happened (camera, k_exit (konica_data))) return (GP_ERROR);
	return (GP_OK);
}


int camera_folder_list (Camera *camera, CameraList *list, char *folder)
{
	/* The camera does not support folders. */
	return (GP_OK);
}


int camera_file_list (Camera *camera, CameraList *list, char *folder)
{
	guint self_test_result;
	k_power_level_t power_level;
	k_power_source_t power_source;
	k_card_status_t card_status;
	k_display_t display;
	guint card_size;
	guint pictures = 0;
	guint pictures_left;
	guchar year;
	guchar month;
	guchar day;
	guchar hour;
	guchar minute;
	guchar second;
	guint io_setting_bit_rate;
	guint io_setting_flags;
	guchar flash;
	guchar quality;
	guchar focus;
	guchar exposure;
	guint total_pictures;
	guint total_strobes;
	guint i;
	konica_data_t *konica_data;

	g_return_val_if_fail (camera != NULL, GP_ERROR);
	g_return_val_if_fail (list != NULL, GP_ERROR);
	g_return_val_if_fail (folder != NULL, GP_ERROR);
	konica_data = (konica_data_t *) camera->camlib_data;
	if (konica_data->debug_flag) 
		printf ("*** Entering camera_file_list.\n");
	if (error_happened (camera, k_get_status (
		konica_data,
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
		&total_strobes))) return (GP_ERROR);
	/* We can't get the filename from the camera. 	*/
	/* We'll therefore take fake ones.		*/
	gp_filesystem_populate (
		konica_data->filesystem, 
		"/", 
		"%i.jpg", 
		(int) pictures);
	for (i = 0; i < gp_filesystem_count (konica_data->filesystem, folder); i++)
		gp_list_append (
			list, 
			gp_filesystem_name (konica_data->filesystem, folder, i),
			GP_LIST_FILE);
	if (konica_data->debug_flag) 
		printf ("*** Leaving camera_file_list.\n");
	return (GP_OK);
}


int camera_file_get (
	Camera *camera, 
	CameraFile *file, 
	char *folder, 
	char *filename)
{
	gulong image_id; 
	guint exif_size;
	gboolean protected;
	guchar *information_buffer = NULL;
	guint information_buffer_size;
	gulong image_number;
	konica_data_t *konica_data;

        g_return_val_if_fail (camera != NULL, GP_ERROR);
        g_return_val_if_fail (file != NULL, GP_ERROR);
        g_return_val_if_fail (folder != NULL, GP_ERROR);
	g_return_val_if_fail (filename != NULL, GP_ERROR);
	konica_data = (konica_data_t *) camera->camlib_data;
	if (konica_data->debug_flag) printf ("*** Entering camera_file_get.\n");
        image_number = gp_filesystem_number (
                konica_data->filesystem,
                folder,
                filename) + 1;
	if (error_happened (camera, k_get_image_information (
		konica_data,
		image_number, 
		&image_id, 
		&exif_size, 
		&protected, 
		&information_buffer, 
		&information_buffer_size))) return (GP_ERROR); 
	if (error_happened (camera, k_get_image (
		konica_data,
		image_id, 
		K_IMAGE_JPEG, 
		(guchar **) &file->data, 
		(guint *) &file->size))) return (GP_ERROR);
	strcpy(file->type, "image/jpg");
	sprintf (file->name, "Picture-%i.jpg", (int) image_id);
	g_free (information_buffer);
	if (konica_data->debug_flag) printf ("*** Leaving camera_file_get.\n");
	return (GP_OK);
}


int camera_file_get_preview (
	Camera *camera, 
	CameraFile *preview, 
	char *folder,
	char *filename)
{
	gulong image_id; 
	guint exif_size;
	gboolean protected;
	guchar *information_buffer = NULL;
	guint information_buffer_size;
	gulong image_number;
	konica_data_t *konica_data;

        g_return_val_if_fail (camera != NULL, GP_ERROR);
        g_return_val_if_fail (preview != NULL, GP_ERROR);
        g_return_val_if_fail (folder != NULL, GP_ERROR);
        g_return_val_if_fail (filename != NULL, GP_ERROR);
	konica_data = (konica_data_t *) camera->camlib_data;
	if (konica_data->debug_flag) 
		printf ("*** Entering camera_file_get_preview.\n");
        image_number = gp_filesystem_number (
                konica_data->filesystem,
                folder,
                filename) + 1;
	if (error_happened (camera, k_get_image_information (
		konica_data,
		image_number, 
		&image_id, 
		&exif_size, 
		&protected, 
		&information_buffer, 
		&information_buffer_size))) return (GP_ERROR); 
	if (error_happened (camera, k_get_image (
		konica_data,
		image_id, 
		K_THUMBNAIL, 
		(guchar **) &preview->data, 
		(guint *) &preview->size))) return (GP_ERROR);
	strcpy(preview->type, "image/jpg");
	sprintf (preview->name, "Thumbnail-%i.jpg", (int) image_id);
	g_free (information_buffer);
	if (konica_data->debug_flag) 
		printf ("*** Leaving camera_file_get_preview.\n");
	return (GP_OK);
}


int camera_file_put (Camera *camera, CameraFile *file, char *folder)
{
	/* The camera does not support putting files. */
	return (GP_ERROR);
}


int camera_file_delete (Camera *camera, char *folder, char *filename)
{
	gulong image_id; 
	guint exif_size;
	gboolean protected;
	guchar *information_buffer = NULL;
	guint information_buffer_size;
	konica_data_t *konica_data;
	gulong image_number;

        g_return_val_if_fail (camera != NULL, GP_ERROR);
        g_return_val_if_fail (folder != NULL, GP_ERROR);
        g_return_val_if_fail (filename != NULL, GP_ERROR);
	konica_data = (konica_data_t *) camera->camlib_data;
	if (konica_data->debug_flag) 
		printf ("*** Entering camera_file_delete.\n");
        image_number = gp_filesystem_number (
                konica_data->filesystem,
                folder,
                filename) + 1;
	if (error_happened (camera, k_get_image_information (
		konica_data,
		image_number, 
		&image_id, 
		&exif_size, 
		&protected, 
		&information_buffer, 
		&information_buffer_size))) return (GP_ERROR);
	if (error_happened (camera, k_erase_image (
		konica_data, 
		image_id))) return (GP_ERROR);
	if (konica_data->debug_flag) 
		printf ("*** Leaving camera_file_delete.\n");
	return (GP_OK);
}


int camera_summary (Camera *camera, CameraText *summary)
{
	gchar *model = NULL;
	gchar *serial_number = NULL;
	guchar hardware_version_major;
	guchar hardware_version_minor;
	guchar software_version_major;
	guchar software_version_minor;
	guchar testing_software_version_major;
	guchar testing_software_version_minor;
	gchar *name = NULL;
	gchar *manufacturer = NULL;
	guint shutoff_time;
	guint self_timer_time;
	guint beep;
	guint slide_show_interval;
	guchar year, month, day, hour, minute, second;
        konica_data_t *konica_data;

        g_return_val_if_fail (camera != NULL, GP_ERROR);
        g_return_val_if_fail (summary != NULL, GP_ERROR);
        konica_data = (konica_data_t *) camera->camlib_data;
	if (konica_data->debug_flag) printf ("*** Entering camera_summary.\n");
	if (error_happened (camera, k_get_information (
		konica_data,
		&model, 
		&serial_number, 
		&hardware_version_major, 
		&hardware_version_minor, 
		&software_version_major, 
		&software_version_minor, 
		&testing_software_version_major,
		&testing_software_version_minor,
		&name, 
		&manufacturer))) return (GP_ERROR);
        else if (error_happened (camera, k_get_preferences (
		konica_data,
		&shutoff_time, 
		&self_timer_time, 
		&beep, 
		&slide_show_interval))) return (GP_ERROR);
	else if (error_happened (camera, k_get_date_and_time (
		konica_data,
		&year, 
		&month, 
		&day, 
		&hour, 
		&minute, 
		&second))) return (GP_ERROR);
	else {
		sprintf (summary->text, 
			"About the camera:\n"
			"  Model: %s\n"
			"  Serial Number: %s,\n"
			"  Hardware Version: %i.%i\n"
			"  Software Version: %i.%i\n"
			"  Testing Software Version: %i.%i\n"
			"  Name: %s,\n"
			"  Manufacturer: %s\n"
			"Current settings:\n"
			"  Shutoff time: %i (min)\n"
			"  Self timer time: %i (sec)\n"
			"  Beep: %i\n"
			"  Slide show interval: %i (sec)\n"
			"Date and time:\n"
			"  Year: %i\n"
			"  Month: %i\n"
			"  Day: %i\n"
			"  Hour: %i\n"
			"  Minute: %i\n"
			"  Second: %i\n",
			model,
			serial_number,
			hardware_version_major, 
			hardware_version_minor,
			software_version_major, 
			software_version_minor,
			testing_software_version_major, 
			testing_software_version_minor,
			name,
			manufacturer,
			shutoff_time, 
			self_timer_time, 
			beep, 
			slide_show_interval, 
			year, 
			month, 
			day, 
			hour, 
			minute, 
			second);
		if (konica_data->debug_flag) 
			printf ("*** Leaving camera_summary.\n");
		return (GP_OK);
	}
}


int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) 
{
	gp_camera_message (camera, "Not yet implemented...\n");
	return (GP_ERROR);
}


int camera_manual (Camera *camera, CameraText *manual)
{
	strcpy(manual->text, "No manual available.");

	return (GP_OK);
}


int camera_about (Camera *camera, CameraText *about) 
{
	g_return_val_if_fail (camera != NULL, GP_ERROR);
	g_return_val_if_fail (about != NULL, GP_ERROR);
	strcpy (about->text, 
		"Konica library\n"
		"Lutz Müller <urc8@rz.uni-karlsruhe.de>\n"
		"Support for Konica and HP cameras.");
	return (GP_OK);
}


int camera_config_get (Camera *camera, CameraWidget *window)
{
        CameraWidget *widget, *section;
        guint shutoff_time, self_timer_time, beep, slide_show_interval;
        guint self_test_result;
        k_power_level_t power_level;
        k_power_source_t power_source;
        k_card_status_t card_status;
        k_display_t display;
        guint card_size;
        guint pictures = 0;
        guint pictures_left;
        guchar year;
        guchar month;
        guchar day;
        guchar hour;
        guchar minute;
        guchar second;
        guint io_setting_bit_rate;
        guint io_setting_flags;
        guchar flash;
        guchar resolution;
        guchar focus_self_timer;
        guchar exposure;
        guint total_pictures;
        guint total_strobes;
	konica_data_t *konica_data;
	gchar buffer[1024];

	g_return_val_if_fail (camera != NULL, GP_ERROR);
	g_return_val_if_fail (window != NULL, GP_ERROR);
	konica_data = (konica_data_t *) camera->camlib_data;
        if (error_happened (camera, k_get_status (
		konica_data, 
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
                &total_strobes))) return (GP_ERROR);
        if (error_happened (camera, k_get_preferences (
		konica_data,
                &shutoff_time,
                &self_timer_time,
                &beep,
                &slide_show_interval))) return (GP_ERROR);
        /************************/
        /* Persistent Settings  */
        /************************/
        section = gp_widget_new (GP_WIDGET_SECTION, "Persistent Settings");
        gp_widget_append (window, section);
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
	sprintf (buffer, "%i", self_timer_time);
	gp_widget_value_set (widget, buffer);
        /* Auto Off Time */
        widget = gp_widget_new (GP_WIDGET_RANGE, "Auto Off Time");
        gp_widget_append (section, widget);
        gp_widget_range_set (widget, 1, 255, 1);
        sprintf (buffer, "%i", shutoff_time);
	gp_widget_value_set (widget, buffer);
        /* Slide Show Interval */
        widget = gp_widget_new (GP_WIDGET_RANGE, "Slide Show Interval");
        gp_widget_append (section, widget);
        gp_widget_range_set (widget, 1, 30, 1);
	sprintf (buffer, "%i", slide_show_interval);
	gp_widget_value_set (widget, buffer);
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
        gp_widget_append (window, section);
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
        section = gp_widget_new (
                GP_WIDGET_SECTION,
                "Session-persistent Settings");
        gp_widget_append (window, section);
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
	sprintf (buffer, "%i", exposure);
	gp_widget_value_set (widget, buffer);
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
        gp_widget_append (window, section);
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
        return (GP_OK);
}


int camera_config_set (Camera *camera, CameraSetting *setting, int count)
{
	guint i, j, focus, self_timer;
	konica_data_t *konica_data;
	guchar *data;
	gulong data_size;
	k_date_format_t date_format;
	k_tv_output_format tv_output_format;

        g_return_val_if_fail (camera != NULL, GP_ERROR);
	g_return_val_if_fail (setting != NULL, GP_ERROR);
	focus = 0;
	self_timer = 0;
	konica_data = (konica_data_t *) camera->camlib_data;
	if (konica_data->debug_flag) printf ("*** Entering camera_config_set.\n");
	for (i = 0; i < count; i++) {
		if (konica_data->debug_flag) printf ("-> Setting \"%s\" to \"%s\".\n", setting[i].name, setting[i].value);
                if (strcmp (setting[i].name, "Beep") == 0) {
			if (strcmp (setting[i].value, "Off") == 0) j = 0;
			else j = 1;
			if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_BEEP, j))) return (GP_ERROR);
                        continue;
                }
                if (strcmp (setting[i].name, "Self Timer Time") == 0) {
                        if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_SELF_TIMER_TIME, (guint) atoi (setting[i].value)))) return (GP_ERROR);
                        continue;
                }
                if (strcmp (setting[i].name, "Auto Off Time") == 0) {
                        if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_AUTO_OFF_TIME, (guint) atoi (setting[i].value)))) return (GP_ERROR);
                        continue;
                }
                if (strcmp (setting[i].name, "Slide Show Interval") == 0) {
                        if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_SLIDE_SHOW_INTERVAL, (guint) atoi (setting[i].value)))) return (GP_ERROR);
                        continue;
                }
                if (strcmp (setting[i].name, "Resolution") == 0) {
			if (strcmp (setting[i].value, "High (1152 x 872)") == 0) j = 1;
			if (strcmp (setting[i].value, "Low (576 x 436)") == 0) j = 3;
			else j = 0;
			if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_RESOLUTION, j))) return (GP_ERROR);
                        continue;
                }
		if (strcmp (setting[i].name, "TV Output Format") == 0) {
			if (strcmp (setting[i].value, "NTSC") == 0) tv_output_format = K_TV_OUTPUT_FORMAT_NTSC;
			else if (strcmp (setting[i].value, "PAL") == 0) tv_output_format = K_TV_OUTPUT_FORMAT_PAL;
			else if (strcmp (setting[i].value, "Do not display TV menu") == 0) tv_output_format = K_TV_OUTPUT_FORMAT_HIDE;
			else continue;
			if (error_happened (camera, k_localization_tv_output_format_set (konica_data, tv_output_format))) return (GP_ERROR);
			continue;
		}
		if (strcmp (setting[i].name, "Date Format") == 0) {
			if (strcmp (setting[i].value, "Month/Day/Year") == 0) date_format = K_DATE_FORMAT_MONTH_DAY_YEAR;
			else if (strcmp (setting[i].value, "Day/Month/Year") == 0) date_format = K_DATE_FORMAT_DAY_MONTH_YEAR;
			else if (strcmp (setting[i].value, "Year/Month/Day") == 0) date_format = K_DATE_FORMAT_YEAR_MONTH_DAY;
			else continue;
			if (error_happened (camera, k_localization_date_format_set (konica_data, date_format))) return (GP_ERROR);
			continue;
		}
                if (strcmp (setting[i].name, "Localization File") == 0) {
                        if (strcmp (setting[i].value, "") != 0) {
				data = NULL;
				data_size = 0;
				/* Read localization file */
				if (!localization_file_read (camera, setting[i].value, &data, &data_size)) {
					g_free (data);
					return (GP_ERROR);
				}
				/* Go! */
				if (error_happened (camera, k_localization_data_put (konica_data, data, data_size))) {
					g_free (data);
					return (GP_ERROR);
				}
				g_free (data);
			}
                        continue;
                }
                if (strcmp (setting[i].name, "Flash") == 0) {
			if (strcmp (setting[i].value, "Off") == 0) j = 0;
			else if (strcmp (setting[i].value, "On") == 0) j = 1;
			else if (strcmp (setting[i].value, "On, red-eye reduction") == 0) j = 5;
			else if (strcmp (setting[i].value, "Auto, red-eye reduction") == 0) j = 6;
			else j = 2;
                        if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_FLASH, j))) return (GP_ERROR);
                        continue;
                }
                if (strcmp (setting[i].name, "Exposure") == 0) {
                        if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_EXPOSURE, (guint) atoi (setting[i].value)))) return (GP_ERROR);
                        continue;
                }
                if (strcmp (setting[i].name, "Focus") == 0) {
			if (strcmp (setting[i].value, "Auto") == 0) focus = 2;
			if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_FOCUS_SELF_TIMER, focus + self_timer))) return (GP_ERROR);
                        continue;
                }
                if (strcmp (setting[i].name, "Self Timer") == 0) {
			if (strcmp (setting[i].value, "Self Timer (only next picture)") == 0) self_timer = 1;
			if (error_happened (camera, k_set_preference (konica_data, K_PREFERENCE_FOCUS_SELF_TIMER, focus + self_timer))) return (GP_ERROR);
                        continue;
                }
	}
	if (konica_data->debug_flag) printf ("*** Leaving camera_config_set.\n");
	return (GP_OK);
}


gboolean localization_file_read (
	Camera *camera, 
	gchar *file_name, 
	guchar **data, 
	gulong *data_size)
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
		gp_camera_message (camera, "Could not open requested localization file!");
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
				gp_camera_message (camera, message);
				g_free (message);
				fclose (file);
				return (FALSE);
			}
			c[j] = f;
			if (j == 1) {
				if (sscanf (&c[0], "%X", &d) != 1) {
					gp_camera_message (
						camera,
						"There seems to be an error in"
						" the localization file.");
						fclose (file);
						return (FALSE);
				}
				(*data)[*data_size] = d;
				(*data_size)++;
				if (*data_size == 65536) {
					gp_camera_message (
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
	if (konica_data->debug_flag) 
		printf ("-> %i bytes read.\n", (gint) *data_size);
	return (TRUE);
}

