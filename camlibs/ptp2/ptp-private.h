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
int camera_set_config (Camera *camera, CameraWidget *window, GPContext *context);
int camera_prepare_capture (Camera *camera, GPContext *context);
int camera_unprepare_capture (Camera *camera, GPContext *context);
int camera_canon_eos_update_capture_target(Camera *camera, GPContext *context, int value);

/* library.c */
int translate_ptp_result (uint16_t result);
void fixup_cached_deviceinfo (Camera *camera, PTPDeviceInfo*);

#define C_PTP_REP(RESULT) do {\
	uint16_t ret = (RESULT);\
	if (ret != PTP_RC_OK) {\
		const char* ptp_err_str = ptp_strerror(ret, params->deviceinfo.VendorExtensionID);\
		gp_log (GP_LOG_ERROR, __func__, "'%s' failed: '%s' (0x%x)", #RESULT, ptp_err_str, ret);\
		gp_context_error (context, "%s", dgettext(GETTEXT_PACKAGE, ptp_err_str));\
		return translate_ptp_result (ret);\
	}\
} while(0)

#define CR(result) do { int r=(result); if(r<0) return r; } while (0)

struct _CameraPrivateLibrary {
	PTPParams params;
	int checkevents;
};

struct _PTPData {
	Camera *camera;
	GPContext *context;
};
typedef struct _PTPData PTPData;
