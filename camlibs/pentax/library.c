 /* Pentax K series library
 *
 * Copyright (c) 2011,2017 Marcus Meissner <meissner@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _DEFAULT_SOURCE

#include "config.h"

#define GP_MODULE "pentax"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-setting.h>

#include "libgphoto2/i18n.h"

#include "pslr.h"


bool debug = true;

struct _CameraPrivateLibrary {
	ipslr_handle_t	pslr;
	char		*lastfn;
	int		capcnt;
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "pentax");
	return GP_OK;
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;
	int ret;

	memset (&a, 0, sizeof(a));
	strcpy (a.model, "Pentax:K20D");
	a.status		= GP_DRIVER_STATUS_TESTING;
	a.port			= GP_PORT_USB_SCSI;
	a.speed[0]		= 0;
	a.usb_vendor		= 0x0a17;
	a.usb_product		= 0x0091;
	a.operations 		= GP_OPERATION_CAPTURE_IMAGE | GP_OPERATION_CONFIG | GP_OPERATION_TRIGGER_CAPTURE;
	a.folder_operations	= GP_FOLDER_OPERATION_NONE;
	a.file_operations	= GP_FILE_OPERATION_DELETE;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;
	strcpy (a.model, "Pentax:K10D");
	a.usb_product		= 0x006e;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;
	strcpy (a.model, "Pentax:K100D");
	a.usb_product		= 0x0070;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	strcpy (a.model, "Pentax:K100DS");
	a.usb_product		= 0x00a1;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	strcpy (a.model, "Pentax:K200D");
	a.usb_product		= 0x0093;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	/* weird usb vendor? */
	strcpy (a.model, "Pentax:K5D");
	a.usb_vendor		= 0x25fb;
	a.usb_product		= 0x0102;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	strcpy (a.model, "Pentax:K50D");
	a.usb_vendor		= 0x25fb;
	a.usb_product		= 0x0160;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	/* TommyLimKW at Twitter */
	strcpy (a.model, "Pentax:K01");
	a.usb_vendor		= 0x25fb;
	a.usb_product           = 0x0130;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	strcpy (a.model, "Pentax:K30");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x0132;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;

        strcpy (a.model, "Pentax:K5II");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x0148;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;

	/* TommyLimKW @ Twitter */
        strcpy (a.model, "Pentax:K5IIs");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x014a;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;

        strcpy (a.model, "Pentax:K3");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x0164;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;

        strcpy (a.model, "Pentax:K1");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x0178;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;

        strcpy (a.model, "Pentax:K3II");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x017a;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;

	/* Keld Henningsen <drawsacircle@hotmail.com> */
        strcpy (a.model, "Pentax:K70");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x017c;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;
	/* fifu7fi@gmail.com */
        strcpy (a.model, "Pentax:KP");
	a.usb_vendor		= 0x25fb;
        a.usb_product           = 0x017e;
        if (GP_OK != (ret = gp_abilities_list_append (list, a)))
                return ret;

	/* https://github.com/asalamon74/pktriggercord/issues/21 */
	strcpy (a.model, "Pentax:K1II");
	a.usb_vendor		= 0x25fb;
	a.usb_product           = 0x0183;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	strcpy (a.model, "Pentax:K3III");
	a.usb_vendor		= 0x25fb;
	a.usb_product           = 0x0189;
	if (GP_OK != (ret = gp_abilities_list_append (list, a)))
		return ret;

	return GP_OK;
}

int scsi_write(GPPort *port, uint8_t *cmd, uint32_t cmdLen,
               uint8_t *buf, uint32_t bufLen) {
        int ret;
        char sense_buffer[32];

        ret = gp_port_send_scsi_cmd (port, 1, (char*)cmd, cmdLen,
                sense_buffer, sizeof(sense_buffer), (char*)buf, bufLen);
        if (ret == GP_OK) return PSLR_OK;
        return PSLR_SCSI_ERROR;
}

int scsi_read(GPPort *port, uint8_t *cmd, uint32_t cmdLen,
              uint8_t *buf, uint32_t bufLen)
{
        int ret;
        char sense_buffer[32];

        ret = gp_port_send_scsi_cmd (port, 0, (char*)cmd, cmdLen,
                sense_buffer, sizeof(sense_buffer), (char*)buf, bufLen);
        if (ret == GP_OK) return bufLen;
        return -PSLR_SCSI_ERROR;
}

void close_drive(GPPort **port) {
	/* is going to be closed by other means */
	return;
}



static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	char		*statusinfo;
	pslr_status	status;

	pslr_get_status (&camera->pl->pslr, &status);

	statusinfo = pslr_get_status_info( &camera->pl->pslr, status );

	sprintf (summary->text, _(
		"Pentax K DSLR capture driver.\n"
		"Using code from pktriggercord by Andras Salamon.\n"
		"Collected Status Information:\n%s"
	), statusinfo);
	free (statusinfo);
	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	return GP_ERROR_NOT_SUPPORTED;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	return GP_OK;
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	/* virtual file created by Penta capture */
	if (!strncmp (filename, "capt", 4))
		return GP_OK;
	return GP_ERROR_NOT_SUPPORTED;
}


static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func
};

static int
save_buffer(pslr_handle_t camhandle, int bufno, pslr_buffer_type buftype, uint32_t jpegres, CameraFile *file)
{
	uint8_t			buf[65536];
	uint32_t		current;

	gp_log(GP_LOG_DEBUG, "pentax", "save_buffer: get buffer %d type %d res %d\n", bufno, buftype, jpegres);
	if ( pslr_buffer_open(camhandle, bufno, buftype, jpegres) != PSLR_OK)
		return GP_ERROR;

	current = 0;
	while (1) {
		uint32_t bytes;
		bytes = pslr_buffer_read(camhandle, buf, sizeof(buf));
		if (bytes == 0)
			break;
		if (((ipslr_handle_t*)camhandle)->model->id == 0x12b9c) {
			// PEF file got from K100D Super have broken header, WTF?
			if (current == 0 && (buftype == PSLR_BUF_PEF)) {
				const unsigned char correct_header[92] =
				{
					0x4d, 0x4d, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x08,
					0x00, 0x13, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00,
					0x00, 0x01, 0x00, 0x00, 0x0b, 0xe0, 0x01, 0x01,
					0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
					0x07, 0xe8, 0x01, 0x02, 0x00, 0x03, 0x00, 0x00,
					0x00, 0x01, 0x00, 0x0c, 0x00, 0x00, 0x01, 0x03,
					0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x80, 0x05,
					0x00, 0x00, 0x01, 0x06, 0x00, 0x03, 0x00, 0x00,
					0x00, 0x01, 0x80, 0x23, 0x00, 0x00, 0x01, 0x0f,
					0x00, 0x02, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00,
					0x00, 0xf2, 0x01, 0x10, 0x00, 0x02, 0x00, 0x00,
					0x00, 0x14, 0x00, 0x00
				};

				if (bytes < sizeof(correct_header))
					return GP_ERROR;
				memcpy(buf, correct_header, sizeof(correct_header));
			}
		}
		gp_file_append (file, (char*)buf, bytes);
		current += bytes;
	}
	pslr_buffer_close(camhandle);
	return current;
}

static int
_timeout_passed(struct timeval *start, int timeout) {
	struct timeval curtime;

	gettimeofday (&curtime, NULL);
	return ((curtime.tv_sec - start->tv_sec)*1000)+((curtime.tv_usec - start->tv_usec)/1000) >= timeout;
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	pslr_handle_t		p = &camera->pl->pslr;
	pslr_status		status;
	int			ret, length;
	CameraFile		*file = NULL;
	CameraFileInfo		info;
	int			bufno, i;
	const char		*mimes[2];
	int			buftypes[2], jpegres[2], nrofdownloads = 1;
	char			*fns[2], *lastfn = NULL;
        struct timeval  	event_start;

	gp_log (GP_LOG_DEBUG, "pentax", "camera_capture");

	pslr_get_status (p, &status);
	pslr_shutter (p);

	strcpy (path->folder, "/");
	gp_log (GP_LOG_ERROR, "pentax", "image format image=0x%x, raw=0x%x", status.image_format, status.raw_format);
	switch (status.image_format) {
	case PSLR_IMAGE_FORMAT_JPEG:
		sprintf (path->name, "capt%04d.jpg", camera->pl->capcnt++);
		mimes[0] = GP_MIME_JPEG;
		buftypes[0] = status.jpeg_quality + 1;
		jpegres[0] = status.jpeg_resolution;
		fns[0] = strdup(path->name);
		break;
	case PSLR_IMAGE_FORMAT_RAW_PLUS: /* FIXME: the same as _RAW ? */
		buftypes[1] = status.jpeg_quality + 1;
		jpegres[1] = status.jpeg_resolution;
		mimes[1] = GP_MIME_JPEG;
		sprintf (path->name, "capt%04d.jpg", camera->pl->capcnt);
		fns[1] = strdup(path->name);
		lastfn = strdup (fns[1]);
		nrofdownloads = 2;
		/* FALLTHROUGH */
	case PSLR_IMAGE_FORMAT_RAW:
		jpegres[0] = 0;
		switch (status.raw_format) {
		case PSLR_RAW_FORMAT_PEF:
			sprintf (path->name, "capt%04d.pef", camera->pl->capcnt++);
			fns[0] = strdup (path->name);
			mimes[0] = GP_MIME_RAW;
			buftypes[0] = PSLR_BUF_PEF;
			break;
		case PSLR_RAW_FORMAT_DNG:
			sprintf (path->name, "capt%04d.dng", camera->pl->capcnt++);
			fns[0] = strdup (path->name);
			mimes[0] = "image/x-adobe-dng";
			buftypes[0] = PSLR_BUF_DNG;
			break;
		default:
			gp_log (GP_LOG_ERROR, "pentax", "unknown format image=0x%x, raw=0x%x", status.image_format, status.raw_format);
			return GP_ERROR;
		}
		break;
	default:
		gp_log (GP_LOG_ERROR, "pentax", "unknown format image=0x%x (raw=0x%x)", status.image_format, status.raw_format);
		return GP_ERROR;
	}
	/* get status again, also waits until not busy */
	pslr_get_status (p, &status);

	gettimeofday (&event_start, 0);
	while (1) {
		if (status.bufmask != 0)
			break;
		if (_timeout_passed (&event_start, 30*1000))
			break;
		usleep(100*1000); /* 100 ms */
		pslr_get_status (p, &status);
	}
	if (status.bufmask == 0) {
		gp_log (GP_LOG_ERROR, "pentax", "no buffer available for download after 30 seconds.");
		free (lastfn);
		return GP_ERROR;
	}
	for (bufno=0;bufno<16;bufno++)
		if (status.bufmask & (1 << bufno))
			break;
	/* we will have found one if bufmask != 0 */

	for (i = 0; i<nrofdownloads; i++ ) {
		ret = gp_file_new(&file);
		if (ret!=GP_OK) return ret;
		gp_file_set_mtime (file, time(NULL));
		gp_file_set_mime_type (file, mimes[i]);

		while (1) {
			length = save_buffer( p, bufno, buftypes[i], jpegres[i], file);
			if (length == GP_ERROR_NOT_SUPPORTED) return length;
			if (length >= GP_OK)
				break;
			usleep(100000);
		}

		gp_log (GP_LOG_DEBUG, "pentax", "append image to fs");
		ret = gp_filesystem_append(camera->fs, path->folder, fns[i], context);
		if (ret != GP_OK) {
			gp_file_free (file);
			return ret;
		}
		gp_log (GP_LOG_DEBUG, "pentax", "adding filedata to fs");
		ret = gp_filesystem_set_file_noop(camera->fs, path->folder, fns[i], GP_FILE_TYPE_NORMAL, file, context);
		if (ret != GP_OK) {
			gp_file_free (file);
			return ret;
		}
		/* We have now handed over the file, disclaim responsibility by unref. */
		gp_file_unref (file);

		memset (&info, 0, sizeof (info));
		/* we also get the fs info for free, so just set it */
		info.file.fields = GP_FILE_INFO_TYPE |
				GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
		strcpy (info.file.type, GP_MIME_JPEG);
		info.file.size		= length;
		info.file.mtime		= time(NULL);

		info.preview.fields = 0;
		gp_log (GP_LOG_DEBUG, "pentax", "setting fileinfo in fs");
		ret = gp_filesystem_set_info_noop(camera->fs, path->folder, fns[i], info, context);
		free (fns[i]);
	}
	camera->pl->lastfn = lastfn;

	pslr_delete_buffer(p, bufno );
	pslr_get_status (&camera->pl->pslr, &status);	/* wait until busy is gone */

	return ret;
}

static int
camera_trigger_capture (Camera *camera, GPContext *context)
{
	pslr_handle_t		p = &camera->pl->pslr;
	pslr_status		status;

	gp_log (GP_LOG_DEBUG, "pentax", "camera_trigger_capture");

	pslr_get_status (p, &status);
	pslr_shutter (p);
	/* get status again, also waits until not busy */
	pslr_get_status (p, &status);

	return GP_OK;
}


static int
camera_wait_for_event (Camera *camera, int timeout,
                       CameraEventType *eventtype, void **eventdata,
                       GPContext *context) {
        struct timeval  event_start;
	CameraFilePath	*path;
	pslr_handle_t	p = &camera->pl->pslr;
	int		ret, length;
	CameraFile	*file = NULL;
	CameraFileInfo	info;

	gp_log (GP_LOG_DEBUG, "pentax", "camera_wait_for_event %d ms", timeout);

	*eventtype = GP_EVENT_TIMEOUT;
	*eventdata = NULL;

	if (camera->pl->lastfn) {
		path = malloc(sizeof(CameraFilePath));
		strcpy (path->folder, "/");
		strcpy (path->name, camera->pl->lastfn);
		free (camera->pl->lastfn);
		camera->pl->lastfn = NULL;
		*eventtype = GP_EVENT_FILE_ADDED;
		*eventdata = path;
		return GP_OK;
	}

	gettimeofday (&event_start, 0);
	while (1) {
		pslr_status	status;
		int 		i, bufno;
		const char	*mimes[2];
		int		buftypes[2], jpegres[2], nrofdownloads = 1;
		char		*fns[2];

		if (PSLR_OK != pslr_get_status (&camera->pl->pslr, &status))
			break;

		if (status.bufmask == 0)
			goto next;
		gp_log (GP_LOG_ERROR, "pentax", "wait_for_event: new image found! mask 0x%x", status.bufmask);
		/* New image on camera! */
		for (bufno=0;bufno<16;bufno++)
			if (status.bufmask & (1<<bufno))
				break;
		if (bufno == 16) goto next;

		path = malloc(sizeof(CameraFilePath));
		strcpy (path->folder, "/");
		gp_log (GP_LOG_ERROR, "pentax", "wait_for_event: imageformat %d / rawformat %d", status.image_format, status.raw_format);
		switch (status.image_format) {
		case PSLR_IMAGE_FORMAT_JPEG:
			sprintf (path->name, "capt%04d.jpg", camera->pl->capcnt++);
			mimes[0] = GP_MIME_JPEG;
			buftypes[0] = status.jpeg_quality + 1;
			jpegres[0] = status.jpeg_resolution;
			fns[0] = strdup (path->name);
			break;
		case PSLR_IMAGE_FORMAT_RAW_PLUS: /* FIXME: the same as _RAW ? */
			mimes[1] = GP_MIME_JPEG;
			buftypes[1] = status.jpeg_quality + 1;
			jpegres[1] = status.jpeg_resolution;
			nrofdownloads = 2;
			sprintf (path->name, "capt%04d.jpg", camera->pl->capcnt);
			fns[1] = strdup (path->name);
			/* we pass back the secondary file via the private struct, and return the first */
			camera->pl->lastfn = strdup (fns[1]);
			/* FALLTHROUGH */
		case PSLR_IMAGE_FORMAT_RAW:
			jpegres[0] = 0;
			switch (status.raw_format) {
			case PSLR_RAW_FORMAT_PEF:
				sprintf (path->name, "capt%04d.pef", camera->pl->capcnt++);
				fns[0] = strdup (path->name);
				mimes[0] = GP_MIME_RAW;
				buftypes[0] = PSLR_BUF_PEF;
				break;
			case PSLR_RAW_FORMAT_DNG:
				sprintf (path->name, "capt%04d.dng", camera->pl->capcnt++);
				fns[0] = strdup (path->name);
				mimes[0] = "image/x-adobe-dng";
				buftypes[0] = PSLR_BUF_DNG;
				break;
			default:
				gp_log (GP_LOG_ERROR, "pentax", "unknown format image=0x%x, raw=0x%x", status.image_format, status.raw_format);
				return GP_ERROR;
			}
			break;
		default:
			gp_log (GP_LOG_ERROR, "pentax", "unknown format image=0x%x (raw=0x%x)", status.image_format, status.raw_format);
			return GP_ERROR;
		}

		for (i = 0; i < nrofdownloads; i++) {
			ret = gp_file_new(&file);
			if (ret!=GP_OK) return ret;
			gp_file_set_mtime (file, time(NULL));
			gp_file_set_mime_type (file, mimes[i]);

			while (1) {
				length = save_buffer( p, bufno, buftypes[i], jpegres[i], file);
				if (length == GP_ERROR_NOT_SUPPORTED) return length;
				if (length >= GP_OK)
					break;
				usleep(100000);
			}


			gp_log (GP_LOG_DEBUG, "pentax", "append image to fs");
			ret = gp_filesystem_append(camera->fs, path->folder, fns[i], context);
			if (ret != GP_OK) {
				gp_file_free (file);
				return ret;
			}
			gp_log (GP_LOG_DEBUG, "pentax", "adding filedata to fs");
			ret = gp_filesystem_set_file_noop(camera->fs, path->folder, fns[i], GP_FILE_TYPE_NORMAL, file, context);
			if (ret != GP_OK) {
				gp_file_free (file);
				return ret;
			}
			/* We have now handed over the file, disclaim responsibility by unref. */
			gp_file_unref (file);

			memset (&info, 0, sizeof (info));
			/* we also get the fs info for free, so just set it */
			info.file.fields = GP_FILE_INFO_TYPE |
					GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME;
			strcpy (info.file.type, GP_MIME_JPEG);
			info.file.size		= length;
			info.file.mtime		= time(NULL);

			info.preview.fields = 0;
			gp_log (GP_LOG_DEBUG, "pentax", "setting fileinfo in fs");
			ret = gp_filesystem_set_info_noop(camera->fs, path->folder, fns[i], info, context);
			free (fns[i]);
		}
		pslr_delete_buffer (p, bufno);
		pslr_get_status (&camera->pl->pslr, &status);	/* wait until busy is gone */

		*eventtype = GP_EVENT_FILE_ADDED;
		*eventdata = path;
		return GP_OK;

next:
		if (_timeout_passed (&event_start, timeout))
			break;
		usleep(100*1000); /* 100 ms */
	}
	return GP_OK;
}

static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget	*t, *section;
	const char	*model;
	pslr_status	status;
	char		buf[20];
	int		*available_resolutions;
	int		i, ival;
	float		fval;

	pslr_get_status (&camera->pl->pslr, &status);

	model = pslr_get_camera_name (&camera->pl->pslr);

	available_resolutions = pslr_get_model_jpeg_resolutions (&camera->pl->pslr);

	gp_log (GP_LOG_DEBUG, "pentax", "*** camera_get_config");

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera and Driver Configuration"), window);
	gp_widget_set_name (*window, "main");

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_set_name (section, "settings");
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TEXT, _("Model"), &t);
	gp_widget_set_name (t, "model");
	gp_widget_set_value (t, model);
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_RADIO, _("Image Format"), &t);
	gp_widget_set_name (t, "imageformat");
	/* it seems the upstream code seems to have it always available? */
	gp_widget_add_choice (t, "JPEG");
	gp_widget_add_choice (t, "RAW");
	gp_widget_add_choice (t, "RAW+JPEG");
	switch (status.image_format) {
		case PSLR_IMAGE_FORMAT_JPEG: gp_widget_set_value (t, "JPEG"); break;
		case PSLR_IMAGE_FORMAT_RAW: gp_widget_set_value (t, "RAW"); break;
		case PSLR_IMAGE_FORMAT_RAW_PLUS: gp_widget_set_value (t, "RAW+"); break;
		default:
			sprintf(buf, _("Unknown format %d"), status.image_format);
			gp_widget_set_value (t, buf);
		break;
	}
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_RADIO, _("Image Size"), &t);
	gp_widget_set_name (t, "imgsize");
	for (i = 0; i < MAX_RESOLUTION_SIZE && available_resolutions[i]; i++)
	{
		char buf[20];

		sprintf(buf,"%d", available_resolutions[i]);
		gp_widget_add_choice (t, buf);
	}

	if (status.jpeg_resolution > 0 && status.jpeg_resolution < MAX_RESOLUTION_SIZE) {
		char buf[20];

		sprintf(buf, "%d", status.jpeg_resolution);
		gp_widget_set_value (t, buf);
	} else
		gp_widget_set_value (t, _("Unknown"));

	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_RADIO, _("Image Quality"), &t);
	gp_widget_set_name (t, "imagequality");
	gp_widget_add_choice (t, "4");
	gp_widget_add_choice (t, "3");
	gp_widget_add_choice (t, "2");
	gp_widget_add_choice (t, "1");
	sprintf (buf,"%d",status.jpeg_quality);
	gp_widget_set_value (t, buf);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_RADIO, _("ISO"), &t);
	gp_widget_set_name (t, "iso");
	gp_widget_add_choice (t, "100");
	gp_widget_add_choice (t, "200");
	gp_widget_add_choice (t, "400");
	gp_widget_add_choice (t, "800");
	gp_widget_add_choice (t, "1600");
	gp_widget_add_choice (t, "3200");
	sprintf(buf,"%d",status.current_iso);
	gp_widget_set_value (t, buf);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Shutter Speed"), &t);
	gp_widget_set_name (t, "shutterspeed");
	sprintf(buf,"%d/%d",status.current_shutter_speed.nom,status.current_shutter_speed.denom);
	gp_widget_set_value (t, buf);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Aperture"), &t);
	gp_widget_set_name (t, "aperture");
	if (status.current_aperture.denom == 1) {
		sprintf(buf,"%d",status.current_aperture.nom);
	} else if (status.current_aperture.denom == 10) {
		if (status.current_aperture.nom % 10 == 0)
			sprintf(buf,"%d",status.current_aperture.nom/10);
		else
			sprintf(buf,"%d.%d",status.current_aperture.nom/10,status.current_aperture.nom%10);
	} else {
		sprintf(buf,"%d/%d",status.current_aperture.nom,status.current_aperture.denom);
	}
	gp_widget_set_value (t, buf);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Aperture at Lens Minimum Focal Length"), &t);
	gp_widget_set_name (t, "apertureatminfocallength");
	if (status.lens_min_aperture.denom == 1) {
		sprintf(buf,"%d",status.lens_min_aperture.nom);
	} else if (status.lens_min_aperture.denom == 10) {
		if (status.lens_min_aperture.nom % 10 == 0)
			sprintf(buf,"%d",status.lens_min_aperture.nom/10);
		else
			sprintf(buf,"%d.%d",status.lens_min_aperture.nom/10,status.lens_min_aperture.nom%10);
	} else {
		sprintf(buf,"%d/%d",status.lens_min_aperture.nom,status.lens_min_aperture.denom);
	}
	gp_widget_set_value (t, buf);
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Aperture at Lens Maximum Focal Length"), &t);
	gp_widget_set_name (t, "apertureatmaxfocallength");
	if (status.lens_max_aperture.denom == 1) {
		sprintf(buf,"%d",status.lens_max_aperture.nom);
	} else if (status.lens_max_aperture.denom == 10) {
		if (status.lens_max_aperture.nom % 10 == 0)
			sprintf(buf,"%d",status.lens_max_aperture.nom/10);
		else
			sprintf(buf,"%d.%d",status.lens_max_aperture.nom/10,status.lens_max_aperture.nom%10);
	} else {
		sprintf(buf,"%d/%d",status.lens_max_aperture.nom,status.lens_max_aperture.denom);
	}
	gp_widget_set_value (t, buf);
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Zoom"), &t);
	gp_widget_set_name (t, "zoom");
	sprintf(buf,"%d/%d",status.zoom.nom,status.zoom.denom);
	gp_widget_set_value (t, buf);
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_RANGE, _("Exposure Compensation"), &t);
	gp_widget_set_name (t, "exposurecompensation");
	fval = 1.0*status.ec.nom/status.ec.denom;
	gp_widget_set_range (t, -3.0, 3.0, (status.custom_ev_steps == PSLR_CUSTOM_EV_STEPS_1_2)? 0.5:0.333333);
	gp_widget_set_value (t, &fval);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_RADIO, _("Shooting Mode"), &t);
	gp_widget_set_name (t, "shootingmode");
	gp_widget_add_choice (t, _("GREEN"));
	gp_widget_add_choice (t, _("P"));
	gp_widget_add_choice (t, _("SV"));
	gp_widget_add_choice (t, _("TV"));
	gp_widget_add_choice (t, _("AV"));
	gp_widget_add_choice (t, _("TAV"));
	gp_widget_add_choice (t, _("M"));
	gp_widget_add_choice (t, _("B"));
	gp_widget_add_choice (t, _("X"));
	switch (status.exposure_mode) {
	case PSLR_EXPOSURE_MODE_GREEN:	gp_widget_set_value (t, _("GREEN"));break;
	case PSLR_EXPOSURE_MODE_M:		gp_widget_set_value (t, _("M"));break;
	case PSLR_EXPOSURE_MODE_P:		gp_widget_set_value (t, _("P"));break;
	case PSLR_EXPOSURE_MODE_AV:		gp_widget_set_value (t, _("AV"));break;
	case PSLR_EXPOSURE_MODE_TV:		gp_widget_set_value (t, _("TV"));break;
	case PSLR_EXPOSURE_MODE_SV:		gp_widget_set_value (t, _("SV"));break;
	case PSLR_EXPOSURE_MODE_TAV:	gp_widget_set_value (t, _("TAV"));break;
	case PSLR_EXPOSURE_MODE_X:		gp_widget_set_value (t, _("X"));break;
	case PSLR_EXPOSURE_MODE_B:		gp_widget_set_value (t, _("B"));break;
	default:
		sprintf(buf, _("Unknown mode %d"), status.exposure_mode);
		gp_widget_set_value (t, buf);
		break;
	}
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TOGGLE, _("Bulb"), &t);
	gp_widget_set_name (t, "bulb");
	ival = 2;
	gp_widget_set_value (t, &ival);
	gp_widget_append (section, t);

	return GP_OK;
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget	*w = NULL;
	char		*sval;
	pslr_status	status;
	int		ret;

	pslr_get_status(&camera->pl->pslr, &status);

	gp_log (GP_LOG_DEBUG, "pentax", "*** camera_set_config");
	ret = gp_widget_get_child_by_label (window, _("Image Size"), &w);
	if ((ret == GP_OK) && (gp_widget_changed (w))) {
		int i, resolution = -1;
		int *valid_resolutions;

	        gp_widget_set_changed (w, 0);
		valid_resolutions = pslr_get_model_jpeg_resolutions (&camera->pl->pslr);

		gp_widget_get_value (w, &sval);
		for (i = 0; i < MAX_RESOLUTION_SIZE; i++)
	        {
			int foo;

			sscanf(sval, "%d", &foo);
			if (foo != valid_resolutions[i])
				resolution = i;
		}

		if (resolution == -1) {
			gp_log (GP_LOG_ERROR, "pentax", "Could not decode image size %s", sval);
		} else {
			pslr_set_jpeg_resolution(&camera->pl->pslr, resolution);
			pslr_get_status(&camera->pl->pslr, &status);
		}
	}

	ret = gp_widget_get_child_by_label (window, _("Shooting Mode"), &w);
	if ((ret == GP_OK) && gp_widget_changed (w)) {
		pslr_exposure_mode_t exposuremode;

	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &sval);

		exposuremode = PSLR_EXPOSURE_MODE_MAX;
		if (!strcmp(sval,_("GREEN")))	exposuremode = PSLR_EXPOSURE_MODE_GREEN;
		if (!strcmp(sval,_("M")))	exposuremode = PSLR_EXPOSURE_MODE_M;
		if (!strcmp(sval,_("B")))	exposuremode = PSLR_EXPOSURE_MODE_B;
		if (!strcmp(sval,_("P")))	exposuremode = PSLR_EXPOSURE_MODE_P;
		if (!strcmp(sval,_("SV")))	exposuremode = PSLR_EXPOSURE_MODE_SV;
		if (!strcmp(sval,_("TV")))	exposuremode = PSLR_EXPOSURE_MODE_TV;
		if (!strcmp(sval,_("AV")))	exposuremode = PSLR_EXPOSURE_MODE_AV;
		if (!strcmp(sval,_("TAV")))	exposuremode = PSLR_EXPOSURE_MODE_TAV;
		if (!strcmp(sval,_("X")))	exposuremode = PSLR_EXPOSURE_MODE_TAV;
		if (exposuremode != PSLR_EXPOSURE_MODE_MAX) {
			pslr_set_exposure_mode(&camera->pl->pslr, exposuremode);
			pslr_get_status(&camera->pl->pslr, &status);
		} else {
			gp_log (GP_LOG_ERROR, "pentax", "Could not decode exposuremode %s", sval);
		}
	}

	ret = gp_widget_get_child_by_label (window, _("ISO"), &w);
	if ((ret == GP_OK) && gp_widget_changed (w)) {
		int iso;

	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &sval);
		if (sscanf(sval, "%d", &iso)) {
			/* pslr_set_iso(&camera->pl->pslr, iso); */
			pslr_set_iso(&camera->pl->pslr, iso, 0, 0); /* FIXME: check if 0 is ok */
			pslr_get_status(&camera->pl->pslr, &status);
			// FIXME: K-30 iso doesn't get updated immediately
		} else
			gp_log (GP_LOG_ERROR, "pentax", "Could not decode iso %s", sval);
	}

	gp_widget_get_child_by_label (window, _("Exposure Compensation"), &w);
	if (gp_widget_changed (w)) {
		float		fval;
		pslr_rational_t	rational;

		gp_widget_get_value (w, &fval);
		rational.denom = 10;
		rational.nom = (int)(10 * fval);

		pslr_set_expose_compensation (&camera->pl->pslr, rational);

	}

	gp_widget_get_child_by_label (window, _("Image Quality"), &w);
	if (gp_widget_changed (w)) {
		int qual;

		/* FIXME: decoding is strange. the UI shows number of stars
		 * on k20d: 4 stars = 3, 3 stars = 0, 2 stars = 1, 1 star = 2
		 */
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &sval);
		if (sscanf(sval, "%d", &qual)) {
			pslr_set_jpeg_stars (&camera->pl->pslr, qual);
			pslr_get_status (&camera->pl->pslr, &status);
		} else
			gp_log (GP_LOG_ERROR, "pentax", "Could not decode image quality %s", sval);
	}

	ret = gp_widget_get_child_by_label (window, _("Shutter Speed"), &w);
	if ((ret == GP_OK) && gp_widget_changed (w)) {
		pslr_rational_t speed;

	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &sval);
		if (sscanf(sval, "%d/%d", &speed.nom, &speed.denom)) {
			pslr_set_shutter(&camera->pl->pslr, speed);
			pslr_get_status(&camera->pl->pslr, &status);
		} else {
			char c;
			if (sscanf(sval, "%d%c", &speed.nom, &c) && (c=='s')) {
				speed.denom = 1;
				pslr_set_shutter(&camera->pl->pslr, speed);
				pslr_get_status(&camera->pl->pslr, &status);
			} else {
				gp_log (GP_LOG_ERROR, "pentax", "Could not decode shutterspeed %s", sval);
			}
		}
		/* parse more? */
	}

	ret = gp_widget_get_child_by_label (window, _("Aperture"), &w);
	if ((ret == GP_OK) && gp_widget_changed (w)) {
		pslr_rational_t aperture;
		int apt1,apt2;

	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &sval);
		if (sscanf(sval, "%d.%d", &apt1, &apt2)) {
			if (apt1<11) {
				aperture.nom = apt1*10+apt2;
				aperture.denom = 10;
			} else {
				/* apt2 not in use */
				aperture.nom = apt1;
				aperture.denom = 1;
			}
			pslr_set_aperture(&camera->pl->pslr, aperture);
			pslr_get_status(&camera->pl->pslr, &status);
		} else if (sscanf(sval, "%d", &apt1)) {
			if (apt1<11) {
				aperture.nom = apt1*10;
				aperture.denom = 10;
			} else {
				aperture.nom = apt1;
				aperture.denom = 1;
			}
			pslr_set_aperture(&camera->pl->pslr, aperture);
			pslr_get_status(&camera->pl->pslr, &status);
		} else {
			gp_log (GP_LOG_ERROR, "pentax", "Could not decode aperture %s", sval);
		}
	}

	ret = gp_widget_get_child_by_label (window, _("Bulb"), &w);
	if ((ret == GP_OK) && gp_widget_changed (w)) {
		int bulb;

		if (status.exposure_mode != PSLR_EXPOSURE_MODE_B) {
			gp_context_error (context, _("You need to switch the shooting mode or the camera to 'B' for bulb exposure."));
			return GP_ERROR;
		}

	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &bulb);
		pslr_bulb (&camera->pl->pslr, bulb);

		if (bulb)	/* also press the shutter */
			pslr_shutter (&camera->pl->pslr);
	}

	return GP_OK;
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	pslr_disconnect (&camera->pl->pslr);
	free (camera->pl);
	return GP_OK;
}

char **
get_drives(int *driveNum) {
	/* should not be called */
	return NULL;
}

pslr_result get_drive_info(char* driveName, FDTYPE* hDevice,
                           char* vendorId, int vendorIdSizeMax,
                           char* productId, int productIdSizeMax) {
	/* should not be called */
	return PSLR_NO_MEMORY;
}


int
camera_init (Camera *camera, GPContext *context)
{
	CameraPrivateLibrary	*cpl;

	cpl = calloc (sizeof (CameraPrivateLibrary), 1);
	/* pslr = pslr_init (model, device); ... but it basically just opens the fd */
	cpl->pslr.fd = camera->port;

	camera->pl = cpl;

	pslr_connect (&cpl->pslr);

	camera->functions->exit = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;
	camera->functions->capture = camera_capture;
	camera->functions->trigger_capture = camera_trigger_capture;
	camera->functions->wait_for_event = camera_wait_for_event;
	return gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
}
