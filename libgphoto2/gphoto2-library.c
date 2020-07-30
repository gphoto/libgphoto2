/** \file
 *
 * \author Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <gphoto2/gphoto2-library.h>

/**
 * \brief Get a unique camera id
 * \param id a #CameraText that receives the id string
 *
 * This function should write a unique id into id and return #GP_OK. That is,
 * choose a unique id, use strncpy in order to copy it into the id, and
 * return #GP_OK. The driver name should suffice.
 *
 * \returns a gphoto2 error code
 **/
int
camera_id (CameraText *id)
{
	strcpy (id->text, "sample driver");
	return (GP_OK);
}

/**
 * \brief Get a list of abilities of all supported cameras
 * \param list a #CameraAbilitiesList
 *
 * This function should use #gp_abilities_list_append as many times as the
 * number of models the camera driver supports. That is, fill out (in a loop)
 * the #CameraAbilities for each model and append each of those to the
 * supplied list using gp_abilities_list_append(). Then, return #GP_OK.
 *
 * \returns a gphoto2 error code
 **/
int
camera_abilities (CameraAbilitiesList *list)
{
	/* Dummy implementation */
	return (GP_OK);
}

/**
 * \brief Initialize the camera
 * \param camera a #Camera
 *
 * This is the most interesting function in your library. Here, you tell
 * gphoto2 what operations your camera supports and you try to connect
 * to the camera. That is, access camera->functions directly and set them
 * to your implementation (if you have any). Then, tell the #CameraFilesystem
 * (available in #camera->fs) how to retrieve lists
 * (gp_filesystem_set_list_funcs()), how to retrieve or set file information
 * (gp_filesystem_set_info_funcs()), how to get or delete files
 * (gp_filesystem_set_file_funcs()), or how to put files or delete all files
 * in a folder (#gp_filesystem_set_folder_funcs). After that, configure
 * the port (#camera->port) which is already opened by gphoto2. You just have
 * to call gp_port_settings_get(), adjust the settings, call
 * gp_port_settings_set(), and try to write to and read from the port.
 * If the camera responds, return #GP_OK. If not, return some
 * meaningful error code.
 *
 * \returns a gphoto2 error code
 **/
int
camera_init (Camera *camera)
{
	/* Dummy implementation */
	return (GP_OK);
}
