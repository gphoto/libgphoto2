/* gphoto2-library.c: Dummy file for gtk-doc
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
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

#include "config.h"
#include "gphoto2-library.c"

/**
 * camera_id:
 * @id: a #CameraText
 *
 * This function should write a unique id into @id and return %GP_OK. That is,
 * choose a unique id, use %strncpy in order to copy it into the %id, and 
 * return %GP_OK.
 *
 * Return value: a gphoto2 error code
 **/
int
camera_id (CameraText *id)
{
	/* Dummy implementation */
	return (GP_OK);
}

/**
 * camera_abilities:
 * @list: a #CameraAbilitiesList
 *
 * This function should use #gp_abilities_list_append as many times as the 
 * number of models the camera driver supports. That is, fill out (in a loop)
 * the #CameraAbilities for each model and append each of those to the
 * supplied @list using #gp_abilities_list_append. Then, return %GP_OK.
 *
 * Return value: a gphoto2 error code
 **/
int
camera_abilities (CameraAbilitiesList *list)
{
	/* Dummy implementation */
	return (GP_OK);
}

/**
 * camera_init:
 * @camera: a #Camera
 *
 * This is the most interesting function in your library. Here, you tell
 * gphoto2 what operations your camera supports and you try to connect
 * to the camera. That is, access @camera->functions directly and set them
 * to your implementation (if you have any). Then, tell the #CameraFilesystem 
 * (available in @camera->fs) how to retrieve lists
 * (#gp_filesystem_set_list_funcs), how to retrieve or set file information
 * (#gp_filesystem_set_info_funcs), how to get or delete files
 * (#gp_filesystem_set_file_funcs), or how to put files or delete all files
 * in a folder (#gp_filesystem_set_folder_funcs). After that, configure
 * the port (%camera->port) which is already opened by gphoto2. You just have
 * to call #gp_port_settings_get, adjust the settings, call
 * #gp_port_settings_set, and try to write to and read from the port.
 * If the camera responds, return %GP_OK. If not, return some
 * meaningful error code.
 *
 * Return value: a gphoto2 error code
 **/
int
camera_init (Camera *camera)
{
	/* Dummy implementation */
	return (GP_OK);
}
