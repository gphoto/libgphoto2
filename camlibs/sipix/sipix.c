/* sipix.c
 *
 * Copyright (C) 2002 Marcus Meissner <marcus@jet.franken.de>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2-library.h>
#include <gphoto2-result.h>

#include "web2.h"

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
#  define _(String) (String)
#  define N_(String) (String)
#endif

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "SiPix Web2");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "SiPix Web2");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port     		= GP_PORT_USB;
	a.speed[0] 		= 0;
	a.usb_vendor		= 0x0c77;
	a.usb_product		= 0x1001;
	a.operations		=  GP_OPERATION_NONE;
	a.file_operations	=  GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW | GP_FILE_OPERATION_EXIF;
	a.folder_operations	=  GP_FOLDER_OPERATION_NONE;
	gp_abilities_list_append(list, a);
	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
    	return web2_exit(camera->port, context);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int image_no, result, flags, aeflag, junk;

	if (strcmp(folder,"/"))
	    return GP_ERROR_BAD_PARAMETERS;

	image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < GP_OK)
	    return image_no;

	result = web2_get_picture_info(camera->port, context, image_no, &junk, &junk, &flags, &junk);
	if (result != GP_OK)
	    return result;

	if (flags & 1) {
	    aeflag = 1;
	} else {
	    if (flags & 2) {
		aeflag = 2;
	    } else {
		fprintf(stderr,"Oops , 0xAD returned flags %x?!\n",flags);
		return GP_ERROR;
	    }
	}
	result = web2_select_picture(camera->port, context, image_no);
	if (result != GP_OK)
	    return result;

	result = web2_set_xx_mode(camera->port, context, aeflag);
	if (result != GP_OK)
	    return result;

	/*
	result = _cmd_e6(camera->port, context, fupp);
	if (result != GP_OK)
	    return result;

	for (i=0;i<7;i++)
	    fprintf(stderr,"%d (%x)",fupp[i],fupp[i]);
	fprintf(stderr,"\n");
	*/

	gp_file_set_mime_type (file, "image/jpeg");
	gp_file_set_name (file, filename);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
	    /*result = _cmd_ae(camera->port, context, 1);
	    if (result != GP_OK)
		fprintf(stderr,"ae 1 failed with %d\n",result);
		*/
	    result = web2_getpicture(camera->port,context,file);
	    break;
	case GP_FILE_TYPE_PREVIEW:
	    result = web2_getthumb (camera->port,context,file);
	    break;
	case GP_FILE_TYPE_EXIF:
	    result = web2_getexif(camera->port,context,file);
	    break;
	default:
	    return (GP_ERROR_NOT_SUPPORTED);
	}
	if (result < 0)
	    return result;
	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int aeflag, flags, junk, result, image_no;

	image_no = gp_filesystem_number(fs, folder, filename, context);
	if (image_no < GP_OK)
	    return image_no;

	result = web2_get_picture_info(camera->port, context, image_no, &junk, &junk, &flags, &junk);
	if (result != GP_OK)
	    return result;

	if (flags & 1) {
	    aeflag = 1;
	} else {
	    if (flags & 2) {
		aeflag = 2;
	    } else {
		fprintf(stderr,"Oops , 0xAD returned flags %x?!\n",flags);
		return GP_ERROR;
	    }
	}

	result = web2_select_picture(camera->port, context, image_no);
	if (result != GP_OK)
	    return result;


	result = web2_set_xx_mode(camera->port, context, aeflag);
	if (result != GP_OK)
	    return result;

	/* Delete the file from the camera. */
	result = web2_set_picture_attribute(camera->port, context, 0x40, &junk);
	if (result!= GP_OK)
	    return result;
	/* fprintf(stderr,"ba 0x40 returns %d\n",junk);*/
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("SiPix Web2\n"
			       "Marcus Meissner <marcus@jet.franken.de>\n"
			       "Driver for accessing the SiPix Web2 camera."));

	return (GP_OK);
}

/* 
 * This function makes sure that the <index in folder> is <picture index in
 * camera>. Even delete should keep that relation.
 */
static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int i, count, result, junk, flags, aeflag;

	result =  web2_getnumpics(camera->port, context, &junk, &count, &junk, &junk);
	if (result != GP_OK)
		return result;

	for (i=0;i<count;i++) {
	    char fn[20];
	    int	size;

	    result = web2_get_picture_info(camera->port, context, i, &junk, &junk, &flags, &junk);
	    if (result != GP_OK)
		return result;

	    if (flags & 1) {
		aeflag = 1;
	    } else {
		if (flags & 2) {
		    aeflag = 2;
		} else {
		    fprintf(stderr,"Oops , 0xAD returned flags %x?!\n",flags);
		    return GP_ERROR;
		}
	    }
	    result = web2_select_picture(camera->port, context, i);
	    if (result != GP_OK)
		return result;

	    result = web2_set_xx_mode(camera->port, context, aeflag);
	    if (result != GP_OK)
		return result;

	    result = web2_get_file_info(camera->port, context, fn, &size);
	    if (result != GP_OK)
		return result;
	    gp_list_append(list, fn, NULL);
	}
	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context) 
{
        camera->functions->exit                 = camera_exit;
        camera->functions->about                = camera_about;
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);
	return web2_init(camera->port, context);
}
