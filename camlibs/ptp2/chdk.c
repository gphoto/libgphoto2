/* chdk.c
 *
 * Copyright (C) 2015 Marcus Meissner <marcus@jet.franken.de>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
#include <langinfo.h>
#endif
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "ptp.h"
#include "ptp-bugs.h"
#include "ptp-private.h"
#include "ptp-pack.c"

/* CHDK support */

/* Include the rlibs.lua lua libs or try do it on our own? */

/* capture: 
 * send LUA: 
 * init_usb_capture(0); is exiting capture

 * init_usb_capture(1,0,0)  1 = jpeg, 2 = raw, 4 = switch raw to dng
 *
 * ff_imglist(options)

 * return ls('path'[,'opts')

 * return ff_download(srcpaths[,ropts])
 *
 * return ff_mdelete(paths)
 * 
 * return os.stat(path)
 * return os.utime(path)
 * return os.mkdir(path)
 * return os.remove(path)
 * return mkdir_m(path(s))

 * rs_init for starting usb shootinp
 */

static int
chdk_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context, int dirsonly)
{
	Camera *camera = (Camera *)data;
	PTPParams		*params = &camera->pl->params;
	int 			scriptid = 0;
	int			retbool = FALSE;
	unsigned int		status;
	int			tablecnt, luastatus;
	ptp_chdk_script_msg	*msg = NULL;
	/* return ls("A/path/"); */
	const char 		*luascript = PTP_CHDK_LUA_LS"\nreturn ls('A%s',{\nstat='*',\n})";
	char 			*lua = NULL;
	char			*t, *table = NULL;
	char			*xfolder;

	/* strip leading / of folders, except for the root folder */
	xfolder=strdup(folder);
	if (strlen(folder)>2 && (xfolder[strlen(xfolder)-1] == '/'))
		xfolder[strlen(xfolder)-1] = '\0';
        /*Camera *camera = data;*/

	C_MEM (lua = malloc(strlen(luascript)+strlen(xfolder)+1));

	sprintf(lua,luascript,xfolder);
	free(xfolder);

        /* List your files here */
	/*GP_LOG_D ("calling lua script %s", lua);*/
	C_PTP (ptp_chdk_exec_lua(params, lua, 0, &scriptid, &luastatus));
	GP_LOG_D ("called script. script id %d, status %d", scriptid, luastatus);

	while (1) {
		C_PTP (ptp_chdk_get_script_status(params, &status));
		GP_LOG_D ("script status %x", status);

		if (status & PTP_CHDK_SCRIPT_STATUS_MSG) {
			C_PTP (ptp_chdk_read_script_msg(params, &msg));
			GP_LOG_D ("message script id %d, type %d, subtype %d", msg->script_id, msg->type, msg->subtype);
			switch (msg->type) {
			case PTP_CHDK_S_MSGTYPE_RET:
			case PTP_CHDK_S_MSGTYPE_USER:
				switch (msg->subtype) {
				case PTP_CHDK_TYPE_UNSUPPORTED: GP_LOG_D("unsupported");break;
				case PTP_CHDK_TYPE_NIL: GP_LOG_D("nil");break;
				case PTP_CHDK_TYPE_BOOLEAN:
					retbool = msg->data[0]; /* 3 more bytes, but rest are not interestng for bool */
					GP_LOG_D("boolean %d", retbool);
					break;
				case PTP_CHDK_TYPE_INTEGER: GP_LOG_D("int");break;
				case PTP_CHDK_TYPE_STRING: GP_LOG_D("string %s", msg->data);break;
				case PTP_CHDK_TYPE_TABLE:
					GP_LOG_D("table %s", msg->data);
					table = strdup(msg->data);
					break;
				default: GP_LOG_E("unknown chdk msg->type %d", msg->subtype);break;
				}
				break;
			case PTP_CHDK_S_MSGTYPE_ERR:
				GP_LOG_D ("error %d, message %s", msg->subtype, msg->data);
				break;
			default:
				GP_LOG_E ("unknown msg->type %d", msg->type);
			}
			free (msg);
		}
		/* We can get multiple tables due to the msg batcher. */
		if (table) {
			t = table;
		/* table {[1]="NIKON001.DSC",[2]="DCIM",[3]="MISC",[4]="DISKBOOT.BIN",[5]="CHDK",[6]="changelog.txt",[7]="vers.req",[8]="readme.txt",[9]="PS.FI2",} */

		/* table {[1]={is_file=true,mtime=1402161416,name="NIKON001.DSC",ctime=1402161416,attrib=34,is_dir=false,size=512,},[2]={is_file=false,mtime=1402161416,name="DCIM",ctime=1402161416,attrib=16,is_dir=true,size=0,},[3]={is_file=false,mtime=1406460938,name="MISC",ctime=1406460938,attrib=16,is_dir=true,size=0,},[4]={is_file=true,mtime=1398564982,name="DISKBOOT.BIN",ctime=1423347896,attrib=32,is_dir=false,size=138921,},[5]={is_file=false,mtime=1423347900,name="CHDK",ctime=1423347900,attrib=16,is_dir=true,size=0,},[6]={is_file=true,mtime=1395026402,name="changelog.txt",ctime=1423347900,attrib=32,is_dir=false,size=2093,},[7]={is_file=true,mtime=1217623956,name="vers.req",ctime=1423347900,attrib=32,is_dir=false,size=107,},[8]={is_file=true,mtime=1398564982,name="readme.txt",ctime=1423347900,attrib=32,is_dir=false,size=10518,},[9]={is_file=true,mtime=1398564982,name="PS.FI2",ctime=1423347900,attrib=32,is_dir=false,size=80912,},}
 */
			if (*t != '{')
				return GP_ERROR;
			t++;
			tablecnt = 0;
			while (*t) {
				int cnt;
				char *name = NULL;
				int isfile = 0, mtime = 0, attrib = -1, ctime = 0, size = -1;
				CameraFileInfo info;

				if (*t++ != '[') {
					GP_LOG_E("expected [, have %c", t[-1]);
					break;
				}
				if (!sscanf(t,"%d",&cnt)) {
					GP_LOG_E("expected integer");
					break;
				}
				GP_LOG_D("parsing entry %d", cnt);
				if (cnt != tablecnt + 1) {
					GP_LOG_E("cnt %d, tablecnt %d, expected %d", cnt, tablecnt, tablecnt+1);
					break;
				}
				tablecnt++;
				t = strchr(t,']');
				if (!t) {
					GP_LOG_E("expected ]");
					break;
				}
				t++;
				if (*t++ != '=') {
					GP_LOG_E("expected =");
					break;
				}
				/* {is_file=true,mtime=1402161416,name="NIKON001.DSC",ctime=1402161416,attrib=34,is_dir=false,size=512,} */
				if (*t++ != '{') {
					GP_LOG_E("expected {");
					break;
				}
				memset(&info,0,sizeof(info));
				while (*t && *t != '}') {
					GP_LOG_D("parsing %s", t);
					if (t==strstr(t,"is_file=true")) { isfile = TRUE; }
					if (t==strstr(t,"is_file=false")) { isfile = FALSE; }
					if (t==strstr(t,"is_dir=true")) { isfile = FALSE; }
					if (t==strstr(t,"is_dir=false")) { isfile = TRUE; }
					if (t==strstr(t,"name=\"")) {
						char *s;
						name = t+strlen("name=.");
						s = strchr(name,'"');
						if (s) *s='\0';
						name = strdup(name);
						GP_LOG_D("name is %s", name);
						*s = '"';
					}
					if (sscanf(t,"mtime=%d,", &mtime)) {
						info.file.mtime = mtime;
						info.file.fields |= GP_FILE_INFO_MTIME;
					}
					if (sscanf(t,"size=%d,", &size)) {
						info.file.size = size;
						info.file.fields |= GP_FILE_INFO_SIZE;
					}
					if (!sscanf(t,"ctime=%d,", &ctime)) { }
					if (!sscanf(t,"attrib=%d,", &attrib)) { }
					t = strchr(t,',');
					if (t) t++;
				}
				if (*t)
					t++;
				/* Directories: return as list. */
				if (dirsonly && !isfile)
					gp_list_append (list, name, NULL);

				/* Files: Add directly to FS, including the meta info too. */
				if (!dirsonly && isfile) {
					gp_filesystem_append(fs, folder, name, context);
					gp_filesystem_set_info_noop(fs, folder, name, info, context);
				}
				free(name);

				if (*t++ != ',') {
					GP_LOG_E("expected , got %c", t[-1]);
					break;
				}
				if (*t == '}') { t++; break; }
			}
			if (*t) {
				GP_LOG_E("expected end of string");
				return GP_ERROR;
			}
			free (table);
			table = NULL;
		}

		if (!status) /* this means we read out all messages */
			break;

		/* wait for script to finish */
		if (status & PTP_CHDK_SCRIPT_STATUS_RUN)
			usleep(100*1000); /* 100 ms */
	}
	if (retbool)
		return GP_OK;
	GP_LOG_E("boolean return from LUA ls was FALSE");
	return GP_ERROR;
}

static int
chdk_file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
	return chdk_list_func(fs,folder,list,data,context,FALSE);
}


static int
chdk_folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                  void *data, GPContext *context)
{
	return chdk_list_func(fs,folder,list,data,context,TRUE);
}

static int
chdk_get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
               CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = (Camera *)data;
	PTPParams		*params = &camera->pl->params;
	int 			scriptid = 0;
	int			retbool = FALSE;
	unsigned int		status;
	int			luastatus;
	ptp_chdk_script_msg	*msg = NULL;
	/* return os.stat("A/path/"); */
	const char 		*luascript = "\nreturn os.stat('A%s/%s')";
	char 			*lua = NULL;
	char			*table = NULL;

        /*Camera *camera = data;*/

	C_MEM (lua = malloc(strlen(luascript)+strlen(folder)+strlen(filename)+1));

	sprintf(lua,luascript,folder,filename);

        /* List your files here */
	GP_LOG_D ("calling lua script %s", lua);
	C_PTP (ptp_chdk_exec_lua(params, lua, 0, &scriptid, &luastatus));
	GP_LOG_D ("called script. script id %d, status %d", scriptid, luastatus);

	while (1) {
		C_PTP (ptp_chdk_get_script_status(params, &status));
		GP_LOG_D ("script status %x", status);

		if (status & PTP_CHDK_SCRIPT_STATUS_MSG) {
			C_PTP (ptp_chdk_read_script_msg(params, &msg));
			GP_LOG_D ("message script id %d, type %d, subtype %d", msg->script_id, msg->type, msg->subtype);
			switch (msg->type) {
			case PTP_CHDK_S_MSGTYPE_RET:
			case PTP_CHDK_S_MSGTYPE_USER:
				switch (msg->subtype) {
				case PTP_CHDK_TYPE_UNSUPPORTED: GP_LOG_D("unsupported");break;
				case PTP_CHDK_TYPE_NIL: GP_LOG_D("nil");break;
				case PTP_CHDK_TYPE_BOOLEAN:
					retbool = msg->data[0]; /* 3 more bytes, but rest are not interestng for bool */
					GP_LOG_D("boolean %d", retbool);
					break;
				case PTP_CHDK_TYPE_INTEGER: GP_LOG_D("int");break;
				case PTP_CHDK_TYPE_STRING: GP_LOG_D("string %s", msg->data);break;
				case PTP_CHDK_TYPE_TABLE:
					GP_LOG_D("table %s", msg->data);
					table = strdup(msg->data);
					break;
				default: GP_LOG_E("unknown chdk msg->type %d", msg->subtype);break;
				}
				break;
			case PTP_CHDK_S_MSGTYPE_ERR:
				GP_LOG_D ("error %d, message %s", msg->subtype, msg->data);
				break;
			default:
				GP_LOG_E ("unknown msg->type %d", msg->type);
			}
			free (msg);
		}
		if (table) {
			char *t = table;
			int x;
			while (*t) {
				if (sscanf(t,"mtime %d", &x)) {
					info->file.fields |= GP_FILE_INFO_MTIME;
					info->file.mtime = x;
				}
				if (sscanf(t,"size %d", &x)) {
					info->file.fields |= GP_FILE_INFO_SIZE;
					info->file.size = x;
				}
				t = strchr(t,'\n');
				if (t) t++;
			}
		}

		if (!status) /* this means we read out all messages */
			break;

		/* wait for script to finish */
		if (status & PTP_CHDK_SCRIPT_STATUS_RUN)
			usleep(100*1000); /* 100 ms */
	}
	return GP_OK;
}

static int
chdk_get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
               CameraFileType type, CameraFile *file, void *data,
               GPContext *context)
{
        Camera 			*camera = data;
	PTPParams		*params = &camera->pl->params;
	uint16_t        	ret;
	PTPDataHandler  	handler;
	char 			*fn;

	fn = malloc(1+strlen(folder)+1+strlen(filename)+1),
	sprintf(fn,"A%s/%s",folder,filename);
	ptp_init_camerafile_handler (&handler, file);
	ret = ptp_chdk_download(params, fn, &handler);
	free (fn);
	ptp_exit_camerafile_handler (&handler);
	if (ret == PTP_ERROR_CANCEL)
		return GP_ERROR_CANCEL;
	C_PTP_REP (ret);
	return GP_OK;
}


static CameraFilesystemFuncs chdk_fsfuncs = {
	.file_list_func         = chdk_file_list_func,
	.folder_list_func       = chdk_folder_list_func,
	.get_info_func          = chdk_get_info_func,
	.get_file_func          = chdk_get_file_func,
/*
	.set_info_func          = chdk_set_info_func,
	.read_file_func         = chdk_read_file_func,
	.del_file_func          = chdk_delete_file_func,
	.put_file_func          = chdk_put_file_func,
	.make_dir_func          = chdk_make_dir_func,
	.remove_dir_func        = chdk_remove_dir_func,
	.storage_info_func      = chdk_storage_info_func
*/
};

static int
camera_prepare_chdk_capture(Camera *camera, GPContext *context) {
	PTPParams		*params = &camera->pl->params;
	int 			scriptid = 0, major = 0,minor = 0;
	unsigned int		status;
	int			luastatus;
	ptp_chdk_script_msg	*msg = NULL;
	char *lua		=
PTP_CHDK_LUA_SERIALIZE
"if not get_mode() then\n\
        switch_mode_usb(1)\n\
        local i=0\n\
        while not get_mode() and i < 300 do\n\
                sleep(10)\n\
                i=i+1\n\
        end\n\
        if not get_mode() then\n\
                return false, 'switch failed'\n\
        end\n\
        return true\n\
end\n\
return false,'already in rec'\n\
";
	C_PTP (ptp_chdk_get_version (params, &major, &minor));
	GP_LOG_D ("CHDK %d.%d", major, minor);

	GP_LOG_D ("calling lua script %s", lua);
	C_PTP (ptp_chdk_exec_lua(params, lua, 0, &scriptid, &luastatus));
	GP_LOG_D ("called script. script id %d, status %d", scriptid, luastatus);

	while (1) {
		C_PTP (ptp_chdk_get_script_status(params, &status));
		GP_LOG_D ("script status %x", status);

		if (status & PTP_CHDK_SCRIPT_STATUS_MSG) {
			C_PTP (ptp_chdk_read_script_msg(params, &msg));
			GP_LOG_D ("message script id %d, type %d, subtype %d", msg->script_id, msg->type, msg->subtype);
			GP_LOG_D ("message script %s", msg->data);
			free (msg);
		}

		if (!(status & PTP_CHDK_SCRIPT_STATUS_RUN))
			break;
		usleep(100000);
	}
	return GP_OK;
}

static int
camera_unprepare_chdk_capture(Camera *camera, GPContext *context) {
	PTPParams		*params = &camera->pl->params;
	int 			scriptid = 0, status = 0;
	ptp_chdk_script_msg	*msg = NULL;
	char *lua		=
PTP_CHDK_LUA_SERIALIZE
"if get_mode() then\n\
        switch_mode_usb(0)\n\
        local i=0\n\
        while get_mode() and i < 300 do\n\
                sleep(10)\n\
                i=i+1\n\
        end\n\
        if get_mode() then\n\
                return false, 'switch failed'\n\
        end\n\
        return true\n\
end\n\
return false,'already in play'\n\
";
	GP_LOG_D ("calling lua script %s", lua);
	C_PTP (ptp_chdk_exec_lua(params, lua, 0, &scriptid, &status));
	C_PTP (ptp_chdk_read_script_msg(params, &msg));

	GP_LOG_D ("called script. script id %d, status %d", scriptid, status);
	GP_LOG_D ("message script id %d, type %d, subtype %d", msg->script_id, msg->type, msg->subtype);
	GP_LOG_D ("message script %s", msg->data);
	free (msg);
	if (!status) {
		gp_context_error(context,_("CHDK did not leave recording mode."));
		return GP_ERROR;
	}
	return GP_OK;
}


static int
chdk_camera_about (Camera *camera, CameraText *text, GPContext *context)
{
        snprintf (text->text, sizeof(text->text),
         _("PTP2 / CHDK driver\n"
           "(c) 2015-%d by Marcus Meissner <marcus@jet.franken.de>.\n"
           "This is a PTP subdriver that supports CHDK using Canon cameras.\n"
           "\n"
           "Enjoy!"), 2015);
        return GP_OK;
}

static int
chdk_camera_exit (Camera *camera, GPContext *context) 
{
        return GP_OK;
}

static int
chdk_camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
	GPContext *context)
{
	return camera_prepare_chdk_capture(camera, context);
        return GP_OK;
}



int
chdk_init(Camera *camera, GPContext *context) {
        camera->functions->about = chdk_camera_about;
        camera->functions->exit = chdk_camera_exit;
        camera->functions->capture = chdk_camera_capture;
/*
        camera->functions->trigger_capture = camera_trigger_capture;
        camera->functions->capture_preview = camera_capture_preview;
        camera->functions->summary = camera_summary;
        camera->functions->get_config = camera_get_config;
        camera->functions->set_config = camera_set_config;
        camera->functions->wait_for_event = camera_wait_for_event;
*/

	gp_filesystem_set_funcs ( camera->fs, &chdk_fsfuncs, camera);
	return GP_OK;
}
