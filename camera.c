/* camera.c
 *
 * Copyright (C) 2000 Scott Fritzinger
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
#include "gphoto2-core.h"
#include "gphoto2-library.h"
#include "gphoto2-debug.h"
#include "gphoto2-port-core.h"

#include <config.h>
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

static int glob_session_camera = 0;

#define CHECK_NULL(r)              {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result)       {int r = (result); if (r < 0) return (r);}

#define CHECK_OPEN(c)              {int r = ((c)->port ? gp_port_open ((c)->port) : GP_OK); if (r < 0) {gp_camera_status ((c), ""); return (r);}}
#define CHECK_CLOSE(c)             {if ((c)->port) gp_port_close ((c)->port);}

#define CRS(c,res) {int r = (res); if (r < 0) {gp_camera_status ((c), ""); return (r);}}

#define CHECK_RESULT_OPEN_CLOSE(c,result) {int r; CHECK_OPEN (c); r = (result); if (r < 0) {CHECK_CLOSE (c); gp_camera_status ((c), ""); gp_camera_progress ((c), 0.0); return (r);}; CHECK_CLOSE (c);}

#define GP_DEBUG(m) {gp_debug_printf (GP_DEBUG_LOW, "core", (m));}

int
gp_camera_new (Camera **camera)
{
	int result;

	CHECK_NULL (camera);

	/*
	 * Be nice to frontend-writers and call some core function to make
	 * sure libgphoto2 is initialized...
	 */
	CHECK_RESULT (gp_camera_count ());

        *camera = malloc (sizeof (Camera));
	if (!*camera) 
		return (GP_ERROR_NO_MEMORY);
	memset (*camera, 0, sizeof (Camera));

        /* Initialize the members */
        (*camera)->port_info = malloc (sizeof(CameraPortInfo));
	if (!(*camera)->port_info) {
		free (*camera);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*camera)->port_info, 0, sizeof (CameraPortInfo));
	(*camera)->port_info->type = GP_PORT_NONE;

        (*camera)->abilities = malloc(sizeof(CameraAbilities));
	if (!(*camera)->abilities) {
		free ((*camera)->port);
		free (*camera);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*camera)->abilities, 0, sizeof (CameraAbilities));

        (*camera)->functions = malloc(sizeof(CameraFunctions));
	if (!(*camera)->functions) {
		free ((*camera)->port);
		free ((*camera)->abilities);
		free (*camera);
		return (GP_ERROR_NO_MEMORY);
	}
	memset ((*camera)->functions, 0, sizeof (CameraFunctions));

	(*camera)->session    = glob_session_camera++;
        (*camera)->ref_count  = 1;

	/* Create the filesystem */
	result = gp_filesystem_new (&(*camera)->fs);
	if (result != GP_OK) {
		free ((*camera)->functions);
		free ((*camera)->port);
		free ((*camera)->abilities);
		free (*camera);
		return (result);
	}

        return(GP_OK);
}

int
gp_camera_set_model (Camera *camera, const char *model)
{
	CHECK_NULL (camera && model);

	gp_debug_printf (GP_DEBUG_LOW, "core", "Setting model to '%s'", model);
	strncpy (camera->model, model, sizeof (camera->model));
	
	return (GP_OK);
}

int
gp_camera_get_model (Camera *camera, const char **model)
{
	CHECK_NULL (camera && model);

	*model = camera->model;

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
gp_camera_set_port (Camera *camera, CameraPortInfo *info)
{
	gp_port_settings settings;
	CHECK_NULL (camera);

	/*
	 * We need to create a new port because the port could have
	 * changed from a SERIAL to an USB one...
	 */
	CHECK_RESULT (gp_camera_unset_port (camera));
	CHECK_RESULT (gp_port_new (&camera->port, info->type));
	memcpy (camera->port_info, info, sizeof (CameraPortInfo));

	switch (camera->port->type) {
	case GP_PORT_SERIAL:
		CHECK_RESULT (gp_port_settings_get (camera->port, &settings));
		strcpy (settings.serial.port, info->path);
		CHECK_RESULT (gp_port_settings_set (camera->port, settings));
		break;
	default:
		break;
	}

	return (GP_OK);
}

int
gp_camera_set_port_path (Camera *camera, const char *port_path)
{
	int x, count;
	CameraPortInfo info;

	CHECK_NULL (camera && port_path);

	gp_debug_printf (GP_DEBUG_LOW, "core", "Setting port path to '%s'",
			 port_path); 
	CHECK_RESULT (gp_camera_unset_port (camera));
	CHECK_RESULT (count = gp_port_core_count ());
	for (x = 0; x < count; x++)
		if ((gp_port_core_get_info (x, &info) == GP_OK) &&
		    (!strcmp (port_path, info.path)))
			return (gp_camera_set_port (camera, &info));

	return (GP_ERROR_UNKNOWN_PORT); 
}

int
gp_camera_get_port_path (Camera *camera, const char **port_path)
{
	CHECK_NULL (camera && port_path);

	*port_path = camera->port_info->path;

	return (GP_OK);
}

int
gp_camera_set_port_name (Camera *camera, const char *port_name)
{
	int x, count;
	CameraPortInfo info;

	CHECK_NULL (camera && port_name);

	gp_debug_printf (GP_DEBUG_LOW, "core", "Setting port name to '%s'",
			 port_name);
	CHECK_RESULT (gp_camera_unset_port (camera));
	CHECK_RESULT (count = gp_port_core_count ());
	for (x = 0; x < count; x++)
		if ((gp_port_core_get_info (x, &info) == GP_OK) &&
		    (!strcmp (port_name, info.name)))
			return (gp_camera_set_port (camera, &info));

	return (GP_ERROR_UNKNOWN_PORT);
}

int
gp_camera_get_port_name (Camera *camera, const char **port_name)
{
	CHECK_NULL (camera && port_name);

	*port_name = camera->port_info->name;

	return (GP_OK);
}

int
gp_camera_set_port_speed (Camera *camera, int speed)
{
	CHECK_NULL (camera);

	camera->port_info->speed = speed;

	return (GP_OK);
}

int
gp_camera_set_progress_func (Camera *camera, CameraProgressFunc func,
			     void *data)
{
	CHECK_NULL (camera);

	camera->progress_func = func;
	camera->progress_data = data;

	return (GP_OK);
}

int
gp_camera_set_status_func (Camera *camera, CameraStatusFunc func,
			   void *data)
{
	CHECK_NULL (camera);

	camera->status_func = func;
	camera->status_data = data;

	return (GP_OK);
}

int
gp_camera_set_message_func (Camera *camera, CameraMessageFunc func,
			    void *data)
{
	CHECK_NULL (camera);

	camera->message_func = func;
	camera->message_data = data;

	return (GP_OK);
}

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

	if (camera->status_func)
		camera->status_func (camera, buffer, camera->status_data);

	return (GP_OK);
}

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

	if (camera->message_func)
		camera->message_func (camera, buffer, camera->message_data);

	return (GP_OK);
}

int
gp_camera_progress (Camera *camera, float percentage)
{
	CHECK_NULL (camera);

	if ((percentage < 0.) || (percentage > 1.)) {
		gp_debug_printf (GP_DEBUG_LOW, "camera",
				 "Wrong percentage: %f", percentage);
		return (GP_ERROR_BAD_PARAMETERS);
	}

	if (camera->progress_func)
		camera->progress_func (camera, percentage,
				       camera->progress_data);

	return (GP_OK);
}

int
gp_camera_ref (Camera *camera)
{
	CHECK_NULL (camera);

	camera->ref_count += 1;

	return (GP_OK);
}

int
gp_camera_unref (Camera *camera)
{
	CHECK_NULL (camera);

	camera->ref_count -= 1;

	if (camera->ref_count == 0)
		gp_camera_free (camera);

	return (GP_OK);
}

static int
gp_camera_exit (Camera *camera)
{
	int ret;

	GP_DEBUG ("ENTER: gp_camera_exit");
	CHECK_NULL (camera);

	if (camera->functions == NULL)
		return (GP_ERROR);

	if (camera->functions->exit == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_OPEN (camera);
	ret = camera->functions->exit (camera);
	CHECK_CLOSE (camera);

	GP_SYSTEM_DLCLOSE (camera->library_handle);
	CHECK_RESULT (ret);

	GP_DEBUG ("LEAVE: gp_camera_exit");
	return (GP_OK);
}

int
gp_camera_free (Camera *camera)
{
	CHECK_NULL (camera);

	gp_camera_exit (camera);

	CHECK_RESULT (gp_camera_unset_port (camera));
	CHECK_RESULT (gp_filesystem_free (camera->fs));

        if (camera->port_info)
                free (camera->port_info);
        if (camera->abilities) 
                free (camera->abilities);
        if (camera->functions) 
                free (camera->functions);
 
	free (camera);

	return (GP_OK);
}

int
gp_camera_init (Camera *camera)
{
        CameraList list;
	const char *model, *port;
	CameraLibraryInitFunc init_func;

	CHECK_NULL (camera);
	gp_camera_status (camera, _("Initializing camera..."));

	/*
	 * If the port or the model hasn't been indicated, try to
	 * figure it out (USB only). Beware of "Directory Browse".
	 */
	if (strcmp (camera->model, "Directory Browse")) {
		if (!camera->port || !strcmp ("", camera->model)) {
			
			/* Call auto-detect and choose the first camera */
			CRS (camera, gp_autodetect (&list));
			CRS (camera, gp_list_get_name  (&list, 0, &model));
			CRS (camera, gp_camera_set_model (camera, model));
			CRS (camera, gp_list_get_value (&list, 0, &port));
			CRS (camera, gp_camera_set_port_path (camera, port));
		}
		
		/* If we don't have a port at this point, return error */
		if (!camera->port) {
			gp_camera_status (camera, "");
			return (GP_ERROR_UNKNOWN_PORT);
		}

		/* Fill in camera abilities. */
		CRS (camera, gp_camera_abilities_by_name (camera->model,
							  camera->abilities));

		/* In case of USB, find the device */
		switch (camera->port->type) {
		case GP_PORT_USB:
			CRS (camera, gp_port_usb_find_device (camera->port,
					camera->abilities->usb_vendor,
					camera->abilities->usb_product));
			break;
		default:
			break;
		}

	} else {
		
		/* Fill in camera abilities. */
		CRS (camera, gp_camera_abilities_by_name (camera->model,
							  camera->abilities));
	}

	/* Load the library. */
	gp_debug_printf (GP_DEBUG_LOW, "core", "Loading library...");
	camera->library_handle = GP_SYSTEM_DLOPEN (camera->abilities->library);
	if (!camera->library_handle) {
		gp_camera_status (camera, "");
		return (GP_ERROR_LIBRARY);
	}

	/* Initialize the camera */
	init_func = GP_SYSTEM_DLSYM (camera->library_handle, "camera_init");
	CHECK_RESULT_OPEN_CLOSE (camera, init_func (camera));

	GP_DEBUG ("LEAVE: gp_camera_init");
	gp_camera_status (camera, "");
	return (GP_OK);
}

int
gp_camera_get_session (Camera *camera)
{
	CHECK_NULL (camera);

        return (camera->session);
}

int
gp_camera_get_config (Camera *camera, CameraWidget **window)
{
	CHECK_NULL (camera);

        if (camera->functions->get_config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	gp_camera_status (camera, _("Getting configuration..."));
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->get_config (camera,
								   window));
	gp_camera_status (camera, "");

	return (GP_OK);
}

int
gp_camera_set_config (Camera *camera, CameraWidget *window)
{
	CHECK_NULL (camera && window);

        if (camera->functions->set_config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	gp_camera_status (camera, _("Setting configuration..."));
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->set_config (camera,
								   window));
	gp_camera_status (camera, "");

	return (GP_OK);
}

int
gp_camera_get_summary (Camera *camera, CameraText *summary)
{
	CHECK_NULL (camera && summary);

        if (camera->functions->summary == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	gp_camera_status (camera, _("Getting summary..."));
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->summary (camera,
								summary));
	gp_camera_status (camera, "");

	return (GP_OK);
}

int
gp_camera_get_manual (Camera *camera, CameraText *manual)
{
	CHECK_NULL (camera && manual);

        if (camera->functions->manual == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->manual (camera,
								    manual));

	return (GP_OK);
}

int
gp_camera_get_about (Camera *camera, CameraText *about)
{
	CHECK_NULL (camera && about);

        if (camera->functions->about == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->about (camera,
								   about));

	return (GP_OK);
}

int
gp_camera_capture (Camera *camera, int capture_type, 
		       CameraFilePath *path)
{
	CHECK_NULL (camera && path);

        if (camera->functions->capture == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	gp_camera_status (camera, "Capturing image...");
	CHECK_RESULT_OPEN_CLOSE (camera,
		camera->functions->capture (camera, capture_type, path));
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);

	return (GP_OK);
}

int
gp_camera_capture_preview (Camera *camera, CameraFile *file)
{
	CHECK_NULL (camera && file);

        if (camera->functions->capture_preview == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	gp_camera_status (camera, "Capturing preview...");
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->capture_preview (
								camera, file));
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);

	return (GP_OK);
}

const char *
gp_camera_get_result_as_string (Camera *camera, int result)
{
        /* Camlib error? */
        if (result <= -1000 && (camera != NULL)) {
                if (camera->functions->result_as_string == NULL)
                        return _("Error description not available");
                return (camera->functions->result_as_string (camera, result));
        }

        return (gp_result_as_string (result));
}

int
gp_camera_folder_list_files (Camera *camera, const char *folder, 
			     CameraList *list)
{
	int result;

	GP_DEBUG ("ENTER: gp_camera_folder_list_files");
	CHECK_NULL (camera && folder && list);

	gp_list_reset (list);

	/* Check first if the camera driver uses the filesystem */
	CHECK_OPEN (camera);
	result = gp_filesystem_list_files (camera->fs, folder, list);
	CHECK_CLOSE (camera);
//Remove this when camera drivers are updated to use camera->fs
if (!list->count && !camera->functions->folder_list_files)
return (result);
if (list->count)
	if (result != GP_ERROR_NOT_SUPPORTED)
		return (result);

	if (!camera->functions->folder_list_files)
		return (GP_ERROR_NOT_SUPPORTED);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Getting file list for "
			 "folder '%s'...", folder);
	gp_list_reset (list);
	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->folder_list_files (
							camera, folder, list));
	CHECK_RESULT (gp_list_sort (list));

	GP_DEBUG ("LEAVE: gp_camera_folder_list_files");
        return (GP_OK);
}

int
gp_camera_folder_list_folders (Camera *camera, const char* folder, 
			       CameraList *list)
{
	int result;

	GP_DEBUG ("ENTER: gp_camera_folder_list_folders");
	CHECK_NULL (camera && folder && list);

	gp_list_reset (list);

	/* Check first if the camera driver uses the filesystem */
	CHECK_OPEN (camera);
	result = gp_filesystem_list_folders (camera->fs, folder, list);
	CHECK_CLOSE (camera);
//Remove this when camera drivers are updated to use camera->fs
if (!list->count && !camera->functions->folder_list_folders)
return (result);
if (list->count)
	if (result != GP_ERROR_NOT_SUPPORTED)
		return (result);

	if (!camera->functions->folder_list_folders)
		return (GP_ERROR_NOT_SUPPORTED);

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Getting folder list for "
			 "folder '%s'...", folder);
	gp_list_reset (list);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->folder_list_folders(
						camera, folder, list));

	CHECK_RESULT (gp_list_sort (list));

	GP_DEBUG ("LEAVE: gp_camera_folder_list_folders");
        return (GP_OK);
}

int
gp_camera_folder_delete_all (Camera *camera, const char *folder)
{
	GP_DEBUG ("ENTER: gp_camera_folder_delete_all");
	CHECK_NULL (camera && folder);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_all (camera->fs,
								folder));

	GP_DEBUG ("LEAVE: gp_camera_folder_delete_all");
	return (GP_OK);
}

int
gp_camera_folder_put_file (Camera *camera, const char *folder, CameraFile *file)
{
	CHECK_NULL (camera && folder && file);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_put_file (camera->fs,
							folder, file));

	return (GP_OK);
}

int
gp_camera_folder_get_config (Camera *camera, const char *folder, 
			     CameraWidget **window)
{
	CHECK_NULL (camera && folder && window);

        if (camera->functions->folder_get_config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->folder_get_config (
						camera, folder, window));

	return (GP_OK);
}

int
gp_camera_folder_set_config (Camera *camera, const char *folder, 
	                     CameraWidget *window)
{
	CHECK_NULL (camera && folder && window);

        if (camera->functions->folder_set_config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->folder_set_config (
						camera, folder, window));

	return (GP_OK);
}

int
gp_camera_file_get_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info)
{
	int result = GP_OK;
	const char *mime_type;
	const char *data;
	long int size;

	GP_DEBUG ("ENTER: gp_camera_file_get_info");
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
	 * If the camera doesn't support file_info_get, we simply get
	 * the preview and the file and look for ourselves...
	 */
	if (!camera->functions->file_get_info) {
		CameraFile *cfile;

		/* It takes too long to get the file */
		info->file.fields = GP_FILE_INFO_NONE;

		/* Get the preview */
		info->preview.fields = GP_FILE_INFO_NONE;
		CRS (camera, gp_file_new (&cfile));
		if (gp_camera_file_get (camera, folder, file,
					GP_FILE_TYPE_PREVIEW, cfile)== GP_OK) {
			info->preview.fields |= GP_FILE_INFO_SIZE | 
						GP_FILE_INFO_TYPE;
			gp_file_get_data_and_size (cfile, &data, &size);
			info->preview.size = size;
			gp_file_get_mime_type (cfile, &mime_type);
			strncpy (info->preview.type, mime_type,
				 sizeof (info->preview.type));
		}
		gp_file_unref (cfile);
	} else
		CHECK_RESULT_OPEN_CLOSE (camera,
				camera->functions->file_get_info (
						camera, folder, file, info));
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);

	/* We don't trust the camera libraries */
	info->file.fields |= GP_FILE_INFO_NAME;
	strncpy (info->file.name, file, sizeof (info->file.name));
	info->preview.fields &= ~GP_FILE_INFO_NAME;

	GP_DEBUG ("LEAVE: gp_camera_file_get_info");
	return (GP_OK);
}

int
gp_camera_file_set_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info)
{
	int result;

	CHECK_NULL (camera && info && folder && file);

	/* Check first if the camera driver supports the filesystem */
	CHECK_OPEN (camera);
	result = gp_filesystem_set_info (camera->fs, folder, file, info);
	CHECK_CLOSE (camera);
	if (result != GP_ERROR_NOT_SUPPORTED)
		return (result);

	if (camera->functions->file_set_info == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->file_set_info (
						camera, folder, file, info));

	return (GP_OK);
}

int 
gp_camera_file_get (Camera *camera, const char *folder, const char *file,
		    CameraFileType type, CameraFile *camera_file)
{
	int result;

	GP_DEBUG ("ENTER: gp_camera_file_get");
	CHECK_NULL (camera && folder && file && camera_file);

	/* Did we get reasonable foldername/filename? */
	if (strlen (folder) == 0)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	if (strlen (file) == 0)
		return (GP_ERROR_FILE_NOT_FOUND);
   
	CHECK_OPEN (camera);
	gp_camera_status (camera, "Getting '%s' from folder '%s'...",
			  file, folder);
	result = gp_filesystem_get_file (camera->fs, folder, file, type,
					 camera_file);
	gp_camera_status (camera, "");
	gp_camera_progress (camera, 0.0);
	CHECK_CLOSE (camera);
	if (result != GP_ERROR_NOT_SUPPORTED)
		return (result);

	if (camera->functions->file_get == NULL)
		return (GP_ERROR_NOT_SUPPORTED); 

	CHECK_RESULT (gp_file_set_type (camera_file, type));
	CHECK_RESULT (gp_file_set_name (camera_file, file));

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->file_get (camera,
					folder, file, type, camera_file));

	GP_DEBUG ("LEAVE: gp_camera_file_get");
	return (GP_OK);
}

int
gp_camera_file_get_config (Camera *camera, const char *folder, 
			   const char *file, CameraWidget **window)
{
	CHECK_NULL (camera && folder && file && window);

	if (camera->functions->file_get_config == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->file_get_config (
						camera, folder, file, window));

	return (GP_OK);
}

int
gp_camera_file_set_config (Camera *camera, const char *folder, 
			   const char *file, CameraWidget *window)
{
	CHECK_NULL (camera && window && folder && file);

	if (camera->functions->file_set_config == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT_OPEN_CLOSE (camera, camera->functions->file_set_config (
					camera, folder, file, window));

	return (GP_OK);
}

int
gp_camera_file_delete (Camera *camera, const char *folder, const char *file)
{
	GP_DEBUG ("ENTER: gp_camera_file_delete");
	CHECK_NULL (camera && folder && file);

	CHECK_RESULT_OPEN_CLOSE (camera, gp_filesystem_delete_file (
						camera->fs, folder, file));

	GP_DEBUG ("LEAVE: gp_camera_file_delete");
	return (GP_OK);
}


