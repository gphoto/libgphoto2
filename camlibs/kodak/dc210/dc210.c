#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  ifdef gettext_noop
#    define _(String) dgettext (PACKAGE, String)
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "dc210.h"

char * exp_comp[] = {"-2.0 EV", "-1.5 EV", "-1.0 EV", "-0.5 EV", "AUTO", "+0.5 EV",
		     "+1.0 EV", "+1.5 EV", "+2.0 EV"};

int camera_id (CameraText *id) {

	strcpy(id->text, GP_MODULE);

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities a;

	memset (&a, 0, sizeof(a));
	strcpy(a.model, "Kodak DC210");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     = GP_PORT_SERIAL;
	a.speed[0] = 9600;
	a.speed[1] = 19200;
	a.speed[2] = 38400;
	a.speed[3] = 57600;
	a.speed[4] = 115200;
	a.speed[5] = 0;
	a.operations        = 	GP_OPERATION_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
	a.file_operations   = 	GP_FILE_OPERATION_NONE;
	a.folder_operations = 	GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context) 
{
    Camera *camera = data;
    
    return dc210_get_filenames(camera, list, context);
    
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data, GPContext *context) 
{
	Camera *camera = data;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		return dc210_download_picture_by_name(camera, file, filename, 0, context);
	case GP_FILE_TYPE_PREVIEW:
		return dc210_download_picture_by_name(camera, file, filename, 1, context);
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	};
}

static int delete_file_func (CameraFilesystem *fs, const char *folder, 
			     const char *filename, void *data,
			     GPContext *context) 
{
	Camera *camera = data;

	return dc210_delete_picture_by_name(camera, filename);

};

static int get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
			  CameraFileInfo *info, void *data, GPContext *context)
{
        Camera *camera = data;
	int picno;
	dc210_picture_info picinfo;
	
	picno = dc210_get_picture_number(camera, file);

	if (picno < 1) return GP_ERROR;

	if (dc210_get_picture_info(camera, &picinfo, picno) == GP_ERROR)
		return GP_ERROR;

	info->preview.fields |= GP_FILE_INFO_TYPE;
	info->preview.fields |= GP_FILE_INFO_NAME;
	info->preview.fields |= GP_FILE_INFO_SIZE;
	info->preview.fields |= GP_FILE_INFO_WIDTH;
	info->preview.fields |= GP_FILE_INFO_HEIGHT;

	strcpy(info->preview.type, GP_MIME_PPM);
	info->preview.width = 96;
	info->preview.height = 72;
	info->preview.size = picinfo.preview_size;
	strncpy(info->file.name, picinfo.image_name, 9);
	strncpy(info->file.name + 9, "PPM\0", 4);

	info->file.fields |= GP_FILE_INFO_TYPE;
	info->file.fields |= GP_FILE_INFO_NAME;
	info->file.fields |= GP_FILE_INFO_SIZE;
	info->file.fields |= GP_FILE_INFO_WIDTH;
	info->file.fields |= GP_FILE_INFO_HEIGHT;
	info->file.fields |= GP_FILE_INFO_MTIME;

	info->file.size = picinfo.picture_size;
	switch(picinfo.file_type){
	case DC210_FILE_TYPE_JPEG:
		strcpy(info->file.type, GP_MIME_JPEG); break;
	case DC210_FILE_TYPE_FPX:
		strcpy(info->file.type, GP_MIME_UNKNOWN); break;
	};
	switch(picinfo.resolution){
	case DC210_FILE_640:
		info->file.width = 640;
		info->file.height = 480;
		break;
	case DC210_FILE_1152:
		info->file.width = 1152;
		info->file.height = 864;
		break;
	};
	strncpy(info->file.name, picinfo.image_name, 13);
	info->file.mtime = picinfo.picture_time;

	info->audio.fields |= GP_FILE_INFO_NONE;

	return GP_OK;

};


static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *section, *widget;
	CameraAbilities abilities;
	GPPortSettings settings;
	int i;
	char * wvalue;
	char stringbuffer[12];
	
	dc210_status status;

	if (dc210_get_status(camera, &status) == GP_ERROR) return GP_ERROR;

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	gp_widget_new (GP_WIDGET_SECTION, _("File"), &section);
	gp_widget_append (*window, section);

        gp_widget_new (GP_WIDGET_RADIO, _("File type"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("JPEG"));
        gp_widget_add_choice (widget, _("FlashPix"));

	switch (status.file_type){
	case DC210_FILE_TYPE_JPEG:  
	  gp_widget_set_value (widget, _("JPEG")); break;
	case DC210_FILE_TYPE_FPX:  
	  gp_widget_set_value (widget, _("FlashPix")); break;
	};
	gp_widget_get_value (widget, &wvalue);

        gp_widget_new (GP_WIDGET_RADIO, _("File resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("640 x 480"));
        gp_widget_add_choice (widget, _("1152 x 864"));

	switch (status.resolution){
	case DC210_FILE_640:  
	  gp_widget_set_value (widget, _("640 x 480")); break;
	case DC210_FILE_1152:  
	  gp_widget_set_value (widget, _("1152 x 864")); break;
	default:
	  DC210_DEBUG("Undefined value for file resolution.\n"); break;
	};
	gp_widget_get_value (widget, &wvalue);

        gp_widget_new (GP_WIDGET_MENU, _("File compression"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Low (best quality)"));
        gp_widget_add_choice (widget, _("Medium (better quality)"));
        gp_widget_add_choice (widget, _("High (good quality)"));

	switch (status.compression_type){
	case DC210_LOW_COMPRESSION:  
	  gp_widget_set_value (widget, _("Low (best quality)")); break;
	case DC210_MEDIUM_COMPRESSION:  
	  gp_widget_set_value (widget, _("Medium (better quality)")); break;
	case DC210_HIGH_COMPRESSION:  
	  gp_widget_set_value (widget, _("High (good quality)")); break;
	};
	gp_widget_get_value (widget, &wvalue);

	gp_widget_new (GP_WIDGET_SECTION, _("Capture"), &section);
	gp_widget_append (*window, section);

        gp_widget_new (GP_WIDGET_MENU, _("Zoom"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("58 mm"));
        gp_widget_add_choice (widget, _("51 mm"));
        gp_widget_add_choice (widget, _("41 mm"));
        gp_widget_add_choice (widget, _("34 mm"));
        gp_widget_add_choice (widget, _("29 mm"));
        gp_widget_add_choice (widget, _("Macro"));

	switch (status.zoom){
	case DC210_ZOOM_58:  
	  gp_widget_set_value (widget, _("58 mm")); break;
	case DC210_ZOOM_51:  
	  gp_widget_set_value (widget, _("51 mm")); break;
	case DC210_ZOOM_41:  
	  gp_widget_set_value (widget, _("41 mm")); break;
	case DC210_ZOOM_34:  
	  gp_widget_set_value (widget, _("34 mm")); break;
	case DC210_ZOOM_29:  
	  gp_widget_set_value (widget, _("29 mm")); break;
	case DC210_ZOOM_MACRO:  
	  gp_widget_set_value (widget, _("Macro")); break;
	};
	gp_widget_get_value (widget, &wvalue);

        gp_widget_new (GP_WIDGET_MENU, _("Exposure compensation"), &widget);
        gp_widget_append (section, widget);
	for (i = 0; i < sizeof(exp_comp)/sizeof(*exp_comp); i++){
		gp_widget_add_choice (widget, exp_comp[i]);
		if (status.exp_compensation + 4 == i)
			gp_widget_set_value (widget, exp_comp[i]);
	};

        gp_widget_new (GP_WIDGET_RADIO, _("Flash"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Auto"));
        gp_widget_add_choice (widget, _("Force"));
        gp_widget_add_choice (widget, _("None"));

	switch (status.flash){
	case DC210_FLASH_AUTO:  
	  gp_widget_set_value (widget, _("Auto")); break;
	case DC210_FLASH_FORCE:  
	  gp_widget_set_value (widget, _("Force")); break;
	case DC210_FLASH_NONE:  
	  gp_widget_set_value (widget, _("None")); break;
	};
	gp_widget_get_value (widget, &wvalue);

        gp_widget_new (GP_WIDGET_RADIO, _("Red eye flash"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("On"));
        gp_widget_add_choice (widget, _("Off"));

	if (status.preflash)
	  gp_widget_set_value (widget, _("On"));
	else
	  gp_widget_set_value (widget, _("Off"));
	gp_widget_get_value (widget, &wvalue);

	gp_widget_new (GP_WIDGET_SECTION, _("Other"), &section);
	gp_widget_append (*window, section);

        gp_widget_new (GP_WIDGET_BUTTON, "Set time to system time", &widget);
        gp_widget_append (section, widget);
	gp_widget_set_value (widget, dc210_system_time_callback);
	gp_widget_set_info (widget, _("Set clock in camera"));
	
	gp_camera_get_abilities(camera, &abilities);
	gp_port_get_settings (camera->port, &settings);
        gp_widget_new (GP_WIDGET_MENU, _("Port speed"), &widget);
        gp_widget_append (section, widget);
	for (i = 0; i < sizeof(abilities.speed); i++){
		if (abilities.speed[i] == 0) break;
		snprintf(stringbuffer, 12, "%d", abilities.speed[i]);
		gp_widget_add_choice (widget, stringbuffer);
		if (settings.serial.speed == abilities.speed[i])
			gp_widget_set_value (widget, stringbuffer);
	};

        gp_widget_new (GP_WIDGET_TEXT, _("Album name"), &widget);
        gp_widget_append (section, widget);
	gp_widget_set_value (widget, status.album_name);
	gp_widget_set_info (widget, _("Name to set on card when formatting."));

        gp_widget_new (GP_WIDGET_BUTTON, _("Format compact flash"), &widget);
        gp_widget_append (section, widget);
	gp_widget_set_value (widget, dc210_format_callback);
	gp_widget_set_info (widget, _("Format card and set album name."));

#ifdef DEBUG
	gp_widget_new (GP_WIDGET_SECTION, _("Debug"), &section);
	gp_widget_append (*window, section);

        gp_widget_new (GP_WIDGET_TEXT, "Parameter 1", &widget);
        gp_widget_append (section, widget);
	gp_widget_set_value (widget, "0");

        gp_widget_new (GP_WIDGET_TEXT, "Parameter 2", &widget);
        gp_widget_append (section, widget);
	gp_widget_set_value (widget, "0");

        gp_widget_new (GP_WIDGET_TEXT, "Parameter 3", &widget);
        gp_widget_append (section, widget);
	gp_widget_set_value (widget, "0");

        gp_widget_new (GP_WIDGET_BUTTON, "Execute debug command", &widget);
        gp_widget_append (section, widget);
	gp_widget_set_value (widget, dc210_debug_callback);
	gp_widget_set_info (widget, _("Execute predefined command\nwith parameter values."));
#endif

	return GP_OK;
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *w, *w2;
	char *wvalue, *w2value;
	int i;

	gp_widget_get_child_by_label (window, _("File type"), &w);
	if (gp_widget_changed (w)) {
		gp_widget_get_value (w, &wvalue);
		if (wvalue[0] == 'J')
			dc210_set_file_type(camera, DC210_FILE_TYPE_JPEG);
		else
			dc210_set_file_type(camera, DC210_FILE_TYPE_FPX);
	};

	gp_widget_get_child_by_label (window, _("File resolution"), &w);
	if (gp_widget_changed (w)) {
		gp_widget_get_value (w, &wvalue);
		switch(wvalue[0]){
		case '6':
		  dc210_set_resolution(camera, DC210_FILE_640);
		  break;
		case '1':
		  dc210_set_resolution(camera, DC210_FILE_1152);
		  break;
		};
	};

	gp_widget_get_child_by_label (window, _("File compression"), &w);
	if (gp_widget_changed (w)) {
		gp_widget_get_value (w, &wvalue);
		switch(wvalue[0]){
		case 'L':
		  dc210_set_compression(camera, DC210_LOW_COMPRESSION);
		  break;
		case 'M':
		  dc210_set_compression(camera, DC210_MEDIUM_COMPRESSION);
		  break;
		case 'H':
		  dc210_set_compression(camera, DC210_HIGH_COMPRESSION);
		};
	};

	gp_widget_get_child_by_label (window, _("Zoom"), &w);
	if (gp_widget_changed (w)) {
		gp_widget_get_value (w, &wvalue);
		switch(wvalue[0]){
		case '5':
			if (wvalue[1] == '8')
				dc210_set_zoom(camera, DC210_ZOOM_58);
			else
				dc210_set_zoom(camera, DC210_ZOOM_51);
			break;
		case '4':
			dc210_set_zoom(camera, DC210_ZOOM_41);
			break;
		case '3':
			dc210_set_zoom(camera, DC210_ZOOM_34);
			break;
		case '2':
			dc210_set_zoom(camera, DC210_ZOOM_29);
			break;
		case 'M':
			dc210_set_zoom(camera, DC210_ZOOM_MACRO);
			break;
		};
	};

	gp_widget_get_child_by_label (window, _("Exposure compensation"), &w);
	if (gp_widget_changed (w)) {
		gp_widget_get_value (w, &wvalue);
		for (i = 0; i < sizeof(exp_comp)/sizeof(*exp_comp); i++){
			if (strncmp(wvalue, exp_comp[i], 4) == 0){
				dc210_set_exp_compensation(camera, i - 4);
				break;
			};
		};
	};

	gp_widget_get_child_by_label (window, _("Port speed"), &w);
	if (gp_widget_changed (w)) {
		gp_widget_get_value (w, &wvalue);
		dc210_set_speed(camera, atoi(wvalue));
	};

	gp_widget_get_child_by_label (window, _("Flash"), &w);
	gp_widget_get_child_by_label (window, _("Red eye flash"), &w2);
	if (gp_widget_changed (w) || gp_widget_changed(w2)) {
		gp_widget_get_value (w, &wvalue);
		gp_widget_get_value (w2, &w2value);
		switch(wvalue[0]){
		case 'A':
			dc210_set_flash(camera, DC210_FLASH_AUTO, 
					w2value[1] == 'n' ? 1 : 0);
			break;
		case 'F':
			dc210_set_flash(camera, DC210_FLASH_FORCE, 
					w2value[1] == 'n' ? 1 : 0);
			break;
		case 'N':
			dc210_set_flash(camera, DC210_FLASH_NONE, 0);
			gp_widget_set_value(w2, _("Off"));
			break;
		};
	};

	return GP_OK;
}

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context) {


	if (type && type != GP_OPERATION_CAPTURE_IMAGE){
		DC210_DEBUG("Unsupported action 0x%.2X\n", type);
		return (GP_ERROR_NOT_SUPPORTED);
	};

	if (dc210_capture(camera, path, context) == GP_ERROR) 
		return GP_ERROR;

	if (gp_filesystem_append (camera->fs, path->folder, path->name, context) == GP_ERROR)
		return GP_ERROR;;

        return (GP_OK);

}

static int camera_summary (Camera *camera, CameraText *summary,
			   GPContext *context) 
{
    static char summary_string[2048] = "";
    char buff[1024];
    dc210_status status;

    if (GP_OK == dc210_get_status (camera, &status))
    {
        strcpy(summary_string,"Kodak DC210\n");
        
        snprintf(buff,1024,"Pictures in camera: %d\n",
		 status.numPicturesInCamera);
        strcat(summary_string,buff);

        snprintf(buff,1024,"There is space for another\n   %d low compressed\n   %d medium compressed or\n   %d high compressed pictures\n",
		 status.remainingLow, status.remainingMedium, status.remainingHigh);
        strcat(summary_string,buff);

        snprintf(buff,1024,"Total pictures taken: %d\n",
		 status.totalPicturesTaken);
        strcat(summary_string,buff);

        snprintf(buff,1024,"Total flashes fired: %d\n",
		 status.totalFlashesFired);
        strcat(summary_string,buff);

        snprintf(buff,1024,"Firmware: %d.%d\n",status.firmwareMajor,status.firmwareMinor);
        strcat(summary_string,buff);

	switch (status.file_type){
	case DC210_FILE_TYPE_JPEG: 
		snprintf(buff, 1024, "Filetype: JPEG ("); break;
	case DC210_FILE_TYPE_FPX: 
		snprintf(buff, 1024, "Filetype: FlashPix ("); break;
	};
	strcat(summary_string, buff);
        
	switch (status.compression_type){
	case DC210_LOW_COMPRESSION:    
		snprintf(buff, 1024, "low compression, "); break;
	case DC210_MEDIUM_COMPRESSION: 
		snprintf(buff, 1024, "medium compression, "); break;
	case DC210_HIGH_COMPRESSION:   
		snprintf(buff, 1024, "high compression, "); break;
	default:                       
		snprintf(buff, 1024, "unknown compression %d, ", status.compression_type); break;
	};
	strcat(summary_string, buff);
        
	switch (status.resolution){
	case DC210_FILE_640: 
		snprintf(buff, 1024, "640x480 pixel)\n"); break;
	case DC210_FILE_1152: 
		snprintf(buff, 1024, "1152x864 pixel)\n"); break;
	default: 
		snprintf(buff, 1024, "unknown resolution %d)\n", status.resolution); break;
	};
	strcat(summary_string, buff);
        
	if (status.fast_preview)
		snprintf(buff, 1024, "Fast preview is on\n");
	else
		snprintf(buff, 1024, "Fast preview is off\n");
	strcat(summary_string, buff);

	switch (status.battery){
	case 0: snprintf(buff,1024,"Battery charge is good\n"); break;
	case 1: snprintf(buff,1024,"Battery charge is low\n"); break;
	case 2: snprintf(buff,1024,"Battery is not charged\n"); break;
	};
        strcat(summary_string,buff);
        
	if (status.acstatus)
		snprintf(buff,1024,"AC adapter is connected\n");
	else
		snprintf(buff,1024,"AC adapter is not connected\n");
        strcat(summary_string,buff);
        
        strftime(buff,1024,"Time: %a, %d %b %Y %T\n",
		 localtime((time_t *)&status.time));
        strcat(summary_string,buff);
        
	switch (status.zoom){
	case DC210_ZOOM_58: 
		snprintf(buff, 1024, "Zoom: 58 mm\n"); break;
	case DC210_ZOOM_51: 
		snprintf(buff, 1024, "Zoom: 51 mm\n"); break;
	case DC210_ZOOM_41: 
		snprintf(buff, 1024, "Zoom: 41 mm\n"); break;
	case DC210_ZOOM_34: 
		snprintf(buff, 1024, "Zoom: 34 mm\n"); break;
	case DC210_ZOOM_29: 
		snprintf(buff, 1024, "Zoom: 29 mm\n"); break;
	case DC210_ZOOM_MACRO: 
		snprintf(buff, 1024, "Zoom: macro\n"); break;
	default: 
		snprintf(buff, 1024, "Unknown zoom mode %d\n", 
			 status.zoom); break;
	};
        strcat(summary_string,buff);
        
	if (status.exp_compensation > -5 && status.exp_compensation < 4)
		snprintf(buff, 1024, "Exposure compensation: %s\n", exp_comp[status.exp_compensation + 4]);
	else
		snprintf(buff, 1024, "Exposure compensation: %d\n", status.exp_compensation);
        strcat(summary_string,buff);

	switch (status.flash){
	case DC210_FLASH_AUTO: 
		snprintf(buff, 1024, "Flash mode: auto, "); break;
	case DC210_FLASH_FORCE: 
		snprintf(buff, 1024, "Flash mode: force, "); break;
	case DC210_FLASH_NONE: 
		snprintf(buff, 1024, "Flash mode: off\n"); break;
	default: 
		snprintf(buff, 1024, "Unknown flash mode %d, ", 
			 status.flash); break;
	};
        strcat(summary_string,buff);

	if (status.flash != DC210_FLASH_NONE){
		if (status.preflash)
			snprintf(buff,1024,"red eye flash on\n");
		else
			snprintf(buff,1024,"red eye flash off\n");
		strcat(summary_string,buff);
	};

	if (status.card_space == -1)
		snprintf(buff, 1024, "No card in camera.\n");
	else
		snprintf(buff,1024,"Card name: %s\nFree space on card: %d kilobytes\n",
			 status.album_name,
			 status.card_space);
	
        strcat(summary_string,buff);

    }
    else{
	    DC210_DEBUG("Couldn't get summary for camera\n");
    };
    
    strcpy(summary->text, summary_string);
    
    return (GP_OK);
}


static int camera_manual (Camera *camera, CameraText *manual,
			  GPContext *context) 
{
	strcpy (manual->text, _("This library has been tested with a Kodak DC 215 Zoom camera. "
				"It might work also with DC 200 and DC 210 cameras. "
				"If you happen to have such a camera, please send a "
				"message to koltan@gmx.de to let me know, if you have any "
				"troubles with this driver library or if everything is okay."));

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context) 
{
	strcpy(about->text, 
		_("Camera Library for the Kodak DC215 Zoom camera.\n"
		  "Michael Koltan <koltan@gmx.de>\n"));

	return (GP_OK);
}

int camera_init (Camera *camera, GPContext *context) {

        /* First, set up all the function pointers */
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary 	= camera_summary;
        camera->functions->manual       = camera_manual;
        camera->functions->about        = camera_about;

	gp_filesystem_set_info_funcs (camera->fs, get_info_func,
                                        NULL, camera);
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);

	if (dc210_set_speed (camera, 115200) == GP_ERROR) {
		DC210_DEBUG("Error setting camera speed\n");
		return GP_ERROR;
	};


        return (GP_OK);
}


/****************************************************************************/







