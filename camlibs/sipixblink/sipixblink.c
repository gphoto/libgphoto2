/* sipixblink.c
 *
 * Copyright (C) 2002 Vincent Sanders <vince@kyllikki.org>
 * Copyright (C) 2002 Marcus Meissner <marcus@jet.franken.de>
 * Original template is
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2 of the License
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

#include <gphoto2-library.h>
#include <gphoto2-result.h>

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

int
camera_id (CameraText *id)
{
	strcpy(id->text, "Sipix Blink");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Sipix Blink");
	a.status		= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port			= GP_PORT_USB;
	a.usb_vendor		= 0x0851;
	a.usb_product		= 0x1542;

	a.operations		= GP_OPERATION_NONE;
	a.file_operations	= 0;
	a.folder_operations	= GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);
	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	return (GP_OK);
}

static int
_get_number_images(Camera *camera) {
	int ret;
	unsigned char reply[8];

	ret = gp_port_usb_msg_read (camera->port, 0, 0, 0, reply, 2);
	if (ret < GP_OK)
		return ret;
	return reply[0] | (reply[1]<<8);
}

/* this is just a part of the decoder. it does not work yet. */
static int
_decomp(unsigned char *input, unsigned short *out, unsigned short *table) {
	int inpos = 1, outpos = 0;
	int xinp = input[0];
	int curinflags = (input[0] & 0x3f) << 2;

	if (!input[inpos]) return inpos;

	while (1) {
		if (outpos >= 0x40) return inpos;
		if (inpos >= 128) return inpos;

		switch (curinflags) {
		case 0x00:
			xinp = input[inpos];
			curinflags = xinp & 0xc0;
			outpos = xinp & 0x3f;
			fprintf(stderr,"00:input was %02x, outpos is now %d\n", xinp, outpos);
			inpos++;
			break;
		case 0x40:
			xinp = input[inpos];
			curinflags = xinp & 0xc0;
			//out[outpos+1] = (table+0)[xinp];
			//out[outpos  ] = (table+256)[xinp];
			outpos += 2;
			inpos  += 1;
			fprintf(stderr,"40:input was %02x\n", xinp);
			break;
		case 0x80:
			xinp = input[inpos];
			curinflags = xinp & 0xc0;
			//out[outpos] = (table+512)[xinp];
			inpos++;
			outpos++;
			fprintf(stderr,"80:input was %02x\n", xinp);
			break;
		case 0xc0:
			xinp=((input[inpos+0]&0x1f)<<7)|(input[inpos+1]&0x7f);
			if (xinp & 0x80) 
				xinp |= 0xf0;	
			curinflags = input[inpos+0] & 0xc0;
			//out[outpos] = xinp;
			outpos++;
			inpos += 2;
			fprintf(stderr,"80:input was %04x\n", xinp);
			break;
		default:
			break;
		}
		if (!input[inpos]) return inpos;
	}
}

static int
_check_image_header(unsigned char *d , int *width, int *height) {
	if (d[0] != 0)
		return FALSE;
	if (d[1] > 0x3f)
		return FALSE;
	if ((d[2] & 0xc0) != 0x80)
		return FALSE;
	*width	= ((d[2] & 0x3f) << 4) | ((d[3] >> 3) & 0xf);
	*height	= ((d[3] & 0x07) << 7) | (d[4] & 0x7f);

	/*fprintf(stderr,"picture is %d x %d\n",width,height);*/

	return TRUE;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	unsigned char reply[8];
	int image_no, picsize, width, height;
	unsigned char *xdata;


	if (strcmp(folder,"/"))
	    return GP_ERROR_BAD_PARAMETERS;
        image_no = gp_filesystem_number(fs, folder, filename, context);

        /* size */
	do {
		gp_port_usb_msg_read (camera->port, 1, image_no , 1, reply, 8);
	} while(reply[0]!=0);

	picsize = reply[1] | (reply[2] << 8) | ( reply[3] << 16);
	/* Setup bulk transfer */
    	do {
		gp_port_usb_msg_read (camera->port, 2, image_no , 0, reply, 6);
	} while(reply[0]!=0);

	/* bulk read */
	xdata = malloc(picsize);
	gp_port_read(camera->port, xdata, picsize);

	_check_image_header(xdata, &width, &height);

	/*_decomp(xdata+6, NULL, NULL);*/

	gp_file_append(file, xdata, picsize);

	free(xdata);
	gp_file_set_mime_type (file, GP_MIME_UNKNOWN);
	gp_file_set_name (file, filename);
	return (GP_OK);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, CameraFile *file,
	       void *data, GPContext *context)
{
	Camera *camera;

	/*
	 * Upload the file to the camera. Use gp_file_get_data_and_size,
	 * gp_file_get_name, etc.
	 */

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;

	/* Delete the file from the camera. */

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return (GP_OK);
}

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context)
{
	gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration", window);

	/* Append your sections and widgets here. */

	return (GP_OK);
}

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context)
{
	/*
	 * Check if the widgets' values have changed. If yes, tell the camera.
	 */

	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	/*
	 * Capture a preview and return the data in the given file (again,
	 * use gp_file_set_data_and_size, gp_file_set_mime_type, etc.).
	 * libgphoto2 assumes that previews are NOT stored on the camera's
	 * disk. If your camera does, please delete it from the camera.
	 */

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	char reply[6];
	int  oldimgno, newimgno, ret;

	oldimgno = _get_number_images(camera);
	if (oldimgno < GP_OK)
		return oldimgno;
	do {	/* FIXME: this will loop until a picture is taken. */
		ret = gp_port_usb_msg_read (camera->port, 4, 0, 0, reply, 6);
	} while ((ret >= GP_OK) && (reply[0]!=0));
	if (ret < GP_OK)
		return ret;
	newimgno = _get_number_images(camera);
	if (newimgno < GP_OK)
		return newimgno;
	if (newimgno == oldimgno)
		return GP_ERROR;
	strcpy(path->folder,"/");
	sprintf(path->name,"blink%03i.raw",newimgno);
	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	/*
	 * Fill out the summary with some information about the current
	 * state of the camera (like pictures taken, etc.).
	 */

	return (GP_OK);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	/*
	 * If you would like to tell the user some information about how
	 * to use the camera or the driver, this is the place to do.
	 */

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Sipix StyleCam Blink Driver\n"
			       "Vincent Sanders <vince@kyllikki.org>\n"
			       "Marcus Meissner <marcus@jet.franken.de>.\n"
			       ));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;

	/* Get the file info here and write it into <info> */

	return (GP_OK);
}
static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	Camera *camera = data;

	/* Set the file info here from <info> */

	return (GP_OK);
}


static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	Camera *camera = data;

	/* List your folders here */

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	CameraFileInfo  info;
	unsigned char reply[6];
	int i, numpics, ret;

	numpics = _get_number_images(camera);
	if (numpics < GP_OK)
		return numpics;
	gp_list_populate (list, "blink%03i.raw", numpics);

#if 0
	/* This doesn't really work (yet).
	 * The pictures as read from the camera have a different size.
	 *
	 * I get told by this code 'CIF', but the image size is 320x240.
	 */
	for (i=0;i<numpics;i++) {
		/* we also get the fs info for free, so just set it */
		info.file.fields = GP_FILE_INFO_TYPE | GP_FILE_INFO_NAME | 
				GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT | 
				GP_FILE_INFO_SIZE;
		strcpy(info.file.type,GP_MIME_UNKNOWN);
		sprintf(info.file.name,"blink%03i.raw",i);
		ret = gp_filesystem_append(fs, "/", info.file.name, context);
		if (ret != GP_OK)
			return ret;
		do {
			ret=gp_port_usb_msg_read(camera->port,1,i,8,reply,6);
			if (ret < GP_OK)
				return ret;
		} while (reply[0]!=0);

		switch (reply[5] >> 5) {
		case 0:	/* VGA */
			info.file.width		= 640;
			info.file.height	= 480;
			break;
		case 1:	/* CIF */
			info.file.width		= 352;	
			info.file.height	= 288;
			break;
		case 2:	/* QCIF */
			info.file.width		= 176;	
			info.file.height	= 144;
			break;
		case 3:	/* QVGA */
			info.file.width		= 320;	
			info.file.height	= 240;
			break;
		case 4:	/* ? */
			info.file.width		= 800;	
			info.file.height	= 592;
			break;
		case 5:	/* VGA/4 */
			info.file.width		= 160;	
			info.file.height	= 120;
			break;
		default:
			return GP_ERROR;
		}
		info.file.size		=
			(reply[1])	|
			(reply[2] << 8)	|
			(reply[3] << 16)|
			(reply[4] << 24);
		ret = gp_filesystem_set_info_noop(fs, "/", info, context);
		if (ret != GP_OK)
			return ret;
	}
#endif
	return (GP_OK);
}

int
camera_init (Camera *camera, GPContext *context)
{
	int res;
	char reply[8];

	GPPortSettings settings;

	/* First, set up all the function pointers */
	camera->functions->exit                 = camera_exit;
	camera->functions->get_config           = camera_config_get;
	camera->functions->set_config           = camera_config_set;
	camera->functions->capture              = camera_capture;
	camera->functions->capture_preview      = camera_capture_preview;
	camera->functions->summary              = camera_summary;
	camera->functions->manual               = camera_manual;
	camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func,
				      folder_list_func, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, set_info_func,
				      camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, put_file_func,
					delete_all_func, NULL, NULL, camera);

	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */

	/* Get the current settings */
	gp_port_get_settings (camera->port, &settings);

	/* Modify the default settings the core parsed */
	settings.usb.interface=1;/* we need to use interface 1 */
	settings.usb.inep=4;


	/* Set the new settings */
	res = gp_port_set_settings (camera->port, settings);
	if (res != GP_OK) {
		gp_context_error (context, _("Could not apply USB settings"));
		return res;
	}

	/*
	 * Once you have configured the port, you should check if a
	 * connection to the camera can be established.
	 */
	gp_port_usb_msg_read (camera->port, 5, 1, 0, reply, 2);
	if(reply[0]!=1)
	    return (GP_ERROR_IO);

	gp_port_usb_msg_read (camera->port, 5, 0, 0, reply, 8);
	if(reply[0]!=1)
	    return (GP_ERROR_IO);

	return (GP_OK);
}
