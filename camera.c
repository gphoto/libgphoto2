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

#include "gphoto2-camera.h"

#include <stdlib.h>
#include <string.h>

#include "gphoto2-result.h"
#include "gphoto2-frontend.h"
#include "gphoto2-core.h"

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

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

int
gp_camera_new (Camera **camera)
{
	CHECK_NULL (camera);

	/* Be nice to frontend-writers... */
	if (!gp_is_initialized ())
		CHECK_RESULT (gp_init (GP_DEBUG_NONE));

        *camera = malloc (sizeof (Camera));
	if (!*camera) 
		return (GP_ERROR_NO_MEMORY);

        /* Initialize the members */
        (*camera)->port       = (CameraPortInfo*)malloc(sizeof(CameraPortInfo));
	if (!(*camera)->port) 
		return (GP_ERROR_NO_MEMORY);
	(*camera)->port->type = GP_PORT_NONE;
	strcpy ((*camera)->port->path, "");
        (*camera)->abilities  = (CameraAbilities*)malloc(sizeof(CameraAbilities));
        (*camera)->functions  = (CameraFunctions*)malloc(sizeof(CameraFunctions));
        (*camera)->library_handle  = NULL;
        (*camera)->camlib_data     = NULL;
        (*camera)->frontend_data   = NULL;
	(*camera)->session    = glob_session_camera++;
        (*camera)->ref_count  = 1;
	if (!(*camera)->abilities || !(*camera)->functions) 
		return (GP_ERROR_NO_MEMORY);

	/* Initialize function pointers to NULL.*/
	memset((*camera)->functions, 0, sizeof (CameraFunctions));

        return(GP_OK);
}

int
gp_camera_set_model (Camera *camera, const char *model)
{
	CHECK_NULL (camera && model);

	strcpy (camera->model, model);
	
	return (GP_OK);
}

int
gp_camera_get_model (Camera *camera, const char **model)
{
	CHECK_NULL (camera && model);

	*model = camera->model;

	return (GP_OK);
}

int
gp_camera_set_port_path (Camera *camera, const char *port_path)
{
	int x, count;
	CameraPortInfo info;

	CHECK_NULL (camera && port_path);

	CHECK_RESULT (count = gp_port_count_get ());
	for (x = 0; x < count; x++)
		if ((gp_port_info_get (x, &info) == GP_OK) &&
		    (!strcmp (port_path, info.path))) {
			strcpy (camera->port->path, info.path);
			strcpy (camera->port->name, info.name);
			return (GP_OK);
		}

	return (GP_ERROR_IO_UNKNOWN_PORT); 
}

int
gp_camera_get_port_path (Camera *camera, const char **port_path)
{
	CHECK_NULL (camera && port_path);

	*port_path = camera->port->path;

	return (GP_OK);
}

int
gp_camera_set_port_name (Camera *camera, const char *port_name)
{
	int x, count;
	CameraPortInfo info;

	CHECK_NULL (camera && port_name);

	CHECK_RESULT (count = gp_port_count_get ());
	for (x = 0; x < count; x++)
		if ((gp_port_info_get (x, &info) == GP_OK) &&
		    (!strcmp (port_name, info.name))) {
			strcpy (camera->port->path, info.path);
			strcpy (camera->port->name, info.name);
			return (GP_OK);
		}
	
	return (GP_ERROR_IO_UNKNOWN_PORT);
}

int
gp_camera_get_port_name (Camera *camera, const char **port_name)
{
	CHECK_NULL (camera && port_name);

	*port_name = camera->port->name;

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

	CHECK_NULL (camera);

	if (camera->functions == NULL)
		return (GP_ERROR);

	if (camera->functions->exit == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	ret = camera->functions->exit (camera);

	GP_SYSTEM_DLCLOSE (camera->library_handle);

	return (ret);
}

int
gp_camera_free (Camera *camera)
{
	CHECK_NULL (camera);

	gp_camera_exit (camera);

        if (camera->port)
                free (camera->port);
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

	CHECK_NULL (camera);

	/* Beware of 'Directory Browse'... */
	if (strcmp (camera->model, "Directory Browse") != 0) {

		/* Set the port's path if only the name has been indicated. */
		if ((strcmp (camera->port->path, "") == 0) && 
		    (strcmp (camera->port->name, "") != 0))
			CHECK_RESULT (gp_camera_set_port_name (camera, 
						camera->port->name));
	
		/* If the port hasn't been indicated, try to figure it 	*/
		/* out (USB only). 					*/
		if (strcmp (camera->port->path, "") == 0) {
			
			/* Call auto-detect */
			CHECK_RESULT (gp_autodetect (&list));

			/* Retrieve the first auto-detected camera */
			CHECK_RESULT (gp_list_get_name  (&list, 0, &model));
			CHECK_RESULT (gp_camera_set_model (camera, model));
			CHECK_RESULT (gp_list_get_value (&list, 0, &port));
			CHECK_RESULT (gp_camera_set_port_path (camera, port));
		}
	
	        /* Set the port type from the path in case the 	*/
		/* frontend didn't. 				*/
	        if (camera->port->type == GP_PORT_NONE) {
			if (!strncmp (camera->port->path, "serial", 6))
				camera->port->type = GP_PORT_SERIAL;
			else if (!strncmp (camera->port->path, "usb", 3))
				camera->port->type = GP_PORT_USB;
		}
		if (camera->port->type == GP_PORT_NONE)
			return (GP_ERROR_IO_UNKNOWN_PORT);
	}
		
	/* If the model hasn't been indicated, try to figure it out 	*/
	/* through probing. 						*/
	if (!strcmp (camera->model, "")) {

		/* Call auto-detect */
		CHECK_RESULT (gp_autodetect (&list));

		/* Retrieve the first auto-detected camera */
		CHECK_RESULT (gp_list_get_name  (&list, 0, &model));
		CHECK_RESULT (gp_camera_set_model (camera, model));
		CHECK_RESULT (gp_list_get_value (&list, 0, &port));
		CHECK_RESULT (gp_camera_set_port_path (camera, port));
	}

	/* Fill in camera abilities. */
	CHECK_RESULT (gp_camera_abilities_by_name (camera->model,
						   camera->abilities));

	/* Load the library. */
	gp_debug_printf (GP_DEBUG_LOW, "core", "Loading library...");
	camera->library_handle = GP_SYSTEM_DLOPEN (camera->abilities->library);
	if (!camera->library_handle) {
		gp_camera_free (camera);
		return (GP_ERROR);
	}

	camera->functions->id = GP_SYSTEM_DLSYM (camera->library_handle,
						 "camera_id");
	camera->functions->abilities = GP_SYSTEM_DLSYM (camera->library_handle,
					                "camera_abilities");
	camera->functions->init = GP_SYSTEM_DLSYM (camera->library_handle,
						   "camera_init");

        return (camera->functions->init (camera));
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

        return (camera->functions->get_config (camera, window));
}

int
gp_camera_set_config (Camera *camera, CameraWidget *window)
{
	CHECK_NULL (camera && window);

        if (camera->functions->set_config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->set_config (camera, window));
}

int
gp_camera_get_summary (Camera *camera, CameraText *summary)
{
	CHECK_NULL (camera && summary);

        if (camera->functions->summary == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->summary (camera, summary));
}

int
gp_camera_get_manual (Camera *camera, CameraText *manual)
{
	CHECK_NULL (camera && manual);

        if (camera->functions->manual == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->manual (camera, manual));
}

int
gp_camera_get_about (Camera *camera, CameraText *about)
{
	CHECK_NULL (camera && about);

        if (camera->functions->about == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->about (camera, about));
}

int
gp_camera_capture (Camera *camera, int capture_type, 
		       CameraFilePath *path)
{
	CHECK_NULL (camera && path);

        if (camera->functions->capture == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->capture(camera, capture_type, path));
}

int
gp_camera_capture_preview (Camera *camera, CameraFile *file)
{
	CHECK_NULL (camera && file);

        if (camera->functions->capture_preview == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->capture_preview(camera, file));
}

char *
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
	CHECK_NULL (camera && folder && list);

	if (camera->functions->folder_list_files == NULL)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT (gp_list_remove_all (list));

	gp_debug_printf (GP_DEBUG_HIGH, "core", "Getting file list for "
			 "folder '%s'...", folder);
	CHECK_RESULT (gp_list_remove_all (list));
	CHECK_RESULT (camera->functions->folder_list_files (camera, folder,
							    list));
	CHECK_RESULT (gp_list_sort (list));

        return (GP_OK);
}

int
gp_camera_folder_list_folders (Camera *camera, const char* folder, 
			       CameraList *list)
{
	CHECK_NULL (camera && folder && list);

	if (camera->functions->folder_list_folders == NULL)
		return (GP_ERROR_NOT_SUPPORTED);
	
	gp_debug_printf (GP_DEBUG_HIGH, "core", "Getting folder list for "
			 "folder '%s'...", folder);
	CHECK_RESULT (gp_list_remove_all (list));
	CHECK_RESULT (camera->functions->folder_list_folders (camera, folder,
							      list));
	CHECK_RESULT (gp_list_sort (list));

        return (GP_OK);
}

static int
delete_one_by_one (Camera *camera, const char *folder)
{
	CameraList list;
	int i;
	const char *name;

	CHECK_NULL (camera && folder);

	CHECK_RESULT (gp_camera_folder_list_files (camera, folder, &list));

	for (i = gp_list_count (&list); i > 0; i--) {
		CHECK_RESULT (gp_list_get_name (&list, i - 1, &name));
		CHECK_RESULT (gp_camera_file_delete (camera, folder, name));
	}

	return (GP_OK);
}

int
gp_camera_folder_delete_all (Camera *camera, const char *folder)
{
	CameraList list;

	CHECK_NULL (camera && folder);

	/* 
	 * As this function isn't supported by all cameras, we fall back to
	 * deletion one by one.
	 */
        if (camera->functions->folder_delete_all == NULL)
		return (delete_one_by_one (camera, folder));

        if (camera->functions->folder_delete_all (camera, folder) != GP_OK)
		return (delete_one_by_one (camera, folder));

	/* Sanity check: All pictures deleted? */
	CHECK_RESULT (gp_camera_folder_list_files (camera, folder, &list));
	if (gp_list_count (&list) > 0)
		return (delete_one_by_one (camera, folder));

	return (GP_OK);
}

int
gp_camera_folder_put_file (Camera *camera, const char *folder, CameraFile *file)
{
	CHECK_NULL (camera && folder && file);

        if (camera->functions->folder_put_file == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->folder_put_file (camera, folder, file));
}

int
gp_camera_folder_get_config (Camera *camera, const char *folder, 
			     CameraWidget **window)
{
	CHECK_NULL (camera && folder && window);

        if (camera->functions->folder_get_config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->folder_get_config (camera, folder, window));
}

int
gp_camera_folder_set_config (Camera *camera, const char *folder, 
	                     CameraWidget *window)
{
	CHECK_NULL (camera && folder && window);

        if (camera->functions->folder_set_config == NULL)
                return (GP_ERROR_NOT_SUPPORTED);

        return (camera->functions->folder_set_config (camera, folder, window));
}

int
gp_camera_file_get_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info)
{
	int result = GP_OK;
	const char *mime_type;
	const char *data;
	long int size;

	CHECK_NULL (camera && folder && file && info);

	memset (info, 0, sizeof (CameraFileInfo));

	/* If the camera doesn't support file_info_get, we simply get	*/
	/* the preview and the file and look for ourselves...		*/
	if (!camera->functions->file_get_info) {
		CameraFile *cfile;

		/* It takes too long to get the file */
		info->file.fields = GP_FILE_INFO_NONE;

		/* Get the preview */
		info->preview.fields = GP_FILE_INFO_NONE;
		CHECK_RESULT (gp_file_new (&cfile));
		if (gp_camera_file_get (camera, folder, file,
					GP_FILE_TYPE_PREVIEW, cfile)== GP_OK) {
			info->preview.fields |= GP_FILE_INFO_SIZE | 
						GP_FILE_INFO_TYPE;
			gp_file_get_data_and_size (cfile, &data, &size);
			info->preview.size = size;
			gp_file_get_mime_type (cfile, &mime_type);
			strcpy (info->preview.type, mime_type);
		}
		CHECK_RESULT (gp_file_unref (cfile));
	} else
		result = camera->functions->file_get_info (camera, folder,
							   file, info);

	/* We don't trust the camera libraries */
	info->file.fields |= GP_FILE_INFO_NAME;
	strcpy (info->file.name, file);
	info->preview.fields &= ~GP_FILE_INFO_NAME; 

	return (result);
}

int
gp_camera_file_set_info (Camera *camera, const char *folder, 
			 const char *file, CameraFileInfo *info)
{
	CHECK_NULL (camera && info && folder && file);

	if (camera->functions->file_set_info == NULL)
		return (GP_ERROR_NOT_SUPPORTED);
	
	return (camera->functions->file_set_info (camera, folder, file, info));
}

int 
gp_camera_file_get (Camera *camera, const char *folder, const char *file,
		    CameraFileType type, CameraFile *camera_file)
{
	CHECK_NULL (camera && folder && file && camera_file);

        if (camera->functions->file_get == NULL)
                return (GP_ERROR_NOT_SUPPORTED);
	
	/* Did we get reasonable foldername/filename? */
	if (strlen (folder) == 0)
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	if (strlen (file) == 0)
		return (GP_ERROR_FILE_NOT_FOUND);

	CHECK_RESULT (gp_file_set_type (camera_file, type));

        return (camera->functions->file_get (camera, folder, file, type,
					     camera_file));
}

int
gp_camera_file_get_config (Camera *camera, const char *folder, 
			   const char *file, CameraWidget **window)
{
	CHECK_NULL (camera && folder && file && window);

	if (camera->functions->file_get_config == NULL)
		return (GP_ERROR_NOT_SUPPORTED);
		
	return (camera->functions->file_get_config (camera, folder, file, 
						    window));
}

int
gp_camera_file_set_config (Camera *camera, const char *folder, 
			   const char *file, CameraWidget *window)
{
	CHECK_NULL (camera && window && folder && file);

	if (camera->functions->file_set_config == NULL)
		return (GP_ERROR_NOT_SUPPORTED);
	
	return (camera->functions->file_set_config (camera, folder, 
						    file, window));
}

int
gp_camera_file_delete (Camera *camera, const char *folder, const char *file)
{
	CHECK_NULL (camera && folder && file);

        if (camera->functions->file_delete == NULL)
                return (GP_ERROR_NOT_SUPPORTED);
		
        return (camera->functions->file_delete (camera, folder, file));
}


