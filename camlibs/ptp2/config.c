/* config.c
 *
 * Copyright (C) 2003-2007 Marcus Meissner <marcus@jet.franken.de>
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
#define _BSD_SOURCE
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

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

#define GP_MODULE "PTP2"

#define CPR(context,result) {short r=(result); if (r!=PTP_RC_OK) {report_result ((context), r, params->deviceinfo.VendorExtensionID); return (translate_ptp_result (r));}}

int
camera_prepare_capture (Camera *camera, GPContext *context)
{
	PTPUSBEventContainer	event;
	PTPContainer		evc;
	PTPPropertyValue	propval;
	uint16_t		val16;
	int 			i, ret, isevent;
	PTPParams		*params = &camera->pl->params;
	int oldtimeout;
	
	gp_log (GP_LOG_DEBUG, "ptp", "prepare_capture\n");
	switch (params->deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		if (!ptp_operation_issupported(params, PTP_OC_CANON_StartShootingMode)) {
			gp_context_error(context,
			_("Sorry, your Canon camera does not support Canon capture"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		
		propval.u16 = 0;
    		ret = ptp_getdevicepropvalue(params, 0xd045, &propval, PTP_DTC_UINT16);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_DEBUG, "ptp", "failed get 0xd045\n");
			return GP_ERROR;
		}
		gp_log (GP_LOG_DEBUG, "ptp","prop 0xd045 value is 0x%4X\n",propval.u16);

    		propval.u16=1;
		ret = ptp_setdevicepropvalue(params, 0xd045, &propval, PTP_DTC_UINT16);
		ret = ptp_getdevicepropvalue(params, 0xd02e, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02e value is 0x%8X, ret %d\n",propval.u32, ret);
		ret = ptp_getdevicepropvalue(params, 0xd02f, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02f value is 0x%8X, ret %d\n",propval.u32, ret);

		ret = ptp_getdeviceinfo (params, &params->deviceinfo);
		ret = ptp_getdeviceinfo (params, &params->deviceinfo);

		ret = ptp_getdevicepropvalue(params, 0xd02e, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02e value is 0x%8x, ret %d\n",propval.u32, ret);
		ret = ptp_getdevicepropvalue(params, 0xd02f, &propval, PTP_DTC_UINT32);
		gp_log (GP_LOG_DEBUG, "ptp", "prop 0xd02f value is 0x%8x, ret %d\n",propval.u32,ret);
		ret = ptp_getdeviceinfo (params, &params->deviceinfo);
		ret = ptp_getdevicepropvalue(params, 0xd045, &propval, PTP_DTC_UINT16);
		gp_log (GP_LOG_DEBUG, "ptp","prop 0xd045 value is 0x%4x, ret %d\n",propval.u16,ret);

		gp_log (GP_LOG_DEBUG, "ptp","Magic code ends.\n");
        
		gp_log (GP_LOG_DEBUG, "ptp","Setting prop. 0xd045 to 4\n");
		propval.u16=4;
		ret = ptp_setdevicepropvalue(params, PTP_DPC_CANON_D045, &propval, PTP_DTC_UINT16);

		CPR (context, ptp_canon_startshootingmode (params));

		gp_port_get_timeout (camera->port, &oldtimeout);
		gp_port_set_timeout (camera->port, 1000);

		/* Catch event */
		if (PTP_RC_OK==(val16=params->event_wait (params, &evc))) {
			if (evc.Code==PTP_EC_StorageInfoChanged)
				gp_log (GP_LOG_DEBUG, "ptp", "Event: entering  shooting mode. \n");
			else 
				gp_log (GP_LOG_DEBUG, "ptp", "Event: 0x%X\n", evc.Code);
		} else {
			printf("No event yet, we'll try later.\n");
		}
    
		/* Emptying event stack */
		for (i=0;i<2;i++) {
			ret = ptp_canon_checkevent (params,&event,&isevent);
			if (ret != PTP_RC_OK) {
				gp_log (GP_LOG_DEBUG, "ptp", "error during check event: %d\n", ret);
			}
			if (isevent)
				gp_log (GP_LOG_DEBUG, "ptp", "evdata: L=0x%X, T=0x%X, C=0x%X, trans_id=0x%X, p1=0x%X, p2=0x%X, p3=0x%X\n",
					event.length,event.type,event.code,event.trans_id,
					event.param1, event.param2, event.param3);
		}
		/* Catch event, attempt  2 */
		if (val16!=PTP_RC_OK) {
			if (PTP_RC_OK==params->event_wait (params, &evc)) {
				if (evc.Code == PTP_EC_StorageInfoChanged)
					gp_log (GP_LOG_DEBUG, "ptp","Event: entering shooting mode.\n");
				else
					gp_log (GP_LOG_DEBUG, "ptp","Event: 0x%X\n", evc.Code);
			} else
				gp_log (GP_LOG_DEBUG, "ptp", "No expected mode change event.\n");
		}
		/* Reget device info, they change on the Canons. */
		ptp_getdeviceinfo(&camera->pl->params, &camera->pl->params.deviceinfo);
		gp_port_set_timeout (camera->port, oldtimeout);
		return GP_OK;
	default:
		/* generic capture does not need preparation */
		return GP_OK;
	}
	return GP_OK;
}

int
camera_unprepare_capture (Camera *camera, GPContext *context)
{
	int ret;

	gp_log (GP_LOG_DEBUG, "ptp", "Unprepare_capture\n");
	switch (camera->pl->params.deviceinfo.VendorExtensionID) {
	case PTP_VENDOR_CANON:
		if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_CANON_EndShootingMode)) {
			gp_context_error(context,
			_("Sorry, your Canon camera does not support Canon capture"));
			return GP_ERROR_NOT_SUPPORTED;
		}
		ret = ptp_canon_endshootingmode (&camera->pl->params);
		if (ret != PTP_RC_OK) {
			gp_log (GP_LOG_DEBUG, "ptp", "end shooting mode error %d\n", ret);
			return GP_ERROR;
		}
		/* Reget device info, they change on the Canons. */
		ptp_getdeviceinfo(&camera->pl->params, &camera->pl->params.deviceinfo);
		return GP_OK;
	default:
		/* generic capture does not need unpreparation */
		return GP_OK;
	}
	return GP_OK;
}


static int
have_prop(Camera *camera, uint16_t vendor, uint16_t prop) {
	int i;

	/* prop 0 matches */
	if (!prop && (camera->pl->params.deviceinfo.VendorExtensionID==vendor))
		return 1;

	for (i=0; i<camera->pl->params.deviceinfo.DevicePropertiesSupported_len; i++) {
		if (prop != camera->pl->params.deviceinfo.DevicePropertiesSupported[i])
			continue;
		if ((prop & 0xf000) == 0x5000) /* generic property */
			return 1;
		if (camera->pl->params.deviceinfo.VendorExtensionID==vendor)
			return 1;
	}
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

/* Generic helper function for:
 *
 * ENUM UINT16 propertiess, with potential vendor specific variables.
 */
static int
_get_Generic16Table(CONFIG_GET_ARGS, struct deviceproptableu16* tbl, int tblsize) {
	int i, j;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	if (!dpd->FORM.Enum.NumberOfValues) {
		/* fill in with all values we have in the table. */
		for (j=0;j<tblsize;j++) {
			if ((tbl[j].vendor_id == 0) ||
		     	    (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID)
			) {
				gp_widget_add_choice (*widget, _(tbl[j].label));
				if (tbl[j].value == dpd->CurrentValue.u16)
					gp_widget_set_value (*widget, _(tbl[j].label));
			}
		}
		return GP_OK;
	}
	for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
		int isset = FALSE;

		for (j=0;j<tblsize;j++) {
			if ((tbl[j].value == dpd->FORM.Enum.SupportedValue[i].u16) &&
			    ((tbl[j].vendor_id == 0) ||
			     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
			) {
				gp_widget_add_choice (*widget, _(tbl[j].label));
				if (tbl[j].value == dpd->CurrentValue.u16)
					gp_widget_set_value (*widget, _(tbl[j].label));
				isset = TRUE;
				break;
			}
		}
		if (!isset) {
			char buf[200];
			sprintf(buf, _("Unknown value %04x"), dpd->FORM.Enum.SupportedValue[i].u16);
			gp_widget_add_choice (*widget, buf);
			if (dpd->FORM.Enum.SupportedValue[i].u16 == dpd->CurrentValue.u16)
				gp_widget_set_value (*widget, buf);
		}
	}
	return (GP_OK);
}


static int
_put_Generic16Table(CONFIG_PUT_ARGS, struct deviceproptableu16* tbl, int tblsize) {
	char *value;
	int i, ret, intval;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<tblsize;i++) {
		if (!strcmp(_(tbl[i].label),value) &&
		    ((tbl[i].vendor_id == 0) || (tbl[i].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
		) {
			propval->u16 = tbl[i].value;
			gp_log (GP_LOG_DEBUG, "ptp2/config:g16tbl", "returning %d for %s", propval->u16, value);
			return GP_OK;
		}
	}
	if (!sscanf(value, _("Unknown value %04x"), &intval)) {
		gp_log (GP_LOG_ERROR, "ptp2/config", "failed to find value %s in list", value);
		return (GP_ERROR);
	}
	propval->u16 = intval;
	gp_log (GP_LOG_DEBUG, "ptp2/config:g16tbl", "returning %d for %s", propval->u16, value);
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
static int						\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Generic16Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

static int
_get_Generic8Table(CONFIG_GET_ARGS, struct deviceproptableu8* tbl, int tblsize) {
	int i, j;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		if (dpd->DataType != PTP_DTC_UINT8)
			return (GP_ERROR);
		for (i = 0; i<dpd->FORM.Enum.NumberOfValues; i++) {
			int isset = FALSE;

			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == dpd->FORM.Enum.SupportedValue[i].u8) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.u8)
						gp_widget_set_value (*widget, _(tbl[j].label));
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
		return (GP_OK);
	}
	if (dpd->FormFlag & PTP_DPFF_Range) {
		if (dpd->DataType != PTP_DTC_UINT8)
			return (GP_ERROR);
		for (	i = dpd->FORM.Range.MinimumValue.u8;
			i <= dpd->FORM.Range.MaximumValue.u8;
			i+= dpd->FORM.Range.StepSize.u8
		) {
			int isset = FALSE;

			for (j=0;j<tblsize;j++) {
				if ((tbl[j].value == i) &&
				    ((tbl[j].vendor_id == 0) ||
				     (tbl[j].vendor_id == camera->pl->params.deviceinfo.VendorExtensionID))
				) {
					gp_widget_add_choice (*widget, _(tbl[j].label));
					if (tbl[j].value == dpd->CurrentValue.u8)
						gp_widget_set_value (*widget, _(tbl[j].label));
					isset = TRUE;
					break;
				}
			}
			if (!isset) {
				char buf[200];
				sprintf(buf, _("Unknown value %04x"), dpd->FORM.Range.MaximumValue.u8);
				gp_widget_add_choice (*widget, buf);
				if (dpd->FORM.Range.MaximumValue.u8 == dpd->CurrentValue.u8)
					gp_widget_set_value (*widget, buf);
			}
		}
		return (GP_OK);
	}
	return (GP_ERROR);
}


static int
_put_Generic8Table(CONFIG_PUT_ARGS, struct deviceproptableu8* tbl, int tblsize) {
	char *value;
	int i, ret, intval;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
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
static int						\
_put_##name(CONFIG_PUT_ARGS) {				\
	return _put_Generic8Table(CONFIG_PUT_NAMES,	\
		tbl,sizeof(tbl)/sizeof(tbl[0])		\
	);						\
}

static int
_get_AUINT8_as_CHAR_ARRAY(CONFIG_GET_ARGS) {
	int	j;
	char 	value[128];

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
        int ret;
        ret = gp_widget_get_value (widget,&string);
        if (ret != GP_OK)
                return ret;
        propval->str = strdup (string);
	if (!propval->str)
		return (GP_ERROR_NO_MEMORY);
        return (GP_OK);
}

static int
_put_AUINT8_as_CHAR_ARRAY(CONFIG_PUT_ARGS) {
	char	*value;
	int	i, ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	memset(propval,0,sizeof(PTPPropertyValue));
	/* add \0 ? */
	propval->a.v = malloc((strlen(value)+1)*sizeof(PTPPropertyValue));
	if (!propval->a.v)
		return (GP_ERROR_NO_MEMORY);
	propval->a.count = strlen(value)+1;
	for (i=0;i<strlen(value)+1;i++)
		propval->a.v[i].u8 = value[i];
	return (GP_OK);
}

#if 0
static int
_get_Range_INT8(CONFIG_GET_ARGS) {
	float CurrentValue;
	
	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	if (dpd->FormFlag != PTP_DPFF_Range)
		return (GP_ERROR_NOT_SUPPORTED);
	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR_NOT_SUPPORTED);
	CurrentValue = (float) dpd->CurrentValue.i8;
	gp_widget_set_range ( *widget, (float) dpd->FORM.Range.MinimumValue.i8, (float) dpd->FORM.Range.MaximumValue.i8, (float) dpd->FORM.Range.StepSize.i8);
	gp_widget_set_value ( *widget, &CurrentValue);
	return (GP_OK);
}


static int
_put_Range_INT8(CONFIG_PUT_ARGS) {
	int ret;
	float f;
	ret = gp_widget_get_value (widget, &f);
	if (ret != GP_OK) 
		return ret;
	propval->i8 = (int) f;
	return (GP_OK);
}
#endif

static int
_get_Nikon_OnOff_UINT8(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name ( *widget, menu->name);
	if (dpd->FormFlag != PTP_DPFF_Range)
                return (GP_ERROR_NOT_SUPPORTED);
        if (dpd->DataType != PTP_DTC_UINT8)
                return (GP_ERROR_NOT_SUPPORTED);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value ( *widget, (dpd->CurrentValue.u8?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Nikon_OnOff_UINT8(CONFIG_PUT_ARGS) {
	int ret;
	char *value;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK) 
		return ret;
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
};
GENERIC16TABLE(WhiteBalance,whitebalance)


/* Everything is camera specific. */
static struct deviceproptableu8 compression[] = {
	{ N_("JPEG Basic"),	0x00, PTP_VENDOR_NIKON },
	{ N_("JPEG Normal"),	0x01, PTP_VENDOR_NIKON },
	{ N_("JPEG Fine"),	0x02, PTP_VENDOR_NIKON },
	{ N_("NEF (Raw)"),	0x03, PTP_VENDOR_NIKON },
	{ N_("NEF+Basic"),	0x04, PTP_VENDOR_NIKON },
	{ N_("NEF+Normal"),	0x05, PTP_VENDOR_NIKON },
	{ N_("NEF+Fine"),	0x06, PTP_VENDOR_NIKON },
};
GENERIC8TABLE(Compression,compression)


static int
_get_ImageSize(CONFIG_GET_ARGS) {
        int j;

        gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
        gp_widget_set_name (*widget, menu->name);
        if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
                return(GP_ERROR);
        if (dpd->DataType != PTP_DTC_STR)
                return(GP_ERROR);
        for (j=0;j<dpd->FORM.Enum.NumberOfValues; j++) {
                gp_widget_add_choice (*widget,dpd->FORM.Enum.SupportedValue[j].str);
        }
        gp_widget_set_value (*widget,dpd->CurrentValue.str);
        return GP_OK;
}

static int
_put_ImageSize(CONFIG_PUT_ARGS) {
        char *value;
        int ret;

        ret = gp_widget_get_value (widget,&value);
        if(ret != GP_OK)
                return ret;
        propval->str = strdup (value);
	if (!propval->str)
		return (GP_ERROR_NO_MEMORY);
        return(GP_OK);
}


static int
_get_Canon_AssistLight(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value (*widget,(dpd->CurrentValue.u16?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Canon_AssistLight(CONFIG_PUT_ARGS) {
	char *value;
	int ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	if (!strcmp (value, _("On"))) {
		propval->u16 = 1;
		return (GP_OK);
	}
	if (!strcmp (value, _("Off"))) {
		propval->u16 = 0;
		return (GP_OK);
	}
	return (GP_ERROR);
}

static int
_get_Canon_BeepMode(CONFIG_GET_ARGS) {
	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	gp_widget_add_choice (*widget,_("On"));
	gp_widget_add_choice (*widget,_("Off"));
	gp_widget_set_value (*widget,(dpd->CurrentValue.u8?_("On"):_("Off")));
	return (GP_OK);
}

static int
_put_Canon_BeepMode(CONFIG_PUT_ARGS) {
	char *value;
	int ret;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;
	if (!strcmp (value, _("On"))) {
		propval->u8 = 1;
		return (GP_OK);
	}
	if (!strcmp (value, _("Off"))) {
		propval->u8 = 0;
		return (GP_OK);
	}
	return (GP_ERROR);
}

static int
_get_Canon_ZoomRange(CONFIG_GET_ARGS) {
	float	f, t, b, s;

	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget,menu->name);
	f = (float)dpd->CurrentValue.u16;
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
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
	int	ret;

	f = 0.0;
	ret = gp_widget_get_value (widget,&f);
	if (ret != GP_OK) return ret;
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
	int	ret;

	f = 0.0;
	ret = gp_widget_get_value (widget,&f);
	if (ret != GP_OK) return ret;
	propval->i8 = (signed char)f;
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
	int	ret;

	if (dpd->FormFlag & PTP_DPFF_Range) {
		float	f = 0.0;
		ret = gp_widget_get_value (widget,&f);
		if (ret != GP_OK) return ret;
		propval->i8 = (signed char)f;
		return (GP_OK);
	}
	if (dpd->FormFlag & PTP_DPFF_Enumeration) {
		char *val;
		int ival;
		
		ret = gp_widget_get_value (widget, &val);
		if (ret != GP_OK) return ret;
		sscanf (val, "%d", &ival);
		propval->i8 = ival;
		return (GP_OK);
	}
	return (GP_ERROR);
}


static struct deviceproptableu8 canon_quality[] = {
	{ N_("normal"),		0x02, 0 },
	{ N_("fine"),		0x03, 0 },
	{ N_("super fine"),	0x05, 0 },
};
GENERIC8TABLE(Canon_Quality,canon_quality)

static struct deviceproptableu8 canon_shootmode[] = {
	{ N_("Auto"),		0x01, 0 },
	{ N_("Tv"),		0x02, 0 },
	{ N_("Av"),		0x03, 0 },
	{ N_("Manual"),		0x04, 0 },
};
GENERIC8TABLE(Canon_ShootMode,canon_shootmode)

static struct deviceproptableu8 canon_flash[] = {
	{ N_("off"),	0, 0 },
	{ N_("auto"),	1, 0 },
	{ N_("on"),	2, 0 },
	{ N_("auto red eye"), 5, 0 },
	{ N_("on red eye"), 6, 0 },
};
GENERIC8TABLE(Canon_FlashMode,canon_flash)

static struct deviceproptableu8 nikon_flashmode[] = {
	{ N_("iTTL"),		0, 0 },
	{ N_("Manual"),		1, 0 },
	{ N_("Commander"),	2, 0 },
};
GENERIC8TABLE(Nikon_FlashMode,nikon_flashmode)

static struct deviceproptableu8 flash_modemanualpower[] = {
	{ N_("Full"),	0x00, 0 },
	{ "1/2",	0x01, 0 },
	{ "1/4",	0x02, 0 },
	{ "1/8",	0x03, 0 },
	{ "1/16",	0x04, 0 },
};
GENERIC8TABLE(Nikon_FlashModeManualPower,flash_modemanualpower)

static struct deviceproptableu8 canon_meteringmode[] = {
	{ "center weighted(?)",	0, 0 },
	{ N_("spot"),		1, 0 },
	{ "integral(?)",	3, 0 },
};
GENERIC8TABLE(Canon_MeteringMode,canon_meteringmode)


static struct deviceproptableu16 canon_shutterspeed[] = {
      {  "auto",0x0000,0 },
      {  "15\"",0x0018,0 },
      {  "13\"",0x001b,0 },
      {  "10\"",0x001d,0 },
      {  "8\"",0x0020,0 },
      {  "6\"",0x0023,0 },
      {  "5\"",0x0025,0 },
      {  "4\"",0x0028,0 },
      {  "3\"2",0x002b,0 },
      {  "2\"5",0x002d,0 },
      {  "2\"",0x0030,0 },
      {  "1\"6",0x0033,0 },
      {  "1\"3",0x0035,0 },
      {  "1\"",0x0038,0 },
      {  "0\"8",0x003b,0 },
      {  "0\"6",0x003d,0 },
      {  "0\"5",0x0040,0 },
      {  "0\"4",0x0043,0 },
      {  "0\"3",0x0045,0 },
      {  "1/4",0x0048,0 },
      {  "1/5",0x004b,0 },
      {  "1/6",0x004d,0 },
      {  "1/8",0x0050,0 },
      {  "1/10",0x0053,0 },
      {  "1/13",0x0055,0 },
      {  "1/15",0x0058,0 },
      {  "1/20",0x005b,0 },
      {  "1/25",0x005d,0 },
      {  "1/30",0x0060,0 },
      {  "1/40",0x0063,0 },
      {  "1/50",0x0065,0 },
      {  "1/60",0x0068,0 },
      {  "1/80",0x006b,0 },
      {  "1/100",0x006d,0 },
      {  "1/125",0x0070,0 },
      {  "1/160",0x0073,0 },
      {  "1/200",0x0075,0 },
      {  "1/250",0x0078,0 },
      {  "1/320",0x007b,0 },
      {  "1/400",0x007d,0 },
      {  "1/500",0x0080,0 },
      {  "1/640",0x0083,0 },
      {  "1/800",0x0085,0 },
      {  "1/1000",0x0088,0 },
      {  "1/1250",0x008b,0 },
      {  "1/1600",0x008d,0 },
      {  "1/2000",0x0090,0 },
};
GENERIC16TABLE(Canon_ShutterSpeed,canon_shutterspeed)


static struct deviceproptableu16 canon_focuspoints[] = {
	{ N_("Center"),	0x3003, 0 },
	{ N_("Auto"),	0x3001, 0 },
};
GENERIC16TABLE(Canon_FocusingPoint,canon_focuspoints)


static struct deviceproptableu8 canon_size[] = {
	{ N_("large"),		0x00, 0 },
	{ N_("medium 1"),	0x01, 0 },
	{ N_("medium 2"),	0x03, 0 },
	{ N_("small"),		0x02, 0 },
};
GENERIC8TABLE(Canon_Size,canon_size)


static struct deviceproptableu16 canon_isospeed[] = {
	{ N_("Factory Default"),0xffff, 0 },
	{ "50",			0x0040, 0 },
	{ "100",		0x0048, 0 },
	{ "200",		0x0050, 0 },
	{ "400",		0x0058, 0 },
	{ "800",		0x0060, 0 },
	{ "1600",		0x0068, 0 },
	{ N_("Auto"),		0x0000, 0 },
};
GENERIC16TABLE(Canon_ISO,canon_isospeed)

static int
_get_ISO(CONFIG_GET_ARGS) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);

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
	int ret;
	char *value;
	unsigned int	u;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;

	if (sscanf(value, "%ud", &u)) {
		propval->u16 = u;
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_get_FNumber(CONFIG_GET_ARGS) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);

        for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char	buf[20];

		sprintf(buf,"f/%g",(dpd->FORM.Enum.SupportedValue[i].u16*1.0)/100.0);
                gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u16 == dpd->CurrentValue.u16)
                	gp_widget_set_value (*widget,buf);
        }
	return (GP_OK);
}

static int
_put_FNumber(CONFIG_PUT_ARGS)
{
	int	ret;
	char	*value;
	float	f;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;

	if (sscanf(value, "f/%g", &f)) {
		propval->u16 = f*100;
		return GP_OK;
	}
	return GP_ERROR;
}

static int
_get_ExpTime(CONFIG_GET_ARGS) {
	int i;

	gp_widget_new (GP_WIDGET_RADIO, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Enumeration))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT32)
		return (GP_ERROR);

        for (i=0;i<dpd->FORM.Enum.NumberOfValues; i++) {
		char	buf[20];

		if (dpd->FORM.Enum.SupportedValue[i].u32%1000)
			sprintf (buf,"%d.%03d",
				dpd->FORM.Enum.SupportedValue[i].u32/1000,
				dpd->FORM.Enum.SupportedValue[i].u32%1000
			);
		else
			sprintf (buf,"%d",dpd->FORM.Enum.SupportedValue[i].u32/1000);
                gp_widget_add_choice (*widget,buf);
		if (dpd->FORM.Enum.SupportedValue[i].u32 == dpd->CurrentValue.u32)
                	gp_widget_set_value (*widget,buf);
        }
	return (GP_OK);
}

static int
_put_ExpTime(CONFIG_PUT_ARGS)
{
	int	ret, val;
	char	*value;

	ret = gp_widget_get_value (widget, &value);
	if (ret != GP_OK)
		return ret;

	if (strchr(value,'.')) {
		int val2;

		if (!sscanf(value,"%d.%d",&val,&val2))
			return (GP_ERROR);
		propval->u32 = val*1000+val2;
		return (GP_OK);
	}
	if (!sscanf(value,"%d",&val))
		return (GP_ERROR);
	propval->u32 = val*1000;
	return (GP_OK);
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
};
GENERIC16TABLE(ExposureProgram,exposure_program_modes)


static struct deviceproptableu16 capture_mode[] = {
	{ N_("Single Shot"),		0x0001, 0 },
	{ N_("Burst"),			0x0002, 0 },
	{ N_("Timelapse"),		0x0003, 0 },
	{ N_("Continuous Low Speed"),	0x8010, PTP_VENDOR_NIKON},
	{ N_("Timer"),			0x8011, PTP_VENDOR_NIKON},
	{ N_("Mirror Up"),		0x8012, PTP_VENDOR_NIKON},
	{ N_("Remote"),			0x8013, PTP_VENDOR_NIKON},
	{ N_("Timer + Remote"),		0x8014, PTP_VENDOR_NIKON},
};
GENERIC16TABLE(CaptureMode,capture_mode)


static struct deviceproptableu16 focus_metering[] = {
	{ N_("Dynamic Area"),	0x0002, 0 },
	{ N_("Single Area"),	0x8010, PTP_VENDOR_NIKON},
	{ N_("Closest Subject"),0x8011, PTP_VENDOR_NIKON},
	{ N_("Group Dynamic"),  0x8012, PTP_VENDOR_NIKON},
};
GENERIC16TABLE(FocusMetering,focus_metering)


static struct deviceproptableu8 nikon_colormodel[] = {
	{ N_("sRGB (portrait)"), 0x00, 0 },
	{ N_("AdobeRGB"),        0x01, 0 },
	{ N_("sRGB (nature)"),   0x02, 0 },
};
GENERIC8TABLE(Nikon_ColorModel,nikon_colormodel)


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
};
GENERIC16TABLE(ExposureMetering,exposure_metering)


static struct deviceproptableu16 flash_mode[] = {
	{ N_("Automatic Flash"),		0x0001, 0 },
	{ N_("Flash off"),			0x0002, 0 },
	{ N_("Fill flash"),			0x0003, 0 },
	{ N_("Red-eye automatic"),		0x0004, 0 },
	{ N_("Red-eye fill"),			0x0005, 0 },
	{ N_("External sync"),			0x0006, 0 },
	{ N_("Default"),			0x8010, PTP_VENDOR_NIKON},
	{ N_("Slow Sync"),			0x8011, PTP_VENDOR_NIKON},
	{ N_("Rear Curtain Sync + Slow Sync"),	0x8012, PTP_VENDOR_NIKON},
	{ N_("Red-eye Reduction + Slow Sync"),	0x8013, PTP_VENDOR_NIKON},
};
GENERIC16TABLE(FlashMode,flash_mode)

static struct deviceproptableu16 effect_modes[] = {
	{ N_("Standard"),	0x0001, 0 },
	{ N_("Black & White"),	0x0002, 0 },
	{ N_("Sepia"),		0x0003, 0 },
};
GENERIC16TABLE(EffectMode,effect_modes)


static int
_get_FocalLength(CONFIG_GET_ARGS) {
	float value_float , start=0.0, end=0.0, step=0.0;
	int i;

	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	if (!(dpd->FormFlag & (PTP_DPFF_Range|PTP_DPFF_Enumeration)))
		return (GP_ERROR);

	if (dpd->DataType != PTP_DTC_UINT32)
		return (GP_ERROR);

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
	int ret, i;
	float value_float;
	uint32_t curdiff, newval;

	ret = gp_widget_get_value (widget, &value_float);
	if (ret != GP_OK)
		return ret;
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
_get_Nikon_FlashExposureCompensation(CONFIG_GET_ARGS) {
	float value_float;

	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_INT8)
		return (GP_ERROR);
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
	int ret;
	float value_float;

	ret = gp_widget_get_value (widget, &value_float);
	if (ret != GP_OK)
		return ret;
	propval->i8 = 6.0*value_float;
	return GP_OK;

}

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
};
GENERIC8TABLE(Nikon_LCDOffTime,nikon_lcdofftime)

static struct deviceproptableu8 nikon_meterofftime[] = {
	{ N_("4 seconds"),	0x00, 0 },
	{ N_("6 seconds"),	0x01, 0 },
	{ N_("8 seconds"),	0x02, 0 },
	{ N_("16 seconds"),	0x03, 0 },
	{ N_("30 minutes"),	0x04, 0 },
};
GENERIC8TABLE(Nikon_MeterOffTime,nikon_meterofftime)

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
	{ N_("20 mm"),	0x03, 0 },
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
	{ N_("Medium low"),	0x03, 0 },
	{ N_("Medium high"),	0x04, 0 },
	{ N_("High control"),	0x05, 0 },
	{ N_("Custom"),		0x06, 0 },
};
GENERIC8TABLE(Nikon_ToneCompensation,nikon_tonecompensation)

static struct deviceproptableu8 canon_afdistance[] = {
	{ N_("Off"),		0x01, 0 },
	{ N_("Macro"),		0x03, 0 },
	{ N_("Long distance"),	0x07, 0 }, /* Unchecked. */
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
};
GENERIC16TABLE(FocusMode,focusmodes)


static struct deviceproptableu8 canon_whitebalance[] = {
      { N_("Auto"),		0, 0 },
      { N_("Daylight"),		1, 0 },
      { N_("Cloudy"),		2, 0 },
      { N_("Tungsten"),		3, 0 },
      { N_("Fluorescent"),	4, 0 },
      { N_("Fluorescent H"),	7, 0 },
      { N_("Custom"),		6, 0 },
};
GENERIC8TABLE(Canon_WhiteBalance,canon_whitebalance)


static struct deviceproptableu8 canon_expcompensation[] = {
      { N_("Factory Default"),	0xff, 0 },
      { "+2",			0x08, 0 },
      { "+1 2/3",		0x0b, 0 },
      { "+1 1/3",		0x0d, 0 },
      { "+1",			0x10, 0 },
      { "+2/3",			0x13, 0 },
      { "+1/3",			0x15, 0 },
      { "0",			0x18, 0 },
      { "-1/3",			0x1b, 0 },
      { "-2/3",			0x1d, 0 },
      { "-1",			0x20, 0 },
      { "-1 1/3",		0x23, 0 },
      { "-1 2/3",		0x25, 0 },
      { "-2",			0x28, 0 },
};
GENERIC8TABLE(Canon_ExpCompensation,canon_expcompensation)


static struct deviceproptableu16 canon_photoeffect[] = {
      { N_("Off"),		0, 0 },
      { N_("Vivid"),		1, 0 },
      { N_("Neutral"),		2, 0 },
      { N_("Low sharpening"),	3, 0 },
      { N_("Sepia"),		4, 0 },
      { N_("Black & white"),	5, 0 },
};
GENERIC16TABLE(Canon_PhotoEffect,canon_photoeffect)


static struct deviceproptableu16 canon_aperture[] = {
      { N_("auto"),	0xffff, 0 },
      { "1.8",		0x0015, 0 },
      { "2.0",		0x0018, 0 },
      { "2.2",		0x001b, 0 },
      { "2.5",		0x001d, 0 },
      { "2.8",		0x0020, 0 },
      { "3.2",		0x0023, 0 },
      { "3.5",		0x0025, 0 },
      { "4.0",		0x0028, 0 },
      { "4.5",		0x002b, 0 },
      { "5",		0x002d, 0 },
      { "5.6",		0x0030, 0 },
      { "6.3",		0x0033, 0 },
      { "7.1",		0x0035, 0 },
      { "8.0",		0x0038, 0 },
      { "9.0",		0x003b, 0 },
      { "10",		0x003d, 0 },
      { "11",		0x0040, 0 },
      { "13",		0x0043, 0 },
      { "14",		0x0045, 0 },
      { "16",		0x0048, 0 },
      { "18",		0x004b, 0 },
      { "20",		0x004d, 0 },
      { "22",		0x0050, 0 },
};
GENERIC16TABLE(Canon_Aperture,canon_aperture)


static struct deviceproptableu8 nikon_bracketset[] = {
      { N_("AE & Flash"),	0, 0 },
      { N_("AE only"),		1, 0 },
      { N_("Flash only"),	2, 0 },
      { N_("WB bracketing"),	3, 0 },
};
GENERIC8TABLE(Nikon_BracketSet,nikon_bracketset)

static struct deviceproptableu8 nikon_saturation[] = {
      { N_("Normal"),	0, 0 },
      { N_("Moderate"),	1, 0 },
      { N_("Enhanced"),	2, 0 },
};
GENERIC8TABLE(Nikon_Saturation,nikon_saturation)


static struct deviceproptableu8 nikon_bracketorder[] = {
      { N_("MTR > Under"),	0, 0 },
      { N_("Under > MTR"),	1, 0 },
};
GENERIC8TABLE(Nikon_BracketOrder,nikon_bracketorder)

static int
_get_BurstNumber(CONFIG_GET_ARGS) {
	float value_float , start=0.0, end=0.0, step=0.0;

	gp_widget_new (GP_WIDGET_RANGE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);

	if (dpd->DataType != PTP_DTC_UINT16)
		return (GP_ERROR);

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
	int ret;
	float value_float;

	ret = gp_widget_get_value (widget, &value_float);
	if (ret != GP_OK)
		return ret;
	propval->u16 = value_float;
	return GP_OK;
}

static int
_get_BatteryLevel(CONFIG_GET_ARGS) {
	unsigned char value_float , start, end;
	char	buffer[20];

	gp_widget_new (GP_WIDGET_TEXT, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	if (!(dpd->FormFlag & PTP_DPFF_Range))
		return (GP_ERROR);
	if (dpd->DataType != PTP_DTC_UINT8)
		return (GP_ERROR);
	start = dpd->FORM.Range.MinimumValue.u8;
	end = dpd->FORM.Range.MaximumValue.u8;
	value_float = dpd->CurrentValue.u8;
	sprintf (buffer, "%d%%", (int)((value_float-start+1)*100/(end-start+1)));
	gp_widget_set_value(*widget, buffer);
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
	int	ret;

	camtime = 0;
	ret = gp_widget_get_value (widget,&camtime);
	if (ret != GP_OK)
		return ret;
	propval->u32 = camtime;
	return (GP_OK);
}

static int
_get_STR_as_time(CONFIG_GET_ARGS) {
	time_t		camtime;
	struct tm	tm;
	char		capture_date[64],tmp[5];

	/* strptime() is not widely accepted enough to use yet */
	memset(&tm,0,sizeof(tm));
	gp_widget_new (GP_WIDGET_DATE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	if (!dpd->CurrentValue.str)
		return (GP_ERROR);
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
	int		ret;
	char		asctime[64];

	camtime = 0;
	ret = gp_widget_get_value (widget,&camtime);
	if (ret != GP_OK)
		return ret;
#ifdef HAVE_GMTIME_R
	pxtm = gmtime_r (&camtime, &xtm);
#else
	pxtm = gmtime (&camtime);
#endif
	/* 20020101T123400.0 is returned by the HP Photosmart */
	sprintf(asctime,"%04d%02d%02dT%02d%02d%02d.0",pxtm->tm_year+1900,pxtm->tm_mon+1,pxtm->tm_mday,pxtm->tm_hour,pxtm->tm_min,pxtm->tm_sec);
	propval->str = strdup(asctime);
	if (!propval->str)
		return (GP_ERROR_NO_MEMORY);
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
	val = 0;
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Canon_CaptureMode(CONFIG_PUT_ARGS) {
	int val, ret;

	ret = gp_widget_get_value (widget, &val);
	if (ret != GP_OK)
		return ret;
	if (val)
		return camera_prepare_capture (camera, NULL);
	else
		return camera_unprepare_capture (camera, NULL);
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
	int val, ret;

	ret = gp_widget_get_value (widget, &val);
	if (ret != GP_OK)
		return ret;
	if (val)
		ret = ptp_canon_focuslock (params);
	else
		ret = ptp_canon_focusunlock (params);
	if (ret == PTP_RC_OK)
		return (GP_OK);
	return (GP_ERROR);
}


static int
_get_Nikon_BeepMode(CONFIG_GET_ARGS) {
	int val;

	gp_widget_new (GP_WIDGET_TOGGLE, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);
	val = !dpd->CurrentValue.u8;
	gp_widget_set_value  (*widget, &val);
	return (GP_OK);
}

static int
_put_Nikon_BeepMode(CONFIG_PUT_ARGS) {
	int val, ret;

	ret = gp_widget_get_value (widget, &val);
	if (ret != GP_OK)
		return ret;

	propval->u8 = !val;
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
	int val, ret;
	char buf[20];

	ret = gp_widget_get_value (widget, &val);
	if (ret != GP_OK)
		return ret;
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
	return (GP_OK);
}

static int
_put_CaptureTarget(CONFIG_PUT_ARGS) {
	int i, ret;
	char *val;

	ret = gp_widget_get_value (widget, &val);
	if (ret != GP_OK)
		return ret;
	for (i=0;i<sizeof(capturetargets)/sizeof(capturetargets[i]);i++) {
		if (!strcmp( val, _(capturetargets[i].label))) {
			gp_setting_set("ptp2","capturetarget",capturetargets[i].name);
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
	int deleted = 0;

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
				ptp_nikon_deletewifiprofile(&(camera->pl->params), val);
				gp_widget_set_value(child2, 0);
				deleted = 1;
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

	gp_widget_new (GP_WIDGET_SECTION, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	if (params->deviceinfo.VendorExtensionID != PTP_VENDOR_NIKON)
		return (GP_ERROR_NOT_SUPPORTED);

	if (!ptp_operation_issupported(&camera->pl->params, PTP_OC_NIKON_GetProfileAllData)) 
		return (GP_ERROR_NOT_SUPPORTED);

	ret = ptp_nikon_getwifiprofilelist(params);
	if (ret != PTP_RC_OK)
		return (GP_ERROR_NOT_SUPPORTED);

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
	int ret;
	ret = gp_widget_get_value(widget,&string);
	if (ret != GP_OK)
		return ret;
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
	char *string, *name;
	int ret;
	float val;
	char buffer[16];
	ret = gp_widget_get_value(widget,&string);
	if (ret != GP_OK)
		return ret;
	gp_widget_get_name(widget,(const char**)&name);
	gp_widget_get_value(widget, &val);

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
	int ret;
	int i;
	char buffer[16];
	ret = gp_widget_get_value(widget,&string);
	if (ret != GP_OK)
		return ret;
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
	int ret;
	int i;
	char buffer[16];
	ret = gp_widget_get_value(widget,&string);
	if (ret != GP_OK)
		return ret;
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
	int ret;
	ret = gp_widget_get_value(widget,&value);
	if (ret != GP_OK)
		return ret;
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
		if (buffer[0] != 0) { /* No DHCP */
			if (!inet_aton (buffer, &inp)) {
				fprintf(stderr,"failed to scan for addr in %s\n", buffer);
				return GP_ERROR_BAD_PARAMETERS;
			}
			profile.ip_address = inp.s_addr;
			gp_setting_get("ptp2_wifi","netmask",buffer);
			if (!inet_aton (buffer, &inp)) {
				fprintf(stderr,"failed to scan for netmask in %s\n", buffer);
				return GP_ERROR_BAD_PARAMETERS;
			}
			inp.s_addr = be32toh(inp.s_addr); /* Reverse bytes so we can use the code below. */
			profile.subnet_mask = 32;
			while (((inp.s_addr >> (32-profile.subnet_mask)) & 0x01) == 0) {
				profile.subnet_mask--;
				if (profile.subnet_mask <= 0) {
					fprintf(stderr,"Invalid subnet mask %s: no zeros\n", buffer);
					return GP_ERROR_BAD_PARAMETERS;
				}
			}
			/* Check there is only ones left */
			if ((inp.s_addr | ((0x01 << (32-profile.subnet_mask)) - 1)) != 0xFFFFFFFF) {
				fprintf(stderr,"Invalid subnet mask %s: misplaced zeros\n", buffer);
				return GP_ERROR_BAD_PARAMETERS;
			}

			/* Gateway (never tested) */
			gp_setting_get("ptp2_wifi","gw",buffer);
			if (*buffer) {
				if (!inet_aton (buffer, &inp)) {
					fprintf(stderr,"failed to scan for gw in %s\n", buffer);
					return GP_ERROR_BAD_PARAMETERS;
				}
				profile.gateway_address = inp.s_addr;
			}
		}
		else { /* DHCP */
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
				if (!*(pos+1)) {
					fprintf(stderr,"Bad key: '%s'\n", buffer);
					return GP_ERROR_BAD_PARAMETERS;	
				}
				keypart[0] = *(pos++);
				keypart[1] = *(pos++);
				profile.key[i++] = strtol(keypart, &endptr, 16);
				if (endptr != keypart+2) {
					fprintf(stderr,"Bad key: '%s', '%s' is not a number\n", buffer, keypart);
					return GP_ERROR_BAD_PARAMETERS;	
				}
				if (*pos == ':')
					pos++;
			}
			if (profile.encryption == 1) { /* WEP 64-bit */
				if (i != 5) { /* 5*8 = 40 bit + 24 bit (IV) = 64 bit */
					fprintf(stderr,"Bad key: '%s', %d bit length, should be 40 bit.\n", buffer, i*8);
					return GP_ERROR_BAD_PARAMETERS;	
				}
			}
			else if (profile.encryption == 2) { /* WEP 128-bit */
				if (i != 13) { /* 13*8 = 104 bit + 24 bit (IV) = 128 bit */
					fprintf(stderr,"Bad key: '%s', %d bit length, should be 104 bit.\n", buffer, i*8);
					return GP_ERROR_BAD_PARAMETERS;	
				}
			}
		}

		ptp_nikon_writewifiprofile(&(camera->pl->params), &profile);
	}
	return (GP_OK);
}

static struct submenu create_wifi_profile_submenu[] = {
	{ N_("Profile name"), "name", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_prop, _put_nikon_wifi_profile_prop },
	{ N_("Wifi essid"), "essid", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_prop, _put_nikon_wifi_profile_prop },
	{ N_("IP address (empty for DHCP)"), "ipaddr", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_prop, _put_nikon_wifi_profile_prop },
	{ N_("Network mask"), "netmask", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_prop, _put_nikon_wifi_profile_prop },
	{ N_("Default gateway"), "gw", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_prop, _put_nikon_wifi_profile_prop },
	{ N_("Access mode"), "accessmode", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_accessmode, _put_nikon_wifi_profile_accessmode },
	{ N_("Wifi channel"), "channel", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_channel, _put_nikon_wifi_profile_channel },
	{ N_("Encryption"), "encryption", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_encryption, _put_nikon_wifi_profile_encryption },
	{ N_("Encryption key (hex)"), "key", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_prop, _put_nikon_wifi_profile_prop },
	{ N_("Write"), "write", 0, PTP_VENDOR_NIKON, 0, _get_nikon_wifi_profile_write, _put_nikon_wifi_profile_write },
	{ 0,0,0,0,0,0,0 },
};

static int
_get_nikon_create_wifi_profile (CONFIG_GET_ARGS)
{
	int submenuno;
	CameraWidget *subwidget;
	
	gp_widget_new (GP_WIDGET_SECTION, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	for (submenuno = 0; create_wifi_profile_submenu[submenuno].name ; submenuno++ ) {
		struct submenu *cursub = create_wifi_profile_submenu+submenuno;

		cursub->getfunc (camera, &subwidget, cursub, NULL);
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
	int submenuno;

	gp_widget_new (GP_WIDGET_SECTION, _(menu->label), widget);
	gp_widget_set_name (*widget, menu->name);

	if (camera->pl->params.deviceinfo.VendorExtensionID != PTP_VENDOR_NIKON)
		return (GP_ERROR_NOT_SUPPORTED);

	for (submenuno = 0; wifi_profiles_menu[submenuno].name ; submenuno++ ) {
		struct submenu *cursub = wifi_profiles_menu+submenuno;

		cursub->getfunc (camera, &subwidget, cursub, NULL);
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

static struct submenu camera_settings_menu[] = {
	{ N_("Camera Owner"), "owner", PTP_DPC_CANON_CameraOwner, PTP_VENDOR_CANON, PTP_DTC_AUINT8, _get_AUINT8_as_CHAR_ARRAY, _put_AUINT8_as_CHAR_ARRAY },
	{ N_("Camera Model"), "model", PTP_DPC_CANON_CameraModel, PTP_VENDOR_CANON, PTP_DTC_STR, _get_STR, _put_None },
	{ N_("Firmware Version"), "firmwareversion", PTP_DPC_CANON_FirmwareVersion, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_CANON_FirmwareVersion, _put_None },
	{ N_("Camera Time"),  "time", PTP_DPC_CANON_UnixTime,     PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_UINT32_as_time, _put_UINT32_as_time },
	{ N_("Camera Time"),  "time", PTP_DPC_DateTime,           0,                PTP_DTC_STR, _get_STR_as_time, _put_STR_as_time },
	{ N_("Beep Mode"),  "beep",   PTP_DPC_CANON_BeepMode,     PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_BeepMode, _put_Canon_BeepMode },
        { N_("Image Comment"), "imgcomment", PTP_DPC_NIKON_ImageCommentString, PTP_VENDOR_NIKON, PTP_DTC_STR, _get_STR, _put_STR },
        { N_("LCD Off Time"), "lcdofftime", PTP_DPC_NIKON_MonitorOff, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_LCDOffTime, _put_Nikon_LCDOffTime },
        { N_("Meter Off Time"), "meterofftime", PTP_DPC_NIKON_MeterOff, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_MeterOffTime, _put_Nikon_MeterOffTime },
        { N_("CSM Menu"), "csmmenu", PTP_DPC_NIKON_CSMMenu, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8 },
        { N_("Battery Level"), "battery", PTP_DPC_BatteryLevel, 0, PTP_DTC_UINT8, _get_BatteryLevel, _put_None },

/* virtual */
	{ N_("Fast Filesystem"), "fastfs", 0, PTP_VENDOR_NIKON, 0, _get_Nikon_FastFS, _put_Nikon_FastFS },
	{ N_("Capture Target"), "capturetarget", 0, PTP_VENDOR_NIKON, 0, _get_CaptureTarget, _put_CaptureTarget },
	{ N_("Capture Target"), "capturetarget", 0, PTP_VENDOR_CANON, 0, _get_CaptureTarget, _put_CaptureTarget },
	{ N_("Capture"), "capture", 0, PTP_VENDOR_CANON, 0, _get_Canon_CaptureMode, _put_Canon_CaptureMode},
	{ 0,0,0,0,0,0,0 },
};

/* think of this as properties of the "film" */
static struct submenu image_settings_menu[] = {
        { N_("Image Quality"), "imgquality", PTP_DPC_CompressionSetting, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Compression, _put_Compression},
        { N_("Image Quality"), "imgquality", PTP_DPC_CANON_ImageQuality, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_Quality, _put_Canon_Quality},
        { N_("Image Size"), "imgsize", PTP_DPC_ImageSize, 0, PTP_DTC_STR, _get_ImageSize, _put_ImageSize},
        { N_("Image Size"), "imgsize", PTP_DPC_CANON_ImageSize, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_Size, _put_Canon_Size},
        { N_("ISO Speed"), "iso", PTP_DPC_CANON_ISOSpeed, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ISO, _put_Canon_ISO},
        { N_("ISO Speed"), "iso", PTP_DPC_CANON_ISOSpeed2, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ISO, _put_Canon_ISO},
	{ N_("WhiteBalance"), "whitebalance", PTP_DPC_CANON_WhiteBalance, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_WhiteBalance, _put_Canon_WhiteBalance},
	{ N_("WhiteBalance"), "whitebalance", PTP_DPC_WhiteBalance, 0, PTP_DTC_UINT16, _get_WhiteBalance, _put_WhiteBalance},
	{ N_("Photo Effect"), "photoeffect", PTP_DPC_CANON_PhotoEffect, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_PhotoEffect, _put_Canon_PhotoEffect},
	{ N_("Color Model"), "colormodel", PTP_DPC_NIKON_ColorModel, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_ColorModel, _put_Nikon_ColorModel},
	{ N_("Auto ISO"), "autoiso", PTP_DPC_NIKON_ISOAuto, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
        { 0,0,0,0,0,0,0 },
};

static struct submenu capture_settings_menu[] = {
        { N_("Long Exp Noise Reduction"), "longexpnr", PTP_DPC_NIKON_LongExposureNoiseReduction, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
        { N_("Auto Focus Mode"), "autofocusmode", PTP_DPC_NIKON_AutofocusMode, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Zoom"), "zoom", PTP_DPC_CANON_Zoom, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ZoomRange, _put_Canon_ZoomRange},
	{ N_("Assist Light"), "assistlight", PTP_DPC_CANON_AssistLight, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_AssistLight, _put_Canon_AssistLight},
	{ N_("Assist Light"), "assistlight", PTP_DPC_NIKON_AFAssist, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Exposure Compensation"), "exposurecompensation", PTP_DPC_CANON_ExpCompensation, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_ExpCompensation, _put_Canon_ExpCompensation},
	{ N_("Flash Mode"), "canonflashmode", PTP_DPC_CANON_FlashMode, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_FlashMode, _put_Canon_FlashMode},
	{ N_("Flash Mode"), "nikonflashmode", PTP_DPC_NIKON_FlashMode, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_FlashMode, _put_Nikon_FlashMode},
	{ N_("AF Area Illumination"), "af-area-illumination", PTP_DPC_NIKON_AFAreaIllumination, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_AFAreaIllum, _put_Nikon_AFAreaIllum},
	{ N_("AF Beep Mode"), "afbeep", PTP_DPC_NIKON_BeepOff, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_BeepMode, _put_Nikon_BeepMode},
        { N_("F Number"), "f-number", PTP_DPC_FNumber, 0, PTP_DTC_UINT16, _get_FNumber, _put_FNumber},
        { N_("Focal Length"), "focallength", PTP_DPC_FocalLength, 0, PTP_DTC_UINT32, _get_FocalLength, _put_FocalLength},
        { N_("Focus Mode"), "focusmode", PTP_DPC_FocusMode, 0, PTP_DTC_UINT16, _get_FocusMode, _put_FocusMode},
        { N_("ISO Speed"), "iso", PTP_DPC_ExposureIndex, 0, PTP_DTC_UINT16, _get_ISO, _put_ISO},
        { N_("Exposure Time"), "exptime", PTP_DPC_ExposureTime, 0, PTP_DTC_UINT32, _get_ExpTime, _put_ExpTime},
        { N_("Effect Mode"), "effectmode", PTP_DPC_EffectMode, 0, PTP_DTC_UINT16, _get_EffectMode, _put_EffectMode},
        { N_("Exposure Program"), "expprogram", PTP_DPC_ExposureProgramMode, 0, PTP_DTC_UINT16, _get_ExposureProgram, _put_ExposureProgram},
        { N_("Still Capture Mode"), "capturemode", PTP_DPC_StillCaptureMode, 0, PTP_DTC_UINT16, _get_CaptureMode, _put_CaptureMode},
        { N_("Canon Shooting Mode"), "shootingmode", PTP_DPC_CANON_ShootingMode, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ShootMode, _put_Canon_ShootMode},
        { N_("Focus Metering Mode"), "focusmetermode", PTP_DPC_FocusMeteringMode, 0, PTP_DTC_UINT16, _get_FocusMetering, _put_FocusMetering},
        { N_("Exposure Metering Mode"), "exposuremetermode", PTP_DPC_ExposureMeteringMode, 0, PTP_DTC_UINT16, _get_ExposureMetering, _put_ExposureMetering},
        { N_("Flash Mode"), "flashmode", PTP_DPC_FlashMode, 0, PTP_DTC_UINT16, _get_FlashMode, _put_FlashMode},
	{ N_("Aperture"), "aperture", PTP_DPC_CANON_Aperture, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_Aperture, _put_Canon_Aperture},
	{ N_("Aperture"), "aperture", PTP_DPC_CANON_Aperture2, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_Aperture, _put_Canon_Aperture},
	{ N_("Focusing Point"), "focusingpoint", PTP_DPC_CANON_FocusingPoint, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_FocusingPoint, _put_Canon_FocusingPoint},
	{ N_("Shutter Speed"), "shutterspeed", PTP_DPC_CANON_ShutterSpeed, PTP_VENDOR_CANON, PTP_DTC_UINT16, _get_Canon_ShutterSpeed, _put_Canon_ShutterSpeed},
	{ N_("Metering Mode"), "meteringmode", PTP_DPC_CANON_MeteringMode, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_MeteringMode, _put_Canon_MeteringMode},
        { N_("AF Distance"), "afdistance", PTP_DPC_CANON_AFDistance, PTP_VENDOR_CANON, PTP_DTC_UINT8, _get_Canon_AFDistance, _put_Canon_AFDistance},
	{ N_("Focus Area Wrap"), "focusareawrap", PTP_DPC_NIKON_FocusAreaWrap, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Exposure Lock"), "exposurelock", PTP_DPC_NIKON_AELockMode, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("AE-L/AF-L Mode"), "aelaflmode", PTP_DPC_NIKON_AELAFLMode, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_AELAFLMode, _put_Nikon_AELAFLMode},
	{ N_("File Number Sequencing"), "filenrsequencing", PTP_DPC_NIKON_FileNumberSequence, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Flash Sign"), "flashsign", PTP_DPC_NIKON_FlashSign, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Viewfinder Grid"), "viewfindergrid", PTP_DPC_NIKON_GridDisplay, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Image Review"), "imagereview", PTP_DPC_NIKON_ImageReview, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Image Rotation Flag"), "imagerotationflag", PTP_DPC_NIKON_ImageRotation, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Release without CF card"), "nocfcardrelease", PTP_DPC_NIKON_NoCFCard, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OnOff_UINT8, _put_Nikon_OnOff_UINT8},
	{ N_("Flash Mode Manual Power"), "flashmodemanualpower", PTP_DPC_NIKON_FlashModeManualPower, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_FlashModeManualPower, _put_Nikon_FlashModeManualPower},
	{ N_("Auto Focus Area"), "autofocusarea", PTP_DPC_NIKON_AutofocusArea, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_AutofocusArea, _put_Nikon_AutofocusArea},
	{ N_("Flash Exposure Compensation"), "flashexposurecompensation", PTP_DPC_NIKON_FlashExposureCompensation, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_FlashExposureCompensation, _put_Nikon_FlashExposureCompensation},
	{ N_("Bracket Set"), "bracketset", PTP_DPC_NIKON_BracketSet, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_BracketSet, _put_Nikon_BracketSet},
	{ N_("Bracket Order"), "bracketorder", PTP_DPC_NIKON_BracketOrder, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_BracketOrder, _put_Nikon_BracketOrder},
	{ N_("Burst Number"), "burstnumber", PTP_DPC_BurstNumber, 0, PTP_DTC_UINT16, _get_BurstNumber, _put_BurstNumber},
	{ N_("Auto Whitebalance Bias"), "autowhitebias", PTP_DPC_NIKON_WhiteBalanceAutoBias, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_WBBias, _put_Nikon_WBBias},
	{ N_("Tungsten Whitebalance Bias"), "tungstenwhitebias", PTP_DPC_NIKON_WhiteBalanceTungstenBias, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_WBBias, _put_Nikon_WBBias},
	{ N_("Fluorescent Whitebalance Bias"), "flourescentwhitebias", PTP_DPC_NIKON_WhiteBalanceFluorescentBias, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_WBBias, _put_Nikon_WBBias},
	{ N_("Daylight Whitebalance Bias"), "daylightwhitebias", PTP_DPC_NIKON_WhiteBalanceDaylightBias, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_WBBias, _put_Nikon_WBBias},
	{ N_("Flash Whitebalance Bias"), "flashwhitebias", PTP_DPC_NIKON_WhiteBalanceFlashBias, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_WBBias, _put_Nikon_WBBias},
	{ N_("Cloudy Whitebalance Bias"), "cloudywhitebias", PTP_DPC_NIKON_WhiteBalanceCloudyBias, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_WBBias, _put_Nikon_WBBias},
	{ N_("Shade Whitebalance Bias"), "shadewhitebias", PTP_DPC_NIKON_WhiteBalanceShadeBias, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_WBBias, _put_Nikon_WBBias},
        { N_("Selftimer Delay"), "selftimerdelay", PTP_DPC_NIKON_SelfTimer, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_SelfTimerDelay, _put_Nikon_SelfTimerDelay },
        { N_("Center Weight Area"), "centerweightsize", PTP_DPC_NIKON_CenterWeightArea, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_CenterWeight, _put_Nikon_CenterWeight },
        { N_("Flash Shutter Speed"), "flashshutterspeed", PTP_DPC_NIKON_FlashShutterSpeed, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_FlashShutterSpeed, _put_Nikon_FlashShutterSpeed },
        { N_("Remote Timeout"), "remotetimeout", PTP_DPC_NIKON_RemoteTimeout, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_RemoteTimeout, _put_Nikon_RemoteTimeout },
        { N_("Optimize Image"), "optimizeimage", PTP_DPC_NIKON_OptimizeImage, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_OptimizeImage, _put_Nikon_OptimizeImage },
        { N_("Sharpening"), "sharpening", PTP_DPC_NIKON_ImageSharpening, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_Sharpening, _put_Nikon_Sharpening },
        { N_("Tone Compensation"), "tonecompensation", PTP_DPC_NIKON_ToneCompensation, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_ToneCompensation, _put_Nikon_ToneCompensation },
        { N_("Saturation"), "saturation", PTP_DPC_NIKON_Saturation, PTP_VENDOR_NIKON, PTP_DTC_UINT8, _get_Nikon_Saturation, _put_Nikon_Saturation },
        { N_("Hue Adjustment"), "hueadjustment", PTP_DPC_NIKON_HueAdjustment, PTP_VENDOR_NIKON, PTP_DTC_INT8, _get_Nikon_HueAdjustment, _put_Nikon_HueAdjustment },
	/* { N_("Viewfinder Mode"), "viewfinder", PTP_DPC_CANON_ViewFinderMode, PTP_VENDOR_CANON, PTP_DTC_UINT32, _get_Canon_ViewFinderMode, _put_Canon_ViewFinderMode}, */
	{ N_("Focus Lock"), "focuslock", 0, PTP_VENDOR_CANON, 0, _get_Canon_FocusLock, _put_Canon_FocusLock},
	{ 0,0,0,0,0,0,0 },
};

static struct menu menus[] = {
	{ N_("Camera Settings"), "settings", camera_settings_menu, NULL, NULL },
	{ N_("Image Settings"), "imgsettings", image_settings_menu, NULL, NULL },
	{ N_("Capture Settings"), "capturesettings", capture_settings_menu, NULL, NULL },
	{ N_("Wifi profiles"), "wifiprofiles", NULL, _get_wifi_profiles_menu, _put_wifi_profiles_menu },
};

int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *section, *widget;
	int menuno, submenuno, ret;

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera and Driver Configuration"), window);
	gp_widget_set_name (*window, "main");
	for (menuno = 0; menuno < sizeof(menus)/sizeof(menus[0]) ; menuno++ ) {
		if (!menus[menuno].submenus) { /* Custom menu */
			struct menu *cur = menus+menuno;
			cur->getfunc(camera, &section, cur);
			gp_widget_append(*window, section);
			continue;
		}
		
		/* Standard menu with submenus */
		gp_widget_new (GP_WIDGET_SECTION, _(menus[menuno].label), &section);
		gp_widget_set_name (section, menus[menuno].name);
		gp_widget_append (*window, section);

		for (submenuno = 0; menus[menuno].submenus[submenuno].name ; submenuno++ ) {
			struct submenu *cursub = menus[menuno].submenus+submenuno;
			widget = NULL;

			if (!have_prop(camera,cursub->vendorid,cursub->propid))
				continue;
			if (cursub->propid) {
				PTPDevicePropDesc	dpd;

				memset(&dpd,0,sizeof(dpd));
				ptp_getdevicepropdesc(&camera->pl->params,cursub->propid,&dpd);
				ret = cursub->getfunc (camera, &widget, cursub, &dpd);
				ptp_free_devicepropdesc(&dpd);
			} else {
				ret = cursub->getfunc (camera, &widget, cursub, NULL);
			}
			if (ret != GP_OK)
				continue;
			gp_widget_append (section, widget);
		}
	}
#if 0
	char value[255];
	if (have_prop(camera,0,PTP_DPC_BatteryLevel)) {
		memset(&dpd,0,sizeof(dpd));
		ptp_getdevicepropdesc(&camera->pl->params,PTP_DPC_BatteryLevel,&dpd);
		GP_DEBUG ("Data Type = 0x%04x",dpd.DataType);
		GP_DEBUG ("Get/Set = 0x%02x",dpd.GetSet);
		GP_DEBUG ("Form Flag = 0x%02x",dpd.FormFlag);
		if (dpd.DataType!=PTP_DTC_UINT8) {
			ptp_free_devicepropdesc(&dpd);
			return GP_ERROR_NOT_SUPPORTED;
		}
		GP_DEBUG ("Factory Default Value = %0.2x",dpd.FactoryDefaultValue.u8);
		GP_DEBUG ("Current Value = %0.2x",dpd.CurrentValue.u8);

		gp_widget_new (GP_WIDGET_SECTION, _("Power (readonly)"), &section);
		gp_widget_append (*window, section);
		switch (dpd.FormFlag) {
		case PTP_DPFF_Enumeration: {
			uint16_t i;
			char tmp[64];

			GP_DEBUG ("Number of values %i",
				dpd.FORM.Enum.NumberOfValues);
			gp_widget_new (GP_WIDGET_TEXT, _("Number of values"),&widget);
			snprintf (value,255,"%i",dpd.FORM.Enum.NumberOfValues);
			gp_widget_set_value (widget,value);
			gp_widget_append (section,widget);
			gp_widget_new (GP_WIDGET_TEXT, _("Supported values"),&widget);
			value[0]='\0';
			for (i=0;i<dpd.FORM.Enum.NumberOfValues;i++){
				snprintf (tmp,6,"|%.3i|",dpd.FORM.Enum.SupportedValue[i].u8);
				strncat(value,tmp,6);
			}
			gp_widget_set_value (widget,value);
			gp_widget_append (section,widget);
			gp_widget_new (GP_WIDGET_TEXT, _("Current value"),&widget);
			snprintf (value,255,"%i",dpd.CurrentValue.u8);
			gp_widget_set_value (widget,value);
			gp_widget_append (section,widget);
			break;
		}
		case PTP_DPFF_Range: {
			float value_float;
			gp_widget_new (GP_WIDGET_RANGE, _("Power (readonly)"), &widget);
			gp_widget_append (section,widget);
			gp_widget_set_range (widget, dpd.FORM.Range.MinimumValue.u8, dpd.FORM.Range.MaximumValue.u8, dpd.FORM.Range.StepSize.u8);
			/* this is a write only capability */
			value_float = dpd.CurrentValue.u8;
			gp_widget_set_value (widget, &value_float);
			gp_widget_changed(widget);
			break;
		}
		case PTP_DPFF_None:
			break;
		default: break;
		}
	}
#endif
	return GP_OK;
}

int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *section, *widget, *subwindow;
	int menuno, submenuno, ret;

	ret = gp_widget_get_child_by_label (window, _("Camera and Driver Configuration"), &subwindow);
	if (ret != GP_OK)
		return ret;
	for (menuno = 0; menuno < sizeof(menus)/sizeof(menus[0]) ; menuno++ ) {
		ret = gp_widget_get_child_by_label (subwindow, _(menus[menuno].label), &section);
		if (ret != GP_OK)
			continue;

		if (!menus[menuno].submenus) { /* Custom menu */
			struct menu *cur = menus+menuno;
			cur->putfunc(camera, section);
			continue;
		}
		
		/* Standard menu with submenus */

		for (submenuno = 0; menus[menuno].submenus[submenuno].label ; submenuno++ ) {
			PTPPropertyValue	propval;

			struct submenu *cursub = menus[menuno].submenus+submenuno;
			if (!have_prop(camera,cursub->vendorid,cursub->propid))
				continue;

			ret = gp_widget_get_child_by_label (section, _(cursub->label), &widget);
			if (ret != GP_OK)
				continue;

			if (!gp_widget_changed (widget))
				continue;

			if (cursub->propid) {
				PTPDevicePropDesc dpd;

				memset(&dpd,0,sizeof(dpd));
				ptp_getdevicepropdesc(&camera->pl->params,cursub->propid,&dpd);
				ret = cursub->putfunc (camera, widget, &propval, &dpd);
				if (ret == GP_OK)
					ptp_setdevicepropvalue (&camera->pl->params, cursub->propid, &propval, cursub->type);
				ptp_free_devicepropvalue (cursub->type, &propval);
				ptp_free_devicepropdesc(&dpd);
			} else {
				ret = cursub->putfunc (camera, widget, NULL, NULL);
			}
		}
	}
	return GP_OK;
}

