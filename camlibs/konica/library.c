/****************************************************************/
/* Included header files					*/
/****************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gphoto2.h>
#include "konica.h"


/****************************************************************/
/* Type Definitions						*/
/****************************************************************/
typedef enum {
	QM100,
	QM200} qmxxx_protocol_t;


/****************************************************************/
/* Variables                                                    */
/****************************************************************/
gboolean debug_flag = FALSE;


char *models_qm100[] = {"Konica Q-EZ",	"Konica Q-M100", "HP PhotoSmart C20", "HP PhotoSmart C30", NULL};


char *models_qm200[] = {"Konica Q-M200", "HP PhotoSmart C200",	NULL};


char **models[2] = {models_qm100, models_qm200};


/****************************************************************/
/* Prototypes                                                   */
/****************************************************************/
gboolean error_happened (k_return_status_t return_status);


/****************************************************************/
/* Functions                                                    */
/****************************************************************/
gboolean error_happened (k_return_status_t return_status)
{
        switch (return_status) {
		case K_PROGRAM_ERROR:
			gp_message ("Program error!");
			return (TRUE);
                case K_L_IO_ERROR:
                        gp_message ("IO error!");
                        return (TRUE);
                case K_L_TRANSMISSION_ERROR:
                        gp_message ("Transmission error!\n");
                        return (TRUE);
                case K_L_SUCCESS:
                        return (FALSE);
                case K_SUCCESS:
                        return (FALSE);
                case K_ERROR_FOCUSING_ERROR:
                        gp_message ("Focussing error!");
                case K_ERROR_IRIS_ERROR:
                        gp_message ("Iris error!");
                        return (TRUE);
                case K_ERROR_STROBE_ERROR:
                        gp_message ("Strobe error!");
                        return (TRUE);
                case K_ERROR_EEPROM_CHECKSUM_ERROR:
                        gp_message ("Eeprom checksum error!");
                        return (TRUE);
                case K_ERROR_INTERNAL_ERROR1:
                        gp_message ("Internal error (1)!");
                        return (TRUE);
                case K_ERROR_INTERNAL_ERROR2:
                        gp_message ("Internal error (2)!");
                        return (TRUE);
                case K_ERROR_NO_CARD_PRESENT:
                        gp_message ("No card present!");
                        return (TRUE);
                case K_ERROR_CARD_NOT_SUPPORTED:
                        gp_message ("Card not supported!");
                        return (TRUE);
                case K_ERROR_CARD_REMOVED_DURING_ACCESS:
                        gp_message ("Card removed during access!");
                        return (TRUE);
                case K_ERROR_IMAGE_NOT_PROTECTED_DOES_NOT_EXIST:
                        gp_message ("Image not protected (does not exist)!");
                        return (TRUE);
                case K_ERROR_CARD_CAN_NOT_BE_WRITTEN:
                        gp_message ("Card can not be written!");
                        return (TRUE);
                case K_ERROR_CARD_IS_WRITE_PROTECTED:
                        gp_message ("Card is write protected!");
                        return (TRUE);
                case K_ERROR_NO_SPACE_LEFT_ON_CARD:
                        gp_message ("ERROR!\n");
                        return (TRUE);
                case K_ERROR_NO_PICTURE_ERASED_AS_IMAGE_PROTECTED:
                        gp_message ("No picture erased as image protected!");
                        return (TRUE);
                case K_ERROR_LIGHT_TOO_DARK:
                        gp_message ("Light too dark!");
                        return (TRUE);
                case K_ERROR_AUTOFOCUS_ERROR:
                        gp_message ("Autofocus error!");
                        return (TRUE);
                case K_ERROR_SYSTEM_ERROR:
                        gp_message ("System Error!");
                        return (TRUE);
                case K_ERROR_ILLEGAL_PARAMETER:
                        gp_message ("Illegal parameter!");
                        return (TRUE);
                case K_ERROR_COMMAND_CANNOT_BE_CANCELLED:
                        gp_message ("Command cannot be cancelled!");
                        return (TRUE);
                case K_ERROR_IMAGE_NUMBER_NOT_VALID:
                        gp_message ("Image number not valid!");
                        return (TRUE);
                case K_ERROR_UNSUPPORTED_COMMAND:
                        gp_message ("Unsupported command!");
                        return (TRUE);
                case K_ERROR_OTHER_COMMAND_EXECUTING:
                        gp_message ("Other command executing!");
                        return (TRUE);
                case K_ERROR_COMMAND_ORDER_ERROR:
                        gp_message ("Command order error!");
                        return (TRUE);
                case K_ERROR_UNKNOWN_ERROR:
                        gp_message ("Unknown error!");
                        return (TRUE);
                case K_ERROR_NOT_YET_DISCOVERED:
                        gp_message ("This is an unknown error message!\n");
                        return (TRUE);
                default:
                        return (TRUE);
        }	
}


int camera_id (char *id)
{
	strcpy (id, "konica");
	return (GP_OK);
}


int camera_abilities (CameraAbilities *abilities, int *count)
{
	gint i, j;
	int x = 0;
	CameraAbilities a;

	a.serial   = 1;
	a.parallel = 0;
	a.usb      = 0;
	a.ieee1394 = 0;
	a.speed[0] = 300;
	a.speed[1] = 600;
	a.speed[2] = 1200;
	a.speed[3] = 2400;
	a.speed[4] = 4800;
	a.speed[5] = 9600;
	a.speed[6] = 19200;
	a.speed[7] = 38400;
	a.speed[8] = 57600;
	a.speed[9] = 115200;
	a.speed[10] = 0;
	a.capture   = 1;
	a.config    = 1;
	a.file_delete  = 1;
	a.file_preview = 1;
	a.file_put  = 0;
	for (i = 0; i < 2; i++) {
		for (j = 0; ; j++) {
			if (models[i][j] == NULL) break;
			memcpy (&abilities[x], &a, sizeof(a));
			strcpy (abilities[x].model, models[i][j]);
			x++;
		}
        }
	*count = x;
	return (GP_OK);
}


int camera_init (CameraInit *init)
{
	gint i, j;
	CameraPortSettings port_settings;
	gboolean image_id_long = FALSE;
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

	if (init->debug == 1) debug_flag = TRUE;
	if (debug_flag) printf ("*** Entering camera_init.\n");
	else debug_flag = FALSE;
	for (i = 0; i < 2; i++) {
		for (j = 0; ; j++) {
			if (models[i][j] == NULL) break;
			if (strcmp (models[i][j], init->model) == 0) {
				if (i == 1) image_id_long = TRUE;
				break;
			}
		}
	}
	/**********************************************************************/
	/* Speed: We are given 0: We'll try all possibilities, starting with  */
	/*          9600, then trying all other values. Once we found the     */
	/*          camera's current speed, we'll set it to the highest value */
	/*          supported by the camera.                                  */
        /*        We are not given 0: We will first try to communicate with   */
	/*          the camera with the given speed. If we do not succeed, we */
	/*          will try all possibilities. If the given speed does not   */
	/*          equal the current speed, we will change the current speed */
	/*          to the given one.                                         */
	/**********************************************************************/
	strcpy (port_settings.path, init->port_settings.path);
	/************************************************/
	/* In case we got a speed not 0, let's first 	*/
	/* test if we do already have the given speed.	*/
	/************************************************/
	if (init->port_settings.speed != 0) {
		if (k_init (
			image_id_long, 
			init->port_settings, 
			debug_flag) == K_L_SUCCESS) {
			if (debug_flag) printf ("*** Leaving camera_init.\n");
			return (GP_OK);
		}
	}
	/************************************************/
	/* We either do have a speed of 0 or are not	*/
	/* communicating in the given speed. We'll test	*/
	/* what speed the camera currently uses...	*/
	/************************************************/
	for (i = 0; i < 10; i++) {
		port_settings.speed = test_bit_rate[i]; 
		if (k_init (
			image_id_long, 
			port_settings, 
			debug_flag) == K_L_SUCCESS) break; 
	}
	if ((i == 1) && (init->port_settings.speed == 0)) {
		/**************************************************************/
		/* We have a speed of 0 and are already communicating at the  */
		/* highest bit rate possible. What else do we want...         */
		/**************************************************************/
		if (debug_flag) printf ("*** Leaving camera_init.\n");
		return (GP_OK);
	}
	if (i == 10) {
		gp_message ("Could not communicate with camera!");
		return (GP_ERROR);
	}
	/************************************************/
	/* ... and what speed it is able to handle.	*/
	/************************************************/
	if (error_happened (k_get_io_capability (
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
		/************************/
		/* We are given 0. Set 	*/
		/* the highest speed.	*/
		/************************/
		for (i = 9; i >= 0; i--) if (bit_rate_supported[i]) break;
		if (i < 0) return (GP_ERROR);
		if (error_happened (k_set_io_capability (
			bit_rate[i], 
			TRUE, 
			FALSE, 
			FALSE, 
			FALSE, 
			FALSE))) return (GP_ERROR);
		port_settings.speed = bit_rate[i]; 
                if (error_happened (k_init (
			image_id_long, 
			port_settings, 
			debug_flag))) return (GP_ERROR);
		if (debug_flag) printf ("*** Leaving camera_init.\n");
		return (GP_OK);
	} else {
		/***********************************/
		/* Does the camera allow us to set */
		/* the bit rate to given speed?	   */
		/***********************************/
		speed = (guint) init->port_settings.speed;
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
			gp_message ("Unsupported speed!");
			return (GP_ERROR);
		}
		/***********************************/
		/* Now we can set the given speed. */
		/***********************************/
		if (error_happened (k_set_io_capability (
			init->port_settings.speed, 
			TRUE, 
			FALSE, 
			FALSE, 
			FALSE, 
			FALSE))) return (GP_ERROR);
		if (error_happened (k_init (
			image_id_long, 
			init->port_settings, 
			debug_flag))) return (GP_ERROR);
		if (debug_flag) printf ("*** Leaving camera_init.\n");
		return (GP_OK);
	}
}


int camera_exit ()
{
	return (GP_OK);
}


int camera_folder_list (char *folder_name, CameraFolderInfo *list)
{
	return (GP_OK);
}


int camera_folder_set (char *folder_name)
{
	return (GP_OK);
}


int camera_file_count ()
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

	if (error_happened (k_get_status (
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
	return ((int) pictures);
}


int camera_file_get (int file_number, CameraFile *file)
{
	gulong image_id; 
	guint exif_size;
	gboolean protected;
	guchar *information_buffer = NULL;
	guint information_buffer_size;

	if (error_happened (k_get_image_information (
		(gulong) (file_number + 1), 
		&image_id, 
		&exif_size, 
		&protected, 
		&information_buffer, 
		&information_buffer_size))) return (GP_ERROR); 
	if (error_happened (k_get_image (
		image_id, 
		K_IMAGE_JPEG, 
		(guchar **) &file->data, 
		(guint *) &file->size))) return (GP_ERROR);
	strcpy(file->type, "image/jpg");
	sprintf (file->name, "%i.jpg", (int) image_id);
	g_free (information_buffer);
	return (GP_OK);
}


int camera_file_get_preview (int file_number, CameraFile *preview)
{
	gulong image_id; 
	guint exif_size;
	gboolean protected;
	guchar *information_buffer = NULL;
	guint information_buffer_size;

	if (error_happened (k_get_image_information (
		(gulong) (file_number + 1), 
		&image_id, 
		&exif_size, 
		&protected, 
		&information_buffer, 
		&information_buffer_size))) return (GP_ERROR); 
		return (0);
	if (error_happened (k_get_image (
		image_id, 
		K_THUMBNAIL, 
		(guchar **) &preview->data, 
		(guint *) &preview->size))) return (GP_ERROR);
	strcpy(preview->type, "image/jpg");
	sprintf (preview->name, "%i.jpg", (int) image_id);
	g_free (information_buffer);
	return (GP_OK);
}


int camera_file_put (CameraFile *file)
{
	return (GP_ERROR);
}


int camera_file_delete (int file_number)
{
	gulong image_id; 
	guint exif_size;
	gboolean protected;
	guchar *information_buffer = NULL;
	guint information_buffer_size;

	if (error_happened (k_get_image_information (
		file_number, 
		&image_id, 
		&exif_size, 
		&protected, 
		&information_buffer, 
		&information_buffer_size))) return (GP_ERROR);
	if (error_happened (k_erase_image (image_id))) return (GP_ERROR);
	return (GP_OK);
}


int camera_summary (char *summary)
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

	if (debug_flag) printf ("*** Entering camera_summary.\n");
	if (error_happened (k_get_information (
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
        else if (error_happened (k_get_preferences (
		&shutoff_time, 
		&self_timer_time, 
		&beep, 
		&slide_show_interval))) return (GP_ERROR);
	else if (error_happened (k_get_date_and_time (
		&year, 
		&month, 
		&day, 
		&hour, 
		&minute, 
		&second))) return (GP_ERROR);
	else {
		sprintf (summary, 
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
		if (debug_flag) printf ("*** Leaving camera_summary.\n");
		return (GP_OK);
	}
}


int camera_about (char *about) 
{
	strcpy (about, 
		"Konica library\n"
		"Lutz Müller <urc8@rz.uni-karlsruhe.de>\n"
		"Support for Konica and HP cameras.");
	return (GP_OK);
}


int camera_config_get (CameraWidget *window)
{
	printf ("Not yet implemented...\n");
	return (GP_ERROR);
}


int camera_config_set (CameraSetting *conf, int count)
{
	printf ("Not yet implemented...\n");
	return (GP_ERROR);
}


