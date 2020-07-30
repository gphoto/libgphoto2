/* olympus_desc.c:
 *
 * Olympus-specific code: David Selmeczi <david@esr.elte.hu>
 * Original code:
 *  Copyright 2002 Patrick Mansfield <patman@aracnet.com>
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
#include <_stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2/gphoto2-library.h>
#include "sierra.h"
#include "library.h"
#include "sierra-desc.h"

/*
 * Camera descriptor for Olympus C-3040Z (and possibly others).
 */

/*
 * Notes:
 *
 * The variable "value" in a ValueNameType is the first element of
 * the name/value union, and so it is the default value initialized.
 *
 * To init to a range, use:
 *	{ .range = { 100.1, 2000.0, 10 } }, NULL
 *
 * Casts are used to avoid warnings about using a const.
 *
 * Only registers that are used by the camera configuration are listed
 * here (for use in the get/set config functions); no settings, capture or
 * other data is stored here.
 */

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

/*
 * Register 1: resolution/size.
 */
static const ValueNameType ep3000z_reg_01_val_names[] = {
	{ { 0x01 }, "Standard 640x480" },
	{ { 0x02 }, "Fine 2048x1536" },
	{ { 0x03 }, "SuperFine 2048x1536" },
	{ { 0x04 }, "HyPict 2544x1904" },
};
static const RegisterDescriptorType ep3000z_reg_01[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"resolution", N_("Resolution plus Size"),
		VAL_NAME_INIT (ep3000z_reg_01_val_names)
	}
};

/*
 * Register 2: Date and time.
 */
static const ValueNameType ep3000z_reg_02_val_names[] = {
	/*
	 * Dummy value, since we need at least one of these to
	 * display anything.
	 */
	{ { 0x00 }, "Dummy" },
};
static const RegisterDescriptorType ep3000z_reg_02[] = {
	{
		GP_WIDGET_DATE, GP_REG_NO_MASK,
		"date-time", N_("Date and time (GMT)"),
		VAL_NAME_INIT (ep3000z_reg_02_val_names)
	}
};

/*
 * Register 3: shutter speed, not on ep3000z
 */

/*
 * Register 5: aperture settings.
 */
static const ValueNameType ep3000z_reg_05_val_names[] = {
	{ { 6 }, N_("auto") },
	{ { 0 }, "F2.0" },
	{ { 1 }, "F2.3" },
	{ { 2 }, "F2.8" },
	{ { 3 }, "F4.0" },
	{ { 4 }, "F5.6" },
	{ { 5 }, "F8.0" },
};
static const RegisterDescriptorType ep3000z_reg_05[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"aperture", N_("Aperture Settings"),
		VAL_NAME_INIT (ep3000z_reg_05_val_names)
	}
};

/*
 * Register 6: color mode
 */
static const ValueNameType ep3000z_reg_06_val_names[] = {
	{ { 1 }, N_("Color") },
	{ { 2 }, N_("Black & White") },
};
static const RegisterDescriptorType ep3000z_reg_06[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"color", N_("Color Mode"),
		VAL_NAME_INIT (ep3000z_reg_06_val_names)
	}
};

/*
 * Register 7: flash settings
 */
static const ValueNameType ep3000z_reg_07_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, N_("Force") },
	{ { 2 }, N_("Off") },
	{ { 3 }, N_("Red-eye Reduction") },
	{ { 4 }, N_("Slow Sync") },
};
static const RegisterDescriptorType ep3000z_reg_07[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"flash", N_("Flash Settings"),
		VAL_NAME_INIT (ep3000z_reg_07_val_names)
	}
};

/*
 * Register 20: white balance.
 */
static const ValueNameType ep3000z_reg_20_val_names[] = {
	{ { 0x00 }, N_("Auto") },
	{ { 0x01 }, N_("Fixed") },
	{ { 0xFF }, N_("Custom") },
};
static const RegisterDescriptorType ep3000z_reg_20[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"whitebalance", N_("White Balance"),
		VAL_NAME_INIT (ep3000z_reg_20_val_names)
	}
};

/*
 * Register 23: camera power save when connected to PC
 * (idle time before entering power save mode)
 * TOCHECK: what is the exact valid range?
 * I have previously set 0,255,1 is old camera_get_config_epson()
 */
static const ValueNameType ep3000z_reg_23_val_names[] = {
	{ { .range = { 30, 600, 30  } }, NULL },
};
static const RegisterDescriptorType ep3000z_reg_23[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"host-power-save", N_("Host power save (seconds)"),
		VAL_NAME_INIT (ep3000z_reg_23_val_names)
	}
};

/*
 * Register 24: camera power save (idle time before entering power save mode)
 * TOCHECK: what is the exact valid range?
 * I have previously set 0,255,1 is old camera_get_config_epson()
 */
static const ValueNameType ep3000z_reg_24_val_names[] = {
	{ { .range = { 30, 600, 30  } }, NULL },
};
static const RegisterDescriptorType ep3000z_reg_24[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"camera-power-save", N_("Camera power save (seconds)"),
		VAL_NAME_INIT (ep3000z_reg_24_val_names)
	}
};

/*
 * Register 33: Lens mode.
 */
static const ValueNameType ep3000z_reg_33_val_names[] = {
	{ { 0x01 }, N_("Macro") },
	{ { 0x02 }, N_("Normal") },
};
static const RegisterDescriptorType ep3000z_reg_33[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"lens-mode", N_("Lens Mode"),
		VAL_NAME_INIT (ep3000z_reg_33_val_names)
	}
};

/*
 * Register 53: language setting.
 */
static const ValueNameType ep3000z_reg_53_val_names[] = {
	{ { 0x01 }, N_("Korean") },
	{ { 0x03 }, N_("English") },
	{ { 0x04 }, N_("French") },
	{ { 0x05 }, N_("German") },
	{ { 0x06 }, N_("Italian") },
	{ { 0x07 }, N_("Japanese") },
	{ { 0x08 }, N_("Spanish") },
	{ { 0x09 }, N_("Portuguese") },

};
static const RegisterDescriptorType ep3000z_reg_53[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"language", N_("Language"),
		VAL_NAME_INIT (ep3000z_reg_53_val_names)
	}
};

/*
 * All of the register used to modify picture settings. The register value
 * received from the camera is stored into this data area, so it cannot be
 * a const.
 */
static CameraRegisterType ep3000z_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (ep3000z, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (ep3000z, 05, 4, CAM_DESC_DEFAULT, 0), /* aperture (f-stop) */
	CAM_REG_TYPE_INIT (ep3000z, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (ep3000z, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (ep3000z, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
	CAM_REG_TYPE_INIT (ep3000z, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
};

/*
 * All of the register used to modify camera settings.
 */
static CameraRegisterType ep3000z_cam_regs[] = {
	CAM_REG_TYPE_INIT (ep3000z, 53, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (ep3000z, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (ep3000z, 24, 4, CAM_DESC_DEFAULT, 0), /* Camera power save */
	CAM_REG_TYPE_INIT (ep3000z, 23, 4, CAM_DESC_DEFAULT, 0), /* Host power save */
};

static const CameraRegisterSetType ep3000z_desc[] = {
		{
			N_("Picture Settings"),
			SIZE_ADDR (CameraRegisterType, ep3000z_pic_regs)
		},
		{
			N_("Camera Settings"),
			SIZE_ADDR (CameraRegisterType, ep3000z_cam_regs)
		},
};

static const char ep3000z_manual[] =
N_(
"Some notes about Epson cameras:\n"
"- Some parameters are not controllable remotely:\n"
"  * zoom\n"
"  * focus\n"
"  * custom white balance setup\n"
"- Configuration has been reverse-engineered with\n"
"  a PhotoPC 3000z, if your camera acts differently\n"
"  please send a mail to the gphoto developer mailing list (in English)\n"
);

const CameraDescType ep3000z_cam_desc = { ep3000z_desc, ep3000z_manual,
	SIERRA_EXT_PROTO, };
