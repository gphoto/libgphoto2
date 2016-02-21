/* config.c
 *
 * Copyright (C) 2003-2016 Marcus Meissner <marcus@jet.franken.de>
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
#define _BSD_SOURCE
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#ifdef WIN32
# include <winsock.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif
#include <sys/types.h>

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

#ifdef __GNUC__
# define __unused__ __attribute__((unused))
#else
# define __unused__
#endif

#define SET_CONTEXT(camera, ctx) ((PTPData *) camera->pl->params.data)->context = ctx

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
camera_prepare_canon_powershot_capture(Camera *camera, GPContext *context) {
	uint16_t		ret;
	PTPContainer		event;
	PTPPropertyValue	propval;
	PTPParams		*params = &camera->pl->params;
	int 			found, oldtimeout;

        if (ptp_property_issupported(params, PTP_DPC_CANON_FlashMode)) {
		GP_LOG_D ("Canon capture mode is already set up.");
		C_PTP (ptp_getdevicepropvalue(params, PTP_DPC_CANON_EventEmulateMode, &propval, PTP_DTC_UINT16));
		GP_LOG_D ("Event emulate mode 0x%04x", propval.u16);
		params->canon_event_mode = propval.u16;
		return GP_OK;
	}

	propval.u16 = 0;
	C_PTP (ptp_getdevicepropvalue(params, PTP_DPC_CANON_EventEmulateMode, &propval, PTP_DTC_UINT16));
	GP_LOG_D ("prop 0xd045 value is 0x%04x",propval.u16);

	propval.u16=1;
	C_PTP (ptp_setdevicepropvalue(params, PTP_DPC_CANON_EventEmulateMode, &propval, PTP_DTC_UINT16));
	params->canon_event_mode = propval.u16;
	C_PTP (ptp_getdevicepropvalue(params, PTP_DPC_CANON_SizeOfOutputDataFromCamera, &propval, PTP_DTC_UINT32));
	GP_LOG_D ("prop PTP_DPC_CANON_SizeOfOutputDataFromCamera value is %d",propval.u32);
	C_PTP (ptp_getdevicepropvalue(params, PTP_DPC_CANON_SizeOfInputDataToCamera, &propval, PTP_DTC_UINT32));
	GP_LOG_D ("prop PTP_DPC_CANON_SizeOfInputDataToCamera value is %d",propval.u32);

	C_PTP (ptp_getdeviceinfo (params, &params->deviceinfo));
	C_PTP (ptp_getdeviceinfo (params, &params->deviceinfo));
	CR (fixup_cached_deviceinfo (camera, &params->deviceinfo));

	C_PTP (ptp_getdevicepropvalue(params, PTP_DPC_CANON_SizeOfOutputDataFromCamera, &propval, PTP_DTC_UINT32));
	GP_LOG_D ("prop PTP_DPC_CANON_SizeOfOutputDataFromCamera value is %d",propval.u32);
	C_PTP (ptp_getdevicepropvalue(params, PTP_DPC_CANON_SizeOfInputDataToCamera, &propval, PTP_DTC_UINT32));
	GP_LOG_D ("prop PTP_DPC_CANON_SizeOfInputDataToCamera value is %d",propval.u32);
	C_PTP (ptp_getdeviceinfo (params, &params->deviceinfo));
	CR (fixup_cached_deviceinfo (camera, &params->deviceinfo));
	C_PTP (ptp_getdevicepropvalue(params, PTP_DPC_CANON_EventEmulateMode, &propval, PTP_DTC_UINT16));
	params->canon_event_mode = propval.u16;
	GP_LOG_D ("prop 0xd045 value is 0x%04x",propval.u16);

	GP_LOG_D ("Magic code ends.");

	GP_LOG_D ("Setting prop. EventEmulateMode to 7.");
/* interrupt  9013 get event
 1     Yes      No
 2     Yes      No
 3     Yes      Yes
 4     Yes      Yes
 5     Yes      Yes
 6     No       No
 7     No       Yes
 */
	propval.u16 = 7;
	C_PTP (ptp_setdevicepropvalue(params, PTP_DPC_CANON_EventEmulateMode, &propval, PTP_DTC_UINT16));
	params->canon_event_mode = propval.u16;

	ret = ptp_canon_startshootingmode (params);
	if (ret == PTP_RC_CANON_A009) {
		/* I think this means ... already in capture mode */
		return GP_OK;
	}
	if (ret != PTP_RC_OK) {
		GP_LOG_E ("'ptp_canon_startshootingmode (params)' failed: 0x%04x", ret);
		C_PTP_REP (ret);
	}
	gp_port_get_timeout (camera->port, &oldtimeout);
	gp_port_set_timeout (camera->port, 1000);

	/* Catch the event telling us the mode was switched ... */
	/* If we were prepared already, it will be 5*50*1000 wait, so 1/4 second ... hmm */
	found = 0;
	while (found++<10) {
		ret = ptp_check_event (params);
		if (ret != PTP_RC_OK)
			break;

		while (ptp_get_one_event (params, &event)) {
			GP_LOG_D ("Event: 0x%x", event.Code);
			if ((event.Code==0xc00c) ||
			    (event.Code==PTP_EC_StorageInfoChanged)) {
				GP_LOG_D ("Event: Entered shooting mode.");
				found = 1;
				break;
			}
		}
		usleep(50*1000);
	}

#if 0
	gp_port_set_timeout (camera->port, oldtimeout);
	if (ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOn)) {
		ret = ptp_canon_viewfinderon (params);
		if (ret != PTP_RC_OK)
			GP_LOG_E ("Canon enable viewfinder failed: %d", ret);
		/* ignore errors here */
	}
	gp_port_set_timeout (camera->port, 1000);
#endif

	/* Reget device info, they change on the Canons. */
	C_PTP (ptp_getdeviceinfo(&camera->pl->params, &camera->pl->params.deviceinfo));
	CR (fixup_cached_deviceinfo (camera, &camera->pl->params.deviceinfo));
	gp_port_set_timeout (camera->port, oldtimeout);
	return GP_OK;
}

int
camera_canon_eos_update_capture_target(Camera *camera, GPContext *context, int value) {
	PTPParams		*params = &camera->pl->params;
	char			buf[200];
	PTPPropertyValue	ct_val;
	PTPDevicePropDesc	dpd;
	int			cardval = -1;

	memset(&dpd,0,sizeof(dpd));
	C_PTP (ptp_canon_eos_getdevicepropdesc (params,PTP_DPC_CANON_EOS_CaptureDestination, &dpd));

	/* Look for the correct value of the card mode */
	if (value != PTP_CANON_EOS_CAPTUREDEST_HD) {
		if (dpd.FormFlag == PTP_DPFF_Enumeration) {
			unsigned int	i;
			for (i=0;i<dpd.FORM.Enum.NumberOfValues;i++) {
				if (dpd.FORM.Enum.SupportedValue[i].u32 != PTP_CANON_EOS_CAPTUREDEST_HD) {
					cardval = dpd.FORM.Enum.SupportedValue[i].u32;
					break;
				}
			}
			GP_LOG_D ("Card value is %d",cardval);
		}
		if (cardval == -1) {
			GP_LOG_D ("NO Card found - falling back to SDRAM!");
			cardval = PTP_CANON_EOS_CAPTUREDEST_HD;
		}
	}

	if (value == 1)
		value = cardval;

	/* -1 == use setting from config-file, 1 == card, 4 == ram*/
	ct_val.u32 = (value == -1)
		     ? (GP_OK == gp_setting_get("ptp2","capturetarget",buf)) && strcmp(buf,"sdram") ? cardval : PTP_CANON_EOS_CAPTUREDEST_HD
		     : value;

	/* otherwise we get DeviceBusy for some reason */
	if (ct_val.u32 != dpd.CurrentValue.u32) {
		C_PTP_MSG (ptp_canon_eos_setdevicepropvalue (params, PTP_DPC_CANON_EOS_CaptureDestination, &ct_val, PTP_DTC_UINT32),
			   "setdevicepropvalue of capturetarget to 0x%x failed", ct_val.u32);
		if (ct_val.u32 == PTP_CANON_EOS_CAPTUREDEST_HD) {
			uint16_t	ret;

			/* if we want to download the image from the device, we need to tell the camera
			 * that we have enough space left. */
			/* this might be a trigger value for "no space" -Marcus
			ret = ptp_canon_eos_pchddcapacity(params, 0x7fffffff, 0x00001000, 0x00000001);
			 */

			ret = ptp_canon_eos_pchddcapacity(params, 0x04ffffff, 0x00001000, 0x00000001);
			/* not so bad if its just busy, would also fail later. */
			if (ret == PTP_RC_DeviceBusy) ret = PTP_RC_OK;
			C_PTP (ret);
		}
	} else {
		GP_LOG_D ("optimized ... setdevicepropvalue of capturetarget to 0x%x not done as it was set already.", ct_val.u32 );
	}
	ptp_free_devicepropdesc (&dpd);
	return GP_OK;
}

static int
camera_prepare_canon_eos_capture(Camera *camera, GPContext *context) {
	PTPParams	*params = &camera->pl->params;
	PTPStorageIDs	sids;

	GP_LOG_D ("preparing EOS capture...");

	C_PTP (ptp_canon_eos_setremotemode(params, 1));
	C_PTP (ptp_canon_eos_seteventmode(params, 1));
	params->eos_camerastatus = -1;	/* aka unknown */

	if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_SetRequestOLCInfoGroup))
		C_PTP (ptp_canon_eos_setrequestolcinfogroup(params, 0x00001fff));

	/* Get the initial bulk set of event data */
	C_PTP (ptp_check_eos_events (params));

	if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RequestDevicePropValue)) {
		/* request additional properties */
		/* if one of this is not present GeneralError will be returned, so only check
		 * and log, but do not error return */
		LOG_ON_PTP_E (ptp_canon_eos_requestdevicepropvalue (params, PTP_DPC_CANON_EOS_Owner));
		LOG_ON_PTP_E (ptp_canon_eos_requestdevicepropvalue (params, PTP_DPC_CANON_EOS_Artist));
		LOG_ON_PTP_E (ptp_canon_eos_requestdevicepropvalue (params, PTP_DPC_CANON_EOS_Copyright));
		LOG_ON_PTP_E (ptp_canon_eos_requestdevicepropvalue (params, PTP_DPC_CANON_EOS_SerialNumber));

/*		LOG_ON_PTP_E (ptp_canon_eos_requestdevicepropvalue (params, PTP_DPC_CANON_EOS_DPOFVersion)); */
/*		LOG_ON_PTP_E (ptp_canon_eos_requestdevicepropvalue (params, PTP_DPC_CANON_EOS_MyMenuList)); */
/*		LOG_ON_PTP_E (ptp_canon_eos_requestdevicepropvalue (params, PTP_DPC_CANON_EOS_LensAdjustParams)); */
	}
	if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_GetDeviceInfoEx)) {
		PTPCanonEOSDeviceInfo x;
		unsigned int i;

		C_PTP (ptp_canon_eos_getdeviceinfo (params, &x));
		for (i=0;i<x.EventsSupported_len;i++)
			GP_LOG_D ("event: %04x", x.EventsSupported[i]);
		for (i=0;i<x.DevicePropertiesSupported_len;i++)
			GP_LOG_D ("deviceprop: %04x", x.DevicePropertiesSupported[i]);
		for (i=0;i<x.unk_len;i++)
			GP_LOG_D ("unk: %04x", x.unk[i]);
		ptp_free_EOS_DI (&x);
	}

	/* Get the second bulk set of event data */
	C_PTP (ptp_check_eos_events (params));

	CR (camera_canon_eos_update_capture_target( camera, context, -1 ));

	ptp_free_DI (&params->deviceinfo);
	C_PTP (ptp_getdeviceinfo(params, &params->deviceinfo));
	CR (fixup_cached_deviceinfo (camera, &params->deviceinfo));
	C_PTP (ptp_canon_eos_getstorageids(params, &sids));
	if (sids.n >= 1) {
		unsigned char *sdata;
		unsigned int slen;
		C_PTP (ptp_canon_eos_getstorageinfo(params, sids.Storage[0], &sdata, &slen ));
		free (sdata);
	}
	free (sids.Storage);

	/* FIXME: 9114 call missing here! */

	/* Get the second bulk set of 0x9116 property data */
	C_PTP (ptp_check_eos_events (params));
	params->eos_captureenabled = 1;
	return GP_OK;
}

int
camera_prepare_capture (Camera *camera, GPContext *context)
{
	PTPParams		*params = &camera->pl->params;
	
	GP_LOG_D ("prepare_capture");
	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		if (ptp_operation_issupported(params, PTP_OC_CANON_InitiateReleaseControl))
			return camera_prepare_canon_powershot_capture(camera,context);

		if (ptp_operation_issupported(params, PTP_OC_CHDK))
			return camera_prepare_chdk_capture(camera,context);

		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease))
			return camera_prepare_canon_eos_capture(camera,context);
		gp_context_error(context, _("Sorry, your Canon camera does not support Canon capture"));
		return GP_ERROR_NOT_SUPPORTED;
	default:
		/* generic capture does not need preparation */
		return GP_OK;
	}
	return GP_OK;
}

static int
camera_unprepare_canon_powershot_capture(Camera *camera, GPContext *context) {
	PTPParams		*params = &camera->pl->params;

	C_PTP (ptp_canon_endshootingmode (params));

	if (ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOff)) {
		if (params->canon_viewfinder_on) {
			params->canon_viewfinder_on = 0;
			LOG_ON_PTP_E (ptp_canon_viewfinderoff (params));
			/* ignore errors here */
		}
	}
	/* Reget device info, they change on the Canons. */
	C_PTP (ptp_getdeviceinfo(params, &params->deviceinfo));
	CR (fixup_cached_deviceinfo (camera, &params->deviceinfo));
	return GP_OK;
}

static int
camera_unprepare_canon_eos_capture(Camera *camera, GPContext *context) {
	PTPParams		*params = &camera->pl->params;

	/* just in case we had autofocus running */
	CR (ptp_canon_eos_afcancel(params));

	/* then emits 911b and 911c ... not done yet ... */
	CR (camera_canon_eos_update_capture_target(camera, context, 1));

	C_PTP (ptp_canon_eos_resetuilock (params));

	/* Drain the rest set of the event data */
	C_PTP (ptp_check_eos_events (params));
	C_PTP (ptp_canon_eos_setremotemode(params, 0));
	C_PTP (ptp_canon_eos_seteventmode(params, 0));
	params->eos_captureenabled = 0;
	return GP_OK;
}

int
camera_unprepare_capture (Camera *camera, GPContext *context)
{
	GP_LOG_D ("Unprepare_capture");
	switch (camera->pl->params.deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_TerminateReleaseControl))
			return camera_unprepare_canon_powershot_capture (camera, context);

		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CHDK))
			return camera_unprepare_chdk_capture(camera,context);

		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_RemoteRelease))
			return camera_unprepare_canon_eos_capture (camera, context);

		gp_context_error(context,
		_("Sorry, your Canon camera does not support Canon capture"));
		return GP_ERROR_NOT_SUPPORTED;
	default:
		/* generic capture does not need unpreparation */
		return GP_OK;
	}
	return GP_OK;
}

static uint16_t
nikon_wait_busy(PTPParams *params, int waitms, int timeout) {
	uint16_t	res;
	int		tries;

	/* wait either 1 second, or 50 tries */
	if (waitms)
		tries=timeout/waitms;
	else
		tries=50;
	do {
		res = ptp_nikon_device_ready(params);
		if (res != PTP_RC_DeviceBusy)
			return res;
		if (waitms) usleep(waitms*1000)/*wait a bit*/;
	} while (tries--);
	return res;
}

int
have_prop(Camera *camera, uint16_t vendor, uint16_t prop) {
	unsigned int i;

	/* prop 0 matches */
	if (!prop && (camera->pl->params.deviceinfo.VendorExtensionID==vendor))
		return 1;

	if (	((prop & 0x7000) == 0x5000) ||
		(NIKON_1(&camera->pl->params) && ((prop & 0xf000) == 0xf000))
	) { /* properties */
		for (i=0; i<camera->pl->params.deviceinfo.DevicePropertiesSupported_len; i++) {
			if (prop != camera->pl->params.deviceinfo.DevicePropertiesSupported[i])
				continue;
			if ((prop & 0xf000) == 0x5000) { /* generic property */
				if (!vendor || (camera->pl->params.deviceinfo.VendorExtensionID==vendor))
					return 1;
			}
			if (camera->pl->params.deviceinfo.VendorExtensionID==vendor)
				return 1;
		}
	}
	if ((prop & 0x7000) == 0x1000) { /* commands */
		for (i=0; i<camera->pl->params.deviceinfo.OperationsSupported_len; i++) {
			if (prop != camera->pl->params.deviceinfo.OperationsSupported[i])
				continue;
			if ((prop & 0xf000) == 0x1000) /* generic property */
				return 1;
			if (camera->pl->params.deviceinfo.VendorExtensionID==vendor)
				return 1;
		}
	}
	return 0;
}

static int
have_eos_prop(Camera *camera, uint16_t vendor, uint16_t prop) {
	unsigned int i;

	/* The special Canon EOS property set gets special treatment. */
	if ((camera->pl->params.deviceinfo.VendorExtensionID != PTP_VENDOR_CANON) ||
	    (vendor != PTP_VENDOR_CANON)
	)
		return 0;
	for (i=0;i<camera->pl->params.nrofcanon_props;i++)
		if (camera->pl->params.canon_props[i].proptype == prop)
			return 1;
	return 0;
}

struct submenu;
#define CONFIG_GET_ARGS Camera *camera, CameraWidget **widget, struct submenu* menu, PTPDevicePropDesc *dpd
#define CONFIG_GET_NAMES camera, widget, menu, dpd
typedef int (*get_func)(CONFIG_GET_ARGS);
#define CONFIG_PUT_ARGS Camera *camera, CameraWidget *widget, PTPPropertyValue *propval, PTPDevicePropDesc *dpd
#define CONFIG_PUT_NAMES camera, widget, propval, dpd
typedef int (*put_func)(CONFIG_PUT_ARGS);

struct menu;
#define CONFIG_MENU_GET_ARGS Camera *camera, CameraWidget **widget, struct menu* menu
typedef int (*get_menu_func)(CONFIG_MENU_GET_ARGS);
#define CONFIG_MENU_PUT_ARGS Camera *camera, CameraWidget *widget
typedef int (*put_menu_func)(CONFIG_MENU_PUT_ARGS);

struct submenu {
	char 		*label;
	char		*name;
	uint16_t	propid;
	uint16_t	vendorid;
	uint32_t	type;	/* for 32bit alignment */
	get_func	getfunc;
	put_func	putfunc;
};

struct menu {
	char		*label;
	char		*name;

	uint16_t	usb_vendorid;
	uint16_t	usb_productid;

	/* Either: Standard menu */
	struct	submenu	*submenus;
	/* Or: Non-standard menu with custom behaviour */
	get_menu_func	getfunc;
	put_menu_func	putfunc;
};

struct deviceproptableu8 {
	char		*label;
	uint8_t		value;
	uint16_t	vendor_id;
};

struct deviceproptableu16 {
	char		*label;
	uint16_t	value;
	uint16_t	vendor_id;
};

struct deviceproptablei16 {
	char		*label;
	int16_t		value;
	uint16_t	vendor_id;
};

/* Generic helper function for:
 *
 * ENUM UINT16 propertiess, with potential vendor specific variables.
 */
static int
_get_Generic16Table(CONFIG_GET_ARGS, struct deviceproptableu16* tbl, int tblsize) {
	int i, j;
	int isset = FALSE, isset2 = FALSE;

	if (!(dpd->FormFlag & (PTP_DPFF_Enumeration|PTP_DPFF_Range))) {
		GP_LOG_D ("no enumeration/range in 16bit table code... going on");
	}
	if (dpd->DataType != PTP_DTC_UINT16) {
		GP_LOG_D ("no uint16 prop in 16bit table code");
		return (GP_ERROR);
	}

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		if (!dpd->FORM.Enum.NumberOfValues) {
			/* fill in with all values we have in the table. */
			for (j=0;j<tblsize;j++) {
				if ((tbl[j].vendor_id == 0) ||
				    (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.u16) {
						gp_widget_set_value (*widget, _(tbl[j].label));
						isset2 = TRUE;
					}
				}
			}
			/* fallthrough in case we do not have currentvalue in the table. isset2 = FALSE */
		}
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
			isset = FALSE;
			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == dpd->FORM.Enum.SupportedValue[i].u16) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.u16) {
						isset2 = TRUE;
						gp_widget_set_value (*widget, _(tbl[j].label));
					}
					isset = TRUE;
					break;
				}
			}
			if (!isset) {
				char buf[200];
				sprintf(buf, _("Unknown value %04x"), dpd->FORM.Enum.SupportedValue[i].u16);
				gp_widget_add_choice (*widget, buf);
				if (dpd->FORM.Enum.SupportedValue[i].u16 == dpd->CurrentValue.u16) {
					isset2 = TRUE;
					gp_widget_set_value (*widget, buf);
				}
			}
		}
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		for (	i = dpd->FORM.Range.MinimumValue.u16;
			i<=dpd->FORM.Range.MaximumValue.u16;
			i+= dpd->FORM.Range.StepSize.u16
		) {
			isset = FALSE;
			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == i) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (i == dpd->CurrentValue.u16) {
						isset2 = TRUE;
						gp_widget_set_value (*widget, _(tbl[j].label));
					}
					isset = TRUE;
					break;
				}
			}
			if (!isset) {
				char buf[200];
				sprintf(buf, _("Unknown value %04d"), i);
				gp_widget_add_choice (*widget, buf);
				if (i == dpd->CurrentValue.u16) {
					isset2 = TRUE;
					gp_widget_set_value (*widget, buf);
				}
			}
		}
	}
	if (!isset2) {
		for (j=0;j<tblsize;j++) {
			if (((tbl[j].vendor_id == 0) ||
			     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)) &&
			     (tbl[j].value == dpd->CurrentValue.u16)
			) {
				gp_widget_add_choice (*widget, _(tbl[j].label));
				isset2 = TRUE;
				gp_widget_set_value (*widget, _(tbl[j].label));
			}
		}
		if (!isset2) {
			char buf[200];
			sprintf(buf, _("Unknown value %04x"), dpd->CurrentValue.u16);
			gp_widget_add_choice (*widget, buf);
			gp_widget_set_value (*widget, buf);
		}
	}
	return (GP_OK);
}


static int
_put_Generic16Table(CONFIG_PUT_ARGS, struct deviceproptableu16* tbl, int tblsize) {
	char *value;
	int i, intval, j;
	int foundvalue = 0;
	uint16_t	u16val = 0;

	CR (gp_widget_get_value (widget, &value));
	for (i=0;i<tblsize;i++) {
		if (!strcmp(_(tbl[i].label),value) &&
		    ((tbl[i].vendor_id == 0) || (tbl[i].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
		) {
			u16val = tbl[i].value;
			foundvalue = 1;
		
			if (dpd->FormFlag & PTP_DPFF_Enumeration) {
				for (j = 0; j<dpd->FORM.Enum.NumberOfValues; j++) {
					if (u16val == dpd->FORM.Enum.SupportedValue[j].u16) {
						GP_LOG_D ("FOUND right value for %s in the enumeration at val %d", value, u16val);
						propval->u16 = u16val;
						return GP_OK;
					}
				}
				GP_LOG_D ("did not find the right value for %s in the enumeration at val %d... continuing", value, u16val);
				/* continue looking, but with this value as fallback */
			} else {
				GP_LOG_D ("not an enumeration ... return %s as %d", value, u16val);
				propval->u16 = u16val;
				return GP_OK;
			}
		}
	}
	if (foundvalue) {
		GP_LOG_D ("Using fallback, not found in enum... return %s as %d", value, u16val);
		propval->u16 = u16val;
		return GP_OK;
	}
	if (!sscanf(value, _("Unknown value %04x"), &intval)) {
		GP_LOG_E ("failed to find value %s in list", value);
		return (GP_ERROR);
	}
	GP_LOG_D ("Using fallback, not found in enum... return %s as %d", value, u16val);
	propval->u16 = intval;
	return GP_OK;
}

#define GENERIC16TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_Generic16Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Generic16Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

static int
_get_GenericI16Table(CONFIG_GET_ARGS, struct deviceproptablei16* tbl, int tblsize) {
	int i, j;
	int isset = FALSE, isset2 = FALSE;

	if (!(dpd->FormFlag & (PTP_DPFF_Range|PTP_DPFF_Enumeration))) {
		GP_LOG_D ("no enumeration/range in 16bit table code");
	}
	if (dpd->DataType != PTP_DTC_INT16) {
		GP_LOG_D ("no int16 prop in 16bit table code");
		return (GP_ERROR);
	}

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		if (!dpd->FORM.Enum.NumberOfValues) {
			/* fill in with all values we have in the table. */
			for (j=0;j<tblsize;j++) {
				if ((tbl[j].vendor_id == 0) ||
				    (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.i16) {
						gp_widget_set_value (*widget, _(tbl[j].label));
						isset2 = TRUE;
					}
				}
			}
			/* fallthrough */
		}
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
			isset = FALSE;
			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == dpd->FORM.Enum.SupportedValue[i].i16) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.i16) {
						gp_widget_set_value (*widget, _(tbl[j].label));
						isset2 = TRUE;
					}
					isset = TRUE;
					break;
				}
			}
			if (!isset) {
				char buf[200];
				sprintf(buf, _("Unknown value %04x"), dpd->FORM.Enum.SupportedValue[i].i16);
				gp_widget_add_choice (*widget, buf);
				if (dpd->FORM.Enum.SupportedValue[i].i16 == dpd->CurrentValue.i16)
					gp_widget_set_value (*widget, buf);
			}
		}
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		for (i = dpd->FORM.Range.MinimumValue.i16; i<=dpd->FORM.Range.MaximumValue.i16; i+= dpd->FORM.Range.StepSize.i16) {
			int isset3 = FALSE;

			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == i) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (i == dpd->CurrentValue.i16) {
						isset2 = TRUE;
						gp_widget_set_value (*widget, _(tbl[j].label));
					}
					isset3 = TRUE;
					break;
				}
			}
			if (!isset3) {
				char buf[200];
				sprintf(buf, _("Unknown value %04d"), i);
				gp_widget_add_choice (*widget, buf);
				if (i == dpd->CurrentValue.i16) {
					isset2 = TRUE;
					gp_widget_set_value (*widget, buf);
				}
			}
		}
	}
	if (!isset2) {
		char buf[200];

		/* if we are not in enum or range mode, but have a value, check our internal table */
		for (j=0;j<tblsize;j++) {
			if (((tbl[j].vendor_id == 0) ||
			     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)) &&
			     (tbl[j].value == dpd->CurrentValue.i16)
			) {
				isset2 = TRUE;
				gp_widget_add_choice (*widget, _(tbl[j].label));
				gp_widget_set_value (*widget, _(tbl[j].label));
				break;
			}
		}
		if (!isset2) {
			sprintf(buf, _("Unknown value %04x"), dpd->CurrentValue.i16);
			gp_widget_add_choice (*widget, buf);
			gp_widget_set_value (*widget, buf);
		}
	}
	return (GP_OK);
}


static int
_put_GenericI16Table(CONFIG_PUT_ARGS, struct deviceproptablei16* tbl, int tblsize) {
	char *value;
	int i, intval;

	CR (gp_widget_get_value (widget, &value));
	for (i=0;i<tblsize;i++) {
		if (!strcmp(_(tbl[i].label),value) &&
		    ((tbl[i].vendor_id == 0) || (tbl[i].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
		) {
			propval->i16 = tbl[i].value;
			return GP_OK;
		}
	}
	if (!sscanf(value, _("Unknown value %04d"), &intval)) {
		GP_LOG_E ("failed to find value %s in list", value);
		return (GP_ERROR);
	}
	propval->i16 = intval;
	return GP_OK;
}

#define GENERICI16TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_GenericI16Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_GenericI16Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

static int
_get_Generic8Table(CONFIG_GET_ARGS, struct deviceproptableu8* tbl, int tblsize) {
	int i, j;
	int isset = FALSE, isset2 = FALSE;

	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		if ((dpd->DataType != PTP_DTC_UINT8) && (dpd->DataType != PTP_DTC_INT8))
			return (GP_ERROR);
		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		gp_widget_set_name (*widget, menu->name);
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
			isset = FALSE;
			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == dpd->FORM.Enum.SupportedValue[i].u8) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.u8) {
						isset2 = TRUE;
						gp_widget_set_value (*widget, _(tbl[j].label));
					}
					isset = TRUE;
					break;
				}
			}
			if (!isset) {
				char buf[200];
				sprintf(buf, _("Unknown value %04x"), dpd->FORM.Enum.SupportedValue[i].u8);
				gp_widget_add_choice (*widget, buf);
				if (dpd->FORM.Enum.SupportedValue[i].u8 == dpd->CurrentValue.u8)
					gp_widget_set_value (*widget, buf);
			}
		}
		if (!isset2) {
			char buf[200];
			sprintf(buf, _("Unknown value %04x"), dpd->CurrentValue.u8);
			gp_widget_add_choice (*widget, buf);
			gp_widget_set_value (*widget, buf);
		}
		return (GP_OK);
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		if (dpd->DataType != PTP_DTC_UINT8)
			return (GP_ERROR);
		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		gp_widget_set_name (*widget, menu->name);
		for (	i = dpd->FORM.Range.MinimumValue.u8;
			i <= dpd->FORM.Range.MaximumValue.u8;
			i+= dpd->FORM.Range.StepSize.u8
		) {
			isset = FALSE;
			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == i) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.u8) {
						isset2 = TRUE;
						gp_widget_set_value (*widget, _(tbl[j].label));
					}
					isset = TRUE;
					break;
				}
			}
			if (!isset) {
				char buf[200];
				sprintf(buf, _("Unknown value %04x"), i);
				gp_widget_add_choice (*widget, buf);
				if (i == dpd->CurrentValue.u8) {
					isset2 = TRUE;
					gp_widget_set_value (*widget, buf);
				}
			}
		}
		if (!isset2) {
			char buf[200];
			sprintf(buf, _("Unknown value %04x"), dpd->CurrentValue.u8);
			gp_widget_add_choice (*widget, buf);
			gp_widget_set_value (*widget, buf);
		}
		return (GP_OK);
	}
	return (GP_ERROR);
}


static int
_put_Generic8Table(CONFIG_PUT_ARGS, struct deviceproptableu8* tbl, int tblsize) {
	char *value;
	int i, intval;

	CR (gp_widget_get_value(widget, &value));
	for (i=0;i<tblsize;i++) {
		if (!strcmp(_(tbl[i].label),value) &&
		    ((tbl[i].vendor_id == 0) || (tbl[i].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
		) {
			propval->u8 = tbl[i].value;
			return GP_OK;
		}
	}
	if (!sscanf(value, _("Unknown value %04x"), &intval))
		return (GP_ERROR);
	propval->u8 = intval;
	return GP_OK;
}

#define GENERIC8TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_Generic8Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Generic8Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}


static int
_get_AUINT8_as_CHAR_ARRAY(CONFIG_GET_ARGS) {
	unsigned int	j;
	char 		value[128];

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->DataType != PTP_DTC_AUINT8) {
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
	} else {
		memset(value,0,sizeof(value));
		for (j=0;j<dpd->CurrentValue.a.count;j++)
			value[j] = dpd->CurrentValue.a.v[j].u8;
	}
	gp_widget_set_value (*widget,value);
	return (GP_OK);
}

static int
_get_STR(CONFIG_GET_ARGS) {
	char value[64];

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->DataType != PTP_DTC_STR) {
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
		gp_widget_set_value (*widget,value);
	} else {
		gp_widget_set_value (*widget,dpd->CurrentValue.str);
	}
	return (GP_OK);
}

static int
_put_STR(CONFIG_PUT_ARGS) {
	const char *string;

	CR (gp_widget_get_value(widget, &string));
	C_MEM (propval->str = strdup (string));
	return (GP_OK);
}

static int
_put_AUINT8_as_CHAR_ARRAY(CONFIG_PUT_ARGS) {
	char	*value;
	unsigned int i;

	CR (gp_widget_get_value(widget, &value));
	memset(propval,0,sizeof(PTPPropertyValue));
	/* add \0 ? */
	C_MEM (propval->a.v = malloc((strlen(value)+1)*sizeof(PTPPropertyValue)));
	propval->a.count = strlen(value)+1;
	for (i=0;i<strlen(value)+1;i++)
		propval->a.v[i].u8 = value[i];
	return (GP_OK);
}

static int
_get_Range_INT8(CONFIG_GET_ARGS) {
	float CurrentValue;
	
	if (dpd->FormFlag != PTP_DPFF_Range)
		return (GP_ERROR_NOT_SUPPORTED);
	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	CurrentValue = (float) dpd->CurrentValue.i8;
	gp_widget_set_range ( *widget, (float) dpd->FORM.Range.MinimumValue.i8, (float) dpd->FORM.Range.MaximumValue.i8, (float) dpd->FORM.Range.StepSize.i8);
	gp_widget_set_value ( *widget, &CurrentValue);
	return (GP_OK);
}


static int
_put_Range_INT8(CONFIG_PUT_ARGS) {
	float f;

	CR (gp_widget_get_value(widget, &f));
	propval->i8 = (int) f;
	return (GP_OK);
}

static int
_get_Range_UINT8(CONFIG_GET_ARGS) {
	float CurrentValue;
	
	if (dpd->FormFlag != PTP_DPFF_Range)
		return (GP_ERROR_NOT_SUPPORTED);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	CurrentValue = (float) dpd->CurrentValue.u8;
	gp_widget_set_range ( *widget, (float) dpd->FORM.Range.MinimumValue.u8, (float) dpd->FORM.Range.MaximumValue.u8, (float) dpd->FORM.Range.StepSize.u8);
	gp_widget_set_value ( *widget, &CurrentValue);
	return (GP_OK);
}


static int
_put_Range_UINT8(CONFIG_PUT_ARGS) {
	float f;

	CR (gp_widget_get_value(widget, &f));
	propval->u8 = (int) f;
	return (GP_OK);
}

/* generic int getter */
static int
_get_INT(CONFIG_GET_ARGS) {
	char value[64];

	switch (dpd->DataType) {
	case PTP_DTC_UINT32:
		sprintf (value, "%u", dpd->CurrentValue.u32 );
		break;
	case PTP_DTC_INT32:
		sprintf (value, "%d", dpd->CurrentValue.i32 );
		break;
	case PTP_DTC_UINT16:
		sprintf (value, "%u", dpd->CurrentValue.u16 );
		break;
	case PTP_DTC_INT16:
		sprintf (value, "%d", dpd->CurrentValue.i16 );
		break;
	case PTP_DTC_UINT8:
		sprintf (value, "%u", dpd->CurrentValue.u8 );
		break;
	case PTP_DTC_INT8:
		sprintf (value, "%d", dpd->CurrentValue.i8 );
		break;
	default:
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
		return GP_ERROR;
	}
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value (*widget,value);
	return GP_OK;
}

static int
_put_INT(CONFIG_PUT_ARGS) {
	char *value;
	unsigned int u;
	int i;

	CR (gp_widget_get_value(widget, &value));

	switch (dpd->DataType) {
	case PTP_DTC_UINT32:
	case PTP_DTC_UINT16:
	case PTP_DTC_UINT8:
		C_PARAMS (1 == sscanf (value, "%u", &u ));
		break;
	case PTP_DTC_INT32:
	case PTP_DTC_INT16:
	case PTP_DTC_INT8:
		C_PARAMS (1 == sscanf (value, "%d", &i ));
		break;
	default:
		return GP_ERROR;
	}
	switch (dpd->DataType) {
	case PTP_DTC_UINT32:
		propval->u32 = u;
		break;
	case PTP_DTC_INT32:
		propval->i32 = i;
		break;
	case PTP_DTC_UINT16:
		propval->u16 = u;
		break;
	case PTP_DTC_INT16:
		propval->i16 = i;
		break;
	case PTP_DTC_UINT8:
		propval->u8 = u;
		break;
	case PTP_DTC_INT8:
		propval->i8 = i;
		break;
	}
	return GP_OK;
}


static int
_get_Nikon_OnOff_UINT8(CONFIG_GET_ARGS) {
	if (dpd->FormFlag != PTP_DPFF_Range)
		return (GP_ERROR_NOT_SUPPORTED);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value ( *widget, (dpd->CurrentValue.u8?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Nikon_OnOff_UINT8(CONFIG_PUT_ARGS) {
	char *value;

	CR (gp_widget_get_value(widget, &value));
	if(!strcmp(value,_("On"))) {
		propval->u8 = 1;
		return (GP_OK);
	}
	if(!strcmp(value,_("Off"))) {
		propval->u8 = 0;
		return (GP_OK);
	}
	return (GP_ERROR);
}

static int
_get_Nikon_OffOn_UINT8(CONFIG_GET_ARGS) {
	if (dpd->FormFlag != PTP_DPFF_Range)
		return (GP_ERROR_NOT_SUPPORTED);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value ( *widget, (!dpd->CurrentValue.u8?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Nikon_OffOn_UINT8(CONFIG_PUT_ARGS) {
	char *value;

	CR (gp_widget_get_value(widget, &value));
	if(!strcmp(value,_("On"))) {
		propval->u8 = 0;
		return (GP_OK);
	}
	if(!strcmp(value,_("Off"))) {
		propval->u8 = 1;
		return (GP_OK);
	}
	return (GP_ERROR);
}

#define PUT_SONY_VALUE_(bits) 								\
static int										\
_put_sony_value_u##bits (PTPParams*params, uint16_t prop, uint##bits##_t value,int useenumorder) {	\
	GPContext 		*context = ((PTPData *) params->data)->context;		\
	PTPDevicePropDesc	dpd;							\
	PTPPropertyValue	propval;						\
	uint32_t		origval;						\
	time_t			start,end;						\
											\
	GP_LOG_D("setting 0x%04x to 0x%08x", prop, value);				\
											\
	C_PTP_REP (ptp_generic_getdevicepropdesc (params, prop, &dpd));			\
	do {										\
		origval = dpd.CurrentValue.u##bits;					\
		/* if it is a ENUM, the camera will walk through the ENUM */		\
		if (useenumorder && (dpd.FormFlag & PTP_DPFF_Enumeration)) {				\
			int i, posorig = -1, posnew = -1;				\
											\
			for (i=0;i<dpd.FORM.Enum.NumberOfValues;i++) {			\
				if (origval == dpd.FORM.Enum.SupportedValue[i].u##bits)	\
					posorig = i;					\
				if (value == dpd.FORM.Enum.SupportedValue[i].u##bits)	\
					posnew = i;					\
				if ((posnew != -1) && (posorig != -1))			\
					break;						\
			}								\
			if (posnew == -1) {						\
				gp_context_error (context, _("Target value is not in enumeration."));\
				return GP_ERROR_BAD_PARAMETERS;				\
			}								\
			GP_LOG_D("posnew %d, posorig %d, value %d", posnew, posorig, value);	\
			if (posnew > posorig)						\
				propval.u8 = 0x01;					\
			else								\
				propval.u8 = 0xff;					\
		} else {								\
			if (value > origval)						\
				propval.u8 = 0x01;					\
			else								\
				propval.u8 = 0xff;					\
		}									\
		C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, prop, &propval, PTP_DTC_UINT8 ));\
											\
		GP_LOG_D ("value is (0x%x vs target 0x%x)", origval, value);		\
											\
		/* we tell the camera to do it, but it takes around 0.7 seconds for the SLT-A58 */	\
		time(&start);								\
		do {									\
			C_PTP_REP (ptp_sony_getalldevicepropdesc (params));		\
			C_PTP_REP (ptp_generic_getdevicepropdesc (params, prop, &dpd));	\
											\
			if (dpd.CurrentValue.u##bits == value) {			\
				GP_LOG_D ("Value matched!");				\
				break;							\
			}								\
			if (dpd.CurrentValue.u##bits != origval) {			\
				GP_LOG_D ("value changed (0x%x vs 0x%x vs target 0x%x), next step....", dpd.CurrentValue.u##bits, origval, value);\
				break;							\
			}								\
											\
			usleep(200*1000);						\
											\
			time(&end);							\
		} while (end-start <= 3);						\
											\
		if (dpd.CurrentValue.u##bits == value) {				\
			GP_LOG_D ("Value matched!");					\
			break;								\
		}									\
		if (dpd.CurrentValue.u##bits == origval) {				\
			GP_LOG_D ("value did not change (0x%x vs 0x%x vs target 0x%x), not good ...", dpd.CurrentValue.u##bits, origval, value);\
			break;								\
		}									\
	} while (1);									\
	return GP_OK;									\
}

PUT_SONY_VALUE_(16) /* _put_sony_value_u16 */
PUT_SONY_VALUE_(32) /* _put_sony_value_u32 */

static int
_get_CANON_FirmwareVersion(CONFIG_GET_ARGS) {
	char value[64];

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->DataType != PTP_DTC_UINT32) {
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
	} else {
		uint32_t x = dpd->CurrentValue.u32;
		sprintf (value,"%d.%d.%d.%d",((x&0xff000000)>>24),((x&0xff0000)>>16),((x&0xff00)>>8),x&0xff);
	}
	gp_widget_set_value (*widget,value);
	return (GP_OK);
}

static struct deviceproptableu16 whitebalance[] = {
	{ N_("Manual"),			0x0001, 0 },
	{ N_("Automatic"),		0x0002, 0 },
	{ N_("One-push Automatic"),	0x0003, 0 },
	{ N_("Daylight"),		0x0004, 0 },
	{ N_("Fluorescent"),		0x0005, 0 },
	{ N_("Tungsten"),		0x0006, 0 },
	{ N_("Flash"),			0x0007, 0 },
	{ N_("Cloudy"),			0x8010, PTP_VENDOR_NIKON },
	{ N_("Shade"),			0x8011, PTP_VENDOR_NIKON },
	{ N_("Color Temperature"),	0x8012, PTP_VENDOR_NIKON },
	{ N_("Preset"),			0x8013, PTP_VENDOR_NIKON },

	{ N_("Fluorescent Lamp 1"),	0x8001, PTP_VENDOR_FUJI },
	{ N_("Fluorescent Lamp 2"),	0x8002, PTP_VENDOR_FUJI },
	{ N_("Fluorescent Lamp 3"),	0x8003, PTP_VENDOR_FUJI },
	{ N_("Fluorescent Lamp 4"),	0x8004, PTP_VENDOR_FUJI },
	{ N_("Fluorescent Lamp 5"),	0x8005, PTP_VENDOR_FUJI },
	{ N_("Shade"),			0x8006, PTP_VENDOR_FUJI },
	{ N_("Choose Color Temperature"),0x8007, PTP_VENDOR_FUJI },
	{ N_("Preset Custom 1"),	0x8008, PTP_VENDOR_FUJI },
	{ N_("Preset Custom 2"),	0x8009, PTP_VENDOR_FUJI },
	{ N_("Preset Custom 3"),	0x800a, PTP_VENDOR_FUJI },
	{ N_("Preset Custom 4"),	0x800b, PTP_VENDOR_FUJI },
	{ N_("Preset Custom 5"),	0x800c, PTP_VENDOR_FUJI },

	{ N_("Shade"),			0x8011, PTP_VENDOR_SONY },
	{ N_("Cloudy"),			0x8010, PTP_VENDOR_SONY },
	{ N_("Fluorescent: Warm White"),0x8001, PTP_VENDOR_SONY },
	{ N_("Fluorescent: Cold White"),0x8002, PTP_VENDOR_SONY },
	{ N_("Fluorescent: Day White"),	0x8003, PTP_VENDOR_SONY },
	{ N_("Fluorescent: Daylight"),	0x8004, PTP_VENDOR_SONY },
	{ N_("Choose Color Temperature"),0x8012, PTP_VENDOR_SONY },
	{ N_("Preset"),			0x8023, PTP_VENDOR_SONY },

	{ N_("Shade"),			0x8001, PTP_VENDOR_PENTAX },
	{ N_("Cloudy"),			0x8002, PTP_VENDOR_PENTAX },
	{ N_("Tungsten 2"),		0x8020, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: Daylight"),	0x8003, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: Day White"),	0x8004, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: White"),	0x8005, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: Tungsten"),	0x8006, PTP_VENDOR_PENTAX },
};
GENERIC16TABLE(WhiteBalance,whitebalance)

static struct deviceproptableu16 fuji_imageformat[] = {
	{ N_("RAW"),			1,	PTP_VENDOR_FUJI },
	{ N_("JPEG Fine"),		2,	PTP_VENDOR_FUJI },
	{ N_("JPEG Normal"),		3,	PTP_VENDOR_FUJI },
	{ N_("RAW + JPEG Fine"),	4,	PTP_VENDOR_FUJI },
	{ N_("RAW + JPEG Normal"),	5,	PTP_VENDOR_FUJI },
};
GENERIC16TABLE(Fuji_ImageFormat,fuji_imageformat)

static struct deviceproptableu16 fuji_releasemode[] = {
	{ N_("Single frame"),		1,	PTP_VENDOR_FUJI },
	{ N_("Continuous low speed"),	2,	PTP_VENDOR_FUJI },
	{ N_("Continuous high speed"),	3,	PTP_VENDOR_FUJI },
	{ N_("Self-timer"),		4,	PTP_VENDOR_FUJI },
	{ N_("Mup Mirror up"),		5,	PTP_VENDOR_FUJI },
};
GENERIC16TABLE(Fuji_ReleaseMode,fuji_releasemode)

static int
_get_ImageSize(CONFIG_GET_ARGS) {
	int j;

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return(GP_ERROR);
	if (dpd->DataType != PTP_DTC_STR)
		return(GP_ERROR);
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
		gp_widget_add_choice (*widget,dpd->FORM.Enum.SupportedValue[j].str);
	}
	gp_widget_set_value (*widget,dpd->CurrentValue.str);
	return GP_OK;
}

static int
_put_ImageSize(CONFIG_PUT_ARGS) {
	char *value;

	CR (gp_widget_get_value(widget, &value));
	C_MEM (propval->str = strdup (value));
	return(GP_OK);
}


static int
_get_ExpCompensation(CONFIG_GET_ARGS) {
	int j;
	char buf[10];

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_INT16)
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
		sprintf(buf, "%g", dpd->FORM.Enum.SupportedValue[j].i16/1000.0);
		gp_widget_add_choice (*widget,buf);
	}
	sprintf(buf, "%g", dpd->CurrentValue.i16/1000.0);
	gp_widget_set_value (*widget,buf);
	return GP_OK;
}

static int
_put_ExpCompensation(CONFIG_PUT_ARGS) {
	char *value;
	float x;

	CR (gp_widget_get_value(widget, &value));
	if (1 != sscanf(value,"%g", &x))
		return GP_ERROR;
	propval->i16 = x*1000.0;
	return GP_OK ;
}

static int
_put_Sony_ExpCompensation(CONFIG_PUT_ARGS) {
	int ret;

	ret = _put_ExpCompensation(CONFIG_PUT_NAMES);
	if (ret != GP_OK) return ret;
	return _put_sony_value_u16 (&camera->pl->params, PTP_DPC_ExposureBiasCompensation, propval->i16, 0);
}

static struct deviceproptableu16 canon_assistlight[] = {
	{ N_("On"),	0x0000, PTP_VENDOR_CANON },
	{ N_("Off"),	0x0001, PTP_VENDOR_CANON },
};
GENERIC16TABLE(Canon_AssistLight,canon_assistlight)

static struct deviceproptableu16 canon_autorotation[] = {
	{ N_("On"),	0x0000, PTP_VENDOR_CANON },
	{ N_("Off"),	0x0001, PTP_VENDOR_CANON },
};
GENERIC16TABLE(Canon_AutoRotation,canon_autorotation)

static struct deviceproptableu16 canon_beepmode[] = {
	{ N_("Off"),	0x00, PTP_VENDOR_CANON },
	{ N_("On"),	0x01, PTP_VENDOR_CANON },
};
GENERIC16TABLE(Canon_BeepMode,canon_beepmode)

static int
_get_Canon_ZoomRange(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.u16;
	b = (float)dpd->FORM.Range.MinimumValue.u16;
	t = (float)dpd->FORM.Range.MaximumValue.u16;
	s = (float)dpd->FORM.Range.StepSize.u16;
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return (GP_OK);
}

static int
_put_Canon_ZoomRange(CONFIG_PUT_ARGS)
{
	float	f;

	CR (gp_widget_get_value(widget, &f));
	propval->u16 = (unsigned short)f;
	return (GP_OK);
}

static int
_get_Nikon_WBBias(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR);
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.i8;
	b = (float)dpd->FORM.Range.MinimumValue.i8;
	t = (float)dpd->FORM.Range.MaximumValue.i8;
	s = (float)dpd->FORM.Range.StepSize.i8;
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return (GP_OK);
}

static int
_put_Nikon_WBBias(CONFIG_PUT_ARGS)
{
	float	f;

	CR (gp_widget_get_value(widget, &f));
	propval->i8 = (signed char)f;
	return (GP_OK);
}

static int
_get_Nikon_UWBBias(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.u8;
	b = (float)dpd->FORM.Range.MinimumValue.u8;
	t = (float)dpd->FORM.Range.MaximumValue.u8;
	s = (float)dpd->FORM.Range.StepSize.u8;
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return (GP_OK);
}

static int
_put_Nikon_UWBBias(CONFIG_PUT_ARGS)
{
	float	f;

	CR (gp_widget_get_value(widget, &f));
	propval->u8 = (unsigned char)f;
	return (GP_OK);
}

static int
_get_Nikon_WBBiasPresetVal(CONFIG_GET_ARGS) {
	char buf[20];

	if (dpd->DataType != PTP_DTC_UINT32)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	sprintf (buf, "%d", dpd->CurrentValue.u32);
	gp_widget_set_value (*widget, buf);
	return (GP_OK);
}
static int
_get_Nikon_WBBiasPreset(CONFIG_GET_ARGS) {
	char buf[20];
	int i;

	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	for (i = dpd->FORM.Range.MinimumValue.u8; i < dpd->FORM.Range.MaximumValue.u8; i++) {
		sprintf (buf, "%d", i);
		gp_widget_add_choice (*widget, buf);
		if (i == dpd->CurrentValue.u8)
			gp_widget_set_value (*widget, buf);
	}
	return (GP_OK);
}

static int
_put_Nikon_WBBiasPreset(CONFIG_PUT_ARGS) {
	int	ret;
	char	*val;

	CR (gp_widget_get_value(widget, &val));
	sscanf (val, "%u", &ret);
	propval->u8 = ret;
	return (GP_OK);
}

static int
_get_Nikon_HueAdjustment(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR);
	if (dpd->FormFlag & PTP_DPFF_Range) {
		gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
		gp_widget_set_name (*widget,menu->name);
		f = (float)dpd->CurrentValue.i8;
		b = (float)dpd->FORM.Range.MinimumValue.i8;
		t = (float)dpd->FORM.Range.MaximumValue.i8;
		s = (float)dpd->FORM.Range.StepSize.i8;
		gp_widget_set_range (*widget, b, t, s);
		gp_widget_set_value (*widget, &f);
		return (GP_OK);
	}
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		char buf[20];
		int i, isset = FALSE;

		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		gp_widget_set_name (*widget,menu->name);
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {

			sprintf (buf, "%d", dpd->FORM.Enum.SupportedValue[i].i8);
			gp_widget_add_choice (*widget, buf);
			if (dpd->FORM.Enum.SupportedValue[i].i8 == dpd->CurrentValue.i8) {
				gp_widget_set_value (*widget, buf);
				isset = TRUE;
			}
		}
		if (!isset) {
			sprintf (buf, "%d", dpd->FORM.Enum.SupportedValue[0].i8);
			gp_widget_set_value (*widget, buf);
		}
		return (GP_OK);
	}
	return (GP_ERROR);
}

static int
_put_Nikon_HueAdjustment(CONFIG_PUT_ARGS)
{
	if (dpd->FormFlag & PTP_DPFF_Range) {
		float	f = 0.0;
		CR (gp_widget_get_value(widget, &f));
		propval->i8 = (signed char)f;
		return (GP_OK);
	}
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		char *val;
		int ival;
		
		CR (gp_widget_get_value(widget, &val));
		sscanf (val, "%d", &ival);
		propval->i8 = ival;
		return (GP_OK);
	}
	return (GP_ERROR);
}


static struct deviceproptableu8 canon_quality[] = {
	{ N_("undefined"),	0x00, 0 },
	{ N_("economy"),	0x01, 0 },
	{ N_("normal"),		0x02, 0 },
	{ N_("fine"),		0x03, 0 },
	{ N_("lossless"),	0x04, 0 },
	{ N_("superfine"),	0x05, 0 },
};
GENERIC8TABLE(Canon_Quality,canon_quality)

static struct deviceproptableu8 canon_fullview_fileformat[] = {
	{ N_("Undefined"),	0x00, 0 },
	{ N_("JPEG"),		0x01, 0 },
	{ N_("CRW"),		0x02, 0 },
};
GENERIC8TABLE(Canon_Capture_Format,canon_fullview_fileformat)

static struct deviceproptableu8 canon_shootmode[] = {
	{ N_("Auto"),		0x01, 0 },
	{ N_("TV"),		0x02, 0 },
	{ N_("AV"),		0x03, 0 },
	{ N_("Manual"),		0x04, 0 },
	{ N_("A_DEP"),		0x05, 0 },
	{ N_("M_DEP"),		0x06, 0 },
	{ N_("Bulb"),		0x07, 0 },
	/* Marcus: The SDK has more listed, but I have never seen them 
	 * enumerated by the cameras. Lets leave them out for now. */
};
GENERIC8TABLE(Canon_ShootMode,canon_shootmode)

static struct deviceproptableu16 canon_eos_autoexposuremode[] = {
	{ N_("P"),		0x0000, 0 },
	{ N_("TV"),		0x0001, 0 },
	{ N_("AV"),		0x0002, 0 },
	{ N_("Manual"),		0x0003, 0 },
	{ N_("Bulb"),		0x0004, 0 },
	{ N_("A_DEP"),		0x0005, 0 },
	{ N_("DEP"),		0x0006, 0 },
	{ N_("Custom"),		0x0007, 0 },
	{ N_("Lock"),		0x0008, 0 },
	{ N_("Green"),		0x0009, 0 },
	{ N_("Night Portrait"),	0x000a, 0 },
	{ N_("Sports"),		0x000b, 0 },
	{ N_("Portrait"),	0x000c, 0 },
	{ N_("Landscape"),	0x000d, 0 },
	{ N_("Closeup"),	0x000e, 0 },
	{ N_("Flash Off"),	0x000f, 0 },
};
GENERIC16TABLE(Canon_EOS_AutoExposureMode,canon_eos_autoexposuremode)

static struct deviceproptableu8 canon_flash[] = {
	{ N_("off"),				0, 0 },
	{ N_("auto"),				1, 0 },
	{ N_("on"),				2, 0 },
	{ N_("red eye suppression"),		3, 0 },
	{ N_("fill in"), 			4, 0 },
	{ N_("auto + red eye suppression"),	5, 0 },
	{ N_("on + red eye suppression"),	6, 0 },
};
GENERIC8TABLE(Canon_FlashMode,canon_flash)

static struct deviceproptableu8 nikon_internalflashmode[] = {
	{ N_("iTTL"),		0, 0 },
	{ N_("Manual"),		1, 0 },
	{ N_("Commander"),	2, 0 },
	{ N_("Repeating"),	3, 0 }, /* stroboskop */
};
GENERIC8TABLE(Nikon_InternalFlashMode,nikon_internalflashmode)

static struct deviceproptableu8 nikon_flashcommandermode[] = {
	{ N_("TTL"),		0, 0 },
	{ N_("Auto Aperture"),	1, 0 },
	{ N_("Full Manual"),	2, 0 },
};
GENERIC8TABLE(Nikon_FlashCommanderMode,nikon_flashcommandermode)

static struct deviceproptableu8 nikon_flashcommanderpower[] = {
	{ N_("Full"),		0, 0 },
	{ "1/2",		1, 0 },
	{ "1/4",		2, 0 },
	{ "1/8",		3, 0 },
	{ "1/16",		4, 0 },
	{ "1/32",		5, 0 },
	{ "1/64",		6, 0 },
	{ "1/128",		7, 0 },
};
GENERIC8TABLE(Nikon_FlashCommanderPower,nikon_flashcommanderpower)

/* 0xd1d3 */
static struct deviceproptableu8 nikon_flashcommandchannel[] = {
	{ "1",		0, 0 },
	{ "2",		1, 0 },
	{ "3",		2, 0 },
	{ "4",		3, 0 },
};
GENERIC8TABLE(Nikon_FlashCommandChannel,nikon_flashcommandchannel)

/* 0xd1d4 */
static struct deviceproptableu8 nikon_flashcommandselfmode[] = {
	{ N_("TTL"),		0, 0 },
	{ N_("Manual"),		1, 0 },
	{ N_("Off"),		2, 0 },
};
GENERIC8TABLE(Nikon_FlashCommandSelfMode,nikon_flashcommandselfmode)

/* 0xd1d5, 0xd1d8, 0xd1da */
static struct deviceproptableu8 nikon_flashcommandXcompensation[] = {
	{ "-3.0",		0, 0 },
	{ "-2.7",		1, 0 },
	{ "-2.3",		2, 0 },
	{ "-2.0",		3, 0 },
	{ "-1.7",		4, 0 },
	{ "-1.3",		5, 0 },
	{ "-1.0",		6, 0 },
	{ "-0.7",		7, 0 },
	{ "-0.3",		8, 0 },
	{ "0.0",		9, 0 },
	{ "0.3",		10, 0 },
	{ "0.7",		11, 0 },
	{ "1.0",		12, 0 },
	{ "1.3",		13, 0 },
	{ "1.7",		14, 0 },
	{ "2.0",		15, 0 },
	{ "2.3",		16, 0 },
	{ "2.7",		17, 0 },
	{ "3.0",		18, 0 },
};
GENERIC8TABLE(Nikon_FlashCommandXCompensation,nikon_flashcommandXcompensation)

/* 0xd1d5, 0xd1d9, 0xd1dc */
static struct deviceproptableu8 nikon_flashcommandXvalue[] = {
	{ N_("Full"),		0, 0 },
	{ "1/1.3",		1, 0 },
	{ "1/1.7",		2, 0 },
	{ "1/2",		3, 0 },
	{ "1/2.5",		4, 0 },
	{ "1/3.2",		5, 0 },
	{ "1/4",		6, 0 },
	{ "1/5",		7, 0 },
	{ "1/6.4",		8, 0 },
	{ "1/8",		9, 0 },
	{ "1/10",		10, 0 },
	{ "1/13",		11, 0 },
	{ "1/16",		12, 0 },
	{ "1/20",		13, 0 },
	{ "1/25",		14, 0 },
	{ "1/32",		15, 0 },
	{ "1/40",		16, 0 },
	{ "1/50",		17, 0 },
	{ "1/64",		18, 0 },
	{ "1/80",		19, 0 },
	{ "1/100",		20, 0 },
	{ "1/128",		21, 0 },
};
GENERIC8TABLE(Nikon_FlashCommandXValue,nikon_flashcommandXvalue)


/* 0xd1d7, 0xd1da */
static struct deviceproptableu8 nikon_flashcommandXmode[] = {
	{ N_("TTL"),		0, 0 },
	{ N_("Auto Aperture"),	1, 0 },
	{ N_("Manual"),		2, 0 },
	{ N_("Off"),		3, 0 },
};
GENERIC8TABLE(Nikon_FlashCommandXMode,nikon_flashcommandXmode)


static struct deviceproptableu8 nikon_afmode[] = {
	{ N_("AF-S"),		0, 0 },
	{ N_("AF-C"),		1, 0 },
	{ N_("AF-A"),		2, 0 },
	{ N_("MF (fixed)"),	3, 0 },
	{ N_("MF (selection)"),	4, 0 },
	/* more for newer */
};
GENERIC8TABLE(Nikon_AFMode,nikon_afmode)

static struct deviceproptableu8 nikon_videomode[] = {
	{ N_("NTSC"),		0, 0 },
	{ N_("PAL"),		1, 0 },
};
GENERIC8TABLE(Nikon_VideoMode,nikon_videomode)

static struct deviceproptableu8 flash_modemanualpower[] = {
	{ N_("Full"),	0x00, 0 },
	{ "1/2",	0x01, 0 },
	{ "1/4",	0x02, 0 },
	{ "1/8",	0x03, 0 },
	{ "1/16",	0x04, 0 },
	{ "1/32",	0x05, 0 },
};
GENERIC8TABLE(Nikon_FlashModeManualPower,flash_modemanualpower)

static struct deviceproptableu8 canon_meteringmode[] = {
	{ N_("Center-weighted"),		0, 0 },
	{ N_("Spot"),				1, 0 },
	{ N_("Average"),			2, 0 },
	{ N_("Evaluative"),			3, 0 },
	{ N_("Partial"),			4, 0 },
	{ N_("Center-weighted average"),	5, 0 },
	{ N_("Spot metering interlocked with AF frame"),	6, 0 },
	{ N_("Multi spot"),			7, 0 },
};
GENERIC8TABLE(Canon_MeteringMode,canon_meteringmode)

static struct deviceproptableu8 canon_eos_picturestyle[] = {
	{ N_("Standard"),	0x81, 0 },
	{ N_("Portrait"),	0x82, 0 },
	{ N_("Landscape"),	0x83, 0 },
	{ N_("Neutral"),	0x84, 0 },
	{ N_("Faithful"),	0x85, 0 },
	{ N_("Monochrome"),	0x86, 0 },
	{ N_("Auto"),		0x87, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("User defined 1"),	0x21, 0 },
	{ N_("User defined 2"),	0x22, 0 },
	{ N_("User defined 3"),	0x23, 0 },
};
GENERIC8TABLE(Canon_EOS_PictureStyle,canon_eos_picturestyle)

static struct deviceproptableu16 canon_shutterspeed[] = {
	{ "auto",	0x0000,0 },
	{ "bulb",	0x0004,0 },
	{ "bulb",	0x000c,0 },
	{ "30",		0x0010,0 },
	{ "25",		0x0013,0 },
	{ "20",		0x0014,0 }, /* + 1/3 */
	{ "20",		0x0015,0 },
	{ "15",		0x0018,0 },
	{ "13",		0x001b,0 },
	{ "10",		0x001c,0 },
	{ "10",		0x001d,0 }, /* 10.4 */
	{ "8",		0x0020,0 },
	{ "6",		0x0023,0 }, /* + 1/3 */
	{ "6",		0x0024,0 },
	{ "5",		0x0025,0 },
	{ "4",		0x0028,0 },
	{ "3.2",	0x002b,0 },
	{ "3",		0x002c,0 },
	{ "2.5",	0x002d,0 },
	{ "2",		0x0030,0 },
	{ "1.6",	0x0033,0 },
	{ "1.5",	0x0034,0 },
	{ "1.3",	0x0035,0 },
	{ "1",		0x0038,0 },
	{ "0.8",	0x003b,0 },
	{ "0.7",	0x003c,0 },
	{ "0.6",	0x003d,0 },
	{ "0.5",	0x0040,0 },
	{ "0.4",	0x0043,0 },
	{ "0.3",	0x0044,0 },
	{ "0.3",	0x0045,0 }, /* 1/3 */
	{ "1/4",	0x0048,0 },
	{ "1/5",	0x004b,0 },
	{ "1/6",	0x004c,0 },
	{ "1/6",	0x004d,0 }, /* 1/3? */
	{ "1/8",	0x0050,0 },
	{ "1/10",	0x0053,0 }, /* 1/3? */
	{ "1/10",	0x0054,0 },
	{ "1/13",	0x0055,0 },
	{ "1/15",	0x0058,0 },
	{ "1/20",	0x005b,0 }, /* 1/3? */
	{ "1/20",	0x005c,0 },
	{ "1/25",	0x005d,0 },
	{ "1/30",	0x0060,0 },
	{ "1/40",	0x0063,0 },
	{ "1/45",	0x0064,0 },
	{ "1/50",	0x0065,0 },
	{ "1/60",	0x0068,0 },
	{ "1/80",	0x006b,0 },
	{ "1/90",	0x006c,0 },
	{ "1/100",	0x006d,0 },
	{ "1/125",	0x0070,0 },
	{ "1/160",	0x0073,0 },
	{ "1/180",	0x0074,0 },
	{ "1/200",	0x0075,0 },
	{ "1/250",	0x0078,0 },
	{ "1/320",	0x007b,0 },
	{ "1/350",	0x007c,0 },
	{ "1/400",	0x007d,0 },
	{ "1/500",	0x0080,0 },
	{ "1/640",	0x0083,0 },
	{ "1/750",	0x0084,0 },
	{ "1/800",	0x0085,0 },
	{ "1/1000",	0x0088,0 },
	{ "1/1250",	0x008b,0 },
	{ "1/1500",	0x008c,0 },
	{ "1/1600",	0x008d,0 },
	{ "1/2000",	0x0090,0 },
	{ "1/2500",	0x0093,0 },
	{ "1/3000",	0x0094,0 },
	{ "1/3200",	0x0095,0 },
	{ "1/4000",	0x0098,0 },
	{ "1/5000",	0x009b,0 },
	{ "1/6000",	0x009c,0 },
	{ "1/6400",	0x009d,0 },
	{ "1/8000",	0x00a0,0 },
};
GENERIC16TABLE(Canon_ShutterSpeed,canon_shutterspeed)


static struct deviceproptableu16 canon_focuspoints[] = {
	{ N_("Focusing Point on Center Only, Manual"),	0x1000, 0 },
	{ N_("Focusing Point on Center Only, Auto"),	0x1001, 0 },
	{ N_("Multiple Focusing Points (No Specification), Manual"),	0x3000, 0 },
	{ N_("Multiple Focusing Points, Auto"),		0x3001, 0 },
	{ N_("Multiple Focusing Points (Right)"),	0x3002, 0 },
	{ N_("Multiple Focusing Points (Center)"),	0x3003, 0 },
	{ N_("Multiple Focusing Points (Left)"),	0x3004, 0 },
};
GENERIC16TABLE(Canon_FocusingPoint,canon_focuspoints)

static struct deviceproptableu8 canon_size[] = {
	{ N_("Large"),		0x00, 0 },
	{ N_("Medium 1"),	0x01, 0 },
	{ N_("Medium 2"),	0x03, 0 },
	{ N_("Medium 3"),	0x07, 0 },
	{ N_("Small"),		0x02, 0 },
};
GENERIC8TABLE(Canon_Size,canon_size)

static struct deviceproptableu8 sony_size[] = {
	{ N_("Large"),		0x01, 0 },
	{ N_("Medium"),		0x02, 0 },
	{ N_("Small"),		0x03, 0 },
};
GENERIC8TABLE(Sony_ImageSize,sony_size)

static struct deviceproptableu8 nikon1_size[] = {
	{ N_("Small"),		0x00, 0 },
	{ N_("Medium"),		0x01, 0 },
	{ N_("Large"),		0x02, 0 },
};
GENERIC8TABLE(Nikon1_ImageSize,nikon1_size)

static struct deviceproptableu8 sony_aspectratio[] = {
	{ N_("16:9"),		0x01, 0 },
	{ N_("3:2"),		0x02, 0 },
};
GENERIC8TABLE(Sony_AspectRatio,sony_aspectratio)

/* actually in 1/10s of a second, but only 3 values in use */
static struct deviceproptableu16 canon_selftimer[] = {
	{ N_("Not used"),	0,	0 },
	{ N_("10 seconds"),	100,	0 },
	{ N_("2 seconds"), 	20,	0 },
};
GENERIC16TABLE(Canon_SelfTimer,canon_selftimer)

/* FIXME: Canon EOS 5D Mark III has more enumerations. Need to determine
 * what they mean.
 */
/* actually it is a flag value, 1 = TFT, 2 = PC */
static struct deviceproptableu16 canon_eos_cameraoutput[] = {
	{ N_("Off"),		0, 0 }, /*On 5DM3, LCD/TFT is off, mirror down and optical view finder enabled */
	{ N_("TFT"),		1, 0 },
	{ N_("PC"), 		2, 0 },
	{ N_("TFT + PC"), 	3, 0 },
	/*The below show up as possible on Canon 5D MIII, but unknown what exactly they are */
	{ N_("Setting 4"),	4, 0 },
	{ N_("Setting 5"),	5, 0 }, /*5DM3 - Appears to be dup of enum: 1. Probably something else is also enabled. */
	{ N_("Setting 6"),	6, 0 }, /*5DM3 - Appears to be dup of enum: 2. */
	{ N_("Setting 7"),	7, 0 }, /*5DM3 - Appears to be dup of enum: 3. */
};
GENERIC16TABLE(Canon_EOS_CameraOutput,canon_eos_cameraoutput)

static struct deviceproptableu16 canon_eos_evfrecordtarget[] = {
	{ N_("None"),		0, 0 },
	{ N_("Card"),		4, 0 },
};
GENERIC16TABLE(Canon_EOS_EVFRecordTarget,canon_eos_evfrecordtarget)

/* values currently unknown */
static struct deviceproptableu16 canon_eos_evfmode[] = {
	{ "0",	0, 0 },
	{ "1",	1, 0 },
};
GENERIC16TABLE(Canon_EOS_EVFMode,canon_eos_evfmode)

#if 0 /* reimplement with viewfinder on/off below */
static struct deviceproptableu8 canon_cameraoutput[] = {
	{ N_("Undefined"),	0, 0 },
	{ N_("LCD"),		1, 0 },
	{ N_("Video OUT"), 	2, 0 },
	{ N_("Off"), 		3, 0 },
};
GENERIC8TABLE(Canon_CameraOutput,canon_cameraoutput)
#endif

static int
_get_Canon_CameraOutput(CONFIG_GET_ARGS) {
	int i,isset=0;
	char buf[30];

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char *x;

		switch (dpd->FORM.Enum.SupportedValue[i].u8) {
		default:sprintf(buf,_("Unknown %d"),dpd->FORM.Enum.SupportedValue[i].u8);
			x=buf;
			break;
		case 1: x=_("LCD");break;
		case 2: x=_("Video OUT");break;
		case 3: x=_("Off");break;
		}
		gp_widget_add_choice (*widget,x);
		if (dpd->FORM.Enum.SupportedValue[i].u8 == dpd->CurrentValue.u8) {
			gp_widget_set_value (*widget,x);
			isset = 1;
		}
	}
	if (!isset) {
		sprintf(buf,_("Unknown %d"),dpd->CurrentValue.u8);
		gp_widget_set_value (*widget,buf);
	}
	return GP_OK;
}

static int
_put_Canon_CameraOutput(CONFIG_PUT_ARGS) {
	int	u, i;
	char	*value;
	PTPParams *params = &camera->pl->params;

	CR (gp_widget_get_value(widget, &value));

	u = -1;
	if (!strcmp(value,_("LCD"))) { u = 1; }
	if (!strcmp(value,_("Video OUT"))) { u = 2; }
	if (!strcmp(value,_("Off"))) { u = 3; }
	if (sscanf(value,_("Unknown %d"),&i)) { u = i; }
	C_PARAMS (u != -1);

	if ((u==1) || (u==2)) {
		if (ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOn)) {
			if (!params->canon_viewfinder_on)  {
				if (LOG_ON_PTP_E (ptp_canon_viewfinderon (params)) == PTP_RC_OK);
					params->canon_viewfinder_on=1;
			}
		}
	}
	if (u==3) {
		if (ptp_operation_issupported(params, PTP_OC_CANON_ViewfinderOff)) {
			if (params->canon_viewfinder_on)  {
				if (LOG_ON_PTP_E (ptp_canon_viewfinderoff (params)) == PTP_RC_OK)
					params->canon_viewfinder_on=0;
			}
		}
	}
	propval->u8 = u;
	return GP_OK;
}


static struct deviceproptableu16 canon_isospeed[] = {
	{ N_("Factory Default"),0xffff, 0 },
	{ "6",			0x0028, 0 },
	{ "12",			0x0030, 0 },
	{ "25",			0x0038, 0 },
	{ "50",			0x0040, 0 },
	{ "64",			0x0043, 0 },
	{ "80",			0x0045, 0 },
	{ "100",		0x0048, 0 },
	{ "125",		0x004b, 0 },
	{ "160",		0x004d, 0 },
	{ "200",		0x0050, 0 },
	{ "250",		0x0053, 0 },
	{ "320",		0x0055, 0 },
	{ "400",		0x0058, 0 },
	{ "500",		0x005b, 0 },
	{ "640",		0x005d, 0 },
	{ "800",		0x0060, 0 },
	{ "1000",		0x0063, 0 },
	{ "1250",		0x0065, 0 },
	{ "1600",		0x0068, 0 },
	{ "2000",		0x006b, 0 },
	{ "2500",		0x006d, 0 },
	{ "3200",		0x0070, 0 },
	{ "4000",		0x0073, 0 },
	{ "5000",		0x0075, 0 },
	{ "6400",		0x0078, 0 },
	{ "8000",		0x007b, 0 },
	{ "10000",		0x007d, 0 },
	{ "12800",		0x0080, 0 },
	{ "16000",		0x0083, 0 },
	{ "20000",		0x0085, 0 },
	{ "25600",		0x0088, 0 },
	{ "51200",		0x0090, 0 },
	{ "102400",		0x0098, 0 },
	{ N_("Auto"),		0x0000, 0 },
};
GENERIC16TABLE(Canon_ISO,canon_isospeed)

/* see ptp-pack.c:ptp_unpack_EOS_ImageFormat */
static struct deviceproptableu16 canon_eos_image_format[] = {
	{ N_("RAW"),				0x0400, 0 },
	{ N_("mRAW"),				0x1400, 0 },
	{ N_("sRAW"),				0x2400, 0 },
	{ N_("Large Fine JPEG"),		0x0300, 0 },
	{ N_("Large Normal JPEG"),		0x0200, 0 },
	{ N_("Medium Fine JPEG"),		0x1300, 0 },
	{ N_("Medium Normal JPEG"),		0x1200, 0 },
	{ N_("Small Fine JPEG"),		0x2300, 0 },
	{ N_("Small Normal JPEG"),		0x2200, 0 },
	{ N_("Small Fine JPEG"),		0xd300, 0 },
	{ N_("Small Normal JPEG"),		0xd200, 0 },
	{ N_("Smaller JPEG"),			0xe300, 0 },
	{ N_("Tiny JPEG"),			0xf300, 0 },
	{ N_("RAW + Large Fine JPEG"),		0x0403, 0 },
	{ N_("mRAW + Large Fine JPEG"),		0x1403, 0 },
	{ N_("sRAW + Large Fine JPEG"),		0x2403, 0 },
	{ N_("RAW + Medium Fine JPEG"),		0x0413, 0 },
	{ N_("mRAW + Medium Fine JPEG"),	0x1413, 0 },
	{ N_("sRAW + Medium Fine JPEG"),	0x2413, 0 },
	{ N_("RAW + Small Fine JPEG"),		0x0423, 0 },
	{ N_("RAW + Small Fine JPEG"),		0x04d3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Small Fine JPEG"),		0x1423, 0 },
	{ N_("mRAW + Small Fine JPEG"),		0x14d3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Small Fine JPEG"),		0x2423, 0 },
	{ N_("sRAW + Small Fine JPEG"),		0x24d3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("RAW + Large Normal JPEG"),	0x0402, 0 },
	{ N_("mRAW + Large Normal JPEG"),	0x1402, 0 },
	{ N_("sRAW + Large Normal JPEG"),	0x2402, 0 },
	{ N_("RAW + Medium Normal JPEG"),	0x0412, 0 },
	{ N_("mRAW + Medium Normal JPEG"),	0x1412, 0 },
	{ N_("sRAW + Medium Normal JPEG"),	0x2412, 0 },
	{ N_("RAW + Small Normal JPEG"),	0x0422, 0 },
	{ N_("RAW + Small Normal JPEG"),	0x04d2, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Small Normal JPEG"),	0x1422, 0 },
	{ N_("mRAW + Small Normal JPEG"),	0x14d2, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Small Normal JPEG"),	0x2422, 0 },
	{ N_("sRAW + Small Normal JPEG"),	0x24d2, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("RAW + Smaller JPEG"),		0x04e3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Smaller JPEG"),		0x14e3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Smaller JPEG"),		0x24e3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("RAW + Tiny JPEG"),		0x04f3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Tiny JPEG"),		0x14f3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Tiny JPEG"),		0x24f3, 0 }, /*Canon EOS 5D Mark III*/
	/* There are more RAW + 'smallish' JPEG combinations for at least the 5DM3 possible.
	   Axel was simply to lazy to exercise the combinatorial explosion. :-/ */
};
GENERIC16TABLE(Canon_EOS_ImageFormat,canon_eos_image_format)

static struct deviceproptableu16 canon_eos_aeb[] = {
	{ N_("off"),		0x0000, 0 },
	{ "+/- 1/3",		0x0003, 0 },
	{ "+/- 1/2",		0x0004, 0 },
	{ "+/- 2/3",		0x0005, 0 },
	{ "+/- 1",		0x0008, 0 },
	{ "+/- 1 1/3",		0x000b, 0 },
	{ "+/- 1 1/2",		0x000c, 0 },
	{ "+/- 1 2/3",		0x000d, 0 },
	{ "+/- 2",		0x0010, 0 },
	{ "+/- 2 1/3",		0x0013, 0 },
	{ "+/- 2 1/2",		0x0014, 0 },
	{ "+/- 2 2/3",		0x0015, 0 },
	{ "+/- 3",		0x0018, 0 },
};
GENERIC16TABLE(Canon_EOS_AEB,canon_eos_aeb)

static struct deviceproptableu16 canon_eos_drive_mode[] = {
	{ N_("Single"),			0x0000, 0 },
	{ N_("Continuous"),		0x0001, 0 },
	{ N_("Continuous high speed"),	0x0004, 0 },
	{ N_("Continuous low speed"),	0x0005, 0 },
	{ N_("Timer 10 sec"),		0x0010, 0 },
	{ N_("Timer 2 sec"),		0x0011, 0 },
	{ N_("Single silent"),		0x0013, 0 },
	{ N_("Continuous silent"),	0x0014, 0 },
};
GENERIC16TABLE(Canon_EOS_DriveMode,canon_eos_drive_mode)

static int
_get_ISO(CONFIG_GET_ARGS) {
	int i;

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char	buf[20];

		sprintf(buf,"%d",dpd->FORM.Enum.SupportedValue[i].u16);
		gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u16 == dpd->CurrentValue.u16)
			gp_widget_set_value (*widget,buf);
	}
	return (GP_OK);
}

static int
_put_ISO(CONFIG_PUT_ARGS)
{
	char *value;
	unsigned int	u;

	CR (gp_widget_get_value(widget, &value));

	if (sscanf(value, "%ud", &u)) {
		propval->u16 = u;
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_get_ISO32(CONFIG_GET_ARGS) {
	int i;

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char	buf[20];

		sprintf(buf,"%d",dpd->FORM.Enum.SupportedValue[i].u32);
		gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32)
			gp_widget_set_value (*widget,buf);
	}
	return GP_OK;
}

static int
_put_ISO32(CONFIG_PUT_ARGS)
{
	char *value;
	unsigned int	u;

	CR (gp_widget_get_value(widget, &value));

	if (sscanf(value, "%ud", &u)) {
		propval->u32 = u;
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_get_Sony_ISO(CONFIG_GET_ARGS) {
	int	i,isset=0;
	char	buf[50];

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		if (dpd->FORM.Enum.SupportedValue[i].u32 == 0x00ffffffU) {
			sprintf(buf,_("Auto ISO"));
		} else if (dpd->FORM.Enum.SupportedValue[i].u32 == 0x01ffffffU) {
			sprintf(buf,_("Auto ISO Multi Frame Noise Reduction"));
		} else {
			if (dpd->FORM.Enum.SupportedValue[i].u32 & 0xff000000) {
				sprintf(buf,_("%d Multi Frame Noise Reduction"),dpd->FORM.Enum.SupportedValue[i].u32 & 0xffff);
			} else {
				sprintf(buf,"%d",dpd->FORM.Enum.SupportedValue[i].u32);
			}
		}
		gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32) {
			isset=1;
			gp_widget_set_value (*widget,buf);
		}
	}
	if (!isset) {
		if (dpd->CurrentValue.u32 == 0x00ffffffU)
			sprintf(buf,_("Auto ISO"));
		else if (dpd->CurrentValue.u32 == 0x01ffffffU)
			sprintf(buf,_("Auto ISO Multi Frame Noise Reduction"));
		else {
			if (dpd->CurrentValue.u32 & 0xff000000) {
				sprintf(buf,_("%d Multi Frame Noise Reduction"),dpd->CurrentValue.u32 & 0xffff);
			} else {
				sprintf(buf,"%d",dpd->CurrentValue.u32);
			}
		}
		gp_widget_set_value (*widget,buf);
	}
	return GP_OK;
}

static int
_put_Sony_ISO(CONFIG_PUT_ARGS)
{
	char 		*value;
	uint32_t	u;
	PTPParams	*params = &(camera->pl->params);

	CR (gp_widget_get_value(widget, &value));
	if (!strcmp(value,_("Auto ISO"))) {
		u = 0x00ffffff;
		goto setiso;
	}
	if (!strcmp(value,_("Auto ISO Multi Frame Noise Reduction"))) {
		u = 0x01ffffff;
		goto setiso;
	}

	if (!sscanf(value, "%ud", &u))
		return GP_ERROR;

	if (strstr(value,_("Multi Frame Noise Reduction")))
		u |= 0x10000;

setiso:
	propval->u32 = u;
	return _put_sony_value_u32(params, PTP_DPC_SONY_ISO, u, 1);
}


static int
_get_Milliseconds(CONFIG_GET_ARGS) {
	unsigned int i, min, max;

	if (!(dpd->FormFlag & (PTP_DPFF_Range|PTP_DPFF_Enumeration)))
		return (GP_ERROR);
	if ((dpd->DataType != PTP_DTC_UINT32) && (dpd->DataType != PTP_DTC_UINT16))
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		unsigned int t;

		if (dpd->DataType == PTP_DTC_UINT32)
			t = dpd->CurrentValue.u32;
		else
			t = dpd->CurrentValue.u16;
		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			char	buf[20];
			unsigned int x;

			if (dpd->DataType == PTP_DTC_UINT32)
				x = dpd->FORM.Enum.SupportedValue[i].u32;
			else
				x = dpd->FORM.Enum.SupportedValue[i].u16;

			sprintf(buf,"%0.3fs",x/1000.0);
			gp_widget_add_choice (*widget,buf);
			if (x == t)
				gp_widget_set_value (*widget,buf);
		}
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		unsigned int s;

		if (dpd->DataType == PTP_DTC_UINT32) {
			min = dpd->FORM.Range.MinimumValue.u32;
			max = dpd->FORM.Range.MaximumValue.u32;
			s = dpd->FORM.Range.StepSize.u32;
		} else {
			min = dpd->FORM.Range.MinimumValue.u16;
			max = dpd->FORM.Range.MaximumValue.u16;
			s = dpd->FORM.Range.StepSize.u16;
		}
		for (i=min; i<=max; i+=s) {
			char buf[20];

			sprintf (buf, "%0.3fs", i/1000.0);
			gp_widget_add_choice (*widget, buf);
			if (	((dpd->DataType == PTP_DTC_UINT32) && (dpd->CurrentValue.u32 == i)) ||
				((dpd->DataType == PTP_DTC_UINT16) && (dpd->CurrentValue.u16 == i))
			   )
				gp_widget_set_value (*widget, buf);
		}

	}
	return GP_OK;
}

static int
_put_Milliseconds(CONFIG_PUT_ARGS)
{
	char *value;
	float	f;

	CR (gp_widget_get_value(widget, &value));

	if (sscanf(value, "%f", &f)) {
		if (dpd->DataType == PTP_DTC_UINT32)
			propval->u32 = f*1000;
		else
			propval->u16 = f*1000;
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_get_FNumber(CONFIG_GET_ARGS) {
	int i;

	GP_LOG_D ("get_FNumber");
	if (!(dpd->FormFlag & (PTP_DPFF_Enumeration|PTP_DPFF_Range)))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);

	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		gp_widget_set_name (*widget, menu->name);

		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			char	buf[20];

			sprintf(buf,"f/%g",(dpd->FORM.Enum.SupportedValue[i].u16*1.0)/100.0);
			gp_widget_add_choice (*widget,buf);
			if (dpd->FORM.Enum.SupportedValue[i].u16 == dpd->CurrentValue.u16)
				gp_widget_set_value (*widget,buf);
		}
		GP_LOG_D ("get_FNumber via enum");
	} else { /* Range */
		float value_float;

		gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
		gp_widget_set_name (*widget, menu->name);
		gp_widget_set_range (*widget,
				dpd->FORM.Range.MinimumValue.u16/100.0,
				dpd->FORM.Range.MaximumValue.u16/100.0,
				dpd->FORM.Range.StepSize.u16/100.0
				);
		value_float = dpd->CurrentValue.u16/100.0;
		gp_widget_set_value (*widget, &value_float);
		GP_LOG_D ("get_FNumber via float");
	}
	return GP_OK;
}

static int
_put_FNumber(CONFIG_PUT_ARGS)
{
	int i;

	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		char	*value;
		float	f;

		CR (gp_widget_get_value(widget, &value));
		if (strstr (value, "f/") == value)
			value += strlen("f/");

		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			char	buf[20];

			sprintf(buf,"%g",(dpd->FORM.Enum.SupportedValue[i].u16*1.0)/100.0);
			if (!strcmp (buf, value)) {
				propval->u16 = dpd->FORM.Enum.SupportedValue[i].u16;
				return GP_OK;
			}
		}
		if (sscanf(value, "%g", &f)) {
			propval->u16 = f*100;
			return GP_OK;
		}
	} else { /* RANGE uses float */
		float fvalue;

		CR (gp_widget_get_value (widget, &fvalue));
		propval->u16 = fvalue*100;
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_put_Sony_FNumber(CONFIG_PUT_ARGS)
{
	float			fvalue;
	PTPParams		*params = &(camera->pl->params);

	CR (gp_widget_get_value (widget, &fvalue));

	propval->u16 = fvalue*100; /* probably not used */
	return _put_sony_value_u16 (params, PTP_DPC_FNumber, fvalue*100, 0);
}

static int
_get_ExpTime(CONFIG_GET_ARGS) {
	int		i;
	PTPParams	*params = &(camera->pl->params);

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT32)
		return (GP_ERROR);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char	buf[20];

		if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) {
			if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xffffffff) {
				sprintf(buf,_("Bulb"));
				goto choicefound;
			}
			if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xfffffffd) {
				sprintf(buf,_("Time"));
				goto choicefound;
			}
		}
		sprintf (buf,_("%0.4fs"), (1.0*dpd->FORM.Enum.SupportedValue[i].u32)/10000.0);
choicefound:
		gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32)
			gp_widget_set_value (*widget,buf);
	}
	return (GP_OK);
}

static int
_put_ExpTime(CONFIG_PUT_ARGS)
{
	unsigned int	i, delta, xval, ival1, ival2, ival3;
	float		val;
	char		*value;
	PTPParams	*params = &(camera->pl->params);

	CR (gp_widget_get_value (widget, &value));

	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_NIKON) {
		if (!strcmp(value,_("Bulb"))) {
			propval->u32 = 0xffffffff;
			return GP_OK;
		}
		if (!strcmp(value,_("Time"))) {
			propval->u32 = 0xfffffffd;
			return GP_OK;
		}
	}

	if (sscanf(value,_("%d %d/%d"),&ival1,&ival2,&ival3) == 3) {
		GP_LOG_D ("%d %d/%d case", ival1, ival2, ival3);
		val = ((float)ival1) + ((float)ival2/(float)ival3);
	} else if (sscanf(value,_("%d/%d"),&ival1,&ival2) == 2) {
		GP_LOG_D ("%d/%d case", ival1, ival2);
		val = (float)ival1/(float)ival2;
	} else if (!sscanf(value,_("%f"),&val)) {
		GP_LOG_E ("failed to parse: %s", value);
		return (GP_ERROR);
	} else
		GP_LOG_D ("%fs case", val);
	val = val*10000.0;
	delta = 1000000;
	xval = val;
	/* match the closest value */
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		/*GP_LOG_D ("delta is currently %d, val is %f, supval is %u, abs is %u",delta,val,dpd->FORM.Enum.SupportedValue[i].u32,abs(val - dpd->FORM.Enum.SupportedValue[i].u32));*/
		if (abs(val - dpd->FORM.Enum.SupportedValue[i].u32)<delta) {
			xval = dpd->FORM.Enum.SupportedValue[i].u32;
			delta = abs(val - dpd->FORM.Enum.SupportedValue[i].u32);
		}
	}
	GP_LOG_D ("value %s is %f, closest match was %d",value,val,xval);
	propval->u32 = xval;
	return (GP_OK);
}

static int
_get_Sharpness(CONFIG_GET_ARGS) {
	int i, min, max, t;

	if (!(dpd->FormFlag & (PTP_DPFF_Enumeration|PTP_DPFF_Range)))
		return (GP_ERROR);
	if ((dpd->DataType != PTP_DTC_UINT8) && (dpd->DataType != PTP_DTC_INT8))
		return (GP_ERROR);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	if (dpd->FormFlag & PTP_DPFF_Range) {
		int s;

		if (dpd->DataType == PTP_DTC_UINT8) {
			min = dpd->FORM.Range.MinimumValue.u8;
			max = dpd->FORM.Range.MaximumValue.u8;
			s = dpd->FORM.Range.StepSize.u8;
		} else {
			min = dpd->FORM.Range.MinimumValue.i8;
			max = dpd->FORM.Range.MaximumValue.i8;
			s = dpd->FORM.Range.StepSize.i8;
		}
		for (i=min;i<=max; i+=s) {
			char buf[20];

			sprintf (buf, "%d%%", (i-min)*100/(max-min));
			gp_widget_add_choice (*widget, buf);
			if (	((dpd->DataType == PTP_DTC_UINT8) && (dpd->CurrentValue.u8 == i)) ||
				((dpd->DataType == PTP_DTC_INT8)  && (dpd->CurrentValue.i8 == i))
			)
				gp_widget_set_value (*widget, buf);
		}
	}

	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		min = 256;
		max = -256;
		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			if (dpd->DataType == PTP_DTC_UINT8) {
				if (dpd->FORM.Enum.SupportedValue[i].u8 < min)
					min = dpd->FORM.Enum.SupportedValue[i].u8;
				if (dpd->FORM.Enum.SupportedValue[i].u8 > max)
					max = dpd->FORM.Enum.SupportedValue[i].u8;
			} else {
				if (dpd->FORM.Enum.SupportedValue[i].i8 < min)
					min = dpd->FORM.Enum.SupportedValue[i].i8;
				if (dpd->FORM.Enum.SupportedValue[i].i8 > max)
					max = dpd->FORM.Enum.SupportedValue[i].i8;
			}
		}
		if (dpd->DataType == PTP_DTC_UINT8)
			t = dpd->CurrentValue.u8;
		else
			t = dpd->CurrentValue.i8;
		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			char buf[20];
			int x;

			if (dpd->DataType == PTP_DTC_UINT8)
				x = dpd->FORM.Enum.SupportedValue[i].u8;
			else
				x = dpd->FORM.Enum.SupportedValue[i].i8;

			sprintf (buf, "%d%%", (x-min)*100/(max-min));
			gp_widget_add_choice (*widget, buf);
			if (t == x)
				gp_widget_set_value (*widget, buf);
		}
	}
	return (GP_OK);
}

static int
_put_Sharpness(CONFIG_PUT_ARGS) {
	const char *val;
	int i, min, max, x;

	gp_widget_get_value (widget, &val);
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		min = 256;
		max = -256;
		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			if (dpd->DataType == PTP_DTC_UINT8) {
				if (dpd->FORM.Enum.SupportedValue[i].u8 < min)
					min = dpd->FORM.Enum.SupportedValue[i].u8;
				if (dpd->FORM.Enum.SupportedValue[i].u8 > max)
					max = dpd->FORM.Enum.SupportedValue[i].u8;
			} else {
				if (dpd->FORM.Enum.SupportedValue[i].i8 < min)
					min = dpd->FORM.Enum.SupportedValue[i].i8;
				if (dpd->FORM.Enum.SupportedValue[i].i8 > max)
					max = dpd->FORM.Enum.SupportedValue[i].i8;
			}
		}
		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			char buf[20];

			if (dpd->DataType == PTP_DTC_UINT8)
				x = dpd->FORM.Enum.SupportedValue[i].u8;
			else
				x = dpd->FORM.Enum.SupportedValue[i].i8;

			sprintf (buf, "%d%%", (x-min)*100/(max-min));
			if (!strcmp(buf, val)) {
				if (dpd->DataType == PTP_DTC_UINT8)
					propval->u8 = x;
				else
					propval->i8 = x;
				return GP_OK;
			}
		}
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		int s;

		if (dpd->DataType == PTP_DTC_UINT8) {
			min = dpd->FORM.Range.MinimumValue.u8;
			max = dpd->FORM.Range.MaximumValue.u8;
			s = dpd->FORM.Range.StepSize.u8;
		} else {
			min = dpd->FORM.Range.MinimumValue.i8;
			max = dpd->FORM.Range.MaximumValue.i8;
			s = dpd->FORM.Range.StepSize.i8;
		}
		for (i=min; i<=max; i+=s) {
			char buf[20];

			sprintf (buf, "%d%%", (i-min)*100/(max-min));
			if (strcmp (buf, val))
				continue;
			if (dpd->DataType == PTP_DTC_UINT8)
				propval->u8 = i;
			else
				propval->i8 = i;
			return GP_OK;
		}
	}
	return GP_ERROR;
}


static struct deviceproptableu16 exposure_program_modes[] = {
	{ "M",			0x0001, 0 },
	{ "P",			0x0002, 0 },
	{ "A",			0x0003, 0 },
	{ "S",			0x0004, 0 },
	{ N_("Creative"),	0x0005, 0 },
	{ N_("Action"),		0x0006, 0 },
	{ N_("Portrait"),	0x0007, 0 },
	{ N_("Auto"),		0x8010, PTP_VENDOR_NIKON},
	{ N_("Portrait"),	0x8011, PTP_VENDOR_NIKON},
	{ N_("Landscape"),	0x8012, PTP_VENDOR_NIKON},
	{ N_("Macro"),		0x8013, PTP_VENDOR_NIKON},
	{ N_("Sports"),		0x8014, PTP_VENDOR_NIKON},
	{ N_("Night Portrait"),	0x8015, PTP_VENDOR_NIKON},
	{ N_("Night Landscape"),0x8016, PTP_VENDOR_NIKON},
	{ N_("Children"),	0x8017, PTP_VENDOR_NIKON},
	{ N_("Automatic (No Flash)"),	0x8018, PTP_VENDOR_NIKON},

	{ N_("Intelligent Auto"),		0x8000, PTP_VENDOR_SONY},
	{ N_("Superior Auto"),			0x8001, PTP_VENDOR_SONY},
	{ N_("Movie"),				0x8050, PTP_VENDOR_SONY},
	{ N_("Tele-zoom Cont. Priority AE"),	0x8031, PTP_VENDOR_SONY},
	{ N_("Sweep Panorama"),			0x8041, PTP_VENDOR_SONY},
	{ N_("Intelligent Auto Flash Off"),	0x8060, PTP_VENDOR_SONY},
	{ N_("Sports Action"),			0x8011, PTP_VENDOR_SONY},
	{ N_("Macro"),				0x8015, PTP_VENDOR_SONY},
	{ N_("Landscape"),			0x8014, PTP_VENDOR_SONY},
	{ N_("Sunset"),				0x8012, PTP_VENDOR_SONY},
	{ N_("Night Scene"),			0x8013, PTP_VENDOR_SONY},
	{ N_("Hand-held Twilight"),		0x8016, PTP_VENDOR_SONY},
	{ N_("Night Portrait"),			0x8017, PTP_VENDOR_SONY},
	{ N_("Picture Effect"),			0x8070, PTP_VENDOR_SONY},
};
GENERIC16TABLE(ExposureProgram,exposure_program_modes)

static struct deviceproptableu8 nikon_scenemode[] = {
	{ N_("Night landscape"),	0, 0 },
	{ N_("Party/Indoor"),		1, 0 },
	{ N_("Beach/Snow"),		2, 0 },
	{ N_("Sunset"),			3, 0 },
	{ N_("Dusk/Dawn"),		4, 0 },
	{ N_("Pet Portrait"),		5, 0 },
	{ N_("Candlelight"),		6, 0 },
	{ N_("Blossom"),		7, 0 },
	{ N_("Autumn colors"),		8, 0 },
	{ N_("Food"),			9, 0 },
	/* ? */
	{ N_("Portrait"),		13, 0 },
	{ N_("Landscape"),		14, 0 },
	{ N_("Child"),			15, 0 },
	{ N_("Sports"),			16, 0 },
	{ N_("Close up"),		17, 0 },
	{ N_("Night Portrait"),		18, 0 },

};
GENERIC8TABLE(NIKON_SceneMode,nikon_scenemode);

/* Nikon 1 S1 specials in here */
static struct deviceproptableu8 nikon_1_j3_iso[] = {
	/* values from a J3 */
	{ N_("A6400 (160-6400)"),	0x01, 0 },
	{ N_("A3200 (160-3200)"),	0x02, 0 },
	{ N_("A800 (160-800)"),		0x03, 0 },

	/* these same same for all 1 */
	{ "100",	0x0a, 0 },
	{ "110",	0x0b, 0 },
	{ "125",	0x0c, 0 },
	{ "140",	0x0d, 0 },
	{ "160",	0x0e, 0 },
	{ "180",	0x0f, 0 },
	{ "200",	0x10, 0 },
	{ "220",	0x11, 0 },
	{ "250",	0x12, 0 },
	{ "280",	0x13, 0 },
	{ "320",	0x14, 0 },
	{ "360",	0x15, 0 },
	{ "400",	0x16, 0 },
	{ "400",	0x16, 0 },
	{ "450",	0x17, 0 },
	{ "500",	0x18, 0 },
	{ "560",	0x19, 0 },
	{ "640",	0x1a, 0 },
	{ "720",	0x1b, 0 },
	{ "800",	0x1c, 0 },
	{ "900",	0x1d, 0 },
	{ "1000",	0x1e, 0 },
	{ "1100",	0x1f, 0 },
	{ "1250",	0x20, 0 },
	{ "1400",	0x21, 0 },
	{ "1600",	0x22, 0 },
	{ "1800",	0x23, 0 },
	{ "2000",	0x24, 0 },
	{ "2200",	0x25, 0 },
	{ "2500",	0x26, 0 },
	{ "2800",	0x27, 0 },
	{ "3200",	0x28, 0 },
	{ "6400",	0x2e, 0 },
	/* more unknown values */
};
GENERIC8TABLE(Nikon_1_J3_ISO,nikon_1_j3_iso);

/* Nikon 1 S1 specials in here */
static struct deviceproptableu8 nikon_1_s1_iso[] = {
	{ "100",	0x0a, 0 },
	{ "110",	0x0b, 0 },
	{ "125",	0x0c, 0 },
	{ "140",	0x0d, 0 },
	{ "160",	0x0e, 0 },
	{ "180",	0x0f, 0 },
	{ "200",	0x10, 0 },
	{ "220",	0x11, 0 },
	{ "250",	0x12, 0 },
	{ "280",	0x13, 0 },
	{ "320",	0x14, 0 },
	{ "360",	0x15, 0 },
	{ "400",	0x16, 0 },
	{ "450",	0x17, 0 },
	{ "500",	0x18, 0 },
	{ "560",	0x19, 0 },
	{ "640",	0x1a, 0 },
	{ "720",	0x1b, 0 },
	{ "800",	0x1c, 0 },
	{ "900",	0x1d, 0 },
	{ "1000",	0x1e, 0 },
	{ "1100",	0x1f, 0 },
	{ "1250",	0x20, 0 },
	{ "1400",	0x21, 0 },
	{ "1600",	0x22, 0 },
	{ "1800",	0x23, 0 },
	{ "2000",	0x24, 0 },
	{ "2200",	0x25, 0 },
	{ "2500",	0x26, 0 },
	{ "2800",	0x27, 0 },
	{ "3200",	0x28, 0 },
	{ "6400",	0x2e, 0 },
};
GENERIC8TABLE(Nikon_1_S1_ISO,nikon_1_s1_iso);

/* Generic Nikon 1 ISO */
static struct deviceproptableu8 nikon_1_iso[] = {
	{ "100",	0x0a, 0 },
	{ "110",	0x0b, 0 },
	{ "125",	0x0c, 0 },
	{ "140",	0x0d, 0 },
	{ "160",	0x0e, 0 },
	{ "180",	0x0f, 0 },
	{ "200",	0x10, 0 },
	{ "220",	0x11, 0 },
	{ "250",	0x12, 0 },
	{ "280",	0x13, 0 },
	{ "320",	0x14, 0 },
	{ "360",	0x15, 0 },
	{ "400",	0x16, 0 },
	{ "450",	0x17, 0 },
	{ "500",	0x18, 0 },
	{ "560",	0x19, 0 },
	{ "640",	0x1a, 0 },
	{ "720",	0x1b, 0 },
	{ "800",	0x1c, 0 },
	{ "900",	0x1d, 0 },
	{ "1000",	0x1e, 0 },
	{ "1100",	0x1f, 0 },
	{ "1250",	0x20, 0 },
	{ "1400",	0x21, 0 },
	{ "1600",	0x22, 0 },
	{ "1800",	0x23, 0 },
	{ "2000",	0x24, 0 },
	{ "2200",	0x25, 0 },
	{ "2500",	0x26, 0 },
	{ "2800",	0x27, 0 },
	{ "3200",	0x28, 0 },
	{ "6400",	0x2e, 0 },
};
GENERIC8TABLE(Nikon_1_ISO,nikon_1_iso);

static struct deviceproptableu8 nikon_1_whitebalance[] = {
	/* values from a J3 */
	{ N_("Auto"),			0x00, 0 },
	{ N_("Tungsten"),		0x01, 0 },
	{ N_("Flourescent"),		0x02, 0 },
	{ N_("Daylight"),		0x03, 0 },
	{ N_("Flash"),			0x04, 0 },
	{ N_("Cloudy"),			0x05, 0 },
	{ N_("Shade"),			0x06, 0 },

	/* these are not in the enum range on the j3 ... but reported? */
	{ N_("Water"),			0x0a, 0 },
	{ N_("Preset"),			0x08, 0 },
};
GENERIC8TABLE(Nikon_1_WhiteBalance,nikon_1_whitebalance);

static struct deviceproptableu8 nikon_hdrhighdynamic[] = {
	{ N_("Auto"),	0, 0 },
	{ N_("1 EV"),	1, 0 },
	{ N_("2 EV"),	2, 0 },
	{ N_("3 EV"),	3, 0 },
};
GENERIC8TABLE(Nikon_HDRHighDynamic,nikon_hdrhighdynamic);

static struct deviceproptableu8 nikon_hdrsmoothing[] = {
	{ N_("Auto"),		3, 0 },
	{ N_("Low"),		2, 0 },
	{ N_("Normal"),		1, 0 },
	{ N_("High"),		0, 0 },
	{ N_("Extra high"),	4, 0 },
};
GENERIC8TABLE(Nikon_HDRSmoothing,nikon_hdrsmoothing);

static struct deviceproptableu16 nikon_d7100_exposure_program_modes[] = {
	{ "M",			0x0001, 0 },
	{ "P",			0x0002, 0 },
	{ "A",			0x0003, 0 },
	{ "S",			0x0004, 0 },
	{ N_("Auto"),		0x8010, PTP_VENDOR_NIKON},
	{ N_("Portrait"),	0x8011, PTP_VENDOR_NIKON},
	{ N_("Landscape"),	0x8012, PTP_VENDOR_NIKON},
	{ N_("Macro"),		0x8013, PTP_VENDOR_NIKON},
	{ N_("Sports"),		0x8014, PTP_VENDOR_NIKON},
	{ N_("No Flash"),	0x8016, PTP_VENDOR_NIKON},
	{ N_("Children"),	0x8017, PTP_VENDOR_NIKON},
	{ N_("Scene"),		0x8018, PTP_VENDOR_NIKON},
	{ N_("Effects"),	0x8019, PTP_VENDOR_NIKON},
	{ N_("U1"),		0x8050, PTP_VENDOR_NIKON},
	{ N_("U2"),		0x8051, PTP_VENDOR_NIKON},
};
GENERIC16TABLE(NIKON_D7100_ExposureProgram,nikon_d7100_exposure_program_modes)

static struct deviceproptableu16 nikon_d5100_exposure_program_modes[] = {
	{ "M",			0x0001, 0 },
	{ "P",			0x0002, 0 },
	{ "A",			0x0003, 0 },
	{ "S",			0x0004, 0 },
	{ N_("Auto"),		0x8010, PTP_VENDOR_NIKON},
	{ N_("Portrait"),	0x8011, PTP_VENDOR_NIKON},
	{ N_("Landscape"),	0x8012, PTP_VENDOR_NIKON},
	{ N_("Macro"),		0x8013, PTP_VENDOR_NIKON},
	{ N_("Sports"),		0x8014, PTP_VENDOR_NIKON},
	{ N_("No Flash"),	0x8016, PTP_VENDOR_NIKON},
	{ N_("Children"),	0x8017, PTP_VENDOR_NIKON},
	{ N_("Scene"),		0x8018, PTP_VENDOR_NIKON},
	{ N_("Effects"),	0x8019, PTP_VENDOR_NIKON},
};
GENERIC16TABLE(NIKON_D5100_ExposureProgram,nikon_d5100_exposure_program_modes)

static struct deviceproptableu8 nikon_1_exposure_program_modes[] = {
	{ "P",			0x00, 0 },
	{ "S",			0x01, 0 },
	{ "A",			0x02, 0 },
	{ "M",			0x03, 0 },
	{ N_("Night Landscape"),0x04, 0 },
	{ N_("Night Portrait"),	0x05, 0 },
	{ N_("Back Light"),	0x06, 0 },
	{ N_("Panorama"),	0x07, 0 },
	{ N_("Smoothing"),	0x08, 0 },
	{ N_("Tilt-Shift"),	0x09, 0 },
	{ N_("Select Color"),	0x0a, 0 },
};
GENERIC8TABLE(NIKON_1_ExposureProgram,nikon_1_exposure_program_modes)

static struct deviceproptableu16 capture_mode[] = {
	{ N_("Single Shot"),		0x0001, 0 },
	{ N_("Burst"),			0x0002, 0 },
	{ N_("Timelapse"),		0x0003, 0 },
	{ N_("Continuous Low Speed"),	0x8010, PTP_VENDOR_NIKON},
	{ N_("Timer"),			0x8011, PTP_VENDOR_NIKON},
	{ N_("Mirror Up"),		0x8012, PTP_VENDOR_NIKON},
	{ N_("Remote"),			0x8013, PTP_VENDOR_NIKON},
	{ N_("Quick Response Remote"),	0x8014, PTP_VENDOR_NIKON}, /* others nikons */
	{ N_("Delayed Remote"),		0x8015, PTP_VENDOR_NIKON}, /* d90 */
	{ N_("Quiet Release"),		0x8016, PTP_VENDOR_NIKON}, /* d5000 */

	{ N_("Continuous Low Speed"),	0x8012, PTP_VENDOR_SONY},
	{ N_("Selftimer 2s"),		0x8005, PTP_VENDOR_SONY},
	{ N_("Selftimer 10s"),		0x8004, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.3 Steps"),	0x8337, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.7 Steps"),	0x8377, PTP_VENDOR_SONY},
	{ N_("Bracketing C 1.0 Steps"),	0x8311, PTP_VENDOR_SONY},
	{ N_("Bracketing C 2.0 Steps"),	0x8321, PTP_VENDOR_SONY},
	{ N_("Bracketing C 3.0 Steps"),	0x8331, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.3 Steps"),	0x8336, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.7 Steps"),	0x8376, PTP_VENDOR_SONY},
	{ N_("Bracketing S 1.0 Steps"),	0x8310, PTP_VENDOR_SONY},
	{ N_("Bracketing S 2.0 Steps"),	0x8320, PTP_VENDOR_SONY},
	{ N_("Bracketing S 3.0 Steps"),	0x8330, PTP_VENDOR_SONY},
	{ N_("Bracketing WB Lo"),	0x8018, PTP_VENDOR_SONY},
	{ N_("Bracketing WB Hi"),	0x8028, PTP_VENDOR_SONY},
/*
	{ N_("Continuous"),		0x8001, PTP_VENDOR_CASIO},
	{ N_("Prerecord"),		0x8002, PTP_VENDOR_CASIO},
*/
};
GENERIC16TABLE(CaptureMode,capture_mode)

static struct deviceproptableu16 focus_metering[] = {
	{ N_("Centre-spot"),	0x0001, 0 },
	{ N_("Multi-spot"),	0x0002, 0 },
	{ N_("Single Area"),	0x8010, PTP_VENDOR_NIKON},
	{ N_("Closest Subject"),0x8011, PTP_VENDOR_NIKON},
	{ N_("Group Dynamic"),  0x8012, PTP_VENDOR_NIKON},
	{ N_("Single-area AF"),	0x8001, PTP_VENDOR_FUJI},
	{ N_("Dynamic-area AF"),0x8002, PTP_VENDOR_FUJI},
	{ N_("Group-dynamic AF"),0x8003, PTP_VENDOR_FUJI},
	{ N_("Dynamic-area AF with closest subject priority"),0x8004, PTP_VENDOR_FUJI},
};
GENERIC16TABLE(FocusMetering,focus_metering)

static struct deviceproptableu16 nikon_d7100_focus_metering[] = {
	{ N_("Auto"),0x8011, PTP_VENDOR_NIKON},
	{ N_("Single Area"),	0x8010, PTP_VENDOR_NIKON},
	{ N_("Dynamic Area (9)"),	0x0002, 0 },
	{ N_("Dynamic Area (21)"),  0x8013, PTP_VENDOR_NIKON},
	{ N_("Dynamic Area (51)"),  0x8014, PTP_VENDOR_NIKON},
	{ N_("3D Tracking"),  0x8012, PTP_VENDOR_NIKON},
};
GENERIC16TABLE(Nikon_D7100_FocusMetering,nikon_d7100_focus_metering)


static struct deviceproptableu8 nikon_colormodel[] = {
	{ N_("sRGB (portrait)"),0x00, 0 },
	{ N_("AdobeRGB"),	0x01, 0 },
	{ N_("sRGB (nature)"),	0x02, 0 },
};
GENERIC8TABLE(Nikon_ColorModel,nikon_colormodel)

static struct deviceproptableu8 nikon_colorspace[] = {
	{ N_("sRGB"),		0x00, 0 },
	{ N_("AdobeRGB"),	0x01, 0 },
};
GENERIC8TABLE(Nikon_ColorSpace,nikon_colorspace)

static struct deviceproptableu16 canon_eos_colorspace[] = {
	{ N_("sRGB"), 		0x01, 0 },
	{ N_("AdobeRGB"),	0x02, 0 },
};
GENERIC16TABLE(Canon_EOS_ColorSpace,canon_eos_colorspace)


static struct deviceproptableu8 nikon_evstep[] = {
	{ "1/3",	0, 0 },
	{ "1/2",	1, 0 },
};
GENERIC8TABLE(Nikon_EVStep,nikon_evstep)

static struct deviceproptableu8 nikon_orientation[] = {
	{ "0'",		0, 0 },
	{ "270'",	1, 0 },
	{ "90'",	2, 0 },
	{ "180'",	3, 0 },
};
GENERIC8TABLE(Nikon_CameraOrientation,nikon_orientation)

static struct deviceproptableu16 canon_orientation[] = {
	{ "0'",		0, 0 },
	{ "90'",	1, 0 },
	{ "180'",	2, 0 },
	{ "270'",	3, 0 },
};

static int
_get_Canon_CameraOrientation(CONFIG_GET_ARGS) {
	char	orient[50]; /* needs also to fit the translated string */
	int	i;

	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<sizeof(canon_orientation)/sizeof(canon_orientation[0]);i++) {
		if (canon_orientation[i].value != dpd->CurrentValue.u16)
			continue;
		gp_widget_set_value (*widget, canon_orientation[i].label);
		return GP_OK;
	}
	sprintf (orient, _("Unknown value 0x%04x"), dpd->CurrentValue.u16);
	gp_widget_set_value (*widget, orient);
	return GP_OK;
}

static int
_get_Nikon_AngleLevel(CONFIG_GET_ARGS) {
	char	orient[20];

	if (dpd->DataType != PTP_DTC_INT32)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	sprintf (orient, "%.f'", dpd->CurrentValue.i32/65536.0);
	gp_widget_set_value (*widget, orient);
	return GP_OK;
}


static struct deviceproptableu8 nikon_afsensor[] = {
	{ N_("Centre"),	0x00, 0 },
	{ N_("Top"),	0x01, 0 },
	{ N_("Bottom"),	0x02, 0 },
	{ N_("Left"),	0x03, 0 },
	{ N_("Right"),	0x04, 0 },
};
GENERIC8TABLE(Nikon_AutofocusArea,nikon_afsensor)


static struct deviceproptableu16 exposure_metering[] = {
	{ N_("Average"),	0x0001, 0 },
	{ N_("Center Weighted"),0x0002, 0 },
	{ N_("Multi Spot"),	0x0003, 0 },
	{ N_("Center Spot"),	0x0004, 0 },
	{ N_("Spot"),		0x8001, PTP_VENDOR_FUJI },
};
GENERIC16TABLE(ExposureMetering,exposure_metering)


static struct deviceproptableu16 flash_mode[] = {
	{ N_("Automatic Flash"),		0x0001, 0 },
	{ N_("Flash off"),			0x0002, 0 },
	{ N_("Fill flash"),			0x0003, 0 },
	{ N_("Red-eye automatic"),		0x0004, 0 },
	{ N_("Red-eye fill"),			0x0005, 0 },
	{ N_("External sync"),			0x0006, 0 },
	{ N_("Auto"),				0x8010, PTP_VENDOR_NIKON},
	{ N_("Auto Slow Sync"),			0x8011, PTP_VENDOR_NIKON},
	{ N_("Rear Curtain Sync + Slow Sync"),	0x8012, PTP_VENDOR_NIKON},
	{ N_("Red-eye Reduction + Slow Sync"),	0x8013, PTP_VENDOR_NIKON},
	{ N_("Front-curtain sync"),			0x8001, PTP_VENDOR_FUJI},
	{ N_("Red-eye reduction"),			0x8002, PTP_VENDOR_FUJI},
	{ N_("Red-eye reduction with slow sync"),	0x8003, PTP_VENDOR_FUJI},
	{ N_("Slow sync"),				0x8004, PTP_VENDOR_FUJI},
	{ N_("Rear-curtain with slow sync"),		0x8005, PTP_VENDOR_FUJI},
	{ N_("Rear-curtain sync"),			0x8006, PTP_VENDOR_FUJI},
	
	{ N_("Rear Curtain Sync"),			0x8003, PTP_VENDOR_SONY},
	{ N_("Wireless Sync"),				0x8004, PTP_VENDOR_SONY},
	{ N_("Slow Sync"),				0x8032, PTP_VENDOR_SONY},
};
GENERIC16TABLE(FlashMode,flash_mode)

static struct deviceproptableu16 effect_modes[] = {
	{ N_("Standard"),	0x0001, 0 },
	{ N_("Black & White"),	0x0002, 0 },
	{ N_("Sepia"),		0x0003, 0 },
};
GENERIC16TABLE(EffectMode,effect_modes)

static struct deviceproptableu8 nikon_effect_modes[] = {
	{ N_("Night Vision"),		0x00, 0 },
	{ N_("Color sketch"),		0x01, 0 },
	{ N_("Miniature effect"),	0x02, 0 },
	{ N_("Selective color"),	0x03, 0 },
	{ N_("Silhouette"),		0x04, 0 },
	{ N_("High key"),		0x05, 0 },
	{ N_("Low key"),		0x06, 0 },
};
GENERIC8TABLE(NIKON_EffectMode,nikon_effect_modes)


static int
_get_FocalLength(CONFIG_GET_ARGS) {
	float value_float , start=0.0, end=0.0, step=0.0;
	int i;

	if (!(dpd->FormFlag & (PTP_DPFF_Range|PTP_DPFF_Enumeration)))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT32)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		/* Find the range we need. */
		start = 10000.0;
		end = 0.0;
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
			float cur = dpd->FORM.Enum.SupportedValue[i].u32 / 100.0;

			if (cur < start) start = cur;
			if (cur > end)   end = cur;
		}
		step = 1.0;
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		start = dpd->FORM.Range.MinimumValue.u32/100.0;
		end = dpd->FORM.Range.MaximumValue.u32/100.0;
		step = dpd->FORM.Range.StepSize.u32/100.0;
	}
	gp_widget_set_range (*widget, start, end, step);
	value_float = dpd->CurrentValue.u32/100.0;
	gp_widget_set_value (*widget, &value_float);
	return (GP_OK);
}

static int
_put_FocalLength(CONFIG_PUT_ARGS) {
	int i;
	float value_float;
	uint32_t curdiff, newval;

	CR (gp_widget_get_value (widget, &value_float));
	propval->u32 = 100*value_float;
	if (dpd->FormFlag & PTP_DPFF_Range)
		return GP_OK;
	/* If FocalLength is enumerated, we need to hit the 
	 * values exactly, otherwise nothing will happen.
	 * (problem encountered on my Nikon P2)
	 */
	curdiff = 10000;
	newval = propval->u32;
	for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
		uint32_t diff = abs(dpd->FORM.Enum.SupportedValue[i].u32  - propval->u32);

		if (diff < curdiff) {
			newval = dpd->FORM.Enum.SupportedValue[i].u32;
			curdiff = diff;
		}
	}
	propval->u32 = newval;
	return GP_OK;
}

static int
_get_FocusDistance(CONFIG_GET_ARGS) {
	if (!(dpd->FormFlag & (PTP_DPFF_Range|PTP_DPFF_Enumeration)))
		return (GP_ERROR);

	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);

	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		int i, valset = 0;
		char buf[200];

		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		gp_widget_set_name (*widget, menu->name);

		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {

			if (dpd->FORM.Enum.SupportedValue[i].u16 == 0xFFFF)
				strcpy (buf, _("infinite"));
			else
				sprintf (buf, _("%d mm"), dpd->FORM.Enum.SupportedValue[i].u16);
			gp_widget_add_choice (*widget,buf);
			if (dpd->CurrentValue.u16 == dpd->FORM.Enum.SupportedValue[i].u16) {
				gp_widget_set_value (*widget, buf);
				valset = 1;
			}
		}
		if (!valset) {
			sprintf (buf, _("%d mm"), dpd->CurrentValue.u16);
			gp_widget_set_value (*widget, buf);
		}
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		float value_float , start=0.0, end=0.0, step=0.0;

		gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
		gp_widget_set_name (*widget, menu->name);

		start = dpd->FORM.Range.MinimumValue.u16/100.0;
		end = dpd->FORM.Range.MaximumValue.u16/100.0;
		step = dpd->FORM.Range.StepSize.u16/100.0;
		gp_widget_set_range (*widget, start, end, step);
		value_float = dpd->CurrentValue.u16/100.0;
		gp_widget_set_value (*widget, &value_float);
	}
	return GP_OK;
}

static int
_put_FocusDistance(CONFIG_PUT_ARGS) {
	int val;
	const char *value_str;

	if (dpd->FormFlag & PTP_DPFF_Range) {
		float value_float;

		CR (gp_widget_get_value (widget, &value_float));
		propval->u16 = value_float;
		return GP_OK;
	}
	/* else ENUMeration */
	CR (gp_widget_get_value (widget, &value_str));
	if (!strcmp (value_str, _("infinite"))) {
		propval->u16 = 0xFFFF;
		return GP_OK;
	}
	C_PARAMS (sscanf(value_str, _("%d mm"), &val));
	propval->u16 = val;
	return GP_OK;
}

static int
_get_Nikon_ShutterSpeed(CONFIG_GET_ARGS) {
	int i, valset = 0;
	char buf[200];
	int x,y;

	if (dpd->DataType != PTP_DTC_UINT32)
		return (GP_ERROR);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
		if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xffffffff) {
			sprintf(buf,_("Bulb"));
			goto choicefound;
		}
		if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xfffffffe) {
			sprintf(buf,_("x 200"));
			goto choicefound;
		}
		if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xfffffffd) {
			sprintf(buf,_("Time"));
			goto choicefound;
		}
		x = dpd->FORM.Enum.SupportedValue[i].u32>>16;
		y = dpd->FORM.Enum.SupportedValue[i].u32&0xffff;
		if (y == 1) { /* x/1 */
			sprintf (buf, "%d", x);
		} else {
			sprintf (buf, "%d/%d",x,y);
		}
choicefound:
		gp_widget_add_choice (*widget,buf);
		if (dpd->CurrentValue.u32 == dpd->FORM.Enum.SupportedValue[i].u32) {
			gp_widget_set_value (*widget, buf);
			valset = 1;
		}
	}
	if (!valset) {
		x = dpd->CurrentValue.u32>>16;
		y = dpd->CurrentValue.u32&0xffff;
		if (y == 1) {
			sprintf (buf, "%d",x);
		} else {
			sprintf (buf, "%d/%d",x,y);
		}
		gp_widget_set_value (*widget, buf);
	}
	return GP_OK;
}

static int
_put_Nikon_ShutterSpeed(CONFIG_PUT_ARGS) {
	int x,y;
	const char *value_str;

	gp_widget_get_value (widget, &value_str);

	if (!strcmp(value_str,_("Bulb"))) {
		propval->u32 = 0xffffffff;
		return GP_OK;
	}
	if (!strcmp(value_str,_("x 200"))) {
		propval->u32 = 0xfffffffe;
		return GP_OK;
	}
	if (!strcmp(value_str,_("Time"))) {
		propval->u32 = 0xfffffffd;
		return GP_OK;
	}

	if (strchr(value_str, '/')) {
		if (2 != sscanf (value_str, "%d/%d", &x, &y))
			return GP_ERROR;
	} else {
		if (!sscanf (value_str, "%d", &x))
			return GP_ERROR;
		y = 1;
	}
	propval->u32 = (x<<16) | y;
	return GP_OK;
}

static int
_get_Ricoh_ShutterSpeed(CONFIG_GET_ARGS) {
	int i, valset = 0;
	char buf[200];
	int x,y;

	if (dpd->DataType != PTP_DTC_UINT64)
		return GP_ERROR;
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
		if (dpd->FORM.Enum.SupportedValue[i].u64 == 0) {
			sprintf(buf,_("Auto"));
			goto choicefound;
		}
		x = dpd->FORM.Enum.SupportedValue[i].u64>>32;
		y = dpd->FORM.Enum.SupportedValue[i].u64&0xffffffff;
		if (y == 1) {
			sprintf (buf, "1/%d", x);
		} else {
			sprintf (buf, "%d/%d",y,x);
		}
choicefound:
		gp_widget_add_choice (*widget,buf);
		if (dpd->CurrentValue.u64 == dpd->FORM.Enum.SupportedValue[i].u64) {
			gp_widget_set_value (*widget, buf);
			valset = 1;
		}
	}
	if (!valset) {
		x = dpd->CurrentValue.u64>>32;
		y = dpd->CurrentValue.u64&0xffffffff;
		if (y == 1) {
			sprintf (buf, "1/%d",x);
		} else {
			sprintf (buf, "%d/%d",y,x);
		}
		gp_widget_set_value (*widget, buf);
	}
	return GP_OK;
}

static int
_put_Ricoh_ShutterSpeed(CONFIG_PUT_ARGS) {
	int x,y;
	const char *value_str;

	gp_widget_get_value (widget, &value_str);

	if (!strcmp(value_str,_("Auto"))) {
		propval->u64 = 0;
		return GP_OK;
	}

	if (strchr(value_str, '/')) {
		if (2 != sscanf (value_str, "%d/%d", &y, &x))
			return GP_ERROR;
	} else {
		if (!sscanf (value_str, "%d", &x))
			return GP_ERROR;
		y = 1;
	}
	propval->u64 = ((unsigned long)x<<32) | y;
	return GP_OK;
}

static int
_get_Sony_ShutterSpeed(CONFIG_GET_ARGS) {
	int x,y;
	char buf[20];

	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	if (dpd->CurrentValue.u32 == 0) {
		strcpy(buf,_("Bulb"));
	} else {
		x = dpd->CurrentValue.u32>>16;
		y = dpd->CurrentValue.u32&0xffff;
		if (y == 1)
			sprintf (buf, "%d",x);
		else
			sprintf (buf, "%d/%d",x,y);
	}
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
_put_Sony_ShutterSpeed(CONFIG_PUT_ARGS) {
	int			x,y,a,b;
	const char		*val;
	float 			old,new;
	PTPPropertyValue	value;
	uint32_t		new32, origval;
	PTPParams		*params = &(camera->pl->params);
	GPContext 		*context = ((PTPData *) params->data)->context;
	time_t			start,end;

	CR (gp_widget_get_value (widget, &val));

	x = dpd->CurrentValue.u32>>16;
	y = dpd->CurrentValue.u32&0xffff;
	old = ((float)x)/(float)y;

	if (!strcmp(val,_("Bulb"))) {
		new32 = 0;
		x = 65536; y = 1;
	} else {
		if (2!=sscanf(val, "%d/%d", &x, &y)) {
			if (1==sscanf(val,"%d", &x)) {
				y = 1;
			} else {
				return GP_ERROR_BAD_PARAMETERS;
			}
		}
		new32 = (x<<16)|y;
	}
	new = ((float)x)/(float)y;
	do {
		origval = dpd->CurrentValue.u32;
		if (old > new)
			value.u8 = 0x01;
		else
			value.u8 = 0xff;
		a = dpd->CurrentValue.u32>>16;
		b = dpd->CurrentValue.u32&0xffff;
		C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_ShutterSpeed, &value, PTP_DTC_UINT8 ));

		GP_LOG_D ("shutterspeed value is (0x%x vs target 0x%x)", origval, new32);

		/* we tell the camera to do it, but it takes around 0.7 seconds for the SLT-A58 */
		time(&start);
		do {
			C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
			C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ShutterSpeed, dpd));

			if (dpd->CurrentValue.u32 == new32) {
				GP_LOG_D ("Value matched!");
				break;
			}
			a = dpd->CurrentValue.u32>>16;
			b = dpd->CurrentValue.u32&0xffff;
			if ((a*y != 0) && (a*y == b*x)) {
				GP_LOG_D ("Value matched via math(tm) %d/%d == %d/%d!",x,y,a,b);
				break;
			}

			if (dpd->CurrentValue.u32 != origval) {
				GP_LOG_D ("value changed (0x%x vs 0x%x vs target 0x%x), next step....", dpd->CurrentValue.u32, origval, new32);
				break;
			}

			usleep(200*1000);

			time(&end);
		} while (end-start <= 3);

		if (dpd->CurrentValue.u32 == new32) {
			GP_LOG_D ("Value matched!");
			break;
		}
		if ((a*y != 0) && (a*y == b*x)) {
			GP_LOG_D ("Value matched via math(tm) %d/%d == %d/%d!",x,y,a,b);
			break;
		}
		if (dpd->CurrentValue.u32 == origval) {
			GP_LOG_D ("value did not change (0x%x vs 0x%x vs target 0x%x), not good ...", dpd->CurrentValue.u32, origval, new32);
			break;
		}
	} while (1);
	propval->u32 = new;
	return GP_OK;
}


static int
_get_Nikon_FocalLength(CONFIG_GET_ARGS) {
	char	len[20];

	if (dpd->DataType != PTP_DTC_UINT32)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	sprintf (len, "%.0f mm", dpd->CurrentValue.u32 * 0.01);
	gp_widget_set_value (*widget, len);
	return (GP_OK);
}

static int
_get_Nikon_ApertureAtFocalLength(CONFIG_GET_ARGS) {
	char	len[20];

	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	sprintf (len, "%.0f", dpd->CurrentValue.u16 * 0.01);
	gp_widget_set_value (*widget, len);
	return (GP_OK);
}

static int
_get_Nikon_LightMeter(CONFIG_GET_ARGS) {
	char	meter[20];

	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	sprintf (meter, "%.1f", dpd->CurrentValue.i8 * 0.08333);
	gp_widget_set_value (*widget, meter);
	return (GP_OK);
}


static int
_get_Nikon_FlashExposureCompensation(CONFIG_GET_ARGS) {
	float value_float;

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_range (*widget,
		dpd->FORM.Range.MinimumValue.i8/6.0,
		dpd->FORM.Range.MaximumValue.i8/6.0,
		dpd->FORM.Range.StepSize.i8/6.0
	);
	value_float = dpd->CurrentValue.i8/6.0;
	gp_widget_set_value (*widget, &value_float);
	return (GP_OK);
}

static int
_put_Nikon_FlashExposureCompensation(CONFIG_PUT_ARGS) {
	float val;

	CR (gp_widget_get_value(widget, &val));
	propval->i8 = 6.0*val;
	return GP_OK;
}

static int
_get_Nikon_LowLight(CONFIG_GET_ARGS) {
	float value_float;

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_range (*widget,
		dpd->FORM.Range.MinimumValue.u8,
		dpd->FORM.Range.MaximumValue.u8,
		dpd->FORM.Range.StepSize.u8
	);
	value_float = dpd->CurrentValue.u8;
	gp_widget_set_value (*widget, &value_float);
	return (GP_OK);
}

static int
_get_Canon_EOS_WBAdjust(CONFIG_GET_ARGS) {
	int i, valset = 0;
	char buf[200];

	if (dpd->DataType != PTP_DTC_INT16)
		return (GP_ERROR);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
		sprintf (buf, "%d", dpd->FORM.Enum.SupportedValue[i].i16);
		gp_widget_add_choice (*widget,buf);
		if (dpd->CurrentValue.i16 == dpd->FORM.Enum.SupportedValue[i].i16) {
			gp_widget_set_value (*widget, buf);
			valset = 1;
		}
	}
	if (!valset) {
		sprintf (buf, "%d",dpd->CurrentValue.i16);
		gp_widget_set_value (*widget, buf);
	}
	return GP_OK;
}

static int
_put_Canon_EOS_WBAdjust(CONFIG_PUT_ARGS) {
	int x;
	const char *value_str;

	gp_widget_get_value (widget, &value_str);
	if (!sscanf (value_str, "%d", &x))
		return GP_ERROR;
	propval->i16 = x;
	return GP_OK;
}

static struct deviceproptableu8 nikon_liveviewaf[] = {
	{ N_("Face-priority AF"),	0, 0 },
	{ N_("Wide-area AF"),		1, 0 },
	{ N_("Normal-area AF"),		2, 0 },
	{ N_("Subject-tracking AF"),	3, 0 },
};
GENERIC8TABLE(Nikon_LiveViewAF,nikon_liveviewaf)

static struct deviceproptableu8 nikon_liveviewaffocus[] = {
	{ N_("Single-servo AF"),	0, 0 },
	{ N_("Full-time-servo AF"),	2, 0 },
	{ N_("Manual Focus"),		4, 0 },
};
GENERIC8TABLE(Nikon_LiveViewAFFocus,nikon_liveviewaffocus)

static struct deviceproptableu8 nikon_afareaillum[] = {
	{ N_("Auto"),		0, 0 },
	{ N_("Off"),		1, 0 },
	{ N_("On"),		2, 0 },
};
GENERIC8TABLE(Nikon_AFAreaIllum,nikon_afareaillum)


static struct deviceproptableu8 nikon_aelaflmode[] = {
	{ N_("AE/AF Lock"),	0x00, 0 },
	{ N_("AE Lock only"),	0x01, 0 },
	{ N_("AF Lock Only"),	0x02, 0 },
	{ N_("AF Lock Hold"),	0x03, 0 },
	{ N_("AF On"),		0x04, 0 },
	{ N_("Flash Level Lock"),0x05, 0 },
};
GENERIC8TABLE(Nikon_AELAFLMode,nikon_aelaflmode)

static struct deviceproptableu8 nikon_lcdofftime[] = {
	{ N_("10 seconds"),	0x00, 0 },
	{ N_("20 seconds"),	0x01, 0 },
	{ N_("1 minute"),	0x02, 0 },
	{ N_("5 minutes"),	0x03, 0 },
	{ N_("10 minutes"),	0x04, 0 },
	{ N_("5 seconds"),	0x05, 0 },	/* d80 observed */
};
GENERIC8TABLE(Nikon_LCDOffTime,nikon_lcdofftime)

static struct deviceproptableu8 nikon_recordingmedia[] = {
	{ N_("Card"),		0x00, 0 },
	{ N_("SDRAM"),		0x01, 0 },
};
GENERIC8TABLE(Nikon_RecordingMedia,nikon_recordingmedia)

static struct deviceproptableu8 nikon_selftimerdelay[] = {
	{ N_("2 seconds"),	0x00, 0 },
	{ N_("5 seconds"),	0x01, 0 },
	{ N_("10 seconds"),	0x02, 0 },
	{ N_("20 seconds"),	0x03, 0 },
};
GENERIC8TABLE(Nikon_SelfTimerDelay,nikon_selftimerdelay)

static struct deviceproptableu8 nikon_centerweight[] = {
	{ N_("6 mm"),	0x00, 0 },
	{ N_("8 mm"),	0x01, 0 },
	{ N_("10 mm"),	0x02, 0 },
	{ N_("12 mm"),	0x03, 0 },
	{ N_("Average"),0x04, 0 },	/* ? */
};
GENERIC8TABLE(Nikon_CenterWeight,nikon_centerweight)

static struct deviceproptableu8 nikon_flashshutterspeed[] = {
	{ N_("1/60"),	0x00, 0 },
	{ N_("1/30"),	0x01, 0 },
	{ N_("1/15"),	0x02, 0 },
	{ N_("1/8"),	0x03, 0 },
	{ N_("1/4"),	0x04, 0 },
	{ N_("1/2"),	0x05, 0 },
	{ N_("1"),	0x06, 0 },
	{ N_("2"),	0x07, 0 },
	{ N_("4"),	0x08, 0 },
	{ N_("8"),	0x09, 0 },
	{ N_("15"),	0x0a, 0 },
	{ N_("30"),	0x0b, 0 },
};
GENERIC8TABLE(Nikon_FlashShutterSpeed,nikon_flashshutterspeed)

static struct deviceproptablei16 fuji_shutterspeed[] = {
	{ N_("bulb"),	-31, 0 },
	{ N_("30s"),	-30, 0 },
	{ N_("25s"),	-28, 0 },
	{ N_("20s"),	-26, 0 },
	{ N_("15s"),	-24, 0 },
	{ N_("13s"),	-22, 0 },
	{ N_("10s"),	-20, 0 },
	{ N_("8s"),	-18, 0 },
	{ N_("6s"),	-16, 0 },
	{ N_("5s"),	-14, 0 },
	{ N_("4s"),	-12, 0 },
	{ N_("3s"),	-10, 0 },
	{ N_("2.5s"),	-8, 0 },
	{ N_("2s"),	-6, 0 },
	{ N_("1.6s"),	-4, 0 },
	{ N_("1.3s"),	-2, 0 },
	{ N_("1s"),	0, 0 },
	{ N_("1/1.3s"),	2, 0 },
	{ N_("1/1.6s"),	4, 0 },
	{ N_("1/2s"),	6, 0 },
	{ N_("1/2.5s"),	8, 0 },
	{ N_("1/3s"),	10, 0 },
	{ N_("1/4s"),	12, 0 },
	{ N_("1/5s"),	14, 0 },
	{ N_("1/6s"),	16, 0 },
	{ N_("1/8s"),	18, 0 },
	{ N_("1/10s"),	20, 0 },
	{ N_("1/13s"),	22, 0 },
	{ N_("1/15s"),	24, 0 },
	{ N_("1/20s"),	26, 0 },
	{ N_("1/25s"),	28, 0 },
	{ N_("1/30s"),	30, 0 },
	{ N_("1/40s"),	32, 0 },
	{ N_("1/50s"),	34, 0 },
	{ N_("1/60s"),	36, 0 },
	{ N_("1/80s"),	38, 0 },
	{ N_("1/100s"),	40, 0 },
	{ N_("1/125s"),	42, 0 },
	{ N_("1/160s"),	44, 0 },
	{ N_("1/200s"),	46, 0 },
	{ N_("1/250s"),	48, 0 },
	{ N_("1/320s"),	50, 0 },
	{ N_("1/400s"),	52, 0 },
	{ N_("1/500s"),	54, 0 },
	{ N_("1/640s"),	56, 0 },
	{ N_("1/800s"),	58, 0 },
	{ N_("1/1000s"),60, 0 },
	{ N_("1/1250s"),62, 0 },
	{ N_("1/1250s"),62, 0 },
	{ N_("1/1600s"),64, 0 },
	{ N_("1/2000s"),66, 0 },
	{ N_("1/2500s"),68, 0 },
	{ N_("1/3200s"),70, 0 },
	{ N_("1/4000s"),72, 0 },
	{ N_("1/5000s"),74, 0 },
	{ N_("1/6400s"),76, 0 },
	{ N_("1/8000s"),78, 0 },
};
GENERICI16TABLE(Fuji_ShutterSpeed,fuji_shutterspeed)

static struct deviceproptableu8 nikon_remotetimeout[] = {
	{ N_("1 minute"),	0x00, 0 },
	{ N_("5 minutes"),	0x01, 0 },
	{ N_("10 minutes"),	0x02, 0 },
	{ N_("15 minutes"),	0x03, 0 },
};
GENERIC8TABLE(Nikon_RemoteTimeout,nikon_remotetimeout)

static struct deviceproptableu8 nikon_optimizeimage[] = {
	{ N_("Normal"),		0x00, 0 },
	{ N_("Vivid"),		0x01, 0 },
	{ N_("Sharper"),	0x02, 0 },
	{ N_("Softer"),		0x03, 0 },
	{ N_("Direct Print"),	0x04, 0 },
	{ N_("Portrait"),	0x05, 0 },
	{ N_("Landscape"),	0x06, 0 },
	{ N_("Custom"),		0x07, 0 },
};
GENERIC8TABLE(Nikon_OptimizeImage,nikon_optimizeimage)

static struct deviceproptableu8 nikon_sharpening[] = {
	{ N_("Auto"),		0x00, 0 },
	{ N_("Normal"),		0x01, 0 },
	{ N_("Low"),		0x02, 0 },
	{ N_("Medium Low"),	0x03, 0 },
	{ N_("Medium high"),	0x04, 0 },
	{ N_("High"),		0x05, 0 },
	{ N_("None"),		0x06, 0 },
};
GENERIC8TABLE(Nikon_Sharpening,nikon_sharpening)

static struct deviceproptableu8 nikon_tonecompensation[] = {
	{ N_("Auto"),		0x00, 0 },
	{ N_("Normal"),		0x01, 0 },
	{ N_("Low contrast"),	0x02, 0 },
	{ N_("Medium Low"),	0x03, 0 },
	{ N_("Medium High"),	0x04, 0 },
	{ N_("High control"),	0x05, 0 },
	{ N_("Custom"),		0x06, 0 },
};
GENERIC8TABLE(Nikon_ToneCompensation,nikon_tonecompensation)

static struct deviceproptableu8 canon_afdistance[] = {
	{ N_("Manual"),			0x00, 0 },
	{ N_("Auto"),			0x01, 0 },
	{ N_("Unknown"),		0x02, 0 },
	{ N_("Zone Focus (Close-up)"),	0x03, 0 },
	{ N_("Zone Focus (Very Close)"),0x04, 0 },
	{ N_("Zone Focus (Close)"),	0x05, 0 },
	{ N_("Zone Focus (Medium)"),	0x06, 0 },
	{ N_("Zone Focus (Far)"),	0x07, 0 },
	{ N_("Zone Focus (Reserved 1)"),0x08, 0 },
	{ N_("Zone Focus (Reserved 2)"),0x09, 0 },
	{ N_("Zone Focus (Reserved 3)"),0x0a, 0 },
	{ N_("Zone Focus (Reserved 4)"),0x0b, 0 },
};
GENERIC8TABLE(Canon_AFDistance,canon_afdistance)


/* Focus Modes as per PTP standard. |0x8000 means vendor specific. */
static struct deviceproptableu16 focusmodes[] = {
	{ N_("Undefined"),	0x0000, 0 },
	{ N_("Manual"),		0x0001, 0 },
	{ N_("Automatic"),	0x0002, 0 },
	{ N_("Automatic Macro"),0x0003, 0 },
	{ N_("AF-S"),		0x8010, PTP_VENDOR_NIKON },
	{ N_("AF-C"),		0x8011, PTP_VENDOR_NIKON },
	{ N_("AF-A"),		0x8012, PTP_VENDOR_NIKON },
	{ N_("Single-Servo AF"),0x8001, PTP_VENDOR_FUJI },
	{ N_("Continuous-Servo AF"),0x8002, PTP_VENDOR_FUJI },

	{ N_("AF-A"),		0x8005, PTP_VENDOR_SONY },
	{ N_("AF-C"),		0x8004, PTP_VENDOR_SONY },
};
GENERIC16TABLE(FocusMode,focusmodes)

static struct deviceproptableu16 eos_focusmodes[] = {
	{ N_("One Shot"),	0x0000, 0 },
	{ N_("AI Servo"),	0x0001, 0 },
	{ N_("AI Focus"),	0x0002, 0 },
	{ N_("Manual"),		0x0003, 0 },
};
GENERIC16TABLE(Canon_EOS_FocusMode,eos_focusmodes)

static struct deviceproptableu16 eos_quickreviewtime[] = {
	{ N_("None"),		0x0000, 0 },
	{ N_("2 seconds"),	0x0002, 0 },
	{ N_("4 seconds"),	0x0004, 0 },
	{ N_("8 seconds"),	0x0008, 0 },
	{ N_("Hold"),		0x00ff, 0 },
};
GENERIC16TABLE(Canon_EOS_QuickReviewTime,eos_quickreviewtime)


static struct deviceproptableu8 canon_whitebalance[] = {
	{ N_("Auto"),			0, 0 },
	{ N_("Daylight"),		1, 0 },
	{ N_("Cloudy"),			2, 0 },
	{ N_("Tungsten"),		3, 0 },
	{ N_("Fluorescent"),		4, 0 },
	{ N_("Custom"),			6, 0 },
	{ N_("Fluorescent H"),		7, 0 },
	{ N_("Color Temperature"),	9, 0 },
	{ N_("Custom Whitebalance PC-1"),	10, 0 },
	{ N_("Custom Whitebalance PC-2"),	11, 0 },
	{ N_("Custom Whitebalance PC-3"),	12, 0 },
	{ N_("Missing Number"),		13, 0 },
	/*{ N_("Flourescent H"),		14, 0 }, ... dup? */
};
GENERIC8TABLE(Canon_WhiteBalance,canon_whitebalance)

/* confirmed against EOS 450D - Marcus */
/* I suspect every EOS uses a different table :( */
static struct deviceproptableu8 canon_eos_whitebalance[] = {
	{ N_("Auto"),		0, 0 },
	{ N_("Daylight"),	1, 0 },
	{ N_("Cloudy"),		2, 0 },
	{ N_("Tungsten"),	3, 0 },
	{ N_("Fluorescent"),	4, 0 },
	{ N_("Flash"),		5, 0 },
	{ N_("Manual"),		6, 0 },
	{"Unknown 7",		7, 0 },
	{ N_("Shadow"),		8, 0 },
	{ N_("Color Temperature"),9, 0 }, /* from eos 40d / 5D Mark II dump */
	{ "Unknown 10",		10, 0 },
	{ "Unknown 11",		11, 0 },
};
GENERIC8TABLE(Canon_EOS_WhiteBalance,canon_eos_whitebalance)


static struct deviceproptableu8 canon_expcompensation[] = {
	{ N_("Factory Default"),0xff, 0 },
	{ "+3",			0x00, 0 },
	{ "+2 2/3",		0x03, 0 },
	{ "+2 1/2",		0x04, 0 },
	{ "+2 1/3",		0x05, 0 },
	{ "+2",			0x08, 0 },
	{ "+1 2/3",		0x0b, 0 },
	{ "+1 1/2",		0x0c, 0 },
	{ "+1 1/3",		0x0d, 0 },
	{ "+1",			0x10, 0 },
	{ "+2/3",		0x13, 0 },
	{ "+1/2",		0x14, 0 },
	{ "+1/3",		0x15, 0 },
	{ "0",			0x18, 0 },
	{ "-1/3",		0x1b, 0 },
	{ "-1/2",		0x1c, 0 },
	{ "-2/3",		0x1d, 0 },
	{ "-1",			0x20, 0 },
	{ "-1 1/3",		0x23, 0 },
	{ "-1 1/2",		0x24, 0 },
	{ "-1 2/3",		0x25, 0 },
	{ "-2",			0x28, 0 },
	{ "-2 1/3",		0x2b, 0 },
	{ "-2 1/2",		0x2c, 0 },
	{ "-2 2/3",		0x2d, 0 },
	{ "-3",			0x30, 0 },
};
GENERIC8TABLE(Canon_ExpCompensation,canon_expcompensation)

static struct deviceproptableu8 canon_expcompensation2[] = {
	{ "5",		0x28, 0 },
	{ "4.6",	0x25, 0 },
	{ "4.3",	0x23, 0 },
	{ "4",		0x20, 0 },
	{ "3.6",	0x1d, 0 },
	{ "3.3",	0x1b, 0 },
	{ "3",		0x18, 0 },
	{ "2.6",	0x15, 0 },
	{ "2.5",	0x14, 0 },
	{ "2.3",	0x13, 0 },
	{ "2",		0x10, 0 },
	{ "1.6",	0x0d, 0 },
	{ "1.5",	0x0c, 0 },
	{ "1.3",	0x0b, 0 },
	{ "1.0",	0x08, 0 },
	{ "0.6",	0x05, 0 },
	{ "0.5",	0x04, 0 },
	{ "0.3",	0x03, 0 },
	{ "0",		0x00, 0 },
	{ "-0.3",	0xfd, 0 },
	{ "-0.5",	0xfc, 0 },
	{ "-0.6",	0xfb, 0 },
	{ "-1.0",	0xf8, 0 },
	{ "-1.3",	0xf5, 0 },
	{ "-1.5",	0xf4, 0 },
	{ "-1.6",	0xf3, 0 },
	{ "-2",		0xf0, 0 },
	{ "-2.3",	0xed, 0 },
	{ "-2.5",	0xec, 0 },
	{ "-2.6",	0xeb, 0 },
	{ "-3",		0xe8, 0 },
	{ "-3.3",	0xe5, 0 },
	{ "-3.6",	0xe3, 0 },
	{ "-4",		0xe0, 0 },
	{ "-4.3",	0xdd, 0 }, /*Canon EOS 5D Mark III*/
	{ "-4.6",	0xdb, 0 },
	{ "-5",		0xd8, 0 },
};
GENERIC8TABLE(Canon_ExpCompensation2,canon_expcompensation2)


static struct deviceproptableu16 canon_photoeffect[] = {
	{ N_("Off"),		0, 0 },
	{ N_("Vivid"),		1, 0 },
	{ N_("Neutral"),	2, 0 },
	{ N_("Low sharpening"),	3, 0 },
	{ N_("Sepia"),		4, 0 },
	{ N_("Black & white"),	5, 0 },
};
GENERIC16TABLE(Canon_PhotoEffect,canon_photoeffect)


static struct deviceproptableu16 canon_aperture[] = {
	{ N_("auto"),	0xffff, 0 },
	{ "1",		0x0008, 0 },
	{ "1.1",	0x000b, 0 },
	{ "1.2",	0x000c, 0 },
	{ "1.2",	0x000d, 0 }, /* (1/3)? */
	{ "1.4",	0x0010, 0 },
	{ "1.6",	0x0013, 0 },
	{ "1.8",	0x0014, 0 },
	{ "1.8",	0x0015, 0 }, /* (1/3)? */
	{ "2",		0x0018, 0 },
	{ "2.2",	0x001b, 0 },
	{ "2.5",	0x001c, 0 },
	{ "2.5",	0x001d, 0 }, /* (1/3)? */
	{ "2.8",	0x0020, 0 },
	{ "3.2",	0x0023, 0 },
	{ "3.5",	0x0024, 0 },
	{ "3.5",	0x0025, 0 }, /* (1/3)? */
	{ "4",		0x0028, 0 },
	{ "4.5",	0x002c, 0 },
	{ "4.5",	0x002b, 0 }, /* (1/3)? */
	{ "5",		0x002d, 0 }, /* 5.6 (1/3)??? */
	{ "5.6",	0x0030, 0 },
	{ "6.3",	0x0033, 0 },
	{ "6.7",	0x0034, 0 },
	{ "7.1",	0x0035, 0 },
	{ "8",		0x0038, 0 },
	{ "9",		0x003b, 0 },
	{ "9.5",	0x003c, 0 },
	{ "10",		0x003d, 0 },
	{ "11",		0x0040, 0 },
	{ "13",		0x0043, 0 }, /* (1/3)? */
	{ "13",		0x0044, 0 },
	{ "14",		0x0045, 0 },
	{ "16",		0x0048, 0 },
	{ "18",		0x004b, 0 },
	{ "19",		0x004c, 0 },
	{ "20",		0x004d, 0 },
	{ "22",		0x0050, 0 },
	{ "25",		0x0053, 0 },
	{ "27",		0x0054, 0 },
	{ "29",		0x0055, 0 },
	{ "32",		0x0058, 0 },
	{ "36",		0x005b, 0 },
	{ "38",		0x005c, 0 },
	{ "40",		0x005d, 0 },
	{ "45",		0x0060, 0 },
	{ "51",		0x0063, 0 },
	{ "54",		0x0064, 0 },
	{ "57",		0x0065, 0 },
	{ "64",		0x0068, 0 },
	{ "72",		0x006b, 0 },
	{ "76",		0x006c, 0 },
	{ "81",		0x006d, 0 },
	{ "91",		0x0070, 0 },
};
GENERIC16TABLE(Canon_Aperture,canon_aperture)

static struct deviceproptableu16 fuji_aperture[] = {
	{ "1.8",	10, 0 },
	{ "2",		12, 0 },
	{ "2.2",	14, 0 },
	{ "2.5",	16, 0 },
	{ "2.8",	18, 0 },
	{ "3.2",	20, 0 },
	{ "3.5",	22, 0 },
	{ "4",		24, 0 },
	{ "4.5",	26, 0 },
	{ "5",		28, 0 },
	{ "5.6",	30, 0 },
	{ "6.3",	32, 0 },
	{ "7.1",	34, 0 },
	{ "8",		36, 0 },
	{ "9",		38, 0 },
	{ "10",		40, 0 },
	{ "11",		42, 0 },
	{ "13",		44, 0 },
	{ "14",		46, 0 },
	{ "16",		48, 0 },
	{ "18",		50, 0 },
	{ "20",		52, 0 },
	{ "22",		54, 0 },
	{ "25",		56, 0 },
	{ "29",		58, 0 },
	{ "32",		60, 0 },
	{ "36",		62, 0 },
};
GENERIC16TABLE(Fuji_Aperture,fuji_aperture)



static struct deviceproptableu8 nikon_bracketset[] = {
	{ N_("AE & Flash"),	0, 0 },
	{ N_("AE only"),	1, 0 },
	{ N_("Flash only"),	2, 0 },
	{ N_("WB bracketing"),	3, 0 },
	{ N_("ADL bracketing"),	4, 0 },
};
GENERIC8TABLE(Nikon_BracketSet,nikon_bracketset)

static struct deviceproptableu8 nikon_cleansensor[] = {
	{ N_("Off"),			0, 0 },
	{ N_("Startup"),		1, 0 },
	{ N_("Shutdown"),		2, 0 },
	{ N_("Startup and Shutdown"),	3, 0 },
};
GENERIC8TABLE(Nikon_CleanSensor,nikon_cleansensor)

static struct deviceproptableu8 nikon_flickerreduction[] = {
	{ N_("50 Hz"),			0, 0 },
	{ N_("60 Hz"),			1, 0 },
	{ N_("Auto"),			2, 0 },
};
GENERIC8TABLE(Nikon_FlickerReduction,nikon_flickerreduction)

static struct deviceproptableu8 nikon_remotemode[] = {
	{ N_("Delayed Remote"),			0, 0 },
	{ N_("Quick Response"),			1, 0 },
	{ N_("Remote Mirror Up"),		2, 0 },
};
GENERIC8TABLE(Nikon_RemoteMode,nikon_remotemode)

static struct deviceproptableu8 nikon_applicationmode[] = {
	{ N_("Application Mode 0"),			0, 0 },
	{ N_("Application Mode 1"),			1, 0 },
};
GENERIC8TABLE(Nikon_ApplicationMode,nikon_applicationmode)

static struct deviceproptableu8 nikon_saturation[] = {
	{ N_("Normal"),		0, 0 },
	{ N_("Moderate"),	1, 0 },
	{ N_("Enhanced"),	2, 0 },
};
GENERIC8TABLE(Nikon_Saturation,nikon_saturation)


static struct deviceproptableu8 nikon_bracketorder[] = {
	{ N_("MTR > Under"),	0, 0 },
	{ N_("Under > MTR"),	1, 0 },
};
GENERIC8TABLE(Nikon_BracketOrder,nikon_bracketorder)

/* There is a table for it in the internet */
static struct deviceproptableu8 nikon_lensid[] = {
	{N_("Unknown"),	0, 0},
	{"Sigma 70-300mm 1:4-5.6 D APO Macro",		38, 0},
	{"AF Nikkor 80-200mm 1:2.8 D ED",		83, 0},
	{"AF Nikkor 50mm 1:1.8 D",			118, 0},
	{"AF-S Nikkor 18-70mm 1:3.5-4.5G ED DX",	127, 0},
	{"AF-S Nikkor 18-200mm 1:3.5-5.6 GED DX VR",	139, 0},
	{"AF-S Nikkor 24-70mm 1:2.8G ED DX",		147, 0},
	{"AF-S Nikkor 18-55mm 1:3.5-F5.6G DX VR",	154, 0},
	{"AF-S Nikkor 35mm 1:1.8G DX", 			159, 0},
	{"Sigma EX 30mm 1:1.4 DC HSM",			248, 0}, /* from mge */
};
GENERIC8TABLE(Nikon_LensID,nikon_lensid)

static struct deviceproptableu8 nikon_microphone[] = {
	{N_("Auto sensitivity"),	0, 0},
	{N_("High sensitivity"),	1, 0},
	{N_("Medium sensitivity"),	2, 0},
	{N_("Low sensitivity"),		3, 0},
	{N_("Microphone off"),		4, 0},
};
GENERIC8TABLE(Nikon_Microphone, nikon_microphone);

static struct deviceproptableu8 nikon_moviequality[] = {
	{"320x216",	0, 0},
	{"640x424",	1, 0},
	{"1280x720",	2, 0},
};
GENERIC8TABLE(Nikon_MovieQuality, nikon_moviequality);

static struct deviceproptableu8 nikon_d5100_moviequality[] = {
	{"640x424; 25fps; normal",		0, 0},
	{"640x424; 25fps; high quality",	1, 0},
 	{"1280x720; 24fps; normal",		2, 0},
	{"1280x720; 24fps; high quality",	3, 0},
	{"1280x720; 25fps; normal",		4, 0},
	{"1280x720; 25fps; high quality",	5, 0},
	{"1920x1080; 24fps; normal",		6, 0},
	{"1920x1080; 24fps; high quality",	7, 0},
	{"1920x1080; 25fps; normal",		8, 0},
	{"1920x1080; 25fps; high quality",	9, 0},
};
GENERIC8TABLE(Nikon_D5100_MovieQuality, nikon_d5100_moviequality);

static struct deviceproptableu8 nikon_d7100_moviequality[] = {
	{"1920x1080; 60i",	0, 0},
	{"1920x1080; 50i",	1, 0},
 	{"1920x1080; 30p",	2, 0},
	{"1920x1080; 25p",	3, 0},
	{"1920x1080; 24p",	4, 0},
	{"1280x720; 60p",	5, 0},
	{"1280x720; 50p",	6, 0},
};
GENERIC8TABLE(Nikon_D7100_MovieQuality, nikon_d7100_moviequality);

static struct deviceproptableu8 nikon_d7100_moviequality2[] = {
	{"Norm",	0, 0},
	{"High",	1, 0},
};
GENERIC8TABLE(Nikon_D7100_MovieQuality2, nikon_d7100_moviequality2);

static struct deviceproptableu8 nikon_1_moviequality[] = {
	{"1080/60i",	0, 0},
 	{"1080/30p",	1, 0},
	{"720/60p",	3, 0},
	{"720/30p",	4, 0},
};
GENERIC8TABLE(Nikon_1_MovieQuality, nikon_1_moviequality);

static struct deviceproptableu8 nikon_d90_isoautohilimit[] = {
	{"400",		0, 0},
	{"800",		1, 0},
	{"1600",	2, 0},
	{"3200",	3, 0},
	{N_("Hi 1"),	4, 0},
	{N_("Hi 2"),	5, 0},
};
GENERIC8TABLE(Nikon_D90_ISOAutoHiLimit, nikon_d90_isoautohilimit);

static struct deviceproptableu8 nikon_d7100_isoautohilimit[] = {
	{ "200",    0,  0 },
	{ "250",    1,  0 },
	{ "280",    2,  0 },
	{ "320",    3,  0 },
	{ "400",    4,  0 },
	{ "500",    5,  0 },
	{ "560",    6,  0 },
	{ "640",    7,  0 },
	{ "800",    8,  0 },
	{ "1000",   9,  0 },
	{ "1100",   10, 0 },
	{ "1250",   11, 0 },
	{ "1600",   12, 0 },
	{ "2000",   13, 0 },
	{ "2200",   14, 0 },
	{ "2500",   15, 0 },
	{ "3200",   16, 0 },
	{ "4000",   17, 0 },
	{ "4500",   18, 0 },
	{ "5000",   19, 0 },
	{ "6400",   20, 0 },
	{ "Hi 0.3", 21, 0 },
	{ "Hi 0.5", 22, 0 },
	{ "Hi 0.7", 23, 0 },
	{ "Hi 1",   24, 0 },
	{ "Hi 2",   25, 0 },
};
GENERIC8TABLE(Nikon_D7100_ISOAutoHiLimit, nikon_d7100_isoautohilimit);

static struct deviceproptableu8 nikon_manualbracketmode[] = {
	{N_("Flash/speed"),	0, 0},
	{N_("Flash/speed/aperture"),	1, 0},
	{N_("Flash/aperture"),	2, 0},
	{N_("Flash only"),	3, 0},
};
GENERIC8TABLE(Nikon_ManualBracketMode, nikon_manualbracketmode);

static struct deviceproptableu8 nikon_d3s_isoautohilimit[] = {
	{"400",	   0, 0},
	{"500",	   1, 0},
	{"640",	   3, 0},
	{"800",    4, 0},
	{"1000",   5, 0},
	{"1250",   7, 0},
	{"1600",   8, 0},
	{"2000",   9, 0},
	{"2500",  11, 0},
	{"3200",  12, 0},
	{"4000",  13, 0},
	{"5000",  15, 0},
	{"6400",  16, 0},
	{"8000",  17, 0},
	{"10000", 19, 0},
	{"12800", 20, 0},
	{"14400", 21, 0},
	{"20000", 23, 0},
	{"25600", 24, 0},
	{"51200", 25, 0},
	{"102400",26, 0},
};
GENERIC8TABLE(Nikon_D3s_ISOAutoHiLimit, nikon_d3s_isoautohilimit);

#if 0
static struct deviceproptableu8 nikon_d70s_padvpvalue[] = {
	{ "1/125",	0x00, 0 },
	{ "1/60",	0x01, 0 },
	{ "1/30",	0x02, 0 },
	{ "1/15",	0x03, 0 },
	{ "1/8",	0x04, 0 },
	{ "1/4",	0x05, 0 },
	{ "1/2",	0x06, 0 },
	{ "1",		0x07, 0 },
	{ "2",		0x08, 0 },
	{ "4",		0x09, 0 },
	{ "8",		0x0a, 0 },
	{ "15",		0x0b, 0 },
	{ "30",		0x0c, 0 },
};
GENERIC8TABLE(Nikon_D70s_PADVPValue,nikon_d70s_padvpvalue)
#endif

static struct deviceproptableu8 nikon_d90_padvpvalue[] = {
	{ "1/2000",	0x00, 0 },
	{ "1/1600",	0x01, 0 },
	{ "1/1250",	0x02, 0 },
	{ "1/1000",	0x03, 0 },
	{ "1/800",	0x04, 0 },
	{ "1/640",	0x05, 0 },
	{ "1/500",	0x06, 0 },
	{ "1/400",	0x07, 0 },
	{ "1/320",	0x08, 0 },
	{ "1/250",	0x09, 0 },
	{ "1/200",	0x0a, 0 },
	{ "1/160",	0x0b, 0 },
	{ "1/125",	0x0c, 0 },
	{ "1/100",	0x0d, 0 },
	{ "1/80",	0x0e, 0 },
	{ "1/60",	0x0f, 0 },
	{ "1/50",	0x10, 0 },
	{ "1/40",	0x11, 0 },
	{ "1/30",	0x12, 0 },
	{ "1/15",	0x13, 0 },
	{ "1/8",	0x14, 0 },
	{ "1/4",	0x15, 0 },
	{ "1/2",	0x16, 0 },
	{ "1",		0x17, 0 },
};
GENERIC8TABLE(Nikon_D90_PADVPValue,nikon_d90_padvpvalue)

static struct deviceproptableu8 nikon_d7100_padvpvalue[] = {
	{ "1/4000",	0x00, 0 },
	{ "1/3200",	0x01, 0 },
	{ "1/2500",	0x02, 0 },
	{ "1/2000",	0x03, 0 },
	{ "1/1600",	0x04, 0 },
	{ "1/1250",	0x05, 0 },
	{ "1/1000",	0x06, 0 },
	{ "1/800",	0x07, 0 },
	{ "1/640",	0x08, 0 },
	{ "1/500",	0x09, 0 },
	{ "1/400",	0x0a, 0 },
	{ "1/320",	0x0b, 0 },
	{ "1/250",	0x0c, 0 },
	{ "1/200",	0x0d, 0 },
	{ "1/160",	0x0e, 0 },
	{ "1/125",	0x0f, 0 },
	{ "1/100",	0x10, 0 },
	{ "1/80",	0x11, 0 },
	{ "1/60",	0x12, 0 },
	{ "1/50",	0x13, 0 },
	{ "1/40",	0x14, 0 },
	{ "1/30",	0x15, 0 },
	{ "1/15",	0x16, 0 },
	{ "1/8",	0x17, 0 },
        { "1/4",	0x18, 0 },
        { "1/2",	0x19, 0 },
        { "1",		0x1a, 0 },
        { "Auto",	0x1b, 0 },
};
GENERIC8TABLE(Nikon_D7100_PADVPValue,nikon_d7100_padvpvalue)

static struct deviceproptableu8 nikon_d3s_padvpvalue[] = {
	{ "1/4000",	0x00, 0 },
	{ "1/3200",	0x01, 0 },
	{ "1/2500",	0x02, 0 },
	{ "1/2000",	0x03, 0 },
	{ "1/1600",	0x04, 0 },
	{ "1/1250",	0x05, 0 },
	{ "1/1000",	0x06, 0 },
	{ "1/800",	0x07, 0 },
	{ "1/640",	0x08, 0 },
	{ "1/500",	0x09, 0 },
	{ "1/400",	0x0a, 0 },
	{ "1/320",	0x0b, 0 },
	{ "1/250",	0x0c, 0 },
	{ "1/200",	0x0d, 0 },
	{ "1/160",	0x0e, 0 },
	{ "1/125",	0x0f, 0 },
	{ "1/100",	0x10, 0 },
	{ "1/80",	0x11, 0 },
	{ "1/60",	0x12, 0 },
	{ "1/50",	0x13, 0 },
	{ "1/40",	0x14, 0 },
	{ "1/30",	0x15, 0 },
	{ "1/15",	0x16, 0 },
	{ "1/8",	0x17, 0 },
	{ "1/4",	0x18, 0 },
	{ "1/2",	0x19, 0 },
	{ "1",		0x1a, 0 },
};
GENERIC8TABLE(Nikon_D3s_PADVPValue,nikon_d3s_padvpvalue)

static struct deviceproptableu8 nikon_d90_activedlighting[] = {
	{ N_("Extra high"), 0x00,   0 },
	{ N_("High"),       0x01,   0 },
	{ N_("Normal"),     0x02,   0 },
	{ N_("Low"),        0x03,   0 },
	{ N_("Off"),        0x04,   0 },
	{ N_("Auto"),       0x05,   0 },
};
GENERIC8TABLE(Nikon_D90_ActiveDLighting,nikon_d90_activedlighting)

static struct deviceproptableu8 nikon_1_compression[] = {
	{ N_("JPEG Normal"),	0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),	0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Basic"),	0x02, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),	0x03, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),	0x04, PTP_VENDOR_NIKON }, /* for j5 */
	{ N_("NEF (Raw)"),	0x06, PTP_VENDOR_NIKON }, /* for j3 */
};
GENERIC8TABLE(Nikon_1_Compression,nikon_1_compression)

static struct deviceproptableu8 nikon_d90_compression[] = {
	{ N_("JPEG Basic"),	0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"),	0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),	0x02, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),	0x04, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),	0x05, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal"),	0x06, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),	0x07, PTP_VENDOR_NIKON },
};
GENERIC8TABLE(Nikon_D90_Compression,nikon_d90_compression)

static struct deviceproptableu8 nikon_d3s_compression[] = {
	{ N_("JPEG Basic"),	0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"),	0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),	0x02, PTP_VENDOR_NIKON },
	{ N_("TIFF (RGB)"),	0x03, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),	0x04, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),	0x05, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal"),	0x06, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),	0x07, PTP_VENDOR_NIKON },
};
GENERIC8TABLE(Nikon_D3s_Compression,nikon_d3s_compression)

static struct deviceproptableu8 nikon_d40_compression[] = {
	{ N_("JPEG Basic"),  0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"), 0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),   0x02, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),   0x03, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),   0x04, PTP_VENDOR_NIKON },
};
GENERIC8TABLE(Nikon_D40_Compression,nikon_d40_compression)

static struct deviceproptableu8 compressionsetting[] = {
	{ N_("JPEG Basic"),	0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"),	0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),	0x02, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),	0x04, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),	0x05, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal"),	0x06, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),	0x07, PTP_VENDOR_NIKON },

	{ N_("Standard"),	0x02, PTP_VENDOR_SONY },
	{ N_("Fine"),		0x03, PTP_VENDOR_SONY },
	{ N_("RAW"),		0x10, PTP_VENDOR_SONY },
	{ N_("RAW+JPEG"),	0x13, PTP_VENDOR_SONY },
};
GENERIC8TABLE(CompressionSetting,compressionsetting)

/* Sony specific, we need to wait for it settle (around 1 second), otherwise we get trouble later on */
static int
_put_Sony_CompressionSetting(CONFIG_PUT_ARGS) {
	PTPParams		*params = &(camera->pl->params);
	GPContext 		*context = ((PTPData *) params->data)->context;
	int 			ret;
	PTPDevicePropDesc	dpd2;
	time_t			start,end;

	ret = _put_CompressionSetting(CONFIG_PUT_NAMES);
	if (ret != GP_OK) return ret;
	start = time(NULL);
	C_PTP_REP (ptp_generic_setdevicepropvalue (params, PTP_DPC_CompressionSetting, propval, PTP_DTC_UINT8));
	while (1) {
		C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
		C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_CompressionSetting, &dpd2));
		if (dpd2.CurrentValue.u8 == propval->u8)
			break;
		end = time(NULL);
		if (end-start >= 2) {
			GP_LOG_E("failed to change variable to %d (current %d)\n", propval->u8, dpd2.CurrentValue.u8);
			break;
		}
	}
	return GP_OK;
}

static struct deviceproptableu8 nikon_d90_highisonr[] = {
	{ N_("Off"),	0, 0 },
	{ N_("Low"),	1, 0 },
	{ N_("Normal"),	2, 0 },
	{ N_("High"),	3, 0 },
};
GENERIC8TABLE(Nikon_D90_HighISONR,nikon_d90_highisonr)

static struct deviceproptableu8 nikon_1_highisonr[] = {
	{ N_("On"),	0, 0 },
	{ N_("Off"),	3, 0 },
};
GENERIC8TABLE(Nikon_1_HighISONR,nikon_1_highisonr)

static struct deviceproptableu8 nikon_d90_meterofftime[] = {
	{ N_("4 seconds"),	0x00, 0 },
	{ N_("6 seconds"),	0x01, 0 },
	{ N_("8 seconds"),	0x02, 0 },
	{ N_("16 seconds"),	0x03, 0 },
	{ N_("30 seconds"),	0x04, 0 },
	{ N_("1 minute"),	0x05, 0 },
	{ N_("5 minutes"),	0x06, 0 },
	{ N_("10 minutes"),	0x07, 0 },
	{ N_("30 minutes"),	0x08, 0 },
};
GENERIC8TABLE(Nikon_D90_MeterOffTime,nikon_d90_meterofftime)


static struct deviceproptableu8 nikon_rawcompression[] = {
	{ N_("Lossless"),	0x00, 0 },
	{ N_("Lossy"),		0x01, 0 },
};
GENERIC8TABLE(Nikon_RawCompression,nikon_rawcompression)

static struct deviceproptableu8 nikon_d3s_jpegcompressionpolicy[] = {
	{ N_("Size Priority"),	0x00, 0 },
	{ N_("Optimal quality"),0x01, 0 },
};
GENERIC8TABLE(Nikon_D3s_JPEGCompressionPolicy,nikon_d3s_jpegcompressionpolicy)

static struct deviceproptableu8 nikon_d3s_flashsyncspeed[] = {
	{ N_("1/250s (Auto FP)"),	0x00, 0 },
	{ N_("1/250s"),			0x01, 0 },
	{ N_("1/200s"),			0x02, 0 },
	{ N_("1/160s"),			0x03, 0 },
	{ N_("1/125s"),			0x04, 0 },
	{ N_("1/100s"),			0x05, 0 },
	{ N_("1/80s"),			0x06, 0 },
	{ N_("1/60s"),			0x07, 0 },
};
GENERIC8TABLE(Nikon_D3s_FlashSyncSpeed,nikon_d3s_flashsyncspeed)

static struct deviceproptableu8 nikon_d7100_flashsyncspeed[] = {
	{ N_("1/320s (Auto FP)"),	0x00, 0 },
	{ N_("1/250s (Auto FP)"),	0x01, 0 },
	{ N_("1/250s"),			0x02, 0 },
	{ N_("1/200s"),			0x03, 0 },
	{ N_("1/160s"),			0x04, 0 },
	{ N_("1/125s"),			0x05, 0 },
	{ N_("1/100s"),			0x06, 0 },
	{ N_("1/80s"),			0x07, 0 },
	{ N_("1/60s"),			0x08, 0 },
};
GENERIC8TABLE(Nikon_D7100_FlashSyncSpeed,nikon_d7100_flashsyncspeed)

static struct deviceproptableu8 nikon_d3s_afcmodepriority[] = {
	{ N_("Release"),	0x00, 0 },
	{ N_("Release + Focus"),0x01, 0 },
	{ N_("Focus"),		0x02, 0 },
};
GENERIC8TABLE(Nikon_D3s_AFCModePriority,nikon_d3s_afcmodepriority)

static struct deviceproptableu8 nikon_d3s_afsmodepriority[] = {
	{ N_("Release"),	0x00, 0 },
	{ N_("Focus"),		0x01, 0 },
};
GENERIC8TABLE(Nikon_D3s_AFSModePriority,nikon_d3s_afsmodepriority)

static struct deviceproptableu8 nikon_d3s_dynamicafarea[] = {
	{ N_("9 points"),	0x00, 0 },
	{ N_("21 points"),	0x01, 0 },
	{ N_("51 points"),	0x02, 0 },
	{ N_("51 points (3D)"),	0x03, 0 },
};
GENERIC8TABLE(Nikon_D3s_DynamicAFArea,nikon_d3s_dynamicafarea)

static struct deviceproptableu8 nikon_d3s_aflockon[] = {
	{ N_("5 (Long)"),	0x00, 0 },
	{ N_("4"),		0x01, 0 },
	{ N_("3 (Normal)"),	0x02, 0 },
	{ N_("2"),		0x03, 0 },
	{ N_("1 (Short)"),	0x04, 0 },
	{ N_("Off"),		0x05, 0 },
};
GENERIC8TABLE(Nikon_D3s_AFLockOn,nikon_d3s_aflockon)

static struct deviceproptableu8 nikon_d3s_afactivation[] = {
	{ N_("Shutter/AF-ON"),	0x00, 0 },
	{ N_("AF-ON"),		0x01, 0 },
};
GENERIC8TABLE(Nikon_D3s_AFActivation,nikon_d3s_afactivation)

static struct deviceproptableu8 nikon_d3s_afareapoint[] = {
	{ N_("AF51"),	0x00, 0 },
	{ N_("AF11"),	0x01, 0 },
};
GENERIC8TABLE(Nikon_D3s_AFAreaPoint,nikon_d3s_afareapoint)

static struct deviceproptableu8 nikon_d3s_normalafon[] = {
	{ N_("AF-ON"),		0x00, 0 },
	{ N_("AE/AF lock"),	0x01, 0 },
	{ N_("AE lock only"),	0x02, 0 },
	{ N_("AE lock (Reset on release)"),	0x03, 0 },
	{ N_("AE lock (Hold)"),	0x04, 0 },
	{ N_("AF lock only"),	0x05, 0 },
};
GENERIC8TABLE(Nikon_D3s_NormalAFOn,nikon_d3s_normalafon)

static struct deviceproptableu8 nikon_d3s_flashshutterspeed[] = {
	{ N_("1/60s"),  0x00,   0 },
	{ N_("1/30s"),  0x01,   0 },
	{ N_("1/15s"),  0x02,   0 },
	{ N_("1/8s"),   0x03,   0 },
	{ N_("1/4s"),   0x04,   0 },
	{ N_("1/2s"),   0x05,   0 },
	{ N_("1s"),     0x06,   0 },
	{ N_("2s"),     0x07,   0 },
	{ N_("4s"),     0x08,   0 },
	{ N_("8s"),     0x09,   0 },
	{ N_("15s"),    0x0a,   0 },
	{ N_("30s"),    0x0b,   0 },
};
GENERIC8TABLE(Nikon_D3s_FlashShutterSpeed,nikon_d3s_flashshutterspeed)

static struct deviceproptableu8 nikon_d90_shootingspeed[] = {
	{ N_("4 fps"),	0x00, 0 },
	{ N_("3 fps"),	0x01, 0 },
	{ N_("2 fps"),	0x02, 0 },
	{ N_("1 fps"),	0x03, 0 },
};
GENERIC8TABLE(Nikon_D90_ShootingSpeed,nikon_d90_shootingspeed)

static struct deviceproptableu8 nikon_d7100_shootingspeed[] = {
	{ N_("6 fps"),	0x00, 0 },
	{ N_("5 fps"),	0x01, 0 },
	{ N_("4 fps"),	0x02, 0 },
	{ N_("3 fps"),	0x03, 0 },
	{ N_("2 fps"),	0x04, 0 },
	{ N_("1 fps"),	0x05, 0 },
};
GENERIC8TABLE(Nikon_D7100_ShootingSpeed,nikon_d7100_shootingspeed)

static struct deviceproptableu8 nikon_d3s_shootingspeed[] = {
	{ N_("9 fps"),	0x00, 0 },
	{ N_("8 fps"),	0x01, 0 },
	{ N_("7 fps"),	0x02, 0 },
	{ N_("6 fps"),	0x03, 0 },
	{ N_("5 fps"),	0x04, 0 },
	{ N_("4 fps"),	0x05, 0 },
	{ N_("3 fps"),	0x06, 0 },
	{ N_("2 fps"),	0x07, 0 },
	{ N_("1 fps"),	0x08, 0 },
};
GENERIC8TABLE(Nikon_D3s_ShootingSpeed,nikon_d3s_shootingspeed)

static struct deviceproptableu8 nikon_d3s_shootingspeedhigh[] = {
	{ N_("11 fps"),	0x00, 0 },
	{ N_("10 fps"),	0x01, 0 },
	{ N_("9 fps"),	0x02, 0 },
};
GENERIC8TABLE(Nikon_D3s_ShootingSpeedHigh,nikon_d3s_shootingspeedhigh)

static struct deviceproptableu8 nikon_d7000_funcbutton[] = {
	{ N_("Unassigned"),			0x00, 0 },/* unselectable */
	{ N_("Preview"),			0x01, 0 },
	{ N_("FV lock"),			0x02, 0 },
	{ N_("AE/AF lock"),			0x03, 0 },
	{ N_("AE lock only"),			0x04, 0 },
	{ N_("Invalid"),			0x05, 0 },/* unselectable */
	{ N_("AE lock (hold)"),			0x06, 0 },
	{ N_("AF lock only"),			0x07, 0 },
	{ N_("Flash off"),			0x08, 0 },
	{ N_("Bracketing burst"),		0x09, 0 },
	{ N_("Matrix metering"),		0x0a, 0 },
	{ N_("Center-weighted metering"),	0x0b, 0 },
	{ N_("Spot metering"),			0x0c, 0 },
	{ N_("Playback"),			0x0d, 0 },
	{ N_("Access top item in MY MENU"),	0x0e, 0 },
	{ N_("+NEF (RAW)"),			0x0f, 0 },
	{ N_("Framing grid"),			0x10, 0 },
	{ N_("Active D-Lighting"),		0x11, 0 },
	{ N_("1 step spd/aperture"),		0x12, 0 },
	{ N_("Choose non-CPU lens number"),	0x13, 0 },
	{ N_("Viewfinder virtual horizont"),	0x14, 0 },
	{ N_("Start movie recording"),		0x15, 0 },
};
GENERIC8TABLE(Nikon_D7000_FuncButton,nikon_d7000_funcbutton)

static struct deviceproptableu8 nikon_menus_and_playback[] = {
	{ N_("Off"),				0x0, 0 },
	{ N_("On"),				0x1, 0 },
	{ N_("On (image review excluded)"),	0x2, 0 },
};
GENERIC8TABLE(Nikon_MenusAndPlayback,nikon_menus_and_playback)


static int
_get_BurstNumber(CONFIG_GET_ARGS) {
	float value_float , start=0.0, end=0.0, step=0.0;

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	start = dpd->FORM.Range.MinimumValue.u16;
	end = dpd->FORM.Range.MaximumValue.u16;
	step = dpd->FORM.Range.StepSize.u16;
	gp_widget_set_range (*widget, start, end, step);
	value_float = dpd->CurrentValue.u16;
	gp_widget_set_value (*widget, &value_float);
	return (GP_OK);
}

static int
_put_BurstNumber(CONFIG_PUT_ARGS) {
	float val;

	CR (gp_widget_get_value(widget, &val));
	propval->u16 = val;
	return GP_OK;
}

static int
_get_BatteryLevel(CONFIG_GET_ARGS) {
	unsigned char value_float , start, end;
	char	buffer[20];

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	start = dpd->FORM.Range.MinimumValue.u8;
	end = dpd->FORM.Range.MaximumValue.u8;
	value_float = dpd->CurrentValue.u8;
	sprintf (buffer, "%d%%", (int)((value_float-start+1)*100/(end-start+1)));
	gp_widget_set_value(*widget, buffer);
	return (GP_OK);
}

static int
_get_Canon_EOS_BatteryLevel(CONFIG_GET_ARGS) {
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	switch (dpd->CurrentValue.u16) {
	case 0: gp_widget_set_value(*widget, _("Low")); break;
	case 1: gp_widget_set_value(*widget, _("50%")); break;
	case 2: gp_widget_set_value(*widget, _("100%")); break;
	case 4: gp_widget_set_value(*widget, _("75%")); break;
	case 5: gp_widget_set_value(*widget, _("25%")); break;
	default: gp_widget_set_value(*widget, _("Unknown value")); break;
	}
	return (GP_OK);
}

static int
_get_UINT32_as_time(CONFIG_GET_ARGS) {
	time_t	camtime;

	gp_widget_new (GP_WIDGET_DATE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	camtime = dpd->CurrentValue.u32;
	gp_widget_set_value (*widget,&camtime);
	return (GP_OK);
}

static int
_put_UINT32_as_time(CONFIG_PUT_ARGS) {
	time_t	camtime;

	camtime = 0;
	CR (gp_widget_get_value (widget,&camtime));
	propval->u32 = camtime;
	return (GP_OK);
}

static int
_get_UINT32_as_localtime(CONFIG_GET_ARGS) {
	time_t	camtime;
	struct	tm *ptm;

	gp_widget_new (GP_WIDGET_DATE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	camtime = dpd->CurrentValue.u32;
	/* hack to convert from local time on camera to utc */
	ptm = gmtime(&camtime);
	ptm->tm_isdst = -1;
	camtime = mktime (ptm);
	gp_widget_set_value (*widget,&camtime);
	return (GP_OK);
}

static int
_put_UINT32_as_localtime(CONFIG_PUT_ARGS) {
	time_t	camtime,newcamtime;
	struct	tm *ptm;
#if HAVE_SETENV
	char	*tz;
#endif

	camtime = 0;
	CR (gp_widget_get_value (widget, &camtime));
	ptm = localtime(&camtime);

#if HAVE_SETENV
	tz = getenv("TZ");
	if (tz)
		C_MEM (tz = strdup(tz));
	setenv("TZ", "", 1);
	tzset();
#endif
	newcamtime = mktime(ptm);
#if HAVE_SETENV
	if (tz) {
		setenv("TZ", tz, 1);
		free(tz);
	} else
		unsetenv("TZ");
	tzset();
#endif

	propval->u32 = newcamtime;
	return (GP_OK);
}

static int
_get_Canon_SyncTime(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	return (GP_OK);
}

static int
_put_Canon_SyncTime(CONFIG_PUT_ARGS) {
	/* Just set the time if the entry changes. */
	propval->u32 = time(NULL);
	return (GP_OK);
}

static int
_get_Nikon_AFDrive(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	return (GP_OK);
}

static int
_put_Nikon_AFDrive(CONFIG_PUT_ARGS) {
	PTPParams	*params = &(camera->pl->params);
	GPContext 	*context = ((PTPData *) params->data)->context;

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_AfDrive)) 
		return (GP_ERROR_NOT_SUPPORTED);

	C_PTP (ptp_nikon_afdrive (&camera->pl->params));
	/* wait at most 5 seconds for focusing currently */
	C_PTP_REP (nikon_wait_busy (params, 10, 5000));
	/* this can return PTP_RC_OK or PTP_RC_NIKON_OutOfFocus */
	return GP_OK;
}

static int
_get_Nikon_ChangeAfArea(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	gp_widget_set_value (*widget,"0x0");
	return (GP_OK);
}

static int
_put_Nikon_ChangeAfArea(CONFIG_PUT_ARGS) {
	uint16_t	ret;
	char		*val;
	int		x,y;
	PTPParams	*params = &(camera->pl->params);
	GPContext 	*context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));

	C_PARAMS (2 == sscanf(val, "%dx%d", &x, &y));

	ret = ptp_nikon_changeafarea (&camera->pl->params, x, y);
	if (ret == PTP_RC_NIKON_NotLiveView) {
		gp_context_error (context, _("Nikon changeafarea works only in LiveView mode."));
		return GP_ERROR;
	}

	C_PTP_MSG (ret, "Nikon changeafarea failed");
#if 0
	int		tries = 0;
	/* wait at most 5 seconds for focusing currently */
	while (PTP_RC_DeviceBusy == (ret = ptp_nikon_device_ready(&camera->pl->params))) {
		tries++;
		if (tries == 500)
			return GP_ERROR_CAMERA_BUSY;
		usleep(10*1000);
	}
	/* this can return PTP_RC_OK or PTP_RC_NIKON_OutOfFocus */
	if (ret == PTP_RC_NIKON_OutOfFocus)
		gp_context_error (context, _("Nikon autofocus drive did not focus."));
#endif
	return translate_ptp_result (ret);
}

static int
_get_Canon_EOS_AFDrive(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	return (GP_OK);
}

static int
_put_Canon_EOS_AFDrive(CONFIG_PUT_ARGS) {
	int		val;
	PTPParams	*params = &(camera->pl->params);

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_DoAf)) 
		return (GP_ERROR_NOT_SUPPORTED);

	CR (gp_widget_get_value(widget, &val));
	if (val)
		C_PTP (ptp_canon_eos_afdrive (params));
	else
		C_PTP (ptp_canon_eos_afcancel (params));
	/* Get the next set of event data */
	C_PTP (ptp_check_eos_events (params));
	return GP_OK;
}

static int
_get_Canon_EOS_AFCancel(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	return (GP_OK);
}

static int
_put_Canon_EOS_AFCancel(CONFIG_PUT_ARGS) {
	PTPParams *params = &(camera->pl->params);

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_AfCancel)) 
		return (GP_ERROR_NOT_SUPPORTED);

	C_PTP (ptp_canon_eos_afcancel (params));
	/* Get the next set of event data */
	C_PTP (ptp_check_eos_events (params));
	return GP_OK;
}

static int
_get_Nikon_MFDrive(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	gp_widget_set_range(*widget, -32767.0, 32767.0, 1.0);
	return (GP_OK);
}

static int
_put_Nikon_MFDrive(CONFIG_PUT_ARGS) {
	uint16_t	ret;
	float		val;
	unsigned int	xval, flag;
	PTPParams	*params = &(camera->pl->params);
	GPContext 	*context = ((PTPData *) params->data)->context;

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_MfDrive)) 
		return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_get_value(widget, &val);

	if (val<0) {
		xval = -val;
		flag = 0x1;
	} else {
		xval = val;
		flag = 0x2;
	}
	if (!xval) xval = 1;
	ret = LOG_ON_PTP_E (ptp_nikon_mfdrive (&camera->pl->params, flag, xval));
	if (ret == PTP_RC_NIKON_NotLiveView) {
		gp_context_error (context, _("Nikon manual focus works only in LiveView mode."));
		return GP_ERROR;
	}
	if (ret != PTP_RC_OK)
		return translate_ptp_result(ret);

	/* The mf drive operation has started ... wait for it to
	 * finish. */
	ret = LOG_ON_PTP_E (nikon_wait_busy (&camera->pl->params, 20, 1000));
	if (ret == PTP_RC_NIKON_MfDriveStepEnd) {
		gp_context_error (context, _("Nikon manual focus at limit."));
		return GP_ERROR_CAMERA_ERROR;
	}
	if (ret == PTP_RC_NIKON_MfDriveStepInsufficiency) {
		gp_context_error (context, _("Nikon manual focus stepping too small."));
		return GP_ERROR_CAMERA_ERROR;
	}
	return translate_ptp_result(ret);
}

static int
_get_Nikon_ControlMode(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	gp_widget_set_value(*widget, "0");
	return (GP_OK);
}

static int
_put_Nikon_ControlMode(CONFIG_PUT_ARGS) {
	PTPParams *params = &(camera->pl->params);
	char*		val;
	unsigned int	xval = 0;

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_SetControlMode)) 
		return GP_ERROR_NOT_SUPPORTED;
	gp_widget_get_value(widget, &val);

	if (!sscanf(val,"%d",&xval))	
		return GP_ERROR;

	C_PTP (ptp_nikon_setcontrolmode (&camera->pl->params, xval));
	return GP_OK;
}

static int
_get_Canon_EOS_RemoteRelease(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	/* FIXME: remember state of release */
	gp_widget_add_choice (*widget, _("None"));
	gp_widget_add_choice (*widget, _("Press Half"));
	gp_widget_add_choice (*widget, _("Press Full"));
	gp_widget_add_choice (*widget, _("Release Half"));
	gp_widget_add_choice (*widget, _("Release Full"));
	gp_widget_add_choice (*widget, _("Immediate"));
	/* debugging */
	gp_widget_add_choice (*widget, _("Press 1"));
	gp_widget_add_choice (*widget, _("Press 2"));
	gp_widget_add_choice (*widget, _("Press 3"));
	gp_widget_add_choice (*widget, _("Release 1"));
	gp_widget_add_choice (*widget, _("Release 2"));
	gp_widget_add_choice (*widget, _("Release 3"));
	gp_widget_set_value (*widget, _("None"));
	return (GP_OK);
}

/* On EOS 7D:
 * 9128 1 0  (half?)
 * 9128 2 0  (full?)
 * paramters: press mode, ? afmode ? SDK seems to suggest 1==NonAF, 0 == AF
 */

static int
_put_Canon_EOS_RemoteRelease(CONFIG_PUT_ARGS) {
	const char*	val;
	PTPParams	*params = &(camera->pl->params);
	GPContext 	*context = ((PTPData *) params->data)->context;

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)) 
		return (GP_ERROR_NOT_SUPPORTED);

	/* If someone has set the capture target inbetween */
	CR (camera_canon_eos_update_capture_target( camera, context, -1 ));

	gp_widget_get_value(widget, &val);

	if (!strcmp (val, _("None"))) {
		return GP_OK;
	} else if (!strcmp (val, _("Press Half"))) {
		C_PTP (ptp_canon_eos_remotereleaseon (params, 1, 1));
	} else if (!strcmp (val, _("Press Full"))) {
		C_PTP (ptp_canon_eos_remotereleaseon (params, 3, 1));
	} else if (!strcmp (val, _("Immediate"))) {
		/* HACK by Flori Radlherr: "fire and forget" half release before release:
		   Avoids autofocus drive while focus-switch on the lens is in AF state */
		C_PTP (ptp_canon_eos_remotereleaseon (params, 1, 1));
		C_PTP (ptp_canon_eos_remotereleaseon (params, 3, 1));
	/* try out others with 0 */
	} else if (!strcmp (val, _("Press 1"))) {
		C_PTP (ptp_canon_eos_remotereleaseon (params, 1, 0));
	} else if (!strcmp (val, _("Press 2"))) {
		C_PTP (ptp_canon_eos_remotereleaseon (params, 2, 0));
	} else if (!strcmp (val, _("Press 3"))) {
		C_PTP (ptp_canon_eos_remotereleaseon (params, 3, 0));
	} else if (!strcmp (val, _("Release 1"))) {
		C_PTP (ptp_canon_eos_remotereleaseoff (params, 1));
	} else if (!strcmp (val, _("Release 2"))) {
		C_PTP (ptp_canon_eos_remotereleaseoff (params, 2));
	} else if (!strcmp (val, _("Release 3"))) {
		C_PTP (ptp_canon_eos_remotereleaseoff (params, 3));
	} else if (!strcmp (val, _("Release Half"))) {
		C_PTP (ptp_canon_eos_remotereleaseoff (params, 1));
	} else if (!strcmp (val, _("Release Full"))) {
		C_PTP (ptp_canon_eos_remotereleaseoff (params, 3));
	} else {
		GP_LOG_D ("Unknown value %s", val);
		return GP_ERROR_NOT_SUPPORTED;
	}

	/* Get the next set of event data */
	C_PTP (ptp_check_eos_events (params));
	return GP_OK;
}

static int
_get_Canon_EOS_ContinousAF(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	gp_widget_add_choice (*widget, _("Off"));
	gp_widget_add_choice (*widget, _("On"));
	switch (dpd->CurrentValue.u32) {
	case 0: gp_widget_set_value (*widget, _("Off")); break;
	case 1: gp_widget_set_value (*widget, _("On")); break;
	default: {
		char buf[200];
		sprintf(buf,"Unknown value 0x%08x", dpd->CurrentValue.u32);
		gp_widget_set_value (*widget, buf);
		break;
	}
	}
	return GP_OK;
}

static int
_put_Canon_EOS_ContinousAF(CONFIG_PUT_ARGS) {
	char *val;
	unsigned int ival;

	CR(gp_widget_get_value (widget, &val));
	if (!strcmp(val,_("Off"))) { propval->u32 = 0; return GP_OK; }
	if (!strcmp(val,_("On"))) { propval->u32 = 1; return GP_OK; }
	if (!sscanf(val,"Unknown value 0x%08x",&ival))
		return GP_ERROR_BAD_PARAMETERS;
	propval->u32 = ival;
	return GP_OK;
}

static int
_get_Canon_EOS_MFDrive(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	gp_widget_add_choice (*widget, _("Near 1"));
	gp_widget_add_choice (*widget, _("Near 2"));
	gp_widget_add_choice (*widget, _("Near 3"));
	gp_widget_add_choice (*widget, _("None"));
	gp_widget_add_choice (*widget, _("Far 1"));
	gp_widget_add_choice (*widget, _("Far 2"));
	gp_widget_add_choice (*widget, _("Far 3"));

	gp_widget_set_value (*widget, _("None"));
	return (GP_OK);
}

static int
_put_Canon_EOS_MFDrive(CONFIG_PUT_ARGS) {
	const char*	val;
	unsigned int	xval;
	PTPParams *params = &(camera->pl->params);

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_DriveLens)) 
		return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_get_value(widget, &val);

	if (!strcmp (val, _("None"))) return GP_OK;

	if (!sscanf (val, _("Near %d"), &xval)) {
		if (!sscanf (val, _("Far %d"), &xval)) {
			GP_LOG_D ("Could not parse %s", val);
			return GP_ERROR;
		} else {
			xval |= 0x8000;
		}
	}
	C_PTP_MSG (ptp_canon_eos_drivelens (params, xval),
		   "Canon manual focus drive 0x%x failed", xval);
	/* Get the next set of event data */
	C_PTP (ptp_check_eos_events (params));
	return GP_OK;
}


static int
_get_Canon_EOS_Zoom(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	gp_widget_set_value (*widget, "0");
	return (GP_OK);
}

/* Only 1 and 5 seem to work on the EOS 1000D */
static int
_put_Canon_EOS_Zoom(CONFIG_PUT_ARGS) {
	const char*	val;
	unsigned int	xval;
	PTPParams *params = &(camera->pl->params);

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_Zoom)) 
		return (GP_ERROR_NOT_SUPPORTED);

	gp_widget_get_value(widget, &val);
	if (!sscanf (val, "%d", &xval)) {
		GP_LOG_D ("Could not parse %s", val);
		return GP_ERROR;
	}
	C_PTP_MSG (ptp_canon_eos_zoom (params, xval),
		   "Canon zoom 0x%x failed", xval);

	/* Get the next set of event data */
	C_PTP (ptp_check_eos_events (params));
	return GP_OK;
}

/* EOS Zoom. Works in approx 64 pixel steps on the EOS 1000D, but just accept
 * all kind of pairs */
static int
_get_Canon_EOS_ZoomPosition(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	gp_widget_set_value (*widget, "0,0");
	return (GP_OK);
}

static int
_put_Canon_EOS_ZoomPosition(CONFIG_PUT_ARGS) {
	const char*	val;
	unsigned int	x,y;
	PTPParams *params = &(camera->pl->params);

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_ZoomPosition)) 
		return (GP_ERROR_NOT_SUPPORTED);

	gp_widget_get_value(widget, &val);
	if (2!=sscanf (val, "%d,%d", &x,&y)) {
		GP_LOG_D ("Could not parse %s (expected 'x,y')", val);
		return GP_ERROR;
	}
	C_PTP_MSG (ptp_canon_eos_zoomposition (params, x,y),
		   "Canon zoom position %d,%d failed", x, y);
	/* Get the next set of event data */
	C_PTP (ptp_check_eos_events (params));
	return GP_OK;
}

static int
_get_Canon_CHDK_Script(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	gp_widget_add_choice  (*widget, "cls();exit_alt();");
	gp_widget_add_choice  (*widget, "shoot();cls();exit_alt();");
	gp_widget_set_value  (*widget, "cls();exit_alt();");
	return (GP_OK);
}

static int
_put_Canon_CHDK_Script(CONFIG_PUT_ARGS) {
	char		*script;
	PTPParams	*params = &(camera->pl->params);
	int		script_id;
	unsigned int	status;
	int		luastatus;
	
	CR (gp_widget_get_value(widget, &script));

//  Nafraf: Working on this!!!
//
//  gphoto: config.c   
//  ret = ptp_chdk_exec_lua (params, val, &output);
//
//  chdkptp: chdkptp.c
//    ret = ptp_chdk_exec_lua (params, 
//                (char *)luaL_optstring(L,2,""),
//                luaL_optnumber(L,3,0), 
//                &ptp_cs->script_id,
//                &status)
//
// Unfinished, I'm not sure of last 3 parameters
	GP_LOG_D ("calling script: %s", script);
	C_PTP (ptp_chdk_exec_lua (params, script, 0, &script_id, &luastatus));
	GP_LOG_D ("called script, id %d, status %d", script_id, luastatus);

	while (1) {
		C_PTP (ptp_chdk_get_script_status(params, &status));
		GP_LOG_D ("script status %x", status);

		if (status & PTP_CHDK_SCRIPT_STATUS_MSG) {
			ptp_chdk_script_msg	*msg = NULL;

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
_get_STR_as_time(CONFIG_GET_ARGS) {
	time_t		camtime;
	struct tm	tm;
	char		capture_date[64],tmp[5];

	/* strptime() is not widely accepted enough to use yet */
	memset(&tm,0,sizeof(tm));
	if (!dpd->CurrentValue.str)
		return (GP_ERROR);
	gp_widget_new (GP_WIDGET_DATE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	strncpy(capture_date,dpd->CurrentValue.str,sizeof(capture_date));
	strncpy (tmp, capture_date, 4);
	tmp[4] = 0;
	tm.tm_year=atoi (tmp) - 1900;
	strncpy (tmp, capture_date + 4, 2);
	tmp[2] = 0;
	tm.tm_mon = atoi (tmp) - 1;
	strncpy (tmp, capture_date + 6, 2);
	tmp[2] = 0;
	tm.tm_mday = atoi (tmp);
	strncpy (tmp, capture_date + 9, 2);
	tmp[2] = 0;
	tm.tm_hour = atoi (tmp);
	strncpy (tmp, capture_date + 11, 2);
	tmp[2] = 0;
	tm.tm_min = atoi (tmp);
	strncpy (tmp, capture_date + 13, 2);
	tmp[2] = 0;
	tm.tm_sec = atoi (tmp);
	tm.tm_isdst = -1; /* autodetect */
	camtime = mktime(&tm);
	gp_widget_set_value (*widget,&camtime);
	return (GP_OK);
}

static int
_put_STR_as_time(CONFIG_PUT_ARGS) {
	time_t		camtime;
#ifdef HAVE_GMTIME_R
	struct tm	xtm;
#endif
	struct tm	*pxtm;
	char		asctime[64];

	camtime = 0;
	CR (gp_widget_get_value (widget,&camtime));
#ifdef HAVE_GMTIME_R
	memset(&xtm,0,sizeof(xtm));
	pxtm = localtime_r (&camtime, &xtm);
#else
	pxtm = localtime (&camtime);
#endif
	/* 20020101T123400.0 is returned by the HP Photosmart */
	sprintf(asctime,"%04d%02d%02dT%02d%02d%02d",pxtm->tm_year+1900,pxtm->tm_mon+1,pxtm->tm_mday,pxtm->tm_hour,pxtm->tm_min,pxtm->tm_sec);

	/* if the camera reported fractional seconds, also add it */
	if (strchr(dpd->CurrentValue.str,'.'))
		strcat(asctime,".0");

	C_MEM (propval->str = strdup(asctime));
	return (GP_OK);
}

static int
_put_None(CONFIG_PUT_ARGS) {
	return (GP_ERROR_NOT_SUPPORTED);
}

static int
_get_Canon_CaptureMode(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	/* we use presence of FlashMode as indication of capture enablement or not */
	val = have_prop (camera, PTP_VENDOR_CANON, PTP_DPC_CANON_FlashMode);
	return gp_widget_set_value  (*widget, &val);
}

static int
_put_Canon_CaptureMode(CONFIG_PUT_ARGS) {
	int val;

	CR (gp_widget_get_value(widget, &val));
	if (val)
		return camera_prepare_capture (camera, NULL);
	else
		return camera_unprepare_capture (camera, NULL);
}

static int
_get_Canon_EOS_ViewFinder(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	val = 2;	/* always changed, unless we can find out the state ... */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Canon_EOS_ViewFinder(CONFIG_PUT_ARGS) {
	int			val;
	uint16_t		res;
	PTPParams		*params = &(camera->pl->params);
	PTPPropertyValue	xval;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_InitiateViewfinder)) {
			res = ptp_canon_eos_start_viewfinder (params);
			params->inliveview = 1;
			return translate_ptp_result (res);
		}
	} else {
		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_TerminateViewfinder)) {
			res = ptp_canon_eos_end_viewfinder (params);
			params->inliveview = 0;
			return translate_ptp_result (res);
		}
	}
	if (val)
		xval.u32 = 2;
	else
		xval.u32 = 0;
	C_PTP_MSG (ptp_canon_eos_setdevicepropvalue (params, PTP_DPC_CANON_EOS_EVFOutputDevice, &xval, PTP_DTC_UINT32),
		   "ptp2_eos_viewfinder enable", "setval of evf outputmode to %d failed", xval.u32);
	return GP_OK;
}

static int
_get_Nikon_ViewFinder(CONFIG_GET_ARGS) {
	int			val;
	PTPPropertyValue	value;
	PTPParams		*params = &(camera->pl->params);

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (LOG_ON_PTP_E (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &value, PTP_DTC_UINT8)) != PTP_RC_OK )
		value.u8 = 0;
	val = value.u8 ? 1 : 0;
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Nikon_ViewFinder(CONFIG_PUT_ARGS) {
	int			val;
	PTPParams		*params = &(camera->pl->params);
	GPContext 		*context = ((PTPData *) params->data)->context;

	if (!ptp_operation_issupported(params, PTP_OC_NIKON_StartLiveView))
		return GP_ERROR_NOT_SUPPORTED;

	CR (gp_widget_get_value (widget, &val));
	if (val) {
		PTPPropertyValue	value;

		if (LOG_ON_PTP_E (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &value, PTP_DTC_UINT8)) != PTP_RC_OK)
			value.u8 = 0;

                if (!value.u8) {
			value.u8 = 1;
			LOG_ON_PTP_E (ptp_setdevicepropvalue (params, PTP_DPC_NIKON_RecordingMedia, &value, PTP_DTC_UINT8));
			C_PTP_REP_MSG (ptp_nikon_start_liveview (params),
				       _("Nikon enable liveview failed"));
			/* Has to put the mirror up, so takes a bit. */
			C_PTP (nikon_wait_busy(params, 50, 1000));
		}
	} else {
		if (ptp_operation_issupported(params, PTP_OC_NIKON_EndLiveView))
			C_PTP (ptp_nikon_end_liveview (params));
	}
	return GP_OK;
}

static int
_get_Canon_FocusLock(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Canon_FocusLock(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;

	CR (gp_widget_get_value(widget, &val));
	if (val)
		C_PTP (ptp_canon_focuslock (params));
	else
		C_PTP (ptp_canon_focusunlock (params));
	return GP_OK;
}

static int
_get_PowerDown(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_PowerDown(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;

	CR (gp_widget_get_value(widget, &val));
	if (val)
		C_PTP (ptp_powerdown (params));
	return GP_OK;
}

static int
_get_Generic_OPCode(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	gp_widget_set_value  (*widget, "0x1001,0xparam1,0xparam2");
	return GP_OK;
}

static int
_put_Generic_OPCode(CONFIG_PUT_ARGS)
{
	PTPParams		*params = &(camera->pl->params);
	char			*val, *x;
	int			opcode;
	int			nparams;
	uint32_t		xparams[5];
	uint16_t		ret;
	PTPContainer		ptp;
	unsigned char		*data = NULL;
	unsigned int		size = 0;

	CR (gp_widget_get_value(widget, &val));

	if (!sscanf(val,"0x%x", &opcode))
		return GP_ERROR_BAD_PARAMETERS;
	nparams = 0; x = val;
	while ((x = strchr(x,',')) && (nparams<5)) {
		x++;
		if (!sscanf(x,"0x%x", &xparams[nparams]))
			return GP_ERROR_BAD_PARAMETERS;
		nparams++;
	}
	ptp.Code = opcode;
	ptp.Nparam = nparams;
	ptp.Param1 = xparams[0];
	ptp.Param2 = xparams[1];
	ptp.Param3 = xparams[2];
	ptp.Param4 = xparams[3];
	ptp.Param5 = xparams[4];

	/* FIXME: handle in data */

	ret = ptp_transaction (params, &ptp, PTP_DP_GETDATA, 0, &data, &size);

	/* FIXME: handle out data (store locally?) */

	return translate_ptp_result (ret);
}

static int
_get_Sony_Movie(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Sony_Movie(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	PTPPropertyValue	value;
	GPContext *context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));
	if (val)
		value.u16 = 2;
	else
		value.u16 = 1;
        C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, 0xD2C8, &value, PTP_DTC_UINT16 ));
	return GP_OK;
}
static int
_get_Nikon_Movie(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Nikon_Movie(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val, ret;
	GPContext *context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		PTPPropertyValue	value;

		if (have_prop(camera,PTP_VENDOR_NIKON,PTP_DPC_NIKON_ApplicationMode)) {
			value.u8 = 0;
			C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_ApplicationMode, &value, PTP_DTC_UINT8));
			if (value.u8 != 1) {
				value.u8 = 1;
				C_PTP (ptp_setdevicepropvalue (params, PTP_DPC_NIKON_ApplicationMode, &value, PTP_DTC_UINT8));
			}
		}

                ret = ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &value, PTP_DTC_UINT8);
                if (ret != PTP_RC_OK)
                        value.u8 = 0;

                if (!value.u8) {
                        value.u8 = 1;
                        LOG_ON_PTP_E (ptp_setdevicepropvalue (params, PTP_DPC_NIKON_RecordingMedia, &value, PTP_DTC_UINT8));
                        C_PTP_REP_MSG (ptp_nikon_start_liveview (params),
                                       _("Nikon enable liveview failed"));
			C_PTP_REP_MSG (nikon_wait_busy(params, 50, 1000),
				       _("Nikon enable liveview failed"));
		}
		C_PTP_REP (ptp_nikon_startmovie (params));
	} else
		C_PTP_REP (ptp_nikon_stopmovie (params));
	return GP_OK;
}

static int
_get_Nikon_Bulb(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Nikon_Bulb(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		PTPPropertyValue propval2;
		char buf[20];

		C_PTP (ptp_nikon_setcontrolmode (params, 1));
		propval2.u16 = 1; /* Exposure Mode to Full Manual */
		C_PTP (ptp_setdevicepropvalue (params, PTP_DPC_ExposureProgramMode, &propval2, PTP_DTC_UINT16));
		propval2.u32 = 0xffffffff; /* Exposure Time to bulb */
		C_PTP_MSG (ptp_setdevicepropvalue (params, PTP_DPC_ExposureTime, &propval2, PTP_DTC_UINT32),
			   "failed to set exposuretime to bulb");
		/* If there is no capturetarget set yet, the default is "sdram" */
		if (GP_OK != gp_setting_get("ptp2","capturetarget",buf))
			strcpy (buf, "sdram");

		C_PTP_MSG (ptp_nikon_capture2 (params, 0/*No AF*/, !strcmp(buf,"sdram")),
			   "failed to initiate bulb capture");
		return GP_OK;
	} else {
		C_PTP (ptp_nikon_terminatecapture (params, 0, 0));
		C_PTP (nikon_wait_busy(params, 100, 5000));
		return GP_OK;
	}
}


static int
_get_OpenCapture(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_OpenCapture(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	GPContext *context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		C_PTP_REP (ptp_initiateopencapture (params, 0x0, 0x0)); /* so far use only defaults for storage and ofc */
		params->opencapture_transid = params->transaction_id;
	} else {
		C_PTP_REP (ptp_terminateopencapture (params, params->opencapture_transid));
	}
	return GP_OK;
}

static int
_get_Sony_Bulb(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Sony_Bulb(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	PTPPropertyValue xpropval;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		xpropval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, 0xD2C1, &xpropval, PTP_DTC_UINT16));

		xpropval.u16 = 2;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_StillImage, &xpropval, PTP_DTC_UINT16));
	} else {
		xpropval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, 0xD2C2, &xpropval, PTP_DTC_UINT16));

		xpropval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, 0xD2C1, &xpropval, PTP_DTC_UINT16));
	}
	return GP_OK;
}


static int
_get_Canon_EOS_Bulb(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Canon_EOS_Bulb(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	GPContext *context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		int ret = ptp_canon_eos_bulbstart (params);
		if (ret == PTP_RC_GeneralError) {
			gp_context_error (((PTPData *) camera->pl->params.data)->context,
			_("For bulb capture to work, make sure the mode dial is switched to 'M' and set 'shutterspeed' to 'bulb'."));
			return translate_ptp_result (ret);
		}
		C_PTP_REP (ret);
	} else {
		C_PTP_REP (ptp_canon_eos_bulbend (params));
	}
	return GP_OK;
}

static int
_get_Canon_EOS_UILock(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Canon_EOS_UILock(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	GPContext *context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));
	if (val)
		C_PTP_REP (ptp_canon_eos_setuilock (params));
	else
		C_PTP_REP (ptp_canon_eos_resetuilock (params));
	return GP_OK;
}

static int
_get_Nikon_FastFS(CONFIG_GET_ARGS) {
	int val;
	char buf[1024];

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	val = 1; /* default is fast fs */
	if (GP_OK == gp_setting_get("ptp2","nikon.fastfilesystem", buf))
		val = atoi(buf);
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Nikon_FastFS(CONFIG_PUT_ARGS) {
	int val;
	char buf[20];

	CR (gp_widget_get_value(widget, &val));
	sprintf(buf,"%d",val);
	gp_setting_set("ptp2","nikon.fastfilesystem",buf);
	return GP_OK;
}

static struct {
	char	*name;
	char	*label;
} capturetargets[] = {
	{"sdram", N_("Internal RAM") },
	{"card", N_("Memory card") },
};

static int
_get_CaptureTarget(CONFIG_GET_ARGS) {
	int i;
	char buf[1024];

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (GP_OK != gp_setting_get("ptp2","capturetarget", buf))
		strcpy(buf,"sdram");

	for (i=0;i<sizeof (capturetargets)/sizeof (capturetargets[i]);i++) {
		gp_widget_add_choice (*widget, _(capturetargets[i].label));
		if (!strcmp (buf,capturetargets[i].name))
			gp_widget_set_value (*widget, _(capturetargets[i].label));
	}
	return GP_OK;
}

static int
_put_CaptureTarget(CONFIG_PUT_ARGS) {
	int		i;
	char		*val;
	PTPParams	*params = &(camera->pl->params);
	GPContext	*context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));
	for (i=0;i<sizeof(capturetargets)/sizeof(capturetargets[i]);i++) {
		if (!strcmp( val, _(capturetargets[i].label))) {
			gp_setting_set("ptp2","capturetarget",capturetargets[i].name);
			break;
		}
	}
	/* Also update it in the live Canon EOS camera. (Nikon and Canon Powershot just use different opcodes.) */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_RemoteRelease)
	)
		CR (camera_canon_eos_update_capture_target( camera, context, -1 ));

	return GP_OK;
}

static struct {
	char	*name;
	char	*label;
} chdkonoff[] = {
	{"on", N_("On") },
	{"off", N_("Off") },
};

static int
_get_CHDK(CONFIG_GET_ARGS) {
	int i;
	char buf[1024];

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
_put_CHDK(CONFIG_PUT_ARGS) {
	int i;
	char *val;

	CR (gp_widget_get_value(widget, &val));
	for (i=0;i<sizeof(chdkonoff)/sizeof(chdkonoff[i]);i++) {
		if (!strcmp( val, _(chdkonoff[i].label))) {
			gp_setting_set("ptp2","chdk",chdkonoff[i].name);
			break;
		}
	}
	return GP_OK;
}
/* Wifi profiles functions */

static int
_put_nikon_list_wifi_profiles (CONFIG_PUT_ARGS)
{
	int i;
	CameraWidget *child, *child2;
	const char *name;
	int value;
	char* endptr;
	long val;
	PTPParams *params = &(camera->pl->params);

	if (camera->pl->params.deviceinfo.VendorExtensionID != PTP_VENDOR_NIKON)
		return (GP_ERROR_NOT_SUPPORTED);

	for (i = 0; i < gp_widget_count_children(widget); i++) {
		gp_widget_get_child(widget, i, &child);
		gp_widget_get_child_by_name(child, "delete", &child2);

		gp_widget_get_value(child2, &value);
		if (value) {
			gp_widget_get_name(child, &name);
			/* FIXME: far from elegant way to get ID back... */
			val = strtol(name, &endptr, 0);
			if (!*endptr) {
				C_PTP (ptp_nikon_deletewifiprofile(&(camera->pl->params), val));
				gp_widget_set_value(child2, 0);
			}
		}
	}

	/* FIXME: deleted entry still exists, rebuild tree if deleted = 1 ? */
	
	return GP_OK;
}

static int
_get_nikon_list_wifi_profiles (CONFIG_GET_ARGS)
{
	CameraWidget *child;
	int ret;
	char buffer[4096];
	int i;
	PTPParams *params = &(camera->pl->params);

	if (params->deviceinfo.VendorExtensionID != PTP_VENDOR_NIKON)
		return (GP_ERROR_NOT_SUPPORTED);

	/* check for more codes, on non-wireless nikons getwifiprofilelist might hang */
	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetProfileAllData)	||
	    !ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_SendProfileData)	||
	    !ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_DeleteProfile)		||
	    !ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_SetProfileData))
		return (GP_ERROR_NOT_SUPPORTED);

	ret = ptp_nikon_getwifiprofilelist(params);
	if (ret != PTP_RC_OK)
		return (GP_ERROR_NOT_SUPPORTED);

	gp_widget_new (GP_WIDGET_SECTION, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_new (GP_WIDGET_TEXT, "Version", &child);
	snprintf(buffer, 4096, "%d", params->wifi_profiles_version);
	gp_widget_set_value(child, buffer);
	gp_widget_append(*widget, child);

	for (i = 0; i < params->wifi_profiles_number; i++) {
		CameraWidget *child2;
		if (params->wifi_profiles[i].valid) {
			gp_widget_new (GP_WIDGET_SECTION, params->wifi_profiles[i].profile_name, &child);
			snprintf(buffer, 4096, "%d", params->wifi_profiles[i].id);
			gp_widget_set_name(child, buffer);
			gp_widget_append(*widget, child);

			gp_widget_new (GP_WIDGET_TEXT, _("ID"), &child2);
			snprintf (buffer, 4096, "%d", params->wifi_profiles[i].id);
			gp_widget_set_value(child2, buffer);
			gp_widget_append(child, child2);

			gp_widget_new (GP_WIDGET_TEXT, _("ESSID"), &child2);
			snprintf (buffer, 4096, "%s", params->wifi_profiles[i].essid);
			gp_widget_set_value(child2, buffer);
			gp_widget_append(child, child2);

			gp_widget_new (GP_WIDGET_TEXT, _("Display"), &child2);
			snprintf (buffer, 4096, "Order: %d, Icon: %d, Device type: %d",
			          params->wifi_profiles[i].display_order,
			          params->wifi_profiles[i].icon_type,
			          params->wifi_profiles[i].device_type);
			gp_widget_set_value(child2, buffer);
			gp_widget_append(child, child2);
			
			gp_widget_new (GP_WIDGET_TEXT, "Dates", &child2);
			snprintf (buffer, 4096,
				_("Creation date: %s, Last usage date: %s"),
				params->wifi_profiles[i].creation_date,
				params->wifi_profiles[i].lastusage_date);
			gp_widget_set_value(child2, buffer);
			gp_widget_append(child, child2);

			gp_widget_new (GP_WIDGET_TOGGLE, _("Delete"), &child2);
			gp_widget_set_value(child2, 0);
			gp_widget_set_name(child2, "delete");
			gp_widget_append(child, child2);
		}
	}

	return GP_OK;
}

static int
_get_nikon_wifi_profile_prop(CONFIG_GET_ARGS) {
	char buffer[1024];
	
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_setting_get("ptp2_wifi",menu->name,buffer);
	gp_widget_set_value(*widget,buffer);
	return (GP_OK);
}

static int
_put_nikon_wifi_profile_prop(CONFIG_PUT_ARGS) {
	char *string, *name;
	CR (gp_widget_get_value(widget, &string));
	gp_widget_get_name(widget,(const char**)&name);
	gp_setting_set("ptp2_wifi",name,string);
	return (GP_OK);
}

static int
_get_nikon_wifi_profile_channel(CONFIG_GET_ARGS) {
	char buffer[1024];
	float val;
	
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_setting_get("ptp2_wifi", menu->name, buffer);
	val = (float)atoi(buffer);
	gp_widget_set_range(*widget, 1.0, 11.0, 1.0);
	if (!val)
		val = 1.0;
	gp_widget_set_value(*widget, &val);
	
	return (GP_OK);
}

static int
_put_nikon_wifi_profile_channel(CONFIG_PUT_ARGS) {
	char *name;
	float val;
	char buffer[16];
	CR (gp_widget_get_value(widget, &val));
	gp_widget_get_name(widget,(const char**)&name);

	snprintf(buffer, 16, "%d", (int)val);
	gp_setting_set("ptp2_wifi",name,buffer);
	return GP_OK;
}

static char* encryption_values[] = {
N_("None"),
N_("WEP 64-bit"),
N_("WEP 128-bit"),
NULL
};

static int
_get_nikon_wifi_profile_encryption(CONFIG_GET_ARGS) {
	char buffer[1024];
	int i;
	int val;
	
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_setting_get("ptp2_wifi", menu->name, buffer);
	val = atoi(buffer);
	
	for (i = 0; encryption_values[i]; i++) {
		gp_widget_add_choice(*widget, _(encryption_values[i]));
		if (i == val)
			gp_widget_set_value(*widget, _(encryption_values[i]));
	}
	
	return (GP_OK);
}

static int
_put_nikon_wifi_profile_encryption(CONFIG_PUT_ARGS) {
	char *string, *name;
	int i;
	char buffer[16];
	CR (gp_widget_get_value(widget, &string));
	gp_widget_get_name(widget,(const char**)&name);

	for (i = 0; encryption_values[i]; i++) {
		if (!strcmp(_(encryption_values[i]), string)) {
			snprintf(buffer, 16, "%d", i);
			gp_setting_set("ptp2_wifi",name,buffer);
			return GP_OK;
		}
	}

	return GP_ERROR_BAD_PARAMETERS;
}

static char* accessmode_values[] = {
N_("Managed"),
N_("Ad-hoc"),
NULL
};

static int
_get_nikon_wifi_profile_accessmode(CONFIG_GET_ARGS) {
	char buffer[1024];
	int i;
	int val;
	
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_setting_get("ptp2_wifi", menu->name, buffer);
	val = atoi(buffer);
	
	for (i = 0; accessmode_values[i]; i++) {
		gp_widget_add_choice(*widget, _(accessmode_values[i]));
		if (i == val)
			gp_widget_set_value(*widget, _(accessmode_values[i]));
	}
	
	return (GP_OK);
}

static int
_put_nikon_wifi_profile_accessmode(CONFIG_PUT_ARGS) {
	char *string, *name;
	int i;
	char buffer[16];
	CR (gp_widget_get_value(widget, &string));
	gp_widget_get_name(widget,(const char**)&name);

	for (i = 0; accessmode_values[i]; i++) {
		if (!strcmp(_(accessmode_values[i]), string)) {
			snprintf(buffer, 16, "%d", i);
			gp_setting_set("ptp2_wifi",name,buffer);
			return GP_OK;
		}
	}

	return GP_ERROR_BAD_PARAMETERS;
}

static int
_get_nikon_wifi_profile_write(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value(*widget, 0);
	return (GP_OK);
}

static int
_put_nikon_wifi_profile_write(CONFIG_PUT_ARGS) {
	char buffer[1024];
	char keypart[3];
	char* pos, *endptr;
	int value, i;
	CR (gp_widget_get_value(widget, &value));
	if (value) {
		struct in_addr inp;
		PTPNIKONWifiProfile profile;
		memset(&profile, 0, sizeof(PTPNIKONWifiProfile));
		profile.icon_type = 1;
		profile.key_nr = 1;

		gp_setting_get("ptp2_wifi","name",buffer);
		strncpy(profile.profile_name, buffer, 16);
		gp_setting_get("ptp2_wifi","essid",buffer);
		strncpy(profile.essid, buffer, 32);

		gp_setting_get("ptp2_wifi","accessmode",buffer);
		profile.access_mode = atoi(buffer);

		gp_setting_get("ptp2_wifi","ipaddr",buffer);
/* fixme: replacement on win32 where we do not have this helper */
#ifdef HAVE_INET_ATON
		if (buffer[0] != 0) { /* No DHCP */
			C_PARAMS_MSG (inet_aton (buffer, &inp), "failed to scan for addr in %s.", buffer);
			profile.ip_address = inp.s_addr;
			gp_setting_get("ptp2_wifi","netmask",buffer);
			C_PARAMS_MSG (inet_aton (buffer, &inp), "failed to scan for netmask in %s.", buffer);
			inp.s_addr = be32toh(inp.s_addr); /* Reverse bytes so we can use the code below. */
			profile.subnet_mask = 32;
			while (((inp.s_addr >> (32-profile.subnet_mask)) & 0x01) == 0) {
				profile.subnet_mask--;
				C_PARAMS_MSG (profile.subnet_mask > 0, "Invalid subnet mask %s: no zeros.", buffer);
			}
			/* Check there is only ones left */
			C_PARAMS_MSG ((inp.s_addr | ((0x01 << (32-profile.subnet_mask)) - 1)) == 0xFFFFFFFF,
				"Invalid subnet mask %s: misplaced zeros.", buffer);

			/* Gateway (never tested) */
			gp_setting_get("ptp2_wifi","gw",buffer);
			if (*buffer) {
				C_PARAMS_MSG (inet_aton (buffer, &inp), "failed to scan for gw in %s", buffer);
				profile.gateway_address = inp.s_addr;
			}
		}
		else
#endif
		{ /* DHCP */
			/* Never use mode 2, as mode 3 switches to mode 2
			 * if it gets no DHCP answer. */
			profile.address_mode = 3;
		}

		gp_setting_get("ptp2_wifi","channel",buffer);
		profile.wifi_channel = atoi(buffer);

		/* Encryption */
		gp_setting_get("ptp2_wifi","encryption",buffer);
		profile.encryption = atoi(buffer);
		
		if (profile.encryption != 0) {
			gp_setting_get("ptp2_wifi","key",buffer);
			i = 0;
			pos = buffer;
			keypart[2] = 0;
			while (*pos) {
				C_PARAMS_MSG (*(pos+1), "Bad key: '%s'", buffer);
				keypart[0] = *(pos++);
				keypart[1] = *(pos++);
				profile.key[i++] = strtol(keypart, &endptr, 16);
				C_PARAMS_MSG (endptr == keypart+2, "Bad key: '%s', '%s' is not a number.", buffer, keypart);
				if (*pos == ':')
					pos++;
			}
			if (profile.encryption == 1) /* WEP 64-bit */
				C_PARAMS_MSG (i == 5, "Bad key: '%s', %d bit length, should be 40 bit.", buffer, i*8); /* 5*8 = 40 bit + 24 bit (IV) = 64 bit */
			else if (profile.encryption == 2) /* WEP 128-bit */
				C_PARAMS_MSG (i == 13, "Bad key: '%s', %d bit length, should be 104 bit.", buffer, i*8); /* 13*8 = 104 bit + 24 bit (IV) = 128 bit */
		}

		ptp_nikon_writewifiprofile(&(camera->pl->params), &profile);
	}
	return (GP_OK);
}

static struct submenu create_wifi_profile_submenu[] = {
	{ N_("Profile name"),                   "name",         0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_prop,       _put_nikon_wifi_profile_prop },
	{ N_("WIFI ESSID"),                     "essid",        0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_prop,       _put_nikon_wifi_profile_prop },
	{ N_("IP address (empty for DHCP)"),    "ipaddr",       0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_prop,       _put_nikon_wifi_profile_prop },
	{ N_("Network mask"),                   "netmask",      0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_prop,       _put_nikon_wifi_profile_prop },
	{ N_("Default gateway"),                "gw",           0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_prop,       _put_nikon_wifi_profile_prop },
	{ N_("Access mode"),                    "accessmode",   0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_accessmode, _put_nikon_wifi_profile_accessmode },
	{ N_("WIFI channel"),                   "channel",      0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_channel,    _put_nikon_wifi_profile_channel },
	{ N_("Encryption"),                     "encryption",   0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_encryption, _put_nikon_wifi_profile_encryption },
	{ N_("Encryption key (hex)"),           "key",          0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_prop,       _put_nikon_wifi_profile_prop },
	{ N_("Write"),                          "write",        0,  PTP_VENDOR_NIKON,   0,  _get_nikon_wifi_profile_write,      _put_nikon_wifi_profile_write },
	{ 0,0,0,0,0,0,0 },
};

static int
_get_nikon_create_wifi_profile (CONFIG_GET_ARGS)
{
	int submenuno, ret;
	CameraWidget *subwidget;
	
	gp_widget_new (GP_WIDGET_SECTION, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (submenuno = 0; create_wifi_profile_submenu[submenuno].name ; submenuno++ ) {
		struct submenu *cursub = create_wifi_profile_submenu+submenuno;

		ret = cursub->getfunc (camera, &subwidget, cursub, NULL);
		if (ret == GP_OK)
			gp_widget_append (*widget, subwidget);
	}
	
	return GP_OK;
}

static int
_put_nikon_create_wifi_profile (CONFIG_PUT_ARGS)
{
	int submenuno, ret;
	CameraWidget *subwidget;
	
	for (submenuno = 0; create_wifi_profile_submenu[submenuno].name ; submenuno++ ) {
		struct submenu *cursub = create_wifi_profile_submenu+submenuno;

		ret = gp_widget_get_child_by_label (widget, _(cursub->label), &subwidget);
		if (ret != GP_OK)
			continue;

		if (!gp_widget_changed (subwidget))
			continue;

		ret = cursub->putfunc (camera, subwidget, NULL, NULL);
	}

	return GP_OK;
}

static struct submenu wifi_profiles_menu[] = {
	/* wifi */
	{ N_("List Wifi profiles"), "list", 0, PTP_VENDOR_NIKON, 0, _get_nikon_list_wifi_profiles, _put_nikon_list_wifi_profiles },
	{ N_("Create Wifi profile"), "new", 0, PTP_VENDOR_NIKON, 0, _get_nikon_create_wifi_profile, _put_nikon_create_wifi_profile },
	{ 0,0,0,0,0,0,0 },
};

/* Wifi profiles menu is a non-standard menu, because putfunc is always
 * called on each submenu, whether or not the value has been changed. */
static int
_get_wifi_profiles_menu (CONFIG_MENU_GET_ARGS)
{
	CameraWidget *subwidget;
	int submenuno, ret;

	if (camera->pl->params.deviceinfo.VendorExtensionID != PTP_VENDOR_NIKON)
		return (GP_ERROR_NOT_SUPPORTED);

	if (!ptp_operation_issupported (&camera->pl->params, PTP_OC_NIKON_GetProfileAllData))
		return (GP_ERROR_NOT_SUPPORTED);

	gp_widget_new (GP_WIDGET_SECTION, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (submenuno = 0; wifi_profiles_menu[submenuno].name ; submenuno++ ) {
		struct submenu *cursub = wifi_profiles_menu+submenuno;

		ret = cursub->getfunc (camera, &subwidget, cursub, NULL);
		if (ret == GP_OK)
			gp_widget_append (*widget, subwidget);
	}

	return GP_OK;
}

static int
_put_wifi_profiles_menu (CONFIG_MENU_PUT_ARGS)
{
	int submenuno, ret;
	CameraWidget *subwidget;
	
	for (submenuno = 0; wifi_profiles_menu[submenuno].name ; submenuno++ ) {
		struct submenu *cursub = wifi_profiles_menu+submenuno;

		ret = gp_widget_get_child_by_label (widget, _(cursub->label), &subwidget);
		if (ret != GP_OK)
			continue;

		ret = cursub->putfunc (camera, subwidget, NULL, NULL);
	}

	return GP_OK;
}

static int
_get_PTP_DeviceVersion_STR(CONFIG_GET_ARGS) {
	PTPParams	*params = &camera->pl->params;

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value (*widget,params->deviceinfo.DeviceVersion?params->deviceinfo.DeviceVersion:_("None"));
	return GP_OK;
}

static int
_get_PTP_Model_STR(CONFIG_GET_ARGS) {
	PTPParams	*params = &camera->pl->params;

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value (*widget,params->deviceinfo.Model?params->deviceinfo.Model:_("None"));
	return GP_OK;
}

static int
_get_PTP_VendorExtension_STR(CONFIG_GET_ARGS) {
	PTPParams	*params = &camera->pl->params;

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value (*widget,params->deviceinfo.VendorExtensionDesc?params->deviceinfo.VendorExtensionDesc:_("None"));
	return GP_OK;
}

static int
_get_PTP_Serial_STR(CONFIG_GET_ARGS) {
	PTPParams	*params = &camera->pl->params;

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value (*widget,params->deviceinfo.SerialNumber?params->deviceinfo.SerialNumber:_("None"));
	return GP_OK;
}

static int
_get_PTP_Manufacturer_STR(CONFIG_GET_ARGS) {
	PTPParams	*params = &camera->pl->params;

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value (*widget,params->deviceinfo.Manufacturer?params->deviceinfo.Manufacturer:_("None"));
	return GP_OK;
}


static struct submenu camera_actions_menu[] = {
	/* { N_("Viewfinder Mode"), "viewfinder", PTP_DPC_CANON_ViewFinderMode, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_ViewFinderMode, _put_Canon_ViewFinderMode}, */
	/*{ N_("Synchronize camera date and time with PC"),"syncdatetime", PTP_DPC_CANON_UnixTime, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_SyncTime, _put_Canon_SyncTime },*/
	{ N_("Synchronize camera date and time with PC"),"syncdatetimeutc", PTP_DPC_CANON_EOS_UTCTime, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_SyncTime, _put_Canon_SyncTime },
	{ N_("Synchronize camera date and time with PC"),"syncdatetime", PTP_DPC_CANON_EOS_CameraTime, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_SyncTime, _put_Canon_SyncTime },

	{ N_("Power Down"),                     "powerdown",        0,  0,                  PTP_OC_PowerDown,                   _get_PowerDown,                 _put_PowerDown },
	{ N_("Focus Lock"),                     "focuslock",        0,  PTP_VENDOR_CANON,   PTP_OC_CANON_FocusLock,             _get_Canon_FocusLock,           _put_Canon_FocusLock },
	{ N_("Bulb Mode"),                      "bulb",             0xd2c1,  PTP_VENDOR_SONY,   0,                              _get_Sony_Bulb,                 _put_Sony_Bulb },
	{ N_("Bulb Mode"),                      "bulb",             0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_BulbStart,         _get_Canon_EOS_Bulb,            _put_Canon_EOS_Bulb },
	{ N_("Bulb Mode"),                      "bulb",             0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_TerminateCapture,      _get_Nikon_Bulb,                _put_Nikon_Bulb },
	{ N_("UI Lock"),                        "uilock",           0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_SetUILock,         _get_Canon_EOS_UILock,          _put_Canon_EOS_UILock },
	{ N_("Drive Nikon DSLR Autofocus"),     "autofocusdrive",   0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_AfDrive,               _get_Nikon_AFDrive,             _put_Nikon_AFDrive },
	{ N_("Drive Canon DSLR Autofocus"),     "autofocusdrive",   0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_DoAf,              _get_Canon_EOS_AFDrive,         _put_Canon_EOS_AFDrive },
	{ N_("Drive Nikon DSLR Manual focus"),  "manualfocusdrive", 0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_MfDrive,               _get_Nikon_MFDrive,             _put_Nikon_MFDrive },
	{ N_("Set Nikon Autofocus area"),       "changeafarea",     0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_ChangeAfArea,          _get_Nikon_ChangeAfArea,        _put_Nikon_ChangeAfArea },
	{ N_("Set Nikon Control Mode"),         "controlmode",      0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_SetControlMode,        _get_Nikon_ControlMode,         _put_Nikon_ControlMode },
	{ N_("Drive Canon DSLR Manual focus"),  "manualfocusdrive", 0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_DriveLens,         _get_Canon_EOS_MFDrive,         _put_Canon_EOS_MFDrive },
	{ N_("Cancel Canon DSLR Autofocus"),    "cancelautofocus",  0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_AfCancel,          _get_Canon_EOS_AFCancel,        _put_Canon_EOS_AFCancel },
	{ N_("Canon EOS Zoom"),                 "eoszoom",          0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_Zoom,              _get_Canon_EOS_Zoom,            _put_Canon_EOS_Zoom },
	{ N_("Canon EOS Zoom Position"),        "eoszoomposition",  0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_ZoomPosition,      _get_Canon_EOS_ZoomPosition,    _put_Canon_EOS_ZoomPosition },
	{ N_("Canon EOS Viewfinder"),           "viewfinder",       0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_GetViewFinderData, _get_Canon_EOS_ViewFinder,      _put_Canon_EOS_ViewFinder },
	{ N_("Nikon Viewfinder"),               "viewfinder",       0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_StartLiveView,         _get_Nikon_ViewFinder,          _put_Nikon_ViewFinder },
	{ N_("Canon EOS Remote Release"),       "eosremoterelease", 0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_RemoteReleaseOn,   _get_Canon_EOS_RemoteRelease,   _put_Canon_EOS_RemoteRelease },
	{ N_("CHDK Script"),                    "chdk_script",      0,  PTP_VENDOR_CANON,   PTP_OC_CHDK,                        _get_Canon_CHDK_Script,         _put_Canon_CHDK_Script },
	{ N_("Movie Capture"),                  "movie",            0,  0,                  PTP_OC_InitiateOpenCapture,         _get_OpenCapture,               _put_OpenCapture },
	{ N_("Movie Capture"),                  "movie",            0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_StartMovieRecInCard,   _get_Nikon_Movie,               _put_Nikon_Movie },
	{ N_("Movie Capture"),                  "movie",            0,  PTP_VENDOR_SONY,    PTP_OC_SONY_SDIOConnect,            _get_Sony_Movie,                _put_Sony_Movie },
	{ N_("PTP Opcode"),                     "opcode",           0,  0,                  PTP_OC_GetDeviceInfo,               _get_Generic_OPCode,            _put_Generic_OPCode },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu camera_status_menu[] = {
	{ N_("Serial Number"),          "serialnumber",     0,  0,  PTP_OC_GetDeviceInfo,   _get_PTP_Serial_STR,            _put_None },
	{ N_("Camera Manufacturer"),    "manufacturer",     0,  0,  PTP_OC_GetDeviceInfo,   _get_PTP_Manufacturer_STR,      _put_None },
	{ N_("Camera Model"),           "cameramodel",      0,  0,  PTP_OC_GetDeviceInfo,   _get_PTP_Model_STR,             _put_None },
	{ N_("Device Version"),         "deviceversion",    0,  0,  PTP_OC_GetDeviceInfo,   _get_PTP_DeviceVersion_STR,     _put_None },
	{ N_("Vendor Extension"),       "vendorextension",  0,  0,  PTP_OC_GetDeviceInfo,   _get_PTP_VendorExtension_STR,   _put_None },

	{ N_("Camera Model"),           "model",            PTP_DPC_CANON_CameraModel,              PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Camera Model"),           "model",            PTP_DPC_CANON_EOS_ModelID,              PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Firmware Version"),       "firmwareversion",  PTP_DPC_CANON_FirmwareVersion,          PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_CANON_FirmwareVersion,     _put_None },
	{ N_("PTP Version"),            "ptpversion",       PTP_DPC_CANON_EOS_PTPExtensionVersion,  PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("DPOF Version"),           "dpofversion",      PTP_DPC_CANON_EOS_DPOFVersion,          PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_INT,                       _put_None },
	{ N_("AC Power"),               "acpower",          PTP_DPC_NIKON_ACPower,                  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("External Flash"),         "externalflash",    PTP_DPC_NIKON_ExternalFlashAttached,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("Battery Level"),          "batterylevel",     PTP_DPC_BatteryLevel,                   0,                  PTP_DTC_UINT8,  _get_BatteryLevel,              _put_None },
	{ N_("Battery Level"),          "batterylevel",     PTP_DPC_CANON_EOS_BatteryPower,         PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_BatteryLevel,    _put_None },
	{ N_("Camera Orientation"),     "orientation",      PTP_DPC_NIKON_CameraOrientation,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_CameraOrientation,   _put_None },
	{ N_("Camera Orientation"),     "orientation2",     PTP_DPC_NIKON_AngleLevel,               PTP_VENDOR_NIKON,   PTP_DTC_INT32,  _get_Nikon_AngleLevel,          _put_None },
	{ N_("Camera Orientation"),     "orientation",      PTP_DPC_CANON_RotationAngle,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_CameraOrientation,   _put_None },
	{ N_("Flash Open"),             "flashopen",        PTP_DPC_NIKON_FlashOpen,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("Flash Charged"),          "flashcharged",     PTP_DPC_NIKON_FlashCharged,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("Lens Name"),              "lensname",         PTP_DPC_NIKON_LensID,                   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LensID,              _put_None },
	{ N_("Lens Name"),              "lensname",         PTP_DPC_CANON_EOS_LensName,             PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Serial Number"),          "eosserialnumber",  PTP_DPC_CANON_EOS_SerialNumber,         PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Shutter Counter"),        "shuttercounter",   PTP_DPC_CANON_EOS_ShutterCounter,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Available Shots"),        "availableshots",   PTP_DPC_CANON_EOS_AvailableShots,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Focal Length Minimum"),   "minfocallength",   PTP_DPC_NIKON_FocalLengthMin,           PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_FocalLength,         _put_None },
	{ N_("Focal Length Maximum"),   "maxfocallength",   PTP_DPC_NIKON_FocalLengthMax,           PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_FocalLength,         _put_None },
	{ N_("Maximum Aperture at Focal Length Minimum"), "apertureatminfocallength", PTP_DPC_NIKON_MaxApAtMinFocalLength, PTP_VENDOR_NIKON, PTP_DTC_UINT16, _get_Nikon_ApertureAtFocalLength, _put_None },
	{ N_("Maximum Aperture at Focal Length Maximum"), "apertureatmaxfocallength", PTP_DPC_NIKON_MaxApAtMaxFocalLength, PTP_VENDOR_NIKON, PTP_DTC_UINT16, _get_Nikon_ApertureAtFocalLength, _put_None },
	{ N_("Low Light"),              "lowlight",         PTP_DPC_NIKON_ExposureDisplayStatus,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LowLight,            _put_None },
	{ N_("Light Meter"),            "lightmeter",       PTP_DPC_NIKON_LightMeter,               PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_LightMeter,          _put_None },
	{ N_("Light Meter"),            "lightmeter",       PTP_DPC_NIKON_ExposureIndicateStatus,   PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Range_INT8,                _put_None },
	{ N_("AF Locked"),              "aflocked",         PTP_DPC_NIKON_AFLockStatus,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("AE Locked"),              "aelocked",         PTP_DPC_NIKON_AELockStatus,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("FV Locked"),              "fvlocked",         PTP_DPC_NIKON_FVLockStatus,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu camera_settings_menu[] = {
	{ N_("Camera Date and Time"),   "datetimeutc",          PTP_DPC_CANON_EOS_UTCTime,          PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_UINT32_as_time,            _put_UINT32_as_time },
	{ N_("Camera Date and Time"),   "datetime",             PTP_DPC_CANON_UnixTime,             PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_UINT32_as_localtime,       _put_UINT32_as_localtime },
	{ N_("Camera Date and Time"),   "datetime",             PTP_DPC_CANON_EOS_CameraTime,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_UINT32_as_localtime,       _put_UINT32_as_localtime },
	{ N_("Camera Date and Time"),   "datetime",             PTP_DPC_DateTime,                   0,                  PTP_DTC_STR,    _get_STR_as_time,               _put_STR_as_time },
	{ N_("Beep Mode"),              "beep",                 PTP_DPC_CANON_BeepMode,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_BeepMode,            _put_Canon_BeepMode },
	{ N_("Image Comment"),          "imagecomment",         PTP_DPC_NIKON_ImageCommentString,   PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("WLAN GUID"),          	"guid",         	PTP_DPC_NIKON_GUID,   		    PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Enable Image Comment"),   "imagecommentenable",   PTP_DPC_NIKON_ImageCommentEnable,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("LCD Off Time"),           "lcdofftime",           PTP_DPC_NIKON_MonitorOff,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LCDOffTime,          _put_Nikon_LCDOffTime },
	{ N_("Recording Media"),        "recordingmedia",       PTP_DPC_NIKON_RecordingMedia,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_RecordingMedia,      _put_Nikon_RecordingMedia },
	{ N_("Quick Review Time"),      "reviewtime",           PTP_DPC_CANON_EOS_QuickReviewTime,  PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_QuickReviewTime, _put_Canon_EOS_QuickReviewTime },
	{ N_("CSM Menu"),               "csmmenu",              PTP_DPC_NIKON_CSMMenu,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("Reverse Command Dial"),   "reversedial",          PTP_DPC_NIKON_ReverseCommandDial,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("Camera Output"),          "output",               PTP_DPC_CANON_CameraOutput,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_CameraOutput,        _put_Canon_CameraOutput },
	{ N_("Camera Output"),          "output",               PTP_DPC_CANON_EOS_EVFOutputDevice,  PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_CameraOutput,    _put_Canon_EOS_CameraOutput },
	{ N_("Recording Destination"),  "movierecordtarget",    PTP_DPC_CANON_EOS_EVFRecordStatus,  PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_EVFRecordTarget, _put_Canon_EOS_EVFRecordTarget },
	{ N_("EVF Mode"),               "evfmode",              PTP_DPC_CANON_EOS_EVFMode,          PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_EVFMode,         _put_Canon_EOS_EVFMode },
	{ N_("Owner Name"),             "ownername",            PTP_DPC_CANON_CameraOwner,          PTP_VENDOR_CANON,   PTP_DTC_AUINT8, _get_AUINT8_as_CHAR_ARRAY,      _put_AUINT8_as_CHAR_ARRAY },
	{ N_("Owner Name"),             "ownername",            PTP_DPC_CANON_EOS_Owner,            PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Artist"),                 "artist",               PTP_DPC_CANON_EOS_Artist,           PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Artist"),                 "artist",               PTP_DPC_NIKON_ArtistName,           PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("CCD Number"),             "ccdnumber",            PTP_DPC_NIKON_CCDNumber,            PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Copyright"),              "copyright",            PTP_DPC_CANON_EOS_Copyright,        PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Copyright"),              "copyright",            PTP_DPC_NIKON_CopyrightInfo,        PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Clean Sensor"),           "cleansensor",          PTP_DPC_NIKON_CleanImageSensor,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_CleanSensor,         _put_Nikon_CleanSensor },
	{ N_("Flicker Reduction"),      "flickerreduction",     PTP_DPC_NIKON_FlickerReduction,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlickerReduction,    _put_Nikon_FlickerReduction },
	{ N_("Custom Functions Ex"),    "customfuncex",         PTP_DPC_CANON_EOS_CustomFuncEx,     PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Focus Info"),             "focusinfo",            PTP_DPC_CANON_EOS_FocusInfoEx,      PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Focus Area"),             "focusarea",            PTP_DPC_CANON_EOS_AFSelectFocusArea,PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Auto Power Off"),         "autopoweroff",         PTP_DPC_CANON_EOS_AutoPowerOff,     PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_INT,                       _put_INT },
	{ N_("Depth of Field"),         "depthoffield",         PTP_DPC_CANON_EOS_DepthOfFieldPreview, PTP_VENDOR_CANON,PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Menus and Playback"),     "menusandplayback",     PTP_DPC_NIKON_MenusAndPlayback,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_MenusAndPlayback,    _put_Nikon_MenusAndPlayback },

/* virtual */
	{ N_("Fast Filesystem"),	"fastfs",	0,  PTP_VENDOR_NIKON,   0,  _get_Nikon_FastFS,      _put_Nikon_FastFS },
	{ N_("Capture Target"),		"capturetarget",0,  PTP_VENDOR_NIKON,   0,  _get_CaptureTarget,     _put_CaptureTarget },
	{ N_("Capture Target"),		"capturetarget",0,  PTP_VENDOR_CANON,   0,  _get_CaptureTarget,     _put_CaptureTarget },
	{ N_("CHDK"),     		"chdk",		PTP_OC_CHDK,  PTP_VENDOR_CANON,   0,  _get_CHDK,     _put_CHDK },
	{ N_("Capture"),		"capture",	0,  PTP_VENDOR_CANON,   0,  _get_Canon_CaptureMode, _put_Canon_CaptureMode },
	{ 0,0,0,0,0,0,0 },
};

/* think of this as properties of the "film" */
static struct submenu image_settings_menu[] = {
	{ N_("Image Quality"),          "imagequality",         PTP_DPC_CANON_ImageQuality,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_Quality,             _put_Canon_Quality },
	{ N_("Image Format"),           "imageformat",          PTP_DPC_CANON_FullViewFileFormat,       PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_Capture_Format,      _put_Canon_Capture_Format },
	{ N_("Image Format"),           "imageformat",          PTP_DPC_CANON_EOS_ImageFormat,          PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Image Format SD"),        "imageformatsd",        PTP_DPC_CANON_EOS_ImageFormatSD,        PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Image Format CF"),        "imageformatcf",        PTP_DPC_CANON_EOS_ImageFormatCF,        PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Image Format"),           "imageformat",          PTP_DPC_FUJI_Quality,                   PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_ImageFormat,          _put_Fuji_ImageFormat },
	{ N_("Image Format Ext HD"),    "imageformatexthd",     PTP_DPC_CANON_EOS_ImageFormatExtHD,     PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_ImageSize,                      0,                  PTP_DTC_STR,    _get_ImageSize,                 _put_ImageSize },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_NIKON_1_ImageSize,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon1_ImageSize,          _put_Nikon1_ImageSize },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_SONY_ImageSize,                 PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_ImageSize,            _put_Sony_ImageSize },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_CANON_ImageSize,                PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_Size,                _put_Canon_Size },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_CANON_ISOSpeed,                 PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ISO,                 _put_Canon_ISO },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_ExposureIndex,                  0,                  PTP_DTC_UINT16, _get_ISO,                       _put_ISO },
	{ N_("Movie ISO Speed"),        "movieiso",             PTP_DPC_NIKON_MovieISO,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_ISO32,                     _put_ISO32 },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_CANON_EOS_ISOSpeed,             PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ISO,                 _put_Canon_ISO },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_SONY_ISO,                       PTP_VENDOR_SONY,    PTP_DTC_UINT32, _get_Sony_ISO,                  _put_Sony_ISO },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_NIKON_1_ISO,                    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_ISO,               _put_Nikon_1_ISO },
	{ N_("ISO Auto"),               "isoauto",              PTP_DPC_NIKON_ISO_Auto,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_CANON_WhiteBalance,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_WhiteBalance,        _put_Canon_WhiteBalance },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_CANON_EOS_WhiteBalance,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_EOS_WhiteBalance,    _put_Canon_EOS_WhiteBalance },
	{ N_("Color Temperature"),      "colortemperature",     PTP_DPC_CANON_EOS_ColorTemperature,     PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Color Temperature"),      "colortemperature",     PTP_DPC_SONY_ColorTemp,                 PTP_VENDOR_SONY,    PTP_DTC_UINT16, _get_INT,                       _put_INT },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_WhiteBalance,                   0,                  PTP_DTC_UINT16, _get_WhiteBalance,              _put_WhiteBalance },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_NIKON_1_WhiteBalance,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_WhiteBalance,      _put_Nikon_1_WhiteBalance },
	{ N_("WhiteBalance Adjust A"),  "whitebalanceadjusta",  PTP_DPC_CANON_EOS_WhiteBalanceAdjustA,  PTP_VENDOR_CANON,   PTP_DTC_INT16,  _get_Canon_EOS_WBAdjust,        _put_Canon_EOS_WBAdjust },
	{ N_("WhiteBalance Adjust B"),  "whitebalanceadjustb",  PTP_DPC_CANON_EOS_WhiteBalanceAdjustB,  PTP_VENDOR_CANON,   PTP_DTC_INT16,  _get_Canon_EOS_WBAdjust,        _put_Canon_EOS_WBAdjust },
	{ N_("WhiteBalance X A"),       "whitebalancexa",       PTP_DPC_CANON_EOS_WhiteBalanceXA,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("WhiteBalance X B"),       "whitebalancexb",       PTP_DPC_CANON_EOS_WhiteBalanceXA,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Photo Effect"),           "photoeffect",          PTP_DPC_CANON_PhotoEffect,              PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_PhotoEffect,         _put_Canon_PhotoEffect },
	{ N_("Color Model"),            "colormodel",           PTP_DPC_NIKON_ColorModel,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ColorModel,          _put_Nikon_ColorModel },
	{ N_("Color Space"),            "colorspace",           PTP_DPC_NIKON_ColorSpace,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ColorSpace,          _put_Nikon_ColorSpace },
	{ N_("Color Space"),            "colorspace",           PTP_DPC_CANON_EOS_ColorSpace,           PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ColorSpace,      _put_Canon_EOS_ColorSpace },
	{ N_("Auto ISO"),               "autoiso",              PTP_DPC_NIKON_ISOAuto,                  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu capture_settings_menu[] = {
	{ N_("Long Exp Noise Reduction"),       "longexpnr",                PTP_DPC_NIKON_LongExposureNoiseReduction, PTP_VENDOR_NIKON, PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Long Exp Noise Reduction"),       "longexpnr",                PTP_DPC_NIKON_1_LongExposureNoiseReduction, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8,            _put_Nikon_OnOff_UINT8 },
	{ N_("Auto Focus Mode 2"),              "autofocusmode2",           PTP_DPC_NIKON_A4AFActivation,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Zoom"),                           "zoom",                     PTP_DPC_CANON_Zoom,                     PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ZoomRange,               _put_Canon_ZoomRange },
	{ N_("Assist Light"),                   "assistlight",              PTP_DPC_CANON_AssistLight,              PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_AssistLight,             _put_Canon_AssistLight },
	{ N_("Rotation Flag"),                  "autorotation",             PTP_DPC_CANON_RotationScene,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_AutoRotation,            _put_Canon_AutoRotation },
	{ N_("Self Timer"),                     "selftimer",                PTP_DPC_CANON_SelfTime,                 PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_SelfTimer,               _put_Canon_SelfTimer },
	{ N_("Assist Light"),                   "assistlight",              PTP_DPC_NIKON_AFAssist,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_ExposureBiasCompensation,       PTP_VENDOR_SONY,    PTP_DTC_INT16,  _get_ExpCompensation,               _put_Sony_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_ExposureBiasCompensation,       0,                  PTP_DTC_INT16,  _get_ExpCompensation,               _put_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_CANON_ExpCompensation,          PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ExpCompensation,         _put_Canon_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_CANON_EOS_ExpCompensation,      PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ExpCompensation2,        _put_Canon_ExpCompensation2 },
	/* these cameras also have PTP_DPC_ExposureBiasCompensation, avoid overlap */
	{ N_("Exposure Compensation"),          "exposurecompensation2",    PTP_DPC_NIKON_ExposureCompensation,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Flash Compensation"),             "flashcompensation",        PTP_DPC_CANON_FlashCompensation,        PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ExpCompensation,         _put_Canon_ExpCompensation },
	{ N_("AEB Exposure Compensation"),      "aebexpcompensation",       PTP_DPC_CANON_AEBExposureCompensation,  PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ExpCompensation,         _put_Canon_ExpCompensation },
	{ N_("Flash Mode"),                     "flashmode",                PTP_DPC_CANON_FlashMode,                PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_FlashMode,               _put_Canon_FlashMode },
	{ N_("Flash Mode"),                     "flashmode",                PTP_DPC_FlashMode,                      0,                  PTP_DTC_UINT16, _get_FlashMode,                     _put_FlashMode },
	{ N_("Nikon Flash Mode"),               "nikonflashmode",           PTP_DPC_NIKON_FlashMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_InternalFlashMode,       _put_Nikon_InternalFlashMode },
	{ N_("Flash Commander Mode"),           "flashcommandermode",       PTP_DPC_NIKON_FlashCommanderMode,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommanderMode,      _put_Nikon_FlashCommanderMode },
	{ N_("Flash Commander Power"),          "flashcommanderpower",      PTP_DPC_NIKON_FlashModeCommanderPower,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommanderPower,     _put_Nikon_FlashCommanderPower },
	{ N_("Flash Command Channel"),          "flashcommandchannel",      PTP_DPC_NIKON_FlashCommandChannel,      PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommandChannel,     _put_Nikon_FlashCommandChannel },
	{ N_("Flash Command Self Mode"),        "flashcommandselfmode",     PTP_DPC_NIKON_FlashCommandSelfMode,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommandSelfMode,    _put_Nikon_FlashCommandSelfMode },
	{ N_("Flash Command Self Compensation"), "flashcommandselfcompensation", PTP_DPC_NIKON_FlashCommandSelfCompensation, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_FlashCommandXCompensation, _put_Nikon_FlashCommandXCompensation },
	{ N_("Flash Command Self Value"),       "flashcommandselfvalue",    PTP_DPC_NIKON_FlashCommandSelfValue,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommandXValue,      _put_Nikon_FlashCommandXValue },
	{ N_("Flash Command A Mode"),           "flashcommandamode",        PTP_DPC_NIKON_FlashCommandAMode,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommandXMode,       _put_Nikon_FlashCommandXMode },
	{ N_("Flash Command A Compensation"),   "flashcommandacompensation", PTP_DPC_NIKON_FlashCommandACompensation, PTP_VENDOR_NIKON, PTP_DTC_UINT8,  _get_Nikon_FlashCommandXCompensation, _put_Nikon_FlashCommandXCompensation },
	{ N_("Flash Command A Value"),          "flashcommandavalue",       PTP_DPC_NIKON_FlashCommandAValue,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommandXValue,      _put_Nikon_FlashCommandXValue },
	{ N_("Flash Command B Mode"),           "flashcommandbmode",        PTP_DPC_NIKON_FlashCommandBMode,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommandXMode,       _put_Nikon_FlashCommandXMode },
	{ N_("Flash Command B Compensation"),   "flashcommandbcompensation", PTP_DPC_NIKON_FlashCommandBCompensation, PTP_VENDOR_NIKON, PTP_DTC_UINT8,  _get_Nikon_FlashCommandXCompensation, _put_Nikon_FlashCommandXCompensation },
	{ N_("Flash Command B Value"),          "flashcommandbvalue",       PTP_DPC_NIKON_FlashCommandBValue,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashCommandXValue,      _put_Nikon_FlashCommandXValue },
	{ N_("AF Area Illumination"),           "af-area-illumination",     PTP_DPC_NIKON_AFAreaIllumination,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AFAreaIllum,             _put_Nikon_AFAreaIllum },
	{ N_("AF Beep Mode"),                   "afbeep",                   PTP_DPC_NIKON_BeepOff,                  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("F-Number"),                       "f-number",                 PTP_DPC_FNumber,                        PTP_VENDOR_SONY,    PTP_DTC_UINT16, _get_FNumber,                       _put_Sony_FNumber },
	{ N_("F-Number"),                       "f-number",                 PTP_DPC_FNumber,                        0,                  PTP_DTC_UINT16, _get_FNumber,                       _put_FNumber },
	{ N_("Movie F-Number"),                 "movief-number",            PTP_DPC_NIKON_MovieFNumber,             PTP_VENDOR_NIKON,   PTP_DTC_UINT16, _get_FNumber,                       _put_FNumber },
	{ N_("Flexible Program"),               "flexibleprogram",          PTP_DPC_NIKON_FlexibleProgram,          PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Range_INT8,                    _put_Range_INT8 },
	{ N_("Image Quality"),                  "imagequality",             PTP_DPC_CompressionSetting,             PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_CompressionSetting,            _put_Sony_CompressionSetting },
	{ N_("Image Quality"),                  "imagequality",             PTP_DPC_CompressionSetting,             0,                  PTP_DTC_UINT8,  _get_CompressionSetting,            _put_CompressionSetting },
	{ N_("Focus Distance"),                 "focusdistance",            PTP_DPC_FocusDistance,                  0,                  PTP_DTC_UINT16, _get_FocusDistance,                 _put_FocusDistance },
	{ N_("Focal Length"),                   "focallength",              PTP_DPC_FocalLength,                    0,                  PTP_DTC_UINT32, _get_FocalLength,                   _put_FocalLength },
	{ N_("Focus Mode"),                     "focusmode",                PTP_DPC_FocusMode,                      0,                  PTP_DTC_UINT16, _get_FocusMode,                     _put_FocusMode },
	/* Nikon DSLR have both PTP focus mode and Nikon specific focus mode */
	{ N_("Focus Mode 2"),                   "focusmode2",               PTP_DPC_NIKON_AutofocusMode,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AFMode,                  _put_Nikon_AFMode },
	{ N_("Focus Mode"),                     "focusmode",                PTP_DPC_CANON_EOS_FocusMode,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_FocusMode,           _put_Canon_EOS_FocusMode },
	{ N_("Continuous AF"),                  "continuousaf",             PTP_DPC_CANON_EOS_ContinousAFMode,      PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_Canon_EOS_ContinousAF,         _put_Canon_EOS_ContinousAF },
	{ N_("Effect Mode"),                    "effectmode",               PTP_DPC_EffectMode,                     0,                  PTP_DTC_UINT16, _get_EffectMode,                    _put_EffectMode },
	{ N_("Effect Mode"),                    "effectmode",               PTP_DPC_NIKON_EffectMode,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_NIKON_EffectMode,              _put_NIKON_EffectMode },
	{ N_("Exposure Program"),               "expprogram",               PTP_DPC_ExposureProgramMode,            0,                  PTP_DTC_UINT16, _get_ExposureProgram,               _put_ExposureProgram },
	{ N_("Exposure Program"),               "expprogram2",              PTP_DPC_NIKON_1_Mode,                   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_NIKON_1_ExposureProgram,       _put_NIKON_1_ExposureProgram },
	{ N_("Scene Mode"),                     "scenemode",                PTP_DPC_NIKON_SceneMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_NIKON_SceneMode,               _put_NIKON_SceneMode },
	{ N_("Aspect Ratio"),                   "aspectratio",              PTP_DPC_SONY_AspectRatio,               PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_AspectRatio,              _put_Sony_AspectRatio },
	
	{ N_("HDR Mode"),                       "hdrmode",                  PTP_DPC_NIKON_HDRMode,                  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("HDR High Dynamic"),               "hdrhighdynamic",           PTP_DPC_NIKON_HDRHighDynamic,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_HDRHighDynamic,          _put_Nikon_HDRHighDynamic },
	{ N_("HDR Smoothing"),                  "hdrsmoothing",             PTP_DPC_NIKON_HDRSmoothing,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_HDRSmoothing,            _put_Nikon_HDRSmoothing },
	{ N_("Still Capture Mode"),             "capturemode",              PTP_DPC_StillCaptureMode,               0,                  PTP_DTC_UINT16, _get_CaptureMode,                   _put_CaptureMode },
	{ N_("Still Capture Mode"),             "capturemode",              PTP_DPC_FUJI_ReleaseMode,               PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_ReleaseMode,              _put_Fuji_ReleaseMode },
	{ N_("Canon Shooting Mode"),            "shootingmode",             PTP_DPC_CANON_ShootingMode,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ShootMode,               _put_Canon_ShootMode },
	{ N_("Canon Auto Exposure Mode"),       "autoexposuremode",         PTP_DPC_CANON_EOS_AutoExposureMode,     PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_AutoExposureMode,    _put_Canon_EOS_AutoExposureMode },
	{ N_("Drive Mode"),                     "drivemode",                PTP_DPC_CANON_EOS_DriveMode,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_DriveMode,           _put_Canon_EOS_DriveMode },
	{ N_("Picture Style"),                  "picturestyle",             PTP_DPC_CANON_EOS_PictureStyle,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_EOS_PictureStyle,        _put_Canon_EOS_PictureStyle },
	{ N_("Focus Metering Mode"),            "focusmetermode",           PTP_DPC_FocusMeteringMode,              0,                  PTP_DTC_UINT16, _get_FocusMetering,                 _put_FocusMetering },
	{ N_("Exposure Metering Mode"),         "exposuremetermode",        PTP_DPC_ExposureMeteringMode,           0,                  PTP_DTC_UINT16, _get_ExposureMetering,              _put_ExposureMetering },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_CANON_Aperture,                 PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_FUJI_Aperture,                  PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_Aperture,                 _put_Fuji_Aperture },
	{ N_("AV Open"),                        "avopen",                   PTP_DPC_CANON_AvOpen,                   PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("AV Max"),                         "avmax",                    PTP_DPC_CANON_AvMax,                    PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_CANON_EOS_Aperture,             PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("Focusing Point"),                 "focusingpoint",            PTP_DPC_CANON_FocusingPoint,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_FocusingPoint,           _put_Canon_FocusingPoint },
	{ N_("Sharpness"),                      "sharpness",                PTP_DPC_Sharpness,                      0,                  PTP_DTC_UINT8,  _get_Sharpness,                     _put_Sharpness },
	{ N_("Capture Delay"),                  "capturedelay",             PTP_DPC_CaptureDelay,                   0,                  PTP_DTC_UINT32, _get_Milliseconds,                  _put_Milliseconds },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_ExposureTime,                   0,                  PTP_DTC_UINT32, _get_ExpTime,                       _put_ExpTime },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_CANON_ShutterSpeed,             PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ShutterSpeed,            _put_Canon_ShutterSpeed },
	/* these cameras also have PTP_DPC_ExposureTime, avoid overlap */
	{ N_("Shutter Speed 2"),                "shutterspeed2",            PTP_DPC_NIKON_ExposureTime,             PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_ShutterSpeed,            _put_Nikon_ShutterSpeed },
	{ N_("Movie Shutter Speed 2"),          "movieshutterspeed",        PTP_DPC_NIKON_MovieShutterSpeed,        PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_ShutterSpeed,            _put_Nikon_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_CANON_EOS_ShutterSpeed,         PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ShutterSpeed,            _put_Canon_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_FUJI_ShutterSpeed,              PTP_VENDOR_FUJI,    PTP_DTC_INT16,  _get_Fuji_ShutterSpeed,             _put_Fuji_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_SONY_ShutterSpeed,              PTP_VENDOR_SONY,    PTP_DTC_INT32,  _get_Sony_ShutterSpeed,             _put_Sony_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_RICOH_ShutterSpeed,             PTP_VENDOR_PENTAX,  PTP_DTC_UINT64, _get_Ricoh_ShutterSpeed,            _put_Ricoh_ShutterSpeed },
	{ N_("Metering Mode"),                  "meteringmode",             PTP_DPC_CANON_MeteringMode,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_MeteringMode,            _put_Canon_MeteringMode },
	{ N_("Metering Mode"),                  "meteringmode",             PTP_DPC_CANON_EOS_MeteringMode,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_MeteringMode,            _put_Canon_MeteringMode },
	{ N_("AF Distance"),                    "afdistance",               PTP_DPC_CANON_AFDistance,               PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_AFDistance,              _put_Canon_AFDistance },
	{ N_("Focus Area Wrap"),                "focusareawrap",            PTP_DPC_NIKON_FocusAreaWrap,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Exposure Delay Mode"),            "exposuredelaymode",        PTP_DPC_NIKON_ExposureDelayMode,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Exposure Lock"),                  "exposurelock",             PTP_DPC_NIKON_AELockMode,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("AE-L/AF-L Mode"),                 "aelaflmode",               PTP_DPC_NIKON_AELAFLMode,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AELAFLMode,              _put_Nikon_AELAFLMode },
	{ N_("Live View AF Mode"),              "liveviewafmode",           PTP_DPC_NIKON_LiveViewAFArea,           PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_LiveViewAF,              _put_Nikon_LiveViewAF },
	{ N_("Live View AF Mode"),              "liveviewafmode",           PTP_DPC_NIKON_LiveViewAFArea,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LiveViewAF,              _put_Nikon_LiveViewAF },
	{ N_("Live View AF Focus"),             "liveviewaffocus",          PTP_DPC_NIKON_LiveViewAFFocus,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LiveViewAFFocus,         _put_Nikon_LiveViewAFFocus },
	{ N_("File Number Sequencing"),         "filenrsequencing",         PTP_DPC_NIKON_FileNumberSequence,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Flash Sign"),                     "flashsign",                PTP_DPC_NIKON_FlashSign,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Modelling Flash"),                "modelflash",               PTP_DPC_NIKON_E4ModelingFlash,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Viewfinder Grid"),                "viewfindergrid",           PTP_DPC_NIKON_GridDisplay,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Image Review"),                   "imagereview",              PTP_DPC_NIKON_ImageReview,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Image Rotation Flag"),            "imagerotationflag",        PTP_DPC_NIKON_ImageRotation,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Release without CF card"),        "nocfcardrelease",          PTP_DPC_NIKON_NoCFCard,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Flash Mode Manual Power"),        "flashmodemanualpower",     PTP_DPC_NIKON_FlashModeManualPower,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashModeManualPower,    _put_Nikon_FlashModeManualPower },
	{ N_("Auto Focus Area"),                "autofocusarea",            PTP_DPC_NIKON_AutofocusArea,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AutofocusArea,           _put_Nikon_AutofocusArea },
	{ N_("Flash Exposure Compensation"),    "flashexposurecompensation", PTP_DPC_NIKON_FlashExposureCompensation, PTP_VENDOR_NIKON, PTP_DTC_UINT8,  _get_Nikon_FlashExposureCompensation, _put_Nikon_FlashExposureCompensation },
	{ N_("Bracketing"),                     "bracketing",               PTP_DPC_NIKON_Bracketing,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Bracketing"),                     "bracketmode",              PTP_DPC_NIKON_E6ManualModeBracketing,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ManualBracketMode,       _put_Nikon_ManualBracketMode },
	{ N_("Bracket Mode"),                   "bracketmode",              PTP_DPC_CANON_EOS_BracketMode,          PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_INT,                           _put_None /*FIXME*/ },
	{ N_("EV Step"),                        "evstep",                   PTP_DPC_NIKON_EVStep,                   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_EVStep,                  _put_Nikon_EVStep },
	{ N_("Bracket Set"),                    "bracketset",               PTP_DPC_NIKON_BracketSet,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_BracketSet,              _put_Nikon_BracketSet },
	{ N_("Bracket Order"),                  "bracketorder",             PTP_DPC_NIKON_BracketOrder,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_BracketOrder,            _put_Nikon_BracketOrder },
	{ N_("Burst Number"),                   "burstnumber",              PTP_DPC_BurstNumber,                    0,                  PTP_DTC_UINT16, _get_BurstNumber,                   _put_BurstNumber },
	{ N_("Burst Interval"),                 "burstinterval",            PTP_DPC_BurstNumber,                    0,                  PTP_DTC_UINT16, _get_Milliseconds,                  _put_Milliseconds },
	{ N_("Maximum Shots"),                  "maximumshots",             PTP_DPC_NIKON_MaximumShots,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_INT,                           _put_None },

	/* Newer Nikons have UINT8 ranges */
	{ N_("Auto White Balance Bias"),        "autowhitebias",            PTP_DPC_NIKON_WhiteBalanceAutoBias,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Tungsten White Balance Bias"),    "tungstenwhitebias",        PTP_DPC_NIKON_WhiteBalanceTungstenBias, PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Fluorescent White Balance Bias"), "flourescentwhitebias",     PTP_DPC_NIKON_WhiteBalanceFluorescentBias, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Daylight White Balance Bias"),    "daylightwhitebias",        PTP_DPC_NIKON_WhiteBalanceDaylightBias, PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Flash White Balance Bias"),       "flashwhitebias",           PTP_DPC_NIKON_WhiteBalanceFlashBias,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Cloudy White Balance Bias"),      "cloudywhitebias",          PTP_DPC_NIKON_WhiteBalanceCloudyBias,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Shady White Balance Bias"),       "shadewhitebias",           PTP_DPC_NIKON_WhiteBalanceShadeBias,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	/* older Nikons have INT8 ranges */
	{ N_("Auto White Balance Bias"),        "autowhitebias",            PTP_DPC_NIKON_WhiteBalanceAutoBias,     PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_WBBias,                  _put_Nikon_WBBias },
	{ N_("Tungsten White Balance Bias"),    "tungstenwhitebias",        PTP_DPC_NIKON_WhiteBalanceTungstenBias, PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_WBBias,                  _put_Nikon_WBBias },
	{ N_("Fluorescent White Balance Bias"), "flourescentwhitebias",     PTP_DPC_NIKON_WhiteBalanceFluorescentBias, PTP_VENDOR_NIKON, PTP_DTC_INT8,  _get_Nikon_WBBias,                  _put_Nikon_WBBias },
	{ N_("Daylight White Balance Bias"),    "daylightwhitebias",        PTP_DPC_NIKON_WhiteBalanceDaylightBias, PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_WBBias,                  _put_Nikon_WBBias },
	{ N_("Flash White Balance Bias"),       "flashwhitebias",           PTP_DPC_NIKON_WhiteBalanceFlashBias,    PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_WBBias,                  _put_Nikon_WBBias },
	{ N_("Cloudy White Balance Bias"),      "cloudywhitebias",          PTP_DPC_NIKON_WhiteBalanceCloudyBias,   PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_WBBias,                  _put_Nikon_WBBias },
	{ N_("Shady White Balance Bias"),       "shadewhitebias",           PTP_DPC_NIKON_WhiteBalanceShadeBias,    PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_WBBias,                  _put_Nikon_WBBias },

	{ N_("White Balance Bias Preset Nr"),   "whitebiaspresetno",        PTP_DPC_NIKON_WhiteBalancePresetNo,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_WBBiasPreset,            _put_Nikon_WBBiasPreset },
	{ N_("White Balance Bias Preset 0"),    "whitebiaspreset0",         PTP_DPC_NIKON_WhiteBalancePresetVal0,   PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_WBBiasPresetVal,         _put_None },
	{ N_("White Balance Bias Preset 1"),    "whitebiaspreset1",         PTP_DPC_NIKON_WhiteBalancePresetVal1,   PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_WBBiasPresetVal,         _put_None },
	{ N_("White Balance Bias Preset 2"),    "whitebiaspreset2",         PTP_DPC_NIKON_WhiteBalancePresetVal2,   PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_WBBiasPresetVal,         _put_None },
	{ N_("White Balance Bias Preset 3"),    "whitebiaspreset3",         PTP_DPC_NIKON_WhiteBalancePresetVal3,   PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_WBBiasPresetVal,         _put_None },
	{ N_("White Balance Bias Preset 4"),    "whitebiaspreset4",         PTP_DPC_NIKON_WhiteBalancePresetVal4,   PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_WBBiasPresetVal,         _put_None },
	{ N_("Selftimer Delay"),                "selftimerdelay",           PTP_DPC_NIKON_SelfTimer,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_SelfTimerDelay,          _put_Nikon_SelfTimerDelay },
	{ N_("Center Weight Area"),             "centerweightsize",         PTP_DPC_NIKON_CenterWeightArea,         PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_CenterWeight,            _put_Nikon_CenterWeight },
	{ N_("Flash Shutter Speed"),            "flashshutterspeed",        PTP_DPC_NIKON_FlashShutterSpeed,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashShutterSpeed,       _put_Nikon_FlashShutterSpeed },
	{ N_("Remote Timeout"),                 "remotetimeout",            PTP_DPC_NIKON_RemoteTimeout,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_RemoteTimeout,           _put_Nikon_RemoteTimeout },
	{ N_("Remote Mode"),                    "remotemode",               PTP_DPC_NIKON_RemoteMode,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_RemoteMode,              _put_Nikon_RemoteMode },
	{ N_("Application Mode"),               "applicationmode",          PTP_DPC_NIKON_ApplicationMode,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ApplicationMode,         _put_Nikon_ApplicationMode },
	{ N_("Optimize Image"),                 "optimizeimage",            PTP_DPC_NIKON_OptimizeImage,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OptimizeImage,           _put_Nikon_OptimizeImage },
	{ N_("Sharpening"),                     "sharpening",               PTP_DPC_NIKON_ImageSharpening,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_Sharpening,              _put_Nikon_Sharpening },
	{ N_("Tone Compensation"),              "tonecompensation",         PTP_DPC_NIKON_ToneCompensation,         PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ToneCompensation,        _put_Nikon_ToneCompensation },
	{ N_("Saturation"),                     "saturation",               PTP_DPC_NIKON_Saturation,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_Saturation,              _put_Nikon_Saturation },
	{ N_("Hue Adjustment"),                 "hueadjustment",            PTP_DPC_NIKON_HueAdjustment,            PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_HueAdjustment,           _put_Nikon_HueAdjustment },
	{ N_("Auto Exposure Bracketing"),       "aeb",                      PTP_DPC_CANON_EOS_AEB,                  PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_AEB,                 _put_Canon_EOS_AEB },
	{ N_("Movie Sound"),                    "moviesound",               PTP_DPC_NIKON_MovVoice,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Manual Movie Setting"),           "manualmoviesetting",       PTP_DPC_NIKON_ManualMovieSetting,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Microphone"),                     "microphone",               PTP_DPC_NIKON_MovMicrophone,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_Microphone,              _put_Nikon_Microphone },
	{ N_("Reverse Indicators"),             "reverseindicators",        PTP_DPC_NIKON_IndicatorDisp,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Auto Distortion Control"),        "autodistortioncontrol",    PTP_DPC_NIKON_AutoDistortionControl,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Video Mode"),                     "videomode",                PTP_DPC_NIKON_VideoMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_VideoMode,               _put_Nikon_VideoMode },

	{ 0,0,0,0,0,0,0 },
};

/* Nikon camera specific values, as unfortunately the values are handled differently
 * A generic fallback for the "rest" of the Nikons is in the main menu.
 */
static struct submenu nikon_1_j3_camera_settings[] = {
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_NIKON_1_ISO,                    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_J3_ISO,            _put_Nikon_1_J3_ISO },
	{ 0,0,0,0,0,0,0 },
};
static struct submenu nikon_1_s1_camera_settings[] = {
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_NIKON_1_ISO,                    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_S1_ISO,            _put_Nikon_1_S1_ISO },
	{ 0,0,0,0,0,0,0 },
};
/* Nikon D90. Marcus Meissner <marcus@jet.franken.de> */
static struct submenu nikon_d90_camera_settings[] = {
	{ N_("Meter Off Time"),         "meterofftime",         PTP_DPC_NIKON_MeterOff,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_MeterOffTime,        _put_Nikon_D90_MeterOffTime },
	{ 0,0,0,0,0,0,0 },
};

/* Nikon D7000. Marcus Meissner <marcus@jet.franken.de> */
static struct submenu nikon_d7000_camera_settings[] = {
	{ N_("Assign Func Button"),     "funcbutton",           PTP_DPC_NIKON_F4AssignFuncButton,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7000_FuncButton,        _put_Nikon_D7000_FuncButton },
	{ N_("Assign Preview Button"),  "previewbutton",        PTP_DPC_NIKON_PreviewButton,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7000_FuncButton,        _put_Nikon_D7000_FuncButton },
	{ 0,0,0,0,0,0,0 },
};

/* Nikon D7100. Daniel Wagenaar <wagenadl@uc.edu> */
static struct submenu nikon_d7100_camera_settings[] = {
	{ N_("Assign Func Button"),     "funcbutton",           PTP_DPC_NIKON_F4AssignFuncButton,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7000_FuncButton,        _put_Nikon_D7000_FuncButton },
	{ N_("Assign Preview Button"),  "previewbutton",        PTP_DPC_NIKON_PreviewButton,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7000_FuncButton,        _put_Nikon_D7000_FuncButton },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu nikon_d40_capture_settings[] = {
	{ N_("Image Quality"),          "imagequality",         PTP_DPC_CompressionSetting,         PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D40_Compression,         _put_Nikon_D40_Compression },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu nikon_d5100_capture_settings[] = {
	{ N_("Movie Quality"),          "moviequality",         PTP_DPC_NIKON_MovScreenSize,        0,                  PTP_DTC_UINT8,  _get_Nikon_D5100_MovieQuality,      _put_Nikon_D5100_MovieQuality },
	{ N_("Exposure Program"),       "expprogram",           PTP_DPC_ExposureProgramMode,        0,                  PTP_DTC_UINT16, _get_NIKON_D5100_ExposureProgram,   _put_NIKON_D5100_ExposureProgram },
	{ N_("Minimum Shutter Speed"),  "minimumshutterspeed",  PTP_DPC_NIKON_PADVPMode,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_PADVPValue,          _put_Nikon_D90_PADVPValue },
	{ 0,0,0,0,0,0,0 },
};

/* Nikon D7100. Daniel Wagenaar <wagenadl@uc.edu> */
static struct submenu nikon_d7100_capture_settings[] = {
	{ N_("Movie Resolution"),               "moviequality",         PTP_DPC_NIKON_MovScreenSize,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_MovieQuality,      _put_Nikon_D7100_MovieQuality },
	{ N_("Movie Quality"),                  "moviequality2",        PTP_DPC_NIKON_MovQuality,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_MovieQuality2,     _put_Nikon_D7100_MovieQuality2 },
	{ N_("Exposure Program"),               "expprogram",           PTP_DPC_ExposureProgramMode,    0,                  PTP_DTC_UINT16, _get_NIKON_D7100_ExposureProgram,   _put_NIKON_D7100_ExposureProgram },
	{ N_("Minimum Shutter Speed"),          "minimumshutterspeed",  PTP_DPC_NIKON_PADVPMode,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_PADVPValue,        _put_Nikon_D7100_PADVPValue },
	{ N_("Continuous Shooting Speed Slow"), "shootingspeed",        PTP_DPC_NIKON_D1ShootingSpeed,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_ShootingSpeed,     _put_Nikon_D7100_ShootingSpeed },
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",       PTP_DPC_NIKON_ISOAutoHiLimit,   PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D7100_ISOAutoHiLimit,    _put_Nikon_D7100_ISOAutoHiLimit },
	{ N_("Flash Sync. Speed"),              "flashsyncspeed",       PTP_DPC_NIKON_FlashSyncSpeed,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_FlashSyncSpeed,    _put_Nikon_D7100_FlashSyncSpeed },
	{ N_("Focus Metering"),                 "focusmetering",        PTP_DPC_FocusMeteringMode,      PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_FocusMetering,     _put_Nikon_D7100_FocusMetering },
	{ 0,0,0,0,0,0,0 },
};
static struct submenu nikon_d90_capture_settings[] = {
	{ N_("Minimum Shutter Speed"),          "minimumshutterspeed",  PTP_DPC_NIKON_PADVPMode,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_PADVPValue,      _put_Nikon_D90_PADVPValue },
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",       PTP_DPC_NIKON_ISOAutoHiLimit,   PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_ISOAutoHiLimit,  _put_Nikon_D90_ISOAutoHiLimit },
	{ N_("Active D-Lighting"),              "dlighting",            PTP_DPC_NIKON_ActiveDLighting,  PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_ActiveDLighting, _put_Nikon_D90_ActiveDLighting },
	{ N_("High ISO Noise Reduction"),       "highisonr",            PTP_DPC_NIKON_NrHighISO,        PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_HighISONR,       _put_Nikon_D90_HighISONR },
	{ N_("Image Quality"),                  "imagequality",         PTP_DPC_CompressionSetting,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_Compression,     _put_Nikon_D90_Compression },
	{ N_("Continuous Shooting Speed Slow"), "shootingspeed",        PTP_DPC_NIKON_D1ShootingSpeed,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ShootingSpeed,   _put_Nikon_D90_ShootingSpeed },
	{ 0,0,0,0,0,0,0 },
};

/* One D3s reporter is Matthias Blaicher */
static struct submenu nikon_d3s_capture_settings[] = {
	{ N_("Minimum Shutter Speed"),          "minimumshutterspeed",      PTP_DPC_NIKON_PADVPMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_PADVPValue,              _put_Nikon_D3s_PADVPValue },
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",           PTP_DPC_NIKON_ISOAutoHiLimit,           PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D3s_ISOAutoHiLimit,          _put_Nikon_D3s_ISOAutoHiLimit },
	{ N_("Continuous Shooting Speed Slow"), "shootingspeed",            PTP_DPC_NIKON_D1ShootingSpeed,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_ShootingSpeed,           _put_Nikon_D3s_ShootingSpeed },
	{ N_("Continuous Shooting Speed High"), "shootingspeedhigh",        PTP_DPC_NIKON_ContinuousSpeedHigh,      PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_ShootingSpeedHigh,       _put_Nikon_D3s_ShootingSpeedHigh },
	{ N_("Flash Sync. Speed"),              "flashsyncspeed",           PTP_DPC_NIKON_FlashSyncSpeed,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_FlashSyncSpeed,          _put_Nikon_D3s_FlashSyncSpeed },
	{ N_("Flash Shutter Speed"),            "flashshutterspeed",        PTP_DPC_NIKON_FlashShutterSpeed,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_FlashShutterSpeed,       _put_Nikon_D3s_FlashShutterSpeed },
	{ N_("Image Quality"),                  "imagequality",             PTP_DPC_CompressionSetting,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_Compression,             _put_Nikon_D3s_Compression },
	{ N_("JPEG Compression Policy"),        "jpegcompressionpolicy",    PTP_DPC_NIKON_JPEG_Compression_Policy,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_JPEGCompressionPolicy,   _put_Nikon_D3s_JPEGCompressionPolicy },
	{ N_("AF-C Mode Priority"),             "afcmodepriority",          PTP_DPC_NIKON_A1AFCModePriority,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_AFCModePriority,         _put_Nikon_D3s_AFCModePriority },
	{ N_("AF-S Mode Priority"),             "afsmodepriority",          PTP_DPC_NIKON_A2AFSModePriority,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_AFSModePriority,         _put_Nikon_D3s_AFSModePriority },
	{ N_("AF Activation"),                  "afactivation",             PTP_DPC_NIKON_A4AFActivation,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_AFActivation,            _put_Nikon_D3s_AFActivation },
	{ N_("Dynamic AF Area"),                "dynamicafarea",            PTP_DPC_NIKON_DynamicAFArea,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_DynamicAFArea,           _put_Nikon_D3s_DynamicAFArea },
	{ N_("AF Lock On"),                     "aflockon",                 PTP_DPC_NIKON_AFLockOn,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_AFLockOn,                _put_Nikon_D3s_AFLockOn },
	{ N_("AF Area Point"),                  "afareapoint",              PTP_DPC_NIKON_AFAreaPoint,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_AFAreaPoint,             _put_Nikon_D3s_AFAreaPoint },
	{ N_("AF On Button"),                   "afonbutton",               PTP_DPC_NIKON_NormalAFOn,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_NormalAFOn,              _put_Nikon_D3s_NormalAFOn },

	/* same as D90 */
	{ N_("High ISO Noise Reduction"),       "highisonr",                PTP_DPC_NIKON_NrHighISO,                PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_HighISONR,               _put_Nikon_D90_HighISONR },
	{ N_("Active D-Lighting"),              "dlighting",                PTP_DPC_NIKON_ActiveDLighting,          PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_ActiveDLighting,         _put_Nikon_D90_ActiveDLighting },

	{ 0,0,0,0,0,0,0 },
};

static struct submenu nikon_generic_capture_settings[] = {
	/* filled in with D90 values */
	{ N_("Minimum Shutter Speed"),          "minimumshutterspeed",      PTP_DPC_NIKON_PADVPMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_PADVPValue,          _put_Nikon_D90_PADVPValue },
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",           PTP_DPC_NIKON_ISOAutoHiLimit,           PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_ISOAutoHiLimit,      _put_Nikon_D90_ISOAutoHiLimit },
	{ N_("Active D-Lighting"),              "dlighting",                PTP_DPC_NIKON_ActiveDLighting,          PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_ActiveDLighting,     _put_Nikon_D90_ActiveDLighting },
	{ N_("High ISO Noise Reduction"),       "highisonr",                PTP_DPC_NIKON_NrHighISO,                PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_HighISONR,           _put_Nikon_D90_HighISONR },
	{ N_("Movie High ISO Noise Reduction"), "moviehighisonr",           PTP_DPC_NIKON_MovieNrHighISO,           PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_D90_HighISONR,           _put_Nikon_D90_HighISONR },
	{ N_("Continuous Shooting Speed Slow"), "shootingspeed",            PTP_DPC_NIKON_D1ShootingSpeed,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ShootingSpeed,       _put_Nikon_D90_ShootingSpeed },
	{ N_("Maximum continuous release"),     "maximumcontinousrelease",  PTP_DPC_NIKON_D2MaximumShots,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Range_UINT8,                   _put_Range_UINT8 },
	{ N_("Movie Quality"),                  "moviequality",             PTP_DPC_NIKON_MovScreenSize,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_MovieQuality,            _put_Nikon_MovieQuality },
	{ N_("Movie Quality"),                  "moviequality",             PTP_DPC_NIKON_1_MovQuality,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_MovieQuality,          _put_Nikon_1_MovieQuality },
	{ N_("High ISO Noise Reduction"),       "highisonr",                PTP_DPC_NIKON_1_HiISONoiseReduction,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_HighISONR,             _put_Nikon_1_HighISONR },

	{ N_("Raw Compression"),                "rawcompression",           PTP_DPC_NIKON_RawCompression,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_RawCompression,          _put_Nikon_RawCompression },

	{ N_("Image Quality 2"),                "imagequality2",            PTP_DPC_NIKON_1_ImageCompression,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_Compression,           _put_Nikon_1_Compression },

	/* And some D3s values */
	{ N_("Continuous Shooting Speed High"), "shootingspeedhigh",        PTP_DPC_NIKON_ContinuousSpeedHigh,      PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_ShootingSpeedHigh,   _put_Nikon_D3s_ShootingSpeedHigh },
	{ N_("Flash Sync. Speed"),              "flashsyncspeed",           PTP_DPC_NIKON_FlashSyncSpeed,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_FlashSyncSpeed,      _put_Nikon_D3s_FlashSyncSpeed },
	{ N_("Flash Shutter Speed"),            "flashshutterspeed",        PTP_DPC_NIKON_FlashShutterSpeed,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_FlashShutterSpeed,   _put_Nikon_D3s_FlashShutterSpeed },

	{ 0,0,0,0,0,0,0 },
};


static struct menu menus[] = {
	{ N_("Camera Actions"),             "actions",          0,      0,      camera_actions_menu,            NULL,   NULL },

	{ N_("Camera Settings"),            "settings",         0x4b0,  0x0428, nikon_d7000_camera_settings,    NULL,   NULL },
	{ N_("Camera Settings"),            "settings",         0x4b0,  0x0430, nikon_d7100_camera_settings,    NULL,   NULL },
	{ N_("Camera Settings"),            "settings",         0x4b0,  0x0421, nikon_d90_camera_settings,      NULL,   NULL },
	{ N_("Camera Settings"),            "settings",         0x4b0,  0x0606, nikon_1_s1_camera_settings,     NULL,   NULL },
	{ N_("Camera Settings"),            "settings",         0x4b0,  0x0605, nikon_1_j3_camera_settings,     NULL,   NULL },
	{ N_("Camera Settings"),            "settings",         0,      0,      camera_settings_menu,           NULL,   NULL },

	{ N_("Camera Status Information"),  "status",           0,      0,      camera_status_menu,             NULL,   NULL },
	{ N_("Image Settings"),             "imgsettings",      0,      0,      image_settings_menu,            NULL,   NULL },


	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0421, nikon_d90_capture_settings,     NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0426, nikon_d3s_capture_settings,     NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0429, nikon_d5100_capture_settings,   NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0430, nikon_d7100_capture_settings,   NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0414, nikon_d40_capture_settings,     NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0,      nikon_generic_capture_settings, NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0,      0,      capture_settings_menu,          NULL,   NULL },

	{ N_("WIFI profiles"),              "wifiprofiles",     0,      0,      NULL,                           _get_wifi_profiles_menu, _put_wifi_profiles_menu },
};

/*
 * Can do 3 things:
 * - get the whole widget dialog tree (confname = NULL, list = NULL, widget = rootwidget)
 * - get the named single widget  (confname = the specified property, widget = property widget, list = NULL)
 * - get a flat ascii list of configuration names (confname = NULL, widget = NULL, list = list to fill in)
 */
static int
_get_config (Camera *camera, const char *confname, CameraWidget **outwidget, CameraList *list, GPContext *context)
{
	CameraWidget	*section, *widget, *window;
	unsigned int	menuno, submenuno;
	int 		ret;
	uint16_t	*setprops = NULL;
	int		i, nrofsetprops = 0;
	PTPParams	*params = &camera->pl->params;
	CameraAbilities	ab;

	enum {
		MODE_GET,
		MODE_LIST,
		MODE_SINGLE_GET
	} mode = MODE_GET;

	if (confname)
		mode = MODE_SINGLE_GET;
	if (list) {
		gp_list_reset (list);
		mode = MODE_LIST;
	}

	SET_CONTEXT(camera, context);
	memset (&ab, 0, sizeof(ab));
	gp_camera_get_abilities (camera, &ab);
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_RemoteRelease)
	) {
		if (!params->eos_captureenabled)
			camera_prepare_capture (camera, context);
		ptp_check_eos_events (params);
		/* Otherwise the camera will auto-shutdown */
		C_PTP (ptp_canon_eos_keepdeviceon (params));
	}

	if (mode == MODE_GET) {
		gp_widget_new (GP_WIDGET_WINDOW, _("Camera and Driver Configuration"), &window);
		gp_widget_set_name (window, "main");
		*outwidget = window;
	}

	for (menuno = 0; menuno < sizeof(menus)/sizeof(menus[0]) ; menuno++ ) {
		if (!menus[menuno].submenus) { /* Custom menu */
			if (mode == MODE_GET) {
				struct menu *cur = menus+menuno;
				ret = cur->getfunc(camera, &section, cur);
				if (ret == GP_OK)
					gp_widget_append(window, section);
			} /* else ... not supported in single get and list */
			continue;
		}
		if ((menus[menuno].usb_vendorid != 0) && (ab.port == GP_PORT_USB)) {
			if (menus[menuno].usb_vendorid != ab.usb_vendor)
				continue;
			if (	menus[menuno].usb_productid &&
				(menus[menuno].usb_productid != ab.usb_product)
			)
				continue;
			GP_LOG_D ("usb vendor/product specific path entered");
		}

		if (mode == MODE_GET) {
			/* Standard menu with submenus */
			ret = gp_widget_get_child_by_label (window, _(menus[menuno].label), &section);
			if (ret != GP_OK) {
				gp_widget_new (GP_WIDGET_SECTION, _(menus[menuno].label), &section);
				gp_widget_set_name (section, menus[menuno].name);
				gp_widget_append (window, section);
			}
		}
		for (submenuno = 0; menus[menuno].submenus[submenuno].name ; submenuno++ ) {
			struct submenu *cursub = menus[menuno].submenus+submenuno;
			widget = NULL;

			if (	have_prop(camera,cursub->vendorid,cursub->propid) ||
				((cursub->propid == 0) && have_prop(camera,cursub->vendorid,cursub->type))
			) {
				int			j;

				/* Do not handle a property we have already handled.
				 * needed for the vendor specific but different configs.
				 */
				if (cursub->propid) {
					for (j=0;j<nrofsetprops;j++)
						if (setprops[j] == cursub->propid)
							break;
					if (j<nrofsetprops) {
						GP_LOG_D ("Property '%s' / 0x%04x already handled before, skipping.", cursub->label, cursub->propid );
						continue;
					}
					if (nrofsetprops)
						C_MEM (setprops = realloc(setprops,sizeof(setprops[0])*(nrofsetprops+1)));
					else
						C_MEM (setprops = malloc(sizeof(setprops[0])));
					setprops[nrofsetprops++] = cursub->propid;
				}
				/* ok, looking good */
				if (	((cursub->propid & 0x7000) == 0x5000) ||
					(NIKON_1(params) && ((cursub->propid & 0xf000) == 0xf000))
				) {
					PTPDevicePropDesc	dpd;

					if ((mode == MODE_SINGLE_GET) && strcmp (cursub->name, confname))
						continue;
					if (mode == MODE_LIST) {
						gp_list_append (list, cursub->name, NULL);
						continue;
					}

					GP_LOG_D ("Getting property '%s' / 0x%04x", cursub->label, cursub->propid );
					memset(&dpd,0,sizeof(dpd));
					C_PTP (ptp_generic_getdevicepropdesc(params,cursub->propid,&dpd));
					ret = cursub->getfunc (camera, &widget, cursub, &dpd);
					if ((ret == GP_OK) && (dpd.GetSet == PTP_DPGS_Get))
						gp_widget_set_readonly (widget, 1);
					ptp_free_devicepropdesc(&dpd);
					if (mode == MODE_SINGLE_GET) {
						*outwidget = widget;
						free (setprops);
						return GP_OK;
					}
				} else {
					/* if it is a OPC, check for its presence. Otherwise just create the widget. */
					if (	((cursub->type & 0x7000) != 0x1000) ||
						 ptp_operation_issupported(params, cursub->type)
					) {
						if ((mode == MODE_SINGLE_GET) && strcmp (cursub->name, confname))
							continue;
						if (mode == MODE_LIST) {
							gp_list_append (list, cursub->name, NULL);
							continue;
						}

						GP_LOG_D ("Getting function prop '%s' / 0x%04x", cursub->label, cursub->type );
						ret = cursub->getfunc (camera, &widget, cursub, NULL);
						if (mode == MODE_SINGLE_GET) {
							*outwidget = widget;
							free (setprops);
							return GP_OK;
						}
					} else
						continue;
				}
				if (ret != GP_OK) {
					GP_LOG_D ("Failed to parse value of property '%s' / 0x%04x: error code %d", cursub->label, cursub->propid, ret);
					continue;
				}
				if (mode == MODE_GET)
					gp_widget_append (section, widget);
				continue;
			}
			if (have_eos_prop(camera,cursub->vendorid,cursub->propid)) {
				PTPDevicePropDesc	dpd;

				if ((mode == MODE_SINGLE_GET) && strcmp (cursub->name, confname))
					continue;
				if (mode == MODE_LIST) {
					gp_list_append (list, cursub->name, NULL);
					continue;
				}
				GP_LOG_D ("Getting property '%s' / 0x%04x", cursub->label, cursub->propid );
				memset(&dpd,0,sizeof(dpd));
				ptp_canon_eos_getdevicepropdesc (params,cursub->propid, &dpd);
				ret = cursub->getfunc (camera, &widget, cursub, &dpd);
				ptp_free_devicepropdesc(&dpd);
				if (ret != GP_OK) {
					GP_LOG_D ("Failed to parse value of property '%s' / 0x%04x: error code %d", cursub->label, cursub->propid, ret);
					continue;
				}
				if (mode == MODE_SINGLE_GET) {
					*outwidget = widget;
					free (setprops);
					return GP_OK;
				}
				if (mode == MODE_GET)
					gp_widget_append (section, widget);
				continue;
			}
		}
	}

	if (!params->deviceinfo.DevicePropertiesSupported_len) {
		free (setprops);
		return GP_OK;
	}

	if (mode == MODE_GET) {
		/* Last menu is "Other", a generic property fallback window. */
		gp_widget_new (GP_WIDGET_SECTION, _("Other PTP Device Properties"), &section);
		gp_widget_set_name (section, "other");
		gp_widget_append (window, section);
	}

	for (i=0;i<params->deviceinfo.DevicePropertiesSupported_len;i++) {
		uint16_t		propid = params->deviceinfo.DevicePropertiesSupported[i];
		char			buf[20], *label;
		PTPDevicePropDesc	dpd;
		CameraWidgetType	type;

#if 0 /* enable this for suppression of generic properties for already decoded ones */
		int j;

		for (j=0;j<nrofsetprops;j++)
			if (setprops[j] == propid)
				break;
		if (j<nrofsetprops) {
			GP_LOG_D ("Property 0x%04x already handled before, skipping.", propid );
			continue;
		}
#endif

		sprintf(buf,"%04x", propid);
		if ((mode == MODE_SINGLE_GET) && strcmp (buf, confname))
			continue;
		if (mode == MODE_LIST) {
			gp_list_append (list, buf, NULL);
			continue;
		}

		ret = ptp_generic_getdevicepropdesc (params,propid,&dpd);
		if (ret != PTP_RC_OK)
			continue;

		label = (char*)ptp_get_property_description(params, propid);
		if (!label) {
			sprintf (buf, N_("PTP Property 0x%04x"), propid);
			label = buf;
		}
		switch (dpd.FormFlag) {
		case PTP_DPFF_None:
			type = GP_WIDGET_TEXT;
			break;
		case PTP_DPFF_Range:
			type = GP_WIDGET_RANGE;
			switch (dpd.DataType) {
			/* simple ranges might just be enumerations */
#define X(dtc,val) 							\
			case dtc: 					\
				if (	((dpd.FORM.Range.MaximumValue.val - dpd.FORM.Range.MinimumValue.val) < 128) &&	\
					(dpd.FORM.Range.StepSize.val == 1)) {						\
					type = GP_WIDGET_MENU;								\
				} \
				break;

		X(PTP_DTC_INT8,i8)
		X(PTP_DTC_UINT8,u8)
		X(PTP_DTC_INT16,i16)
		X(PTP_DTC_UINT16,u16)
		X(PTP_DTC_INT32,i32)
		X(PTP_DTC_UINT32,u32)
#undef X
			default:break;
			}
			break;
		case PTP_DPFF_Enumeration:
			type = GP_WIDGET_MENU;
			break;
		default:
			type = GP_WIDGET_TEXT;
			break;
		}
		gp_widget_new (type, _(label), &widget);
		sprintf(buf,"%04x", propid);
		gp_widget_set_name (widget, buf);
		switch (dpd.FormFlag) {
		case PTP_DPFF_None: break;
		case PTP_DPFF_Range:
			switch (dpd.DataType) {
#define X(dtc,val) 										\
			case dtc: 								\
				if (type == GP_WIDGET_RANGE) {					\
					gp_widget_set_range ( widget, (float) dpd.FORM.Range.MinimumValue.val, (float) dpd.FORM.Range.MaximumValue.val, (float) dpd.FORM.Range.StepSize.val);\
				} else {							\
					long k;							\
					for (k=dpd.FORM.Range.MinimumValue.val;k<=dpd.FORM.Range.MaximumValue.val;k+=dpd.FORM.Range.StepSize.val) { \
						sprintf (buf, "%ld", k); 			\
						gp_widget_add_choice (widget, buf);		\
					}							\
				} 								\
				break;

		X(PTP_DTC_INT8,i8)
		X(PTP_DTC_UINT8,u8)
		X(PTP_DTC_INT16,i16)
		X(PTP_DTC_UINT16,u16)
		X(PTP_DTC_INT32,i32)
		X(PTP_DTC_UINT32,u32)
		X(PTP_DTC_INT64,i64)
		X(PTP_DTC_UINT64,u64)
#undef X
			default:break;
			}
			break;
		case PTP_DPFF_Enumeration:
			switch (dpd.DataType) {
#define X(dtc,val,fmt) 									\
			case dtc: { 							\
				int k;							\
				for (k=0;k<dpd.FORM.Enum.NumberOfValues;k++) {		\
					sprintf (buf, fmt, dpd.FORM.Enum.SupportedValue[k].val); \
					gp_widget_add_choice (widget, buf);		\
				}							\
				break;							\
			}

		X(PTP_DTC_INT8,i8,"%d")
		X(PTP_DTC_UINT8,u8,"%d")
		X(PTP_DTC_INT16,i16,"%d")
		X(PTP_DTC_UINT16,u16,"%d")
		X(PTP_DTC_INT32,i32,"%d")
		X(PTP_DTC_UINT32,u32,"%d")
		X(PTP_DTC_INT64,i64,"%ld")
		X(PTP_DTC_UINT64,u64,"%ld")
#undef X
			case PTP_DTC_STR: {
				int k;
				for (k=0;k<dpd.FORM.Enum.NumberOfValues;k++)
					gp_widget_add_choice (widget, dpd.FORM.Enum.SupportedValue[k].str);
				break;
			}
			default:break;
			}
			break;
		}
		switch (dpd.DataType) {
#define X(dtc,val,fmt) 							\
		case dtc:						\
			if (type == GP_WIDGET_RANGE) {			\
				float f = dpd.CurrentValue.val;		\
				gp_widget_set_value (widget, &f);	\
			} else {					\
				sprintf (buf, fmt, dpd.CurrentValue.val);	\
				gp_widget_set_value (widget, buf);	\
			}\
			break;

		X(PTP_DTC_INT8,i8,"%d")
		X(PTP_DTC_UINT8,u8,"%d")
		X(PTP_DTC_INT16,i16,"%d")
		X(PTP_DTC_UINT16,u16,"%d")
		X(PTP_DTC_INT32,i32,"%d")
		X(PTP_DTC_UINT32,u32,"%d")
		X(PTP_DTC_INT64,i64,"%ld")
		X(PTP_DTC_UINT64,u64,"%ld")
#undef X
		case PTP_DTC_STR:
			gp_widget_set_value (widget, dpd.CurrentValue.str);
			break;
		default:
			break;
		}
		if (dpd.GetSet == PTP_DPGS_Get)
			gp_widget_set_readonly (widget, 1);
		ptp_free_devicepropdesc(&dpd);
		if (mode == MODE_GET)
			gp_widget_append (section, widget);
		if (mode == MODE_SINGLE_GET) {
			*outwidget = widget;
			free (setprops);
			return GP_OK;
		}
	}
	free (setprops);
	if (mode == MODE_SINGLE_GET) {
		/* if we get here, we have not found anything */
		gp_context_error (context, _("Property '%s' not found."), confname);
		return GP_ERROR_BAD_PARAMETERS;
	}
	return GP_OK;
} 

int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	return _get_config (camera, NULL, window, NULL, context);
}

int
camera_list_config (Camera *camera, CameraList *list, GPContext *context)
{
	return _get_config (camera, NULL, NULL, list, context);
}

int
camera_get_single_config (Camera *camera, const char *confname, CameraWidget **widget, GPContext *context)
{
	return _get_config (camera, confname, widget, NULL, context);
}


static int
_set_config (Camera *camera, const char *confname, CameraWidget *window, GPContext *context)
{
	CameraWidget		*section, *widget = window, *subwindow;
	uint16_t		ret_ptp;
	unsigned int		menuno, submenuno;
	int			ret;
	PTPParams		*params = &camera->pl->params;
	PTPPropertyValue	propval;
	unsigned int		i;
	CameraAbilities		ab;
	enum {
		MODE_SET, MODE_SINGLE_SET
	} mode = MODE_SET;

	if (confname) mode = MODE_SINGLE_SET;

	SET_CONTEXT(camera, context);
	memset (&ab, 0, sizeof(ab));
	gp_camera_get_abilities (camera, &ab);

	camera->pl->checkevents = TRUE;
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_RemoteRelease)
	) {
		if (!params->eos_captureenabled)
			camera_prepare_capture (camera, context);
		ptp_check_eos_events (params);
	}

	if (mode == MODE_SET)
		CR (gp_widget_get_child_by_label (window, _("Camera and Driver Configuration"), &subwindow));
	for (menuno = 0; menuno < sizeof(menus)/sizeof(menus[0]) ; menuno++ ) {
		if (mode == MODE_SET) {
			ret = gp_widget_get_child_by_label (subwindow, _(menus[menuno].label), &section);
			if (ret != GP_OK)
				continue;
		}

		if (!menus[menuno].submenus) { /* Custom menu */
			if (mode == MODE_SET)
				menus[menuno].putfunc(camera, section);
			continue;
		}
		if ((menus[menuno].usb_vendorid != 0) && (ab.port == GP_PORT_USB)) {
			if (menus[menuno].usb_vendorid != ab.usb_vendor)
				continue;
			if (	menus[menuno].usb_productid &&
				(menus[menuno].usb_productid != ab.usb_product)
			)
				continue;
			GP_LOG_D ("usb vendor/product specific path entered");
		}

		/* Standard menu with submenus */
		for (submenuno = 0; menus[menuno].submenus[submenuno].label ; submenuno++ ) {
			struct submenu *cursub = menus[menuno].submenus+submenuno;

			ret = GP_OK;
			if (mode == MODE_SET) {
				ret = gp_widget_get_child_by_label (section, _(cursub->label), &widget);
				if (ret != GP_OK)
					continue;

				if (!gp_widget_changed (widget))
					continue;

				/* restore the "changed flag" */
				gp_widget_set_changed (widget, TRUE);
			}

			if (	have_prop(camera,cursub->vendorid,cursub->propid) ||
				((cursub->propid == 0) && have_prop(camera,cursub->vendorid,cursub->type))
			) {
				if ((mode == MODE_SINGLE_SET) && strcmp (confname, cursub->name))
					continue;

				gp_widget_changed (widget); /* clear flag */
				GP_LOG_D ("Setting property '%s' / 0x%04x", cursub->label, cursub->propid );
				if (	((cursub->propid & 0x7000) == 0x5000) ||
					(NIKON_1(params) && ((cursub->propid & 0xf000) == 0xf000))
				){
					PTPDevicePropDesc dpd;

					memset(&dpd,0,sizeof(dpd));
					ptp_generic_getdevicepropdesc(params,cursub->propid,&dpd);
					if (dpd.GetSet == PTP_DPGS_GetSet) {
						ret = cursub->putfunc (camera, widget, &propval, &dpd);
					} else {
						gp_context_error (context, _("Sorry, the property '%s' / 0x%04x is currently ready-only."), _(cursub->label), cursub->propid);
						ret = GP_ERROR_NOT_SUPPORTED;
					}
					if (ret == GP_OK) {
						ret_ptp = LOG_ON_PTP_E (ptp_generic_setdevicepropvalue (params, cursub->propid, &propval, cursub->type));
						if (ret_ptp != PTP_RC_OK) {
							gp_context_error (context, _("The property '%s' / 0x%04x was not set (0x%04x: %s)"),
									  _(cursub->label), cursub->propid, ret_ptp, _(ptp_strerror(ret_ptp, params->deviceinfo.VendorExtensionID)));
							ret = translate_ptp_result (ret_ptp);
						}
					}
					ptp_free_devicepropvalue (cursub->type, &propval);
					ptp_free_devicepropdesc(&dpd);
				} else {
					ret = cursub->putfunc (camera, widget, NULL, NULL);
				}
				if (mode == MODE_SINGLE_SET)
					return GP_OK;
			}
			if (have_eos_prop(camera,cursub->vendorid,cursub->propid)) {
				PTPDevicePropDesc	dpd;

				if ((mode == MODE_SINGLE_SET) && strcmp (confname, cursub->name))
					continue;
				gp_widget_changed (widget); /* clear flag */
				GP_LOG_D ("Setting property '%s' / 0x%04x", cursub->label, cursub->propid);
				memset(&dpd,0,sizeof(dpd));
				ptp_canon_eos_getdevicepropdesc (params,cursub->propid, &dpd);
				ret = cursub->putfunc (camera, widget, &propval, &dpd);
				if (ret == GP_OK) {
					ret_ptp = LOG_ON_PTP_E (ptp_canon_eos_setdevicepropvalue (params, cursub->propid, &propval, cursub->type));
					if (ret_ptp != PTP_RC_OK) {
						gp_context_error (context, _("The property '%s' / 0x%04x was not set (0x%04x: %s)."),
								  _(cursub->label), cursub->propid, ret_ptp, _(ptp_strerror(ret_ptp, params->deviceinfo.VendorExtensionID)));
						ret = translate_ptp_result (ret_ptp);
					}
				} else
					gp_context_error (context, _("Parsing the value of widget '%s' / 0x%04x failed with %d."), _(cursub->label), cursub->propid, ret);
				ptp_free_devicepropdesc(&dpd);
				ptp_free_devicepropvalue(cursub->type, &propval);
				if (mode == MODE_SINGLE_SET)
					return GP_OK;
			}
			if (ret != GP_OK)
				return ret;
		}
	}
	if (!params->deviceinfo.DevicePropertiesSupported_len)
		return GP_OK;

	if (mode == MODE_SET)
		CR (gp_widget_get_child_by_label (subwindow, _("Other PTP Device Properties"), &section));
	/* Generic property setter */
	for (i=0;i<params->deviceinfo.DevicePropertiesSupported_len;i++) {
		uint16_t		propid = params->deviceinfo.DevicePropertiesSupported[i];
		CameraWidgetType	type;
		char			buf[20], *label, *xval;
		PTPDevicePropDesc	dpd;

		if (mode == MODE_SINGLE_SET) {
			sprintf(buf,"%04x", propid);
			if (strcmp (confname, buf))
				continue;
		}
		label = (char*)ptp_get_property_description(params, propid);
		if (!label) {
			sprintf (buf, N_("PTP Property 0x%04x"), propid);
			label = buf;
		}
		if (mode == MODE_SET) {
			ret = gp_widget_get_child_by_label (section, _(label), &widget);
			if (ret != GP_OK)
				continue;
			if (!gp_widget_changed (widget))
				continue;
		}

		gp_widget_get_type (widget, &type);

		memset (&dpd,0,sizeof(dpd));
		memset (&propval,0,sizeof(propval));
		ret = ptp_getdevicepropdesc (params,propid,&dpd);
		if (ret != PTP_RC_OK)
			continue;
		if (dpd.GetSet != PTP_DPGS_GetSet) {
			gp_context_error (context, _("Sorry, the property '%s' / 0x%04x is currently ready-only."), _(label), propid);
			return GP_ERROR_NOT_SUPPORTED;
		}

		switch (dpd.DataType) {
#define X(dtc,val) 							\
		case dtc:						\
			if (type == GP_WIDGET_RANGE) {			\
				float f;				\
				gp_widget_get_value (widget, &f);	\
				propval.val = f;			\
			} else {					\
				long x;					\
				ret = gp_widget_get_value (widget, &xval);	\
				sscanf (xval, "%ld", &x);		\
				propval.val = x;			\
			}\
			break;

		X(PTP_DTC_INT8,i8)
		X(PTP_DTC_UINT8,u8)
		X(PTP_DTC_INT16,i16)
		X(PTP_DTC_UINT16,u16)
		X(PTP_DTC_INT32,i32)
		X(PTP_DTC_UINT32,u32)
		X(PTP_DTC_INT64,i64)
		X(PTP_DTC_UINT64,u64)
#undef X
		case PTP_DTC_STR: {
			char *val;
			gp_widget_get_value (widget, &val);
			C_MEM (propval.str = strdup(val));
			break;
		}
		default:
			break;
		}

		/* TODO: ret is ignored here, this is inconsistent with the code above */
		ret = LOG_ON_PTP_E (ptp_generic_setdevicepropvalue (params, propid, &propval, dpd.DataType));
		if (ret != PTP_RC_OK) {
			gp_context_error (context, _("The property '%s' / 0x%04x was not set (0x%04x: %s)."),
					  _(label), propid, ret, _(ptp_strerror(ret, params->deviceinfo.VendorExtensionID)));
			ret = GP_ERROR;
		}
		ptp_free_devicepropvalue (dpd.DataType, &propval);
		ptp_free_devicepropdesc (&dpd);
		if (mode == MODE_SINGLE_SET)
			return GP_OK;
	}
	if (mode == MODE_SINGLE_SET) {
		/* if we get here, we have not found anything */
		gp_context_error (context, _("Property '%s' not found."), confname);
		return GP_ERROR_BAD_PARAMETERS;
	}
	return GP_OK;
}

int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	return _set_config (camera, NULL, window, context);
}

int
camera_set_single_config (Camera *camera, const char *confname, CameraWidget *widget, GPContext *context)
{
	return _set_config (camera, confname, widget, context);
}
