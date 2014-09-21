/* g3.c
 *
 * Copyright 2003 Marcus Meissner <marcus@jet.franken.de>
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

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>

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

/* channel header: 
 *
 * channel  host/client  length       data     checksum padding
 * 01 01    00 00        07 00 00 00  <7 byte> <1 byte> <fill to next 4>
 * 
 */

static int
g3_channel_read(GPPort *port, int *channel, char **buffer, int *len)
{
	unsigned char xbuf[0x800];
	int tocopy, ret, curlen;

	ret = gp_port_read(port, (char *)xbuf, 0x800);
	if (ret < GP_OK) { 
		gp_log(GP_LOG_ERROR, "g3", "read error in g3_channel_read\n");
		return ret;
	}

	if ((xbuf[2] != 0xff) && (xbuf[3] != 0xff)) {
		gp_log(GP_LOG_ERROR, "g3" ,"first bytes do not match.\n");
		return GP_ERROR_IO;
	}

	*channel = xbuf[1];
	*len = xbuf[4] + (xbuf[5]<<8) + (xbuf[6]<<16) + (xbuf[7]<<24);
	/* Safety buffer of 0x800 ... we can only read in 0x800 chunks, 
	 * otherwise the communication gets hickups. However *len might be
	 * less.
	 */
	if (!*buffer)
		*buffer = malloc(*len + 1 + 0x800);
	else
		*buffer = realloc(*buffer, *len + 1 + 0x800);
	tocopy = *len;
	if (tocopy > 0x800-8) tocopy = 0x800-8;
	memcpy(*buffer, xbuf+8, tocopy);
	curlen = tocopy;
	while (curlen < *len) {
		ret = gp_port_read(port, *buffer + curlen, 0x800);
		if (ret < GP_OK) {
			gp_log(GP_LOG_ERROR, "g3", "read error in g3_channel_read\n");
			return ret;
		}
		curlen += ret;
	}
	(*buffer)[*len] = 0x00;
	return GP_OK;
}

static int
g3_channel_read_bytes(
	GPPort *port, int *channel, char **buffer, int expected, GPContext *context,
	const char *msg
) {
	unsigned char *xbuf;
	int id, ret, len, xlen = 0;

	if (!*buffer)
		*buffer = malloc (expected);
	else
		*buffer = realloc (*buffer, expected);
	xbuf = malloc(65536 + 12);

	id = gp_context_progress_start (context, expected, "%s", msg);

	/* The camera lets us read in packets of at least max 64kb size. */
	while (expected > 0) {
		int rest = expected;
		if (rest > 65536) rest = 65536;

		rest = (rest + 9 + 3) & ~3;
		if (rest < 0x800) rest = 0x800;

		ret = gp_port_read(port, (char *)xbuf, rest);
		if (ret < GP_OK) {
			gp_log(GP_LOG_ERROR, "g3", "read error in g3_channel_read\n");
			gp_context_progress_stop (context, id);
			free(xbuf);
			return ret;
		}
		if (ret != rest) {
			gp_log(GP_LOG_ERROR, "g3", "read error in g3_channel_read\n");
			gp_context_progress_stop (context, id);
			free(xbuf);
			return ret;
		}

		if ((xbuf[2] != 0xff) || (xbuf[3] != 0xff)) {
			gp_log(GP_LOG_ERROR, "g3", "first bytes do not match.\n");
			gp_context_progress_stop (context, id);
			free(xbuf);
			return GP_ERROR_IO;
		}
		len = xbuf[4] + (xbuf[5]<<8) + (xbuf[6]<<16) + (xbuf[7]<<24);
		*channel = xbuf[1];
		if (len > expected)
			gp_log(GP_LOG_ERROR,"g3","len %d is > rest expected %d\n", len, expected);
		memcpy(*buffer+xlen, xbuf+8, len);
		expected	-= len;
		xlen		+= len;
		gp_context_progress_update (context, id, xlen);
	}
	gp_context_progress_stop (context, id);
	free(xbuf);
	return GP_OK;
}

static int
g3_channel_write(GPPort *port, int channel, char *buffer, int len)
{
	unsigned char *xbuf;
	int ret = GP_OK, nlen, curlen = 0;

	while (len > 0) {
		int sendlen = len;
		
		if (sendlen>65536) sendlen = 65536;
		nlen = (4 + 4 + sendlen + 1 +  3) & ~3;
		xbuf = calloc(nlen,1);

		xbuf[0] = 1;
		xbuf[1] = channel;
		xbuf[2] = 0;
		xbuf[3] = 0;
		xbuf[4] =  sendlen      & 0xff;
		xbuf[5] = (sendlen>>8)  & 0xff;
		xbuf[6] = (sendlen>>16) & 0xff;
		xbuf[7] = (sendlen>>24) & 0xff;
		memcpy(xbuf+8, buffer + curlen, sendlen);
		curlen += sendlen;
		xbuf[8+sendlen] = 0x03;
		ret = gp_port_write( port, (char*)xbuf, nlen);
		free(xbuf);
		if (ret < GP_OK) break;
		len -= sendlen;
	}
	return ret;
}

static int
g3_ftp_command_and_reply(GPPort *port, char *cmd, char **reply) {
	int ret, channel, len;
	char *realcmd, *s;

	realcmd = malloc(strlen(cmd)+2+1);strcpy(realcmd, cmd);strcat(realcmd, "\r\n");

	gp_log(GP_LOG_DEBUG, "g3" , "sending %s", cmd);
	ret = g3_channel_write(port, 1, realcmd, strlen(realcmd));
	free(realcmd);
	if (ret < GP_OK) {
		gp_log (GP_LOG_ERROR, "g3", "ftp command write failed? %d\n", ret);
		return ret;
	}
	ret = g3_channel_read(port, &channel, reply, &len);
	if (ret < GP_OK) {
		gp_log (GP_LOG_ERROR, "g3", "ftp reply read failed? %d\n", ret);
		return ret;
	}
	s = strchr(*reply, '\r');
	if (s) *s='\0';

	gp_log(GP_LOG_DEBUG, "g3" , "reply %s", *reply);
	return GP_OK;
}

int
camera_id (CameraText *id) 
{
	strcpy(id->text, "ricoh_g3");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "Ricoh:Caplio G3");
	a.status	= GP_DRIVER_STATUS_PRODUCTION;
	a.port		= GP_PORT_USB;
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2204;

	a.operations        = 	GP_OPERATION_NONE;
	a.file_operations   = 	GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_EXIF;
	a.folder_operations = 	GP_FOLDER_OPERATION_MAKE_DIR |
				GP_FOLDER_OPERATION_REMOVE_DIR ;

	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio RR30");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2202;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio 300G");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2203;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Medion:MD 6126");
	a.usb_vendor    = 0x5ca;
	a.usb_product   = 0x2205;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio G4");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2208;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Capilo RX");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x220b;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio GX");
	a.usb_vendor    = 0x5ca;
	a.usb_product   = 0x220c;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio R1");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x220d;
	gp_abilities_list_append(list, a);

	/* same id as above */
	strcpy(a.model, "Ricoh:Caplio RZ1");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x220d;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Sea & Sea:5000G");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x220e;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Rollei:dr5");
	a.usb_vendor    = 0x5ca;
	a.usb_product   = 0x220f;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio R1v");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2212;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio R2");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2213;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio GX 8");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2214;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio R3");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2216;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio R4");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x2217;
	gp_abilities_list_append(list, a);

	strcpy(a.model, "Ricoh:Caplio R5");
	a.usb_vendor	= 0x5ca;
	a.usb_product	= 0x221a;
	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static int
g3_cwd_command( GPPort *port, const char *folder) {
	char *cmd, *reply = NULL;
	int ret;

	cmd = malloc(strlen("CWD ") + strlen(folder) + 2 + 1);
	sprintf(cmd,"CWD %s", folder);
	ret = g3_ftp_command_and_reply(port, cmd, &reply);
	free(cmd);
	if (ret < GP_OK) {
		if (reply) free(reply);
		return ret;
	}
	if (reply[0]=='5') /* Failed, most likely no such directory */
		ret = GP_ERROR_DIRECTORY_NOT_FOUND;
	free(reply);
	return ret;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	char *buf = NULL, *reply = NULL, *cmd =NULL, *msg = NULL;
	int ret, channel, bytes, seek, len;

	ret = g3_cwd_command (camera->port, folder);
	if (ret < GP_OK) goto out;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		msg = _("Downloading...");
		if (strstr(filename,"AVI") || strstr(filename,"avi"))
			msg = _("Downloading movie...");
		if (strstr(filename,"jpg") || strstr(filename,"JPG") ||
		    strstr(filename,"tif") || strstr(filename,"TIF")
		)
			msg = _("Downloading image...");
		if (strstr(filename,"wav") || strstr(filename,"WAV"))
			msg = _("Downloading audio...");
		cmd = malloc(strlen("RETR ") + strlen(filename) + 2 + 1);
		sprintf(cmd,"RETR %s", filename);
		ret = g3_ftp_command_and_reply(camera->port, cmd, &buf);
		free(cmd);
		if (ret < GP_OK) goto out;
		if (buf[0] != '1') { /* error, most likely file not found */
			ret = GP_ERROR_FILE_NOT_FOUND;
			goto out;
		}

		bytes = 0;
		sscanf(buf, "150 data connection for RETR.(%d)", &bytes);
		break;
	case GP_FILE_TYPE_EXIF:
		msg = _("Downloading EXIF data...");
		if (!strstr(filename,".JPG") && !strstr(filename,".jpg")) {
			gp_context_error (context,_("No EXIF data available for %s."),
                                          filename);
                        ret = GP_ERROR_FILE_NOT_FOUND;
			goto out;
		}
		cmd = malloc(strlen("-SRET ") + strlen(filename) + 2 + 1);
		sprintf(cmd,"-SRET %s", filename);
		ret = g3_ftp_command_and_reply(camera->port, cmd, &buf);
		free(cmd);
		if (ret < GP_OK) goto out;
		if (buf[0] != '1') { /* error, most likely file not found */
			ret = GP_ERROR_FILE_NOT_FOUND;
			goto out;
		}
		bytes = seek = 0;
		sscanf(buf, "150 %d byte Seek=%d", &bytes, &seek);
		if (seek == -2) {
			/* FIXME: pretty bad, the camera has some time out problems
			 * if this happens */
			gp_context_error (context,_("No EXIF data available for %s."),
                                          filename);
                        ret = GP_ERROR_FILE_NOT_FOUND;
			g3_channel_read(camera->port, &channel, &reply, &len); /* reply */
			goto out;
		}
		bytes += seek;
		break;
	default:
		return GP_ERROR_NOT_SUPPORTED;
	}

	ret = g3_channel_read_bytes(camera->port, &channel, &buf, bytes, context, msg);
	if (ret < GP_OK) goto out;
	ret = g3_channel_read(camera->port, &channel, &reply, &len); /* reply */
	if (ret < GP_OK) goto out;
	gp_log(GP_LOG_DEBUG, "g3" , "reply %s", reply);
	gp_file_set_data_and_size (file, buf, bytes);
	buf = NULL; /* now owned by libgphoto2 filesystem */

out:
	if (buf) free(buf);
	if (reply) free(reply);
	return (GP_OK);
}

#if 0
/* Works to some degree, but the firmware on the camera is not really happy
 * with it and sometimes refuses to send data the correct way
 */
static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *fn, CameraFile *file,
	       void *data, GPContext *context)
{
	Camera *camera = data;
	char *buf = NULL, *reply = NULL, *cmd =NULL;
	const char *imgdata = NULL;
	int ret, channel, len;
	long size;

	ret = g3_cwd_command (camera->port, folder);
	if (ret < GP_OK) goto out;

	ret = gp_file_get_data_and_size (file, &imgdata, &size);
	if (ret < GP_OK) goto out;

	cmd = malloc(strlen("-STOR ") + 20 + strlen(fn) + 2 + 1);
	sprintf(cmd,"-STOR %ld %s", size, fn);
	ret = g3_ftp_command_and_reply(camera->port, cmd, &buf);
	free(cmd);
	if (ret < GP_OK) goto out;
	if (buf[0] != '1') { /* error, most likely file not found */
		ret = GP_ERROR;
		goto out;
	}
	ret = g3_channel_write(camera->port, 2, (char*)imgdata, size); /* data */
	if (ret < GP_OK) goto out;
	ret = g3_channel_read(camera->port, &channel, &reply, &len); /* reply */
	if (ret < GP_OK) goto out;
out:
	if (buf) free(buf);
	if (reply) free(reply);
	return (GP_OK);
}
#endif

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	char *reply = NULL, *cmd = NULL;
	int ret;

	ret = g3_cwd_command (camera->port, folder);
	if (ret < GP_OK)
		return ret;

	cmd = malloc(strlen("DELE ")+strlen(filename)+1);
	if (!cmd) return GP_ERROR_NO_MEMORY;
	sprintf(cmd,"DELE %s",filename);
	ret = g3_ftp_command_and_reply(camera->port, cmd, &reply);
	if (ret < GP_OK)
		goto out;
	if (reply[0] == '5') {
		gp_context_error (context, _("Could not delete file."));
		ret = GP_ERROR;
	}
out:
	free(cmd);
	free(reply);
	return (GP_OK);
}

static int
rmdir_func (CameraFilesystem *fs, const char *folder,
		  const char *name, void *data, GPContext *context)
{
	Camera *camera = data;
	char *reply = NULL, *cmd = NULL;
	int ret;

	ret = g3_cwd_command (camera->port, folder);
	if (ret<GP_OK)
		return ret;

	cmd = realloc(cmd,strlen("RMD ")+strlen(name)+1);
	if (!cmd) return GP_ERROR_NO_MEMORY;
	sprintf(cmd,"RMD %s",name);
	ret = g3_ftp_command_and_reply(camera->port, cmd, &reply);
	if (ret < GP_OK)
		goto out;
	if (reply[0] == '5') {
		gp_context_error (context, _("Could not remove directory."));
		ret = GP_ERROR;
	}
out:
	free(cmd);
	free(reply);
	return (GP_OK);
}

static int
mkdir_func (CameraFilesystem *fs, const char *folder,
	  const char *name, void *data, GPContext *context)
{
	Camera *camera = data;
	char *reply = NULL, *cmd = NULL;
	int ret;

	ret = g3_cwd_command (camera->port, folder);
	if (ret<GP_OK)
		return ret;

	cmd = realloc(cmd,strlen("MKD ")+strlen(name)+1);
	if (!cmd) return GP_ERROR_NO_MEMORY;
	sprintf(cmd,"MKD %s",name);
	ret = g3_ftp_command_and_reply(camera->port, cmd, &reply);
	if (ret < GP_OK)
		goto out;
	if (reply[0] == '5') {
		gp_context_error (context, _("Could not create directory."));
		ret = GP_ERROR;
	}
out:
	free(cmd);
	free(reply);
	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	char *t = summary->text;
	int ret;
	char *buf = NULL;

	t[0]='\0';
	ret = g3_ftp_command_and_reply (camera->port, "-VER", &buf);
	if (ret == GP_OK)
		sprintf(t+strlen(t), _("Version: %s\n"), buf+4); /* skip "200 " */
	ret = g3_ftp_command_and_reply(camera->port, "-RTST", &buf); /* RTC test */
	if (ret == GP_OK) {
		int rtcstat;
		if (sscanf(buf, "200 RTC status=%d", &rtcstat))
			sprintf(t+strlen(t), _("RTC Status: %d\n"), rtcstat);
	}
	ret = g3_ftp_command_and_reply(camera->port, "-TIME", &buf);
	if (ret == GP_OK) {
		char day[20], time[20];
		if (sscanf(buf, "200 %s %s for -TIME", day, time))
			sprintf(t+strlen(t), _("Camera time: %s %s\n"), day, time);
	}
	ret = g3_ftp_command_and_reply(camera->port, "-GCID", &buf);
	if (ret == GP_OK) {
		char camid[40];
		if (sscanf(buf, "200 CameraID=%s for -GCID", camid))
			sprintf(t+strlen(t), _("Camera ID: %s\n"), camid);
	}
	ret = g3_ftp_command_and_reply(camera->port, "-GSID", &buf);
	if (ret == GP_OK) {
		char cardid[40];
		if (strstr(buf, "200 SD ID= for -GSID")) {
			sprintf(t+strlen(t), _("No SD Card inserted.\n"));
		} else {
			if (sscanf(buf, "200 SD ID=%s for -GSID", cardid)) {
				sprintf(t+strlen(t), _("SD Card ID: %s\n"), cardid);
			}
		}
	}
	ret = g3_ftp_command_and_reply(camera->port, "-GTPN", &buf);
	if (ret == GP_OK) {
		int total;
		if (sscanf (buf, "200 TotalPhotoNo=%d for -GTPN", &total))
			sprintf(t+strlen(t), _("Photos on camera: %d\n"), total);
	}
	ret = g3_ftp_command_and_reply(camera->port, "-DCAP /EXT0", &buf);
	if (ret == GP_OK) {
		int space, sfree;
		if (sscanf (buf, "200 /EXT0 capacity %d byte,free %d byte.", &space, &sfree)) {
			sprintf(t+strlen(t), _("SD memory: %d MB total, %d MB free.\n"), space/1024/1024, sfree/1024/1024);
		}
	}
	ret = g3_ftp_command_and_reply(camera->port, "-DCAP /IROM", &buf);
	if (ret == GP_OK) {
		int space, sfree;
		if (sscanf (buf, "200 /IROM capacity %d byte,free %d byte.", &space, &sfree)) {
			sprintf(t+strlen(t), _("Internal memory: %d MB total, %d MB free.\n"), space/1024/1024, sfree/1024/1024);
		}
	}
	free (buf);
	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Ricoh Caplio G3.\n"
			       "Marcus Meissner <marcus@jet.franken.de>\n"
			       "Reverse engineered using USB Snoopy, looking\n"
			       "at the firmware update image and wild guessing.\n"
				));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int ret, bytes, width, height, k;
	struct tm xtm;
	char *cmd = NULL, *reply = NULL;

	info->file.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	strcpy(info->file.type,GP_MIME_UNKNOWN);

	if (!strcmp(filename+9,"JPG") || !strcmp(filename+9,"jpg"))
		strcpy(info->file.type,GP_MIME_JPEG);

	if (!strcmp(filename+9,"AVI") || !strcmp(filename+9,"avi"))
		strcpy(info->file.type,GP_MIME_AVI);

	if (!strcmp(filename+9,"WAV") || !strcmp(filename+9,"wav"))
		strcpy(info->file.type,GP_MIME_WAV);

	if (!strcmp(filename+9,"MTA") || !strcmp(filename+9,"mta"))
		strcpy(info->file.type,"text/plain");

	cmd = malloc(strlen("-FDAT ")+strlen(folder)+1+strlen(filename)+1);
	if (!cmd) return GP_ERROR_NO_MEMORY;
	sprintf(cmd, "-FDAT %s/%s", folder,filename);
	ret = g3_ftp_command_and_reply(camera->port, cmd, &reply);
	if (ret < GP_OK) goto out;
	if (sscanf(reply, "200 date=%d:%d:%d %d:%d:%d",
			&xtm.tm_year,
			&xtm.tm_mon,
			&xtm.tm_mday,
			&xtm.tm_hour,
			&xtm.tm_min,
			&xtm.tm_sec
	)) {
		xtm.tm_mon--; 		/* range 0 - 11 */
		xtm.tm_year -= 1900;	/* number of years since 1900 */
		info->file.mtime = mktime(&xtm);
		info->file.fields |= GP_FILE_INFO_MTIME;
	}

	/* -INFO command only sometimes work on non jpeg/avi files */
	if (	!strcmp(info->file.type,GP_MIME_JPEG) ||
		!strcmp(info->file.type,GP_MIME_AVI)
	) {
		sprintf(cmd, "-INFO %s/%s", folder,filename);
		ret = g3_ftp_command_and_reply(camera->port, cmd, &reply);
		if (ret < GP_OK) goto out;

		/* 200 1330313 byte W=2048 H=1536 K=0 for -INFO */
		if (sscanf(reply, "200 %d byte W=%d H=%d K=%d for -INFO", &bytes, &width, &height , &k)) {
			if (width != 0 && height != 0) {
				info->file.fields |= GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT;
				info->file.height = height;
				info->file.width = width;
			}
			info->file.fields |= GP_FILE_INFO_SIZE;
			info->file.size = bytes;
			if (k != 0)
				gp_log(GP_LOG_ERROR, "g3", "k is %d for %s/%s\n", k, folder,filename);
		}
	}
out:
	free(reply);
	free(cmd);

	return (GP_OK);

}


static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	Camera *camera = data;
	char *buf = NULL, *reply = NULL;
	char *cmd = NULL;
	int ret = GP_OK, channel, len, rlen;

	if (!strcmp("/",folder)) {
		/* Lets hope they dont invent other names. */

		/* We check the folder we get, since we should
		 * not add EXT0 without SD card.
		 */
		ret = g3_ftp_command_and_reply(camera->port, "-NLST /", &buf);
		if (ret < GP_OK) goto out;
		if (buf[0] == '4') {
			/* seen with R3: 450 Open Error... likely OK. */
			goto out;
		}
		if (buf[0] != '1') {
			ret = GP_ERROR_IO;
			goto out;
		}
		ret = g3_channel_read(camera->port, &channel, &buf, &len); /* data. */
		if (ret < GP_OK) goto out;
		ret = g3_channel_read(camera->port, &channel, &reply, &rlen); /* next reply  */
		if (ret < GP_OK) goto out;
		gp_log(GP_LOG_DEBUG, "g3" , "reply %s", reply);

		if (!strcmp("/EXT0",buf))
			gp_list_append (list, "EXT0", NULL);
		
		/* always add internal memory */
		gp_list_append (list, "IROM", NULL);
		return GP_OK;
	}

	cmd = malloc(6 + strlen(folder) + 1);
	strcpy(cmd, "-NLST ");
	strcat(cmd, folder);

	ret = g3_ftp_command_and_reply(camera->port, cmd, &buf);
	free(cmd);cmd = NULL;
	if (ret < GP_OK) goto out;
	if (buf[0] == '1') { /* 1xx means another reply follows */
		int n = 0;

		ret = g3_channel_read(camera->port, &channel, &buf, &len); /* data. */
		if (ret < GP_OK) goto out;
		g3_channel_read(camera->port, &channel, &reply, &rlen); /* next reply  */
		if (ret < GP_OK) goto out;
		gp_log(GP_LOG_DEBUG, "g3" , "reply %s", reply);

		for (n=0;n<len/32;n++) {
			if (buf[n*32+11] == 0x10) {
				if (buf[n*32] == '.') /* skip . and .. entries */
					continue;
				ret = gp_list_append (list, buf+n*32, NULL);
				if (ret != GP_OK) goto out;
			}
		}
	} else {
		if (buf[0] == '4')
			ret = GP_OK;
		else	
			ret = GP_ERROR_IO;
	}
out:
	if (buf) free(buf);
	if (reply) free(reply);
	return ret;
}

/* for DOS FAT -> UNIX time conversion */
static int day_n[] = { 0,31,59,90,120,151,181,212,243,273,304,334,0,0,0,0 };

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	char *buf = NULL, *reply = NULL, *cmd;
	int ret = GP_OK;
	unsigned char *ubuf;

	cmd = malloc(6 + strlen(folder) + 1);
	strcpy(cmd, "-NLST ");
	strcat(cmd, folder);
	ret = g3_ftp_command_and_reply(camera->port, cmd, (char**)&buf);
	free(cmd); cmd = NULL;
	if (ret < GP_OK) goto out;
	if (buf[0] == '1') { /* 1xx means another reply follows */
		int n = 0, channel, len, rlen;
		ret = g3_channel_read(camera->port, &channel, &buf, &len); /* data. */
		if (ret < GP_OK) goto out;
		ret = g3_channel_read(camera->port, &channel, &reply, &rlen); /* next reply  */
		if (ret < GP_OK) goto out;
		gp_log(GP_LOG_DEBUG, "g3" , "reply %s", reply);

		ubuf = (unsigned char*)buf;
		for (n=0;n < len/32;n++) {
			if (buf[n*32+11] == 0x20) {
				CameraFileInfo  info;
				char xfn[13];
				int date, time, month, year;

				strncpy (xfn, buf+n*32, 8);
				xfn[8] = '.';
				strncpy (xfn+9, buf+n*32+8, 3);
				xfn[8+1+3] = '\0';

				ret = gp_filesystem_append (fs, folder, xfn, context);
				if (ret < GP_OK) goto out;

				/* we also get parts of fs info for free, so just set it */
				info.file.fields =
						GP_FILE_INFO_SIZE |
						GP_FILE_INFO_MTIME;
				info.file.size =(ubuf[n*32+28]<<24)|
						(ubuf[n*32+29]<<16)|
						(ubuf[n*32+30]<< 8)|
						(ubuf[n*32+31]    );
				if (!strcmp(xfn+9,"JPG") || !strcmp(xfn+9,"jpg")) {
					strcpy(info.file.type,GP_MIME_JPEG);
					info.file.fields |= GP_FILE_INFO_TYPE;
				}

				if (!strcmp(xfn+9,"AVI") || !strcmp(xfn+9,"avi")) {
					strcpy(info.file.type,GP_MIME_AVI);
					info.file.fields |= GP_FILE_INFO_TYPE;
				}

				if (!strcmp(xfn+9,"WAV") || !strcmp(xfn+9,"wav")) {
					strcpy(info.file.type,GP_MIME_WAV);
					info.file.fields |= GP_FILE_INFO_TYPE;
				}

				if (!strcmp(xfn+9,"MTA") || !strcmp(xfn+9,"mta")) {
					strcpy(info.file.type,"text/plain");
					info.file.fields |= GP_FILE_INFO_TYPE;
				}
				info.preview.fields = 0;
				date = (ubuf[n*32+16]) | (ubuf[n*32+17]<<8);
				time = (ubuf[n*32+14]) | (ubuf[n*32+15]<<8);

				/* from kernel fs/fat/, time_dos2unix. */
        			month = ((date >> 5) - 1) & 15;
				year = date >> 9;
				info.file.mtime =
					(time & 31)*2+60*((time >> 5) & 63)+
					(time >> 11)*3600+86400*((date & 31)-1+
					day_n[month]+(year/4)+year*365-
					((year & 3) == 0 && month < 2 ? 1 : 0)+
					3653);

				ret = gp_filesystem_set_info_noop(fs, folder, xfn, info, context);
 
			}
		}
	} else {
		if (buf[0] == '4') /* 450 Open Error ... like dir not there */
			ret = GP_OK;
		else		
			ret = GP_ERROR_IO;
	}
out:
	if (buf) free(buf);
	if (reply) free(reply);
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.get_info_func = get_info_func,
	.folder_list_func = folder_list_func,
	.make_dir_func = mkdir_func,
	.remove_dir_func = rmdir_func,
};

int
camera_init (Camera *camera, GPContext *context) 
{
	/*char *buf;*/
	GPPortSettings settings;

        /* First, set up all the needed function pointers */
        camera->functions->summary              = camera_summary;
        camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

        gp_port_get_settings(camera->port, &settings);
	settings.usb.inep = 0x81;
	settings.usb.outep = 0x02;
	settings.usb.intep = 0x83;
        gp_port_set_settings(camera->port, settings);
	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */
	
	/*
	 * Once you have configured the port, you should check if a 
	 * connection to the camera can be established.
	 */

	/* testing code ... 
	buf = NULL;
	g3_ftp_command_and_reply(camera->port, "-PWOF STDBY", &buf);
	*/
	return (GP_OK);
}
