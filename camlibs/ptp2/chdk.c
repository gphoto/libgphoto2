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
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_LIBJPEG
#  include <jpeglib.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#include "libgphoto2/i18n.h"

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
chdk_generic_script_run (
	PTPParams *params, const char *luascript,
	char **table, int *retint, GPContext *context
) {
	int			ret = GP_OK;
	int 			scriptid = 0;
	unsigned int		status;
	int			luastatus;
	ptp_chdk_script_msg	*msg = NULL;
	char			*xtable = NULL;
	int			xint = -1;

	if (!table) table = &xtable;
	if (!retint) retint = &xint;

	GP_LOG_D ("calling lua script %s", luascript);
	C_PTP (ptp_chdk_exec_lua(params, (char*)luascript, 0, &scriptid, &luastatus));
	GP_LOG_D ("called script. script id %d, status %d", scriptid, luastatus);

	*table = NULL;
	*retint = -1;

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
					*retint = msg->data[0]; /* 3 more bytes, but rest are not interestng for bool */
					GP_LOG_D("boolean %d", *retint);
					break;
				case PTP_CHDK_TYPE_INTEGER:
					GP_LOG_D("int %02x %02x %02x %02x", msg->data[0], msg->data[1], msg->data[2], msg->data[3]);
					*retint = le32atoh((unsigned char*)msg->data);
					break;
				case PTP_CHDK_TYPE_STRING:
					GP_LOG_D("string %s", msg->data);
					if (*table) {
						*table = realloc(*table,strlen(*table)+strlen(msg->data)+1);
						strcat(*table,msg->data);
					} else {
						*table = strdup(msg->data);
					}
					break;
				case PTP_CHDK_TYPE_TABLE:
					GP_LOG_D("table %s", msg->data);
					if (*table) {
						*table = realloc(*table,strlen(*table)+strlen(msg->data)+1);
						strcat(*table,msg->data);
					} else {
						*table = strdup(msg->data);
					}
					break;
				default: GP_LOG_E("unknown chdk msg->type %d", msg->subtype);break;
				}
				break;
			case PTP_CHDK_S_MSGTYPE_ERR:
				GP_LOG_D ("error %d, message %s", msg->subtype, msg->data);
				gp_context_error(context, _("CHDK lua engine reports error: %s"), msg->data);
				ret = GP_ERROR_BAD_PARAMETERS;
				break;
			default:
				GP_LOG_E ("unknown msg->type %d", msg->type);
				break;
			}
			free (msg);
		}

		if (!status) /* this means we read out all messages */
			break;

		/* wait for script to finish */
		if (status & PTP_CHDK_SCRIPT_STATUS_RUN)
			usleep(100*1000); /* 100 ms */
	}
	if (xtable)
		GP_LOG_E("a string return was unexpected, returned value: %s", xtable);
	if (xint != -1)
		GP_LOG_E("a int return was unexpected, returned value: %d", xint);
	return ret;
}


static int
chdk_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context, int dirsonly)
{
	Camera *camera = (Camera *)data;
	PTPParams		*params = &camera->pl->params;
	int			retint = FALSE;
	int			ret;
	int			tablecnt;
	/* return ls("A/path/"); */
	const char 		*luascript = PTP_CHDK_LUA_LS"\nreturn ls('A%s',{\nstat='*',\n})";
	char 			*lua = NULL;
	char			*t, *table = NULL;
	char			*xfolder;

	/* strip leading / of folders, except for the root folder */
	xfolder=strdup(folder);
	if (strlen(folder)>2 && (xfolder[strlen(xfolder)-1] == '/'))
		xfolder[strlen(xfolder)-1] = '\0';

	C_MEM (lua = malloc(strlen(luascript)+strlen(xfolder)+1));

	sprintf(lua,luascript,xfolder);
	free(xfolder);

	ret = chdk_generic_script_run (params, lua, &table, &retint, context);
	free (lua);
	if (ret != GP_OK)
		return ret;
	if (table) {
		t = table;
	/* table {[1]="NIKON001.DSC",[2]="DCIM",[3]="MISC",[4]="DISKBOOT.BIN",[5]="CHDK",[6]="changelog.txt",[7]="vers.req",[8]="readme.txt",[9]="PS.FI2",} */

	/* table {[1]={is_file=true,mtime=1402161416,name="NIKON001.DSC",ctime=1402161416,attrib=34,is_dir=false,size=512,},[2]={is_file=false,mtime=1402161416,name="DCIM",ctime=1402161416,attrib=16,is_dir=true,size=0,},[3]={is_file=false,mtime=1406460938,name="MISC",ctime=1406460938,attrib=16,is_dir=true,size=0,},[4]={is_file=true,mtime=1398564982,name="DISKBOOT.BIN",ctime=1423347896,attrib=32,is_dir=false,size=138921,},[5]={is_file=false,mtime=1423347900,name="CHDK",ctime=1423347900,attrib=16,is_dir=true,size=0,},[6]={is_file=true,mtime=1395026402,name="changelog.txt",ctime=1423347900,attrib=32,is_dir=false,size=2093,},[7]={is_file=true,mtime=1217623956,name="vers.req",ctime=1423347900,attrib=32,is_dir=false,size=107,},[8]={is_file=true,mtime=1398564982,name="readme.txt",ctime=1423347900,attrib=32,is_dir=false,size=10518,},[9]={is_file=true,mtime=1398564982,name="PS.FI2",ctime=1423347900,attrib=32,is_dir=false,size=80912,},}
*/

nexttable:
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
				/* GP_LOG_D("parsing %s", t); */
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
			if (*t == '{') goto nexttable;
			GP_LOG_E("expected end of string or { , got %s", t);
			return GP_ERROR;
		}
		free (table);
		table = NULL;
	}
	if (retint)
		return GP_OK;
	GP_LOG_E("boolean return from LUA ls was %d", retint);
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
	int			retint = 0;
	int			ret;
	char			*table = NULL;
	const char 		*luascript = "\nreturn os.stat('A%s/%s')";
	char 			*lua = NULL;

	C_MEM (lua = malloc(strlen(luascript)+strlen(folder)+strlen(filename)+1));
	sprintf(lua,luascript,folder,filename);
	ret = chdk_generic_script_run (params, lua, &table, &retint, context);
	free (lua);
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
		free (table);
	}
	return ret;
}

static int
chdk_delete_file_func (CameraFilesystem *fs, const char *folder,
                        const char *filename, void *data, GPContext *context)
{
	Camera *camera = (Camera *)data;
	PTPParams		*params = &camera->pl->params;
	int			ret;
	const char 		*luascript = "\nreturn os.remove('A%s/%s')";
	char 			*lua = NULL;

	C_MEM (lua = malloc(strlen(luascript)+strlen(folder)+strlen(filename)+1));
	sprintf(lua,luascript,folder,filename);
	ret = chdk_generic_script_run (params, lua, NULL, NULL, context);
	free (lua);
	return ret;
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

	fn = malloc(1+strlen(folder)+1+strlen(filename)+1);
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
	.del_file_func          = chdk_delete_file_func,
/*
	.set_info_func          = chdk_set_info_func,
	.read_file_func         = chdk_read_file_func,
	.put_file_func          = chdk_put_file_func,
	.make_dir_func          = chdk_make_dir_func,
	.remove_dir_func        = chdk_remove_dir_func,
	.storage_info_func      = chdk_storage_info_func
*/
};

static int
camera_prepare_chdk_capture(Camera *camera, GPContext *context) {
	PTPParams		*params = &camera->pl->params;
	int			ret = 0, retint = 0;
	char			*table = NULL;
	char			*lua =
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

	ret = chdk_generic_script_run (params, lua, &table, &retint, context);
	if(table) GP_LOG_D("table returned: %s\n", table);
	free(table);
	return ret;
}

static int
camera_unprepare_chdk_capture(Camera *camera, GPContext *context) {
	PTPParams		*params = &camera->pl->params;
	int 			ret, retint;
	char			*table;
	char			*lua =
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
	ret = chdk_generic_script_run (params, lua, &table, &retint, context);
	if(table) GP_LOG_D("table returned: %s\n", table);
	free(table);

	return ret;
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
chdk_camera_summary (Camera *camera, CameraText *text, GPContext *context)
{
	PTPParams	*params = &camera->pl->params;
	int 		ret;
	char 		*s = text->text;
	int		major, minor, retint;

	C_PTP (ptp_chdk_get_version (params, &major, &minor));
        sprintf (s, _("CHDK %d.%d Status:\n"), major, minor ); s += strlen(s);

	ret = chdk_generic_script_run (params, "return get_mode()", NULL, &retint, context);
	sprintf (s, _("Mode: %d\n"), retint); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_sv96()", NULL, &retint, context);
	sprintf (s, _("SV96: %d, ISO: %d\n"), retint, (int)(exp2(retint/96.0)*3.125)); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_tv96()", NULL, &retint, context);
	sprintf (s, _("TV96: %d, Shutterspeed: %f\n"), retint, 1.0/exp2(retint/96.0)); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_av96()", NULL, &retint, context);
	sprintf (s, _("AV96: %d, Aperture: %f\n"), retint, sqrt(exp2(retint/96.0))); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_focus()", NULL, &retint, context);
	sprintf (s, _("Focus: %d\n"), retint); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_iso_mode()", NULL, &retint, context);
	sprintf (s, _("ISO Mode: %d\n"), retint); s += strlen(s);

	ret = chdk_generic_script_run (params, "return get_zoom()", NULL, &retint, context);
	sprintf (s, _("Zoom: %d\n"), retint); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_temperature(0)", NULL, &retint, context);
	sprintf (s, _("Optical Temperature: %d\n"), retint); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_temperature(1)", NULL, &retint, context);
	sprintf (s, _("CCD Temperature: %d\n"), retint); s += strlen(s);
	ret = chdk_generic_script_run (params, "return get_temperature(2)", NULL, &retint, context);
	sprintf (s, _("Battery Temperature: %d\n"), retint); s += strlen(s);

	ret = chdk_generic_script_run (params, "return get_flash_mode()", NULL, &retint, context);
	sprintf (s, _("Flash Mode: %d\n"), retint); s += strlen(s);
        return ret;
/*
Mode: 256
SV96: 603, ISO: 243
TV96: 478, Shutterspeed: 0
AV96: 294, Aperture: 2,890362
ND Filter: -1
Focus: 166
ISO Mode: 0

*/

}

struct submenu;
typedef int (*get_func) (PTPParams *, struct submenu*, CameraWidget **, GPContext *);
#define CONFIG_GET_ARGS PTPParams *params, struct submenu *menu, CameraWidget **widget, GPContext *context
typedef int (*put_func) (PTPParams *, CameraWidget *, GPContext *);
#define CONFIG_PUT_ARGS PTPParams *params, CameraWidget *widget, GPContext *context

struct submenu {
	char		*label;
	char		*name;
        get_func	getfunc;
        put_func	putfunc;
};

static int
chdk_get_iso(CONFIG_GET_ARGS) {
	int retint = 0, iso = 0;
	char buf[20];

	CR (chdk_generic_script_run (params, "return get_iso_mode()", NULL, &retint, context));
	if (!retint) {
		CR(chdk_generic_script_run (params, "return get_sv96()", NULL, &retint, context));
		iso = (int)(exp2(retint/96.0)*3.125);
	} else {
		iso = retint;
	}
	CR (gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget));
	gp_widget_set_name (*widget, menu->name);
	sprintf(buf,"%d", iso);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
chdk_put_iso(CONFIG_PUT_ARGS) {
	int iso = 0;
	char *val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	if (!sscanf(val, "%d", &iso))
		return GP_ERROR_BAD_PARAMETERS;

	sprintf(lua,"return set_iso_mode(%d)\n", iso);
	CR (chdk_generic_script_run (params, lua, NULL, NULL, context));
	return GP_OK;
}

static int
chdk_get_iso_market(CONFIG_GET_ARGS) {
	int retint = 0, iso = 0;
	char buf[20];

	CR (chdk_generic_script_run (params, "return get_iso_market()", NULL, &retint, context));
	if (!retint) {
		CR(chdk_generic_script_run (params, "return iso_real_to_market(get_sv96())", NULL, &retint, context));
		iso = (int)(exp2(retint/96.0)*3.125);
	} else {
		iso = retint;
	}
	CR (gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget));
	gp_widget_set_name (*widget, menu->name);
	sprintf(buf,"%d", iso);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
chdk_put_iso_market(CONFIG_PUT_ARGS) {
	int iso = 0;
	char *val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	if (!sscanf(val, "%d", &iso))
		return GP_ERROR_BAD_PARAMETERS;

	sprintf(lua,"return set_iso_real(iso_market_to_real(%d))\n", iso);
	CR (chdk_generic_script_run (params, lua, NULL, NULL, context));
	return GP_OK;
}

static int
chdk_get_av(CONFIG_GET_ARGS) {
	int retint = 0;
	char buf[20];
	float f;

	CR (chdk_generic_script_run (params, "return get_av96()", NULL, &retint, context));
	f = sqrt(exp2(retint/96.0));
	CR (gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget));
	gp_widget_set_name (*widget, menu->name);
	sprintf(buf, "%d.%d", (int)f,((int)f*10)%10);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
chdk_put_av(CONFIG_PUT_ARGS) {
	char *val;
	int av1,av2,sqav;
	char lua[100];

	gp_widget_get_value (widget, &val);
	if (2 != sscanf(val, "%d.%d", &av1,&av2)) {
		if (!sscanf(val, "%d", &av1)) {
			return GP_ERROR_BAD_PARAMETERS;
		}
		av2 = 0;
	}

	/* av96 = 96*log2(f^2) */
	sqav = (av1*1.0+av2/10.0)*(av1*1.0+av2/10.0);
	sprintf(lua,"return set_av96(%d)\n", (int)(96.0*log2(sqav)));
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_tv(CONFIG_GET_ARGS) {
	int retint = 0;
	char buf[20];

	CR (chdk_generic_script_run (params, "return get_tv96()", NULL, &retint, context));
	CR (gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget));
	gp_widget_set_name (*widget, menu->name);
	sprintf(buf, "%f", 1.0/exp2(retint/96.0));
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
chdk_put_tv(CONFIG_PUT_ARGS) {
	char *val;
	float	f;
	char lua[100];

	gp_widget_get_value (widget, &val);
	if (!sscanf(val, "%f", &f))
		return GP_ERROR_BAD_PARAMETERS;

	sprintf(lua,"return set_tv96(%d)\n", (int)(96.0*(-log2(f))));
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_focus(CONFIG_GET_ARGS) {
	int retint = 0;
	char buf[20];

	CR (chdk_generic_script_run (params, "return get_focus()", NULL, &retint, context));
	CR (gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget));
	sprintf(buf, "%dmm", retint);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
chdk_put_focus(CONFIG_PUT_ARGS) {
	char *val;
	int focus;
	char lua[100];

	gp_widget_get_value (widget, &val);
	if (!sscanf(val, "%dmm", &focus))
		return GP_ERROR_BAD_PARAMETERS;

	sprintf(lua,"return set_focus(%d)\n", focus);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_zoom(CONFIG_GET_ARGS) {
	int retint = 0;
	char buf[20];

	CR (chdk_generic_script_run (params, "return get_zoom()", NULL, &retint, context));
	CR (gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget));
	sprintf(buf, "%d", retint);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
chdk_put_zoom(CONFIG_PUT_ARGS) {
	char *val;
	int zoom;
	char lua[100];

	gp_widget_get_value (widget, &val);
	if (!sscanf(val, "%d", &zoom))
		return GP_ERROR_BAD_PARAMETERS;

	sprintf(lua,"return set_zoom(%d)\n", zoom);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_orientation(CONFIG_GET_ARGS) {
	int retint = 0;
	char buf[20];

	CR (chdk_generic_script_run (params, "return get_orientation_sensor()", NULL, &retint, context));
	CR (gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget));
	sprintf(buf, "%d'", retint);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
chdk_put_none(CONFIG_PUT_ARGS) {
	return GP_ERROR_NOT_SUPPORTED;
}

static int
chdk_get_ev(CONFIG_GET_ARGS) {
	int retint = 0;
	float val;

	CR (chdk_generic_script_run (params, "return get_ev()", NULL, &retint, context));
	CR (gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget));
        gp_widget_set_range (*widget,
                -5.0,
                5.0,
                1.0/6.0
        );
	val = retint/96.0;
	return gp_widget_set_value (*widget, &val);
}

static int
chdk_put_ev(CONFIG_PUT_ARGS) {
	float val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	sprintf(lua,"return set_ev(%d)\n", (int)(val*96.0));
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static void
add_buttons(CameraWidget *widget) {
	gp_widget_add_choice(widget, "shoot_half");
	gp_widget_add_choice(widget, "shoot_full");
	gp_widget_add_choice(widget, "shoot_full_only");
	gp_widget_add_choice(widget, "erase");
	gp_widget_add_choice(widget, "up");
	gp_widget_add_choice(widget, "print");
	gp_widget_add_choice(widget, "left");
	gp_widget_add_choice(widget, "set");
	gp_widget_add_choice(widget, "right");
	gp_widget_add_choice(widget, "disp");
	gp_widget_add_choice(widget, "down");
	gp_widget_add_choice(widget, "menu");
	gp_widget_add_choice(widget, "zoom_in");
	gp_widget_add_choice(widget, "zoom_out");
	gp_widget_add_choice(widget, "video");
	gp_widget_add_choice(widget, "shoot_full");
	gp_widget_add_choice(widget, "shoot_full_only");
	gp_widget_add_choice(widget, "wheel l");
	gp_widget_add_choice(widget, "wheel r");
	gp_widget_add_choice(widget, "zoom in");
	gp_widget_add_choice(widget, "zoom out");
	gp_widget_add_choice(widget, "iso");
	gp_widget_add_choice(widget, "flash");
	gp_widget_add_choice(widget, "mf");
	gp_widget_add_choice(widget, "macro");
	gp_widget_add_choice(widget, "video");
	gp_widget_add_choice(widget, "timer");
	gp_widget_add_choice(widget, "expo_corr");
	gp_widget_add_choice(widget, "fe");
	gp_widget_add_choice(widget, "face");
	gp_widget_add_choice(widget, "zoom_assist");
	gp_widget_add_choice(widget, "ae_lock");
	gp_widget_add_choice(widget, "metering_mode");
	gp_widget_add_choice(widget, "playback");
	gp_widget_add_choice(widget, "help");
}

static int
chdk_get_press(CONFIG_GET_ARGS) {
	CR (gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget));
	gp_widget_set_value (*widget, "chdk buttonname");
	add_buttons(*widget);
	return GP_OK;
}

static int
chdk_put_press(CONFIG_PUT_ARGS) {
	char *val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	sprintf(lua,"press('%s')\n", val);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_release(CONFIG_GET_ARGS) {
	CR (gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget));
	gp_widget_set_value (*widget, "chdk buttonname");
	add_buttons(*widget);
	return GP_OK;
}

static int
chdk_put_release(CONFIG_PUT_ARGS) {
	char *val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	sprintf(lua,"release('%s')\n", val);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_click(CONFIG_GET_ARGS) {
	CR (gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget));
	gp_widget_set_value (*widget, "chdk buttonname");
	add_buttons(*widget);
	return GP_OK;
}

static int
chdk_put_click(CONFIG_PUT_ARGS) {
	char *val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	if (!strcmp(val,"wheel l"))
		strcpy(lua,"post_levent_to_ui(\"RotateJogDialLeft\",1)\n");
	else if (!strcmp(val,"wheel r"))
		strcpy(lua,"post_levent_to_ui(\"RotateJogDialRight\",1)\n");
	else
		sprintf(lua,"click('%s')\n", val);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_capmode(CONFIG_GET_ARGS) {
	char *s , *table = NULL;
	int retint = 0;
	const char *lua =
PTP_CHDK_LUA_SERIALIZE \
"capmode=require'capmode'\n"
"str=''\n"
"local l={}\n"
"local i=1\n"
"for id,name in ipairs(capmode.mode_to_name) do\n"
"	if capmode.valid(id) then\n"
"		str = str .. name .. '\\n'\n"
"		l[i] = {name=name,id=id}\n"
"		i = i + 1\n"
"	end\n"
"end\n"
"str = str .. capmode.get_name()\n"
"return str\n";

	CR (gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget));

	CR (chdk_generic_script_run (params,lua,&table,&retint,context));

	s = table;
	GP_LOG_D("table is %s", table);
	while (*s) {
		char *x = strchr(s,'\n');

		if (x) *x = 0;
		GP_LOG_D("line is %s", s);
		gp_widget_add_choice (*widget, s);
		if (!x || !strlen(x+1))
			gp_widget_set_value (*widget, s);
		if (!x)
			break;
		s = x+1;
	}
	free (table);
	return GP_OK;
}

static int
chdk_put_capmode(CONFIG_PUT_ARGS) {
	char *val;
	char lua[200];
	const char *luastr =
"capmode=require'capmode'\n"
"str='%s'\n"
"for id,name in ipairs(capmode.mode_to_name) do\n"
"	if capmode.valid(id) and str == name then\n"
"		set_capture_mode(id)\n"
"	end\n"
"end\n"
"return\n";

	gp_widget_get_value (widget, &val);
	/* integer? should actually work ... according to CHDK/TEST/isobase.lua */
	sprintf(lua, luastr, val);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static int
chdk_get_aelock(CONFIG_GET_ARGS) {
	int val = 2;
	CR (gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget));
	gp_widget_set_value (*widget, &val);
	return GP_OK;
}

static int
chdk_put_aelock(CONFIG_PUT_ARGS) {
	int val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	sprintf(lua,"set_aelock(%d)\n", val);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}


static int
chdk_get_aflock(CONFIG_GET_ARGS) {
	int val = 2;
	CR (gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget));
	gp_widget_set_value (*widget, &val);
	return GP_OK;
}

static int
chdk_put_aflock(CONFIG_PUT_ARGS) {
	int val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	sprintf(lua,"set_aflock(%d)\n", val);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}


static int
chdk_get_mflock(CONFIG_GET_ARGS) {
	int val = 2;
	CR (gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget));
	gp_widget_set_value (*widget, &val);
	return GP_OK;
}

static int
chdk_put_mflock(CONFIG_PUT_ARGS) {
	int val;
	char lua[100];

	gp_widget_get_value (widget, &val);
	sprintf(lua,"set_mf(%d)\n", val);
	return chdk_generic_script_run (params, lua, NULL, NULL, context);
}

static struct {
        char    *name;
        char    *label;
} chdkonoff[] = {
        {"on", N_("On") },
        {"off", N_("Off") },
};

static int
chdk_get_onoff(CONFIG_GET_ARGS) {
        unsigned int	i;
        char		buf[1024];

        gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
        gp_widget_set_name (*widget, menu->name);
        if (GP_OK != gp_setting_get("ptp2","chdk", buf))
                strcpy(buf,"off");
        for (i=0;i<sizeof (chdkonoff)/sizeof (chdkonoff[i]);i++) {
                gp_widget_add_choice (*widget, _(chdkonoff[i].label));
                if (!strcmp (buf,chdkonoff[i].name))
                        gp_widget_set_value (*widget, _(chdkonoff[i].label));
        }
        return GP_OK;
}

static int
chdk_put_onoff(CONFIG_PUT_ARGS) {
        unsigned int	i;
        char		*val;

        CR (gp_widget_get_value(widget, &val));
        for (i=0;i<sizeof(chdkonoff)/sizeof(chdkonoff[i]);i++) {
                if (!strcmp( val, _(chdkonoff[i].label))) {
                        gp_setting_set("ptp2","chdk",chdkonoff[i].name);
                        break;
                }
        }
        return GP_OK;
}


struct submenu imgsettings[] = {
	{ N_("Raw ISO"),	"rawiso",	chdk_get_iso,	 	chdk_put_iso},
	{ N_("ISO"),		"iso",		chdk_get_iso_market,	chdk_put_iso_market},
	{ N_("Aperture"),	"aperture",	chdk_get_av,		chdk_put_av},
	{ N_("Shutterspeed"),	"shutterspeed",	chdk_get_tv,		chdk_put_tv},
	{ N_("Focus"),		"focus",	chdk_get_focus,		chdk_put_focus},
	{ N_("Zoom"),		"zoom",		chdk_get_zoom,		chdk_put_zoom},
	{ N_("Press"),		"press",	chdk_get_press,		chdk_put_press},
	{ N_("Release"),	"release",	chdk_get_release,	chdk_put_release},
	{ N_("Click"),		"click",	chdk_get_click,		chdk_put_click},
	{ N_("Capture Mode"),	"capmode",	chdk_get_capmode,	chdk_put_capmode},
	{ N_("AE Lock"),	"aelock",	chdk_get_aelock,	chdk_put_aelock},
	{ N_("AF Lock"),	"aflock",	chdk_get_aflock,	chdk_put_aflock},
	{ N_("MF Lock"),	"mflock",	chdk_get_mflock,	chdk_put_mflock},
	{ N_("Exposure Compensation"),	"exposurecompensation",	chdk_get_ev, chdk_put_ev},
	{ N_("Orientation"),	"orientation",	chdk_get_orientation,	chdk_put_none},
	{ N_("CHDK"),		"chdk",		chdk_get_onoff,		chdk_put_onoff},
	{ NULL,			NULL,		NULL, 		NULL},
};

/* We have way less options than regular PTP now, but try to keep the same structure */
static int
chdk_camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	PTPParams	*params = &(camera->pl->params);
	CameraWidget	*menu, *child;
	int		i, ret;

	CR(camera_prepare_chdk_capture(camera, context));

        gp_widget_new (GP_WIDGET_WINDOW, _("Camera and Driver Configuration"), window);
	gp_widget_set_name (*window, "main");
	gp_widget_new (GP_WIDGET_SECTION, _("Image Settings"), &menu);
	gp_widget_set_name (menu, "imgsettings");
	gp_widget_append(*window, menu);

	for (i=0;imgsettings[i].name;i++) {
		ret = imgsettings[i].getfunc(params,&imgsettings[i],&child,context);
		if (ret != GP_OK) {
			GP_LOG_E("error getting %s menu", imgsettings[i].name);
			continue;
		}
		gp_widget_set_name (child, imgsettings[i].name);
		gp_widget_append (menu, child);
	}
	return GP_OK;
}

static int
chdk_camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	PTPParams	*params = &(camera->pl->params);
	int		i, ret;

	for (i=0;imgsettings[i].name;i++) {
		CameraWidget *widget;

		ret = gp_widget_get_child_by_label (window, _(imgsettings[i].label), &widget);
		if (ret != GP_OK)
			continue;
		if (!gp_widget_changed (widget))
			continue;
	        gp_widget_set_changed (widget, FALSE);
		ret = imgsettings[i].putfunc(params,widget,context);
		if (ret != GP_OK) {
			GP_LOG_E("error putting %s menu", imgsettings[i].name);
			continue;
		}
	}
	return GP_OK;
}

static int
chdk_camera_exit (Camera *camera, GPContext *context)
{
	camera_unprepare_chdk_capture(camera, context);
        return GP_OK;
}

static int
chdk_camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
	GPContext *context)
{
	int		ret, retint;
	char		*table, *s;
	PTPParams	*params = &camera->pl->params;
	const char	*luascript = PTP_CHDK_LUA_SERIALIZE_MSGS_SIMPLEQUOTE \
				PTP_CHDK_LUA_RLIB_SHOOT	\
				"return rlib_shoot({info=true});\n";

	ret =  camera_prepare_chdk_capture(camera, context);
	if (ret != GP_OK) return ret;

	ret = chdk_generic_script_run (params, luascript, &table, &retint, context);
	GP_LOG_D("rlib_shoot returned table %s, retint %d\n", table, retint);
	s = strstr(table, "exp=");
	if (s) {
		int exp;
		if (!sscanf(s,"exp=%d\n", &exp)) {
			GP_LOG_E("%s did not parse for exp=NR?", s);
			ret = GP_ERROR;
		} else {
			sprintf(path->name,"IMG_%04d.JPG", exp);
		}
	} else {
		GP_LOG_E("no exp=nr found?\n");
		ret = GP_ERROR;
	}
	s = strstr(table, "dir=\"A");
	if (s) {
		char *y = strchr(s+6,'"');
		if (y) *y='\0';
		strcpy(path->folder, s+6);
	} else {
		ret = GP_ERROR;
	}
	free (table);
        return ret;
}

#ifdef HAVE_LIBJPEG
static void yuv_live_to_jpeg(unsigned char *p_yuv,
			     unsigned int buf_width, unsigned int width, unsigned int height,
			     int fb_type, CameraFile *file
) {
	struct		jpeg_compress_struct cinfo;
	struct		jpeg_error_mgr jerr;
	JSAMPROW	row_ptr[1];
	uint8_t		*outbuf = NULL, *tmprowbuf = NULL;
	uint64_t	outlen = 0;
	unsigned int	row_inc;
	int		sshift, dshift, xshift, skip;

	/* Pre-Digic 6 cameras: 8 bit per element UYVYYY,
	 * 6 bytes used to encode 4 pixels, need 12 bytes raw YUV data for jpeg encoding */
	if (fb_type == LV_FB_YUV8) {
		row_inc = buf_width*1.5;
		sshift = 6;
		dshift = (width/height > 2) ? 6 : 12;
		xshift = 4;
	/* Digic 6 cameras: 8 bit per element UYVY,
	 * 4 bytes used to encode 2 pixels, need 6 bytes raw YUV data for jpeg encoding */
	} else {
		row_inc = buf_width*2;
		sshift = 4;
		dshift = 6;
		xshift = 2;
	}
	/* Encode only 2 pixels from each UV pair either if it is a UYVY data
	 * (for Digic 6 cameras) or if the width to height ratio provided
	 * by camera is too large (typically width 720 for height 240), so that
	 * the resulting image would be stretched too much in the horizontal
	 * direction if all 4 Y values were used. */
	skip  = (fb_type > LV_FB_YUV8) || (width/height > 2);

	cinfo.err = jpeg_std_error (&jerr);
	jpeg_create_compress (&cinfo);

	cinfo.image_width = (width/height > 2) ? (width/2) & -1 : width & -1;
	cinfo.image_height = height & -1;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_YCbCr; // input color space

	jpeg_mem_dest (&cinfo, &outbuf, &outlen);
	jpeg_set_defaults (&cinfo);
	cinfo.dct_method = JDCT_IFAST; // DCT method
	jpeg_set_quality (&cinfo, 70, TRUE);

	jpeg_start_compress (&cinfo, TRUE);

	tmprowbuf = calloc (cinfo.image_width , 3);
	row_ptr[0] = &tmprowbuf[0];

	while (cinfo.next_scanline < cinfo.image_height) {
		unsigned int x, i, j;
		/* offset to the correct row */
		unsigned int offset = cinfo.next_scanline * row_inc;

		for (x = 0, i = 0, j = 0; x < width; i += sshift, j += dshift, x += xshift) {
			int8_t u = (int8_t) p_yuv[offset + i + 0];
			int8_t v = (int8_t) p_yuv[offset + i + 2];
			if (fb_type == LV_FB_YUV8) {
				u += 0x80;
				v += 0x80;
			}

			tmprowbuf[j + 0] = p_yuv[offset + i + 1];
			tmprowbuf[j + 1] = u;
			tmprowbuf[j + 2] = v;
			tmprowbuf[j + 3] = p_yuv[offset + i + 3];
			tmprowbuf[j + 4] = u;
			tmprowbuf[j + 5] = v;

			if (!skip) {
				tmprowbuf[j + 6] = p_yuv[offset + i + 4];
				tmprowbuf[j + 7] = u;
				tmprowbuf[j + 8] = v;
				tmprowbuf[j + 9] = p_yuv[offset + i + 5];
				tmprowbuf[j +10] = u;
				tmprowbuf[j +11] = v;
			}
		}
		jpeg_write_scanlines (&cinfo, row_ptr, 1);
	}
	jpeg_finish_compress (&cinfo);
	jpeg_destroy_compress (&cinfo);

	gp_file_append (file, (char*)outbuf, outlen);
      	gp_file_set_mime_type (file, GP_MIME_JPEG);
      	gp_file_set_name (file, "chdk_preview.jpg");

	free (outbuf);
	free (tmprowbuf);
}

#else
static inline uint8_t clip_yuv (int v) {
	if (v<0) return 0;
	if (v>255) return 255;
	return v;
}

static inline uint8_t yuv_to_r (uint8_t y, int8_t v) {
	return clip_yuv (((y<<12) +          v*5743 + 2048)>>12);
}

static inline uint8_t yuv_to_g (uint8_t y, int8_t u, int8_t v) {
	return clip_yuv (((y<<12) - u*1411 - v*2925 + 2048)>>12);
}

static inline uint8_t yuv_to_b (uint8_t y, int8_t u) {
	return clip_yuv (((y<<12) + u*7258          + 2048)>>12);
}

static void yuv_live_to_ppm (unsigned char *p_yuv,
			     int buf_width, int width, int height,
			     int fb_type, CameraFile *file
) {
	const unsigned char  *p_row = p_yuv;
	const unsigned char  *p;
	unsigned int	      row, x;
	unsigned int	      row_inc;
	int		      pshift, xshift, skip;
	char		      ppm_header[32];
	uint8_t		      rgb[6];

	/* Pre-Digic 6 cameras:
	 * 8 bit per element UYVYYY, 6 bytes used to encode 4 rgb values */
	if (fb_type == LV_FB_YUV8) {
		row_inc = buf_width*1.5;
		pshift = 6;
		xshift = 4;
	/* Digic 6 cameras:
	 * 8 bit per element UYVY, 4 bytes used to encode 2 rgb values */
	} else {
		row_inc = buf_width*2;
		pshift = 4;
		xshift = 2;
	}
	/* Encode only 2 pixels from each UV pair either if it is a UYVY data
	 * (for Digic 6 cameras) or if the width to height ratio provided
	 * by camera is too large (typically width 720 for height 240), so that
	 * the resulting image would be stretched too much in the horizontal
	 * direction if all 4 Y values were used. */
	skip  = (fb_type > LV_FB_YUV8) || (width/height > 2);

	sprintf (ppm_header, "P6 %d %d 255\n", (width/height > 2) ? width/2 : width, height);
	gp_file_append (file, ppm_header, strlen (ppm_header));

	for (row=0; row<height; row++, p_row += row_inc) {
		for (x=0, p=p_row; x<width; x+=xshift, p+=pshift) {
			/* these are signed unlike the Y values */
			int8_t u = (int8_t) p[0];
			int8_t v = (int8_t) p[2];
			/* See for example
			 * https://chdk.setepontos.com/index.php?topic=12692.msg130137#msg130137 */
			if (fb_type > LV_FB_YUV8) {
				u -= 0x80;
				v -= 0x80;
			}
			rgb[0] = yuv_to_r (p[1], v);
			rgb[1] = yuv_to_g (p[1], u, v);
			rgb[2] = yuv_to_b (p[1], u);

			rgb[3] = yuv_to_r (p[3], v);
			rgb[4] = yuv_to_g (p[3], u, v);
			rgb[5] = yuv_to_b (p[3], u);
			gp_file_append (file, (char*)rgb, 6);

			if (!skip) {
				rgb[0] = yuv_to_r (p[4], v);
				rgb[1] = yuv_to_g (p[4], u, v);
				rgb[2] = yuv_to_b (p[4], u);

				rgb[3] = yuv_to_r (p[5], v);
				rgb[4] = yuv_to_g (p[5], u, v);
				rgb[5] = yuv_to_b (p[5], u);
				gp_file_append (file, (char*)rgb, 6);
			}
		}
	}
      	gp_file_set_mime_type (file, GP_MIME_PPM);
      	gp_file_set_name (file, "chdk_preview.ppm");
}
#endif

static int
chdk_camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned char	*data = NULL;
	uint32_t	size = 0;
	PTPParams	*params = &camera->pl->params;
	unsigned int	flags = LV_TFR_VIEWPORT;

	lv_data_header header;
	lv_framebuffer_desc vpd;
	lv_framebuffer_desc bmd;

	memset (&header, 0, sizeof (header));
	memset (&vpd, 0, sizeof (vpd));
	memset (&vpd, 0, sizeof (bmd));

	CR (camera_prepare_chdk_capture (camera, context));
      	C_PTP_REP_MSG (ptp_chdk_get_live_data (params, flags, &data, &size),
      		       _("CHDK get live data failed"));
	if (ptp_chdk_parse_live_data (params, data, size, &header, &vpd, &bmd) != PTP_RC_OK) {
		gp_context_error (context, _("CHDK get live data failed: incomplete data (%d bytes) returned"), size);
		return GP_ERROR;
	}
#ifdef HAVE_LIBJPEG
	yuv_live_to_jpeg(data+vpd.data_start, vpd.buffer_width, vpd.visible_width,
			 vpd.visible_height, vpd.fb_type, file);
#else
	yuv_live_to_ppm (data+vpd.data_start, vpd.buffer_width, vpd.visible_width,
			 vpd.visible_height, vpd.fb_type, file);
#endif

      	free (data);
      	gp_file_set_mtime (file, time (NULL));
      	return GP_OK;
}

int
chdk_init(Camera *camera, GPContext *context) {
        camera->functions->about = chdk_camera_about;
        camera->functions->exit = chdk_camera_exit;
        camera->functions->capture = chdk_camera_capture;
        camera->functions->summary = chdk_camera_summary;
        camera->functions->get_config = chdk_camera_get_config;
        camera->functions->set_config = chdk_camera_set_config;
        camera->functions->capture_preview = chdk_camera_capture_preview;
/*
        camera->functions->trigger_capture = camera_trigger_capture;
        camera->functions->wait_for_event = camera_wait_for_event;
*/

	gp_filesystem_set_funcs ( camera->fs, &chdk_fsfuncs, camera);
	return GP_OK;
}
