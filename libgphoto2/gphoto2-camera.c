/* gphoto2-camera.c
 *
 * Copyright (C) 2000 Scott Fritzinger
 *
 * Contributions:
 * 	2001: Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
#include "gphoto2-camera.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "gphoto2-result.h"
#include "gphoto2-library.h"
#include "gphoto2-port-core.h"
#include "gphoto2-port-log.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CHECK_NULL(r)              {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result)       {int r = (result); if (r < 0) return (r);}

#define CHECK_OPEN(c)              {int r = ((c)->port ? gp_port_open ((c)->port) : GP_OK); if (r < 0) {gp_camera_status ((c), ""); return (r);}}
#define CHECK_CLOSE(c)             {if ((c)->port) gp_port_close ((c)->port);}

#define CRS(c,res) {int r = (res); if (r < 0) {gp_camera_status ((c), ""); return (r);}}

#define CHECK_RESULT_OPEN_CLOSE(c,result) {int r; CHECK_OPEN (c); r = (result); if (r < 0) {CHECK_CLOSE (c); gp_camera_status ((c), ""); gp_camera_progress ((c), 0.0); return (r);}; CHECK_CLOSE (c);}

struct _CameraPrivateCore {

	/* Some information about the port */
	char port_name[1024];
	char port_path[1024];
	unsigned int speed;

	CameraStatusFunc   status_func;
	void              *status_data;

	CameraProgressFunc progress_func;
	void              *progress_data;

	CameraMessageFunc  message_func;
	void              *message_data;

	/* The abilities of this camera */
	CameraAbilities a;

	/* Library handle */
	void *lh;

	char error[2048];

	unsigned int ref_count;
};

/**
 * gp_camera_new:
 * @camera:
 *
 * Allocates the memory for a #Camera.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_new (Camera **camera)
{
	int result;

	CHECK_NULL (camera);

        *camera = malloc (sizeof (Camera));
	if (!*camera) 
		return (GP_ERROR_NO_MEMORY);
	memset (*camera, 0, sizeof (Camera));

        (*camera)->functions = malloc(sizeof(CameraFunctions));
	if (!(*camera)->functions) {
		gp_camera_free (*camera);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*camera)->functions, 0, sizeof (CameraFunctions));

	(*camera)->pc = malloc (sizeof (CameraPrivateCore));
	if (!(*camera)->pc) {
		gp_camera_free (*camera);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*camera)->pc, 0, sizeof (CameraPrivateCore));

        (*camera)->pc->ref_count = 1;

	/* Create the filesystem */
	result = gp_filesystem_new (&(*camera)->fs);
	if (result != GP_OK) {
		gp_camera_free (*camera);
		return (result);
	}

        return(GP_OK);
}

/**
 * gp_camera_set_abilities:
 * @camera: a #Camera
 * @abilities: the #CameraAbilities to be set
 *
 * Sets the camera abilities. You need to call this function before
 * calling #gp_camera_init unless you want gphoto2 to autodetect 
 * cameras and choose the first detected one. By setting the @abilities, you
 * tell gphoto2 what model the @camera is and what camera driver should 
 * be used for accessing the @camera. You can get @abilities by calling
 * #gp_abilities_list_get_abilities.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_abilities (Camera *camera, CameraAbilities abilities)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Setting abilibites ('%s')...",
		abilities.model);

	CHECK_NULL (camera);

	memcpy (&camera->pc->a, &abilities, sizeof (CameraAbilities));

	return (GP_OK);
}

/**
 * gp_camera_get_abilities:
 * @camera: a #Camera
 * @abilities:
 *
 * Retreives the @abilities of the @camera. 
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_get_abilities (Camera *camera, CameraAbilities *abilities)
{
	CHECK_NULL (camera && abilities);

	memcpy (abilities, &camera->pc->a, sizeof (CameraAbilities));

	return (GP_OK);
}

static int
gp_camera_unset_port (Camera *camera)
{
	CHECK_NULL (camera);

	if (camera->port) {
		CHECK_RESULT (gp_port_free (camera->port));
		camera->port = NULL;
	}

	return (GP_OK);
}

static int
gp_camera_set_port (Camera *camera, GPPortInfo *info)
{
	GPPortSettings settings;
	CHECK_NULL (camera);

	/*
	 * We need to create a new port because the port could have
	 * changed from a SERIAL to an USB one...
	 */
	CHECK_RESULT (gp_camera_unset_port (camera));
	CHECK_RESULT (gp_port_new (&camera->port, info->type));

	switch (camera->port->type) {
	case GP_PORT_SERIAL:
		CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
		strcpy (settings.serial.port, info->path);
		CHECK_RESULT (gp_port_settings_set (camera->port, settings));
		break;
	default:
		break;
	}

	strncpy (camera->pc->port_path, info->path,
		 sizeof (camera->pc->port_path));
	strncpy (camera->pc->port_name, info->name,
		 sizeof (camera->pc->port_name));

	return (GP_OK);
}

/**
 * gp_camera_set_port_path:
 * @camera: a #Camera
 * @port_path: a path
 *
 * Indicates which device should be used for accessing the @camera.
 * Alternatively, #gp_camera_set_port_name can be used.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_port_path (Camera *camera, const char *port_path)
{
	int x, count;
	GPPortInfo info;

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Setting port path to '%s'...",
		port_path);

	CHECK_NULL (camera && port_path);

	CHECK_RESULT (gp_camera_unset_port (camera));
	CHECK_RESULT (count = gp_port_core_count ());
	for (x = 0; x < count; x++)
		if ((gp_port_core_get_info (x, &info) == GP_OK) &&
		    (!strcmp (port_path, info.path)))
			return (gp_camera_set_port (camera, &info));

	gp_camera_set_error (camera, _("Could not find port '%s'"), port_path);
	return (GP_ERROR_UNKNOWN_PORT); 
}

/**
 * gp_camera_get_port_path:
 * @camera: a #Camera
 * @port_path:
 *
 * Retreives the path to the port the @camera is currently using.
 *
 * Return value: a gphoto2 error code.
 **/
int
gp_camera_get_port_path (Camera *camera, const char **port_path)
{
	CHECK_NULL (camera && port_path);

	*port_path = camera->pc->port_path;

	return (GP_OK);
}

/**
 * gp_camera_set_port_name:
 * @camera: a #Camera
 * @port_name: a name
 *
 * Indicates which device should be used for accessing the @camera.
 * Alternatively, #gp_camera_set_port_path can be used.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_port_name (Camera *camera, const char *port_name)
{
	int x, count;
	GPPortInfo info;

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Setting port name to '%s'",
		port_name);

	CHECK_NULL (camera && port_name);

	CHECK_RESULT (gp_camera_unset_port (camera));
	CHECK_RESULT (count = gp_port_core_count ());
	for (x = 0; x < count; x++)
		if ((gp_port_core_get_info (x, &info) == GP_OK) &&
		    (!strcmp (port_name, info.name)))
			return (gp_camera_set_port (camera, &info));

	return (GP_ERROR_UNKNOWN_PORT);
}

/**
 * gp_camera_get_port_name:
 * @camera: a #Camera
 * @port_name:
 *
 * Retreives the name of the port the @camera is currently using.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_get_port_name (Camera *camera, const char **port_name)
{
	CHECK_NULL (camera && port_name);

	*port_name = camera->pc->port_name;

	return (GP_OK);
}

/**
 * gp_camera_set_port_speed:
 * @camera: a #Camera
 * @speed: the speed
 *
 * Sets the speed. This function is typically used prior initialization 
 * using #gp_camera_init for debugging purposes. Normally, a camera driver
 * will try to figure out the current speed of the camera and set the speed
 * to the optimal one automatically. Note that this function only works with 
 * serial ports. In other words, you have to set the camera's port to a 
 * serial one (using #gp_camera_set_port_path or #gp_camera_set_port_name)
 * prior calling this function.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_port_speed (Camera *camera, int speed)
{
	GPPortSettings settings;

	CHECK_NULL (camera);

	if (!camera->port) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("You need to set "
			"a port prior trying to set the speed"));
		return (GP_ERROR_BAD_PARAMETERS);
	}

	if (camera->port->type != GP_PORT_SERIAL) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("You can specify "
			"a speed only with serial ports"));
		return (GP_ERROR_BAD_PARAMETERS);
	}

	CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
	settings.serial.speed = speed;
	CHECK_RESULT (gp_port_settings_set (camera->port, settings));
	camera->pc->speed = speed;

	return (GP_OK);
}

/** 
 * gp_camera_get_port_speed:
 * @camera: a #Camera
 *
 * Retreives the current speed.
 *
 * Return value: The current speed or a gphoto2 error code
 **/
int
gp_camera_get_port_speed (Camera *camera)
{
	CHECK_NULL (camera);

	return (camera->pc->speed);
}

/**
 * gp_camera_set_progress_func:
 * @camera: a #Camera
 * @func: a #CameraProgressFunc
 * @data:
 *
 * Sets the function that will be used to display the progress in case of
 * operations that can take some time (like file download). Typically, this
 * function will be called by the frontend after #gp_camera_new in order to
 * redirect progress information to the status bar.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_progress_func (Camera *camera, CameraProgressFunc func,
			     void *data)
{
	CHECK_NULL (camera);

	camera->pc->progress_func = func;
	camera->pc->progress_data = data;

	return (GP_OK);
}

/**
 * gp_camera_set_status_func:
 * @camera: a #Camera
 * @func: a #CameraStatusFunc
 * @data:
 *
 * Sets the function that will be used to display status messages. Status
 * messages are displayed before operations that can take some time, often
 * in combination with progress information. Typically, a frontend would 
 * call this function after #gp_camera_new in order to redirect status 
 * messages to the status bar.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_status_func (Camera *camera, CameraStatusFunc func,
			   void *data)
{
	CHECK_NULL (camera);

	camera->pc->status_func = func;
	camera->pc->status_data = data;

	return (GP_OK);
}

/**
 * gp_camera_set_message_func:
 * @camera: a #Camera
 * @func: a #CameraMessageFunc
 * @data:
 *
 * Sets the message function that will be used for displaying messages. 
 * This mechanism is used by camera drivers in order to display information
 * that cannot be passed through gphoto2 using standard mechanisms. As gphoto2
 * allows camera specific error codes, the message mechanism should never be
 * used. It is here for historical reasons.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_message_func (Camera *camera, CameraMessageFunc func,
			    void *data)
{
	CHECK_NULL (camera);

	camera->pc->message_func = func;
	camera->pc->message_data = data;

	return (GP_OK);
}

/**
 * gp_camera_status:
 * @camera: a #Camera
 * @format:
 * @...:
 *
 * Sets the status message. Typically, this function gets called by gphoto2
 * or a camera driver before operations that will take some time. For 
 * example, gphoto2 automatically sets a status message before
 * each file download.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_status (Camera *camera, const char *format, ...)
{
	char buffer[1024];
	va_list arg;

	CHECK_NULL (camera && format);

	va_start (arg, format);
#if HAVE_VSNPRINTF
	vsnprintf (buffer, sizeof (buffer), format, arg);
#else
	vsprintf (buffer, format, arg);
#endif
	va_end (arg);

	if (camera->pc->status_func)
		camera->pc->status_func (camera, buffer,
					 camera->pc->status_data);

	return (GP_OK);
}

/**
 * gp_camera_message:
 * @camera: a #Camera
 * @format:
 * @...:
 *
 * This function is here for historical reasons. Please do not use.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_message (Camera *camera, const char *format, ...)
{
	char buffer[2048];
	va_list arg;

	CHECK_NULL (camera && format);

	va_start (arg, format);
#if HAVE_VSNPRINTF
	vsnprintf (buffer, sizeof (buffer), format, arg);
#else
	vsprintf (buffer, format, arg);
#endif
	va_end (arg);

	if (camera->pc->message_func)
		camera->pc->message_func (camera, buffer,
					  camera->pc->message_data);

	return (GP_OK);
}

/**
 * gp_camera_progress:
 * @camera: a #Camera:
 * @percentage: a percentage
 *
 * Informs frontends of the progress of an operation. Typically, this
 * function is used by camera drivers during operations that take some
 * time but that can be monitored.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_progress (Camera *camera, float percentage)
{
	CHECK_NULL (camera);

	if ((percentage < 0.) || (percentage > 1.)) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", 
			"Wrong percentage: %f", percentage);
		return (GP_ERROR_BAD_PARAMETERS);
	}

	if (camera->pc->progress_func)
		camera->pc->progress_func (camera, percentage,
					   camera->pc->progress_data);

	return (GP_OK);
}

/**
 * gp_camera_ref:
 * @camera: a #Camera
 *
 * Increments the reference count of a @camera.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_ref (Camera *camera)
{
	CHECK_NULL (camera);

	camera->pc->ref_count += 1;

	return (GP_OK);
}

/**
 * gp_camera_unref:
 * @camera: a #Camera
 *
 * Decrements the reference count of a @camera. If the reference count 
 * reaches %0, the @camera will be freed automatically.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_unref (Camera *camera)
{
	CHECK_NULL (camera);

	if (!camera->pc->ref_count) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", "gp_camera_unref on "
			"a camera with ref_count == 0 should not happen "
			"at all");
		return (GP_ERROR);
	}

	camera->pc->ref_count -= 1;

	if (!camera->pc->ref_count)
		gp_camera_free (camera);

	return (GP_OK);
}

/**
 * gp_camera_free:
 * @camera: a #Camera
 *
 * Frees the camera. This function should never be used. Please use
 * #gp_camera_unref instead.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_free (Camera *camera)
{
	CHECK_NULL (camera);

	/* We don't care if anything goes wrong */
	if (camera->port) {
		gp_port_open (camera->port);
		if (camera->functions->exit)
	        	camera->functions->exit (camera);
		gp_port_close (camera->port);
		gp_port_free (camera->port);
		camera->port = NULL;
	}

	if (camera->pc) {
		if (camera->pc->lh) {
			GP_SYSTEM_DLCLOSE (camera->pc->lh);
			camera->pc->lh = NULL;
		}
		free (camera->pc);
	}

	if (camera->fs) {
		gp_filesystem_free (camera->fs);
		camera->fs = NULL;
	}

        if (camera->functions) {
                free (camera->functions);
		camera->functions = NULL;
	}
 
	free (camera);

	return (GP_OK);
}

/**
 * gp_camera_init:
 * @camera: a #camera
 *
 * Initiates a connection to the camera. Before calling this function, the
 * @camera should be set up using #gp_camera_set_port_path or
 * #gp_camera_set_port_name and #gp_camera_set_abilities. If that has been
 * omitted, gphoto2 tries to autodetect any cameras and chooses the first one
 * if any cameras are found.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_init (Camera *camera)
{
        CameraList list;
	CameraAbilities a;
	const char *model, *port;
	CameraLibraryInitFunc init_func;

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Initializing camera...");

	CHECK_NULL (camera);

	if (!strcmp (camera->pc->a.model, ""))
		gp_camera_status (camera, _("Initializing camera..."));
	else
		gp_camera_status (camera, _("Initializing '%s'..."),
				  camera->pc->a.model);

	/*
	 * If the model hasn't been indicated, try to
	 * figure it out (USB only). Beware of "Directory Browse".
	 */
	if (strcasecmp (camera->pc->a.model, "Directory Browse") &&
	    !strcmp ("", camera->pc->a.model)) {
		CameraAbilitiesList *al;
		int m;

		gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Neither "
			"port nor model set. Trying auto-detection...");

		/* Call auto-detect and choose the first camera */
		gp_abilities_list_new (&al);
		gp_abilities_list_load (al);
		gp_abilities_list_detect (al, &list);
		if (!gp_list_count (&list)) {
			gp_abilities_list_free (al);
			gp_camera_set_error (camera, _("Could not detect "
					     "any camera"));
			return (GP_ERROR_MODEL_NOT_FOUND);
		}

		gp_list_get_name  (&list, 0, &model);
		m = gp_abilities_list_lookup_model (al, model);
		gp_abilities_list_get_abilities (al, m, &a);
		gp_abilities_list_free (al);
		CRS (camera, gp_camera_set_abilities (camera, a));
		CRS (camera, gp_list_get_value (&list, 0, &port));
		CRS (camera, gp_camera_set_port_path (camera, port));
	}

	if (strcasecmp (camera->pc->a.model, "Directory Browse")) {

		/* If we don't have a port at this point, return error */
		if (!camera->port) {
			gp_camera_status (camera, "");
			return (GP_ERROR_UNKNOWN_PORT);
		}

		/* In case of USB, find the device */
		switch (camera->port->type) {
		case GP_PORT_USB:
			CRS (camera, gp_port_usb_find_device (camera->port,
					camera->pc->a.usb_vendor,
					camera->pc->a.usb_product));
			break;
		default:
			break;
		}
	}

	/* Load the library. */
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Loading '%s'...",
		camera->pc->a.library);
	camera->pc->lh = GP_SYSTEM_DLOPEN (camera->pc->a.library);
	if (!camera->pc->lh) {
		gp_camera_status (camera, "");
		return (GP_ERROR_LIBRARY);
	}

	/* Initialize the camera */
	init_func = GP_SYSTEM_DLSYM (camera->pc->lh, "camera_init");
	if (!init_func) {
		gp_camera_status (camera, "");
		return (GP_ERROR_LIBRARY);
	}
	CHECK_RESULT_OPEN_CLOSE (camera, init_func (camera));

	gp_camera_status (camera, "");
	return (GP_OK);
}

/**
 * gp_camera_get_config:
 * @camera: a #Camera
 * @window:
 *
 * Retreives a configuration @window for the @camera. This window can be
 * used for construction of a configuration dialog.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_get_config (Camera *camera, CameraWidget **window)
{
	CHECK_NULL (camera);

	if (!camera->functions->get_config) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera does "
			"not offer any configuration options"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	gp_camera_status (camera, _("Getting configuration..."));
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->get_config (camera,
								   window));
	gp_camera_status (camera, "");

	return (GP_OK);
}

/**
 * gp_camera_set_config:
 * @camera: a #Camera
 * @window: a #CameraWidget
 *
 * Sets the configuration. Typically, a @window is retreived using
 * #gp_camera_get_config and passed to this function in order to adjust
 * the settings on the camera.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_config (Camera *camera, CameraWidget *window)
{
	CHECK_NULL (camera && window);

	if (!camera->functions->set_config) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera does "
			"not support setting configuration options"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	gp_camera_status (camera, _("Setting configuration..."));
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->set_config (camera,
								   window));
	gp_camera_status (camera, "");

	return (GP_OK);
}

/**
 * gp_camera_get_summary:
 * @camera: a #Camera
 * @summary: a #CameraText
 *
 * Retreives a camera @summary. This summary typically contains information
 * like manufacturer, pictures taken, or generally information that is
 * not configurable.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_get_summary (Camera *camera, CameraText *summary)
{
	CHECK_NULL (camera && summary);

	if (!camera->functions->summary) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera does "
			"not support summaries"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	gp_camera_status (camera, _("Getting summary..."));
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->summary (camera,
								summary));
	gp_camera_status (camera, "");

	return (GP_OK);
}

/**
 * gp_camera_get_manual:
 * @camera: a #Camera
 * @manual: a #CameraText
 *
 * Retreives the @manual for given @camera. This manual typically contains
 * information about using the camera.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_get_manual (Camera *camera, CameraText *manual)
{
	CHECK_NULL (camera && manual);

	if (!camera->functions->manual) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera "
			"does not offer a manual"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->manual (camera,
								    manual));

	return (GP_OK);
}

/**
 * gp_camera_get_about:
 * @camera: a #Camera
 * @about: a #CameraText
 *
 * Retreives information about the camera driver. Typically, this information
 * contains name and address of the author, acknowledgements, etc.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_get_about (Camera *camera, CameraText *about)
{
	CHECK_NULL (camera && about);

	if (!camera->functions->about) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera does "
			"not provide information about the driver"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->about (camera,
								   about));

	return (GP_OK);
}

/**
 * gp_camera_capture:
 * @camera: a #Camera
 * @type: a #CameraCaptureType
 * @path: a #CameraFilePath
 *
 * Captures an image, movie, or sound clip depending on the given @type.
 * The resulting file will be stored on the camera. The location gets stored
 * in @path. The file can then be downloaded using #gp_camera_file_get.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_capture (Camera *camera, CameraCaptureType type,
		   CameraFilePath *path)
{
	CHECK_NULL (camera && path);

	if (!camera->functions->capture) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera can "
			"not capture"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	gp_camera_status (camera, "Capturing image...");
	CHECK_RESULT_OPEN_CLOSE (camera,
		camera->functions->capture (camera, type, path));
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);

	return (GP_OK);
}

/**
 * gp_camera_capture_preview:
 * @camera: a #Camera
 * @file: a #CameraFile
 *
 * Captures a preview that won't be stored on the camera but returned in 
 * supplied @file. For example, you could use #gp_capture_preview for 
 * taking some sample pictures before calling #gp_capture.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_capture_preview (Camera *camera, CameraFile *file)
{
	CHECK_NULL (camera && file);

	if (!camera->functions->capture_preview) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera can "
			"not capture previews"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	gp_camera_status (camera, "Capturing preview...");
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->capture_preview (
								camera, file));
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);

	return (GP_OK);
}

/**
 * gp_camera_folder_list_files:
 * @camera: a #Camera
 * @folder: a folder
 * @list: a #CameraList
 *
 * Lists the files in supplied @folder.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_folder_list_files (Camera *camera, const char *folder, 
			     CameraList *list)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Listing files in '%s'...",
		folder);

	CHECK_NULL (camera && folder && list);
	CHECK_RESULT (gp_list_reset (list));

	gp_camera_status (camera, _("Listing files in '%s'..."), folder);
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_list_files (camera->fs,
							folder, list));
	gp_camera_status (camera, "");

	CHECK_RESULT (gp_list_sort (list));
        return (GP_OK);
}

/**
 * gp_camera_folder_list_folders:
 * @camera: a #Camera
 * @folder: a folder
 * @list: a #CameraList
 *
 * Lists the folders in supplied @folder.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_folder_list_folders (Camera *camera, const char* folder, 
			       CameraList *list)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Listing folders in '%s'...",
		folder);

	CHECK_NULL (camera && folder && list);
	CHECK_RESULT (gp_list_reset (list));

	gp_camera_status (camera, _("Listing folders in '%s'..."), folder);
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_list_folders (
						camera->fs, folder, list));
	gp_camera_status (camera, "");

	CHECK_RESULT (gp_list_sort (list));
        return (GP_OK);
}

/**
 * gp_camera_folder_delete_all:
 * @camera: a #Camera
 * @folder: a folder
 *
 * Deletes all files in a given @folder.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_folder_delete_all (Camera *camera, const char *folder)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Deleting all files in "
		"'%s'...", folder);

	CHECK_NULL (camera && folder);

	gp_camera_status (camera, _("Deleting all files in '%s'..."), folder);
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_all (camera->fs,
								folder));
	gp_camera_status (camera, "");

	return (GP_OK);
}

/**
 * gp_camera_folder_put_file:
 * @camera: a #Camera
 * @folder: a folder
 * @file: a #CameraFile
 *
 * Uploads a file into given @folder.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_folder_put_file (Camera *camera, const char *folder, CameraFile *file)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Uploading file into '%s'...",
		folder);

	CHECK_NULL (camera && folder && file);

	gp_camera_status (camera, _("Uploading file into '%s'..."), folder);
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_put_file (camera->fs,
							folder, file));
	gp_camera_status (camera, "");

	return (GP_OK);
}

/**
 * gp_camera_folder_get_config:
 * @camera: a #Camera
 * @folder: a folder
 * @window: a #CameraWidget
 *
 * Retreives the configuration @window of a @folder. This function is similar
 * to #gp_camera_get_config. The only difference is that it operates on a
 * folder.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_folder_get_config (Camera *camera, const char *folder, 
			     CameraWidget **window)
{
	CHECK_NULL (camera && folder && window);

	if (!camera->functions->folder_get_config) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("The camera does "
			"not offer any configuration options for folders"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->folder_get_config (
						camera, folder, window));

	return (GP_OK);
}

/**
 * gp_camera_folder_set_config:
 * @camera: a #Camera
 * @folder: a folder
 * @window: a #CameraWidget
 *
 * Sets the folder configuration. See #gp_camera_set_config and
 * #gp_camera_folder_get_config for details.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_folder_set_config (Camera *camera, const char *folder, 
	                     CameraWidget *window)
{
	CHECK_NULL (camera && folder && window);

	if (!camera->functions->folder_set_config) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("The camera does "
			"not support setting configuration options on "
			"folders"));
                return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->folder_set_config (
						camera, folder, window));

	return (GP_OK);
}

/**
 * gp_camera_file_get_info:
 * @camera: a #Camera
 * @folder: a folder
 * @file: the name of the file
 * @info:
 *
 * Retreives information about a @file.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_file_get_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info)
{
	int result = GP_OK;
	const char *mime_type;
	const char *data;
	long int size;
	CameraFile *cfile;

	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Getting file info for '%s' "
		"in '%s'...", file, folder);

	CHECK_NULL (camera && folder && file && info);

	memset (info, 0, sizeof (CameraFileInfo));

	/* Check first if the camera driver supports the filesystem */
	CHECK_OPEN (camera);
	gp_camera_status (camera, _("Getting info for '%s' in folder '%s'..."),
			  file, folder);
	result = gp_filesystem_get_info (camera->fs, folder, file, info);
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);
	CHECK_CLOSE (camera);
	if (result != GP_ERROR_NOT_SUPPORTED) {
		gp_camera_status (camera, "");
		return (result);
	}

	/*
	 * The CameraFilesystem doesn't support file info. We simply get
	 * the preview and the file and look for ourselves...
	 */

	/* It takes too long to get the file */
	info->file.fields = GP_FILE_INFO_NONE;

	/* Get the preview */
	info->preview.fields = GP_FILE_INFO_NONE;
	CRS (camera, gp_file_new (&cfile));
	if (gp_camera_file_get (camera, folder, file,
				GP_FILE_TYPE_PREVIEW, cfile)== GP_OK) {
		info->preview.fields |= GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
		gp_file_get_data_and_size (cfile, &data, &size);
		info->preview.size = size;
		gp_file_get_mime_type (cfile, &mime_type);
		strncpy (info->preview.type, mime_type,
			 sizeof (info->preview.type));
	}
	gp_file_unref (cfile);
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);

	/* We don't trust the camera libraries */
	info->file.fields |= GP_FILE_INFO_NAME;
	strncpy (info->file.name, file, sizeof (info->file.name));
	info->preview.fields &= ~GP_FILE_INFO_NAME;

	return (GP_OK);
}

/**
 * gp_camera_file_set_info:
 * @camera: a #Camera
 * @folder: a folder
 * @file: the name of a file
 * @info: the #CameraFileInfo
 *
 * Sets some file properties like name or permissions.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_file_set_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info)
{
	CHECK_NULL (camera && info && folder && file);

	CHECK_RESULT_OPEN_CLOSE (camera,
		gp_filesystem_set_info (camera->fs, folder, file, info));

	return (GP_OK);
}

/**
 * gp_camera_file_get:
 * @camera: a #Camera
 * @folder: a folder
 * @file: the name of a file
 * @type: the #CameraFileType
 * @camera_file: a #CameraFile
 *
 * Retrieves a @file from the @camera.
 *
 * Return value: a gphoto2 error code
 **/
int 
gp_camera_file_get (Camera *camera, const char *folder, const char *file,
		    CameraFileType type, CameraFile *camera_file)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Getting file '%s' in "
		"folder '%s'...", file, folder);

	CHECK_NULL (camera && folder && file && camera_file);

	/* Did we get reasonable foldername/filename? */
	if (strlen (folder) == 0)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	if (strlen (file) == 0)
		return (GP_ERROR_FILE_NOT_FOUND);
  
	gp_camera_status (camera, _("Getting '%s' from folder '%s'..."),
			  file, folder);
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_get_file (camera->fs,
				folder, file, type, camera_file));
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);

	/* We don't trust the camera libraries */
	CHECK_RESULT (gp_file_set_type (camera_file, type));
	CHECK_RESULT (gp_file_set_name (camera_file, file));

	return (GP_OK);
}

/**
 * gp_camera_file_get_config:
 * @camera: a #Camera
 * @folder: a folder
 * @file: the name of a file
 * @window:
 *
 * Gets a configuration window for a given @file. See #gp_camera_get_config
 * and #gp_camera_set_config.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_file_get_config (Camera *camera, const char *folder, 
			   const char *file, CameraWidget **window)
{
	CHECK_NULL (camera && folder && file && window);

	if (!camera->functions->file_get_config) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera does "
			"not offer configuration options for files"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->file_get_config (
						camera, folder, file, window));

	return (GP_OK);
}

/**
 * gp_camera_file_set_config:
 * @camera: a #Camera
 * @folder: a folder
 * @file: the name of a file
 * @window: a #CameraWidget
 *
 * Sets the configuration of a file. See #gp_camera_set_config.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_file_set_config (Camera *camera, const char *folder, 
			   const char *file, CameraWidget *window)
{
	CHECK_NULL (camera && window && folder && file);

	if (!camera->functions->file_set_config) {
		gp_log (GP_LOG_ERROR, "gphoto2-camera", _("This camera does "
			"not support setting configuration options on files"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->file_set_config (
					camera, folder, file, window));

	return (GP_OK);
}

/**
 * gp_camera_file_delete:
 * @camera: a #Camera
 * @folder: a folder
 * @file: the name of a file
 *
 * Deletes the @file from a @folder.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_file_delete (Camera *camera, const char *folder, const char *file)
{
	gp_log (GP_LOG_DEBUG, "gphoto2-camera", "Deleting file '%s' in "
		"folder '%s'...", file, folder);

	CHECK_NULL (camera && folder && file);

	gp_camera_status (camera, _("Deleting '%s' from folder '%s'..."),
			  file, folder);
	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_file (
						camera->fs, folder, file));
	gp_camera_status (camera, "");

	return (GP_OK);
}

/**
 * gp_camera_set_error:
 * @camera: a #Camera
 * @format:
 * ...:
 *
 * Saves an error message that can later be retreived by #gp_camera_get_error.
 * Normally, you would call this function in a camera driver prior 
 * return in some error code. You don't want to call this function when 
 * simply passing errors upwards. In frontends, you call this function with
 * %NULL as error message
 * every time before calling another camera related function of which you 
 * will later check the return value. This is to make sure that no previous
 * error will be reported later on.
 *
 * Return value: a gphoto2 error code
 **/
int
gp_camera_set_error (Camera *camera, const char *format, ...)
{
	va_list args;

	CHECK_NULL (camera);

	if (format) {

		/* Remember the error message and log it */
		va_start (args, format);
		vsnprintf (camera->pc->error, sizeof (camera->pc->error),
			   format, args);
		gp_logv (GP_LOG_ERROR, "gphoto2-camera", format, args);
		va_end (args);

	} else {

		/* Clear the error buffer(s) */
		camera->pc->error[0] = '\0';
		if (camera->port)
			gp_port_set_error (camera->port, NULL);
	}

	return (GP_OK);
}

/**
 * gp_camera_get_error:
 * @camera: a #Camera
 *
 * Retreives an error message. Typically, this function is called by 
 * frontends in order to provide the user with additional information 
 * why an operation failed.
 *
 * Return value: an error description
 **/
const char *
gp_camera_get_error (Camera *camera)
{
	if (strlen (camera->pc->error))
		return (camera->pc->error);

	if (camera->port)
		return (gp_port_get_error (camera->port));

	return (N_("No error description available"));
}
