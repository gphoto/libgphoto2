/*

  Copyright (C) 2004 Peter Urbanec <toppy at urbanec.net>
  Copyright (C) 2008 Marcus Meissner

  This file is part of puppy.

  puppy is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  puppy is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with puppy; if not, write to the
  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301  USA

*/
#define _DEFAULT_SOURCE

#include "config.h"

#include <string.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utime.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
# include <iconv.h>
# include <langinfo.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-setting.h>

#include "libgphoto2/i18n.h"

#include "usb_io.h"
#include "tf_bytes.h"

#define PUT 0
#define GET 1

#if 0
static int quiet = 0;
#endif
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
static iconv_t cd_locale_to_latin1;
static iconv_t cd_latin1_to_locale;
#endif

struct _mapnames {
	char *tfname;
	char *lgname;
};
struct _CameraPrivateLibrary {
	struct _mapnames *names;
	int nrofnames;
};

static void
backslash(char *path) {
	char *s = path;

	while ((s = strchr (s, '/')))
		*s='\\';
}

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
static char*
strdup_to_latin1 (const char *str) {
	size_t	ret, srclen, dstlen, ndstlen;
	char	*src, *dst, *dest = NULL;

	ndstlen = strlen(str)*2;
	while (1) {
		srclen = strlen(str)+1;
		src = (char*)str;
		dstlen = ndstlen;
		free (dest);
		dst = dest = calloc (dstlen,1);
		if (!dst) return NULL;
		ret = iconv (cd_locale_to_latin1, &src, &srclen, &dst, &dstlen);
		if ((ret == (size_t)(-1)) && (errno == E2BIG)) {
			ndstlen *= 2;
			continue;
		}
		if (ret == (size_t)(-1)) {
			perror("iconv");
			free (dest);
			dest = NULL;
		}
		break;
	}
	return dest;
}

static char*
strdup_to_locale (char *str) {
	size_t	ret, srclen, dstlen, ndstlen;
	char	*dst, *dest = NULL, *src;

	if (str[0] == 5) /* Special Movie Marker on Topfield */
		str++;
	ndstlen = strlen(str)*2+1;
	while (1) {
		srclen = strlen(str)+1; /* +1 ? */
		src = str;
		dstlen = ndstlen;
		free (dest);
		dst = dest = malloc (dstlen);
		if (!dst) return NULL;
		ret = iconv (cd_latin1_to_locale, &src, &srclen, &dst, &dstlen);
		if ((ret == (size_t)(-1)) && (errno == E2BIG)) {
			ndstlen *= 2;
			continue;
		}
		if (ret == (size_t)(-1)) {
			perror("iconv");
			free (dest);
			return NULL;
		}
		break;
	}
	src= dest;
	while (1) {
		src = strchr (src, '/');
		if (!src) break;
		*src='-';	/* FIXME: find better replacement char */
		src++;
	}
	return dest;
}
#else
# define strdup_to_latin1 strdup
# define strdup_to_locale strdup
#endif

static char*
_convert_and_logname(Camera *camera, char *tfname) {
	int i;
	struct _mapnames *map;

	/* search for it in list */
	for (i=0;i<camera->pl->nrofnames;i++) {
		if (!strcmp(tfname, camera->pl->names[i].tfname))
			return camera->pl->names[i].lgname;
	}
	camera->pl->names = realloc (camera->pl->names, (camera->pl->nrofnames+1)*sizeof(camera->pl->names[0]));
	map = &camera->pl->names[camera->pl->nrofnames];
	map->tfname = strdup(tfname);
	map->lgname = strdup_to_locale(tfname);
	camera->pl->nrofnames++;
	return map->lgname;
}

static char*
_convert_for_device(Camera *camera, const char *lgname) {
	int i;

	/* search for it in list */
	for (i=0;i<camera->pl->nrofnames;i++) {
		if (!strcmp(lgname, camera->pl->names[i].lgname))
			return camera->pl->names[i].tfname;
	}
	/* should be here ... */
	return NULL;
}



static char *
get_path (Camera *camera, const char *folder, const char *filename) {
	char 	*path;
	char	*xfolder, *xfilename;

	xfolder = strdup_to_latin1 (folder);
	if (!xfolder)
		return NULL;
	xfilename = _convert_for_device (camera, filename);
	if (!xfilename) {
		free (xfolder);
		return NULL;
	}
	path = malloc(strlen(xfolder)+1+strlen(xfilename)+1);
	if (!path) {
		free (xfolder);
		return NULL;
	}
	strcpy (path, xfolder);
	strcat (path, "/");
	backslash (path);
	strcat (path, xfilename);
	free (xfolder);
	free (xfilename);
	return path;
}


static char *
decode_error(struct tf_packet *packet)
{
	uint32_t ecode = get_u32(packet->data);
	switch (ecode) {
	case 1: return "CRC error";
		break;

	case 2:
	case 4: return "Unknown command";
		break;

	case 3: return "Invalid command";
		break;

	case 5: return "Invalid block size";
		break;

	case 6: return "Unknown error while running";
		break;

	case 7: return "Memory is full";
		break;

	default: return "Unknown error or all your base are belong to us";
	}
}

static int
do_cmd_turbo(Camera *camera, char *state, GPContext *context)
{
	int r;
	int turbo_on = atoi(state);
	struct tf_packet reply;
	char buf[1024];

	/* check if we want to use turbo mode. Default is YES */
        if (GP_OK == gp_setting_get("topfield","turbo", buf))
		if (!strcmp(buf,"no"))
			return GP_OK;

	if(0 == strcasecmp("ON", state))
		turbo_on = 1;

	r = send_cmd_turbo(camera, turbo_on, context);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;

	switch (get_u32(&reply.cmd)) {
	case SUCCESS:
		gp_log (GP_LOG_DEBUG, "topfield", "Turbo mode: %s\n", turbo_on ? "ON" : "OFF");
		return GP_OK;
		break;
	case FAIL:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
		break;
	default:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
	}
	return GP_ERROR_IO;
}

#if 0
static int
do_cmd_reset(Camera *camera, GPContext *context)
{
	int r;
	struct tf_packet reply;

	r = send_cmd_reset(camera,context);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;

	switch (get_u32(&reply.cmd)) {
	case SUCCESS:
		gp_log (GP_LOG_DEBUG, "topfield", "TF5000PVRt should now reboot\n");
		return GP_OK;
		break;

	case FAIL:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
		break;

	default:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
		break;
	}
	return GP_ERROR_IO;
}
#endif

static int
do_cmd_ready(Camera *camera, GPContext *context)
{
	int r;
	struct tf_packet reply;

	r = send_cmd_ready(camera,context);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;

	switch (get_u32(&reply.cmd)) {
	case SUCCESS:
		gp_log (GP_LOG_DEBUG, "topfield", "Device reports ready.\n");
		return GP_OK;

	case FAIL:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
		break;

	default:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
		return GP_ERROR_IO;
	}
	return GP_OK;
}

#if 0
static int
do_cancel(Camera *camera, GPContext *context)
{
	int r;
	struct tf_packet reply;

	r = send_cancel(camera,context);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;

	switch (get_u32(&reply.cmd)) {
	case SUCCESS:
		gp_log (GP_LOG_DEBUG, "topfield", "In progress operation cancelled\n");
		return GP_OK;
		break;

	case FAIL:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
		break;

	default:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
		break;
	}
	return GP_ERROR_IO;
}
#endif

static void
decode_dir(Camera *camera, struct tf_packet *p, int listdirs, CameraList *list)
{
	unsigned short count = (get_u16(&p->length) - PACKET_HEAD_SIZE) / sizeof(struct typefile);
	struct typefile *entries = (struct typefile *) p->data;
	int i;
	char *name;

	for(i = 0; i < count; i++) {
		switch (entries[i].filetype) {
		case 1:
			if (listdirs) {
				if (!strcmp ((char*)entries[i].name, ".."))
					continue;
				gp_list_append (list, (char*)entries[i].name, NULL);
			}
			break;
		case 2:
			if (!listdirs) {
				name = _convert_and_logname (camera, (char*)entries[i].name);
				gp_list_append (list, name, NULL);
			}
			break;
		default:
			break;
	}

#if 0
        /* This makes the assumption that the timezone of the Toppy and the system
         * that puppy runs on are the same. Given the limitations on the length of
         * USB cables, this condition is likely to be satisfied. */
        timestamp = tfdt_to_time(&entries[i].stamp);
        printf("%c %20llu %24.24s %s\n", type, get_u64(&entries[i].size),
               ctime(&timestamp), entries[i].name);
#endif
    }
}

static void
decode_and_get_info(Camera *camera, const char *folder, struct tf_packet *p, const char *fn,
		CameraFileInfo *info, GPContext *context)
{
	unsigned short count = (get_u16(&p->length) - PACKET_HEAD_SIZE) / sizeof(struct typefile);
	struct typefile *entries = (struct typefile *) p->data;
	int i;
	char *name;

	for(i = 0; i < count; i++) {
		switch (entries[i].filetype) {
		case 1: break;/*dir*/
		case 2:	/* file */
			name = _convert_and_logname (camera, (char*)entries[i].name);
			if (!strcmp (name, fn)) { /* the wanted current one */
				memset (info, 0, sizeof (*info));
				info->file.fields = GP_FILE_INFO_SIZE|GP_FILE_INFO_MTIME;
				if (strstr (name, ".rec")) {
					info->file.fields |= GP_FILE_INFO_TYPE;
					strcpy (info->file.type, GP_MIME_MPEG);
				}
				info->file.size = get_u64(&entries[i].size);
				info->file.mtime = tfdt_to_time(&entries[i].stamp);
			} else { /* cache the others to avoid further turnarounds */
				CameraFileInfo	xinfo;

				memset (&xinfo, 0, sizeof (xinfo));
				xinfo.file.fields = GP_FILE_INFO_TYPE|GP_FILE_INFO_SIZE|GP_FILE_INFO_MTIME;
				strcpy (xinfo.file.type, GP_MIME_MPEG);
				xinfo.file.size = get_u64(&entries[i].size);
				xinfo.file.mtime = tfdt_to_time(&entries[i].stamp);
				gp_filesystem_append (camera->fs, folder, name, context); /* FIXME: might fail if exist? */
				gp_filesystem_set_info_noop (camera->fs, folder, name, xinfo, context);
			}
			break;
		default:
			break;
	}
#if 0
        /* This makes the assumption that the timezone of the Toppy and the system
         * that puppy runs on are the same. Given the limitations on the length of
         * USB cables, this condition is likely to be satisfied. */
        timestamp = tfdt_to_time(&entries[i].stamp);
        printf("%c %20llu %24.24s %s\n", type, get_u64(&entries[i].size),
               ctime(&timestamp), entries[i].name);
#endif
    }
}

#if 0
static int
do_hdd_rename(Camera *camera, char *srcPath, char *dstPath, GPContext *context)
{
	int r;
	struct tf_packet reply;

	r = send_cmd_hdd_rename(camera, srcPath, dstPath, context);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;
	switch (get_u32(&reply.cmd)) {
	case SUCCESS:
		return GP_OK;
		break;

	case FAIL:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
		break;

	default:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
		break;
	}
	return GP_ERROR_IO;
}

static void progressStats(uint64_t totalSize, uint64_t bytes, time_t startTime)
{
    int delta = time(NULL) - startTime;

    if(quiet)
        return;

    if(delta > 0)
    {
        double rate = bytes / delta;
        int eta = (totalSize - bytes) / rate;

        fprintf(stderr,
                "\r%6.2f%%, %5.2f Mbits/s, %02d:%02d:%02d elapsed, %02d:%02d:%02d remaining",
                100.0 * ((double) (bytes) / (double) totalSize),
                ((bytes * 8.0) / delta) / (1000 * 1000), delta / (60 * 60),
                (delta / 60) % 60, delta % 60, eta / (60 * 60),
                (eta / 60) % 60, eta % 60);
    }
}

static void finalStats(uint64_t bytes, time_t startTime)
{
    int delta = time(NULL) - startTime;

    if(quiet)
        return;

    if(delta > 0)
    {
        fprintf(stderr, "\n%.2f Mbytes in %02d:%02d:%02d (%.2f Mbits/s)\n",
                (double) bytes / (1000.0 * 1000.0),
                delta / (60 * 60), (delta / 60) % 60, delta % 60,
                ((bytes * 8.0) / delta) / (1000.0 * 1000.0));
    }
}
#endif

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context)
{
        CameraWidget *section, *turbo;
	char buf[1024];
	int val;

        gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);
        gp_widget_set_name (*window, "config");

        gp_widget_new (GP_WIDGET_SECTION, _("Driver Settings"), &section);
	gp_widget_set_name (section, "driver");
        gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_RADIO, _("Turbo mode"), &turbo);
        gp_widget_set_name (turbo, "turbo");
        gp_widget_add_choice (turbo,_("On"));
        gp_widget_add_choice (turbo,_("Off"));
        gp_widget_append (section, turbo);

        if (GP_OK == gp_setting_get("topfield","turbo", buf)) {
		if (!strcmp(buf,"no"))
			val = 0;
		else
			val = 1;
	} else {
		val = 1; /* enabled by default */
	}
        gp_widget_set_value ( turbo, val?_("On"):_("Off"));
	return GP_OK;
}


static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context)
{
	int ret;
	CameraWidget *turbo;

	ret = gp_widget_get_child_by_name (window, "turbo", &turbo);
	if (ret != GP_OK) {
		gp_log (GP_LOG_ERROR, "camera_config_set", "did not find turbo menu entry?\n");
		return GP_OK;
	}
	if (gp_widget_changed (turbo)) {
		const char* val;
		int ival;

	        gp_widget_set_changed (turbo, FALSE);
		ret = gp_widget_get_value (turbo, &val);
		if (ret == GP_OK) {
			ival = !strcmp (val, _("On"));
			gp_log (GP_LOG_DEBUG, "camera_config_set", "val %s, ival %d\n", val, ival);
			gp_setting_set ("topfield", "turbo", ival?"yes":"no");
		}
	}
	return GP_OK;
}


static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	return GP_OK;
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Topfield TF5000PVR\n"
			       "Marcus Meissner <marcus@jet.franken.de>\n"
			       "Library to download / upload files from a Topfield PVR.\n"
			       "Ported from puppy (c) Peter Urbanec <toppy at urbanec.net>\n"
			       ));
	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int result = GP_ERROR_IO;
	enum {
		START,
		DATA,
		ABORT
	} state;
	int r, pid = 0, update = 0;
	uint64_t byteCount = 0;
	struct utimbuf mod_utime_buf = { 0, 0 };
	char *path;
	struct tf_packet reply;

	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_NOT_SUPPORTED;

	do_cmd_turbo (camera, "ON", context);

	path = get_path(camera, folder, filename);
	r = send_cmd_hdd_file_send(camera, GET, path, context);
	free (path);
	if(r < 0)
		goto out;

	state = START;
	while(0 < (r = get_tf_packet(camera, &reply, context)))
	{
		update = (update + 1) % 4;
		switch (get_u32(&reply.cmd)) {
		case DATA_HDD_FILE_START:
			if(state == START) {
				struct typefile *tf = (struct typefile *) reply.data;

				byteCount = get_u64(&tf->size);
				pid = gp_context_progress_start (context, byteCount, _("Downloading %s..."), filename);
				mod_utime_buf.actime = mod_utime_buf.modtime =
				tfdt_to_time(&tf->stamp);

				send_success(camera,context);
				state = DATA;
			} else {
				gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unexpected DATA_HDD_FILE_START packet in state %d\n", state);
				send_cancel(camera,context);
				state = ABORT;
			}
			break;

		case DATA_HDD_FILE_DATA:
			if(state == DATA) {
				uint64_t offset = get_u64(reply.data);
				uint16_t dataLen = get_u16(&reply.length) - (PACKET_HEAD_SIZE + 8);
				int w;

				if (!update) { /* avoid doing it too often */
					gp_context_progress_update (context, pid, offset + dataLen);
					if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
						send_cancel(camera,context);
						state = ABORT;
					}
				}

				if(r < get_u16(&reply.length)) {
					gp_log (GP_LOG_ERROR, "topfield", "ERROR: Short packet %d instead of %d\n", r, get_u16(&reply.length));
					/* TODO: Fetch the rest of the packet */
				}

				w = gp_file_append (file, (char*)&reply.data[8], dataLen);

				if(w < GP_OK) {
					/* Can't write data - abort transfer */
					gp_log (GP_LOG_ERROR, "topfield", "ERROR: Can not write data: %d\n", w);
					send_cancel(camera,context);
					state = ABORT;
				}
			} else {
				gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unexpected DATA_HDD_FILE_DATA packet in state %d\n", state);
				send_cancel(camera,context);
				state = ABORT;
			}
			break;

		case DATA_HDD_FILE_END:
			send_success(camera,context);
			result = GP_OK;
			goto out;
			break;

		case FAIL:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
			send_cancel(camera,context);
			state = ABORT;
			break;

		case SUCCESS:
			goto out;
			break;

		default:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet (cmd 0x%x)\n", get_u32(&reply.cmd));
			break;
		}
	}
	if (pid) gp_context_progress_stop (context, pid);
out:
	do_cmd_turbo (camera, "OFF", context);
	return result;
}

#if 0
static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *filename, CameraFile *file,
	       void *data, GPContext *context)
{
	Camera *camera = data;

	/*
	 * Upload the file to the camera. Use gp_file_get_data_and_size etc.
	 */
	int result = -EPROTO;
	time_t startTime = time(NULL);
	enum {
		START,
		DATA,
		END,
		FINISHED
	} state;
	int src = -1;
	int r;
	int update = 0;
	struct stat srcStat;
	__u64 fileSize;
	__u64 byteCount = 0;
	const char *filename;
	char *path;
	struct tf_packet reply;

	if(0 != fstat(src, &srcStat))
	{
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Can not examine source file: %s\n",
			strerror(errno));
		result = errno;
		goto out;
	}

	fileSize = srcStat.st_size;
	if(fileSize == 0)
	{
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Source file is empty - not transferring.\n");
		result = -ENODATA;
		goto out;
	}

	path = get_path (camera, folder, filename);
	r = send_cmd_hdd_file_send(camera, PUT, path, context);
	if(r < 0)
		goto out;

	state = START;
	while(0 < get_tf_packet(camera, &reply, context)) {
		update = (update + 1) % 16;
		switch (get_u32(&reply.cmd)) {
		case SUCCESS:
			switch (state) {
			case START: {
				/* Send start */
				struct typefile *tf = (struct typefile *) packet.data;

				put_u16(&packet.length, PACKET_HEAD_SIZE + 114);
				put_u32(&packet.cmd, DATA_HDD_FILE_START);
				time_to_tfdt(srcStat.st_mtime, &tf->stamp);
				tf->filetype = 2;
				put_u64(&tf->size, srcStat.st_size);
				strncpy((char *) tf->name, path, 94);
				tf->name[94] = '\0';
				tf->unused = 0;
				tf->attrib = 0;
				gp_log (GP_LOG_DEBUG, "topfield", "%s: DATA_HDD_FILE_START\n", __func__);
				r = send_tf_packet(camera, &packet, context);
				if(r < 0)
				{
					gp_log (GP_LOG_ERROR, "topfield", "ERROR: Incomplete send.\n");
					goto out;
				}
				state = DATA;
				break;
			}

			case DATA: {
				int payloadSize = sizeof(packet.data) - 9;
				ssize_t w = read(src, &packet.data[8], payloadSize);

				/* Detect a Topfield protocol bug and prevent the sending of packets
				   that are a multiple of 512 bytes. */
				if((w > 4) && (((((PACKET_HEAD_SIZE + 8 + w) + 1) & ~1) % 0x200) == 0)) {
					lseek(src, -4, SEEK_CUR);
					w -= 4;
					payloadSize -= 4;
				}

				put_u16(&packet.length, PACKET_HEAD_SIZE + 8 + w);
				put_u32(&packet.cmd, DATA_HDD_FILE_DATA);
				put_u64(packet.data, byteCount);
				byteCount += w;

				/* Detect EOF and transition to END */
				if((w < 0) || (byteCount >= fileSize)) {
					state = END;
				}

				if(w > 0) {
					gp_log (GP_LOG_DEBUG, "topfield", "%s: DATA_HDD_FILE_DATA\n", __func__);
					r = send_tf_packet(camera, &packet, context);
					if(r < w) {
						gp_log (GP_LOG_ERROR, "topfield", "ERROR: Incomplete send.\n");
						goto out;
					}
				}

				if(!update && !quiet) {
					progressStats(fileSize, byteCount, startTime);
				}
				break;
			}

			case END:
				/* Send end */
				put_u16(&packet.length, PACKET_HEAD_SIZE);
				put_u32(&packet.cmd, DATA_HDD_FILE_END);
				gp_log (GP_LOG_DEBUG, "topfield", "%s: DATA_HDD_FILE_END\n", __func__);
				r = send_tf_packet(camera, &packet, context);
				if(r < 0) {
					gp_log (GP_LOG_ERROR, "topfield", "ERROR: Incomplete send.\n");
					goto out;
				}
				state = FINISHED;
				break;

			case FINISHED:
				result = 0;
				goto out;
				break;
			}
			break;

		case FAIL:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
			goto out;
			break;

		default:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet (%d)\n", get_u32(&reply.cmd));
			break;
		}
	}
	finalStats(byteCount, startTime);
out:
	close(src);
	return result;
}
#endif


static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int	r;
	char	*path = get_path (camera, folder, filename);
	struct tf_packet reply;

	r = send_cmd_hdd_del(camera, path, context);
	free (path);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;
	switch (get_u32(&reply.cmd)) {
	case SUCCESS:
		return GP_OK;
		break;

	case FAIL:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
		break;
	default:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
		break;
	}
	return GP_ERROR_IO;
}


#if 0
static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	/*Camera *camera = data;*/

	/*
	 * Delete all files in the given folder. If your camera doesn't have
	 * such a functionality, just don't implement this function.
	 */

	return GP_OK;
}
#endif


static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int r;
	struct tf_packet reply;
	char *xfolder = strdup (folder);

	backslash (xfolder);

	r = send_cmd_hdd_dir(camera, xfolder, context);
	free (xfolder);
	if(r < GP_OK)
		return r;

	while(0 < get_tf_packet(camera, &reply, context)) {
		switch (get_u32(&reply.cmd)) {
		case DATA_HDD_DIR:
			decode_and_get_info(camera, folder, &reply, filename, info, context);
			send_success(camera,context);
			break;
		case DATA_HDD_DIR_END:
			return GP_OK;
			break;
		case FAIL:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n",
				decode_error(&reply));
			return GP_ERROR_IO;
			break;
		default:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
			return GP_ERROR_IO;
		}
	}
	return GP_OK;
}


#if 0
static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	/*Camera *camera = data;*/

	/* Set the file info here from <info> */

	return GP_OK;
}
#endif

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	Camera *camera = data;
	int r;
	struct tf_packet reply;
	char *xfolder = strdup (folder);

	backslash (xfolder);
	r = send_cmd_hdd_dir(camera, xfolder, context);
	free (xfolder);
	if(r < GP_OK)
		return r;

	while(0 < get_tf_packet (camera, &reply, context)) {
		switch (get_u32 (&reply.cmd)) {
		case DATA_HDD_DIR:
			decode_dir (camera, &reply, 1, list);
			send_success (camera,context);
			break;
		case DATA_HDD_DIR_END:
			return GP_OK;
			break;
		case FAIL:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n",
				decode_error(&reply));
			return GP_ERROR_IO;
			break;
		default:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
			return GP_ERROR_IO;
		}
	}
	return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int r;
	struct tf_packet reply;
	char *xfolder = strdup (folder);

	backslash (xfolder);

	r = send_cmd_hdd_dir(camera, xfolder, context);
	free (xfolder);
	if(r < GP_OK)
		return r;

	while(0 < get_tf_packet (camera, &reply, context)) {
		switch (get_u32 (&reply.cmd)) {
		case DATA_HDD_DIR:
			decode_dir (camera, &reply, 0, list);
			send_success (camera,context);
			break;
		case DATA_HDD_DIR_END:
			return GP_OK;
			break;
		case FAIL:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n",
				decode_error(&reply));
			return GP_ERROR_IO;
			break;
		default:
			gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
			return GP_ERROR_IO;
		}
	}
	return GP_OK;
}

static int
storage_info_func (CameraFilesystem *fs,
		CameraStorageInformation **storageinformations,
		int *nrofstorageinformations, void *data,
		GPContext *context)
{
	Camera *camera = data;
	int r;
	struct tf_packet reply;
	CameraStorageInformation	*sif;

	/* List your storages here */
	gp_log (GP_LOG_ERROR, "topfield", __func__);

	r = send_cmd_hdd_size(camera,context);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;

	switch (get_u32(&reply.cmd)) {
	case DATA_HDD_SIZE: {
		unsigned int totalk = get_u32(&reply.data);
		unsigned int freek = get_u32(&reply.data[4]);

		sif = *storageinformations = calloc(sizeof(CameraStorageInformation),1);
		*nrofstorageinformations = 1;

		sif->fields |= GP_STORAGEINFO_BASE;
		strcpy (sif->basedir,"/");
		sif->fields |= GP_STORAGEINFO_STORAGETYPE;
		sif->type = GP_STORAGEINFO_ST_FIXED_RAM;
		sif->fields |= GP_STORAGEINFO_ACCESS;
		sif->access = GP_STORAGEINFO_AC_READWRITE;
		sif->fields |= GP_STORAGEINFO_FILESYSTEMTYPE;
		sif->fstype = GP_STORAGEINFO_FST_GENERICHIERARCHICAL;
                sif->fields |= GP_STORAGEINFO_MAXCAPACITY;
		sif->capacitykbytes = totalk / 1024;
		sif->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
		sif->freekbytes = freek / 1024;
		return GP_OK;
		break;
	}

	case FAIL:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n", decode_error(&reply));
		break;

	default:
		gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
		break;
	}
	return GP_ERROR_IO;
}

static int
make_dir_func (CameraFilesystem *fs, const char *folder, const char *foldername,
               void *data, GPContext *context)
{
        Camera *camera = data;
	char *path = get_path (camera, folder, foldername);
	struct tf_packet reply;
	int r;

	r = send_cmd_hdd_create_dir(camera, path, context);
	free (path);
	if(r < 0)
		return r;

	r = get_tf_packet(camera, &reply, context);
	if(r < 0)
		return r;
	switch (get_u32(&reply.cmd)) {
	case SUCCESS:
	    return GP_OK;
	    break;

	case FAIL:
	    gp_log (GP_LOG_ERROR, "topfield", "ERROR: Device reports %s\n",
		    decode_error(&reply));
	    break;

	default:
	    gp_log (GP_LOG_ERROR, "topfield", "ERROR: Unhandled packet\n");
	}
	return GP_ERROR_IO;
}


/**********************************************************************/
int
camera_id (CameraText *id)
{
	strcpy(id->text, "Topfield 5000 PVR");
	return GP_OK;
}


int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Topfield:TF5000PVR");
	a.status	= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port		= GP_PORT_USB;
	a.usb_vendor	= 0x11db;
	a.usb_product	= 0x1000;
	a.operations        = 	GP_OPERATION_NONE;
	a.file_operations   = 	GP_FILE_OPERATION_DELETE;
	a.folder_operations = 	GP_FOLDER_OPERATION_NONE;
	return gp_abilities_list_append(list, a);
}

CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_info_func = get_info_func,
#if 0
	.set_info_func = set_info_func,
#endif
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
#if 0
	.put_file_func = put_file_func,
	.delete_all_func = delete_all_func,
#endif
	.storage_info_func = storage_info_func,
	.make_dir_func = make_dir_func
};

static int
camera_exit (Camera *camera, GPContext *context)
{
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	iconv_close (cd_latin1_to_locale);
	iconv_close (cd_locale_to_latin1);
#endif
	free (camera->pl->names);
	free (camera->pl);
	return GP_OK;
}

int
camera_init (Camera *camera, GPContext *context)
{
	char *curloc;
	/* First, set up all the function pointers */
	camera->functions->get_config	= camera_config_get;
	camera->functions->set_config	= camera_config_set;
	camera->functions->summary	= camera_summary;
	camera->functions->about	= camera_about;
	camera->functions->exit		= camera_exit;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	gp_port_set_timeout (camera->port, TF_PROTOCOL_TIMEOUT);
	gp_port_usb_clear_halt (camera->port, GP_PORT_USB_ENDPOINT_IN);

	camera->pl = calloc (sizeof (CameraPrivateLibrary),1);
	if (!camera->pl) return GP_ERROR_NO_MEMORY;

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	curloc = nl_langinfo (CODESET);
	if (!curloc) curloc="UTF-8";
	cd_latin1_to_locale = iconv_open(curloc, "iso-8859-1");
	if (!cd_latin1_to_locale) return GP_ERROR_NO_MEMORY;
	cd_locale_to_latin1 = iconv_open("iso-8859-1", curloc);
	if (!cd_locale_to_latin1) return GP_ERROR_NO_MEMORY;
#endif

	do_cmd_ready (camera, context);
	return GP_OK;
}
