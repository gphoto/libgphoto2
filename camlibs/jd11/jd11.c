/*
 * Jenoptik JD11 Camera Driver
 * Copyright 1999-2001 Marcus Meissner <marcus@jet.franken.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "serial.h"

int camera_id (CameraText *id) 
{
	strcpy(id->text, "JD11");
	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a,0,sizeof(a));

	a.status		= GP_DRIVER_STATUS_PRODUCTION;
	a.port			= GP_PORT_SERIAL;
	a.speed[0]		= 115200;
	a.speed[1]		= 0;
	a.operations		= GP_OPERATION_CONFIG;
	a.file_operations	= GP_FILE_OPERATION_PREVIEW;
	a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;

	strcpy(a.model, "Jenoptik:JD11");
	gp_abilities_list_append(list, a);

	/* http://www.digitalkamera.de/Kameras/PrakticaQD500-de.htm (german) */
	strcpy(a.model, "Praktica:QD500");
	gp_abilities_list_append(list, a);

	/* Reported to be just a rebranded version by Russ Burdick
	 * <grub@extrapolation.net> */
	/* from Gallantcom, http://www.gallantcom.com/prodfaq-probe99.htm */
	strcpy(a.model, "Quark:Probe 99");
	gp_abilities_list_append(list, a);

	/* 'Argus DC-100' looks the same, see:
	 * http://www.aaadigitalcameras.com/argus_digital_vga_compact_camera.html
	 * but not reported yet.
	 */
	strcpy(a.model, "Argus:DC-100");
	gp_abilities_list_append(list, a);
	/* 'Argus DC-2000' is the same (or uses the same software), check
	 * http://www.arguscamera.com/tech_supp/camoper.htm
	 */
	strcpy(a.model, "Argus:DC-2000");
	gp_abilities_list_append(list, a);

	/* http://www.digitaldreamco.com/shop/digital2000.html */
	strcpy(a.model, "Digitaldream:DIGITAL 2000");
	gp_abilities_list_append(list, a);

	/* The I/O Magic MagicImage 420 has a black cover, but otherwise
	 * appears to be the same. (Not 100% sure.)
	 * http://www.iomagic.com/support/digitalcameras/magicimage420/magicimage420main.htm
	 */
	strcpy(a.model, "IOMagic:MagicImage 420");
	gp_abilities_list_append(list, a);
	return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context) 
{
	Camera *camera = (Camera*)data;

	if (strcmp(folder,"/"))
		return GP_ERROR_BAD_PARAMETERS;

	return jd11_index_reader(camera->port, fs, context);
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	int image_no, result;

	image_no = gp_filesystem_number(fs, folder, filename, context);

	if(image_no < 0)
		return image_no;

	gp_file_set_mime_type (file, GP_MIME_PNM);
	switch (type) {
	case GP_FILE_TYPE_RAW:
		result = jd11_get_image_full(camera,file,image_no,1,context);
		break;
	case GP_FILE_TYPE_NORMAL:
		result = jd11_get_image_full(camera,file,image_no,0,context);
		break;
	case GP_FILE_TYPE_PREVIEW:
		/* We should never get here, we store the thumbs in the fs... */
		return (GP_ERROR_NOT_SUPPORTED);
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
	if (result < 0)
		return result;
	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data,
		 GPContext *context)
{
    Camera *camera = data;
    if (strcmp (folder, "/"))
	return (GP_ERROR_DIRECTORY_NOT_FOUND);
    return jd11_erase_all(camera->port);
}

static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) 
{
	strcpy(manual->text, 
	_(
	"The JD11 camera works rather well with this driver.\n"
	"An RS232 interface @ 115 kbit is required for image transfer.\n"
	"The driver allows you to get\n\n"
	"   - thumbnails (64x48 PGM format)\n"
	"   - full images (640x480 PPM format)\n"
	)); 

	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context) 
{
	strcpy (about->text, 
		_("JD11\n"
		"Marcus Meissner <marcus@jet.franken.de>\n"
		"Driver for the Jenoptik JD11 camera.\n"
		"Protocol reverse engineered using WINE and IDA."));

	return (GP_OK);
}
static int camera_config_get (Camera *camera, CameraWidget **window,
			      GPContext *context) 
{
	CameraWidget *widget,*section;
	float value_float,red,green,blue;
	int ret;
	gp_widget_new (GP_WIDGET_WINDOW, _("JD11 Configuration"), window);
	gp_widget_set_name (*window, "config");

	gp_widget_new (GP_WIDGET_SECTION, _("Other Settings"), &section);
	gp_widget_set_name (section, "othersettings");
	gp_widget_append (*window, section);

       /* Bulb Exposure Time */
	gp_widget_new (GP_WIDGET_RANGE, _("Bulb Exposure Time"), &widget);
	gp_widget_set_name (widget, "exposuretime");
	gp_widget_append (section, widget);
	gp_widget_set_range (widget, 1, 9, 1);
	/* this is a write only capability */
	value_float = 1;
	gp_widget_set_value (widget, &value_float);
	gp_widget_changed(widget);

	gp_widget_new (GP_WIDGET_SECTION, _("Color Settings"), &section);
	gp_widget_append (*window, section);
	gp_widget_set_name (section, "colorsettings");

	ret = jd11_get_rgb(camera->port,&red,&green,&blue);
	if (ret < GP_OK)
	    return ret;
	gp_widget_new (GP_WIDGET_RANGE, _("Red"), &widget);
	gp_widget_append (section, widget);
	gp_widget_set_name (widget, "red");
	gp_widget_set_range (widget, 50, 150, 1);
	red*=100.0;
	gp_widget_set_value (widget, &red);
	gp_widget_changed(widget);

	gp_widget_new (GP_WIDGET_RANGE, _("Green"), &widget);
	gp_widget_set_name (widget, "green");
	gp_widget_append (section, widget);
	gp_widget_set_range (widget, 50, 150, 1);
	green*=100.0;
	gp_widget_set_value (widget, &green);
	gp_widget_changed(widget);

	gp_widget_new (GP_WIDGET_RANGE, _("Blue"), &widget);
	gp_widget_set_name (widget, "blue");
	gp_widget_append (section, widget);
	gp_widget_set_range (widget, 50, 150, 1);
	blue*=100.0;
	gp_widget_set_value (widget, &blue);
	gp_widget_changed(widget);

	return (GP_OK);
}

static int camera_config_set (Camera *camera, CameraWidget *window,
			      GPContext *context) 
{
	float f,red,green,blue;
	CameraWidget *widget,*section;
	int ret,changed = 0;
	/*
	 * Check if the widgets' values have changed. If yes, tell the camera.
	 */
	gp_widget_get_child_by_label (window, _("Other Settings"), &section);
	gp_widget_get_child_by_label (section,_("Bulb Exposure Time"), &widget);
	if (gp_widget_changed (widget)) {
		gp_widget_get_value (widget, &f);
		ret = jd11_set_bulb_exposure(camera->port,(int)f);
		if (ret < GP_OK)
		    return ret;
	}

	gp_widget_get_child_by_label (window, _("Color Settings"), &section);

	gp_widget_get_child_by_label (section,_("Red"), &widget);
	changed|=gp_widget_changed (widget);
	gp_widget_get_value (widget, &red);
	red/=100.0;

	gp_widget_get_child_by_label (section,_("Green"), &widget);
	changed|=gp_widget_changed (widget);
	gp_widget_get_value (widget, &green);
	green/=100.0;

	gp_widget_get_child_by_label (section,_("Blue"), &widget);
	changed|=gp_widget_changed (widget);
	gp_widget_get_value (widget, &blue);
	blue/=100.0;

	ret = GP_OK;
	if (changed)
	    ret =jd11_set_rgb(camera->port,red,green,blue);
	return ret;

}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.delete_all_func = delete_all_func,
};

int camera_init (Camera *camera, GPContext *context) 
{
        gp_port_settings settings;

        /* First, set up all the function pointers */
        camera->functions->manual	= camera_manual;
        camera->functions->about	= camera_about;
	camera->functions->get_config	= camera_config_get;
	camera->functions->set_config	= camera_config_set;

        /* Configure port */
        gp_port_set_timeout(camera->port, 1000);
	gp_port_get_settings(camera->port, &settings);
        settings.serial.speed	= 115200;
        settings.serial.bits	= 8;
        settings.serial.parity	= 0;
        settings.serial.stopbits= 1;
        gp_port_set_settings(camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_funcs(camera->fs, &fsfuncs, camera);
        /* test camera */
        return jd11_ping(camera->port);
}
