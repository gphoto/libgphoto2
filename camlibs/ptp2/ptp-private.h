/* ptp-private.h
 *
 * Copyright (C) 2011 Marcus Meissner <marcus@jet.franken.de>
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

/* ptp2 camlib private functions */
#include <gphoto2/gphoto2-library.h>

#include <string.h>

/* config.c */
int camera_get_config (Camera *camera, CameraWidget **window, GPContext *context);
int camera_get_config_list (Camera *camera, CameraList *list, GPContext *context);
int camera_get_single_config (Camera *camera, const char *confname, CameraWidget **window, GPContext *context);
int camera_set_config (Camera *camera, CameraWidget *window, GPContext *context);
int camera_set_single_config (Camera *camera, const char *confname, CameraWidget *window, GPContext *context);
int camera_list_config (Camera *camera, CameraList *list, GPContext *context);
int camera_prepare_capture (Camera *camera, GPContext *context);
int camera_unprepare_capture (Camera *camera, GPContext *context);
int camera_canon_eos_update_capture_target(Camera *camera, GPContext *context, int value);
int have_prop(Camera *camera, uint16_t vendor, uint16_t prop);


/* library.c */
int translate_ptp_result (uint16_t result);
uint16_t translate_gp_result_to_ptp (int gp_result);
int fixup_cached_deviceinfo (Camera *camera, PTPDeviceInfo*);

int chdk_init(Camera*,GPContext*);
uint16_t ptp_init_camerafile_handler (PTPDataHandler *handler, CameraFile *file);
uint16_t ptp_exit_camerafile_handler (PTPDataHandler *handler);



inline static int log_on_ptp_error_helper( int _r, const char* _func, const char* file, int line, const char* func, int vendor ) {
	if (_r != PTP_RC_OK) {
		const char* ptp_err_str = ptp_strerror(_r, vendor);
		gp_log_with_source_location(GP_LOG_ERROR, file, line, func,
					    "'%s' failed: %s (0x%04x)", _func, ptp_err_str, _r);
	}
	return _r;
}
#define LOG_ON_PTP_E( RESULT ) \
	log_on_ptp_error_helper( (RESULT), #RESULT, __FILE__, __LINE__, __func__,\
		params->deviceinfo.VendorExtensionID )

#define C_PTP(RESULT) do {\
	uint16_t c_ptp_ret = (RESULT);\
	if (c_ptp_ret != PTP_RC_OK) {\
		const char* ptp_err_str = ptp_strerror(c_ptp_ret, params->deviceinfo.VendorExtensionID);\
		GP_LOG_E ("'%s' failed: %s (0x%04x)", #RESULT, ptp_err_str, c_ptp_ret);\
		return translate_ptp_result (c_ptp_ret);\
	}\
} while(0)

#define C_PTP_MSG(RESULT, MSG, ...) do {\
	uint16_t c_ptp_msg_ret = (RESULT);\
	if (c_ptp_msg_ret != PTP_RC_OK) {\
		const char* ptp_err_str = ptp_strerror(c_ptp_msg_ret, params->deviceinfo.VendorExtensionID);\
		char fmt_str[256];\
		snprintf(fmt_str, sizeof(fmt_str), "%s%s%s", "'%s' failed: ", MSG, " (0x%04x: %s)");\
		GP_LOG_E (fmt_str, #RESULT, ##__VA_ARGS__, c_ptp_msg_ret, ptp_err_str);\
		return translate_ptp_result (c_ptp_msg_ret);\
	}\
} while(0)

#define C_PTP_REP(RESULT) do {\
	uint16_t c_ptp_rep_ret = (RESULT);\
	if (c_ptp_rep_ret != PTP_RC_OK) {\
		const char* ptp_err_str = ptp_strerror(c_ptp_rep_ret, params->deviceinfo.VendorExtensionID);\
		GP_LOG_E ("'%s' failed: '%s' (0x%04x)", #RESULT, ptp_err_str, c_ptp_rep_ret);\
		gp_context_error (context, "%s", dgettext(GETTEXT_PACKAGE, ptp_err_str));\
		return translate_ptp_result (c_ptp_rep_ret);\
	}\
} while(0)

#define C_PTP_REP_MSG(RESULT, MSG, ...) do {\
	uint16_t c_ptp_rep_msg_ret = (RESULT);\
	if (c_ptp_rep_msg_ret != PTP_RC_OK) {\
		const char* ptp_err_str = ptp_strerror(c_ptp_rep_msg_ret, params->deviceinfo.VendorExtensionID);\
		char fmt_str[256];\
		snprintf(fmt_str, sizeof(fmt_str), "%s%s%s", "'%s' failed: ", MSG, " (0x%04x: %s)");\
		GP_LOG_E (fmt_str, #RESULT, ##__VA_ARGS__, c_ptp_rep_msg_ret, ptp_err_str);\
		snprintf(fmt_str, sizeof(fmt_str), "%s%s", MSG, " (0x%04x: %s)");\
		gp_context_error (context, fmt_str, ##__VA_ARGS__, c_ptp_rep_msg_ret, dgettext(GETTEXT_PACKAGE, ptp_err_str));\
		return translate_ptp_result (c_ptp_rep_msg_ret);\
	}\
} while(0)

#define CR(RESULT) do {\
	int cr_r=(RESULT);\
	if (cr_r<0) {\
		GP_LOG_E ("'%s' failed: '%s' (%d)", #RESULT, gp_port_result_as_string(cr_r), cr_r);\
		return cr_r;\
	}\
} while (0)

static inline int
is_canon_eos_m(PTPParams *params) {
	if (params->deviceinfo.VendorExtensionID != PTP_VENDOR_CANON) return 0;
	if (!ptp_operation_issupported(params, PTP_OC_CANON_EOS_SetRemoteMode)) return 0;
	if (!params->deviceinfo.Model) return 0;

	/* classic EOS M */
	if (!strncmp(params->deviceinfo.Model, "Canon EOS M", strlen("Canon EOS M")))
		return 1;

	/* We encountered newer Powershot SX models that seem to have EOS M like firmware, see https://github.com/gphoto/libgphoto2/issues/316 */
	if (	!strncmp(params->deviceinfo.Model, "Canon PowerShot SX", strlen("Canon PowerShot SX"))	||
		!strncmp(params->deviceinfo.Model, "Canon PowerShot G",  strlen("Canon PowerShot G"))	||
		!strncmp(params->deviceinfo.Model, "Canon Digital IXUS", strlen("Canon Digital IXUS"))
	)
		return ptp_operation_issupported(params, PTP_OC_CANON_EOS_RemoteReleaseOn);
	return 0;
}

static inline int
have_eos_prop(PTPParams *params, uint16_t vendor, uint16_t prop) {
	unsigned int i;

	/* The special Canon EOS property set gets special treatment. */
	if ((params->deviceinfo.VendorExtensionID != PTP_VENDOR_CANON) || (vendor != PTP_VENDOR_CANON))
		return 0;
	for (i=0;i<params->nrofcanon_props;i++)
		if (params->canon_props[i].proptype == prop)
			return 1;
	return 0;
}

struct _CameraPrivateLibrary {
	PTPParams params;
	int checkevents;
};

struct _PTPData {
	Camera *camera;
	GPContext *context;
};
typedef struct _PTPData PTPData;
