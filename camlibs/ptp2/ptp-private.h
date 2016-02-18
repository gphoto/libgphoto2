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


struct _CameraPrivateLibrary {
	PTPParams params;
	int checkevents;
};

struct _PTPData {
	Camera *camera;
	GPContext *context;
};
typedef struct _PTPData PTPData;
