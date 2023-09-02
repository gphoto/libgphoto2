/* config.c
 *
 * Copyright (C) 2003-2021 Marcus Meissner <marcus@jet.franken.de>
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
#define _DEFAULT_SOURCE
#include "config.h"

#include <math.h>
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
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-setting.h>

#include "libgphoto2/i18n.h"

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
	if (!have_eos_prop(params, PTP_VENDOR_CANON, PTP_DPC_CANON_EOS_CaptureDestination) ) {
		GP_LOG_D ("No CaptureDestination property?");
		return GP_OK;
	}
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
			/* https://github.com/gphoto/gphoto2/issues/383 ... we dont get an enum, but the value is 2 ... */
			if (dpd.CurrentValue.u32 != PTP_CANON_EOS_CAPTUREDEST_HD) {
				cardval = dpd.CurrentValue.u32;
			} else {
				GP_LOG_D ("NO Card found - falling back to SDRAM!");
				cardval = PTP_CANON_EOS_CAPTUREDEST_HD;
			}
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
#if 0
			int		uilocked = params->uilocked;

			/* if we want to download the image from the device, we need to tell the camera
			 * that we have enough space left. */
			/* this might be a trigger value for "no space" -Marcus
			ret = ptp_canon_eos_pchddcapacity(params, 0x7fffffff, 0x00001000, 0x00000001);
			 */

			if (!uilocked)
				LOG_ON_PTP_E (ptp_canon_eos_setuilock (params));
#endif
			ret = ptp_canon_eos_pchddcapacity(params, 0x0fffffff, 0x00001000, 0x00000001);
#if 0
			if (!uilocked)
				LOG_ON_PTP_E (ptp_canon_eos_resetuilock (params));
#endif
			/* not so bad if its just busy, would also fail later. */
			if (ret == PTP_RC_DeviceBusy) ret = PTP_RC_OK;
			C_PTP (ret);
			/* Tricky ... eos1200d seems to take a while to register this change and the first capture
			 * when it is still switching might be going down the drain. */
			while (1) {
				C_PTP (ptp_check_eos_events (params));
				C_PTP (ptp_canon_eos_getdevicepropdesc (params,PTP_DPC_CANON_EOS_AvailableShots, &dpd));
				if (dpd.CurrentValue.u32 > 0)
					break;
			}
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
	int		tries;

	GP_LOG_D ("preparing EOS capture...");

	if (is_canon_eos_m(params)) {
		int mode = 0x15;	/* default for EOS M and newer Powershot SX */

		if (!strcmp(params->deviceinfo.Model,"Canon PowerShot SX540 HS")) mode = 0x11;	/* testing for https://github.com/gphoto/libgphoto2/issues/360 */
		if (!strcmp(params->deviceinfo.Model,"Canon PowerShot SX600 HS")) goto skip;

		if (!strcmp(params->deviceinfo.Model,"Canon PowerShot G5 X")) mode = 0x11;
		if (!strcmp(params->deviceinfo.Model,"Canon EOS M6 Mark II")) mode = 0x1;
		C_PTP (ptp_canon_eos_setremotemode(params, mode));
	} else {
		C_PTP (ptp_canon_eos_setremotemode(params, 1));
	}
skip:
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

	/* The new EOS occasionally returned an empty event set ... likely because we are too fast. try again some times. */
	C_PTP (ptp_check_eos_events (params));
	tries = 10;
	while (--tries && !have_eos_prop(params,PTP_VENDOR_CANON,PTP_DPC_CANON_EOS_EVFOutputDevice)) {
		GP_LOG_D("evfoutput device not found, retrying");
		usleep(50*1000);
		C_PTP (ptp_check_eos_events (params));
	}

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

	/* run this only on EOS M, not on PowerShot SX */
	/* I lost track where it is needed.
	 * Need it:
	 * + EOS M10
	 * + PowerShot SX 720HS
	 * + PowerShot G9x mark II
	 *
	 * NOT NEEDED for EOS M6 Mark II ... this behaves like a regular EOS no-M and will disable tethering if we set output=8.
	 */
	if (	is_canon_eos_m (params) &&
		(strcmp(params->deviceinfo.Model,"Canon EOS M6 Mark II") != 0)
	) {
		/* This code is needed on EOS m3 at least. might not be needed on others ... mess :/ */
		PTPPropertyValue    ct_val;

		GP_LOG_D ("EOS M detected");

		C_PTP (ptp_canon_eos_seteventmode(params, 2));
		ct_val.u32 = 0x0008;
		C_PTP (ptp_canon_eos_setdevicepropvalue (params, PTP_DPC_CANON_EOS_EVFOutputDevice, &ct_val, PTP_DTC_UINT32));

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
		usleep(1000*1000); /* 1 second */
#endif

		C_PTP (ptp_check_eos_events (params));
	}
	return GP_OK;
}

int
camera_prepare_capture (Camera *camera, GPContext *context)
{
	PTPParams		*params = &camera->pl->params;

	GP_LOG_D ("prepare_capture");
	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_FUJI:
		{
			PTPPropertyValue propval;

			/* without the firmware update ... not an error... */
			if (!have_prop (camera, PTP_VENDOR_FUJI, PTP_DPC_FUJI_PriorityMode))
				return GP_OK;

			/* timelapse does:
			 * d38c -> 1	(PC Mode)
			 * d207 -> 2	(USB control)
			 */

			propval.u16 = 0x0001;
			LOG_ON_PTP_E (ptp_setdevicepropvalue (params, 0xd38c, &propval, PTP_DTC_UINT16));
			propval.u16 = 0x0002;
			LOG_ON_PTP_E (ptp_setdevicepropvalue (params, PTP_DPC_FUJI_PriorityMode, &propval, PTP_DTC_UINT16));
			return GP_OK;
		}
		break;
	case PTP_VENDOR_CANON:
		if (ptp_operation_issupported(params, PTP_OC_CANON_InitiateReleaseControl))
			return camera_prepare_canon_powershot_capture(camera,context);

		if (ptp_operation_issupported(params, PTP_OC_CHDK))
			return camera_prepare_chdk_capture(camera,context);

		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
		    ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)
		)
			return camera_prepare_canon_eos_capture(camera,context);
		gp_context_error(context, _("Sorry, your Canon camera does not support Canon capture"));
		return GP_ERROR_NOT_SUPPORTED;
	case PTP_VENDOR_PANASONIC: {
		char buf[1024];
		if ((GP_OK != gp_setting_get("ptp2","capturetarget",buf)) || !strcmp(buf,"sdram"))
			C_PTP (ptp_panasonic_setcapturetarget(params, 1));
		else
			C_PTP (ptp_panasonic_setcapturetarget(params, 0));
		break;
	}
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
	if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_AfCancel))
		CR (ptp_canon_eos_afcancel(params));

	if (is_canon_eos_m (params)) {
		PTPPropertyValue    ct_val;

		ct_val.u32 = 0x0000;
		C_PTP (ptp_canon_eos_setdevicepropvalue (params, PTP_DPC_CANON_EOS_EVFOutputDevice, &ct_val, PTP_DTC_UINT32));
	}

	/* then emits 911b and 911c ... not done yet ... */
	CR (camera_canon_eos_update_capture_target(camera, context, 1));

	if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_ResetUILock)) {
		if (params->uilocked) {
			LOG_ON_PTP_E (ptp_canon_eos_resetuilock (params));
			params->uilocked = 0;
		}
	}

	/* Drain the rest set of the event data */
	C_PTP (ptp_check_eos_events (params));
	/* Resets the camera state */
	C_PTP (ptp_canon_eos_setremotemode(params, 0));
	/* Enables camera display for some reason. */
	C_PTP (ptp_canon_eos_setremotemode(params, 1));
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

		if (ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_RemoteRelease) ||
		    ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EOS_RemoteReleaseOn)
		)
			return camera_unprepare_canon_eos_capture (camera, context);

		gp_context_error(context,
		_("Sorry, your Canon camera does not support Canon capture"));
		return GP_ERROR_NOT_SUPPORTED;
	case PTP_VENDOR_FUJI:
		{
			PTPPropertyValue propval;
			PTPParams *params = &camera->pl->params;

			if (params->inliveview) {
				C_PTP_REP (ptp_terminateopencapture (params, params->opencapture_transid));
				params->inliveview = 0;
			}

			propval.u16 = 0x0001;
			C_PTP (ptp_setdevicepropvalue (params, PTP_DPC_FUJI_PriorityMode, &propval, PTP_DTC_UINT16));
			return GP_OK;
		}
		break;
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
		if (	(res != PTP_RC_DeviceBusy) &&
			(res != PTP_RC_NIKON_Bulb_Release_Busy)
		)
			return res;
		if (waitms) usleep(waitms*1000)/*wait a bit*/;
	} while (tries--);
	return res;
}

struct submenu;
#define CONFIG_GET_ARGS Camera *camera, CameraWidget **widget, struct submenu* menu, PTPDevicePropDesc *dpd
#define CONFIG_GET_NAMES camera, widget, menu, dpd
typedef int (*get_func)(CONFIG_GET_ARGS);
#define CONFIG_PUT_ARGS Camera *camera, CameraWidget *widget, PTPPropertyValue *propval, PTPDevicePropDesc *dpd, int *alreadyset
#define CONFIG_PUT_NAMES camera, widget, propval, dpd, alreadyset
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

/* Generic helper function for:
 *
 * ENUM xINTxx propertiess, with potential vendor specific variables. \
 */
#define GENERIC_TABLE(bits,type,dpc) \
struct deviceproptable##bits {		\
	char		*label;		\
	type		value;		\
	uint16_t	vendor_id;	\
};\
\
static int \
_get_Generic##bits##Table(CONFIG_GET_ARGS, struct deviceproptable##bits * tbl, int tblsize) { \
	int i, j; \
	int isset = FALSE, isset2 = FALSE; \
 \
	if (!(dpd->FormFlag & (PTP_DPFF_Enumeration|PTP_DPFF_Range))) { \
		GP_LOG_D ("no enumeration/range in %sbit table code... going on", #bits); \
	} \
	if (dpd->DataType != dpc) { \
		GP_LOG_D ("no %s prop in %sbit table code", #bits, #bits); \
		return GP_ERROR; \
	} \
 \
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget); \
	gp_widget_set_name (*widget, menu->name); \
	if (dpd->FormFlag & PTP_DPFF_Enumeration) { \
		if (!dpd->FORM.Enum.NumberOfValues) { \
			/* fill in with all values we have in the table. */ \
			for (j=0;j<tblsize;j++) { \
				if ((tbl[j].vendor_id == 0) || \
				    (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID) \
				) { \
					gp_widget_add_choice (*widget, _(tbl[j].label)); \
					if (tbl[j].value == dpd->CurrentValue.bits) { \
						gp_widget_set_value (*widget, _(tbl[j].label)); \
						isset2 = TRUE; \
					} \
				} \
			} \
			/* fallthrough in case we do not have currentvalue in the table. isset2 = FALSE */ \
		} \
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) { \
			isset = FALSE; \
			for (j=0;j<tblsize;j++) { \
				if ((tbl[j].value == dpd->FORM.Enum.SupportedValue[i].bits) && \
				    ((tbl[j].vendor_id == 0) || \
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)) \
				) { \
					gp_widget_add_choice (*widget, _(tbl[j].label)); \
					if (tbl[j].value == dpd->CurrentValue.bits) { \
						isset2 = TRUE; \
						gp_widget_set_value (*widget, _(tbl[j].label)); \
					} \
					isset = TRUE; \
					break; \
				} \
			} \
			if (!isset) { \
				char buf[200]; \
				sprintf(buf, _("Unknown value %04x"), dpd->FORM.Enum.SupportedValue[i].bits); \
				gp_widget_add_choice (*widget, buf); \
				if (dpd->FORM.Enum.SupportedValue[i].bits == dpd->CurrentValue.bits) { \
					isset2 = TRUE; \
					gp_widget_set_value (*widget, buf); \
				} \
			} \
		} \
	} \
	if (dpd->FormFlag & PTP_DPFF_Range) { \
		type r;	\
		for (	r = dpd->FORM.Range.MinimumValue.bits; \
			r<=dpd->FORM.Range.MaximumValue.bits; \
			r+= dpd->FORM.Range.StepSize.bits \
		) { \
			isset = FALSE; \
			for (j=0;j<tblsize;j++) { \
				if ((tbl[j].value == r) && \
				    ((tbl[j].vendor_id == 0) || \
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)) \
				) { \
					gp_widget_add_choice (*widget, _(tbl[j].label)); \
					if (r == dpd->CurrentValue.bits) { \
						isset2 = TRUE; \
						gp_widget_set_value (*widget, _(tbl[j].label)); \
					} \
					isset = TRUE; \
					break; \
				} \
			} \
			if (!isset) { \
				char buf[200]; \
				sprintf(buf, _("Unknown value %04x"), r); \
				gp_widget_add_choice (*widget, buf); \
				if (r == dpd->CurrentValue.bits) { \
					isset2 = TRUE; \
					gp_widget_set_value (*widget, buf); \
				} \
			} \
 \
			/* device might report stepsize 0. but we do at least 1 round */ \
			if (dpd->FORM.Range.StepSize.bits == 0) \
				break; \
		} \
	} \
	if (!isset2) { \
		for (j=0;j<tblsize;j++) { \
			if (((tbl[j].vendor_id == 0) || \
			     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)) && \
			     (tbl[j].value == dpd->CurrentValue.bits) \
			) { \
				gp_widget_add_choice (*widget, _(tbl[j].label)); \
				isset2 = TRUE; \
				gp_widget_set_value (*widget, _(tbl[j].label)); \
			} \
		} \
		if (!isset2) { \
			char buf[200]; \
			sprintf(buf, _("Unknown value %04x"), dpd->CurrentValue.bits); \
			gp_widget_add_choice (*widget, buf); \
			gp_widget_set_value (*widget, buf); \
		} \
	} \
	return (GP_OK); \
} \
\
\
static int \
_put_Generic##bits##Table(CONFIG_PUT_ARGS, struct deviceproptable##bits * tbl, int tblsize) { \
	char *value; \
	int i, intval, j; \
	int foundvalue = 0; \
	type bits##val = 0; \
 \
	CR (gp_widget_get_value (widget, &value)); \
	for (i=0;i<tblsize;i++) { \
		if ((!strcmp(_(tbl[i].label),value) || !strcmp(tbl[i].label,value)) && \
		    ((tbl[i].vendor_id == 0) || (tbl[i].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)) \
		) { \
			bits##val = tbl[i].value; \
			foundvalue = 1; \
		 \
			if (dpd->FormFlag & PTP_DPFF_Enumeration) { \
				for (j = 0; j<dpd->FORM.Enum.NumberOfValues; j++) { \
					if (bits##val == dpd->FORM.Enum.SupportedValue[j].bits) { \
						GP_LOG_D ("FOUND right value for %s in the enumeration at val %d", value, bits##val); \
						propval->bits = bits##val; \
						return GP_OK; \
					} \
				} \
				GP_LOG_D ("did not find the right value for %s in the enumeration at val %d... continuing", value, bits##val); \
				/* continue looking, but with this value as fallback */ \
			} else { \
				GP_LOG_D ("not an enumeration ... return %s as %d", value, bits##val); \
				propval->bits = bits##val; \
				return GP_OK; \
			} \
		} \
	} \
	if (foundvalue) { \
		GP_LOG_D ("Using fallback, not found in enum... return %s as %d", value, bits##val); \
		propval->bits = bits##val; \
		return GP_OK; \
	} \
	if (!sscanf(value, _("Unknown value %04x"), &intval)) { \
		GP_LOG_E ("failed to find value %s in list", value); \
		return GP_ERROR; \
	} \
	GP_LOG_D ("Using fallback, not found in enum... return %s as %d", value, bits##val); \
	propval->bits = intval; \
	return GP_OK; \
}

GENERIC_TABLE(u32,uint32_t,PTP_DTC_UINT32)
GENERIC_TABLE(u16,uint16_t,PTP_DTC_UINT16)
GENERIC_TABLE(i16,int16_t, PTP_DTC_INT16)
GENERIC_TABLE(u8, uint8_t, PTP_DTC_UINT8)
GENERIC_TABLE(i8, int8_t,  PTP_DTC_INT8)

#define GENERIC16TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_Genericu16Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Genericu16Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

#define GENERIC32TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_Genericu32Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Genericu32Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

#define GENERICI16TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_Generici16Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Generici16Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

#define GENERIC8TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_Genericu8Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Genericu8Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

#define GENERICI8TABLE(name,tbl) 			\
static int						\
_get_##name(CONFIG_GET_ARGS) {				\
	return _get_Generici8Table(CONFIG_GET_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}							\
							\
static int __unused__					\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Generici8Table(CONFIG_PUT_NAMES,	\
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
_get_STR_ENUMList (CONFIG_GET_ARGS) {
	int j;

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_STR)
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++)
		gp_widget_add_choice (*widget,dpd->FORM.Enum.SupportedValue[j].str);
	gp_widget_set_value (*widget,dpd->CurrentValue.str);
	return GP_OK;
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
	C_MEM (propval->a.v = calloc((strlen(value)+1),sizeof(PTPPropertyValue)));
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

static int
_get_Fuji_Totalcount(CONFIG_GET_ARGS) {
	char buf[20];

	sprintf(buf,"%d",dpd->CurrentValue.u32 >> 16);
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_value (*widget, buf);
	return	GP_OK;
}

/* generic int getter */
static int
_get_INT(CONFIG_GET_ARGS) {
	char value[64];
	float	rvalue = 0;

	switch (dpd->DataType) {
	case PTP_DTC_UINT32:
		sprintf (value, "%u", dpd->CurrentValue.u32 ); rvalue = dpd->CurrentValue.u32;
		break;
	case PTP_DTC_INT32:
		sprintf (value, "%d", dpd->CurrentValue.i32 ); rvalue = dpd->CurrentValue.i32;
		break;
	case PTP_DTC_UINT16:
		sprintf (value, "%u", dpd->CurrentValue.u16 ); rvalue = dpd->CurrentValue.u16;
		break;
	case PTP_DTC_INT16:
		sprintf (value, "%d", dpd->CurrentValue.i16 ); rvalue = dpd->CurrentValue.i16;
		break;
	case PTP_DTC_UINT8:
		sprintf (value, "%u", dpd->CurrentValue.u8 ); rvalue = dpd->CurrentValue.u8;
		break;
	case PTP_DTC_INT8:
		sprintf (value, "%d", dpd->CurrentValue.i8 ); rvalue = dpd->CurrentValue.i8;
		break;
	default:
		sprintf (value,_("unexpected datatype %i"),dpd->DataType);
		return GP_ERROR;
	}
	if (dpd->FormFlag == PTP_DPFF_Enumeration) {
		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		gp_widget_set_name (*widget, menu->name);
		gp_widget_set_value (*widget, value); /* text */
	} else {
		if (dpd->FormFlag == PTP_DPFF_Range) {
			gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
			gp_widget_set_name (*widget, menu->name);
			gp_widget_set_value (*widget, &rvalue); /* float */
		} else {
			gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
			gp_widget_set_name (*widget, menu->name);
			gp_widget_set_value (*widget, value); /* text */
		}
	}

	if (dpd->FormFlag == PTP_DPFF_Enumeration) {
		int i;

		for (i=0;i<dpd->FORM.Enum.NumberOfValues;i++) {
			switch (dpd->DataType) {
			case PTP_DTC_UINT32:	sprintf (value, "%u", dpd->FORM.Enum.SupportedValue[i].u32 ); break;
			case PTP_DTC_INT32:	sprintf (value, "%d", dpd->FORM.Enum.SupportedValue[i].i32 ); break;
			case PTP_DTC_UINT16:	sprintf (value, "%u", dpd->FORM.Enum.SupportedValue[i].u16 ); break;
			case PTP_DTC_INT16:	sprintf (value, "%d", dpd->FORM.Enum.SupportedValue[i].i16 ); break;
			case PTP_DTC_UINT8:	sprintf (value, "%u", dpd->FORM.Enum.SupportedValue[i].u8  ); break;
			case PTP_DTC_INT8:	sprintf (value, "%d", dpd->FORM.Enum.SupportedValue[i].i8  ); break;
			default: sprintf (value,_("unexpected datatype %i"),dpd->DataType); return GP_ERROR;
			}
			gp_widget_add_choice (*widget,value);
		}
	}
	if (dpd->FormFlag == PTP_DPFF_Range) {
		float b = 0, t = 0, s = 0;

#define X(type,u) case type: b = (float)dpd->FORM.Range.MinimumValue.u; t = (float)dpd->FORM.Range.MaximumValue.u; s = (float)dpd->FORM.Range.StepSize.u; break;
		switch (dpd->DataType) {
		X(PTP_DTC_UINT32,u32)
		X(PTP_DTC_INT32,i32)
		X(PTP_DTC_UINT16,u16)
		X(PTP_DTC_INT16,i16)
		X(PTP_DTC_UINT8,u8)
		X(PTP_DTC_INT8,i8)
		}
#undef X
		gp_widget_set_range (*widget, b, t, s);
	}
	return GP_OK;
}

static int
_put_INT(CONFIG_PUT_ARGS) {
	if (dpd->FormFlag == PTP_DPFF_Range) {
		float f;

		CR (gp_widget_get_value(widget, &f));
		switch (dpd->DataType) {
		case PTP_DTC_UINT32:	propval->u32 = f; break;
		case PTP_DTC_INT32:	propval->i32 = f; break;
		case PTP_DTC_UINT16:	propval->u16 = f; break;
		case PTP_DTC_INT16:	propval->i16 = f; break;
		case PTP_DTC_UINT8:	propval->u8 = f; break;
		case PTP_DTC_INT8:	propval->i8 = f; break;
		}
		return GP_OK;
	} else {
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

#define PUT_SONY_VALUE_(bits,inttype) 								\
static int										\
_put_sony_value_##bits (PTPParams*params, uint16_t prop, inttype value,int useenumorder) {	\
	GPContext 		*context = ((PTPData *) params->data)->context;		\
	PTPDevicePropDesc	dpd;							\
	PTPPropertyValue	propval;						\
	inttype			origval;						\
	time_t			start,end;						\
	int			tries = 100;	/* 100 steps allowed towards the new value */	\
	int			firstloop = 1;						\
											\
	GP_LOG_D("setting 0x%04x to 0x%08x", prop, value);				\
											\
	C_PTP_REP (ptp_sony_getalldevicepropdesc (params));				\
	C_PTP_REP (ptp_generic_getdevicepropdesc (params, prop, &dpd));			\
	if (value == dpd.CurrentValue.bits) {						\
		GP_LOG_D("value is already 0x%08x", value);				\
		return GP_OK;								\
	}										\
fallback:										\
	useenumorder = useenumorder && (dpd.FormFlag & PTP_DPFF_Enumeration) && dpd.FORM.Enum.NumberOfValues; \
	do {										\
		int posorig = -1, posnew = -1, posnow = -1; \
		origval = dpd.CurrentValue.bits;					\
		/* if it is a ENUM, the camera will walk through the ENUM */		\
		if (useenumorder) {		\
			int i;				\
											\
			for (i=0;i<dpd.FORM.Enum.NumberOfValues;i++) {			\
				if (origval == dpd.FORM.Enum.SupportedValue[i].bits)	\
					posorig = i;					\
				if (value == dpd.FORM.Enum.SupportedValue[i].bits)	\
					posnew = i;					\
				if ((posnew != -1) && (posorig != -1))			\
					break;						\
			}								\
			if (posnew == -1) {						\
				gp_context_error (context, _("Target value is not in enumeration."));\
				return GP_ERROR_BAD_PARAMETERS;				\
			}								\
			GP_LOG_D("posnew %d, posorig %d, value %d", posnew, posorig, value);	\
			if (posnew == posorig)						\
				break;							\
			/*								\
			Note: Sony seems to report values in an enum that are not	\
			actually supported under certain configurations (e.g. ISO).	\
			If we use `posnew - posorig` to ask camera to jump to the	\
			required position, it will end up overshooting, and on the next	\
			cycle will try to move by the same `posnew - posorig` in the	\
			opposite direction, overshoot again, and so on infinitely.	\
			Instead, we have to ask camera to +/- values one by one and read	\
			back the value it _actually_ switched to each time.		\
			Update: try the large jump first, if that does not work, use single jumps \
			*/ 								\
			if (posnew > posorig)						\
				propval.u8 = firstloop?(posnew-posorig):0x01;		\
			else								\
				propval.u8 = firstloop?(0x100-(posorig-posnew)):0xff;	\
			firstloop = 0;							\
		} else {								\
			if (value == origval)						\
				break;							\
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
			if (dpd.CurrentValue.bits == value) {				\
				GP_LOG_D ("Value matched!");				\
				break;							\
			}								\
			if (dpd.CurrentValue.bits != origval) {				\
				GP_LOG_D ("value changed (0x%x vs 0x%x vs target 0x%x), next step....", dpd.CurrentValue.bits, origval, value);\
				break;							\
			}								\
											\
			usleep(200*1000);						\
											\
			time(&end);							\
		} while (end-start <= 3);						\
											\
		int overshoot = 0; \
		if (useenumorder) { \
			for (int i=0;i<dpd.FORM.Enum.NumberOfValues;i++) {			\
				if (dpd.CurrentValue.bits == dpd.FORM.Enum.SupportedValue[i].bits) {	\
					posnow = i;					\
					break;						\
				}							\
			}								\
			GP_LOG_D("posnow %d, value %d", posnow, dpd.CurrentValue.bits);	\
											\
			/*								\
			 If we made a small +-1 adjustment, check for overshoot.	\
			 However, use enum position instead of value itself for comparisons as enums		\
			 are not always sorted (e.g. Auto ISO has higher numerical value but comes earlier).	\
		    */ 									\
			overshoot = ((propval.u8 == 0x01) && (posnow > posnew)) || ((propval.u8 == 0xff) && (posnow < posnew)); \
		} else { 								\
			overshoot = ((propval.u8 == 0x01) && (dpd.CurrentValue.bits > value)) || ((propval.u8 == 0xff) && (dpd.CurrentValue.bits < value)); \
		} 									\
											\
		if (overshoot) {							\
			GP_LOG_D ("We overshooted value, maybe not exact match possible. Break!");	\
			break;								\
		}									\
											\
		if (dpd.CurrentValue.bits == value) {					\
			GP_LOG_D ("Value matched!");					\
			break;								\
		}									\
		if (dpd.CurrentValue.bits == origval) {					\
			GP_LOG_D ("value did not change (0x%x vs 0x%x vs target 0x%x), not good ...", dpd.CurrentValue.bits, origval, value);\
			break;								\
		}									\
		/* We did not get there. Did we hit 0? */				\
		if (useenumorder) {							\
			if (posnow == -1) {						\
				GP_LOG_D ("Now value is not in enumeration, falling back to ordered tries.");\
				useenumorder = 0;					\
				goto fallback;						\
			}								\
			if ((posnow == 0) && (propval.u8 == 0xff)) {			\
				gp_context_error (context, _("Sony was not able to set the new value, is it valid?"));	\
				GP_LOG_D ("hit bottom of enumeration, not good.");	\
				return GP_ERROR;					\
			}								\
			if ((posnow == dpd.FORM.Enum.NumberOfValues-1) && (propval.u8 == 0x01)) {			\
				GP_LOG_D ("hit top of enumeration, not good.");		\
				gp_context_error (context, _("Sony was not able to set the new value, is it valid?"));	\
				return GP_ERROR;					\
			}								\
		} 									\
	} while (tries--);/* occasionally we fail, make an escape path */		\
	return GP_OK;									\
}

PUT_SONY_VALUE_(u16,uint16_t) /* _put_sony_value_u16 */
PUT_SONY_VALUE_(i16,int16_t) /* _put_sony_value_i16 */
PUT_SONY_VALUE_(u32,uint32_t) /* _put_sony_value_u32 */

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
	{ N_("Off"),			0x8014, PTP_VENDOR_NIKON },
	{ N_("Natural light auto"),	0x8016, PTP_VENDOR_NIKON },

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
	{ N_("Preset 1"),		0x8020, PTP_VENDOR_SONY },
	{ N_("Preset 2"),		0x8021, PTP_VENDOR_SONY },
	{ N_("Preset 3"),		0x8022, PTP_VENDOR_SONY },
	{ N_("Preset"),			0x8023, PTP_VENDOR_SONY },
	{ N_("Underwater: Auto"),	0x8030, PTP_VENDOR_SONY },

	{ N_("Shade"),			0x8001, PTP_VENDOR_PENTAX },
	{ N_("Cloudy"),			0x8002, PTP_VENDOR_PENTAX },
	{ N_("Tungsten 2"),		0x8020, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: Daylight"),	0x8003, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: Day White"),	0x8004, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: White"),	0x8005, PTP_VENDOR_PENTAX },
	{ N_("Fluorescent: Tungsten"),	0x8006, PTP_VENDOR_PENTAX },
};
GENERIC16TABLE(WhiteBalance,whitebalance)

static struct deviceproptableu16 olympus_whitebalance[] = {
	{ N_("Automatic"),		0x0001, 0 },
	{ N_("Daylight"),		0x0002, 0 },
	{ N_("Shade"),			0x0003, 0 },
	{ N_("Cloudy"),			0x0004, 0 },
	{ N_("Tungsten"),		0x0005, 0 },
	{ N_("Fluorescent"),		0x0006, 0 },
	{ N_("Underwater"),		0x0007, 0 },
	{ N_("Flash"),			0x0008, 0 },
	{ N_("Preset 1"),		0x0009, 0 },
	{ N_("Preset 2"),		0x000a, 0 },
	{ N_("Preset 3"),		0x000b, 0 },
	{ N_("Preset 4"),		0x000c, 0 },
	{ N_("Custom"),			0x000d, 0 },
};
GENERIC16TABLE(Olympus_WhiteBalance,olympus_whitebalance)

static struct deviceproptableu16 fuji_imageformat[] = {
	{ N_("RAW"),			1,	PTP_VENDOR_FUJI },
	{ N_("JPEG Fine"),		2,	PTP_VENDOR_FUJI },
	{ N_("JPEG Normal"),		3,	PTP_VENDOR_FUJI },
	{ N_("RAW + JPEG Fine"),	4,	PTP_VENDOR_FUJI },
	{ N_("RAW + JPEG Normal"),	5,	PTP_VENDOR_FUJI },
};
GENERIC16TABLE(Fuji_ImageFormat,fuji_imageformat)

static struct deviceproptableu16 olympus_imageformat[] = {
	{ N_("RAW"),			0x020,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Large Fine JPEG"),	0x101,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Large Normal JPEG"),	0x102,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Medium Normal JPEG"),	0x103,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Small Normal JPEG"),	0x104,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Large Fine JPEG+RAW"),	0x121,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Large Normal JPEG+RAW"),	0x122,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Medium Normal JPEG+RAW"),	0x123,	PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Small Normal JPEG+RAW"),	0x124,	PTP_VENDOR_GP_OLYMPUS_OMD },
};
GENERIC16TABLE(Olympus_Imageformat,olympus_imageformat)

static struct deviceproptableu16 fuji_releasemode[] = {
	{ N_("Single frame"),		1,	PTP_VENDOR_FUJI },
	{ N_("Continuous low speed"),	2,	PTP_VENDOR_FUJI },
	{ N_("Continuous high speed"),	3,	PTP_VENDOR_FUJI },
	{ N_("Self-timer"),		4,	PTP_VENDOR_FUJI },
	{ N_("Mup Mirror up"),		5,	PTP_VENDOR_FUJI },
};
GENERIC16TABLE(Fuji_ReleaseMode,fuji_releasemode)

static struct deviceproptableu16 fuji_filmsimulation[] = {
	{ N_("PROVIA/Standard"),            1,	PTP_VENDOR_FUJI },
	{ N_("Velvia/Vivid"),               2,	PTP_VENDOR_FUJI },
	{ N_("ASTIA/Soft"),                 3,	PTP_VENDOR_FUJI },
	{ N_("PRO Neg.Hi"),                 4,	PTP_VENDOR_FUJI },
	{ N_("PRO Neg.Std"),                5,	PTP_VENDOR_FUJI },
	{ N_("Black & White"),              6,	PTP_VENDOR_FUJI },
	{ N_("Black & White+Ye Filter"),    7,	PTP_VENDOR_FUJI },
	{ N_("Black & White+R Filter"),     8,	PTP_VENDOR_FUJI },
	{ N_("Black & White+G Filter"),     9,	PTP_VENDOR_FUJI },
	{ N_("Sepia"),                      10,	PTP_VENDOR_FUJI },
	{ N_("Classic Chrome"),             11,	PTP_VENDOR_FUJI },
	{ N_("ACROS"),                      12,	PTP_VENDOR_FUJI },
	{ N_("ACROS+Ye Filter"),            13,	PTP_VENDOR_FUJI },
	{ N_("ACROS+R Filter,"),            14,	PTP_VENDOR_FUJI },
	{ N_("ACROS+G Filter"),             15,	PTP_VENDOR_FUJI },
	{ N_("ETERNA/Cinema"),              16,	PTP_VENDOR_FUJI },
	{ N_("Classic Neg"),                17,	PTP_VENDOR_FUJI },
	{ N_("ETERNA BLEACH BYPASS"),       18,	PTP_VENDOR_FUJI },
};
GENERIC16TABLE(Fuji_FilmSimulation,fuji_filmsimulation)

static struct deviceproptableu16 fuji_prioritymode[] = {
	{ N_("Camera"),	1,	PTP_VENDOR_FUJI },
	{ N_("USB"),	2,	PTP_VENDOR_FUJI },
};
GENERIC16TABLE(Fuji_PriorityMode,fuji_prioritymode)

static int
_get_ExpCompensation(CONFIG_GET_ARGS) {
	int j;
	char buf[13];

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
	char	*value;
	float	x;
	int16_t	val, targetval = 0;
	int	mindist = 65535, j;

	CR (gp_widget_get_value(widget, &value));
	if (1 != sscanf(value,"%g", &x))
		return GP_ERROR;

	/* float processing is not always hitting the right values, but close */
	val = x*1000.0;
	for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
		if (abs(dpd->FORM.Enum.SupportedValue[j].i16 - val) < mindist) {
			mindist = abs(dpd->FORM.Enum.SupportedValue[j].i16 - val);
			targetval = dpd->FORM.Enum.SupportedValue[j].i16;
		}
	}
	propval->i16 = targetval;
	return GP_OK ;
}

/* old method, uses stepping */
static int
_put_Sony_ExpCompensation(CONFIG_PUT_ARGS) {
	int ret;

	ret = _put_ExpCompensation(CONFIG_PUT_NAMES);
	if (ret != GP_OK) return ret;
	*alreadyset = 1;
	return _put_sony_value_i16 (&camera->pl->params, dpd->DevicePropertyCode, propval->i16, 0);
}

/* new method, can set directly */
static int
_put_Sony_ExpCompensation2(CONFIG_PUT_ARGS) {
	int ret;

	ret = _put_ExpCompensation(CONFIG_PUT_NAMES);
	if (ret != GP_OK) return ret;
	*alreadyset = 1;
	return translate_ptp_result (ptp_sony_setdevicecontrolvaluea (&camera->pl->params, dpd->DevicePropertyCode, propval, PTP_DTC_INT16));
}

static int
_get_Olympus_ExpCompensation(CONFIG_GET_ARGS) {
	int j;
	char buf[13];

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_UINT16)
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
_put_Olympus_ExpCompensation(CONFIG_PUT_ARGS) {
	char	*value;
	float	x;
	int16_t	val, targetval = 0;
	int	mindist = 65535, j;

	CR (gp_widget_get_value(widget, &value));
	if (1 != sscanf(value,"%g", &x))
		return GP_ERROR;

	/* float processing is not always hitting the right values, but close */
	val = x*1000.0;
	for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
		if (abs(dpd->FORM.Enum.SupportedValue[j].i16 - val) < mindist) {
			mindist = abs(dpd->FORM.Enum.SupportedValue[j].i16 - val);
			targetval = dpd->FORM.Enum.SupportedValue[j].i16;
		}
	}
	propval->i16 = targetval;
	return GP_OK ;
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
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.u16;
	b = (float)dpd->FORM.Range.MinimumValue.u16;
	t = (float)dpd->FORM.Range.MaximumValue.u16;
	s = (float)dpd->FORM.Range.StepSize.u16;
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return GP_OK;
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
_get_Canon_EOS_ZoomRange(CONFIG_GET_ARGS) {
	float	f, t, b, s;

/*
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return GP_ERROR;
*/
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.u32;
	b = 0.0;
	t = 1000.0;
	s = 1.0;
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return GP_OK;
}

static int
_put_Canon_EOS_ZoomRange(CONFIG_PUT_ARGS)
{
	float	f;

	CR (gp_widget_get_value(widget, &f));
	propval->u32 = (unsigned int)f;
	return GP_OK;
}


/* This seems perhaps focal length * 1.000.000 */
static int
_get_Sony_Zoom(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.u32 / 1000000;
	b = (float)dpd->FORM.Range.MinimumValue.u32 / 1000000;
	t = (float)dpd->FORM.Range.MaximumValue.u32 / 1000000;
	s = 1;
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return GP_OK;
}

static int
_put_Sony_Zoom(CONFIG_PUT_ARGS)
{
	float	f;
	PTPParams *params = &camera->pl->params;

	CR (gp_widget_get_value(widget, &f));
	propval->u32 = (uint32_t)f*1000000;
	*alreadyset = 1;
	return _put_sony_value_u32(params, PTP_DPC_SONY_Zoom, propval->u32, 0);
}

static int
_get_Nikon_WBBias(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	if (dpd->DataType != PTP_DTC_INT8)
		return GP_ERROR;
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return GP_ERROR;
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

/* This can get type 1 (INT8) , 2 (UINT8) and 4 (UINT16) */
static int
_get_Nikon_UWBBias(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	switch (dpd->DataType) {
	case PTP_DTC_UINT16:
		f = (float)dpd->CurrentValue.u16;
		b = (float)dpd->FORM.Range.MinimumValue.u16;
		t = (float)dpd->FORM.Range.MaximumValue.u16;
		s = (float)dpd->FORM.Range.StepSize.u16;
		break;
	case PTP_DTC_UINT8:
		f = (float)dpd->CurrentValue.u8;
		b = (float)dpd->FORM.Range.MinimumValue.u8;
		t = (float)dpd->FORM.Range.MaximumValue.u8;
		s = (float)dpd->FORM.Range.StepSize.u8;
		break;
	case PTP_DTC_INT8:
		f = (float)dpd->CurrentValue.i8;
		b = (float)dpd->FORM.Range.MinimumValue.i8;
		t = (float)dpd->FORM.Range.MaximumValue.i8;
		s = (float)dpd->FORM.Range.StepSize.i8;
		break;
	default:
		return GP_ERROR;
	}
	gp_widget_set_range (*widget, b, t, s);
	gp_widget_set_value (*widget, &f);
	return GP_OK;
}

static int
_put_Nikon_UWBBias(CONFIG_PUT_ARGS)
{
	float	f;

	CR (gp_widget_get_value(widget, &f));
	switch (dpd->DataType) {
	case PTP_DTC_UINT16:
		propval->u16 = (unsigned short)f;
		break;
	case PTP_DTC_UINT8:
		propval->u8 = (unsigned char)f;
		break;
	case PTP_DTC_INT8:
		propval->i8 = (char)f;
		break;
	default:
		return GP_ERROR;
	}
	return GP_OK;
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
		if (!isset && (dpd->FORM.Enum.NumberOfValues > 0)) {
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

static int
_get_Nikon_MovieLoopLength(CONFIG_GET_ARGS) {

	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		char buf[20];
		int i, isset = FALSE;

		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		gp_widget_set_name (*widget,menu->name);
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {

			sprintf (buf, "%d", dpd->FORM.Enum.SupportedValue[i].u32/10);
			gp_widget_add_choice (*widget, buf);
			if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32) {
				gp_widget_set_value (*widget, buf);
				isset = TRUE;
			}
		}
		if (!isset && (dpd->FORM.Enum.NumberOfValues > 0)) {
			sprintf (buf, "%d", dpd->FORM.Enum.SupportedValue[0].u32/10);
			gp_widget_set_value (*widget, buf);
		}
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_put_Nikon_MovieLoopLength(CONFIG_PUT_ARGS)
{
	char *val;
	int ival;

	CR (gp_widget_get_value(widget, &val));
	sscanf (val, "%d", &ival);
	propval->u32 = ival*10;
	return GP_OK;
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

	{ N_("C2"),			0x0010, 0 },
	{ N_("C3"),			0x0011, 0 },
	{ N_("Creative Auto"),		0x0013, 0 },
	{ N_("Movie"),			0x0014, 0 },
	{ N_("Auto"),			0x0016, 0 },	/* EOS M6 Mark 2 */
	{ N_("Handheld Night Scene"),	0x0017, 0 },	/* EOS M6 Mark 2 */
	{ N_("HDR Backlight Control"),	0x0018, 0 },	/* EOS M6 Mark 2 */
	{ N_("SCN"),			0x0019, 0 },
	{ N_("Food"),			0x001b, 0 },	/* EOS M6 Mark 2 */
	{ N_("Grainy B/W"),		0x001e, 0 },	/* EOS M6 Mark 2 */
	{ N_("Soft focus"),		0x001f, 0 },	/* EOS M6 Mark 2 */
	{ N_("Toy camera effect"),	0x0020, 0 },	/* EOS M6 Mark 2 */
	{ N_("Fish-eye effect"),	0x0021, 0 },	/* EOS M6 Mark 2 */
	{ N_("Water painting effect"),	0x0022, 0 },	/* EOS M6 Mark 2 */
	{ N_("Miniature effect"),	0x0023, 0 },	/* EOS M6 Mark 2 */
	{ N_("HDR art standard"),	0x0024, 0 },	/* EOS M6 Mark 2 */
	{ N_("HDR art vivid"),		0x0025, 0 },	/* EOS M6 Mark 2 */
	{ N_("HDR art bold"),		0x0026, 0 },	/* EOS M6 Mark 2 */
	{ N_("HDR art embossed"),	0x0027, 0 },	/* EOS M6 Mark 2 */
	{ N_("Panning"),		0x002d, 0 },	/* EOS M6 Mark 2 */
	{ N_("HDR"),			0x0031, 0 },
	{ N_("Self Portrait"),		0x0032, 0 },	/* EOS M6 Mark 2 */
	{ N_("Hybrid Auto"),		0x0033, 0 },	/* EOS M6 Mark 2 */
	{ N_("Smooth skin"),		0x0034, 0 },	/* EOS M6 Mark 2 */
	{ N_("Fv"),			0x0037, 0 },	/* EOS M6 Mark 2 */
};
GENERIC16TABLE(Canon_EOS_AutoExposureMode,canon_eos_autoexposuremode)

static struct deviceproptableu32 canon_eos_alomode[] = {
	{ N_("Standard"),				0x10000, 0 },
	{ N_("Standard (disabled in manual exposure)"),	0x00000, 0 },
	{ N_("Low"),					0x10101, 0 },
	{ N_("Low (disabled in manual exposure)"),	0x00101, 0 },
	{ N_("Off"),					0x10303, 0 },
	{ N_("Off (disabled in manual exposure)"),	0x00303, 0 },
	{ N_("High"),					0x10202, 0 },
	{ N_("High (disabled in manual exposure)"),	0x00202, 0 },
	{ N_("x1"),	0x1, 0 },
	{ N_("x2"),	0x2, 0 },
	{ N_("x3"),	0x3, 0 },
};
GENERIC32TABLE(Canon_EOS_AloMode,canon_eos_alomode)

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

static struct deviceproptableu8 nikon_liveviewsize[] = {
	{ N_("QVGA"),		1, 0 },
	{ N_("VGA"),		2, 0 },
	{ N_("XGA"),		3, 0 },
};
GENERIC8TABLE(Nikon_LiveViewSize,nikon_liveviewsize)

static struct deviceproptableu16 fuji_liveviewsize[] = {
	{ N_("XGA"),		1, 0 },
	{ N_("VGA"),		2, 0 },
	{ N_("QVGA"),		3, 0 },
};
GENERIC16TABLE(Fuji_LiveViewSize,fuji_liveviewsize)

static struct deviceproptableu8 sony_qx_liveviewsize[] = {
	{ "640x480",		1, 0 },
	{ "1024x768",		2, 0 },
	{ "1920x1280",		3, 0 },
};
GENERIC8TABLE(Sony_QX_LiveViewSize,sony_qx_liveviewsize)

static struct deviceproptablei8 sony_prioritymode[] = {
	{ N_("Camera"),		0, 0 },
	{ N_("Application"),	1, 0 },
};
GENERICI8TABLE(Sony_PriorityMode,sony_prioritymode)

static int
_get_Canon_LiveViewSize(CONFIG_GET_ARGS) {
	unsigned int i;
	unsigned int have = 0;
	char buf[20];

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;
/* actually it is a flag value, 1 = TFT, 2 = PC, 4 = MOBILE, 8 = MOBILE2 */

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i=0;i<dpd->FORM.Enum.NumberOfValues;i++) {
		if ((dpd->FORM.Enum.SupportedValue[i].u32 & 0xe) == 0x2) {
			if (!(have & 0x2))
				gp_widget_add_choice (*widget, _("Large"));
			have |= 0x2;
			continue;
		}
		if ((dpd->FORM.Enum.SupportedValue[i].u32 & 0xe) == 0x4) {
			if (!(have & 0x4))
				gp_widget_add_choice (*widget, _("Medium"));
			have |= 0x4;
			continue;
		}
		if ((dpd->FORM.Enum.SupportedValue[i].u32 & 0xe) == 0x8) {
			if (!(have & 0x8))
				gp_widget_add_choice (*widget, _("Small"));
			have |= 0x8;
			continue;
		}
	}
	if ((dpd->CurrentValue.u32 & 0xe) == 0x8) { gp_widget_set_value (*widget, _("Small")); return GP_OK; }
	if ((dpd->CurrentValue.u32 & 0xe) == 0x4) { gp_widget_set_value (*widget, _("Medium")); return GP_OK; }
	if ((dpd->CurrentValue.u32 & 0xe) == 0x2) { gp_widget_set_value (*widget, _("Large")); return GP_OK; }
	sprintf(buf,"val %x", dpd->CurrentValue.u32);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
_put_Canon_LiveViewSize(CONFIG_PUT_ARGS) {
	char		*val;
	unsigned int	outputval = 0;

	CR (gp_widget_get_value (widget, &val));
	if (!strcmp(val,_("Large"))) { outputval = 0x2; }
	if (!strcmp(val,_("Medium"))) { outputval = 0x4; }
	if (!strcmp(val,_("Small"))) { outputval = 0x8; }

	if (outputval == 0)
		return GP_ERROR_BAD_PARAMETERS;
	/* replace the current outputsize, but keep the TFT flag */
	propval->u32 = (dpd->CurrentValue.u32 & ~0xe) | outputval;
	return GP_OK;
}

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
	{ N_("Fine detail"),	0x88, 0 },
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
	{ "20.3",	0x0014,0 }, /* + 1/3 */
	{ "20",		0x0015,0 },
	{ "15",		0x0018,0 },
	{ "13",		0x001b,0 },
	{ "10",		0x001c,0 },
	{ "10.3",	0x001d,0 }, /* 10.4 */
	{ "8",		0x0020,0 },
	{ "6.3",	0x0023,0 }, /* + 1/3 */
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
	{ N_("3:2"),		0x01, 0 },
	{ N_("16:9"),		0x02, 0 },
	{ N_("4:3"),		0x03, 0 },
	{ N_("1:1"),		0x04, 0 },
};
GENERIC8TABLE(Sony_AspectRatio,sony_aspectratio)

/* values are from 6D */
static struct deviceproptableu32 canon_eos_aspectratio[] = {
	{ "3:2",	0x0000, 0},
	{ "1:1",	0x0001, 0},
	{ "4:3",	0x0002, 0},
	{ "16:9",	0x0007, 0},
	{ "1.6x",	0x000d, 0}, /* guess , EOS R */
};
GENERIC32TABLE(Canon_EOS_AspectRatio,canon_eos_aspectratio)

/* actually in 1/10s of a second, but only 3 values in use */
static struct deviceproptableu16 canon_selftimer[] = {
	{ N_("Not used"),	0,	0 },
	{ N_("10 seconds"),	100,	0 },
	{ N_("2 seconds"), 	20,	0 },
};
GENERIC16TABLE(Canon_SelfTimer,canon_selftimer)

/* actually it is a flag value, 1 = TFT, 2 = PC, 4 = MOBILE, 8 = MOBILE2 */
static struct deviceproptableu32 canon_eos_cameraoutput[] = {
	{ N_("Off"),		0, 0 }, /*On 5DM3, LCD/TFT is off, mirror down and optical view finder enabled */
	{ N_("TFT"),		1, 0 },
	{ N_("PC"), 		2, 0 },
	{ N_("TFT + PC"), 	3, 0 },
	{ N_("MOBILE"),		4, 0 },
	{ N_("TFT + MOBILE"),	5, 0 },
	{ N_("PC + MOBILE"),	6, 0 },
	{ N_("TFT + PC + MOBILE"), 7, 0 },
	{ N_("MOBILE2"),	8, 0 },
	{ N_("TFT + MOBILE2"),	9, 0 },
	{ N_("PC + MOBILE2"),	10, 0 },
	{ N_("TFT + PC + MOBILE2"), 11, 0 },
};
GENERIC32TABLE(Canon_EOS_CameraOutput,canon_eos_cameraoutput)

static struct deviceproptableu32 canon_eos_afmethod[] = {
	{ N_("Quick"),			0, 0 },
	{ N_("Live"),			1, 0 },
	{ N_("LiveFace"), 		2, 0 },
	{ N_("LiveMulti"), 		3, 0 },
	{ N_("LiveZone"),		4, 0 },
	{ N_("LiveSingleExpandCross"),	5, 0 },
	{ N_("LiveSingleExpandSurround"),	6, 0 },
	{ N_("LiveZoneLargeH"), 	7, 0 },
	{ N_("LiveZoneLargeV"),		8, 0 },
	{ N_("LiveCatchAF"),		9, 0 },
	{ N_("LiveSpotAF"),		10, 0 },
};
GENERIC32TABLE(Canon_EOS_AFMethod,canon_eos_afmethod)

static struct deviceproptableu16 canon_eos_evfrecordtarget[] = {
	{ N_("None"),		0, 0 },
	{ N_("SDRAM"),		3, 0 },
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
				if (LOG_ON_PTP_E (ptp_canon_viewfinderon (params)) == PTP_RC_OK)
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
	{ "32000",		0x008b, 0 },
	{ "40000",		0x008d, 0 },
	{ "51200",		0x0090, 0 },
	{ "64000",		0x0093, 0 },
	{ "80000",		0x0095, 0 },
	{ "102400",		0x0098, 0 },
	{ "204800",		0x00a0, 0 },
	{ "409600",		0x00a8, 0 },
	{ "819200",		0x00b0, 0 },
	{ N_("Auto"),		0x0000, 0 },
	{ N_("Auto ISO"),	0x0001, 0 },
};
GENERIC16TABLE(Canon_ISO,canon_isospeed)

/* see ptp-pack.c:ptp_unpack_EOS_ImageFormat */
static struct deviceproptableu16 canon_eos_image_format[] = {
	{ N_("RAW"),				0x0c00, 0 },
	{ N_("mRAW"),				0x1c00, 0 },
	{ N_("sRAW"),				0x2c00, 0 },
	{ N_("cRAW"),				0x0b00, 0 },
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
	{ N_("RAW + Large Fine JPEG"),		0x0c03, 0 },
	{ N_("mRAW + Large Fine JPEG"),		0x1c03, 0 },
	{ N_("sRAW + Large Fine JPEG"),		0x2c03, 0 },
	{ N_("cRAW + Large Fine JPEG"),		0x0b03, 0 },
	{ N_("RAW + Medium Fine JPEG"),		0x0c13, 0 },
	{ N_("mRAW + Medium Fine JPEG"),	0x1c13, 0 },
	{ N_("sRAW + Medium Fine JPEG"),	0x2c13, 0 },
	{ N_("cRAW + Medium Fine JPEG"),	0x0b13, 0 },
	{ N_("RAW + Small Fine JPEG"),		0x0c23, 0 },
	{ N_("RAW + Small Fine JPEG"),		0x0cd3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Small Fine JPEG"),		0x1c23, 0 },
	{ N_("mRAW + Small Fine JPEG"),		0x1cd3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Small Fine JPEG"),		0x2c23, 0 },
	{ N_("sRAW + Small Fine JPEG"),		0x2cd3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("cRAW + Small Fine JPEG"),		0x0bd3, 0 },
	{ N_("RAW + Large Normal JPEG"),	0x0c02, 0 },
	{ N_("mRAW + Large Normal JPEG"),	0x1c02, 0 },
	{ N_("sRAW + Large Normal JPEG"),	0x2c02, 0 },
	{ N_("cRAW + Large Normal JPEG"),	0x0b02, 0 },
	{ N_("RAW + Medium Normal JPEG"),	0x0c12, 0 },
	{ N_("mRAW + Medium Normal JPEG"),	0x1c12, 0 },
	{ N_("sRAW + Medium Normal JPEG"),	0x2c12, 0 },
	{ N_("cRAW + Medium Normal JPEG"),	0x0b12, 0 },
	{ N_("RAW + Small Normal JPEG"),	0x0c22, 0 },
	{ N_("RAW + Small Normal JPEG"),	0x0cd2, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Small Normal JPEG"),	0x1c22, 0 },
	{ N_("mRAW + Small Normal JPEG"),	0x1cd2, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Small Normal JPEG"),	0x2c22, 0 },
	{ N_("sRAW + Small Normal JPEG"),	0x2cd2, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("cRAW + Small Normal JPEG"),	0x0bd2, 0 },
	{ N_("RAW + Smaller JPEG"),		0x0ce3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Smaller JPEG"),		0x1ce3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Smaller JPEG"),		0x2ce3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("cRAW + Smaller JPEG"),		0x0be3, 0 }, /*Canon EOS M50*/
	{ N_("RAW + Tiny JPEG"),		0x0cf3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("mRAW + Tiny JPEG"),		0x1cf3, 0 }, /*Canon EOS 5D Mark III*/
	{ N_("sRAW + Tiny JPEG"),		0x2cf3, 0 }, /*Canon EOS 5D Mark III*/
	/* There are more RAW + 'smallish' JPEG combinations for at least the 5DM3 possible.
	   Axel was simply to lazy to exercise the combinatorial explosion. :-/ */
	/* 1DX series 0 compression options */
	{ N_("Small"),				0x2100, 0 },
	{ N_("Medium 1"),			0x5100, 0 },
	{ N_("Medium 2"),			0x6100, 0 },
	{ N_("Large"),				0x0100, 0 },
	{ N_("Small + RAW"),			0x0c21, 0 },
	{ N_("Medium 1 + RAW"),			0x0c51, 0 },
	{ N_("Medium 2 + RAW"),			0x0c61, 0 },
	{ N_("Large + RAW"),			0x0c01, 0 },
	{ N_("Small + cRAW"),			0x0b21, 0 },
	{ N_("Medium 1 + cRAW"),		0x0b51, 0 },
	{ N_("Medium 2 + cRAW"),		0x0b61, 0 },
	{ N_("Large + cRAW"),			0x0b01, 0 },

	/* 1DX mark ii */
	{ N_("Large + mRAW"),			0x1c01, 0 },
	{ N_("Medium 1 + mRAW"),		0x1c51, 0 },
	{ N_("Medium 2 + mRAW"),		0x1c61, 0 },
	{ N_("Small + mRAW"),			0x1c21, 0 },
	{ N_("Large + sRAW"),			0x2c01, 0 },
	{ N_("Medium 1 + sRAW"),		0x2c51, 0 },
	{ N_("Medium 2 + sRAW"),		0x2c61, 0 },
	{ N_("Small + sRAW"),			0x2c21, 0 },
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
	{ N_("Video"),			0x0002, 0 },
	{ N_("Continuous high speed"),	0x0004, 0 },
	{ N_("Continuous low speed"),	0x0005, 0 },
	{ N_("Single: Silent shooting"),0x0006, 0 },
	{ N_("Continuous timer"),	0x0007, 0 },
	{ N_("Timer 10 sec"),		0x0010, 0 },
	{ N_("Timer 2 sec"),		0x0011, 0 },
	{ N_("Super high speed continuous shooting"),		0x0012, 0 },
	{ N_("Single silent"),		0x0013, 0 },
	{ N_("Continuous silent"),	0x0014, 0 },
	{ N_("Silent HS continuous"),	0x0015, 0 },
	{ N_("Silent LS continuous"),	0x0016, 0 },
};
GENERIC16TABLE(Canon_EOS_DriveMode,canon_eos_drive_mode)

static int
_get_Olympus_ISO(CONFIG_GET_ARGS) {
	int i;

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_UINT16)
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char	buf[20];

		sprintf(buf,"%d",dpd->FORM.Enum.SupportedValue[i].u16);
		if (dpd->FORM.Enum.SupportedValue[i].u16 == 0xffff) { strcpy(buf,_("Auto")); }
		if (dpd->FORM.Enum.SupportedValue[i].u16 == 0xfffd) { strcpy(buf,_("Low")); }
		gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u16 == dpd->CurrentValue.u16)
			gp_widget_set_value (*widget,buf);
	}
	return GP_OK;
}

static int
_put_Olympus_ISO(CONFIG_PUT_ARGS)
{
	char *value;
	unsigned int	u;

	CR (gp_widget_get_value(widget, &value));
	if (!strcmp(value,_("Auto"))) {
		propval->u16 = 0xffff;
		return GP_OK;
	}
	if (!strcmp(value,_("Low"))) {
		propval->u16 = 0xfffd;
		return GP_OK;
	}

	if (sscanf(value, "%ud", &u)) {
		propval->u16 = u;
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_get_Olympus_OMD_Bulb(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Olympus_OMD_Bulb(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	GPContext *context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		int ret = ptp_olympus_omd_bulbstart (params);
		if (ret == PTP_RC_GeneralError) {
			gp_context_error (((PTPData *) camera->pl->params.data)->context,
			_("For bulb capture to work, make sure the mode dial is switched to 'M' and set 'shutterspeed' to 'bulb'."));
			return translate_ptp_result (ret);
		}
		C_PTP_REP (ret);
	} else {
		C_PTP_REP (ptp_olympus_omd_bulbend (params));
	}
	return GP_OK;
}

static struct deviceproptableu16 fuji_action[] = {
	{ N_("Shoot"),			0x0304, 0 },
	{ N_("Bulb On"),		0x0500, 0 },
	{ N_("Bulb Off"),		0x000c, 0 },
	{ N_("AF"),			0x0200, 0 },
	{ N_("Cancel AF"),		0x0004, 0 },
/* D208 is some kind of control, likely bitmasked. reported like an enum.
 * 0x200 seems to mean focusing?
 * 0x208 capture?
 * camera starts with 0x304
 *
 * After setting usually it does "initiatecapture" to trigger this mode operation.
 *
 * xt2:    0x104,0x200,0x4,0x304,0x500,0xc,0xa000,6,0x9000,2,0x9100,1,0x9300,5
 * xt3:    0x104,0x200,0x4,0x304,0x500,0xc,0xa000,6,0x9000,2,0x9100,1,0x9200,0x40,0x9300,5,0x804,0x80
 * xt30:   0x104,0x200,0x4,0x304,0x500,0xc,0xa000,6,0x9000,2,0x9100,1,0x9200,0x40,0x9300,5
 * xt4:    0x104,0x200,0x4,0x304,0x500,0xc,0x8000,0xa000,6,0x9000,2,0x9100,1,0x9300,5,0xe,0x9200,0x40,0x804,0x80
 * xh1:    0x104,0x200,0x4,0x304,0x500,0xc,0xa000,6,0x9000,2,0x9100,1,0x9300,5
 * gfx100: 0x104,0x200,0x4,0x304,0x500,0xc,0x8000,0xa000,6,0x9000,2,0x9100,1,0x9300,5,0xe,0x9200
 * gfx50r: 0x104,0x200,0x4,0x304,0x500,0xc,0xa000,6,0x9000,2,0x9100,1,0x9300,5,0xe
 * xpro2:  0x104,0x200,0x4,0x304,0x500,0xc,0xa000,6,0x9000,2,0x9100,1
 *
 * 0x304 is for regular capture         SDK_ShootS2toS0 (default) (SDK_Shoot)
 * 0x200 seems for autofocus (s1?)      SDK_ShootS1
 * 0x500 start bulb? 0xc end bulb?      SDK_StartBulb
 * 0xc                                  SDK_EndBulb
 * 0x600                                SDK_1PushAF
 * 0x4                                  SDK_CancelS1
 * 0x300                                SDK_ShootS2
 * 0x8000 might be autowhitebalance
 * working bulb transition (with autofocus):
 * 	0x200 -> wait for d209 turn from 1 to 2 -> 0x500 -> wait BULBTIME seconds -> 0xc
 * seen in fuji webcam traces:
 * 	0x9300 -> wait for d209 turn from 1 to 2 -> 0x0005
 * 	0x9000 -> ? not sure, was polling with d212 ?  -> 0x0002
 */
};
GENERIC16TABLE(Fuji_Action,fuji_action)

static int
_get_Fuji_AFDrive(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Fuji_AFDrive(CONFIG_PUT_ARGS)
{
	PTPParams		*params = &(camera->pl->params);
	GPContext		*context = ((PTPData *) params->data)->context;
	PTPPropertyValue	pval;

	/* Focusing first ... */
	pval.u16 = 0x9300;
	C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &pval, PTP_DTC_UINT16));
	C_PTP_REP (ptp_initiatecapture(params, 0x00000000, 0x00000000));

	/* poll camera until it is ready */
	pval.u16 = 0x0001;
	while (pval.u16 == 0x0001) {
		C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_FUJI_AFStatus, &pval, PTP_DTC_UINT16));
		GP_LOG_D ("XXX Ready to shoot? %X", pval.u16);
	}

	/* 2 - means OK apparently, 3 - means failed and initiatecapture will get busy. */
	if (pval.u16 == 3) { /* reported on out of focus */
		gp_context_error (context, _("Fuji Capture failed: Perhaps no auto-focus?"));
		return GP_ERROR;
	}

	/* release focus lock */

	pval.u16 = 0x0005;
	C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &pval, PTP_DTC_UINT16));
	C_PTP_REP (ptp_initiatecapture(params, 0x00000000, 0x00000000));
	return GP_OK;
}

static struct deviceproptableu16 fuji_focuspoints[] = {
	{ N_("91 Points (7x13)"),   0x0001, 0 },
	{ N_("325 Points (13x25)"), 0x0002, 0 },
	{ N_("117 Points (9x13)"),  0x0003, 0 },
	{ N_("425 Points (17x25)"), 0x0004, 0 },
};
GENERIC16TABLE(Fuji_FocusPoints,fuji_focuspoints)

static int
_get_Fuji_FocusPoint(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	gp_widget_set_value (*widget, dpd->CurrentValue.str);
	return GP_OK;
}

static int
_put_Fuji_FocusPoint(CONFIG_PUT_ARGS) {
        PTPParams *params = &(camera->pl->params);
        GPContext *context = ((PTPData *) params->data)->context;
        PTPPropertyValue pval;
        char *focus_point;

        CR (gp_widget_get_value(widget, &focus_point));
        C_MEM (pval.str = strdup(focus_point));
        C_PTP_REP(ptp_setdevicepropvalue(params, PTP_DPC_FUJI_FocusArea4, &pval, PTP_DTC_STR));
	*alreadyset = 1;
        return GP_OK;
}


static int
_get_Fuji_Bulb(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Fuji_Bulb(CONFIG_PUT_ARGS)
{
	PTPParams		*params = &(camera->pl->params);
	int			val;
	GPContext		*context = ((PTPData *) params->data)->context;
	PTPPropertyValue	pval;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		/* Focusing first ... */
		pval.u16 = 0x0200;
		C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &pval, PTP_DTC_UINT16));
		C_PTP_REP (ptp_initiatecapture(params, 0x00000000, 0x00000000));

		/* poll camera until it is ready */
		pval.u16 = 0x0001;
		while (pval.u16 == 0x0001) {
			C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_FUJI_AFStatus, &pval, PTP_DTC_UINT16));
			GP_LOG_D ("XXX Ready to shoot? %X", pval.u16);
		}

		/* 2 - means OK apparently, 3 - means failed and initiatecapture will get busy. */
		if (pval.u16 == 3) { /* reported on out of focus */
			gp_context_error (context, _("Fuji Capture failed: Perhaps no auto-focus?"));
			return GP_ERROR;
		}

		/* now start bulb capture */
		pval.u16 = 0x0500;
		C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &pval, PTP_DTC_UINT16));

		C_PTP_REP (ptp_initiatecapture(params, 0x00000000, 0x00000000));
	} else {
		pval.u16 = 0x000c;
		C_PTP_REP (ptp_setdevicepropvalue (params, 0xd208, &pval, PTP_DTC_UINT16));

		C_PTP_REP (ptp_initiatecapture(params, 0x00000000, 0x00000000));
	}
	return GP_OK;
}

#define SONY_ISO_MASK 0x00ffffffU
#define SONY_ISO_NR_SHIFT 24

static void _stringify_Sony_ISO(uint32_t raw_iso, char *buf) {
	char *buf_pos = buf;

	uint32_t base_iso = raw_iso & SONY_ISO_MASK;
	if (base_iso == SONY_ISO_MASK) {
		buf_pos += sprintf(buf_pos,_("Auto ISO"));
	} else {
		buf_pos += sprintf(buf_pos,"%d",base_iso);
	}

	uint8_t noise_reduction = raw_iso >> SONY_ISO_NR_SHIFT;
	if (noise_reduction) {
		buf_pos += sprintf(buf_pos, " ");
		buf_pos += sprintf(buf_pos,_("Multi Frame Noise Reduction"));
		if (noise_reduction == 2) {
			// Distinguish Standard vs High modes for MFNR.
			buf_pos += sprintf(buf_pos,"+");
		}
	}
}

static int _parse_Sony_ISO(const char *buf, uint32_t *raw_iso) {
	const char *s;
	int len;

	if (sscanf(buf, "%d%n", raw_iso, &len) == 0) {
		s = _("Auto ISO");
		len = strlen(s);

		if (strncmp(buf, s, len) != 0) {
			return GP_ERROR_BAD_PARAMETERS;
		}
		*raw_iso = SONY_ISO_MASK;
	}
	buf += len;

	// If this is the end of string, we're done - no MFNR.
	if (*buf == 0)
		return GP_OK;

	// Otherwise value should be followed by space...
	if (*buf != ' ')
		return GP_ERROR_BAD_PARAMETERS;
	buf++;

	// ...and by the actual MFNR string.
	s = _("Multi Frame Noise Reduction");
	len = strlen(s);
	if (strncmp(buf, s, len) != 0)
		return GP_ERROR_BAD_PARAMETERS;
	buf += len;

	uint8_t noise_reduction = 1;

	if (*buf == '+') {
		// If there's a `+`, it's a High mode for MFNR.
		noise_reduction = 2;
		buf++;
	}

	// There is still something unrecognized in the end of the string.
	if (*buf != 0)
		return GP_ERROR_BAD_PARAMETERS;

	*raw_iso |= noise_reduction << SONY_ISO_NR_SHIFT;
	return GP_OK;
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
		_stringify_Sony_ISO(dpd->FORM.Enum.SupportedValue[i].u32, buf);

		gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32) {
			isset=1;
			gp_widget_set_value (*widget,buf);
		}
	}
	if (!isset) {
		_stringify_Sony_ISO(dpd->CurrentValue.u32, buf);
		gp_widget_set_value (*widget,buf);
	}
	return GP_OK;
}

/* old method, using stepping */
static int
_put_Sony_ISO(CONFIG_PUT_ARGS)
{
	const char	*value;
	uint32_t	raw_iso;
	PTPParams	*params = &(camera->pl->params);

	CR (gp_widget_get_value(widget, &value));
	CR (_parse_Sony_ISO(value, &raw_iso));

	propval->u32 = raw_iso;
	*alreadyset = 1;

	return _put_sony_value_u32(params, dpd->DevicePropertyCode, raw_iso, 1);
}

/* new method, can just set the value via setcontroldevicea */
static int
_put_Sony_ISO2(CONFIG_PUT_ARGS)
{
	const char 	*value;
	uint32_t	raw_iso;
	PTPParams	*params = &(camera->pl->params);

	CR (gp_widget_get_value(widget, &value));
	CR (_parse_Sony_ISO(value, &raw_iso));

	propval->u32 = raw_iso;

	*alreadyset = 1;
	return translate_ptp_result (ptp_sony_setdevicecontrolvaluea(params, dpd->DevicePropertyCode, propval, PTP_DTC_UINT32));
}

static int
_put_Sony_QX_ISO(CONFIG_PUT_ARGS)
{
	char 		*value;
	uint32_t	u;

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
		u |= 0x1000000;

setiso:
	propval->u32 = u;

	/*return translate_ptp_result (ptp_sony_qx_setdevicecontrolvaluea(params, dpd->DevicePropertyCode, propval, PTP_DTC_UINT32));*/

	return GP_OK; /* will be set by generic code */
}


static int
_get_Olympus_AspectRatio(CONFIG_GET_ARGS) {
	int i;

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char	buf[20];

		sprintf(buf,"%d:%d",(dpd->FORM.Enum.SupportedValue[i].u32 >> 16), dpd->FORM.Enum.SupportedValue[i].u32 & 0xffff);
		gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32)
			gp_widget_set_value (*widget,buf);
	}
	return GP_OK;
}

static int
_put_Olympus_AspectRatio(CONFIG_PUT_ARGS)
{
	char		*value;
	unsigned int	x,y;

	CR (gp_widget_get_value(widget, &value));

	if (2 == sscanf(value, "%d:%d", &x, &y)) {
		propval->u32 = (x<<16) | y;
		return GP_OK;
	}
	return GP_ERROR;
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
			CR (gp_widget_add_choice (*widget, buf));
			if (	((dpd->DataType == PTP_DTC_UINT32) && (dpd->CurrentValue.u32 == i)) ||
				((dpd->DataType == PTP_DTC_UINT16) && (dpd->CurrentValue.u16 == i))
			   )
				CR (gp_widget_set_value (*widget, buf));

			/* device might report stepsize 0. but we do at least 1 round */
			if (s == 0)
				break;
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

/* the common sony f-numbers */
static int sony_fnumbers[] = {
	100,
	110,
	120,
	140,
	160,
	180,
	200,
	220,
	250,
	280,
	320,
	350,
	400,
	450,
	500,
	560,
	630,
	710,
	800,
	900,
	1000,
	1100,
	1300,
	1400,
	1600,
	1800,
	2000,
	2200,
	2500,
	2900,
	3200,
	3600,
	4200,
	4500,
	5000,
	5700,
	6400,
};

static int
_get_Sony_FNumber(CONFIG_GET_ARGS) {
	unsigned int	i, isset = 0;
	char	buf[20];

	GP_LOG_D ("get_Sony_FNumber");
	if (!(dpd->FormFlag & (PTP_DPFF_Enumeration|PTP_DPFF_Range)))
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_UINT16)
		return GP_ERROR;

	if (dpd->FormFlag & PTP_DPFF_Enumeration)
		return _get_FNumber(CONFIG_GET_NAMES);	/* just use the normal code */

	/* Range */
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i=0;i<sizeof(sony_fnumbers)/sizeof(sony_fnumbers[0]); i++) {
		sprintf(buf,"f/%g",sony_fnumbers[i]/100.0);
		gp_widget_add_choice (*widget,buf);
		if (sony_fnumbers[i] == dpd->CurrentValue.u16) {
			isset = 1;
			gp_widget_set_value (*widget,buf);
		}
	}
	if (!isset) {
		sprintf(buf,"f/%g",dpd->CurrentValue.u16/100.0);
		gp_widget_set_value (*widget,buf);
	}
	GP_LOG_D ("get_Sony_FNumber via range and table");
	return GP_OK;
}


static int
_put_Sony_FNumber(CONFIG_PUT_ARGS)
{
	float		fvalue = 0.0;
	char *		value;
	PTPParams	*params = &(camera->pl->params);

	CR (gp_widget_get_value (widget, &value));
	if (strstr (value, "f/") == value)
		value += strlen("f/");
	if (sscanf(value, "%g", &fvalue))
		propval->u16 = fvalue*100;
	else
		return GP_ERROR;
	*alreadyset = 1;
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
		if (abs((int)(val - dpd->FORM.Enum.SupportedValue[i].u32))<delta) {
			xval = dpd->FORM.Enum.SupportedValue[i].u32;
			delta = abs((int)(val - dpd->FORM.Enum.SupportedValue[i].u32));
		}
	}
	GP_LOG_D ("value %s is %f, closest match was %d",value,val,xval);
	propval->u32 = xval;
	return GP_OK;
}

static int
_get_Video_Framerate(CONFIG_GET_ARGS) {
	char		buf[20];

	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	if (dpd->FormFlag == PTP_DPFF_Enumeration) {
		gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
		/* value will be set below */
	} else {
		if (dpd->FormFlag == PTP_DPFF_Range) {
			gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
			float val = dpd->CurrentValue.u32 / 1000000.0;
			gp_widget_set_value (*widget, &val);
		} else {
			gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
			sprintf (buf, "%0.4f", (1.0*dpd->CurrentValue.u32) / 1000000.0);
			gp_widget_set_value (*widget, buf);
		}
	}

	gp_widget_set_name (*widget, menu->name);

	if (dpd->FormFlag == PTP_DPFF_Enumeration) {
		int		i;

		for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
			sprintf (buf,"%0.4f", (1.0*dpd->FORM.Enum.SupportedValue[i].u32)/1000000.0);
			gp_widget_add_choice (*widget,buf);
			if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32)
				gp_widget_set_value (*widget,buf);
		}
	}
	if (dpd->FormFlag == PTP_DPFF_Range) {
		float b, t, s;

		b = (1.0*dpd->FORM.Range.MinimumValue.u32) / 1000000.0;
		t = (1.0*dpd->FORM.Range.MaximumValue.u32) / 1000000.0;
		s = (1.0*dpd->FORM.Range.StepSize.u32) / 1000000.0;
		gp_widget_set_range (*widget, b, t, s);
	}
	return GP_OK;
}

static int
_put_Video_Framerate(CONFIG_PUT_ARGS)
{
	float		val;
	char		*value;

	if (dpd->FormFlag == PTP_DPFF_Range) {
		CR (gp_widget_get_value (widget, &val));
	} else {
		CR (gp_widget_get_value (widget, &value));

		if (!sscanf(value,_("%f"),&val)) {
			GP_LOG_E ("failed to parse: %s", value);
			return GP_ERROR;
		}
	}
	propval->u32 = val * 1000000;
	return GP_OK;
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
		if (!s) {
			gp_widget_set_value (*widget, "invalid range, stepping 0");
			return GP_OK;
		}
		for (i=min; i<=max; i+=s) {
			char buf[20];

			if (max != min)
				sprintf (buf, "%d%%", (i-min)*100/(max-min));
			else
				strcpy (buf, "range max=min?");
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

			if (max != min)
				sprintf (buf, "%d%%", (x-min)*100/(max-min));
			else
				strcpy (buf, "range max=min?");
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
			if (strcmp (buf, val)) {
				if (s == 0)
					break;
				continue;
			}
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
	{ N_("U1"),		0x8050, PTP_VENDOR_NIKON},
	{ N_("U2"),		0x8051, PTP_VENDOR_NIKON},
	{ N_("U3"),		0x8052, PTP_VENDOR_NIKON},

	{ N_("Intelligent Auto"),		0x8000, PTP_VENDOR_SONY},
	{ N_("Superior Auto"),			0x8001, PTP_VENDOR_SONY},
	{ N_("Movie (P)"),				0x8050, PTP_VENDOR_SONY},
	{ N_("Movie (A)"),				0x8051, PTP_VENDOR_SONY},
	{ N_("Movie (S)"),				0x8052, PTP_VENDOR_SONY},
	{ N_("Movie (M)"),				0x8053, PTP_VENDOR_SONY},
	{ N_("Movie (Scene)"),			0x8054, PTP_VENDOR_SONY},
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
	{ N_("Anti Motion Blur"),		0x8018, PTP_VENDOR_SONY},
	{ N_("Picture Effect"),			0x8070, PTP_VENDOR_SONY},
	{ N_("S&Q"),				0x8084, PTP_VENDOR_SONY}, /* on A7III */
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
	{ "ISO Auto 6400",	0x01, 0 },
	{ "ISO Auto 3200",	0x02, 0 },
	{ "ISO Auto 800",	0x03, 0 },
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
	{ N_("Fluorescent"),		0x02, 0 },
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

static struct deviceproptableu8 nikon_aebracketstep[] = {
	{ N_("1/3 EV"),	0, 0 },
	{ N_("1/2 EV"),	1, 0 },
	{ N_("2/3 EV"),	2, 0 },
	{ N_("1 EV"),	3, 0 },
	{ N_("2 EV"),	4, 0 },
	{ N_("3 EV"),	5, 0 },
};
GENERIC8TABLE(Nikon_AEBracketStep,nikon_aebracketstep);

static struct deviceproptableu8 nikon_wbbracketstep[] = {
	{ N_("1 EV"),	0, 0 },
	{ N_("2 EV"),	1, 0 },
	{ N_("3 EV"),	2, 0 },
};
GENERIC8TABLE(Nikon_WBBracketStep,nikon_wbbracketstep);

static struct deviceproptableu8 nikon_adlbracketstep[] = {
	{ N_("Auto"),		0, 0 },
	{ N_("Low"),		1, 0 },
	{ N_("Normal"),		2, 0 },
	{ N_("High"),		3, 0 },
	{ N_("Extra high"),	4, 0 },
};
GENERIC8TABLE(Nikon_ADLBracketStep,nikon_adlbracketstep);

static struct deviceproptableu8 nikon_bracketpattern[] = {
	{ N_("2 images (normal and under)"),			0, 0 },
	{ N_("2 images (normal and over)"),			1, 0 },
	{ N_("3 images (normal and 2 unders)"),			2, 0 },
	{ N_("3 images (normal and 2 overs)"),			3, 0 },
	{ N_("3 images (normal, under and over)"),		4, 0 },
	{ N_("5 images (normal, 2 unders and 2 overs)"),	5, 0 },
	{ N_("7 images (normal, 3 unders and 3 overs)"),	6, 0 },
	{ N_("9 images (normal, 4 unders and 4 overs)"),	7, 0 },
	{ N_("0 image"),					8, 0 },
};
GENERIC8TABLE(Nikon_BracketPattern,nikon_bracketpattern);

static struct deviceproptableu8 nikon_adlbracketpattern[] = {
	{ N_("2 shots (Off -> User setting)"),				0, 0 },
	{ N_("3 shots (Off -> Low -> User setting)"),			1, 0 },
	{ N_("4 shots (Off -> Low -> Normal -> High)"),			2, 0 },
	{ N_("5 shots (Off -> Low -> Normal -> High -> Extra High)"),	3, 0 },
	{ N_("0 image"),						4, 0 },
};
GENERIC8TABLE(Nikon_ADLBracketPattern,nikon_adlbracketpattern);

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

/* from z6 sdk */
static struct deviceproptableu8 nikon_usermode[] = {
	{ "P",			19, 0 },
	{ "S",			20, 0 },
	{ "A",			21, 0 },
	{ "M",			22, 0 },
	{ N_("Auto"),		23, 0 },
};
GENERIC8TABLE(NIKON_UserMode,nikon_usermode)

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
	{ N_("Continuous Quiet Release"),	0x8018, PTP_VENDOR_NIKON}, /* d850 */

	{ N_("Continuous Low Speed"),	0x8012, PTP_VENDOR_SONY},
	{ N_("Selftimer 2s"),		0x8005, PTP_VENDOR_SONY},
	{ N_("Selftimer 5s"),		0x8003, PTP_VENDOR_SONY},
	{ N_("Selftimer 10s"),		0x8004, PTP_VENDOR_SONY},
	{ N_("Selftimer 10s 3 Pictures"),0x8008, PTP_VENDOR_SONY},
	{ N_("Selftimer 10s 5 Pictures"),0x8009, PTP_VENDOR_SONY},
	{ N_("Selftimer 5s 3 Pictures"),0x800c, PTP_VENDOR_SONY},
	{ N_("Selftimer 5s 5 Pictures"),0x800d, PTP_VENDOR_SONY},
	{ N_("Selftimer 2s 3 Pictures"),0x800e, PTP_VENDOR_SONY},
	{ N_("Selftimer 2s 5 Pictures"),0x800f, PTP_VENDOR_SONY},
	{ N_("Continuous Hi+ Speed"),   0x8010, PTP_VENDOR_SONY}, /* A7III */
	{ N_("Continuous Med Speed"),   0x8015, PTP_VENDOR_SONY}, /* A7III */

	{ N_("Bracketing C 0.3 Steps 3 Pictures"),	0x8337, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.3 Steps 5 Pictures"),	0x8537, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.3 Steps 9 Pictures"),	0x8937, PTP_VENDOR_SONY},

	{ N_("Bracketing C 0.5 Steps 3 Pictures"),	0x8357, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.5 Steps 5 Pictures"),	0x8557, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.5 Steps 9 Pictures"),	0x8957, PTP_VENDOR_SONY},

	{ N_("Bracketing C 0.7 Steps 3 Pictures"),	0x8377, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.7 Steps 5 Pictures"),	0x8577, PTP_VENDOR_SONY},
	{ N_("Bracketing C 0.7 Steps 9 Pictures"),	0x8977, PTP_VENDOR_SONY},

	{ N_("Bracketing C 1.0 Steps 3 Pictures"),	0x8311, PTP_VENDOR_SONY},
	{ N_("Bracketing C 1.0 Steps 5 Pictures"),	0x8511, PTP_VENDOR_SONY},
	{ N_("Bracketing C 1.0 Steps 9 Pictures"),	0x8911, PTP_VENDOR_SONY},

	{ N_("Bracketing C 2.0 Steps 3 Pictures"),	0x8321, PTP_VENDOR_SONY},
	{ N_("Bracketing C 2.0 Steps 5 Pictures"),	0x8521, PTP_VENDOR_SONY},

	{ N_("Bracketing C 3.0 Steps 3 Pictures"),	0x8331, PTP_VENDOR_SONY},
	{ N_("Bracketing C 3.0 Steps 5 Pictures"),	0x8531, PTP_VENDOR_SONY},

	{ N_("Bracketing S 0.3 Steps 3 Pictures"),	0x8336, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.3 Steps 5 Pictures"),	0x8536, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.3 Steps 9 Pictures"),	0x8936, PTP_VENDOR_SONY},

	{ N_("Bracketing S 0.5 Steps 3 Pictures"),	0x8356, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.5 Steps 5 Pictures"),	0x8556, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.5 Steps 9 Pictures"),	0x8956, PTP_VENDOR_SONY},

	{ N_("Bracketing S 0.7 Steps 3 Pictures"),	0x8376, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.7 Steps 5 Pictures"),	0x8576, PTP_VENDOR_SONY},
	{ N_("Bracketing S 0.7 Steps 9 Pictures"),	0x8976, PTP_VENDOR_SONY},

	{ N_("Bracketing S 1.0 Steps 3 Pictures"),	0x8310, PTP_VENDOR_SONY},
	{ N_("Bracketing S 1.0 Steps 5 Pictures"),	0x8510, PTP_VENDOR_SONY},
	{ N_("Bracketing S 1.0 Steps 9 Pictures"),	0x8910, PTP_VENDOR_SONY},

	{ N_("Bracketing S 2.0 Steps 3 Pictures"),	0x8320, PTP_VENDOR_SONY},
	{ N_("Bracketing S 2.0 Steps 5 Pictures"),	0x8520, PTP_VENDOR_SONY},
	{ N_("Bracketing S 3.0 Steps 3 Pictures"),	0x8330, PTP_VENDOR_SONY},
	{ N_("Bracketing S 3.0 Steps 5 Pictures"),	0x8530, PTP_VENDOR_SONY},
	{ N_("Bracketing WB Lo"),	0x8018, PTP_VENDOR_SONY},
	{ N_("Bracketing DRO Lo"),	0x8019, PTP_VENDOR_SONY},
	{ N_("Bracketing WB Hi"),	0x8028, PTP_VENDOR_SONY},
	{ N_("Bracketing DRO Hi"),	0x8029, PTP_VENDOR_SONY},
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

static struct deviceproptableu16 nikon_d850_focus_metering[] = {
	{ N_("Dynamic-area AF (25 points)"),0x0002, 	PTP_VENDOR_NIKON},
	{ N_("Single-point AF"),			0x8010, PTP_VENDOR_NIKON},
	{ N_("Auto-area AF"),				0x8011, PTP_VENDOR_NIKON},
	{ N_("3D-tracking"),				0x8012, PTP_VENDOR_NIKON},
	{ N_("Dynamic-area AF (72 points)"),		0x8013, PTP_VENDOR_NIKON},
	{ N_("Dynamic-area AF (153 points)"),		0x8014, PTP_VENDOR_NIKON},
	{ N_("Group-area AF"),				0x8015, PTP_VENDOR_NIKON},
	{ N_("Dynamic-area AF (9 points)"),		0x8016, PTP_VENDOR_NIKON},

	{ N_("Pinpoint AF"),		0x8017, PTP_VENDOR_NIKON}, /* on Z */
	{ N_("Wide-area AF (S)"),	0x8018, PTP_VENDOR_NIKON}, /* on Z */
	{ N_("Wide-area AF (L)"),	0x8019, PTP_VENDOR_NIKON}, /* on Z */
	{ N_("Wide-area AF (C1)"),	0x801e, PTP_VENDOR_NIKON}, /* on Z */
	{ N_("Wide-area AF (C2)"),	0x801f, PTP_VENDOR_NIKON}, /* on Z */
};
GENERIC16TABLE(Nikon_D850_FocusMetering,nikon_d850_focus_metering)

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
	unsigned int	i;

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

static struct deviceproptableu16 sony_focusarea[] = {
		{ N_("Wide"), 0x01, 0 },
		{ N_("Zone"),	0x02, 0 },
		{ N_("Center"),	0x03, 0 },
		{ N_("Flexible Spot: S"),	0x101, 0 },
		{ N_("Flexible Spot: M"),	0x102, 0 },
		{ N_("Flexible Spot: L"),	0x103, 0 },
		{ N_("Expand Flexible Spot"),	0x104, 0 },
		{ N_("Lock-on AF: Wide"),	0x201, 0 },
		{ N_("Lock-on AF: Zone"),	0x202, 0 },
		{ N_("Lock-on AF: Center"),	0x203, 0 },
		{ N_("Lock-on AF: Flexible Spot: S"),	0x204, 0 },
		{ N_("Lock-on AF: Flexible Spot: M"),	0x205, 0 },
		{ N_("Lock-on AF: Flexible Spot: L"),	0x206, 0 },
		{ N_("Lock-on AF: Expand Flexible Spot"),	0x207, 0 },
};
GENERIC16TABLE(Sony_FocusArea,sony_focusarea)

static struct deviceproptableu16 exposure_metering[] = {
	{ N_("Average"),	0x0001, 0 },
	{ N_("Center Weighted"),0x0002, 0 },
	{ N_("Multi Spot"),	0x0003, 0 },
	{ N_("Center Spot"),	0x0004, 0 },
	{ N_("Spot"),		0x8001, PTP_VENDOR_FUJI },
	{ N_("ESP"),		0x8001, PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Spot+Highlights"),0x8011, PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("Spot+Shadows"),	0x8012, PTP_VENDOR_GP_OLYMPUS_OMD },
	/* next ones taken from A7III */
	{ N_("Multi"),			0x8001, PTP_VENDOR_SONY },
	{ N_("Center"),			0x8002, PTP_VENDOR_SONY },
	{ N_("Entire Screen Avg."),	0x8003, PTP_VENDOR_SONY },
	{ N_("Spot Standard"),		0x8004, PTP_VENDOR_SONY },
	{ N_("Spot Large"),		0x8005, PTP_VENDOR_SONY },
	{ N_("Highlight"),		0x8006, PTP_VENDOR_SONY },
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
	unsigned int i;
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
		uint32_t diff = abs((int)(dpd->FORM.Enum.SupportedValue[i].u32  - propval->u32));

		if (diff < curdiff) {
			newval = dpd->FORM.Enum.SupportedValue[i].u32;
			curdiff = diff;
		}
	}
	propval->u32 = newval;
	return GP_OK;
}

static int
_get_VideoFormat(CONFIG_GET_ARGS) {
	int i, valset = 0;
	char buf[200];

	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;

	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	/* We use FOURCC values, which should be 4 characters always */

	for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
		sprintf (buf, "%c%c%c%c",
			(dpd->FORM.Enum.SupportedValue[i].u32     )  & 0xff,
			(dpd->FORM.Enum.SupportedValue[i].u32 >> 8)  & 0xff,
			(dpd->FORM.Enum.SupportedValue[i].u32 >> 16) & 0xff,
			(dpd->FORM.Enum.SupportedValue[i].u32 >> 24) & 0xff
		);
		gp_widget_add_choice (*widget,buf);
		if (dpd->CurrentValue.u32 == dpd->FORM.Enum.SupportedValue[i].u32) {
			gp_widget_set_value (*widget, buf);
			valset = 1;
		}
	}
	if (!valset) {
		sprintf (buf, "%c%c%c%c",
			(dpd->CurrentValue.u32     )  & 0xff,
			(dpd->CurrentValue.u32 >> 8)  & 0xff,
			(dpd->CurrentValue.u32 >> 16) & 0xff,
			(dpd->CurrentValue.u32 >> 24) & 0xff
		);
		sprintf (buf, _("%d mm"), dpd->CurrentValue.u16);
		gp_widget_set_value (*widget, buf);
	}
	return GP_OK;
}

static int
_put_VideoFormat(CONFIG_PUT_ARGS) {
	const unsigned char *value_str;

	CR (gp_widget_get_value (widget, &value_str));
	if (strlen((char*)value_str) < 4)
		return GP_ERROR_BAD_PARAMETERS;
	/* we could check if we have it in the ENUM */
	propval->u32 = value_str[0] | (value_str[1] << 8) | (value_str[2] << 16) | (value_str[3] << 24);
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
_get_Olympus_ShutterSpeed(CONFIG_GET_ARGS) {
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
		if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xfffffffc) {
			sprintf(buf,_("Bulb"));
			goto choicefound;
		}
		if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xfffffffa) {
		 	sprintf(buf,_("Composite"));
		 	goto choicefound;
		 }
		if (dpd->FORM.Enum.SupportedValue[i].u32 == 0xfffffffb) {
			sprintf(buf,_("Time"));
			goto choicefound;
		}

		x = dpd->FORM.Enum.SupportedValue[i].u32>>16;
		y = dpd->FORM.Enum.SupportedValue[i].u32&0xffff;

		if (((y % 10) == 0) && ((x % 10) == 0)) {
			y /= 10;
			x /= 10;
		}
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
_put_Olympus_ShutterSpeed(CONFIG_PUT_ARGS) {
	int x,y;
	const char *value_str;

	gp_widget_get_value (widget, &value_str);

	if (!strcmp(value_str,_("Bulb"))) {
		propval->u32 = 0xfffffffc;
		return GP_OK;
	}
	if (!strcmp(value_str,_("Composite"))) {
	 	propval->u32 = 0xfffffffa;
	 	return GP_OK;
	 }
	if (!strcmp(value_str,_("Time"))) {
		propval->u32 = 0xfffffffb;
		return GP_OK;
	}

	if (strchr(value_str, '/')) {
		if (2 != sscanf (value_str, "%d/%d", &x, &y))
			return GP_ERROR;
	} else {
		if (!sscanf (value_str, "%d", &x))
			return GP_ERROR;
		y = 10;
		x *=10;
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
	propval->u64 = ((uint64_t)x<<32) | y;
	return GP_OK;
}

struct sigma_aperture {
	uint8_t		numval;
	const char*	val;
} sigma_apertures[] = {
	{8,	"1.0"},
	{11,	"1.1"},
	{13,	"1.2"},
	{16,	"1.4"},
	{19,	"1.6"},
	{21,	"1.8"},
	{24,	"2.0"},
	{27,	"2.2"},
	{29,	"2.5"},
	{32,	"2.8"},
	{35,	"3.2"},
	{40,	"4.0"},
	{43,	"4.5"},
	{45,	"5.0"},
	{48,	"5.6"},
	{51,	"6.3"},
	{53,	"7.1"},
	{56,	"8.0"},
	{59,	"9.0"},
	{61,	"10"},
	{64,	"11"},
	{67,	"13"},
	{69,	"14"},
	{72,	"16"},
	{75,	"18"},
	{77,	"20"},
	{80,	"22"},
	{83,	"25"},
	{85,	"29"},
	{88,	"32"},
	{91,	"36"},
	{93,	"40"},
	{96,	"45"},
	{99,	"51"},
	{101,	"57"},
	{104,	"64"},
	{107,	"72"},
	{109,	"81"},
	{112,	"91"},
};

static int
_get_SigmaFP_Aperture(CONFIG_GET_ARGS) {
	char		buf[200];
	unsigned char	*xdata = NULL;
	unsigned int	xsize = 0;
	unsigned int	i, valset = 0;
	unsigned int	aperture;
	PTPParams	*params = &(camera->pl->params);

	/* shutterspeed is in datagroup1, bit 1 presence, offset 4 */
	C_PTP (ptp_sigma_fp_getdatagroup1 (params, &xdata, &xsize));

	/* byte 0: 	count of bytes
	 * byte 1,2: 	bitmask of presence
	 * byte 3... 	start
	 * byte[last]	checksum
	 */

	if (!(xdata[1] & (1<<1))) {
		free (xdata);
		return GP_ERROR;
	}
	aperture = xdata[4];
	free (xdata);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i=0;i<sizeof(sigma_apertures)/sizeof(sigma_apertures[0]);i++) {
		gp_widget_add_choice (*widget, _(sigma_apertures[i].val));
		if (aperture == sigma_apertures[i].numval) {
			gp_widget_set_value (*widget, _(sigma_apertures[i].val));
			valset = 1;
		}
	}
	if (!valset) {
		sprintf(buf,"unknown value 0x%x", aperture);
		gp_widget_set_value (*widget, buf);
	}
	return GP_OK;
}

static int
_put_SigmaFP_Aperture(CONFIG_PUT_ARGS) {
	const char	*value_str;
	unsigned char	datagrp1[22];
	unsigned int	aperture = 0;
	unsigned int	i, chksum = 0, valfound = 0;
	PTPParams	*params = &(camera->pl->params);

	gp_widget_get_value (widget, &value_str);
	memset(datagrp1,0,sizeof(datagrp1));

	for (i=0;i<sizeof(sigma_apertures)/sizeof(sigma_apertures[0]);i++) {
		if (!strcmp(value_str,_(sigma_apertures[i].val))) {
			aperture = sigma_apertures[i].numval;
			valfound = 1;
			break;
		}
	}
	if (!valfound && !sscanf (value_str, "unknown value 0x%x", &aperture)) {
		return GP_ERROR;
	}
	datagrp1[0] = 0x13;
	datagrp1[1] = (1<<1);
	datagrp1[2] = 0;
	datagrp1[3] = 0;
	datagrp1[4] = aperture;
	chksum = 0;
	for (i=0;i<21;i++)
		chksum+=datagrp1[i];
	datagrp1[21] = chksum & 0xff;
	C_PTP (ptp_sigma_fp_setdatagroup1 (params, datagrp1, 22));
	return GP_OK;
}

struct sigma_shutterspeed {
	uint8_t		numval;
	const char*	val;
} sigma_shutterspeeds[] = {
	{8,	N_("bulb")	},
	{16,	"30"		},
	{19,	"25"		},
	{21,	"20"		},
	{24,	"15"		},
	{27,	"13"		},
	{29,	"10"		},
	{32,	"8"		},
	{35,	"6"		},
	{37,	"5"		},
	{40,	"4"		},
	{43,	"3.2"		},
	{44,	"3"		},	/* 1/2 */
	{45,	"2.5"		},
	{48,	"2"		},
	{51,	"1.6"		},
	{53,	"1.3"		},
	{56,	"1"		},
	{59,	"0.8"		},
	{61,	"0.6"		},
	{64,	"0.5"		},
	{67,	"0.4"		},
	{69,	"0.3"		},
	{72,	"1/4"		},
	{75,	"1/5"		},
	{77,	"1/6"		},
	{80,	"1/8"		},
	{83,	"1/10"		},
	{85,	"1/13"		},
	{88,	"1/15"		},
	{91,	"1/20"		},
	{93,	"1/25"		},
	{96,	"1/30"		},
	{99,	"1/40"		},
	{101,	"1/50"		},
	{104,	"1/60"		},
	{107,	"1/80"		},
	{109,	"1/100"		},
	{112,	"1/125"		},
	{115,	"1/160"		},
	{117,	"1/200"		},
	{120,	"1/250"		},
	{123,	"1/320"		},
	{125,	"1/400"		},
	{128,	"1/500"		},
	{131,	"1/640"		},
	{133,	"1/800"		},
	{136,	"1/1000"	},
	{139,	"1/1250"	},
	{141,	"1/1600"	},
	{144,	"1/2000"	},
	{147,	"1/2500"	},
	{149,	"1/3200"	},
	{152,	"1/4000"	},
	{155,	"1/5000"	},
	{157,	"1/6000"	},
	{160,	"1/8000"	},
	{162,	N_("Sync")	},
	{163,	"1/10000"	},
	{165,	"1/12800"	},
	{168,	"1/16000"	},
	{171,	"1/20000"	},
	{173,	"1/25600"	},
	{176,	"1/32000"	},
};

static int
_get_SigmaFP_ShutterSpeed(CONFIG_GET_ARGS) {
	char		buf[200];
	unsigned char	*xdata = NULL;
	unsigned int	xsize = 0;
	unsigned int	i, valset = 0;
	unsigned int	shutterspeed;
	PTPParams	*params = &(camera->pl->params);

	/* shutterspeed is in datagroup1, bit 0 presence */
	C_PTP (ptp_sigma_fp_getdatagroup1 (params, &xdata, &xsize));

	/* byte 0: 	count of bytes
	 * byte 1,2: 	bitmask of presence
	 * byte 3... 	start
	 * byte[last]	checksum
	 */

	if (!(xdata[1] & (1<<0))) {
		free (xdata);
		return GP_ERROR;
	}
	shutterspeed = xdata[3];
	free (xdata);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i=0;i<sizeof(sigma_shutterspeeds)/sizeof(sigma_shutterspeeds[0]);i++) {
		gp_widget_add_choice (*widget, _(sigma_shutterspeeds[i].val));
		if (shutterspeed == sigma_shutterspeeds[i].numval) {
			gp_widget_set_value (*widget, _(sigma_shutterspeeds[i].val));
			valset = 1;
		}
	}
	if (!valset) {
		sprintf(buf,"unknown value 0x%x", shutterspeed);
		gp_widget_set_value (*widget, buf);
	}
	return GP_OK;
}

static int
_put_SigmaFP_ShutterSpeed(CONFIG_PUT_ARGS) {
	const char	*value_str;
	unsigned char	datagrp1[22];
	unsigned int	shutterspeed = 0;
	unsigned int	i, chksum = 0, valfound = 0;
	PTPParams	*params = &(camera->pl->params);

	gp_widget_get_value (widget, &value_str);
	memset(datagrp1,0,sizeof(datagrp1));

	for (i=0;i<sizeof(sigma_shutterspeeds)/sizeof(sigma_shutterspeeds[0]);i++) {
		if (!strcmp(value_str,_(sigma_shutterspeeds[i].val))) {
			shutterspeed = sigma_shutterspeeds[i].numval;
			valfound = 1;
			break;
		}
	}
	if (!valfound && !sscanf (value_str, "unknown value 0x%x", &shutterspeed)) {
		return GP_ERROR;
	}
	datagrp1[0] = 0x13;
	datagrp1[1] = (1<<0);
	datagrp1[2] = 0;
	datagrp1[3] = shutterspeed;
	chksum = 0;
	for (i=0;i<21;i++)
		chksum+=datagrp1[i];
	datagrp1[21] = chksum & 0xff;
	C_PTP (ptp_sigma_fp_setdatagroup1 (params, datagrp1, 22));
	return GP_OK;
}

/* This list is taken from Sony A58... fill in more if your Sony has more */
static struct sonyshutter {
	int dividend, divisor;
} sony_shuttertable[] = {
	{30,1},
	{25,1},
	{20,1},
	{15,1},
	{13,1},
	{10,1},
	{8,1},
	{6,1},
	{5,1},
	{4,1},
	{32,10},
	{25,10},
	{2,1},
	{16,10},
	{13,10},
	{1,1},
	{8,10},
	{6,10},
	{5,10},
	{4,10},
	{1,3},
	{1,4},
	{1,5},
	{1,6},
	{1,8},
	{1,10},
	{1,13},
	{1,15},
	{1,20},
	{1,25},
	{1,30},
	{1,40},
	{1,50},
	{1,60},
	{1,80},
	{1,100},
	{1,125},
	{1,160},
	{1,200},
	{1,250},
	{1,320},
	{1,400},
	{1,500},
	{1,640},
	{1,800},
	{1,1000},
	{1,1250},
	{1,1600},
	{1,2000},
	{1,2500},
	{1,3200},
	{1,4000},
	/* A7 series */
	{1,5000},
	{1,6400},
	{1,8000},
	/* A9, some RX series cameras */
	{1,10000},
	{1,12500},
	{1,16000},
	{1,20000},
	{1,25000},
	{1,32000},
};

static int
_get_Sony_ShutterSpeed(CONFIG_GET_ARGS) {
	int			x,y;
	char			buf[20];
	PTPParams		*params = &(camera->pl->params);
	GPContext 		*context = ((PTPData *) params->data)->context;

	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;

	if (have_prop (camera, PTP_VENDOR_SONY, PTP_DPC_SONY_ShutterSpeed2))
		C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_SONY_ShutterSpeed2, dpd));

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	/* new style has an ENUM again */
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		unsigned int i;

		for (i=0;i<dpd->FORM.Enum.NumberOfValues;i++) {
			x = dpd->FORM.Enum.SupportedValue[i].u32 >> 16;
			y = dpd->FORM.Enum.SupportedValue[i].u32 & 0xffff;

			// Sony reports high shutter speeds as 300/10, 250/10, etc.
			// Normalize them to integers for better output and to match
			// `sony_shuttertable` format.
			if (y == 10 && x % 10 == 0) {
				x /= 10;
				y = 1;
			}

			if (y == 1)
				sprintf (buf, "%d",x);
			else
				sprintf (buf, "%d/%d",x,y);
			gp_widget_add_choice (*widget, buf);
		}
		gp_widget_add_choice (*widget, _("Bulb"));
	} else {
		unsigned int i;
		/* use our static table */
		for (i=0;i<sizeof(sony_shuttertable)/sizeof(sony_shuttertable[0]);i++) {
			x = sony_shuttertable[i].dividend;
			y = sony_shuttertable[i].divisor;
			if (y == 1)
				sprintf (buf, "%d",x);
			else
				sprintf (buf, "%d/%d",x,y);
			gp_widget_add_choice (*widget, buf);
		}
		gp_widget_add_choice (*widget, _("Bulb"));
	}

	if (dpd->CurrentValue.u32 == 0) {
		strcpy(buf,_("Bulb"));
	} else {
		x = dpd->CurrentValue.u32>>16;
		y = dpd->CurrentValue.u32&0xffff;

		// Normalize current value from 300/10, 250/10 etc. to integers too.
		if (y == 10 && x % 10 == 0) {
			x /= 10;
			y = 1;
		}

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
	int			x,y,a,b,direction,position_current,position_new;
	const char		*val;
	float 			old,new,current;
	PTPPropertyValue	value;
	uint32_t		new32, origval;
	PTPParams		*params = &(camera->pl->params);
	GPContext 		*context = ((PTPData *) params->data)->context;
	time_t			start,end;
	unsigned int		i;

	CR (gp_widget_get_value (widget, &val));

	if (dpd->CurrentValue.u32 == 0) {
		x = 65536; y = 1;
	} else {
		x = dpd->CurrentValue.u32>>16;
		y = dpd->CurrentValue.u32&0xffff;
	}
	old = ((float)x)/(float)y;
	current = old;

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
	/* new style */
	if (have_prop (camera, PTP_VENDOR_SONY, PTP_DPC_SONY_ShutterSpeed2)) {
		propval->u32 = new32;
		return translate_ptp_result (ptp_sony_setdevicecontrolvaluea(params, PTP_DPC_SONY_ShutterSpeed2, propval, PTP_DTC_UINT32));
	}
	/* old style uses stepping */

	new = ((float)x)/(float)y;

	if (old > new) {
		value.u8 = 0x01;
		direction = 1;
	}
	else {
		value.u8 = 0xff;
		direction = -1;
	}

	if (direction == 1) {
		position_new = sizeof(sony_shuttertable)/sizeof(sony_shuttertable[0])-1;

		for (i=0;i<sizeof(sony_shuttertable)/sizeof(sony_shuttertable[0]);i++) {
			a = sony_shuttertable[i].dividend;
			b = sony_shuttertable[i].divisor;
			position_new = i;
			if (new >= ((float)a)/(float)b)
				break;
		}
	} else {
		position_new = 0;

		for (i=sizeof(sony_shuttertable)/sizeof(sony_shuttertable[0])-1;i--;) {
			a = sony_shuttertable[i].dividend;
			b = sony_shuttertable[i].divisor;
			position_new = i;
			if (new <= ((float)a)/(float)b)
				break;
		}
	}

	do {
		origval = dpd->CurrentValue.u32;
		if (old == new)
			break;

		for (i=0;i<sizeof(sony_shuttertable)/sizeof(sony_shuttertable[0]);i++) {
			a = sony_shuttertable[i].dividend;
			b = sony_shuttertable[i].divisor;
			position_current = i;
			if (current >= ((float)a)/(float)b)
				break;
		}

		/* If something failed, fall back to single step, https://github.com/gphoto/libgphoto2/issues/694 */
		if (position_current == position_new) {
			GP_LOG_D ("posNew and pos_current both %d, fall back to single step", position_current);
			if (old > new) {
				value.u8 = 0x01;
				direction = 1;
			}
			else {
				value.u8 = 0xff;
				direction = -1;
			}
		} else {
			// Calculating jump width
			if (direction > 0)
				value.u8 = 0x00 + position_new - position_current;
			else
				value.u8 = 0x100 + position_new - position_current;
		}

		a = dpd->CurrentValue.u32>>16;
		b = dpd->CurrentValue.u32&0xffff;
		C_PTP_REP (ptp_sony_setdevicecontrolvalueb (params, dpd->DevicePropertyCode, &value, PTP_DTC_UINT8 ));

		GP_LOG_D ("shutterspeed value is (0x%x vs target 0x%x)", origval, new32);

		/* we tell the camera to do it, but it takes around 0.7 seconds for the SLT-A58 */
		time(&start);
		do {
			C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
			C_PTP_REP (ptp_generic_getdevicepropdesc (params, dpd->DevicePropertyCode, dpd));

			if (dpd->CurrentValue.u32 == new32) {
				GP_LOG_D ("Value matched!");
				break;
			}
			a = dpd->CurrentValue.u32>>16;
			b = dpd->CurrentValue.u32&0xffff;
			current = ((float)a)/((float)b);

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

		if (direction > 0 && current <= new) {
			GP_LOG_D ("Overshooted value, maybe choice not available!");
			break;
		}
		if (direction < 0 && current >= new) {
			GP_LOG_D ("Overshooted value, maybe choice not available!");
			break;
		}

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
	*alreadyset = 1;
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
_get_Olympus_Aperture(CONFIG_GET_ARGS) {
	char	len[20];
	int	i;

	if (dpd->DataType != PTP_DTC_UINT16)
		return GP_ERROR;
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		sprintf (len, "%.1f", dpd->FORM.Enum.SupportedValue[i].u16 * 0.1);
		gp_widget_add_choice (*widget, len);
	}
	sprintf (len, "%.1f", dpd->CurrentValue.u16 * 0.1);
	gp_widget_set_value (*widget, len);
	return GP_OK;
}

static int
_put_Olympus_Aperture(CONFIG_PUT_ARGS) {
	char	*val;
	float	f;

	gp_widget_get_value (widget, &val);
	sscanf (val, "%f", &f);
	propval->u16 = 10*f;
	return GP_OK;
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
		return GP_ERROR;
	if (dpd->DataType != PTP_DTC_INT8)
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	gp_widget_set_range (*widget,
		dpd->FORM.Range.MinimumValue.i8/6.0,
		dpd->FORM.Range.MaximumValue.i8/6.0,
		dpd->FORM.Range.StepSize.i8/6.0
	);
	value_float = dpd->CurrentValue.i8/6.0;
	gp_widget_set_value (*widget, &value_float);
	return GP_OK;
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

	if (dpd->DataType != PTP_DTC_INT32)
		return (GP_ERROR);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
		sprintf (buf, "%d", dpd->FORM.Enum.SupportedValue[i].i32);
		gp_widget_add_choice (*widget,buf);
		if (dpd->CurrentValue.i32 == dpd->FORM.Enum.SupportedValue[i].i32) {
			gp_widget_set_value (*widget, buf);
			valset = 1;
		}
	}
	if (!valset) {
		sprintf (buf, "%d",dpd->CurrentValue.i32);
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
	propval->i32 = x;
	return GP_OK;
}

static struct deviceproptableu8 nikon_liveviewaf[] = {
	{ N_("Face-priority AF"),	0, 0 },
	{ N_("Wide-area AF"),		1, 0 },
	{ N_("Normal-area AF"),		2, 0 },
	{ N_("Subject-tracking AF"),	3, 0 },
	{ N_("Spot-area AF"),		4, 0 },
};
GENERIC8TABLE(Nikon_LiveViewAFU,nikon_liveviewaf)

/* varies between nikons */
static struct deviceproptableu8 nikon_liveviewimagezoomratio[] = {
	{ N_("Entire Display"),	0, 0 },
	{ N_("25%"),		2, 0 },
	{ N_("50%"),		4, 0 },
	{ N_("100%"),		6, 0 },
	{ N_("200%"),		7, 0 },
};
GENERIC8TABLE(Nikon_LiveViewImageZoomRatio,nikon_liveviewimagezoomratio)

/* varies between nikons */
static struct deviceproptableu8 nikon_liveviewimagezoomratio_d5000[] = {
	{ N_("Entire Display"),	0, 0 },
	{ N_("25%"),		1, 0 },
	{ N_("33%"),		2, 0 },
	{ N_("50%"),		3, 0 },
	{ N_("66%"),		4, 0 },
	{ N_("100%"),		5, 0 },
};
GENERIC8TABLE(Nikon_LiveViewImageZoomRatio_D5000,nikon_liveviewimagezoomratio_d5000)

static struct deviceproptablei8 nikon_liveviewafi[] = {
	{ N_("Face-priority AF"),	0, 0 },
	{ N_("Wide-area AF"),		1, 0 },
	{ N_("Normal-area AF"),		2, 0 },
	{ N_("Subject-tracking AF"),	3, 0 },
	{ N_("Spot-area AF"),		4, 0 },
};
GENERICI8TABLE(Nikon_LiveViewAFI,nikon_liveviewafi)

static struct deviceproptableu8 nikon_liveviewaffocus[] = {
	{ N_("Single-servo AF"),		0, 0 },
	{ N_("Continuous-servo AF"),		1, 0 },
	{ N_("Full-time-servo AF"),		2, 0 },
	{ N_("Manual Focus (fixed)"),		3, 0 },
	{ N_("Manual Focus (selection)"),	4, 0 },
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

static struct deviceproptableu8 nikon_d850_centerweight[] = {
	{ N_("8 mm"),	0x00, 0 },
	{ N_("12 mm"),	0x01, 0 },
	{ N_("15 mm"),	0x02, 0 },
	{ N_("20 mm"),	0x03, 0 },
	{ N_("Average"),0x04, 0 },
};
GENERIC8TABLE(Nikon_D850_CenterWeight,nikon_d850_centerweight)

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

static struct deviceproptableu32 fuji_new_shutterspeed[] = {
	{ N_("bulb"),	0xffffffff, 0 },
	{ "60m",	64000180, 0 },
	{ "30m",	64000150, 0 },
	{ "15m",	64000120, 0 },
	{ "8m",		64000090, 0 },
	{ "4m",		64000060, 0 },
	{ "2m",		64000030, 0 },
	{ "60s",	64000000, 0 },
	{ "50s",	50796833, 0 },
	{ "40s",	40317473, 0 },
	{ "30s",	32000000, 0 },
	{ "25s",	25398416, 0 },
	{ "20s",	20158736, 0 },
	{ "15s",	16000000, 0 },
	{ "13s",	12699208, 0 },
	{ "10s",	10079368, 0 },
	{ "8s",		8000000, 0 },
	{ "6s",		6349604, 0 },
	{ "5s",		5039684, 0 },
	{ "4s",		4000000, 0 },
	{ "3s",		3174802, 0 },
	{ "2.5s",	2519842, 0 },
	{ "2s",		2000000, 0 },
	{ "1.6s",	1587401, 0 },
	{ "1.3s",	1259921, 0 },
	{ "1s",		1000000, 0 },
	{ "0.8s",	793700, 0 },
	{ "0.6s",	629960, 0 },
	{ "1/2",	500000, 0 },
	{ "0.4s",	396850, 0 },
	{ "1/3",	314980, 0 },
	{ "1/4",	250000, 0 },
	{ "1/5",	198425, 0 },
	{ "1/6",	157490, 0 },
	{ "1/8",	125000, 0 },
	{ "1/10",	99212, 0 },
	{ "1/13",	78745, 0 },
	{ "1/15",	62500, 0 },
	{ "1/20",	49606, 0 },
	{ "1/25",	39372, 0 },
	{ "1/30",	31250, 0 },
	{ "1/40",	24803, 0 },
	{ "1/50",	19686, 0 },
	{ "1/60",	15625, 0 },
	{ "1/80",	12401, 0 },
	{ "1/100",	9843, 0 },
	{ "1/125",	7812, 0 },
	{ "1/160",	6200, 0 },
	{ "1/200",	4921, 0 },
	{ "1/250",	3906, 0 },
	{ "1/320",	3100, 0 },
	{ "1/400",	2460, 0 },
	{ "1/500",	1953, 0 },
	{ "1/640",	1550, 0 },
	{ "1/800",	1230, 0 },
	{ "1/1000",	976, 0 },
	{ "1/1250",	775, 0 },
	{ "1/1600",	615, 0 },
	{ "1/2000",	488, 0 },
	{ "1/2500",	387, 0 },
	{ "1/3200",	307, 0 },
	{ "1/4000",	244, 0 },
	{ "1/5000",	193, 0 },
	{ "1/6400",	153, 0 },
	{ "1/8000",	122, 0 },
	{ "1/10000",	96, 0 },
	{ "1/13000",	76, 0 },
	{ "1/16000",	61, 0 },
	{ "1/20000",	48, 0 },
	{ "1/25000",	38, 0 },
	{ "1/32000",	30, 0 },
};
GENERIC32TABLE(Fuji_New_ShutterSpeed,fuji_new_shutterspeed)

static struct deviceproptableu8 nikon_remotetimeout[] = {
	{ N_("1 minute"),	0x00,	0 },
	{ N_("5 minutes"),	0x01,	0 },
	{ N_("10 minutes"),	0x02,	0 },
	{ N_("15 minutes"),	0x03,	0 },
};
GENERIC8TABLE(Nikon_RemoteTimeout,nikon_remotetimeout)

static struct deviceproptableu8 nikon_optimizeimage[] = {
	{ N_("Normal"),		0x00,	0 },
	{ N_("Vivid"),		0x01,	0 },
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
	{ N_("AF-F"),		0x8013, PTP_VENDOR_NIKON },
	{ N_("Single-Servo AF"),0x8001, PTP_VENDOR_FUJI },
	{ N_("Continuous-Servo AF"),0x8002, PTP_VENDOR_FUJI },

	{ N_("C-AF"),		0x8002, PTP_VENDOR_GP_OLYMPUS_OMD },
	{ N_("S-AF+MF"),	0x8001, PTP_VENDOR_GP_OLYMPUS_OMD },

	{ N_("AF-A"),		0x8005, PTP_VENDOR_SONY },
	{ N_("AF-C"),		0x8004, PTP_VENDOR_SONY },
	{ N_("DMF"),		0x8006, PTP_VENDOR_SONY },

};
GENERIC16TABLE(FocusMode,focusmodes)

/* Sony specific, we need to wait for it settle (around 1 second), otherwise we get trouble later on */
static int
_put_Sony_FocusMode(CONFIG_PUT_ARGS) {
	PTPParams		*params = &(camera->pl->params);
	GPContext 		*context = ((PTPData *) params->data)->context;
	int 			ret;
	PTPDevicePropDesc	dpd2;
	time_t			start,end;

	ret = _put_FocusMode(CONFIG_PUT_NAMES);
	if (ret != GP_OK) return ret;
	start = time(NULL);
	C_PTP_REP (ptp_generic_setdevicepropvalue (params, PTP_DPC_FocusMode, propval, PTP_DTC_UINT16));
	while (1) {
		C_PTP_REP (ptp_sony_getalldevicepropdesc (params));
		C_PTP_REP (ptp_generic_getdevicepropdesc (params, PTP_DPC_FocusMode, &dpd2));
		if (dpd2.CurrentValue.u16 == propval->u16)
			break;
		end = time(NULL);
		if (end-start >= 3) {
			GP_LOG_E("failed to change variable to %d (current %d)\n", propval->u16, dpd2.CurrentValue.u16);
			break;
		}
	}
	*alreadyset = 1;
	return GP_OK;
}


static struct deviceproptableu32 eos_focusmodes[] = {
	{ N_("One Shot"),	0x0000, 0 },
	{ N_("AI Servo"),	0x0001, 0 },
	{ N_("AI Focus"),	0x0002, 0 },
	{ N_("Manual"),		0x0003, 0 },
};
GENERIC32TABLE(Canon_EOS_FocusMode,eos_focusmodes)

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
	/*{ N_("Fluorescent H"),		14, 0 }, ... dup? */
};
GENERIC8TABLE(Canon_WhiteBalance,canon_whitebalance)

/* check against SDK */
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
	{ N_("Color Temperature"),9, 0 },
	{ N_("Custom Whitebalance: PC-1"),		10, 0 },
	{ N_("Custom Whitebalance: PC-2"),		11, 0 },
	{ N_("Custom Whitebalance: PC-3"),		12, 0 },
	{ N_("Manual 2"),	15, 0 },
	{ N_("Manual 3"),	16, 0 },
	{ N_("Manual 4"),	18, 0 },
	{ N_("Manual 5"),	19, 0 },
	{ N_("Custom Whitebalance: PC-4"),		20, 0 },
	{ N_("Custom Whitebalance: PC-5"),		21, 0 },
	{ N_("AWB White"),	23, 0 },
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
	{ "4.5",	0x24, 0 },
	{ "4.3",	0x23, 0 },
	{ "4",		0x20, 0 },
	{ "3.6",	0x1d, 0 },
	{ "3.5",	0x1c, 0 },
	{ "3.3",	0x1b, 0 },
	{ "3",		0x18, 0 },
	{ "2.6",	0x15, 0 },
	{ "2.5",	0x14, 0 },
	{ "2.3",	0x13, 0 },
	{ "2",		0x10, 0 },
	{ "1.6",	0x0d, 0 },
	{ "1.5",	0x0c, 0 },
	{ "1.3",	0x0b, 0 },
	{ "1",		0x08, 0 },
	{ "1.0",	0x08, 0 },
	{ "0.6",	0x05, 0 },
	{ "0.5",	0x04, 0 },
	{ "0.3",	0x03, 0 },
	{ "0",		0x00, 0 },
	{ "-0.3",	0xfd, 0 },
	{ "-0.5",	0xfc, 0 },
	{ "-0.6",	0xfb, 0 },
	{ "-1",		0xf8, 0 },
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
	{ "-3.5",	0xe4, 0 },
	{ "-3.6",	0xe3, 0 },
	{ "-4",		0xe0, 0 },
	{ "-4.3",	0xdd, 0 }, /*Canon EOS 5D Mark III*/
	{ "-4.5",	0xdc, 0 },
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


/* FIXME: actually uint32 in SDK doc? also non-standard type in debuglogs */
static struct deviceproptableu16 canon_bracketmode[] = {
	{ N_("AE bracket"),	1, 0 },
	{ N_("ISO bracket"),	2, 0 },
	{ N_("WB bracket"),	4, 0 },
	{ N_("FE bracket"),	8, 0 },
	{ N_("Bracket off"),	0xffff, 0 },
};
GENERIC16TABLE(Canon_BracketMode,canon_bracketmode)

static struct deviceproptableu16 canon_aperture[] = {
	{ N_("implicit auto"),	0x0, 0 },
	{ N_("auto"),	0xffff, 0 },
	{ N_("auto"),	0x00b0, 0 },
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

/* The j5 only reports some of those, there is no clear pattern... fill in with more 1 series */
static struct deviceproptableu8 nikon_1_aperture[] = {
	/* 1 */
	/* 1.1 */
	/* 1.2 */
	/* 1.3 */
	/* 1.4 */
	/* 1.5 */
	/* 1.6 */
	/* 1.7 */
	{ "1.8",	10, 0 },
	/* 1.9 */
	{ "2",		12, 0 },
	{ "2.2",	14, 0 },
	/* 2.4 */
	{ "2.5",	16, 0 },
	/* 2.7 */
	{ "2.8",	18, 0 },
	/* 3 */
	{ "3.2",	20, 0 },
	{ "3.5",	22, 0 },
	/* 3.8 */
	{ "4",		24, 0 },
	/* 4.2 */
	{ "4.5",	26, 0 },
	/* 4.8 */
	{ "5",		28, 0 },
	/* 5.3 */
	{ "5.6",	30, 0 },
	/* 6 */
	{ "6.3",	32, 0 },
	/* 6.7 */
	{ "7.1",	34, 0 },
	/* 7.6 */
	{ "8",		36, 0 },
	/* 8.5 */
	{ "9", 		38, 0 },
	/* 9.5 */
	{ "10", 	40, 0 },
	{ "11", 	42, 0 },
	/* 12 */
	{ "13",		44, 0 },
	{ "14", 	46, 0 },
	/* 15 */
	{ "16",		48, 0 },
	/* 17 */
	/* 18 */
	/* 19 */
	/* 20 */
	/* 21 */
	/* 22 */
	/* 24 */
	/* 25 */
	/* 27.6 */
	/* 29 */
	/* 30 */
	/* 32 */
	/* 0 */
};
GENERIC8TABLE(Nikon_1_Aperture,nikon_1_aperture)

/* The j5 only reports some of those, there is no clear pattern... fill in with more 1 series */
static struct deviceproptablei8 nikon_1_shutterspeedi[] = {
	{ "Bulb",	-31, 0 },
	{ "30",		-30, 0 },
	{ "25",		-28, 0 },
	{ "20",		-26, 0 },
	{ "15",		-24, 0 },
	{ "13",		-22, 0 },
	{ "10",		-20, 0 },
	{ "8",		-18, 0 },
	{ "6",		-16, 0 },
	{ "5",		-14, 0 },
	{ "4",		-12, 0 },
	{ "3",		-10, 0 },
	{ "25/10",	-8, 0 },
	{ "2",		-6, 0 },
	{ "16/10",	-4, 0 },
	/* { "15/10",	xx, 0 }, not in j5 */
	{ "13/10",	-2, 0 },
	{ "1",		 0, 0 },
	{ "10/13",	 2, 0 }, /* 1 1/3 */
	/* { "10/15", 	xx, 0 }, not in j5 */
	{ "10/16",	 4, 0 },
	{ "1/2",	 6, 0 },
	{ "10/25",	 8, 0 },
	{ "1/3",	10, 0 },
	{ "1/4",	12, 0 },
	{ "1/5",	14, 0 },
	{ "1/6",	16, 0 },
	{ "1/8",	18, 0 },
	{ "1/10",	20, 0 },
	{ "1/13",	22, 0 },
	{ "1/15",	24, 0 },
	{ "1/20",	26, 0 },
	{ "1/25",	28, 0 },
	{ "1/30",	30, 0 },
	{ "1/40",	32, 0 },
	/* { "1/45", xx, 0 }, not in j5 */
	{ "1/50",	34, 0 },
	{ "1/60",	36, 0 },
	{ "1/80",	38, 0 },
	/* { "1/90", xx, 0 } not in j5 */
	{ "1/100",	40, 0 },
	{ "1/125",	42, 0 },
	{ "1/160",	44, 0 },
	/* { "1/180", xx, 0 }, not in j5 */
	{ "1/200",	46, 0 },
	{ "1/250",	48, 0 },
	{ "1/320",	50, 0 },
	/* { "1/350", xx, 0 }, not in j5 */
	{ "1/400",	52, 0 },
	{ "1/500",	54, 0 },
	{ "1/640",	56, 0 },
	/* { "1/750",	xx, 0 }, not in j5 */
	{ "1/800", 	58, 0 },
	{ "1/1000",	60, 0 },
	{ "1/1250",	62, 0 },
	/* { "1/1500", xx, 0}, not in j5 */
	{ "1/1600",	64, 0 },
	{ "1/2000",	66, 0 },
	{ "1/2500",	68, 0 },
	/* { "1/3000",	xx, 0 }, not in j5 */
	{ "1/3200",	70, 0 },
	{ "1/4000",	72, 0 },
	{ "1/5000",	74, 0 },
	/*{ "1/6000",	xx, 0 }, not in j5 */
	{ "1/6400",	76, 0 },
	{ "1/8000",	78, 0 },
	/* { "1/9000", xx , 0}, not in j5 */
	{ "1/10000",	80, 0 },
	/* { "1/12500", xx, 0 }, not in j5 */
	{ "1/13000",	82, 0 },
	/* { "1/15000", xx, 0 }, not in j5 */
	{ "1/16000",	84, 0 },
};
GENERICI8TABLE(Nikon_1_ShutterSpeedI,nikon_1_shutterspeedi)

static int
_get_Nikon_1_ShutterSpeedU(CONFIG_GET_ARGS) {
	dpd->DataType = PTP_DTC_INT8;
	return _get_Nikon_1_ShutterSpeedI(CONFIG_GET_NAMES);
}

static int
_put_Nikon_1_ShutterSpeedU(CONFIG_PUT_ARGS) {
	dpd->DataType = PTP_DTC_INT8;
	return _put_Nikon_1_ShutterSpeedI(CONFIG_PUT_NAMES);
}

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
GENERIC8TABLE(Nikon_LensID,nikon_lensid) /* FIXME: seen UINT8 and UINT16 types now */

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

static struct deviceproptableu8 nikon_d850_moviequality[] = {
 	{"3840x2160; 30p",	0, 0},
	{"3840x2160; 25p",	1, 0},
	{"3840x2160; 24p",	2, 0},

 	{"1920x1080; 60p",	3, 0},
	{"1920x1080; 50p",	4, 0},
	{"1920x1080; 30p",	5, 0},
	{"1920x1080; 25p",	6, 0},
	{"1920x1080; 24p",	7, 0},

	{"1280x720; 60p",	8, 0},
	{"1280x720; 50p",	9, 0},

	{"1920x1080; 30p x4 (slow-mo)", 10, 0},
	{"1920x1080; 25p x4 (slow-mo)",	11, 0},
	{"1920x1080; 24p x5 (slow-mo)",	12, 0},
};
GENERIC8TABLE(Nikon_D850_MovieQuality, nikon_d850_moviequality);

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

static struct deviceproptableu8 nikon_z6_padvpvalue[] = {
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
	{ "1/25",	0x16, 0 },
	{ "1/20",	0x17, 0 },
	{ "1/15",	0x18, 0 },
	{ "1/13",	0x19, 0 },
	{ "1/10",	0x1a, 0 },
	{ "1/8",	0x1b, 0 },
	{ "1/6",	0x1c, 0 },
	{ "1/5",	0x1d, 0 },
	{ "1/4",	0x1e, 0 },
	{ "1/3",	0x1f, 0 },
	{ "1/2.5",	0x20, 0 },
	{ "1/2",	0x21, 0 },
	{ "1/1.6",	0x22, 0 },
	{ "1/1.3",	0x23, 0 },
	{ "1",		0x24, 0 },
	{ "1.3",	0x25, 0 },
	{ "1.6",	0x26, 0 },
	{ "2",		0x27, 0 },
	{ "2.5",	0x28, 0 },
	{ "3",		0x29, 0 },
	{ "4",		0x2a, 0 },
	{ "5",		0x2b, 0 },
	{ "6",		0x2c, 0 },
	{ "8",		0x2d, 0 },
	{ "10",		0x2e, 0 },
	{ "13",		0x2f, 0 },
	{ "15",		0x30, 0 },
	{ "20",		0x31, 0 },
	{ "25",		0x32, 0 },
	{ "30",		0x33, 0 },
	{ N_("auto"),	0x34, 0 },
};
GENERIC8TABLE(Nikon_Z6_PADVPValue,nikon_z6_padvpvalue)

static struct deviceproptableu8 nikon_d90_activedlighting[] = {
	{ N_("Extra high"), 0x00,   0 },
	{ N_("High"),       0x01,   0 },
	{ N_("Normal"),     0x02,   0 },
	{ N_("Low"),        0x03,   0 },
	{ N_("Off"),        0x04,   0 },
	{ N_("Auto"),       0x05,   0 },
};
GENERIC8TABLE(Nikon_D90_ActiveDLighting,nikon_d90_activedlighting)

static struct deviceproptablei8 nikon_d850_activedlighting[] = {
	{ N_("Auto"),		0x00,   0 },
	{ N_("Off"),		0x01,   0 },
	{ N_("Low"),		0x02,   0 },
	{ N_("Normal"),		0x03,   0 },
	{ N_("High"),		0x04,   0 },
	{ N_("Extra high"),	0x05,   0 },
};
GENERICI8TABLE(Nikon_D850_ActiveDLighting,nikon_d850_activedlighting)

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

static struct deviceproptableu8 nikon_d850_compression[] = {
	{ N_("JPEG Basic"),  0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Basic*"), 0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"), 0x02, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal*"),   0x03, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),   0x04, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine*"),   0x05, PTP_VENDOR_NIKON },
	{ N_("TIFF"),   0x06, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),   0x07, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),   0x08, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic*"),   0x09, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal"),   0x0A, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal*"),   0x0B, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),   0x0C, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine*"),   0x0D, PTP_VENDOR_NIKON },
};
GENERIC8TABLE(Nikon_D850_Compression,nikon_d850_compression)

static struct deviceproptableu8 nikon_d7500_compression[] = {
	{ N_("JPEG Basic"),  0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Basic*"), 0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"), 0x02, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal*"),   0x03, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),   0x04, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine*"),   0x05, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),   0x07, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),   0x08, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic*"),   0x09, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal"),   0x0A, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal*"),   0x0B, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),   0x0C, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine*"),   0x0D, PTP_VENDOR_NIKON },
};
GENERIC8TABLE(Nikon_D7500_Compression,nikon_d7500_compression)

static struct deviceproptableu8 compressionsetting[] = {
	{ N_("JPEG Basic"),	0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"),	0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),	0x02, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),	0x04, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),	0x05, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal"),	0x06, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),	0x07, PTP_VENDOR_NIKON },

	{ N_("Light"),			0x01, PTP_VENDOR_SONY },
	{ N_("Standard"),		0x02, PTP_VENDOR_SONY },
	{ N_("Fine"),			0x03, PTP_VENDOR_SONY },
	{ N_("Extra Fine"),		0x04, PTP_VENDOR_SONY },
	{ N_("RAW"),			0x10, PTP_VENDOR_SONY },
	{ N_("RAW+JPEG (Light)"),	0x11, PTP_VENDOR_SONY },
	{ N_("RAW+JPEG (Std)"),		0x12, PTP_VENDOR_SONY },
	{ N_("RAW+JPEG (Fine)"),	0x13, PTP_VENDOR_SONY },
	{ N_("RAW+JPEG (X.Fine)"),	0x14, PTP_VENDOR_SONY },
	/* HEIF 4:2:0 4:2:2 is not exposed here */
	{ N_("HEIF (Light)"),		0x31, PTP_VENDOR_SONY },
	{ N_("HEIF (Std)"),		0x32, PTP_VENDOR_SONY },
	{ N_("HEIF (Fine)"),		0x33, PTP_VENDOR_SONY },
	{ N_("HEIF (X.Fine)"),		0x34, PTP_VENDOR_SONY },
	{ N_("RAW+HEIF (Light)"),	0x41, PTP_VENDOR_SONY },
	{ N_("RAW+HEIF (Std)"),		0x42, PTP_VENDOR_SONY },
	{ N_("RAW+HEIF (Fine)"),	0x43, PTP_VENDOR_SONY },
	{ N_("RAW+HEIF (X.Fine)"),	0x44, PTP_VENDOR_SONY },
};
GENERIC8TABLE(CompressionSetting,compressionsetting)

static struct deviceproptableu8 sony_300_compression[] = {
	{ N_("RAW"),         0x01, PTP_VENDOR_SONY },
	{ N_("RAW+JPEG"),    0x02, PTP_VENDOR_SONY },
	{ N_("JPEG"),        0x03, PTP_VENDOR_SONY },
};
GENERIC8TABLE(Sony_300_CompressionSetting,sony_300_compression)

static struct deviceproptableu8 sony_300_jpegcompression[] = {
	{ N_("n/a"),         0x00, PTP_VENDOR_SONY },
	{ N_("X.Fine"),      0x01, PTP_VENDOR_SONY },
	{ N_("Fine"),        0x02, PTP_VENDOR_SONY },
	{ N_("Std"),         0x03, PTP_VENDOR_SONY },
	{ N_("Light"),       0x04, PTP_VENDOR_SONY },
};
GENERIC8TABLE(Sony_300_JpegCompressionSetting,sony_300_jpegcompression)

static struct deviceproptableu8 sony_qx_compression[] = {
	{ N_("Standard"),	0x02, 0 },
	{ N_("Fine"),		0x03, 0 },
	{ N_("Extra Fine"),	0x04, 0 },
	{ N_("RAW"),		0x10, 0 },
	{ N_("RAW+JPEG"),	0x13, 0 },
};
GENERIC8TABLE(Sony_QX_Compression,sony_qx_compression)

static struct deviceproptableu8 sony_pc_save_image_size[] = {
	{ N_("Original"), 0x01, 0 },
	{ N_("2M"),       0x02, 0 },
};
GENERIC8TABLE(Sony_PcSaveImageSize, sony_pc_save_image_size)

static struct deviceproptableu8 sony_pc_save_image_format[] = {
	{ N_("RAW & JPEG"),   0x01, 0 },
	{ N_("JPEG Only"),    0x02, 0 },
	{ N_("RAW Only"),     0x03, 0 },
	{ N_("RAW & HEIF"),   0x04, 0 },
	{ N_("HEIF Only"),    0x05, 0 }
};
GENERIC8TABLE(Sony_PcSaveImageFormat, sony_pc_save_image_format)

static struct deviceproptableu8 sony_sensorcrop[] = {
	{ N_("Off"),	0x01, 0 },
	{ N_("On"),	0x02, 0 },
};
GENERIC8TABLE(Sony_SensorCrop,sony_sensorcrop)

static struct deviceproptableu8 sony_liveviewsettingeffect[] = {
		{ N_("On"),  0x01, 0 },
		{ N_("Off"), 0x02, 0 },
};
GENERIC8TABLE(Sony_LiveViewSettingEffect,sony_liveviewsettingeffect)

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
	*alreadyset = 1;
	return GP_OK;
}

static struct deviceproptableu16 canon_eos_highisonr[] = {
	/* 6d values */
	{ N_("Off"),		0, 0 },
	{ N_("Low"),		1, 0 },
	{ N_("Normal"),		2, 0 },
	{ N_("High"),		3, 0 },
	{ N_("Multi-Shot"),	4, 0 },
};

GENERIC16TABLE(Canon_EOS_HighIsoNr,canon_eos_highisonr)

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
	{ N_("High Efficiency*"),0x03, 0 },
	{ N_("High Efficiency"),0x04, 0 },
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

static struct deviceproptableu8 nikon_d850_shootingspeed[] = {
	{ N_("8 fps"),	0x01, 0 },
	{ N_("7 fps"),	0x02, 0 },
	{ N_("6 fps"),	0x03, 0 },
	{ N_("5 fps"),	0x04, 0 },
	{ N_("4 fps"),	0x05, 0 },
	{ N_("3 fps"),	0x06, 0 },
	{ N_("2 fps"),	0x07, 0 },
	{ N_("1 fps"),	0x08, 0 },
};
GENERIC8TABLE(Nikon_D850_ShootingSpeed,nikon_d850_shootingspeed)

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

static struct deviceproptableu8 nikon_vignettecorrection[] = {
	{ N_("High"),		0x0, 0 },
	{ N_("Normal"),		0x1, 0 },
	{ N_("Moderate"),	0x2, 0 },
	{ N_("Off"),		0x3, 0 },
};
GENERIC8TABLE(Nikon_VignetteCorrection,nikon_vignettecorrection)

static struct deviceproptableu8 nikon_hdmidatadepth[] = {
	{ "8",		0x0, 0 },
	{ "10",		0x1, 0 },
};
GENERIC8TABLE(Nikon_HDMIDataDepth,nikon_hdmidatadepth)

static struct deviceproptableu8 nikon_facedetection[] = {
	{ N_("Off"),				0x0, 0 },
	{ N_("Face detection"),			0x1, 0 },
	{ N_("Face and pupil detection"),	0x2, 0 },
	{ N_("Animal detection"),		0x3, 0 },
};
GENERIC8TABLE(Nikon_FaceDetection,nikon_facedetection)

static struct deviceproptableu32 canon_eos_movieservoaf[] = {
	{ N_("Off"),	0x0, 0 },
	{ N_("On"),	0x1, 0 },
};
GENERIC32TABLE(Canon_EOS_MovieServoAF,canon_eos_movieservoaf)

static int
_get_BatteryLevel(CONFIG_GET_ARGS) {
	unsigned char value_float , start, end;
	char	buffer[20];

	if (dpd->DataType != PTP_DTC_UINT8)
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);

	if (dpd->FormFlag == PTP_DPFF_Enumeration) {
		unsigned int i, highest = 0, factor = 1;

		gp_widget_set_name (*widget, menu->name);

		/* This is for Canon ... they have enums  [0,1,2,3] and [0,25,50,75,100] .. For the 0-3 enum, multiply by 33 */
		for (i=0;i<dpd->FORM.Enum.NumberOfValues;i++) {
			if (dpd->FORM.Enum.SupportedValue[i].u8 > highest)
				highest = dpd->FORM.Enum.SupportedValue[i].u8;
		}
		if (highest == 3) factor = 33;

		sprintf (buffer, "%d%%", dpd->CurrentValue.u8 * factor);
		return gp_widget_set_value(*widget, buffer);
	}
	if (dpd->FormFlag == PTP_DPFF_Range) {
		gp_widget_set_name (*widget, menu->name);
		start = dpd->FORM.Range.MinimumValue.u8;
		end = dpd->FORM.Range.MaximumValue.u8;
		value_float = dpd->CurrentValue.u8;
		if (0 == end - start + 1) {
			/* avoid division by 0 */
			sprintf (buffer, "broken");
		} else {
			sprintf (buffer, "%d%%", (int)((value_float-start+1)*100/(end-start+1)));
		}
		return gp_widget_set_value(*widget, buffer);
	}
	/* Enumeration is also valid on EOS, but this will be just be the % value */
	sprintf (buffer, "%d%%", dpd->CurrentValue.u8);
	return gp_widget_set_value(*widget, buffer);
}

static int
_get_SONY_BatteryLevel(CONFIG_GET_ARGS) {
	unsigned char value_float , start, end;
	char	buffer[20];

	if (dpd->DataType != PTP_DTC_INT8)
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);

	if (dpd->FormFlag == PTP_DPFF_Range) {
		gp_widget_set_name (*widget, menu->name);
		start = dpd->FORM.Range.MinimumValue.i8;
		if (dpd->FORM.Range.MinimumValue.i8 == -1)
			start = 0; /* -1 might be special for unknown? */
		else
			start = dpd->FORM.Range.MinimumValue.i8;
		end = dpd->FORM.Range.MaximumValue.i8;
		value_float = dpd->CurrentValue.i8;
		if (0 == end - start + 1) {
			/* avoid division by 0 */
			sprintf (buffer, "broken");
		} else {
			sprintf (buffer, "%d%%", (int)((value_float-start+1)*100/(end-start+1)));
		}
		return gp_widget_set_value(*widget, buffer);
	}
	/* Enumeration is also valid on EOS, but this will be just be the % value */
	if (dpd->CurrentValue.i8 == -1)
		sprintf (buffer, _("Unknown"));
	else
		sprintf (buffer, "%d%%", dpd->CurrentValue.i8);
	return gp_widget_set_value(*widget, buffer);
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
_get_Canon_EOS_StorageID(CONFIG_GET_ARGS) {
	char buf[16];

	if (dpd->DataType != PTP_DTC_UINT32)
		return GP_ERROR;
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	sprintf(buf,"%08x",dpd->CurrentValue.u32);
	gp_widget_set_value(*widget, buf);
	return GP_OK;
}

static int
_put_Canon_EOS_StorageID(CONFIG_PUT_ARGS) {
	char		*val = NULL;
	unsigned int	x = 0;

	CR (gp_widget_get_value(widget, &val));
	if (!sscanf(val,"%x",&x))
		return GP_ERROR_BAD_PARAMETERS;
	propval->u32 = x;
	return GP_OK;
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

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_ChangeCameraMode))
		return GP_ERROR_NOT_SUPPORTED;
	gp_widget_get_value(widget, &val);

	if (!sscanf(val,"%d",&xval))
		return GP_ERROR;

	C_PTP (ptp_nikon_changecameramode (&camera->pl->params, xval));
	params->controlmode = xval;
	return GP_OK;
}

static int
_get_Nikon_ApplicationMode2(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	gp_widget_set_value(*widget, "0");
	return (GP_OK);
}

static int
_put_Nikon_ApplicationMode2(CONFIG_PUT_ARGS) {
	PTPParams *params = &(camera->pl->params);
	char*		val;
	unsigned int	xval = 0;

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_ChangeApplicationMode))
		return GP_ERROR_NOT_SUPPORTED;
	gp_widget_get_value(widget, &val);

	if (!sscanf(val,"%d",&xval))
		return GP_ERROR;

	C_PTP (ptp_nikon_changeapplicationmode (&camera->pl->params, xval));
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
 * parameters: press mode, ? afmode ? SDK seems to suggest 1==NonAF, 0 == AF
 */

static int
_put_Canon_EOS_RemoteRelease(CONFIG_PUT_ARGS) {
	const char*	val;
	PTPParams	*params = &(camera->pl->params);
	GPContext 	*context = ((PTPData *) params->data)->context;

	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn))
		return (GP_ERROR_NOT_SUPPORTED);

	/* If someone has set the capture target between */
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
_get_Panasonic_MFDrive(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	gp_widget_add_choice (*widget, _("Near 1"));
	gp_widget_add_choice (*widget, _("Near 2"));
	gp_widget_add_choice (*widget, _("None"));
	gp_widget_add_choice (*widget, _("Far 1"));
	gp_widget_add_choice (*widget, _("Far 2"));

	gp_widget_set_value (*widget, _("None"));
	return GP_OK;
}

static int
_put_Panasonic_MFDrive(CONFIG_PUT_ARGS) {
	const char*     val;
	unsigned int    xval;
	uint16_t	direction = 0; // 0=near
	uint16_t	mode = 0x02;
	PTPParams	*params = &(camera->pl->params);

	gp_widget_get_value(widget, &val);

	if (!strcmp (val, _("None"))) return GP_OK;

	if (!sscanf (val, _("Near %d"), &xval)) {
		if (!sscanf (val, _("Far %d"), &xval)) {
			GP_LOG_D ("Could not parse %s", val);
			return GP_ERROR;
		} else {
			direction = 1; // far
		}
	}
	if(direction) { // far
		if(xval == 1) mode = 0x03;
		if(xval == 2) mode = 0x04;
	} else { // near
		if(xval == 1) mode = 0x02;
		if(xval == 2) mode = 0x01;
	}
	gp_widget_set_value (widget, _("None")); /* Marcus: not right here */
	C_PTP_MSG (ptp_panasonic_manualfocusdrive (params, mode), "Panasonic manual focus drive 0x%x failed", xval);
	return GP_OK;
}


static int
_get_Olympus_OMD_MFDrive(CONFIG_GET_ARGS) {
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
_put_Olympus_OMD_MFDrive(CONFIG_PUT_ARGS) {
	const char*	val;
	unsigned int	xval;
	uint32_t direction = 0x01;
	uint32_t step_size = 0x0e;
	PTPParams *params = &(camera->pl->params);

	if (!ptp_operation_issupported(params, PTP_OC_OLYMPUS_OMD_MFDrive))
		return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_get_value(widget, &val);

	if (!strcmp (val, _("None"))) return GP_OK;

	if (!sscanf (val, _("Near %d"), &xval)) {
		if (!sscanf (val, _("Far %d"), &xval)) {
			GP_LOG_D ("Could not parse %s", val);
			return GP_ERROR;
		} else {
			direction = 0x02;
		}
	}
	if(xval == 1) step_size = 0x03;
	if(xval == 2) step_size = 0x0e;
	if(xval == 3) step_size = 0x3c;

	C_PTP_MSG (ptp_olympus_omd_move_focus (params, direction, step_size),
		   "Olympus manual focus drive 0x%x failed", xval);
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
_get_Canon_RemoteMode(CONFIG_GET_ARGS) {
	char		buf[200];
	PTPParams	*params = &(camera->pl->params);
	uint32_t	mode;

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (ptp_operation_issupported (params, PTP_OC_CANON_EOS_GetRemoteMode)) {
		C_PTP (ptp_canon_eos_getremotemode (params, &mode));
		sprintf (buf, "%d", mode);
	} else {
		strcpy (buf, "0");
	}
	return gp_widget_set_value  (*widget, buf);
}

static int
_put_Canon_RemoteMode(CONFIG_PUT_ARGS) {
	uint32_t	mode;
	char		*val;
	PTPParams	*params = &(camera->pl->params);

	CR (gp_widget_get_value(widget, &val));
	if (!sscanf (val, "%d", &mode))
		return GP_ERROR;
	C_PTP (ptp_canon_eos_setremotemode (params, mode));
	return GP_OK;
}


static int
_get_Canon_EventMode(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	return gp_widget_set_value  (*widget, "0");
}

static int
_put_Canon_EventMode(CONFIG_PUT_ARGS) {
	uint32_t	mode;
	char		*val;
	PTPParams	*params = &(camera->pl->params);

	CR (gp_widget_get_value(widget, &val));
	if (!sscanf (val, "%d", &mode))
		return GP_ERROR;
	C_PTP (ptp_canon_eos_seteventmode (params, mode));
	return GP_OK;
}


static int
_get_Canon_EOS_ViewFinder(CONFIG_GET_ARGS) {
	int val;
	PTPParams		*params = &(camera->pl->params);

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	val = params->inliveview;	/* try returning live view mode */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
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
		   "ptp2_eos_viewfinder enable: failed to set evf outputmode to %d", xval.u32);
        return GP_OK;
}

static int
_get_Canon_EOS_TestOLC(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	val = 2;
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Canon_EOS_TestOLC(CONFIG_PUT_ARGS) {
	int			val;
	PTPParams		*params = &(camera->pl->params);
	unsigned int		i;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		/* idea is to request all OLCs separately to see the sizes in the logfile */
		for (i=0;i<13;i++) {	/* 0x1 -> 0x1000 */
			C_PTP (ptp_canon_eos_setrequestolcinfogroup(params, (1<<i)));
			ptp_check_eos_events (params);
		}
		C_PTP (ptp_canon_eos_setrequestolcinfogroup(params, 0x1fff));
	}
        return GP_OK;
}

static int
_get_Panasonic_ViewFinder(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	val = 2;	/* always changed, unless we can find out the state ... */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Panasonic_ViewFinder(CONFIG_PUT_ARGS) {
	int			val;
	uint16_t		res;
	PTPParams		*params = &(camera->pl->params);

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		res = ptp_panasonic_liveview (params, 1);
		params->inliveview = 1;
	} else {
		res = ptp_panasonic_liveview (params, 0);
		params->inliveview = 0;
	}
	return translate_ptp_result (res);
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

	C_PTP_REP (ptp_nikon_device_ready(params));

	CR (gp_widget_get_value (widget, &val));
	if (val) {
		PTPPropertyValue	value;

		if (LOG_ON_PTP_E (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewStatus, &value, PTP_DTC_UINT8)) != PTP_RC_OK)
			value.u8 = 0;

		if (have_prop(camera, params->deviceinfo.VendorExtensionID, PTP_DPC_NIKON_LiveViewProhibitCondition)) {
			C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewProhibitCondition, &value, PTP_DTC_UINT32));

			if (value.u32) {
				/* we could have multiple reasons, but just report the first one. by decreasing order of possibility */
				if (value.u32 & (1<<8)) { gp_context_error (context, _("Liveview cannot start: Battery exhausted")); return GP_ERROR; }
				if (value.u32 & (1<<17)){ gp_context_error (context, _("Liveview cannot start: Temperature too high")); return GP_ERROR; }
				if (value.u32 & (1<<9)) { gp_context_error (context, _("Liveview cannot start: TTL error")); return GP_ERROR; }
				if (value.u32 & (1<<22)){ gp_context_error (context, _("Liveview cannot start: In Mirror-up operation")); return GP_ERROR; }
				if (value.u32 & (1<<24)){ gp_context_error (context, _("Liveview cannot start: Lens is retracting")); return GP_ERROR; }
				if (value.u32 & (1<<5)) { gp_context_error (context, _("Liveview cannot start: Minimum aperture warning")); return GP_ERROR; }
				if (value.u32 & (1<<15)){ gp_context_error (context, _("Liveview cannot start: Processing of shooting operation")); return GP_ERROR; }
				if (value.u32 & (1<<2)) { gp_context_error (context, _("Liveview cannot start: Sequence error")); return GP_ERROR; }
				if (value.u32 & (1<<31)){ gp_context_error (context, _("Liveview cannot start: Exposure Program Mode is not P/A/S/M")); return GP_ERROR; }
				if (value.u32 & (1<<21)){ gp_context_error (context, _("Liveview cannot start: Bulb warning")); return GP_ERROR; }
				if (value.u32 & (1<<20)){ gp_context_error (context, _("Liveview cannot start: Card unformatted")); return GP_ERROR; }
				if (value.u32 & (1<<19)){ gp_context_error (context, _("Liveview cannot start: Card error")); return GP_ERROR; }
				if (value.u32 & (1<<18)){ gp_context_error (context, _("Liveview cannot start: Card protected")); return GP_ERROR; }
				if (value.u32 & (1<<14)){ gp_context_error (context, _("Liveview cannot start: Recording destination card, but no card or card protected")); return GP_ERROR; }
				if (value.u32 & (1<<12)){ gp_context_error (context, _("Liveview cannot start: Pending unretrieved SDRAM image")); return GP_ERROR; }
				if (value.u32 & (1<<12)){ gp_context_error (context, _("Liveview cannot start: Pending unretrieved SDRAM image")); return GP_ERROR; }
				if (value.u32 & (1<<4)) { gp_context_error (context, _("Liveview cannot start: Fully pressed button")); return GP_ERROR; }

				gp_context_error (context, _("Liveview cannot start: code 0x%08x"), value.u32);
				return GP_ERROR;
			}
		}

		if (!value.u8) {
			value.u8 = 1;
			LOG_ON_PTP_E (ptp_setdevicepropvalue (params, PTP_DPC_NIKON_RecordingMedia, &value, PTP_DTC_UINT8));
			C_PTP_REP_MSG (ptp_nikon_start_liveview (params),
				       _("Nikon enable liveview failed"));
			/* Has to put the mirror up, so takes a bit. */
			C_PTP (nikon_wait_busy(params, 50, 1000));
			params->inliveview = 1;
		}
	} else {
		if (ptp_operation_issupported(params, PTP_OC_NIKON_EndLiveView)) {
			// busy check here needed for some nikons to
			// prevent bad camera state
			C_PTP_REP (ptp_nikon_device_ready(params));

			uint16_t res = ptp_nikon_end_liveview (params);
			// printf("Live view end code: %d\n", res);
			if (res == 0xa004) {
				// PTP_C(ptp_nikon_device_ready(params));
				return GP_ERROR_CAMERA_BUSY;
			}
			C_PTP(res);
		}
		params->inliveview = 0;
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
	GP_LOG_D ("opcode 0x%x", opcode);
	nparams = 0; x = val;
	while ((x = strchr(x,',')) && (nparams<5)) {
		x++;
		if (!sscanf(x,"0x%x", &xparams[nparams]))
			return GP_ERROR_BAD_PARAMETERS;
		GP_LOG_D ("param %d 0x%x", nparams, xparams[nparams]);
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
_get_Canon_EOS_MovieModeSw(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Canon_EOS_MovieModeSw(CONFIG_PUT_ARGS) {
	int		val;
	PTPParams	*params = &(camera->pl->params);
	CR (gp_widget_get_value(widget, &val));

	if(val)
		C_PTP_MSG (ptp_generic_no_data(params, PTP_OC_CANON_EOS_MovieSelectSWOn, 0), "Failed to set MovieSetSelectSWOn");
	else
		C_PTP_MSG (ptp_generic_no_data(params, PTP_OC_CANON_EOS_MovieSelectSWOff, 0), "Failed to set MovieSetSelectSWOff");
	return GP_OK;
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
	*alreadyset = 1;
	return GP_OK;
}

static int
_get_Sony_QX_Movie(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Sony_QX_Movie(CONFIG_PUT_ARGS)
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
        C_PTP_REP (ptp_sony_qx_setdevicecontrolvalueb (params, PTP_DPC_SONY_QX_Movie_Rec, &value, PTP_DTC_UINT16 ));
	*alreadyset = 1;
	return GP_OK;
}

static int
_get_Nikon_MovieProhibitCondition(CONFIG_GET_ARGS) {
	char 			buf[2000];
	PTPPropertyValue	value;
	PTPParams 		*params = &(camera->pl->params);

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	value.u32 = 0;
	LOG_ON_PTP_E (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_MovRecProhibitCondition, &value, PTP_DTC_UINT32));
	if (value.u32) {
		strcpy(buf,N_("Movie prohibit conditions: "));
#define X(bit,str)					\
		if (value.u32 & (1<<bit)) {		\
			value.u32 &= ~(1<<bit);		\
			strcat(buf, _(str));		\
			if (value.u32) strcat(buf, ",");\
		}

		X(14,N_("Not in application mode"));
		X(13,N_("Set liveview selector is enabled"));
		X(12,N_("In enlarged liveview"));
		X(12,N_("In enlarged liveview"));
		X(11,N_("Card protected"));
		X(10,N_("Already in movie recording"));
		X( 9,N_("Images / movies not yet record in buffer"));
		X( 3,N_("Card full"));
		X( 2,N_("Card not formatted"));
		X( 1,N_("Card error"));
		X( 0,N_("No card"));
#undef X
		if (value.u32) { sprintf(buf+strlen(buf),"unhandled bitmask %x", value.u32); }
	} else {
		strcpy(buf,N_("No movie prohibit conditions"));
	}
	gp_widget_set_value  (*widget, buf);
	return GP_OK;
}

static int
_get_Nikon_LiveViewProhibitCondition(CONFIG_GET_ARGS) {
	char 			buf[2000];
	PTPPropertyValue	value;
	PTPParams 		*params = &(camera->pl->params);

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);

	value.u32 = 0;
	LOG_ON_PTP_E (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_LiveViewProhibitCondition, &value, PTP_DTC_UINT32));
	if (value.u32) {
		strcpy(buf,N_("Live View prohibit conditions: "));
#define X(bit,str)					\
		if (value.u32 & (1<<bit)) {		\
			value.u32 &= ~(1<<bit);		\
			strcat(buf, _(str));		\
			if (value.u32) strcat(buf, ",");\
		}
		X( 2,N_("Sequence error"));
		X( 4,N_("Fully pressed button"));
		X( 5,N_("Minimum aperture warning"));
		X( 8,N_("Battery exhausted"));
		X( 9,N_("TTL error"));
		X(12,N_("Pending unretrieved SDRAM image"));
		X(14,N_("Recording destination card, but no card or card protected"));
		X(15,N_("Processing of shooting operation"));
		X(17,N_("Temperature too high"));
		X(18,N_("Card protected"));
		X(19,N_("Card error"));
		X(20,N_("Card unformatted"));
		X(21,N_("Bulb warning"));
		X(22,N_("In Mirror-up operation"));
		X(24,N_("Lens is retracting"));
		X(31,N_("Exposure Program Mode is not P/A/S/M"));
#undef X
		if (value.u32) { sprintf(buf+strlen(buf),"unhandled bitmask %x", value.u32); }
	} else {
		strcpy(buf,N_("Liveview should not be prohibited"));
	}
	gp_widget_set_value  (*widget, buf);
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
	PTPPropertyValue	value;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		if (have_prop(camera,PTP_VENDOR_NIKON,PTP_DPC_NIKON_ApplicationMode)) {
			value.u8 = 0;
			C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_ApplicationMode, &value, PTP_DTC_UINT8));
			if (value.u8 != 1) {
				value.u8 = 1;
				C_PTP (ptp_setdevicepropvalue (params, PTP_DPC_NIKON_ApplicationMode, &value, PTP_DTC_UINT8));
			}
		}
		if (ptp_operation_issupported(params, PTP_OC_NIKON_ChangeApplicationMode)) {
			C_PTP (ptp_nikon_changeapplicationmode (params, 1));
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

		if (have_prop(camera,PTP_VENDOR_NIKON,PTP_DPC_NIKON_MovRecProhibitCondition)) {
			value.u32 = 0;
			LOG_ON_PTP_E (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_MovRecProhibitCondition, &value, PTP_DTC_UINT32));
			if (value.u32) {
				if (value.u32 & (1<<14)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Camera is not in application mode")); return GP_ERROR; }
				if (value.u32 & (1<<13)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Set liveview selector is enabled")); return GP_ERROR; }
				if (value.u32 & (1<<12)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("In enlarged liveview")); return GP_ERROR; }
				if (value.u32 & (1<<11)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Card protected")); return GP_ERROR; }
				if (value.u32 & (1<<10)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Already in movie recording")); return GP_ERROR; }
				if (value.u32 & (1<< 9)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Images / movies not yet record in buffer")); return GP_ERROR; }
				if (value.u32 & (1<< 3)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Card full")); return GP_ERROR; }
				if (value.u32 & (1<< 2)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Card not formatted")); return GP_ERROR; }
				if (value.u32 & (1<< 1)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("Card error")); return GP_ERROR; }
				if (value.u32 & (1<< 0)) { gp_context_error (context, _("Movie recording cannot start: %s."),_("No card")); return GP_ERROR; }
				gp_context_error (context, _("Movie recording cannot start: code 0x%08x."), value.u32);
				return GP_ERROR;
			}
		}

		C_PTP_REP (ptp_nikon_startmovie (params));
	} else {
		unsigned int i, havec108 = 0;

		C_PTP_REP (ptp_nikon_stopmovie (params));

		for (i=0;i<params->deviceinfo.EventsSupported_len;i++)
			if (params->deviceinfo.EventsSupported[i] == PTP_EC_Nikon_MovieRecordComplete) {
				havec108 = 1;
				break;
			}

		/* takes 3 seconds for a 10 second movie on Z6 */
		if (havec108) {
			int tries = 100, found = 0;
			do {
				PTPContainer	event;

				ret = ptp_check_event (params);
				if (ret != PTP_RC_OK)
					break;

				while (ptp_get_one_event_by_type (params, PTP_EC_Nikon_MovieRecordComplete, &event)) {
					GP_LOG_D ("Event: movie rec completed.");
					found = 1;
					break;
				}
				usleep(100*1000);
			} while (!found && tries--);
		}
		/* switch Application Mode off again, otherwise we cannot get to the filesystem */
		if (have_prop(camera,PTP_VENDOR_NIKON,PTP_DPC_NIKON_ApplicationMode)) {
			value.u8 = 1;
			C_PTP (ptp_getdevicepropvalue (params, PTP_DPC_NIKON_ApplicationMode, &value, PTP_DTC_UINT8));
			if (value.u8 != 0) {
				value.u8 = 0;
				C_PTP (ptp_setdevicepropvalue (params, PTP_DPC_NIKON_ApplicationMode, &value, PTP_DTC_UINT8));
			}
		}
		if (ptp_operation_issupported(params, PTP_OC_NIKON_ChangeApplicationMode)) {
			C_PTP (ptp_nikon_changeapplicationmode (params, 0));
		}
	}
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

		C_PTP (ptp_nikon_changecameramode (params, 1));
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
		params->opencapture_transid = params->transaction_id-1; /* transid will be incremented already */
	} else {
		C_PTP_REP (ptp_terminateopencapture (params, params->opencapture_transid));
	}
	return GP_OK;
}

static int
_get_Sony_Autofocus(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Sony_Autofocus(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	PTPPropertyValue xpropval;

	CR (gp_widget_get_value(widget, &val));
	xpropval.u16 = val ? 2 : 1;

	C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_AutoFocus, &xpropval, PTP_DTC_UINT16));
	*alreadyset = 1;
	return GP_OK;
}

static int
_get_Sony_ManualFocus(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_range(*widget, -7.0, 7.0, 1.0);
	gp_widget_set_name (*widget,menu->name);
	val = 0.0; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Sony_ManualFocus(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	float val;
	PTPPropertyValue xpropval;

	CR (gp_widget_get_value(widget, &val));

	if(val != 0.0) {
		/* value 2 seems to set it to autofocusmode. see issue https://github.com/gphoto/libgphoto2/issues/434
		xpropval.u16 = 2;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, 0xd2d2, &xpropval, PTP_DTC_UINT16));
		*/
		if(val <= -7) xpropval.i16 = - 7;
		else if(val <= -6.0) xpropval.i16 = -6;
		else if(val <= -5.0) xpropval.i16 = -5;
		else if(val <= -4.0) xpropval.i16 = -4;
		else if(val <= -3.0) xpropval.i16 = -3;
		else if(val <= -2.0) xpropval.i16 = -2;
		else if(val <= -1.0) xpropval.i16 = -1;
		else if(val <= 1.0) xpropval.i16 = 1;
		else if(val <= 2.0) xpropval.i16 = 2;
		else if(val <= 3.0) xpropval.i16 = 3;
		else if(val <= 4.0) xpropval.i16 = 4;
		else if(val <= 5.0) xpropval.i16 = 5;
		else if(val <= 6.0) xpropval.i16 = 6;
		else if(val <= 7.0) xpropval.i16 = 7;
		else xpropval.i16 = 0;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_NearFar, &xpropval, PTP_DTC_INT16));
	} else {
		xpropval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, 0xd2d2, &xpropval, PTP_DTC_UINT16));
	}
	*alreadyset = 1;
	return GP_OK;
}

static int
_get_Sony_Capture(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Sony_Capture(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	PTPPropertyValue xpropval;

	CR (gp_widget_get_value(widget, &val));
	xpropval.u16 = val ? 2 : 1;

	C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_Capture, &xpropval, PTP_DTC_UINT16));
	*alreadyset = 1;
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
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_AutoFocus, &xpropval, PTP_DTC_UINT16));

		xpropval.u16 = 2;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_StillImage, &xpropval, PTP_DTC_UINT16));
	} else {
		xpropval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_Capture, &xpropval, PTP_DTC_UINT16));

		xpropval.u16 = 1;
		C_PTP (ptp_sony_setdevicecontrolvalueb (params, PTP_DPC_SONY_AutoFocus, &xpropval, PTP_DTC_UINT16));
	}
	*alreadyset = 1;
	return GP_OK;
}

static int
_get_Sony_FocusMagnifyProp(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Sony_FocusMagnifyProp(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;
	PTPPropertyValue xpropval;

	CR (gp_widget_get_value(widget, &val));
	xpropval.u16 = val ? 2 : 1;

	C_PTP (ptp_sony_setdevicecontrolvalueb (params, dpd->DevicePropertyCode, &xpropval, PTP_DTC_UINT16));
	*alreadyset = 1;
	return GP_OK;
}

static int
_get_Panasonic_Movie(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Panasonic_Movie(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	int val;

	CR (gp_widget_get_value(widget, &val));
	if (val) {
		C_PTP_MSG (ptp_panasonic_movierec (params, 1), "failed to start movie capture");
		return GP_OK;
	} else {
		C_PTP_MSG (ptp_panasonic_movierec (params, 0), "failed to stop movie capture");
		return GP_OK;
	}
}


static int
_put_Panasonic_Shutter(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	char *xval;
	uint32_t val;
	float f;

	CR (gp_widget_get_value(widget, &xval));
	if(xval[0] == 'b' || xval[0] == 'B') {
		val = 0xFFFFFFFF;
	} else if(xval[1] == '/') {
		sscanf (xval, "1/%f", &f);
		f *= 1000;
		val = (uint32_t) f;
	} else {
		sscanf (xval, "%f", &f);
		f *= 1000;
		val = (uint32_t) f;
		val |= 0x80000000;
	}
	return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_ShutterSpeed_Param, (unsigned char*)&val, 4));
}

static int
_get_Panasonic_Shutter(CONFIG_GET_ARGS) {
	uint32_t currentVal;
	uint32_t listCount;
	uint32_t *list;
	PTPParams *params = &(camera->pl->params);
	GPContext *context = ((PTPData *) params->data)->context;

	C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, PTP_DPC_PANASONIC_ShutterSpeed, 4, &currentVal, &list, &listCount));

	//printf("retrieved %lu property values\n", listCount);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	uint32_t i;
	float f;
	char buf[16];
	for (i = 0; i < listCount; i++) {
		if(currentVal == 0xFFFFFFFF) {
			sprintf (buf, "bulb");
		} else if(list[i] & 0x80000000) {
			list[i] &= ~0x80000000;
			f = (float) list[i];
			f /= 1000;
			if(list[i] % 1000 == 0) {
				sprintf (buf, "%.0f", f);
			} else {
				sprintf (buf, "%.1f", f);
			}
		} else {
			f = (float) list[i];
			f /= 1000;
			if(list[i] % 1000 == 0) {
				sprintf (buf, "1/%.0f", f);
			} else {
				sprintf (buf, "1/%.1f", f);
			}
		}
		gp_widget_add_choice (*widget, buf);
	}

	if(currentVal == 0) {
		uint16_t valuesize;
		ptp_panasonic_getdeviceproperty(params, 0x2000030, &valuesize, &currentVal);
	}

	if(currentVal == 0xFFFFFFFF) {
		sprintf (buf, "bulb");
	} else if(currentVal & 0x80000000) {
		currentVal &= ~0x80000000;
		f = (float) currentVal;
		f /= 1000;
		if(currentVal % 1000 == 0) {
			sprintf (buf, "%.0f", f);
		} else {
			sprintf (buf, "%.1f", f);
		}
	} else {
		f = (float) currentVal;
		f /= 1000;
		if(currentVal % 1000 == 0) {
			sprintf (buf, "1/%.0f", f);
		} else {
			sprintf (buf, "1/%.1f", f);
		}
	}

	gp_widget_set_value (*widget, &buf);

	free(list);

	return GP_OK;
}

static int
_put_Panasonic_ISO(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	char *xval;
	unsigned int ival;
	uint32_t val;

	CR (gp_widget_get_value(widget, &xval));
	sscanf (xval, "%d", &ival);
	val = ival;

	//printf("setting ISO to %lu (%s)\n", val, xval);

	return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_ISO_Param, (unsigned char*)&val, 4));
}

static int
_get_Panasonic_ISO(CONFIG_GET_ARGS) {
	uint32_t currentVal;
	uint32_t listCount;
	uint32_t *list;
	uint16_t valsize;
	PTPParams *params = &(camera->pl->params);
	GPContext *context = ((PTPData *) params->data)->context;

	C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, PTP_DPC_PANASONIC_ISO, 4, &currentVal, &list, &listCount));

	//printf("retrieved %lu property values\n", listCount);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	uint32_t i;
	char buf[16];
	for (i = 0; i < listCount; i++) {
		sprintf (buf, "%d", (unsigned int)list[i]);
		gp_widget_add_choice (*widget, buf);
	}
	ptp_panasonic_getdeviceproperty(params, PTP_DPC_PANASONIC_ISO_Param, &valsize, &currentVal);

	sprintf (buf, "%d", (unsigned int)currentVal);
	gp_widget_set_value (*widget, buf);

	free(list);

	return GP_OK;
}

static
struct {
	char*	str;
	uint16_t val;
} panasonic_wbtable[] = {
	{ N_("Automatic"),	0x0002	},
	{ N_("Daylight"),	0x0004	},
	{ N_("Tungsten"),	0x0006	},
	{ N_("Flourescent"),	0x0005	},
	{ N_("Flash"),		0x0007	},
	{ N_("Cloudy"),		0x8008	},
	{ N_("White set"),	0x8009	},
	{ N_("Black White"),	0x800A	},
	{ N_("Preset 1"),	0x800B	},
	{ N_("Preset 2"),	0x800C	},
	{ N_("Preset 3"),	0x800D	},
	{ N_("Preset 4"),	0x800E	},
	{ N_("Shadow"),		0x800F	},
	{ N_("Temperature 1"),	0x8010	},
	{ N_("Temperature 2"),	0x8011	},
	{ N_("Temperature 3"),	0x8012	},
	{ N_("Temperature 4"),	0x8013	},
	{ N_("Automatic Cool"),	0x8014	},
	{ N_("Automatic Warm"),	0x8015	},
};

static int
_put_Panasonic_AdjustGM(CONFIG_PUT_ARGS)
{
    PTPParams *params = &(camera->pl->params);
    char *xval;
    uint32_t val;

    CR (gp_widget_get_value(widget, &xval));
    int16_t adj;
    sscanf (xval, "%hd", &adj);
    if (adj < 0) {
        adj = abs(adj) + 0x8000;
    }
    val = adj;

    return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_WhiteBalance_ADJ_GM, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_AdjustGM(CONFIG_GET_ARGS) {
    uint32_t currentVal = 0;
    char buf[32];
    PTPParams *params = &(camera->pl->params);
    GPContext *context = ((PTPData *) params->data)->context;

    uint16_t valsize;
    C_PTP_REP (ptp_panasonic_getdeviceproperty(params, PTP_DPC_PANASONIC_WhiteBalance_ADJ_GM, &valsize, &currentVal));

    //printf("retrieved %x with size %x\n", currentVal, valsize);
    if (currentVal & 0x8000) {
        currentVal = (currentVal & 0x7FFF) * -1;
    }
    sprintf(buf, "%d\n", currentVal);

    gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
    gp_widget_set_name (*widget, menu->name);

    gp_widget_set_value(*widget, &buf);
    return GP_OK;
}

static int
_put_Panasonic_AdjustAB(CONFIG_PUT_ARGS)
{
    PTPParams *params = &(camera->pl->params);
    char *xval;
    uint32_t val;

    CR (gp_widget_get_value(widget, &xval));
    int16_t adj;
    sscanf (xval, "%hd", &adj);
    if (adj < 0) {
        adj = abs(adj) + 0x8000;
    }
    val = adj;

    return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_WhiteBalance_ADJ_AB, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_AdjustAB(CONFIG_GET_ARGS) {
    uint32_t currentVal = 0;
    char buf[32];
    PTPParams *params = &(camera->pl->params);
    GPContext *context = ((PTPData *) params->data)->context;

    uint16_t valsize;
    C_PTP_REP (ptp_panasonic_getdeviceproperty(params, PTP_DPC_PANASONIC_WhiteBalance_ADJ_AB, &valsize, &currentVal));

    //printf("retrieved %x with size %x\n", currentVal, valsize);
    if (currentVal & 0x8000) {
        currentVal = (currentVal & 0x7FFF) * -1;
    }
    sprintf(buf, "%d\n", currentVal);

    gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
    gp_widget_set_name (*widget, menu->name);

    gp_widget_set_value(*widget, &buf);
    return GP_OK;
}

static int
_put_Panasonic_ColorTemp(CONFIG_PUT_ARGS)
{
    PTPParams *params = &(camera->pl->params);
    char *xval;
    uint32_t val;

    CR (gp_widget_get_value(widget, &xval));
    uint16_t KSet;
    sscanf (xval, "%hd", &KSet);
    val = KSet;

    return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_WhiteBalance_KSet, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_ColorTemp(CONFIG_GET_ARGS) {
    uint32_t currentVal;
    uint32_t listCount;
    uint32_t *list;
    uint32_t i;
    int	valset = 0;
    char	buf[32];
    PTPParams *params = &(camera->pl->params);
    GPContext *context = ((PTPData *) params->data)->context;

    C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, PTP_DPC_PANASONIC_WhiteBalance_KSet, 2, &currentVal, &list, &listCount));

    //printf("retrieved %lu property values\n", listCount);

    gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
    gp_widget_set_name (*widget, menu->name);

    for (i = 0; i < listCount; i++) {
        sprintf(buf,_("%d"), list[i]);
        if (list[i] == currentVal) {
            gp_widget_set_value (*widget, buf);
            valset = 1;
        }

        gp_widget_add_choice (*widget, buf);
    }
    free(list);
    if (!valset) {
        sprintf(buf,_("Unknown 0x%04x"), currentVal);
        gp_widget_set_value (*widget, buf);
    }
    return GP_OK;
}

static
struct {
    char*	str;
    uint16_t val;
} panasonic_aftable[] = {
    { N_("AF"),             0x0000	},
    { N_("AF macro"),       0x0001	},
    { N_("AF macro (D)"),   0x0002	},
    { N_("MF"),             0x0003	},
    { N_("AF_S"),           0x0004	},
    { N_("AF_C"),           0x0005	},
    { N_("AF_F"),           0x0006	},
};

static int
_put_Panasonic_AFMode(CONFIG_PUT_ARGS)
{
    PTPParams *params = &(camera->pl->params);
    char *xval;
    uint32_t val = 0;
    uint32_t i, found;

    CR (gp_widget_get_value(widget, &xval));

    for (i=0;i<sizeof(panasonic_aftable)/sizeof(panasonic_aftable[0]);i++) {
        if (!strcmp(panasonic_aftable[i].str, xval)) {
            val = panasonic_aftable[i].val;
	    found = 1;
            break;
        }
    }
    if (!found) return GP_ERROR;

    return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_AFArea_AFModeParam, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_AFMode(CONFIG_GET_ARGS) {
    uint32_t currentVal;
    uint32_t listCount;
    uint32_t *list;
    uint32_t i, j;
    int	valset = 0;
    char	buf[32];
    PTPParams *params = &(camera->pl->params);
    GPContext *context = ((PTPData *) params->data)->context;

    C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, PTP_DPC_PANASONIC_AFArea_AFModeParam, 2, &currentVal, &list, &listCount));

    //printf("retrieved %lu property values\n", listCount);

    gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
    gp_widget_set_name (*widget, menu->name);

    for (i = 0; i < listCount; i++) {
        for (j=0;j<sizeof(panasonic_aftable)/sizeof(panasonic_aftable[0]);j++) {
            sprintf(buf,_("%d"), list[i]);
            if ((list[i] == currentVal) && (j == currentVal)) {
                gp_widget_set_value (*widget, panasonic_aftable[j].str);
                valset = 1;
                break;
            }
        }
    }
    for (j=0;j<sizeof(panasonic_aftable)/sizeof(panasonic_aftable[0]);j++) {
        gp_widget_add_choice (*widget, panasonic_aftable[j].str);
    }
    free(list);
    if (!valset) {
        sprintf(buf,_("Unknown 0x%04x"), currentVal);
        gp_widget_set_value (*widget, buf);
    }
    return GP_OK;
}

static
struct {
    char*	str;
    uint16_t val;
} panasonic_mftable[] = {
    { N_("Stop"),        0x0000	},
    { N_("Far fast"),    0x0001	},
    { N_("Far slow"),    0x0002	},
    { N_("Near slow"),   0x0003	},
    { N_("Near fast"),   0x0004	},
};

static int
_put_Panasonic_MFAdjust(CONFIG_PUT_ARGS)
{
    PTPParams *params = &(camera->pl->params);
    char *xval;
    uint32_t val = 0;
    uint32_t i;

    CR (gp_widget_get_value(widget, &xval));
    for (i=0;i<sizeof(panasonic_mftable)/sizeof(panasonic_mftable[0]);i++) {
        if(!strcmp(panasonic_mftable[i].str, xval)) {
            val = panasonic_mftable[i].val;
            break;
        }
    }

    return translate_ptp_result (ptp_panasonic_manualfocusdrive (params, (uint16_t)val));
}

static int
_get_Panasonic_MFAdjust(CONFIG_GET_ARGS) {
    uint32_t i;
    gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
    gp_widget_set_name (*widget,menu->name);

    for (i=0;i<sizeof(panasonic_mftable)/sizeof(panasonic_mftable[0]);i++) {
        gp_widget_add_choice (*widget, panasonic_mftable[i].str);
    }
    gp_widget_set_value (*widget, _("None"));
    return GP_OK;
}

static
struct {
    char*	str;
    uint16_t val;
} panasonic_rmodetable[] = {
    { N_("P"),   0x0000	},
    { N_("A"),   0x0001	},
    { N_("S"),   0x0002	},
    { N_("M"),   0x0003	},
};

static int
_get_Panasonic_ExpMode(CONFIG_GET_ARGS) {

    uint32_t currentVal;
    uint32_t listCount;
    uint32_t *list;
    uint32_t i,j;
    int	valset = 0;
    char	buf[32];
    PTPParams *params = &(camera->pl->params);
    GPContext *context = ((PTPData *) params->data)->context;

    C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, 0x06000011, 2, &currentVal, &list, &listCount));

    //printf("retrieved %lu property values\n", listCount);

    gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
    gp_widget_set_name (*widget, menu->name);

    for (j=0;j<sizeof(panasonic_rmodetable)/sizeof(panasonic_rmodetable[0]);j++) {
        gp_widget_add_choice (*widget, panasonic_rmodetable[j].str);
    }

    for (i = 0; i < listCount; i++) {
        for (j=0;j<sizeof(panasonic_rmodetable)/sizeof(panasonic_rmodetable[0]);j++) {
            sprintf(buf,_("%d"), list[i]);
            if ((list[i] == currentVal) && (j == currentVal)) {
                gp_widget_set_value (*widget, panasonic_rmodetable[j].str);
                valset = 1;
                break;
            }
    	}
    }
    free(list);
    if (!valset) {
        sprintf(buf,_("Unknown 0x%04x"), currentVal);
        gp_widget_set_value (*widget, buf);
    }
    return GP_OK;
}

static int
_put_Panasonic_ExpMode(CONFIG_PUT_ARGS)
{
    PTPParams *params = &(camera->pl->params);
    char *xval;
    uint32_t val = 0;
    uint32_t i;

    CR (gp_widget_get_value(widget, &xval));
    for (i=0;i<sizeof(panasonic_rmodetable)/sizeof(panasonic_rmodetable[0]);i++) {
        if(!strcmp(panasonic_rmodetable[i].str, xval)) {
            val = panasonic_rmodetable[i].val;
            break;
        }
    }

    //printf("val : %d\n", val);
    return translate_ptp_result (ptp_panasonic_recordmode(params, (uint16_t)val));
}
static
struct {
    char*	str;
    uint16_t val;
} panasonic_recordtable[] = {
    { N_("Standby"),        0x0000	},
    { N_("Recording"),      0x0001	},
    { N_("Playing"),        0x0002	},
    { N_("Other process."), 0x0003	},
    { N_("Other playing"),  0x0004	},
    { N_("Noise reduction"),0x0005	},
    { N_("Displaying menu"),0x0006	},
    { N_("Streaming"),   	0x0007	},
};

static int
_get_Panasonic_Recording(CONFIG_GET_ARGS) {
    uint32_t currentVal = 0;
    char buf[32];
    uint32_t i;
    PTPParams *params = &(camera->pl->params);
    GPContext *context = ((PTPData *) params->data)->context;

    uint16_t valsize;
    C_PTP_REP (ptp_panasonic_getrecordingstatus(params, 0x12000013, &valsize, &currentVal));

    gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
    gp_widget_set_name (*widget, menu->name);

    for(i = 0; i < sizeof(panasonic_recordtable) / sizeof(panasonic_recordtable[0]); i++) {
        if (currentVal == panasonic_recordtable[i].val) {
            strcpy(buf, panasonic_recordtable[i].str);
        }
    }

    gp_widget_set_value(*widget, &buf);
    return GP_OK;
}

    static int
_put_Panasonic_Recording(CONFIG_PUT_ARGS)
{
    PTPParams *params = &(camera->pl->params);
    char *xval;

    CR (gp_widget_get_value(widget, &xval));
    if (!strcmp(xval, "start")) {
        return translate_ptp_result (ptp_panasonic_startrecording(params));
    } else if (!strcmp(xval, "stop")) {
        return translate_ptp_result (ptp_panasonic_stoprecording(params));
    } else {
        return GP_ERROR;
    }
}

static int
_put_Panasonic_Whitebalance(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	char	*xval;
	uint32_t val = 0;
/*
	uint32_t currentVal;
	uint32_t listCount;
	uint32_t *list;
*/
	int	ival;
	unsigned int j;

	CR (gp_widget_get_value(widget, &xval));

	/* C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, PTP_DPC_PANASONIC_WhiteBalance, 2, &currentVal, &list, &listCount)); */

	if (sscanf(xval,_("Unknown 0x%04x"), &ival))
		val = ival;
	for (j=0;j<sizeof(panasonic_wbtable)/sizeof(panasonic_wbtable[0]);j++) {
		if (!strcmp(xval,_(panasonic_wbtable[j].str))) {
			val = panasonic_wbtable[j].val;
			break;
		}
	}
	/* free(list); */
	GP_LOG_D("setting whitebalance to 0x%04x", val);
	return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_WhiteBalance_Param, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_Whitebalance(CONFIG_GET_ARGS) {
	uint32_t currentVal;
	uint32_t listCount;
	uint32_t *list;
	uint32_t i,j;
	int	valset = 0;
	char	buf[32];
	PTPParams *params = &(camera->pl->params);
	GPContext *context = ((PTPData *) params->data)->context;

	C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, PTP_DPC_PANASONIC_WhiteBalance_Param, 2, &currentVal, &list, &listCount));

	//printf("retrieved %lu property values\n", listCount);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i = 0; i < listCount; i++) {
		sprintf(buf,_("Unknown 0x%04x"), list[i]);
		for (j=0;j<sizeof(panasonic_wbtable)/sizeof(panasonic_wbtable[0]);j++) {
			if (panasonic_wbtable[j].val == list[i]) {
				strcpy(buf,_(panasonic_wbtable[j].str));
				break;
			}
		}
		if (list[i] == currentVal) {
			gp_widget_set_value (*widget, buf);
			valset = 1;
		}

		gp_widget_add_choice (*widget, buf);
	}
	free(list);
	if (!valset) {
		sprintf(buf,_("Unknown 0x%04x"), currentVal);
		gp_widget_set_value (*widget, buf);
	}
	return GP_OK;
}
static int
_put_Panasonic_Exposure(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	char *xval;
	uint32_t val;
	float f;

	CR (gp_widget_get_value(widget, &xval));

	sscanf (xval, "%f", &f);

	if (f < 0)
		val = (uint32_t)(0x8000 | (int)(((-f)*3)));
	else
		val = (uint32_t) (f*3);
	return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, PTP_DPC_PANASONIC_Exposure_Param, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_Exposure(CONFIG_GET_ARGS) {
	uint32_t currentVal;
	uint32_t listCount;
	uint32_t *list;
	uint32_t i;
	char	buf[16];
	PTPParams *params = &(camera->pl->params);
	GPContext *context = ((PTPData *) params->data)->context;

	C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, PTP_DPC_PANASONIC_Exposure, 2, &currentVal, &list, &listCount));

	//printf("retrieved %lu property values\n", listCount);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i = 0; i < listCount; i++) {
		int val = (int)list[i];

		if (val & 0x8000)
			val = -(val & 0x7fff);
		sprintf (buf, "%f", val/3.0);
		gp_widget_add_choice (*widget, buf);
		if ((int)list[i] == (int)currentVal) {
			sprintf (buf, "%f", val/3.0);
			gp_widget_set_value (*widget, &buf);
		}
	}
	free(list);

	return GP_OK;
}

static int
_put_Panasonic_LiveViewSize(CONFIG_PUT_ARGS)
{
	PTPParams		*params = &(camera->pl->params);
	char			*xval;
	unsigned int		height, width, freq, x;
	PanasonicLiveViewSize	liveviewsize;

	CR (gp_widget_get_value(widget, &xval));
	if (!sscanf(xval, "%dx%d %d %dHZ", &width, &height, &x, &freq))
		return GP_ERROR;
	liveviewsize.width	= width;
	liveviewsize.height	= height;
	liveviewsize.freq	= freq;
	liveviewsize.x		= x;
	return translate_ptp_result (ptp_panasonic_9415(params, &liveviewsize));
}

static int
_get_Panasonic_LiveViewSize(CONFIG_GET_ARGS) {
	unsigned int		i;
	char			buf[100];
	PTPParams		*params = &(camera->pl->params);
	GPContext		*context = ((PTPData *) params->data)->context;
	PanasonicLiveViewSize	liveviewsize, *liveviewsizes = NULL;
	unsigned int		nrofliveviewsizes = 0;

	C_PTP_REP (ptp_panasonic_9414_0d800012(params, &liveviewsizes, &nrofliveviewsizes));

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (i = 0;i < nrofliveviewsizes; i++) {
		sprintf(buf,"%dx%d %d %dHZ", liveviewsizes[i].width, liveviewsizes[i].height, liveviewsizes[i].x, liveviewsizes[i].freq);
                gp_widget_add_choice (*widget, buf);
	}
	free (liveviewsizes);

	C_PTP_REP (ptp_panasonic_9414_0d800011(params, &liveviewsize));
	sprintf(buf,"%dx%d %d %dHZ", liveviewsize.width, liveviewsize.height, liveviewsize.x, liveviewsize.freq);
	gp_widget_set_value (*widget, buf);
	return GP_OK;
}

static int
_put_Panasonic_FNumber(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	char *xval;
	uint32_t val;

	CR (gp_widget_get_value(widget, &xval));
	float f;
	sscanf (xval, "%f", &f);
	val = (uint32_t) (f*10);

	//printf("setting ISO to %lu (%s)\n", val, xval);

	return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, 0x2000041, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_FNumber(CONFIG_GET_ARGS) {
	uint32_t currentVal;
	uint32_t listCount;
	uint16_t valsize;
	uint32_t *list;
	PTPParams *params = &(camera->pl->params);
	GPContext *context = ((PTPData *) params->data)->context;

	C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, 0x2000040, 2, &currentVal, &list, &listCount));

	//printf("retrieved %lu property values\n", listCount);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	uint32_t i;
	float f;
	char buf[16];
	for (i = 0; i < listCount; i++) {
		f = (float) list[i];
		f /= 10.0;
		if(list[i] % 10 == 0) {
			sprintf (buf, "%.0f", f);
		} else {
			sprintf (buf, "%.1f", f);
		}
		gp_widget_add_choice (*widget, buf);
	}
	ptp_panasonic_getdeviceproperty(params, 0x2000041, &valsize, &currentVal);

	f = (float) currentVal;
	f /= 10.0;
	if(currentVal % 10 == 0) {
		sprintf (buf, "%.0f", f);
	} else {
		sprintf (buf, "%.1f", f);
	}

	gp_widget_set_value (*widget, &buf);

	free(list);

	return GP_OK;
}

static int
_put_Panasonic_ImageFormat(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	char *xval;
	uint32_t val;
	unsigned int uval;

	CR (gp_widget_get_value(widget, &xval));

	sscanf (xval, "%u", &uval);
	val = uval;
	//printf("setting ImageFormat to %lu (%s)\n", val, xval);

	return translate_ptp_result (ptp_panasonic_setdeviceproperty(params, 0x20000A2, (unsigned char*)&val, 2));
}

static int
_get_Panasonic_ImageFormat(CONFIG_GET_ARGS) {
	uint32_t currentVal;
	uint32_t listCount;
	uint32_t *list;
	PTPParams *params = &(camera->pl->params);
	GPContext *context = ((PTPData *) params->data)->context;

	C_PTP_REP (ptp_panasonic_getdevicepropertydesc(params, 0x20000A2, 2, &currentVal, &list, &listCount));

	//printf("retrieved %lu property values\n", listCount);

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	uint32_t i;
	char buf[16];
	for (i = 0; i < listCount; i++) {
		sprintf (buf, "%u", (unsigned int)list[i]);
		gp_widget_add_choice (*widget, buf);
	}

	sprintf (buf, "%u", (unsigned int)currentVal);
	gp_widget_set_value (*widget, buf);

	free(list);

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

	if (val) {
		if (!params->uilocked)
			C_PTP_REP (ptp_canon_eos_setuilock (params));
		params->uilocked = 1;
	} else {
		if (params->uilocked)
			C_PTP_REP (ptp_canon_eos_resetuilock (params));
		params->uilocked = 0;
	}
	return GP_OK;
}

static int
_get_Canon_EOS_PopupFlash(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	val = 2; /* always changed */
	gp_widget_set_value  (*widget, &val);
	return GP_OK;
}

static int
_put_Canon_EOS_PopupFlash(CONFIG_PUT_ARGS)
{
	PTPParams *params = &(camera->pl->params);
	GPContext *context = ((PTPData *) params->data)->context;

	C_PTP_REP (ptp_canon_eos_popupflash (params));
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

static int
_get_Nikon_Thumbsize(CONFIG_GET_ARGS) {
	char buf[1024];

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_add_choice (*widget, _("normal"));
	gp_widget_add_choice (*widget, _("large"));
	gp_widget_set_name (*widget, menu->name);
	strcpy(buf,"normal");
	gp_setting_get("ptp2","thumbsize", buf);
	gp_widget_set_value  (*widget, N_(buf));
	return GP_OK;
}

static int
_put_Nikon_Thumbsize(CONFIG_PUT_ARGS) {
	char *buf;
	PTPParams	*params = &(camera->pl->params);
	GPContext	*context = ((PTPData *) params->data)->context;

	CR (gp_widget_get_value  (widget, &buf));
	if (!strcmp(buf,_("normal"))) {
		gp_setting_set("ptp2","thumbsize","normal");
		return GP_OK;
	}
	if (!strcmp(buf,_("large"))) {
		gp_setting_set("ptp2","thumbsize","large");
		return GP_OK;
	}
	gp_context_error (context, _("Unknown thumb size value '%s'."), buf);
	return GP_ERROR;
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
	unsigned int i;
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
	unsigned int	i;
	char		*val;
	PTPParams	*params = &(camera->pl->params);
	GPContext	*context = ((PTPData *) params->data)->context;
	char		buf[1024];

	CR (gp_widget_get_value(widget, &val));
	for (i=0;i<sizeof(capturetargets)/sizeof(capturetargets[i]);i++) {
		if (!strcmp( val, _(capturetargets[i].label))) {
			gp_setting_set("ptp2","capturetarget",capturetargets[i].name);
			break;
		}
	}
	/* Also update it in the live Canon EOS camera. (Nikon and Canon Powershot just use different opcodes.) */
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		(ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
		 ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)
		)
	)
		CR (camera_canon_eos_update_capture_target( camera, context, -1 ));

	if (params->deviceinfo.VendorExtensionID == PTP_VENDOR_PANASONIC) {
		uint16_t	target;

		C_PTP (ptp_panasonic_getcapturetarget(params, &target));
		if ((GP_OK != gp_setting_get("ptp2","capturetarget",buf)) || !strcmp(buf,"sdram"))
			C_PTP (ptp_panasonic_setcapturetarget(params, 1));
		else
			C_PTP (ptp_panasonic_setcapturetarget(params, 0));
	}
	return GP_OK;
}

static struct deviceproptableu16 sony_capturetarget[] = {
	{ "sdram",		0x0001, 0 },
	{ "card",		0x0010, 0 },
	{ "card+sdram",		0x0011, 0 },
};
GENERIC16TABLE(Sony_CaptureTarget,sony_capturetarget)

static struct deviceproptableu32 audio_format[] = {
	{ "PCM",		0x0001, 0 },
	{ "ADPCM",		0x0002, 0 },
	{ "IEEE float",		0x0003, 0 },
	{ "VSELP",		0x0004, 0 },
	{ "IBM CVSD",		0x0005, 0 },
	{ "a-Law",		0x0006, 0 },
	{ "u-Law",		0x0007, 0 },
	{ "DTS",		0x0008, 0 },
	{ "DRM",		0x0009, 0 },
	{ "OKI-ADPCM",		0x0010, 0 },
	{ "IMA-ADPCM",		0x0011, 0 },
	{ "Mediaspace ADPCM",	0x0012, 0 },
	{ "Sierra ADPCM",	0x0013, 0 },
	{ "G723 ADPCM",		0x0014, 0 },
	{ "DIGISTD",		0x0015, 0 },
	{ "DIGIFIX",		0x0016, 0 },
	{ "Dolby AC2",		0x0030, 0 },
	{ "GSM 610",		0x0031, 0 },
	{ "Rockwell ADPCM",	0x003b, 0 },
	{ "Rockwell DIGITALK",	0x003c, 0 },
	{ "G721 ADPCM",		0x0040, 0 },
	{ "G728 CELP",		0x0041, 0 },
	{ "MPEG",		0x0050, 0 },
	{ "RT24",		0x0052, 0 },
	{ "PAC",		0x0053, 0 },
	{ "MP3",		0x0055, 0 },
	{ "G726 ADPCM",		0x0064, 0 },
	{ "G722 ADPCM",		0x0065, 0 },
	{ "IBM u-Law",		0x0101, 0 },
	{ "IBM a-Law",		0x0102, 0 },
	{ "IBM ADPCM",		0x0103, 0 },
	{ "Ogg Vorbis 1",	0x674f, 0 },
	{ "Ogg Vorbis 1 PLUS",	0x676f, 0 },
	{ "Ogg Vorbis 2",	0x6750, 0 },
	{ "Ogg Vorbis 2 PLUS",	0x6770, 0 },
	{ "Ogg Vorbis 3",	0x6751, 0 },
	{ "Ogg Vorbis 3 PLUS",	0x6771, 0 },
	{ "Ogg Vorbis 3 PLUS",	0x6771, 0 },
	/* Development 0xffff */
	/* Reserved 0xffff...  */
};
GENERIC32TABLE(Audio_Format,audio_format)

static struct {
	char	*name;
	char	*label;
} chdkonoff[] = {
	{"on", N_("On") },
	{"off", N_("Off") },
};

static int
_get_CHDK(CONFIG_GET_ARGS) {
	unsigned int i;
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
	unsigned int i;
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

static struct {
	char	*name;
	char	*label;
} afonoff[] = {
	{"on", N_("On") },
	{"off", N_("Off") },
};

static int
_get_Autofocus(CONFIG_GET_ARGS) {
	unsigned int i;
	char buf[1024];

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (GP_OK != gp_setting_get("ptp2","autofocus", buf))
		strcpy(buf,"on");
	for (i=0;i<sizeof (afonoff)/sizeof (afonoff[i]);i++) {
		gp_widget_add_choice (*widget, _(afonoff[i].label));
		if (!strcmp (buf,afonoff[i].name))
			gp_widget_set_value (*widget, _(afonoff[i].label));
	}
	return GP_OK;
}

static int
_put_Autofocus(CONFIG_PUT_ARGS) {
	unsigned int i;
	char *val;

	CR (gp_widget_get_value(widget, &val));
	for (i=0;i<sizeof(afonoff)/sizeof(afonoff[i]);i++) {
		if (!strcmp( val, _(afonoff[i].label))) {
			gp_setting_set("ptp2","autofocus",afonoff[i].name);
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

	        gp_widget_set_changed (subwidget, FALSE);

		ret = cursub->putfunc (camera, subwidget, NULL, NULL, NULL);
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

		ret = cursub->putfunc (camera, subwidget, NULL, NULL, NULL);
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
	{ N_("Synchronize camera date and time with PC (UTC)"),"syncdatetimeutc", PTP_DPC_CANON_EOS_UTCTime, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_SyncTime, _put_Canon_SyncTime },
	{ N_("Synchronize camera date and time with PC"),"syncdatetime", PTP_DPC_CANON_EOS_CameraTime, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_SyncTime, _put_Canon_SyncTime },

	{ N_("Auto-Focus"),                     "autofocus",        PTP_DPC_SONY_AutoFocus, PTP_VENDOR_SONY,   PTP_DTC_UINT16,  _get_Sony_Autofocus,            _put_Sony_Autofocus },
	{ N_("Manual-Focus"),                   "manualfocus",      PTP_DPC_SONY_NearFar,   PTP_VENDOR_SONY,   PTP_DTC_INT16,   _get_Sony_ManualFocus,          _put_Sony_ManualFocus },
	{ N_("Capture"),                        "capture",          PTP_DPC_SONY_Capture,   PTP_VENDOR_SONY,   PTP_DTC_UINT16,  _get_Sony_Capture,              _put_Sony_Capture },
	{ N_("Power Down"),                     "powerdown",        0,  0,                  PTP_OC_PowerDown,                   _get_PowerDown,                 _put_PowerDown },
	{ N_("Focus Lock"),                     "focuslock",        0,  PTP_VENDOR_CANON,   PTP_OC_CANON_FocusLock,             _get_Canon_FocusLock,           _put_Canon_FocusLock },
	{ N_("Bulb Mode"),                      "bulb",             PTP_DPC_SONY_StillImage,PTP_VENDOR_SONY,   0,               _get_Sony_Bulb,                 _put_Sony_Bulb },
	{ N_("Bulb Mode"),                      "bulb",             0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_BulbStart,         _get_Canon_EOS_Bulb,            _put_Canon_EOS_Bulb },
	{ N_("Bulb Mode"),                      "bulb",             0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_TerminateCapture,      _get_Nikon_Bulb,                _put_Nikon_Bulb },
	{ N_("Bulb Mode"),                      "bulb",             0,  PTP_VENDOR_GP_OLYMPUS_OMD,   PTP_OC_OLYMPUS_OMD_Capture,      _get_Olympus_OMD_Bulb,                _put_Olympus_OMD_Bulb },
	{ N_("Bulb Mode"),                      "bulb",             0,  PTP_VENDOR_FUJI,    PTP_OC_InitiateCapture,             _get_Fuji_Bulb,                 _put_Fuji_Bulb },
	{ N_("UI Lock"),                        "uilock",           0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_SetUILock,         _get_Canon_EOS_UILock,          _put_Canon_EOS_UILock },
	{ N_("Popup Flash"),                    "popupflash",       0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_PopupBuiltinFlash, _get_Canon_EOS_PopupFlash,      _put_Canon_EOS_PopupFlash },
	{ N_("Drive Nikon DSLR Autofocus"),     "autofocusdrive",   0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_AfDrive,               _get_Nikon_AFDrive,             _put_Nikon_AFDrive },
	{ N_("Drive Canon DSLR Autofocus"),     "autofocusdrive",   0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_DoAf,              _get_Canon_EOS_AFDrive,         _put_Canon_EOS_AFDrive },
	{ N_("Drive Fuji Autofocus"),           "autofocusdrive",   0,  PTP_VENDOR_FUJI,    0,               			_get_Fuji_AFDrive,              _put_Fuji_AFDrive },
	{ N_("Drive Nikon DSLR Manual focus"),  "manualfocusdrive", 0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_MfDrive,               _get_Nikon_MFDrive,             _put_Nikon_MFDrive },
	{ N_("Set Nikon Autofocus area"),       "changeafarea",     0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_ChangeAfArea,          _get_Nikon_ChangeAfArea,        _put_Nikon_ChangeAfArea },
	{ N_("Set Nikon Control Mode"),         "controlmode",      0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_ChangeCameraMode,      _get_Nikon_ControlMode,         _put_Nikon_ControlMode },
	{ N_("Drive Canon DSLR Manual focus"),  "manualfocusdrive", 0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_DriveLens,         _get_Canon_EOS_MFDrive,         _put_Canon_EOS_MFDrive },
	{ N_("Cancel Canon DSLR Autofocus"),    "cancelautofocus",  0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_AfCancel,          _get_Canon_EOS_AFCancel,        _put_Canon_EOS_AFCancel },
	{ N_("Drive Olympus OMD Manual focus"), "manualfocusdrive", 0,  PTP_VENDOR_GP_OLYMPUS_OMD, PTP_OC_OLYMPUS_OMD_MFDrive,	_get_Olympus_OMD_MFDrive,	_put_Olympus_OMD_MFDrive },
	{ N_("Drive Panasonic Manual focus"),   "manualfocusdrive", 0,  PTP_VENDOR_PANASONIC, PTP_OC_PANASONIC_ManualFocusDrive,_get_Panasonic_MFDrive,		_put_Panasonic_MFDrive },
	{ N_("Get Fuji focuspoint"),		"focuspoint",	    PTP_DPC_FUJI_FocusPoint,  PTP_VENDOR_FUJI, PTP_DTC_STR,	_get_Fuji_FocusPoint,		_put_Fuji_FocusPoint },
	{ N_("Fuji FocusPoint Grid dimensions"),"focuspoints",	    PTP_DPC_FUJI_FocusPoints, PTP_VENDOR_FUJI, 0,		_get_Fuji_FocusPoints,		_put_Fuji_FocusPoints },
	{ N_("Fuji Zoom Position"),		"zoompos",	    PTP_DPC_FUJI_LensZoomPos, PTP_VENDOR_FUJI, PTP_DTC_UINT16,	_get_INT,			_put_None },

	{ N_("Canon EOS Zoom"),                 "eoszoom",          0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_Zoom,              _get_Canon_EOS_Zoom,            _put_Canon_EOS_Zoom },
	{ N_("Canon EOS Zoom Position"),        "eoszoomposition",  0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_ZoomPosition,      _get_Canon_EOS_ZoomPosition,    _put_Canon_EOS_ZoomPosition },
	{ N_("Canon EOS Viewfinder"),           "viewfinder",       0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_GetViewFinderData, _get_Canon_EOS_ViewFinder,      _put_Canon_EOS_ViewFinder },
	{ N_("Panasonic Viewfinder"),           "viewfinder",       0,  PTP_VENDOR_PANASONIC, 0, 								_get_Panasonic_ViewFinder,      _put_Panasonic_ViewFinder },
	{ N_("Nikon Viewfinder"),               "viewfinder",       0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_StartLiveView,         _get_Nikon_ViewFinder,          _put_Nikon_ViewFinder },
	{ N_("Canon EOS Remote Release"),       "eosremoterelease", 0,  PTP_VENDOR_CANON,   PTP_OC_CANON_EOS_RemoteReleaseOn,   _get_Canon_EOS_RemoteRelease,   _put_Canon_EOS_RemoteRelease },
	{ N_("CHDK Script"),                    "chdk_script",      0,  PTP_VENDOR_CANON,   PTP_OC_CHDK,                        _get_Canon_CHDK_Script,         _put_Canon_CHDK_Script },
	{ N_("Movie Capture"),                  "movie",            0,  0,                  PTP_OC_InitiateOpenCapture,         _get_OpenCapture,               _put_OpenCapture },
	{ N_("Movie Capture"),                  "movie",            0,  PTP_VENDOR_NIKON,   PTP_OC_NIKON_StartMovieRecInCard,   _get_Nikon_Movie,               _put_Nikon_Movie },
	{ N_("Movie Capture"),                  "movie",            0,  PTP_VENDOR_SONY,    PTP_OC_SONY_SDIOConnect,            _get_Sony_Movie,                _put_Sony_Movie },
	{ N_("Movie Capture"),                  "movie",            0,  PTP_VENDOR_SONY,    PTP_OC_SONY_QX_Connect,             _get_Sony_QX_Movie,             _put_Sony_QX_Movie },
	{ N_("Movie Capture"),                  "movie",            0,  PTP_VENDOR_PANASONIC,PTP_OC_PANASONIC_MovieRecControl,  _get_Panasonic_Movie,           _put_Panasonic_Movie },
	{ N_("Movie Mode"),                     "eosmoviemode",     0,  PTP_VENDOR_CANON,   0,                                  _get_Canon_EOS_MovieModeSw,     _put_Canon_EOS_MovieModeSw },
	{ N_("Focus Magnify"),                  "focusmagnify",     PTP_DPC_SONY_FocusMagnify, PTP_VENDOR_SONY, PTP_DTC_UINT16, _get_Sony_FocusMagnifyProp,     _put_Sony_FocusMagnifyProp },
	{ N_("Focus Magnify Exit"),             "focusmagnifyexit", PTP_DPC_SONY_FocusMagnifyExit, PTP_VENDOR_SONY, PTP_DTC_UINT16, _get_Sony_FocusMagnifyProp, _put_Sony_FocusMagnifyProp },
	{ N_("Focus Magnify Up"),               "focusmagnifyup",   PTP_DPC_SONY_FocusMagnifyUp, PTP_VENDOR_SONY, PTP_DTC_UINT16, _get_Sony_FocusMagnifyProp,   _put_Sony_FocusMagnifyProp },
	{ N_("Focus Magnify Down"),             "focusmagnifydown", PTP_DPC_SONY_FocusMagnifyDown, PTP_VENDOR_SONY, PTP_DTC_UINT16, _get_Sony_FocusMagnifyProp, _put_Sony_FocusMagnifyProp },
	{ N_("Focus Magnify Left"),             "focusmagnifyleft", PTP_DPC_SONY_FocusMagnifyLeft, PTP_VENDOR_SONY, PTP_DTC_UINT16, _get_Sony_FocusMagnifyProp, _put_Sony_FocusMagnifyProp },
	{ N_("Focus Magnify Right"),            "focusmagnifyright",PTP_DPC_SONY_FocusMagnifyRight, PTP_VENDOR_SONY, PTP_DTC_UINT16, _get_Sony_FocusMagnifyProp,_put_Sony_FocusMagnifyProp },
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
	{ N_("Battery Level"),          "batterylevel",     PTP_DPC_SONY_BatteryLevel,              PTP_VENDOR_SONY,    PTP_DTC_INT8,   _get_SONY_BatteryLevel,         _put_None },
	{ N_("Mirror Up Status"),       "mirrorupstatus",   PTP_DPC_NIKON_MirrorUpStatus,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,		_put_None },
	{ N_("Mirror Lock"),       	"mirrorlock",       PTP_DPC_CANON_EOS_MirrorUpSetting,      PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,			_put_INT },
	{ N_("Mirror Lock Status"),     "mirrorlockstatus", PTP_DPC_CANON_EOS_MirrorLockupState,    PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,			_put_None },
	{ N_("Mirror Down Status"),     "mirrordownstatus", PTP_DPC_CANON_EOS_MirrorDownStatus,     PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,			_put_None },
	{ N_("Mirror Up Shooting Count"),"mirrorupshootingcount", PTP_DPC_NIKON_MirrorUpReleaseShootingCount, PTP_VENDOR_NIKON, PTP_DTC_UINT8,  _get_INT,		_put_None },
	{ N_("Continuous Shooting Count"),"continousshootingcount", PTP_DPC_NIKON_ContinousShootingCount, PTP_VENDOR_NIKON, PTP_DTC_UINT8,_get_INT,			_put_None },
	{ N_("Camera Orientation"),     "orientation",      PTP_DPC_NIKON_CameraOrientation,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_CameraOrientation,   _put_None },
	{ N_("Camera Orientation"),     "orientation2",     PTP_DPC_NIKON_AngleLevel,               PTP_VENDOR_NIKON,   PTP_DTC_INT32,  _get_Nikon_AngleLevel,		_put_None },
	{ N_("Camera Orientation"),     "orientation",      PTP_DPC_CANON_RotationAngle,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_CameraOrientation,   _put_None },
	{ N_("Flash Open"),             "flashopen",        PTP_DPC_NIKON_FlashOpen,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("Active Folder"),          "activefolder",     PTP_DPC_NIKON_ActiveFolder,             PTP_VENDOR_NIKON,   PTP_DTC_UINT16, _get_INT,         		_put_None },
	{ N_("Flash Charged"),          "flashcharged",     PTP_DPC_NIKON_FlashCharged,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_None },
	{ N_("Lens Name"),              "lensname",         PTP_DPC_NIKON_LensID,                   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LensID,              _put_None },
	{ N_("Lens Name"),              "lensname",         PTP_DPC_CANON_EOS_LensName,             PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Lens Name"),              "lensname",         PTP_DPC_FUJI_LensNameAndSerial,         PTP_VENDOR_FUJI,    PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Serial Number"),          "eosserialnumber",  PTP_DPC_CANON_EOS_SerialNumber,         PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Shutter Counter"),        "shuttercounter",   PTP_DPC_CANON_EOS_ShutterCounter,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Shutter Counter"),        "shuttercounter",   PTP_DPC_FUJI_TotalShotCount,            PTP_VENDOR_FUJI,    PTP_DTC_UINT32, _get_Fuji_Totalcount,           _put_None },
	{ N_("Available Shots"),        "availableshots",   PTP_DPC_CANON_EOS_AvailableShots,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Available Shots"),        "availableshots",   PTP_DPC_NIKON_ExposureRemaining,        PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
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
	{ N_("Movie Switch"),	        "eosmovieswitch",   PTP_DPC_CANON_EOS_FixedMovie,           PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Movie Prohibit Condition"), "movieprohibit",  PTP_DPC_NIKON_MovRecProhibitCondition,  PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_MovieProhibitCondition, _put_None },
	{ N_("Liveview Prohibit Condition"), "liveviewprohibit", PTP_DPC_NIKON_LiveViewProhibitCondition, PTP_VENDOR_NIKON, PTP_DTC_UINT32, _get_Nikon_LiveViewProhibitCondition, _put_None },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu camera_settings_menu[] = {
	{ N_("Camera Date and Time"),   "datetimeutc",          PTP_DPC_CANON_EOS_UTCTime,          PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_UINT32_as_time,            _put_UINT32_as_time },
	{ N_("Camera Date and Time"),   "datetime",             PTP_DPC_CANON_UnixTime,             PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_UINT32_as_localtime,       _put_UINT32_as_localtime },
	{ N_("Camera Date and Time"),   "datetime",             PTP_DPC_CANON_EOS_CameraTime,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_UINT32_as_localtime,       _put_UINT32_as_localtime },
	{ N_("Camera Date and Time"),   "datetime",             PTP_DPC_DateTime,                   0,                  PTP_DTC_STR,    _get_STR_as_time,               _put_STR_as_time },
	{ N_("Camera Date and Time"),   "datetime",             PTP_DPC_SONY_QX_DateTime,           PTP_VENDOR_SONY,    PTP_DTC_STR,    _get_STR_as_time,               _put_STR_as_time },
	{ N_("Beep Mode"),              "beep",                 PTP_DPC_CANON_BeepMode,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_BeepMode,            _put_Canon_BeepMode },
	{ N_("Image Comment"),          "imagecomment",         PTP_DPC_NIKON_ImageCommentString,   PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Image Comment"),          "imagecomment",         PTP_DPC_FUJI_Comment,               PTP_VENDOR_FUJI,    PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Device Name"),            "devicename",           PTP_DPC_FUJI_DeviceName,            PTP_VENDOR_FUJI,    PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("WLAN GUID"),          	"guid",         	PTP_DPC_NIKON_GUID,   		    PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Enable Image Comment"),   "imagecommentenable",   PTP_DPC_NIKON_ImageCommentEnable,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("LCD Off Time"),           "lcdofftime",           PTP_DPC_NIKON_MonitorOff,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LCDOffTime,          _put_Nikon_LCDOffTime },
	{ N_("Recording Media"),        "recordingmedia",       PTP_DPC_NIKON_RecordingMedia,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_RecordingMedia,      _put_Nikon_RecordingMedia },
	{ N_("Quick Review Time"),      "reviewtime",           PTP_DPC_CANON_EOS_QuickReviewTime,  PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_QuickReviewTime, _put_Canon_EOS_QuickReviewTime },
	{ N_("CSM Menu"),               "csmmenu",              PTP_DPC_NIKON_CSMMenu,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("Reverse Command Dial"),   "reversedial",          PTP_DPC_NIKON_ReverseCommandDial,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("Camera Output"),          "output",               PTP_DPC_CANON_CameraOutput,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_CameraOutput,        _put_Canon_CameraOutput },
	{ N_("Camera Output"),          "output",               PTP_DPC_CANON_EOS_EVFOutputDevice,  PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_Canon_EOS_CameraOutput,    _put_Canon_EOS_CameraOutput },
	{ N_("Recording Destination"),  "movierecordtarget",    PTP_DPC_CANON_EOS_EVFRecordStatus,  PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_EVFRecordTarget, _put_Canon_EOS_EVFRecordTarget },
	{ N_("EVF Mode"),               "evfmode",              PTP_DPC_CANON_EOS_EVFMode,          PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_EVFMode,         _put_Canon_EOS_EVFMode },
	{ N_("Owner Name"),             "ownername",            PTP_DPC_CANON_CameraOwner,          PTP_VENDOR_CANON,   PTP_DTC_AUINT8, _get_AUINT8_as_CHAR_ARRAY,      _put_AUINT8_as_CHAR_ARRAY },
	{ N_("Owner Name"),             "ownername",            PTP_DPC_CANON_EOS_Owner,            PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Artist"),                 "artist",               PTP_DPC_CANON_EOS_Artist,           PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Artist"),                 "artist",               PTP_DPC_NIKON_ArtistName,           PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("CCD Number"),             "ccdnumber",            PTP_DPC_NIKON_CCDNumber,            PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Copyright"),              "copyright",            PTP_DPC_CANON_EOS_Copyright,        PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Nickname"),               "nickname",             PTP_DPC_CANON_EOS_CameraNickname,   PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Copyright"),              "copyright",            PTP_DPC_FUJI_Copyright,             PTP_VENDOR_FUJI,    PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Copyright"),              "copyright",            PTP_DPC_NIKON_CopyrightInfo,        PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Clean Sensor"),           "cleansensor",          PTP_DPC_NIKON_CleanImageSensor,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_CleanSensor,         _put_Nikon_CleanSensor },
	{ N_("Flicker Reduction"),      "flickerreduction",     PTP_DPC_NIKON_FlickerReduction,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlickerReduction,    _put_Nikon_FlickerReduction },
	{ N_("Custom Functions Ex"),    "customfuncex",         PTP_DPC_CANON_EOS_CustomFuncEx,     PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_STR },
	{ N_("Focus Info"),             "focusinfo",            PTP_DPC_CANON_EOS_FocusInfoEx,      PTP_VENDOR_CANON,   PTP_DTC_STR,    _get_STR,                       _put_None },
	{ N_("Focus Area"),             "focusarea",            PTP_DPC_CANON_EOS_AFSelectFocusArea,PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Strobo Firing"),          "strobofiring",         PTP_DPC_CANON_EOS_StroboFiring,     PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Flash Charging State"),   "flashcharged",         PTP_DPC_CANON_EOS_FlashChargingState,PTP_VENDOR_CANON,  PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("One Shot Raw On"),        "oneshotrawon",         PTP_DPC_CANON_EOS_OneShotRawOn,     PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Auto Power Off"),         "autopoweroff",         PTP_DPC_CANON_EOS_AutoPowerOff,     PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Depth of Field"),         "depthoffield",         PTP_DPC_CANON_EOS_DepthOfFieldPreview, PTP_VENDOR_CANON,PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Menus and Playback"),     "menusandplayback",     PTP_DPC_NIKON_MenusAndPlayback,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_MenusAndPlayback,    _put_Nikon_MenusAndPlayback },
	{ N_("External Recording Control"),     "externalrecordingcontrol", PTP_DPC_NIKON_ExternalRecordingControl,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,    _put_Nikon_OffOn_UINT8 },
	{ N_("Camera Action"),          "cameraaction", 	0xd208, 			    PTP_VENDOR_FUJI,	PTP_DTC_UINT16,	_get_Fuji_Action,		_put_Fuji_Action },
	{ N_("Priority Mode"),		"prioritymode",		PTP_DPC_SONY_PriorityMode,  	    PTP_VENDOR_SONY,	PTP_DTC_INT8, 	_get_Sony_PriorityMode,     	_put_Sony_PriorityMode },
	{ N_("Priority Mode"),          "prioritymode",         PTP_DPC_FUJI_PriorityMode,          PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_PriorityMode,         _put_Fuji_PriorityMode },

/* virtual */
	{ N_("Thumb Size"),		"thumbsize",    0,  PTP_VENDOR_NIKON,   0,  _get_Nikon_Thumbsize,   _put_Nikon_Thumbsize },
	{ N_("Fast Filesystem"),	"fastfs",	0,  PTP_VENDOR_NIKON,   0,  _get_Nikon_FastFS,      _put_Nikon_FastFS },
	{ N_("Capture Target"),		"capturetarget",0,  PTP_VENDOR_NIKON,   0,  _get_CaptureTarget,     _put_CaptureTarget },
	{ N_("Autofocus"),		"autofocus",    0,  PTP_VENDOR_NIKON,   0,  _get_Autofocus,         _put_Autofocus },
	{ N_("Capture Target"),		"capturetarget",0,  PTP_VENDOR_CANON,   0,  _get_CaptureTarget,     _put_CaptureTarget },
	{ N_("Capture Target"),		"capturetarget",0,  PTP_VENDOR_PANASONIC,0, _get_CaptureTarget,     _put_CaptureTarget },
	{ N_("Capture Target"),		"capturetarget",PTP_DPC_SONY_StillImageStoreDestination,  PTP_VENDOR_SONY,0, _get_Sony_CaptureTarget,     _put_Sony_CaptureTarget },
	{ N_("CHDK"),     		"chdk",		PTP_OC_CHDK,  PTP_VENDOR_CANON,   0,  _get_CHDK,     _put_CHDK },
	{ N_("Capture"),		"capture",	0,  PTP_VENDOR_CANON,   0,  _get_Canon_CaptureMode, _put_Canon_CaptureMode },
	{ N_("Remote Mode"),		"remotemode",	PTP_OC_CANON_EOS_SetRemoteMode,  PTP_VENDOR_CANON,   0,  _get_Canon_RemoteMode, _put_Canon_RemoteMode },
	{ N_("Event Mode"),		"eventmode",	PTP_OC_CANON_EOS_SetEventMode,   PTP_VENDOR_CANON,   0,  _get_Canon_EventMode,  _put_Canon_EventMode },
	{ N_("Test OLC"),		"testolc",	PTP_OC_CANON_EOS_SetRequestOLCInfoGroup,   PTP_VENDOR_CANON,   0,  _get_Canon_EOS_TestOLC,  _put_Canon_EOS_TestOLC },
	{ 0,0,0,0,0,0,0 },
};

/* think of this as properties of the "film" */
static struct submenu image_settings_menu[] = {
	{ N_("Image Quality"),          "imagequality",         PTP_DPC_CANON_ImageQuality,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_Quality,             _put_Canon_Quality },
	{ N_("Image Format"),           "imageformat",          PTP_DPC_OLYMPUS_ImageFormat,            PTP_VENDOR_GP_OLYMPUS_OMD,   PTP_DTC_UINT16,  _get_Olympus_Imageformat, _put_Olympus_Imageformat },
	{ N_("Image Format"),           "imageformat",          PTP_DPC_CANON_FullViewFileFormat,       PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_Capture_Format,      _put_Canon_Capture_Format },
	{ N_("Image Format"),           "imageformat",          PTP_DPC_CANON_EOS_ImageFormat,          PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Image Format SD"),        "imageformatsd",        PTP_DPC_CANON_EOS_ImageFormatSD,        PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Image Format CF"),        "imageformatcf",        PTP_DPC_CANON_EOS_ImageFormatCF,        PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Image Format"),           "imageformat",          PTP_DPC_FUJI_Quality,                   PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_ImageFormat,          _put_Fuji_ImageFormat },
	{ N_("Image Format"),           "imageformat",          0,					PTP_VENDOR_PANASONIC,PTP_DTC_UINT16, _get_Panasonic_ImageFormat,    _put_Panasonic_ImageFormat },
	{ N_("Image Format Ext HD"),    "imageformatexthd",     PTP_DPC_CANON_EOS_ImageFormatExtHD,     PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ImageFormat,     _put_Canon_EOS_ImageFormat },
	{ N_("Film Simulation"),        "filmsimulation",       PTP_DPC_FUJI_FilmSimulation,            PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_FilmSimulation,       _put_Fuji_FilmSimulation },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_ImageSize,                      0,                  PTP_DTC_STR,    _get_STR_ENUMList,              _put_STR },
	{ N_("Raw Image Size"),         "rawimagesize",         PTP_DPC_NIKON_RawImageSize,             PTP_VENDOR_NIKON,   PTP_DTC_STR,    _get_STR_ENUMList,              _put_STR },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_NIKON_1_ImageSize,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon1_ImageSize,          _put_Nikon1_ImageSize },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_SONY_ImageSize,                 PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_ImageSize,            _put_Sony_ImageSize },
	{ N_("Image Size"),             "imagesize",            PTP_DPC_CANON_ImageSize,                PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_Size,                _put_Canon_Size },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_CANON_ISOSpeed,                 PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ISO,                 _put_Canon_ISO },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_ExposureIndex,                  PTP_VENDOR_FUJI,    PTP_DTC_INT32,  _get_INT,                       _put_INT },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_ExposureIndex,                  0,                  PTP_DTC_UINT16, _get_INT,                       _put_INT },
	{ N_("Movie ISO Speed"),        "movieiso",             PTP_DPC_NIKON_MovieISO,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_CANON_EOS_ISOSpeed,             PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ISO,                 _put_Canon_ISO },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_SONY_QX_ISO,                    PTP_VENDOR_SONY,    PTP_DTC_UINT32, _get_Sony_ISO,                  _put_Sony_QX_ISO },
	/* these 2 iso will overwrite and conflict with each other... the older Sony do not have d226, so it should pick the next entry ... */
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_SONY_ISO2,                      PTP_VENDOR_SONY,    PTP_DTC_UINT32, _get_Sony_ISO,                  _put_Sony_ISO2 },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_SONY_ISO,                       PTP_VENDOR_SONY,    PTP_DTC_UINT32, _get_Sony_ISO,                  _put_Sony_ISO },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_NIKON_1_ISO,                    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_ISO,               _put_Nikon_1_ISO },
	{ N_("ISO Speed"),              "iso",                  PTP_DPC_OLYMPUS_ISO,                    PTP_VENDOR_GP_OLYMPUS_OMD, PTP_DTC_UINT16,  _get_Olympus_ISO,       _put_Olympus_ISO },
	{ N_("ISO Speed"),              "iso",             	0,         		    		PTP_VENDOR_PANASONIC,   PTP_DTC_UINT32, _get_Panasonic_ISO,         _put_Panasonic_ISO },
	{ N_("ISO Auto"),               "isoauto",              PTP_DPC_NIKON_ISO_Auto,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("Auto ISO"),               "autoiso",              PTP_DPC_NIKON_ISOAuto,                  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,         _put_Nikon_OnOff_UINT8 },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_OLYMPUS_WhiteBalance,           PTP_VENDOR_GP_OLYMPUS_OMD, PTP_DTC_UINT16,  _get_Olympus_WhiteBalance, _put_Olympus_WhiteBalance },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_CANON_WhiteBalance,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_WhiteBalance,        _put_Canon_WhiteBalance },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_CANON_EOS_WhiteBalance,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_EOS_WhiteBalance,    _put_Canon_EOS_WhiteBalance },
	{ N_("Color Temperature"),      "colortemperature",     PTP_DPC_CANON_EOS_ColorTemperature,     PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Color Temperature"),      "colortemperature",     PTP_DPC_FUJI_ColorTemperature,          PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_INT,                       _put_INT },
	{ N_("Color Temperature"),      "colortemperature",     PTP_DPC_SONY_ColorTemp,                 PTP_VENDOR_SONY,    PTP_DTC_UINT16, _get_INT,                       _put_INT },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_WhiteBalance,                   0,                  PTP_DTC_UINT16, _get_WhiteBalance,              _put_WhiteBalance },
	{ N_("WhiteBalance"),           "whitebalance",         PTP_DPC_NIKON_1_WhiteBalance,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_WhiteBalance,      _put_Nikon_1_WhiteBalance },
	{ N_("WhiteBalance Adjust A"),  "whitebalanceadjusta",  PTP_DPC_CANON_EOS_WhiteBalanceAdjustA,  PTP_VENDOR_CANON,   PTP_DTC_INT32,  _get_Canon_EOS_WBAdjust,        _put_Canon_EOS_WBAdjust },
	{ N_("WhiteBalance Adjust B"),  "whitebalanceadjustb",  PTP_DPC_CANON_EOS_WhiteBalanceAdjustB,  PTP_VENDOR_CANON,   PTP_DTC_INT32,  _get_Canon_EOS_WBAdjust,        _put_Canon_EOS_WBAdjust },
	{ N_("WhiteBalance X A"),       "whitebalancexa",       PTP_DPC_CANON_EOS_WhiteBalanceXA,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("WhiteBalance X B"),       "whitebalancexb",       PTP_DPC_CANON_EOS_WhiteBalanceXB,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                       _put_None },
	{ N_("Photo Effect"),           "photoeffect",          PTP_DPC_CANON_PhotoEffect,              PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_PhotoEffect,         _put_Canon_PhotoEffect },
	{ N_("Color Model"),            "colormodel",           PTP_DPC_NIKON_ColorModel,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ColorModel,          _put_Nikon_ColorModel },
	{ N_("Color Space"),            "colorspace",           PTP_DPC_NIKON_ColorSpace,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ColorSpace,          _put_Nikon_ColorSpace },
	{ N_("Color Space"),            "colorspace",           PTP_DPC_CANON_EOS_ColorSpace,           PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_ColorSpace,      _put_Canon_EOS_ColorSpace },
	{ N_("Color Space"),            "colorspace",           PTP_DPC_FUJI_ColorSpace,                PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Canon_EOS_ColorSpace,      _put_Canon_EOS_ColorSpace }, /* uses 1 for sRGB, and 2 for AdobeRGB too, same as EOS */
	{ N_("Video Format"),           "videoformat",          PTP_DPC_VideoFormat,                    0,                  PTP_DTC_UINT32, _get_VideoFormat,               _put_VideoFormat },
	{ N_("Video Resolution"),       "videoresolution",      PTP_DPC_VideoResolution,                0,                  PTP_DTC_STR   , _get_STR_ENUMList,              _put_STR },
	{ N_("Video Quality"),          "videoquality",         PTP_DPC_VideoQuality,                   0,                  PTP_DTC_UINT16, _get_INT,                       _put_INT },
	{ N_("Video Framerate"),        "videoframerate",       PTP_DPC_VideoFrameRate,                 0,                  PTP_DTC_UINT32, _get_Video_Framerate,           _put_Video_Framerate },
	{ N_("Video Contrast"),         "videocontrast",        PTP_DPC_VideoContrast,                  0,                  PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Video Brightness"),       "videobrightness",      PTP_DPC_VideoBrightness,                0,                  PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Audio Format"),           "audioformat",          PTP_DPC_AudioFormat,                    0,                  PTP_DTC_UINT32, _get_Audio_Format,              _put_Audio_Format },
	{ N_("Audio Bitrate"),          "audiobitrate",         PTP_DPC_AudioBitrate,                   0,                  PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Audio Sampling Rate"),    "audiosamplingrate",    PTP_DPC_AudioSamplingRate,              0,                  PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ N_("Audio Bit per Sample"),   "audiobitpersample",    PTP_DPC_AudioBitPerSample,              0,                  PTP_DTC_UINT16, _get_INT,                       _put_INT },
	{ N_("Audio Volume"),           "audiovolume",          PTP_DPC_AudioVolume,                    0,                  PTP_DTC_UINT32, _get_INT,                       _put_INT },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu capture_settings_menu[] = {
	{ N_("Long Exp Noise Reduction"),       "longexpnr",                PTP_DPC_NIKON_LongExposureNoiseReduction, PTP_VENDOR_NIKON, PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Long Exp Noise Reduction"),       "longexpnr",                PTP_DPC_NIKON_1_LongExposureNoiseReduction, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8,            _put_Nikon_OnOff_UINT8 },
	{ N_("Auto Focus Mode 2"),              "autofocusmode2",           PTP_DPC_NIKON_A4AFActivation,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Zoom"),                           "zoom",                     PTP_DPC_CANON_Zoom,                     PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ZoomRange,               _put_Canon_ZoomRange },
	{ N_("Zoom"),                           "zoom",                     PTP_DPC_CANON_EOS_PowerZoomPosition,    PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_Canon_EOS_ZoomRange,           _put_Canon_EOS_ZoomRange },
	{ N_("Zoom"),                           "zoom",                     PTP_DPC_SONY_Zoom,    	            PTP_VENDOR_SONY,    PTP_DTC_UINT32, _get_Sony_Zoom,                     _put_Sony_Zoom },
	{ N_("Zoom Speed"),                     "zoomspeed",                PTP_DPC_CANON_EOS_PowerZoomSpeed,       PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_INT,                           _put_INT },
	{ N_("Assist Light"),                   "assistlight",              PTP_DPC_CANON_AssistLight,              PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_AssistLight,             _put_Canon_AssistLight },
	{ N_("Rotation Flag"),                  "autorotation",             PTP_DPC_CANON_RotationScene,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_AutoRotation,            _put_Canon_AutoRotation },
	{ N_("Self Timer"),                     "selftimer",                PTP_DPC_CANON_SelfTime,                 PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_SelfTimer,               _put_Canon_SelfTimer },
	{ N_("Assist Light"),                   "assistlight",              PTP_DPC_NIKON_AFAssist,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_OLYMPUS_ExposureCompensation,   PTP_VENDOR_GP_OLYMPUS_OMD, PTP_DTC_UINT16,  _get_Olympus_ExpCompensation,_put_Olympus_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_SONY_ExposureCompensation,      PTP_VENDOR_SONY,    PTP_DTC_INT16,  _get_ExpCompensation,               _put_Sony_ExpCompensation2 },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_SONY_QX_ExposureCompensation,   PTP_VENDOR_SONY,    PTP_DTC_INT16,  _get_ExpCompensation,               _put_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_ExposureBiasCompensation,       PTP_VENDOR_SONY,    PTP_DTC_INT16,  _get_ExpCompensation,               _put_Sony_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_ExposureBiasCompensation,       0,                  PTP_DTC_INT16,  _get_ExpCompensation,               _put_ExpCompensation },
	{ N_("Movie Exposure Compensation"),    "movieexposurecompensation",PTP_DPC_NIKON_MovieExposureBiasCompensation,PTP_VENDOR_NIKON,PTP_DTC_INT16, _get_ExpCompensation,               _put_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_CANON_ExpCompensation,          PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ExpCompensation,         _put_Canon_ExpCompensation },
	{ N_("Exposure Compensation"),          "exposurecompensation",     PTP_DPC_CANON_EOS_ExpCompensation,      PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ExpCompensation2,        _put_Canon_ExpCompensation2 },
	{ N_("Exposure Compensation"),		"exposurecompensation",	    0, 					    PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_Exposure,         _put_Panasonic_Exposure },
	{ N_("White Balance"),			"whitebalance",	    0, 					    	PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_Whitebalance,         _put_Panasonic_Whitebalance },
	{ N_("Color temperature"),              "colortemperature",         0,                                      PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_ColorTemp,        _put_Panasonic_ColorTemp },
	{ N_("Adjust A/B"),                     "whitebalanceadjustab",      0,                                      PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_AdjustAB,         _put_Panasonic_AdjustAB }, 
	{ N_("Adjust G/M"),                     "whitebalanceadjustgm",      0,                                      PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_AdjustGM,         _put_Panasonic_AdjustGM }, 
	{ N_("AF Mode"),                        "afmode",                   0,                                      PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_AFMode,           _put_Panasonic_AFMode }, 
	{ N_("MF Adjust"),                      "mfadjust",                 0,                                      PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_MFAdjust,         _put_Panasonic_MFAdjust }, 
	{ N_("Exp mode"),                       "expmode",                 0,                                      PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_ExpMode,          _put_Panasonic_ExpMode }, 
	{ N_("Start/Stop recording"),           "recording",                 0,                                      PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_Recording,       _put_Panasonic_Recording }, 
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
	{ N_("F-Number"),                       "f-number",                 PTP_DPC_FNumber,                        PTP_VENDOR_SONY,    PTP_DTC_UINT16, _get_Sony_FNumber,                  _put_Sony_FNumber },
	{ N_("F-Number"),                       "f-number",                 PTP_DPC_SONY_QX_Aperture,               PTP_VENDOR_SONY,    PTP_DTC_UINT16, _get_FNumber,                       _put_FNumber },
	{ N_("F-Number"),                       "f-number",                 PTP_DPC_FNumber,                        0,                  PTP_DTC_UINT16, _get_FNumber,                       _put_FNumber },
	{ N_("F-Number"),			"f-number",		    0,					    PTP_VENDOR_PANASONIC,PTP_DTC_INT32, _get_Panasonic_FNumber,             _put_Panasonic_FNumber },
	{ N_("Live View Size"),			"liveviewsize",		    0,					    PTP_VENDOR_PANASONIC,PTP_DTC_INT32, _get_Panasonic_LiveViewSize,        _put_Panasonic_LiveViewSize },
	{ N_("Movie F-Number"),                 "movief-number",            PTP_DPC_NIKON_MovieFNumber,             PTP_VENDOR_NIKON,   PTP_DTC_UINT16, _get_FNumber,                       _put_FNumber },
	{ N_("Flexible Program"),               "flexibleprogram",          PTP_DPC_NIKON_FlexibleProgram,          PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Range_INT8,                    _put_Range_INT8 },
	{ N_("Image Quality"),			"imagequality",		    PTP_DPC_SONY_CompressionSetting,	    PTP_VENDOR_SONY,	PTP_DTC_UINT8,	_get_Sony_300_CompressionSetting,   _put_Sony_300_CompressionSetting },
	{ N_("Image Quality"),                  "imagequality",             PTP_DPC_CompressionSetting,             PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_CompressionSetting,            _put_Sony_CompressionSetting },
	{ N_("Image Quality"),                  "imagequality",             PTP_DPC_CompressionSetting,             0,                  PTP_DTC_UINT8,  _get_CompressionSetting,            _put_CompressionSetting },
	{ N_("JPEG Quality"),			"jpegquality",              PTP_DPC_SONY_JpegQuality,               PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_300_JpegCompressionSetting,   _put_Sony_300_JpegCompressionSetting },
	{ N_("PC Save Image Size"),		"pcsaveimgsize",            PTP_DPC_SONY_PcSaveImageSize,           PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_PcSaveImageSize,          _put_Sony_PcSaveImageSize },
	{ N_("PC Save Image Format"),		"pcsaveimgformat",	    PTP_DPC_SONY_PcSaveImageFormat,         PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_PcSaveImageFormat,        _put_Sony_PcSaveImageFormat },
	{ N_("Focus Distance"),                 "focusdistance",            PTP_DPC_FocusDistance,                  0,                  PTP_DTC_UINT16, _get_FocusDistance,                 _put_FocusDistance },
	{ N_("Focal Length"),                   "focallength",              PTP_DPC_FocalLength,                    0,                  PTP_DTC_UINT32, _get_FocalLength,                   _put_FocalLength },
	{ N_("Focus Mode"),                     "focusmode",                PTP_DPC_FocusMode,                      PTP_VENDOR_SONY,    PTP_DTC_UINT16, _get_FocusMode,                     _put_Sony_FocusMode },
	{ N_("Focus Mode"),                     "focusmode",                PTP_DPC_FocusMode,                      0,                  PTP_DTC_UINT16, _get_FocusMode,                     _put_FocusMode },
	{ N_("Focus Mode"),                     "focusmode",                PTP_DPC_OLYMPUS_FocusMode,              PTP_VENDOR_GP_OLYMPUS_OMD,  PTP_DTC_UINT16, _get_FocusMode,             _put_FocusMode },
	/* Nikon DSLR have both PTP focus mode and Nikon specific focus mode */
	{ N_("Focus Mode 2"),                   "focusmode2",               PTP_DPC_NIKON_AutofocusMode,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AFMode,                  _put_Nikon_AFMode },
	{ N_("Focus Mode"),                     "focusmode",                PTP_DPC_CANON_EOS_FocusMode,            PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_Canon_EOS_FocusMode,           _put_Canon_EOS_FocusMode },
	{ N_("Continuous AF"),                  "continuousaf",             PTP_DPC_CANON_EOS_ContinousAFMode,      PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_Canon_EOS_ContinousAF,         _put_Canon_EOS_ContinousAF },
	{ N_("Effect Mode"),                    "effectmode",               PTP_DPC_EffectMode,                     0,                  PTP_DTC_UINT16, _get_EffectMode,                    _put_EffectMode },
	{ N_("Effect Mode"),                    "effectmode",               PTP_DPC_NIKON_EffectMode,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_NIKON_EffectMode,              _put_NIKON_EffectMode },
	{ N_("Exposure Program"),               "expprogram",               PTP_DPC_ExposureProgramMode,            0,                  PTP_DTC_UINT16, _get_ExposureProgram,               _put_ExposureProgram },
	{ N_("Exposure Program"),               "expprogram2",              PTP_DPC_NIKON_1_Mode,                   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_NIKON_1_ExposureProgram,       _put_NIKON_1_ExposureProgram },
	{ N_("User Mode"),               	"usermode",                 PTP_DPC_NIKON_UserMode,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_NIKON_UserMode,                _put_NIKON_UserMode },
	{ N_("Scene Mode"),                     "scenemode",                PTP_DPC_NIKON_SceneMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_NIKON_SceneMode,               _put_NIKON_SceneMode },
	{ N_("Aspect Ratio"),                   "aspectratio",              PTP_DPC_SONY_AspectRatio,               PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_AspectRatio,              _put_Sony_AspectRatio },
	{ N_("Aspect Ratio"),                   "aspectratio",              PTP_DPC_OLYMPUS_AspectRatio,            PTP_VENDOR_GP_OLYMPUS_OMD, PTP_DTC_UINT32,  _get_Olympus_AspectRatio,   _put_Olympus_AspectRatio },
	{ N_("Aspect Ratio"),                   "aspectratio",              PTP_DPC_CANON_EOS_MultiAspect,          PTP_VENDOR_CANON,   PTP_DTC_UINT32,  _get_Canon_EOS_AspectRatio,        _put_Canon_EOS_AspectRatio },
	{ N_("AF Method"),	        	"afmethod",		    PTP_DPC_CANON_EOS_LvAfSystem,	    PTP_VENDOR_CANON,   PTP_DTC_UINT32,  _get_Canon_EOS_AFMethod,     _put_Canon_EOS_AFMethod },
	{ N_("Storage Device"),                 "storageid",                PTP_DPC_CANON_EOS_CurrentStorage,       PTP_VENDOR_CANON,   PTP_DTC_UINT32,  _get_Canon_EOS_StorageID  ,        _put_Canon_EOS_StorageID },
	{ N_("High ISO Noise Reduction"),       "highisonr",		    PTP_DPC_CANON_EOS_HighISOSettingNoiseReduction, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_EOS_HighIsoNr,     _put_Canon_EOS_HighIsoNr },
	{ N_("HDR Mode"),                       "hdrmode",                  PTP_DPC_NIKON_HDRMode,                  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("HDR High Dynamic"),               "hdrhighdynamic",           PTP_DPC_NIKON_HDRHighDynamic,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_HDRHighDynamic,          _put_Nikon_HDRHighDynamic },
	{ N_("HDR Smoothing"),                  "hdrsmoothing",             PTP_DPC_NIKON_HDRSmoothing,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_HDRSmoothing,            _put_Nikon_HDRSmoothing },
	{ N_("Still Capture Mode"),             "capturemode",              PTP_DPC_StillCaptureMode,               0,                  PTP_DTC_UINT16, _get_CaptureMode,                   _put_CaptureMode },
	{ N_("Still Capture Mode"),             "capturemode",              PTP_DPC_FUJI_ReleaseMode,               PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_ReleaseMode,              _put_Fuji_ReleaseMode },
	{ N_("Canon Shooting Mode"),            "shootingmode",             PTP_DPC_CANON_ShootingMode,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_ShootMode,               _put_Canon_ShootMode },
	{ N_("Canon Auto Exposure Mode"),       "autoexposuremode",         PTP_DPC_CANON_EOS_AutoExposureMode,     PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_AutoExposureMode,    _put_Canon_EOS_AutoExposureMode },
	{ N_("Canon Auto Exposure Mode Dial"),  "autoexposuremodedial",     PTP_DPC_CANON_EOS_AEModeDial,           PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_AutoExposureMode,    _put_Canon_EOS_AutoExposureMode },
	{ N_("Drive Mode"),                     "drivemode",                PTP_DPC_CANON_EOS_DriveMode,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_DriveMode,           _put_Canon_EOS_DriveMode },
	{ N_("Picture Style"),                  "picturestyle",             PTP_DPC_CANON_EOS_PictureStyle,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_EOS_PictureStyle,        _put_Canon_EOS_PictureStyle },
	{ N_("Focus Metering Mode"),            "focusmetermode",           PTP_DPC_FocusMeteringMode,              0,                  PTP_DTC_UINT16, _get_FocusMetering,                 _put_FocusMetering },
	{ N_("Focus Metering Mode"),            "exposuremetermode",        PTP_DPC_OLYMPUS_ExposureMeteringMode,   PTP_VENDOR_GP_OLYMPUS_OMD, PTP_DTC_UINT16, _get_ExposureMetering,       _put_ExposureMetering },
	{ N_("Exposure Metering Mode"),         "exposuremetermode",        PTP_DPC_ExposureMeteringMode,           0,                  PTP_DTC_UINT16, _get_ExposureMetering,              _put_ExposureMetering },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_OLYMPUS_Aperture,               PTP_VENDOR_GP_OLYMPUS_OMD,   PTP_DTC_UINT16, _get_Olympus_Aperture,     _put_Olympus_Aperture },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_CANON_Aperture,                 PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_FUJI_Aperture,                  PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_Aperture,                 _put_Fuji_Aperture },
	{ N_("AV Open"),                        "avopen",                   PTP_DPC_CANON_AvOpen,                   PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("AV Max"),                         "avmax",                    PTP_DPC_CANON_AvMax,                    PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_CANON_EOS_Aperture,             PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_Aperture,                _put_Canon_Aperture },
	{ N_("Aperture"),                       "aperture",                 PTP_DPC_NIKON_1_FNumber,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_Aperture,              _put_Nikon_1_Aperture },
	{ N_("Shutterspeed"),                   "shutterspeed2",            PTP_DPC_NIKON_1_ShutterSpeed,           PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_1_ShutterSpeedI,         _put_Nikon_1_ShutterSpeedI },
	{ N_("Shutterspeed"),                   "shutterspeed2",            PTP_DPC_NIKON_1_ShutterSpeed,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_ShutterSpeedU,         _put_Nikon_1_ShutterSpeedU },
	{ N_("Aperture 2"),                     "aperture2",                PTP_DPC_NIKON_1_FNumber2,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_Aperture,              _put_Nikon_1_Aperture },
	{ N_("Focusing Point"),                 "focusingpoint",            PTP_DPC_CANON_FocusingPoint,            PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_FocusingPoint,           _put_Canon_FocusingPoint },
	{ N_("Sharpness"),                      "sharpness",                PTP_DPC_Sharpness,                      0,                  PTP_DTC_UINT8,  _get_Sharpness,                     _put_Sharpness },
	{ N_("Capture Delay"),                  "capturedelay",             PTP_DPC_CaptureDelay,                   0,                  PTP_DTC_UINT32, _get_Milliseconds,                  _put_Milliseconds },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_ExposureTime,		    PTP_VENDOR_FUJI,    PTP_DTC_UINT32,	_get_Fuji_New_ShutterSpeed,	    _put_Fuji_New_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_ExposureTime,                   0,                  PTP_DTC_UINT32, _get_ExpTime,                       _put_ExpTime },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_CANON_ShutterSpeed,             PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ShutterSpeed,            _put_Canon_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             0,						PTP_VENDOR_PANASONIC,   PTP_DTC_INT32, _get_Panasonic_Shutter,         _put_Panasonic_Shutter },
	/* these cameras also have PTP_DPC_ExposureTime, avoid overlap */
	{ N_("Shutter Speed 2"),                "shutterspeed2",            PTP_DPC_NIKON_ExposureTime,             PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_ShutterSpeed,            _put_Nikon_ShutterSpeed },
	{ N_("Movie Shutter Speed 2"),          "movieshutterspeed",        PTP_DPC_NIKON_MovieShutterSpeed,        PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_ShutterSpeed,            _put_Nikon_ShutterSpeed },
	/* olympus uses also a 16 bit/16bit separation */
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_OLYMPUS_Shutterspeed,           PTP_VENDOR_GP_OLYMPUS_OMD,   PTP_DTC_UINT32, _get_Olympus_ShutterSpeed, _put_Olympus_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_CANON_EOS_ShutterSpeed,         PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_ShutterSpeed,            _put_Canon_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_FUJI_ShutterSpeed,              PTP_VENDOR_FUJI,    PTP_DTC_INT16,  _get_Fuji_ShutterSpeed,             _put_Fuji_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_SONY_ShutterSpeed2,             PTP_VENDOR_SONY,    PTP_DTC_UINT32,  _get_Sony_ShutterSpeed,             _put_Sony_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_SONY_ShutterSpeed,              PTP_VENDOR_SONY,    PTP_DTC_UINT32,  _get_Sony_ShutterSpeed,             _put_Sony_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_RICOH_ShutterSpeed,             PTP_VENDOR_PENTAX,  PTP_DTC_UINT64, _get_Ricoh_ShutterSpeed,            _put_Ricoh_ShutterSpeed },
	{ N_("Shutter Speed"),                  "shutterspeed",             PTP_DPC_GP_SIGMA_FP_ShutterSpeed,       PTP_VENDOR_GP_SIGMAFP, PTP_DTC_UINT64, _get_SigmaFP_ShutterSpeed,       _put_SigmaFP_ShutterSpeed },
	{ N_("Aperture"),                  	"aperture",		    PTP_DPC_GP_SIGMA_FP_Aperture,           PTP_VENDOR_GP_SIGMAFP, PTP_DTC_UINT64, _get_SigmaFP_Aperture,           _put_SigmaFP_Aperture },
	{ N_("Metering Mode"),                  "meteringmode",             PTP_DPC_CANON_MeteringMode,             PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_MeteringMode,            _put_Canon_MeteringMode },
	{ N_("Metering Mode"),                  "meteringmode",             PTP_DPC_CANON_EOS_MeteringMode,         PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_MeteringMode,            _put_Canon_MeteringMode },
	{ N_("AF Distance"),                    "afdistance",               PTP_DPC_CANON_AFDistance,               PTP_VENDOR_CANON,   PTP_DTC_UINT8,  _get_Canon_AFDistance,              _put_Canon_AFDistance },
	{ N_("Focus Area Wrap"),                "focusareawrap",            PTP_DPC_NIKON_FocusAreaWrap,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Exposure Delay Mode"),            "exposuredelaymode",        PTP_DPC_NIKON_ExposureDelayMode,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Exposure Lock"),                  "exposurelock",             PTP_DPC_NIKON_AELockMode,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("AE-L/AF-L Mode"),                 "aelaflmode",               PTP_DPC_NIKON_AELAFLMode,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AELAFLMode,              _put_Nikon_AELAFLMode },
	{ N_("Live View AF Mode"),              "liveviewafmode",           PTP_DPC_NIKON_LiveViewAFArea,           PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_LiveViewAFI,             _put_Nikon_LiveViewAFI },
	{ N_("Live View AF Mode"),              "liveviewafmode",           PTP_DPC_NIKON_LiveViewAFArea,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LiveViewAFU,             _put_Nikon_LiveViewAFU },
	{ N_("Live View AF Focus"),             "liveviewaffocus",          PTP_DPC_NIKON_LiveViewAFFocus,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LiveViewAFFocus,         _put_Nikon_LiveViewAFFocus },
	{ N_("Live View Exposure Preview"),     "liveviewexposurepreview",  PTP_DPC_NIKON_LiveViewExposurePreview,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Live View Image Zoom Ratio"),     "liveviewimagezoomratio",   PTP_DPC_NIKON_LiveViewImageZoomRatio,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LiveViewImageZoomRatio,  _put_Nikon_LiveViewImageZoomRatio },
	{ N_("Live View White Balance"),        "liveviewwhitebalance",     PTP_DPC_NIKON_LiveViewWhiteBalance,     PTP_VENDOR_NIKON,   PTP_DTC_UINT16, _get_WhiteBalance,                  _put_WhiteBalance },
	{ N_("Live View Size"),                 "liveviewsize",             PTP_DPC_FUJI_LiveViewImageSize,         PTP_VENDOR_FUJI,    PTP_DTC_UINT16, _get_Fuji_LiveViewSize,             _put_Fuji_LiveViewSize },
	{ N_("Live View Size"),                 "liveviewsize",             PTP_DPC_SONY_QX_LiveviewResolution,     PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_QX_LiveViewSize,          _put_Sony_QX_LiveViewSize },
	{ N_("Live View Size"),                 "liveviewsize",             PTP_DPC_CANON_EOS_EVFOutputDevice,      PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_Canon_LiveViewSize,            _put_Canon_LiveViewSize },
	{ N_("Live View Setting Effect"),       "liveviewsettingeffect",    PTP_DPC_SONY_LiveViewSettingEffect,     PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_LiveViewSettingEffect,    _put_Sony_LiveViewSettingEffect },
	{ N_("File Number Sequencing"),         "filenrsequencing",         PTP_DPC_NIKON_FileNumberSequence,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Flash Sign"),                     "flashsign",                PTP_DPC_NIKON_FlashSign,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Modelling Flash"),                "modelflash",               PTP_DPC_NIKON_E4ModelingFlash,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Viewfinder Grid"),                "viewfindergrid",           PTP_DPC_NIKON_GridDisplay,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Image Review"),                   "imagereview",              PTP_DPC_NIKON_ImageReview,              PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Image Rotation Flag"),            "imagerotationflag",        PTP_DPC_NIKON_ImageRotation,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Release without CF card"),        "nocfcardrelease",          PTP_DPC_NIKON_NoCFCard,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Flash Mode Manual Power"),        "flashmodemanualpower",     PTP_DPC_NIKON_FlashModeManualPower,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FlashModeManualPower,    _put_Nikon_FlashModeManualPower },
	{ N_("Auto Focus Area"),                "autofocusarea",            PTP_DPC_NIKON_AutofocusArea,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AutofocusArea,           _put_Nikon_AutofocusArea },
	{ N_("Focus Area"),                     "focusarea",                PTP_DPC_SONY_FocusArea,                 PTP_VENDOR_SONY,    PTP_DTC_UINT16, _get_Sony_FocusArea,                _put_Sony_FocusArea },
	{ N_("Flash Exposure Compensation"),    "flashexposurecompensation", PTP_DPC_NIKON_FlashExposureCompensation, PTP_VENDOR_NIKON, PTP_DTC_INT8,   _get_Nikon_FlashExposureCompensation, _put_Nikon_FlashExposureCompensation },
	{ N_("Bracketing"),                     "bracketing",               PTP_DPC_NIKON_Bracketing,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Bracketing"),                     "bracketmode",              PTP_DPC_NIKON_E6ManualModeBracketing,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ManualBracketMode,       _put_Nikon_ManualBracketMode },
	{ N_("Bracket Mode"),                   "bracketmode",              PTP_DPC_CANON_EOS_BracketMode,          PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_BracketMode,             _put_Canon_BracketMode },
	{ N_("EV Step"),                        "evstep",                   PTP_DPC_NIKON_EVStep,                   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_EVStep,                  _put_Nikon_EVStep },
	{ N_("Bracket Set"),                    "bracketset",               PTP_DPC_NIKON_BracketSet,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_BracketSet,              _put_Nikon_BracketSet },
	{ N_("Bracket Order"),                  "bracketorder",             PTP_DPC_NIKON_BracketOrder,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_BracketOrder,            _put_Nikon_BracketOrder },
	{ N_("AE Bracketing Step"),             "aebracketingstep",         PTP_DPC_NIKON_AutoExposureBracketStep,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_AEBracketStep,           _put_Nikon_AEBracketStep },
	{ N_("WB Bracketing Step"),             "wbbracketingstep",         PTP_DPC_NIKON_WhiteBalanceBracketStep,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_WBBracketStep,           _put_Nikon_WBBracketStep },
	{ N_("AE Bracketing Pattern"),          "aebracketingpattern",      PTP_DPC_NIKON_AutoExposureBracketProgram,PTP_VENDOR_NIKON,	PTP_DTC_UINT8,  _get_Nikon_BracketPattern,          _put_Nikon_BracketPattern },
	{ N_("WB Bracketing Pattern"),          "wbbracketingpattern",      PTP_DPC_NIKON_WhiteBalanceBracketProgram,PTP_VENDOR_NIKON,	PTP_DTC_UINT8,  _get_Nikon_BracketPattern,          _put_Nikon_BracketPattern },
	{ N_("AE Bracketing Count"),            "aebracketingcount",        PTP_DPC_NIKON_AutoExposureBracketCount, PTP_VENDOR_NIKON,	PTP_DTC_UINT8,  _get_INT,          		    _put_None },
	{ N_("ADL Bracketing Pattern"),         "adlbracketingpattern",     PTP_DPC_NIKON_ADLBracketingPattern,	    PTP_VENDOR_NIKON,	PTP_DTC_UINT8,  _get_Nikon_ADLBracketPattern,       _put_Nikon_ADLBracketPattern },
	{ N_("ADL Bracketing Step"),            "adlbracketingstep",        PTP_DPC_NIKON_ADLBracketingStep,	    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ADLBracketStep,          _put_Nikon_ADLBracketStep },
	{ N_("WB Preset Comment 1"),		"wbpresetcomment1",	    PTP_DPC_NIKON_WhiteBalancePresetName1,  PTP_VENDOR_NIKON,   PTP_DTC_STR,	_get_STR,          		    _put_STR },
	{ N_("WB Preset Comment 2"),		"wbpresetcomment2",	    PTP_DPC_NIKON_WhiteBalancePresetName2,  PTP_VENDOR_NIKON,   PTP_DTC_STR,	_get_STR,          		    _put_STR },
	{ N_("WB Preset Comment 3"),		"wbpresetcomment3",	    PTP_DPC_NIKON_WhiteBalancePresetName3,  PTP_VENDOR_NIKON,   PTP_DTC_STR,	_get_STR,          		    _put_STR },
	{ N_("WB Preset Comment 4"),		"wbpresetcomment4",	    PTP_DPC_NIKON_WhiteBalancePresetName4,  PTP_VENDOR_NIKON,   PTP_DTC_STR,	_get_STR,          		    _put_STR },
	{ N_("WB Preset Comment 5"),		"wbpresetcomment5",	    PTP_DPC_NIKON_WhiteBalancePresetName5,  PTP_VENDOR_NIKON,   PTP_DTC_STR,	_get_STR,          		    _put_STR },
	{ N_("WB Preset Comment 6"),		"wbpresetcomment6",	    PTP_DPC_NIKON_WhiteBalancePresetName6,  PTP_VENDOR_NIKON,   PTP_DTC_STR,	_get_STR,          		    _put_STR },
	{ N_("Burst Number"),                   "burstnumber",              PTP_DPC_BurstNumber,                    0,                  PTP_DTC_UINT16, _get_INT,                           _put_INT },
	{ N_("Burst Interval"),                 "burstinterval",            PTP_DPC_BurstInterval,                  0,                  PTP_DTC_UINT16, _get_Milliseconds,                  _put_Milliseconds },
	{ N_("Maximum Shots"),                  "maximumshots",             PTP_DPC_NIKON_MaximumShots,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_INT,                           _put_None },

	/* Newer Nikons have UINT8 ranges */
	{ N_("Auto White Balance Bias"),        "autowhitebias",            PTP_DPC_NIKON_WhiteBalanceAutoBias,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Tungsten White Balance Bias"),    "tungstenwhitebias",        PTP_DPC_NIKON_WhiteBalanceTungstenBias, PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Fluorescent White Balance Bias"), "flourescentwhitebias",     PTP_DPC_NIKON_WhiteBalanceFluorescentBias, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Daylight White Balance Bias"),    "daylightwhitebias",        PTP_DPC_NIKON_WhiteBalanceDaylightBias, PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Flash White Balance Bias"),       "flashwhitebias",           PTP_DPC_NIKON_WhiteBalanceFlashBias,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Cloudy White Balance Bias"),      "cloudywhitebias",          PTP_DPC_NIKON_WhiteBalanceCloudyBias,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Shady White Balance Bias"),       "shadewhitebias",           PTP_DPC_NIKON_WhiteBalanceShadeBias,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_UWBBias,                 _put_Nikon_UWBBias },
	{ N_("Natural light auto White Balance Bias"),	"naturallightautowhitebias",	PTP_DPC_NIKON_WhiteBalanceNaturalLightAutoBias,    PTP_VENDOR_NIKON,   PTP_DTC_UINT16,  _get_Nikon_UWBBias,	_put_Nikon_UWBBias },
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
	{ N_("Application Mode"),               "applicationmode",          PTP_OC_NIKON_ChangeApplicationMode,     PTP_VENDOR_NIKON,   0,              _get_Nikon_ApplicationMode2,        _put_Nikon_ApplicationMode2 },
	{ N_("Optimize Image"),                 "optimizeimage",            PTP_DPC_NIKON_OptimizeImage,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OptimizeImage,           _put_Nikon_OptimizeImage },
	{ N_("Sharpening"),                     "sharpening",               PTP_DPC_NIKON_ImageSharpening,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_Sharpening,              _put_Nikon_Sharpening },
	{ N_("Tone Compensation"),              "tonecompensation",         PTP_DPC_NIKON_ToneCompensation,         PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_ToneCompensation,        _put_Nikon_ToneCompensation },
	{ N_("Saturation"),                     "saturation",               PTP_DPC_NIKON_Saturation,               PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_Saturation,              _put_Nikon_Saturation },
	{ N_("Hue Adjustment"),                 "hueadjustment",            PTP_DPC_NIKON_HueAdjustment,            PTP_VENDOR_NIKON,   PTP_DTC_INT8,   _get_Nikon_HueAdjustment,           _put_Nikon_HueAdjustment },
	{ N_("Auto Exposure Bracketing"),       "aeb",                      PTP_DPC_CANON_EOS_AEB,                  PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_AEB,                 _put_Canon_EOS_AEB },
	{ N_("Auto Lighting Optimization"),     "alomode",                  PTP_DPC_CANON_EOS_AloMode,              PTP_VENDOR_CANON,   PTP_DTC_UINT16, _get_Canon_EOS_AloMode,             _put_Canon_EOS_AloMode },
	{ N_("Movie Sound"),                    "moviesound",               PTP_DPC_NIKON_MovVoice,                 PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Manual Movie Setting"),           "manualmoviesetting",       PTP_DPC_NIKON_ManualMovieSetting,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Microphone"),                     "microphone",               PTP_DPC_NIKON_MovMicrophone,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_Microphone,              _put_Nikon_Microphone },
	{ N_("Reverse Indicators"),             "reverseindicators",        PTP_DPC_NIKON_IndicatorDisp,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,             _put_Nikon_OffOn_UINT8 },
	{ N_("Auto Distortion Control"),        "autodistortioncontrol",    PTP_DPC_NIKON_AutoDistortionControl,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },
	{ N_("Vignette Correction"),            "vignettecorrection",       PTP_DPC_NIKON_VignetteCtrl,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_VignetteCorrection,      _put_Nikon_VignetteCorrection },
	{ N_("Video Mode"),                     "videomode",                PTP_DPC_NIKON_VideoMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_VideoMode,               _put_Nikon_VideoMode },
	{ N_("Sensor Crop"),                    "sensorcrop",               PTP_DPC_SONY_SensorCrop,                PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_SensorCrop,               _put_Sony_SensorCrop },
	{ N_("HDMI Output Data Depth"),         "hdmioutputdatadepth",      PTP_DPC_NIKON_HDMIOutputDataDepth,      PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_HDMIDataDepth,           _put_Nikon_HDMIDataDepth },
	{ N_("Face Detection"),                 "facedetection",            PTP_DPC_NIKON_FaceDetection,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_FaceDetection,           _put_Nikon_FaceDetection },
	{ N_("Movie Servo AF"),                 "movieservoaf",             PTP_DPC_CANON_EOS_MovieServoAF,         PTP_VENDOR_CANON,   PTP_DTC_UINT32, _get_Canon_EOS_MovieServoAF,        _put_Canon_EOS_MovieServoAF },

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

static struct submenu nikon_d850_capture_settings[] = {
	{ N_("Image Quality"),          	"imagequality",			PTP_DPC_CompressionSetting,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D850_Compression,       _put_Nikon_D850_Compression },
	{ N_("Image Rotation Flag"),            "imagerotationflag",    PTP_DPC_NIKON_ImageRotation,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OffOn_UINT8,            _put_Nikon_OffOn_UINT8 },
	{ N_("Active D-Lighting"),              "dlighting",            PTP_DPC_NIKON_ActiveDLighting,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,   _get_Nikon_D850_ActiveDLighting,   _put_Nikon_D850_ActiveDLighting },
	{ N_("Continuous Shooting Speed Slow"), "shootingspeed",        PTP_DPC_NIKON_D1ShootingSpeed,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D850_ShootingSpeed,     _put_Nikon_D850_ShootingSpeed },
	{ N_("Movie Resolution"),               "moviequality",         PTP_DPC_NIKON_MovScreenSize,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D850_MovieQuality,      _put_Nikon_D850_MovieQuality },
	{ N_("Center Weight Area"),             "centerweightsize",     PTP_DPC_NIKON_CenterWeightArea, PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D850_CenterWeight,      _put_Nikon_D850_CenterWeight },
	{ N_("Focus Metering Mode"),            "focusmetermode",       PTP_DPC_FocusMeteringMode,	PTP_VENDOR_NIKON,   PTP_DTC_UINT16, _get_Nikon_D850_FocusMetering,     _put_Nikon_D850_FocusMetering },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu nikon_d7500_capture_settings[] = {
	{ N_("Image Quality"), "imagequality", PTP_DPC_CompressionSetting, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_D7500_Compression, _put_Nikon_D7500_Compression },
	{ 0,0,0,0,0,0,0 },
};

/* D780 has the same list as the D7500 */
static struct submenu nikon_d780_capture_settings[] = {
	{ N_("Image Quality"), "imagequality", PTP_DPC_CompressionSetting, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_D7500_Compression, _put_Nikon_D7500_Compression },
	{ 0,0,0,0,0,0,0 },
};

/* D500 has the same list as the D850 */
static struct submenu nikon_d500_capture_settings[] = {
	{ N_("Image Quality"), "imagequality", PTP_DPC_CompressionSetting, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_D850_Compression, _put_Nikon_D850_Compression },
	{ 0,0,0,0,0,0,0 },
};

/* D5000...
 * compression is same as D90
 */
static struct submenu nikon_d5000_capture_settings[] = {
	{ N_("Image Quality"),                  "imagequality",         PTP_DPC_CompressionSetting,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_Compression,     _put_Nikon_D90_Compression },
	{ N_("Live View Image Zoom Ratio"),     "liveviewimagezoomratio",   PTP_DPC_NIKON_LiveViewImageZoomRatio,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LiveViewImageZoomRatio_D5000,  _put_Nikon_LiveViewImageZoomRatio_D5000 },
	{ 0,0,0,0,0,0,0 },
};

static struct submenu nikon_z6_capture_settings[] = {
	{ N_("Image Quality"),          	"imagequality",		PTP_DPC_CompressionSetting,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D850_Compression,	_put_Nikon_D850_Compression },
	{ N_("Focus Metering Mode"),            "focusmetermode",       PTP_DPC_FocusMeteringMode,	PTP_VENDOR_NIKON,   PTP_DTC_UINT16, _get_Nikon_D850_FocusMetering,	_put_Nikon_D850_FocusMetering },
	{ N_("Minimum Shutter Speed"),  	"minimumshutterspeed",  PTP_DPC_NIKON_PADVPMode,	PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_Z6_PADVPValue,		_put_Nikon_Z6_PADVPValue },
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
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",       PTP_DPC_NIKON_ISOAutoHiLimit,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_ISOAutoHiLimit,    _put_Nikon_D7100_ISOAutoHiLimit },
	{ N_("Flash Sync. Speed"),              "flashsyncspeed",       PTP_DPC_NIKON_FlashSyncSpeed,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_FlashSyncSpeed,    _put_Nikon_D7100_FlashSyncSpeed },
	{ N_("Focus Metering"),                 "focusmetering",        PTP_DPC_FocusMeteringMode,      PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D7100_FocusMetering,     _put_Nikon_D7100_FocusMetering },
	{ 0,0,0,0,0,0,0 },
};
static struct submenu nikon_d90_capture_settings[] = {
	{ N_("Minimum Shutter Speed"),          "minimumshutterspeed",  PTP_DPC_NIKON_PADVPMode,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_PADVPValue,      _put_Nikon_D90_PADVPValue },
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",       PTP_DPC_NIKON_ISOAutoHiLimit,   PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ISOAutoHiLimit,  _put_Nikon_D90_ISOAutoHiLimit },
	{ N_("Active D-Lighting"),              "dlighting",            PTP_DPC_NIKON_ActiveDLighting,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ActiveDLighting, _put_Nikon_D90_ActiveDLighting },
	{ N_("Image Quality"),                  "imagequality",         PTP_DPC_CompressionSetting,     PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_Compression,     _put_Nikon_D90_Compression },
	{ N_("Continuous Shooting Speed Slow"), "shootingspeed",        PTP_DPC_NIKON_D1ShootingSpeed,  PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ShootingSpeed,   _put_Nikon_D90_ShootingSpeed },
	{ 0,0,0,0,0,0,0 },
};

/* One D3s reporter is Matthias Blaicher */
static struct submenu nikon_d3s_capture_settings[] = {
	{ N_("Minimum Shutter Speed"),          "minimumshutterspeed",      PTP_DPC_NIKON_PADVPMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_PADVPValue,              _put_Nikon_D3s_PADVPValue },
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",           PTP_DPC_NIKON_ISOAutoHiLimit,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_ISOAutoHiLimit,          _put_Nikon_D3s_ISOAutoHiLimit },
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
	{ N_("Active D-Lighting"),              "dlighting",                PTP_DPC_NIKON_ActiveDLighting,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ActiveDLighting,         _put_Nikon_D90_ActiveDLighting },

	{ 0,0,0,0,0,0,0 },
};

static struct submenu nikon_generic_capture_settings[] = {
	/* filled in with D90 values */
	{ N_("Minimum Shutter Speed"),          "minimumshutterspeed",      PTP_DPC_NIKON_PADVPMode,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_PADVPValue,          _put_Nikon_D90_PADVPValue },
	{ N_("ISO Auto Hi Limit"),              "isoautohilimit",           PTP_DPC_NIKON_ISOAutoHiLimit,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ISOAutoHiLimit,      _put_Nikon_D90_ISOAutoHiLimit },
	{ N_("Active D-Lighting"),              "dlighting",                PTP_DPC_NIKON_ActiveDLighting,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ActiveDLighting,     _put_Nikon_D90_ActiveDLighting },
	{ N_("High ISO Noise Reduction"),       "highisonr",                PTP_DPC_NIKON_NrHighISO,                PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_HighISONR,           _put_Nikon_D90_HighISONR },
	{ N_("Movie High ISO Noise Reduction"), "moviehighisonr",           PTP_DPC_NIKON_MovieNrHighISO,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_HighISONR,           _put_Nikon_D90_HighISONR },
	{ N_("Continuous Shooting Speed Slow"), "shootingspeed",            PTP_DPC_NIKON_D1ShootingSpeed,          PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D90_ShootingSpeed,       _put_Nikon_D90_ShootingSpeed },
	{ N_("Maximum continuous release"),     "maximumcontinousrelease",  PTP_DPC_NIKON_D2MaximumShots,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Range_UINT8,                   _put_Range_UINT8 },
	{ N_("Movie Quality"),                  "moviequality",             PTP_DPC_NIKON_MovScreenSize,            PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_MovieQuality,            _put_Nikon_MovieQuality },
	{ N_("Movie Quality"),                  "moviequality",             PTP_DPC_NIKON_1_MovQuality,             PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_MovieQuality,          _put_Nikon_1_MovieQuality },
	{ N_("Movie Loop Length"),              "movielooplength",          PTP_DPC_NIKON_MovieLoopLength,          PTP_VENDOR_NIKON,   PTP_DTC_UINT32, _get_Nikon_MovieLoopLength,         _put_Nikon_MovieLoopLength },
	{ N_("High ISO Noise Reduction"),       "highisonr",                PTP_DPC_NIKON_1_HiISONoiseReduction,    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_HighISONR,             _put_Nikon_1_HighISONR },
	{ N_("Auto ISO"),			"autoiso",		    PTP_DPC_NIKON_ISOAuto,		    PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_OnOff_UINT8,             _put_Nikon_OnOff_UINT8 },

	{ N_("Raw Compression"),                "rawcompression",           PTP_DPC_NIKON_RawCompression,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_RawCompression,          _put_Nikon_RawCompression },

	{ N_("Image Quality 2"),                "imagequality2",            PTP_DPC_NIKON_1_ImageCompression,       PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_1_Compression,           _put_Nikon_1_Compression },
	{ N_("Image Quality"),                  "imagequality",             PTP_DPC_SONY_QX_CompressionSetting,     PTP_VENDOR_SONY,    PTP_DTC_UINT8,  _get_Sony_QX_Compression,           _put_Sony_QX_Compression },

	/* And some D3s values */
	{ N_("Continuous Shooting Speed High"), "shootingspeedhigh",        PTP_DPC_NIKON_ContinuousSpeedHigh,      PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_ShootingSpeedHigh,   _put_Nikon_D3s_ShootingSpeedHigh },
	{ N_("Flash Sync. Speed"),              "flashsyncspeed",           PTP_DPC_NIKON_FlashSyncSpeed,           PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_FlashSyncSpeed,      _put_Nikon_D3s_FlashSyncSpeed },
	{ N_("Flash Shutter Speed"),            "flashshutterspeed",        PTP_DPC_NIKON_FlashShutterSpeed,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_D3s_FlashShutterSpeed,   _put_Nikon_D3s_FlashShutterSpeed },

	{ N_("Live View Size"),                 "liveviewsize",             PTP_DPC_NIKON_LiveViewImageSize,        PTP_VENDOR_NIKON,   PTP_DTC_UINT8,  _get_Nikon_LiveViewSize,            _put_Nikon_LiveViewSize },
	{ N_("Live View Zoom Area"),            "liveviewzoomarea",         PTP_DPC_NIKON_LiveViewZoomArea,         PTP_VENDOR_NIKON,   PTP_DTC_UINT16, _get_INT,                           _put_INT },

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
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0423, nikon_d5000_capture_settings,   NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x043c, nikon_d500_capture_settings,    NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0426, nikon_d3s_capture_settings,     NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0429, nikon_d5100_capture_settings,   NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0430, nikon_d7100_capture_settings,   NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0440, nikon_d7500_capture_settings,   NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0446, nikon_d780_capture_settings,    NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0414, nikon_d40_capture_settings,     NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0441, nikon_d850_capture_settings,    NULL,   NULL },
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0442, nikon_z6_capture_settings,      NULL,   NULL },	/* Z7 */
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0443, nikon_z6_capture_settings,      NULL,   NULL }, /* Z6 */
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0444, nikon_z6_capture_settings,      NULL,   NULL }, /* Z50 */
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0448, nikon_z6_capture_settings,      NULL,   NULL }, /* Z5 guessed */
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x044b, nikon_z6_capture_settings,      NULL,   NULL }, /* Z7_2 guessed */
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x044c, nikon_z6_capture_settings,      NULL,   NULL }, /* Z6_2 guessed */
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x044f, nikon_z6_capture_settings,      NULL,   NULL }, /* Zfc guessed */
        { N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0450, nikon_z6_capture_settings,      NULL,   NULL }, /* Z9 */
        { N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0451, nikon_z6_capture_settings,      NULL,   NULL }, /* Z8 */
	{ N_("Capture Settings"),           "capturesettings",  0x4b0,  0x0452, nikon_z6_capture_settings,      NULL,   NULL }, /* Z30 guessed */
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
	unsigned int	i;
	int		nrofsetprops = 0;
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
		(ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
		 ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)
		)
	) {
		if (!params->eos_captureenabled)
			camera_prepare_capture (camera, context);
		ptp_check_eos_events (params);
		/* Otherwise the camera will auto-shutdown */
		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_KeepDeviceOn))
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
					ret = LOG_ON_PTP_E(ptp_generic_getdevicepropdesc(params,cursub->propid,&dpd));
					if (ret != PTP_RC_OK)
						continue;

					if (cursub->type != dpd.DataType) {
						GP_LOG_E ("Type of property '%s' expected: 0x%04x got: 0x%04x", cursub->label, cursub->type, dpd.DataType );
						/* str is incompatible to all others */
						if ((PTP_DTC_STR == cursub->type) || (PTP_DTC_STR == dpd.DataType))
							continue;
						/* array is not compatible to non-array */
						if (((cursub->type ^ dpd.DataType) & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)
							continue;
						/* FIXME: continue to search here instead of below? */
					}
					ret = cursub->getfunc (camera, &widget, cursub, &dpd);
					if ((ret == GP_OK) && (dpd.GetSet == PTP_DPGS_Get))
						gp_widget_set_readonly (widget, 1);
					ptp_free_devicepropdesc(&dpd);

					if (ret != GP_OK) {
						/* the type might not have matched, try the next */
						GP_LOG_E ("Widget get of property '%s' failed, trying to see if we have another...", cursub->label);
						nrofsetprops--;
						continue;
					}
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
						if (ret == GP_OK && cursub->putfunc == _put_None) {
							gp_widget_set_readonly(widget, 1);
						}
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
			/* Canon EOS special handling */
			if (have_eos_prop(params,cursub->vendorid,cursub->propid)) {
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
				if (cursub->putfunc == _put_None) {
					gp_widget_set_readonly(widget, 1);
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
			/* Sigma FP special handling */
			if (have_sigma_prop(params,cursub->vendorid,cursub->propid)) {
				PTPDevicePropDesc	dpd;

				if ((mode == MODE_SINGLE_GET) && strcmp (cursub->name, confname))
					continue;
				if (mode == MODE_LIST) {
					gp_list_append (list, cursub->name, NULL);
					continue;
				}
				GP_LOG_D ("Getting property '%s' / 0x%04x", cursub->label, cursub->propid );
				memset(&dpd,0,sizeof(dpd));
				ret = cursub->getfunc (camera, &widget, cursub, &dpd);
				if (ret != GP_OK) {
					GP_LOG_D ("Failed to parse value of property '%s' / 0x%04x: error code %d", cursub->label, cursub->propid, ret);
					continue;
				}
				if (cursub->putfunc == _put_None) {
					gp_widget_set_readonly(widget, 1);
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
		char			buf[21], *label;
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
#define X(dtc,val,vartype,format) 										\
			case dtc: 								\
				if (type == GP_WIDGET_RANGE) {					\
					gp_widget_set_range ( widget, (float) dpd.FORM.Range.MinimumValue.val, (float) dpd.FORM.Range.MaximumValue.val, (float) dpd.FORM.Range.StepSize.val);\
				} else {							\
					vartype k;							\
					for (k=dpd.FORM.Range.MinimumValue.val;k<=dpd.FORM.Range.MaximumValue.val;k+=dpd.FORM.Range.StepSize.val) { \
						sprintf (buf, #format, k); 			\
						gp_widget_add_choice (widget, buf);		\
						if (dpd.FORM.Range.StepSize.val == 0) break;	\
					}							\
				} 								\
				break;

		X(PTP_DTC_INT8,i8,int8_t,%d)
		X(PTP_DTC_UINT8,u8,uint8_t,%u)
		X(PTP_DTC_INT16,i16,int16_t,%d)
		X(PTP_DTC_UINT16,u16,uint16_t,%u)
		X(PTP_DTC_INT32,i32,int32_t,%d)
		X(PTP_DTC_UINT32,u32,uint32_t,%u)
		X(PTP_DTC_INT64,i64,int64_t,%ld)
		X(PTP_DTC_UINT64,u64,uint64_t,%lu)
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
		/*gp_context_error (context, _("Property '%s' not found."), confname);*/
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
		(ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
		 ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)
		)
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
			int alreadyset = 0;

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

				gp_widget_set_changed (widget, FALSE); /* clear flag */
				GP_LOG_D ("Setting property '%s' / 0x%04x", cursub->label, cursub->propid );
				if (	((cursub->propid & 0x7000) == 0x5000) ||
					(NIKON_1(params) && ((cursub->propid & 0xf000) == 0xf000))
				){
					PTPDevicePropDesc dpd;

					memset(&dpd,0,sizeof(dpd));
					memset(&propval,0,sizeof(propval));

					C_PTP (ptp_generic_getdevicepropdesc(params,cursub->propid,&dpd));
					if (cursub->type != dpd.DataType) {
						GP_LOG_E ("Type of property '%s' expected: 0x%04x got: 0x%04x", cursub->label, cursub->type, dpd.DataType );
						/* str is incompatible to all others */
						if ((PTP_DTC_STR == cursub->type) || (PTP_DTC_STR == dpd.DataType))
							continue;
						/* array is not compatible to non-array */
						if (((cursub->type ^ dpd.DataType) & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)
							continue;
						/* FIXME: continue to search here perhaps instead of below? */
					}
					if (dpd.GetSet == PTP_DPGS_GetSet) {
						ret = cursub->putfunc (camera, widget, &propval, &dpd, &alreadyset);
					} else {
						gp_context_error (context, _("Sorry, the property '%s' / 0x%04x is currently read-only."), _(cursub->label), cursub->propid);
						ret = GP_ERROR_NOT_SUPPORTED;
					}
					if (ret == GP_OK) {
						if (!alreadyset) {
							ret_ptp = LOG_ON_PTP_E (ptp_generic_setdevicepropvalue (params, cursub->propid, &propval, cursub->type));
							if (ret_ptp != PTP_RC_OK) {
								gp_context_error (context, _("The property '%s' / 0x%04x was not set (0x%04x: %s)"),
										  _(cursub->label), cursub->propid, ret_ptp, _(ptp_strerror(ret_ptp, params->deviceinfo.VendorExtensionID)));
								ret = translate_ptp_result (ret_ptp);
							}
						}
						ptp_free_devicepropvalue (cursub->type, &propval);
					}
					ptp_free_devicepropdesc(&dpd);
					if (ret != GP_OK) continue; /* see if we have another match */
				} else {
					ret = cursub->putfunc (camera, widget, NULL, NULL, NULL);
				}
				if (mode == MODE_SINGLE_SET)
					return ret;
			}
			/* Canon EOS special handling */
			if (have_eos_prop(params,cursub->vendorid,cursub->propid)) {
				PTPDevicePropDesc	dpd;

				if ((mode == MODE_SINGLE_SET) && strcmp (confname, cursub->name))
					continue;
				gp_widget_set_changed (widget, FALSE); /* clear flag */
				if ((cursub->propid & 0x7000) == 0x5000) {
					GP_LOG_D ("Setting property '%s' / 0x%04x", cursub->label, cursub->propid);
					memset(&dpd,0,sizeof(dpd));
					ptp_canon_eos_getdevicepropdesc (params,cursub->propid, &dpd);
					ret = cursub->putfunc (camera, widget, &propval, &dpd, &alreadyset);
					if (ret == GP_OK) {
						if (!alreadyset) {
							ret_ptp = LOG_ON_PTP_E (ptp_canon_eos_setdevicepropvalue (params, cursub->propid, &propval, cursub->type));
							if (ret_ptp != PTP_RC_OK) {
								gp_context_error (context, _("The property '%s' / 0x%04x was not set (0x%04x: %s)."),
										  _(cursub->label), cursub->propid, ret_ptp, _(ptp_strerror(ret_ptp, params->deviceinfo.VendorExtensionID)));
								ret = translate_ptp_result (ret_ptp);
							}
						}
						ptp_free_devicepropvalue(cursub->type, &propval);
					} else
						gp_context_error (context, _("Parsing the value of widget '%s' / 0x%04x failed with %d."), _(cursub->label), cursub->propid, ret);
					ptp_free_devicepropdesc(&dpd);
				} else {
					GP_LOG_D ("Setting virtual property '%s' / 0x%04x", cursub->label, cursub->propid);
					/* if it is a OPC, check for its presence. Otherwise just use the widget. */
					if (	((cursub->type & 0x7000) != 0x1000) ||
						 ptp_operation_issupported(params, cursub->type)
					)
						ret = cursub->putfunc (camera, widget, &propval, &dpd, &alreadyset);
					else
						continue;
				}
				if (mode == MODE_SINGLE_SET)
					return ret;
			}
			/* Sigma FP special handling */
			if (have_sigma_prop(params,cursub->vendorid,cursub->propid)) {
				PTPDevicePropDesc	dpd; /* fake, unused here for now */

				if ((mode == MODE_SINGLE_SET) && strcmp (confname, cursub->name))
					continue;
				gp_widget_set_changed (widget, FALSE); /* clear flag */
				GP_LOG_D ("Setting property '%s' / 0x%04x", cursub->label, cursub->propid);
				memset(&dpd,0,sizeof(dpd));
				ret = cursub->putfunc (camera, widget, &propval, &dpd, &alreadyset);
				if (ret != GP_OK)
					gp_context_error (context, _("Parsing the value of widget '%s' / 0x%04x failed with %d."), _(cursub->label), cursub->propid, ret);
				if (mode == MODE_SINGLE_SET)
					return ret;
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
			gp_widget_set_changed (widget, FALSE);
		}

		gp_widget_get_type (widget, &type);

		memset (&dpd,0,sizeof(dpd));
		memset (&propval,0,sizeof(propval));
		ret = ptp_generic_getdevicepropdesc (params,propid,&dpd);
		if (ret != PTP_RC_OK)
			continue;
		if (dpd.GetSet != PTP_DPGS_GetSet) {
			gp_context_error (context, _("Sorry, the property '%s' / 0x%04x is currently read-only."), _(label), propid);
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

int
camera_lookup_by_property(Camera *camera, PTPDevicePropDesc *dpd, char **name, char **content, GPContext *context)
{
	unsigned int	menuno, submenuno;
	int 		ret;
	PTPParams	*params = &camera->pl->params;
	CameraAbilities	ab;
	uint16_t	propid = dpd->DevicePropertyCode;
	CameraWidget	*widget;

	*name = NULL;
	*content = NULL;

	SET_CONTEXT(camera, context);

	memset (&ab, 0, sizeof(ab));
	gp_camera_get_abilities (camera, &ab);
	if (	(params->deviceinfo.VendorExtensionID == PTP_VENDOR_CANON) &&
		(ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteRelease) ||
		 ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn)
		)
	) {
		if (!params->eos_captureenabled)
			camera_prepare_capture (camera, context);
		ptp_check_eos_events (params);
		/* Otherwise the camera will auto-shutdown */
		if (ptp_operation_issupported(params, PTP_OC_CANON_EOS_KeepDeviceOn))
			C_PTP (ptp_canon_eos_keepdeviceon (params));
	}

	for (menuno = 0; menuno < sizeof(menus)/sizeof(menus[0]) ; menuno++ ) {
		if (!menus[menuno].submenus) { /* Custom menu ... not exposed to by-property method */
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

		for (submenuno = 0; menus[menuno].submenus[submenuno].name ; submenuno++ ) {
			struct submenu		*cursub = menus[menuno].submenus+submenuno;
			CameraWidgetType	type;

			widget = NULL;

			if (	(cursub->propid == propid) &&
				have_prop(camera,cursub->vendorid,cursub->propid)
			) {

				/* ok, looking good */
				if (	((cursub->propid & 0x7000) == 0x5000) ||
					(NIKON_1(params) && ((cursub->propid & 0xf000) == 0xf000))
				) {
					GP_LOG_D ("Getting table based PTP property '%s' / 0x%04x", cursub->label, cursub->propid );
					if (cursub->type != dpd->DataType) {
						GP_LOG_E ("Type of property '%s' expected: 0x%04x got: 0x%04x", cursub->label, cursub->type, dpd->DataType );
						/* str is incompatible to all others */
						if ((PTP_DTC_STR == cursub->type) || (PTP_DTC_STR == dpd->DataType))
							continue;
						/* array is not compatible to non-array */
						if (((cursub->type ^ dpd->DataType) & PTP_DTC_ARRAY_MASK) == PTP_DTC_ARRAY_MASK)
							continue;
						/* FIXME: continue to search here instead of below? */
						continue;
					}
					ret = cursub->getfunc (camera, &widget, cursub, dpd);

					if (ret != GP_OK) {
						/* the type might not have matched, try the next */
						GP_LOG_E ("Widget get of property '%s' failed, trying to see if we have another...", cursub->label);
						continue;
					}
					CR (gp_widget_get_name (widget, (const char**)name));
					*name = strdup(*name?*name:"<null>");
					CR (gp_widget_get_type (widget, &type));
					switch (type) {
					case GP_WIDGET_RADIO:
					case GP_WIDGET_MENU:
					case GP_WIDGET_TEXT: {
						char *val;
						CR (gp_widget_get_value (widget, &val));
						*content = strdup(val?val:"<null>");
						break;
					}
					case GP_WIDGET_RANGE: {
						char buf[20];
						float fval;
						CR (gp_widget_get_value (widget, &fval));
						sprintf(buf,"%f",fval);
						*content = strdup(buf);
						break;
					}
					default:
						*content = strdup("Unhandled type");
						/* FIXME: decode value ... ? date? toggle? */
						break;
					}
					gp_widget_free (widget);
					return GP_OK;
				}
				continue;
			}
			/* Canon EOS special handling */
			if (	(cursub->propid == propid) &&
				have_eos_prop(params,cursub->vendorid,cursub->propid)
			) {
				GP_LOG_D ("Getting EOS property '%s' / 0x%04x", cursub->label, cursub->propid );
				ret = cursub->getfunc (camera, &widget, cursub, dpd);
				if (ret != GP_OK) {
					GP_LOG_D ("Failed to parse value of property '%s' / 0x%04x: error code %d", cursub->label, cursub->propid, ret);
					continue;
				}
				CR (gp_widget_get_name (widget, (const char**)name));
				*name = strdup(*name);
				CR (gp_widget_get_type (widget, &type));
				switch (type) {
				case GP_WIDGET_RADIO:
				case GP_WIDGET_MENU:
				case GP_WIDGET_TEXT: {
					char *val;
					CR (gp_widget_get_value (widget, &val));
					*content = strdup(val);
					break;
				}
				case GP_WIDGET_RANGE: {
					char buf[20];
					float fval;
					CR (gp_widget_get_value (widget, &fval));
					sprintf(buf,"%f",fval);
					*content = strdup(buf);
					break;
				}
				default:
					*content = strdup("Unhandled type");
					/* FIXME: decode value ... ? date? toggle? */
					break;
				}
				gp_widget_free (widget);
				return GP_OK;
			}
			/* Sigma FP special handling */
			if (have_sigma_prop(params,cursub->vendorid,cursub->propid) &&
				(cursub->propid == propid)
			) {
				GP_LOG_D ("Getting Sigma property '%s' / 0x%04x", cursub->label, cursub->propid );
				ret = cursub->getfunc (camera, &widget, cursub, dpd);
				if (ret != GP_OK) {
					GP_LOG_D ("Failed to parse value of property '%s' / 0x%04x: error code %d", cursub->label, cursub->propid, ret);
					continue;
				}
				CR (gp_widget_get_name (widget, (const char**)name));
				*name = strdup(*name);
				CR (gp_widget_get_type (widget, &type));
				switch (type) {
				case GP_WIDGET_RADIO:
				case GP_WIDGET_MENU:
				case GP_WIDGET_TEXT: {
					char *val;
					CR (gp_widget_get_value (widget, &val));
					*content = strdup(val?val:"NULL");
					break;
				}
				case GP_WIDGET_RANGE: {
					char buf[20];
					float fval;
					CR (gp_widget_get_value (widget, &fval));
					sprintf(buf,"%f",fval);
					*content = strdup(buf);
					break;
				}
				default:
					*content = strdup("Unhandled type");
					/* FIXME: decode value ... ? date? toggle? */
					break;
				}
				gp_widget_free (widget);
				return GP_OK;
			}
		}
	}

	if (have_prop(camera, params->deviceinfo.VendorExtensionID, propid)) {
		char			buf[21], *label;

		GP_LOG_D ("Getting generic PTP property 0x%04x", propid );
		sprintf(buf,"%04x", propid);

		label = (char*)ptp_get_property_description(params, propid);
		if (!label) {
			sprintf (buf, N_("PTP Property 0x%04x"), propid);
			label = buf;
		}
		*name = strdup (label);
		switch (dpd->DataType) {
#define X(dtc,val,fmt) 							\
		case dtc:						\
			sprintf (buf, fmt, dpd->CurrentValue.val);	\
			*content = strdup (buf);				\
			return GP_OK;					\

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
			*content = strdup (dpd->CurrentValue.str?dpd->CurrentValue.str:"<null>");
			return GP_OK;
		default:
			sprintf(buf, "Unknown type 0x%04x", dpd->DataType);
			*content = strdup (buf);
			return GP_OK;
		}
	}
	/* not found */
	return GP_ERROR_BAD_PARAMETERS;
}
