/* olympus_desc.c:
 *
 * Olympus-specific code: David Selmeczi <david@esr.elte.hu>
 * Original code:
 *  Copyright © 2002 Patrick Mansfield <patman@aracnet.com>
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
#include "config.h"

#include <stdio.h>
#include <_stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2-library.h>
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
 *	{ range: { 100.1, 2000.0, 10 } }, NULL 
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
 * TOCHEK.
 */
/*#ifdef RANGE_FOR_SHUTTER
static const ValueNameType ep3000z_reg_03_val_names[] = {
	{
		{ range: { 0, 16000000, 1000 } }, NULL
	}
}; 
static const RegisterDescriptorType ep3000z_reg_03[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"shutter", N_("Shutter Speed microseconds (0 auto)"),
		VAL_NAME_INIT (ep3000z_reg_03_val_names)
	}
};
#else
static const ValueNameType ep3000z_reg_03_val_names[] = { 
	{ {       0 }, N_("Auto") },
	{ {    1250 }, "1/800" },
	{ {    2000 }, "1/500" },
	{ {    2500 }, "1/400" },
	{ {    4000 }, "1/250" },
	{ {    5000 }, "1/200" },
	{ {    6250 }, "1/160" },
	{ {    8000 }, "1/125" },
	{ {   10000 }, "1/100" },
	{ {   12500 }, "1/80" },
	{ {   16666 }, "1/60" },
	{ {   20000 }, "1/50" },
	{ {   33333 }, "1/30" },
	{ {   40000 }, "1/25" },
	{ {   66667 }, "1/15" },
	{ {  100000 }, "1/10" },
	{ {  125000 }, "1/8" },
	{ {  200000 }, "1/4" },
	{ {  500000 }, "1/2" },
	{ { 1000000 }, "1" },
	{ { 2000000 }, "2" },
	{ { 3000000 }, "3" },
	{ { 4000000 }, "4" },
	{ { 5000000 }, "5" },
	{ { 6000000 }, "6" },
	{ { 8000000 }, "8" },
	{ {10000000 }, "10" },
	{ {16000000 }, "16" },

};
static const RegisterDescriptorType ep3000z_reg_03[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"shutter", N_("Shutter Speed (in seconds)"),
		VAL_NAME_INIT (ep3000z_reg_03_val_names)
	}
};
#endif
*/

/*
 * Register 5: aperature settings. 
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
		"aperature", N_("Aperature Settings"),
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
	{ { range: { 30, 600, 30  } }, NULL },
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
	{ { range: { 30, 600, 30  } }, NULL },
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
 * Register 34: LCD mode / write-only through SIERRA_SUBACTION / ACTION_LCD_MODE
 * This register always contains 1 (OFF).
 * Note: Always swicth off LCD before disconnecting the camera. Otherwise the camera
 * controls won't work, and you will need to replug the cable and switch off the LCD.
 * (This _is_ a documented "feature".)
 * TOCHECK: was not implemented for ep3000z.
 */
/*
static const ValueNameType ep3000z_reg_34_val_names[] = {
	{ { 0x01 }, N_("Off") },
	{ { 0x02 }, N_("Monitor") },
	{ { 0x03 }, N_("Normal") },
};
static const RegisterDescriptorType ep3000z_reg_34[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK, 
		"lcd-mode", N_("LCD Mode"),
		VAL_NAME_INIT (ep3000z_reg_34_val_names)
	}
};
*/

/*
 * Register 35: lcd brightness.
 * TOCHECK: was not implemented on ep3000z
 */
/*static const ValueNameType ep3000z_reg_35_val_names[] = {
	{ { range: { 0, 7 } }, NULL },
};
static const RegisterDescriptorType ep3000z_reg_35[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"lcd-brightness", N_("LCD Brightness"),
		VAL_NAME_INIT (ep3000z_reg_35_val_names)
	}
};*/

/*
 * Register 38: lcd auto shut off time
 * TOCHECK: was not implemented for ep3000z
 */
/*static const ValueNameType ep3000z_reg_38_val_names[] = {
	{ { range: { 30, 600, 30  } }, NULL },
};
static const RegisterDescriptorType ep3000z_reg_38[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"lcd-auto-shutoff", N_("LCD Auto Shut Off (seconds)"),
		VAL_NAME_INIT (ep3000z_reg_38_val_names)
	}
};*/

/*
 * Register 41: time format, read only
 * TOCHECK: was not implemented on ep3000z
 */
/*
static const ValueNameType ep3000z_reg_41_val_names[] = {
	{ { 0 }, N("Off") },
	{ { 1 }, "yymmdd" },
	{ { 2 }, "mmddyy" },
	{ { 3 }, "ddmmyy" },
};
static const RegisterDescriptorType ep3000z_reg_41[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"timefmt", N_("Time format"),
		VAL_NAME_INIT (ep3000z_reg_41_val_names)
	}
};
*/

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
	{ { 0x09 }, N_("Portugese") },

};
static const RegisterDescriptorType ep3000z_reg_53[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"language", N_("Language"),
		VAL_NAME_INIT (ep3000z_reg_53_val_names)
	}
};


/*
 * Register 69: exposure compensation.
 * TOCHEC: was not implemented on ep3000z
 */
/*static const ValueNameType ep3000z_reg_69_val_names[] = {
	{ { -20 }, "-2.0" },
	{ { -17 }, "-1.7" },
	{ { -13 }, "-1.3" },
	{ { -10 }, "-1.0" },
	{ { -07 }, "-0.7" },
	{ { -03 }, "-0.3" },
	{ {  00 }, " 0" },
	{ { +03 }, "+0.3" },
	{ { +07 }, "+0.7" },
	{ { +10 }, "+1.0" },
	{ { +13 }, "+1.3" },
	{ { +17 }, "+1.7" },
	{ { +20 }, "+2.0" },
};
static const RegisterDescriptorType ep3000z_reg_69[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"exp", N_("Exposure Compensation"),
		VAL_NAME_INIT (ep3000z_reg_69_val_names)
	}
};
*/

/*
 * Register 70: exposure meter.
 * TOCHECK: was not implemented on ep3000z
 */
/*static const ValueNameType ep3000z_reg_70_val_names[] = {
	{ { 0x03 }, N_("Spot") },
	{ { 0x05 }, N_("Matrix") },
};
static const RegisterDescriptorType ep3000z_reg_70[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"exp-meter", N_("Exposure Metering"), 
		VAL_NAME_INIT (ep3000z_reg_70_val_names)
	}
};*/

/*
 * Register 71: optical zoom value.
 * TOCHECK: was not implemented on ep3000z
 */
/*
static const ValueNameType ep3000z_reg_71_val_names[] = {
	{
		{ range: { 7.3, 21.0, .1 } }, NULL
	}
};
static const RegisterDescriptorType ep3000z_reg_71[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK, 
		"zoom", N_("Zoom (in millimeters)"),
		VAL_NAME_INIT (ep3000z_reg_71_val_names)
	}
};
*/

/*
 * Register 72: digital zoom
 * TOCHECK: was not implemented on ep3000z
 */
/*
static const ValueNameType ep3000z_reg_72_val_names[] = {
	{ { 0x0000 }, N_("Off") },
	{ { 0x0208 }, "1.5x" },
	{ { 0x0008 }, "2x" },
	{ { 0x0408 }, "2.5x" },
};
static const RegisterDescriptorType ep3000z_reg_72[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"dzoom", N_("Digital zoom"),
		VAL_NAME_INIT (ep3000z_reg_72_val_names)
	}
};
*/

/*
 * Register 85: ISO speed, read only
 * TOCHECK: was not implemented on ep3000z
 */
/*
static const ValueNameType ep3000z_reg_85_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, "100" },
	{ { 2 }, "200" },
	{ { 3 }, "400" },
};
static const RegisterDescriptorType ep3000z_reg_85[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"iso", N_("ISO Speed"),
		VAL_NAME_INIT (ep3000z_reg_85_val_names)
	}
};
*/

/*
 * Register 103: focus position (only applicable in manual focus mode)
 *     1 to 120: macro positions
 *   121 to 240: normal positions
 * TOCHECK: was not implemented on ep3000z
 */
/*
static const ValueNameType ep3000z_reg_103_val_names[] = {
	{ { range: { 1, 240, 1 } }, NULL },
};
static const RegisterDescriptorType ep3000z_reg_103[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK, 
		"focus-pos", N_("Focus position"),
		VAL_NAME_INIT (ep3000z_reg_103_val_names)
	}
};
*/

/*
 * All of the register used to modify picture settings. The register value
 * received from the camera is stored into this data area, so it cannot be
 * a const.
 */
static CameraRegisterType ep3000z_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (ep3000z, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	//CAM_REG_TYPE_INIT (ep3000z, 03, 4, CAM_DESC_DEFAULT, 0), /* shutter */
	CAM_REG_TYPE_INIT (ep3000z, 05, 4, CAM_DESC_DEFAULT, 0), /* aperature (f-stop) */
	CAM_REG_TYPE_INIT (ep3000z, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (ep3000z, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (ep3000z, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
	CAM_REG_TYPE_INIT (ep3000z, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
	//CAM_REG_TYPE_INIT (ep3000z,103, 4, CAM_DESC_DEFAULT, 0), /* focus position */
	//CAM_REG_TYPE_INIT (ep3000z, 69, 8, CAM_DESC_DEFAULT, 0), /* exposure compensation */
	//CAM_REG_TYPE_INIT (ep3000z, 70, 4, CAM_DESC_DEFAULT, 0), /* exposure metering */
	//CAM_REG_TYPE_INIT (ep3000z, 71, 8, CAM_DESC_DEFAULT, 0), /* optical zoom */
	//CAM_REG_TYPE_INIT (ep3000z, 72, 4, CAM_DESC_DEFAULT, 0), /* digital zoom + lense +  AE lock */
	//CAM_REG_TYPE_INIT (ep3000z, 85, 4, CAM_DESC_DEFAULT, 0), /* ISO Speed, read only  */
};

/*
 * All of the register used to modify camera settings.
 */
static const CameraRegisterType ep3000z_cam_regs[] = {
	CAM_REG_TYPE_INIT (ep3000z, 53, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (ep3000z, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	//CAM_REG_TYPE_INIT (ep3000z, 34, 4, CAM_DESC_SUBACTION, 
	//		  SIERRA_ACTION_LCD_MODE), /* lcd mode */
	//CAM_REG_TYPE_INIT (ep3000z, 35, 4, CAM_DESC_DEFAULT, 0), /* LCD brightness */
	//CAM_REG_TYPE_INIT (ep3000z, 38, 4, CAM_DESC_DEFAULT, 0), /* LCD auto shutoff */
	CAM_REG_TYPE_INIT (ep3000z, 24, 4, CAM_DESC_DEFAULT, 0), /* Camera power save */
	CAM_REG_TYPE_INIT (ep3000z, 23, 4, CAM_DESC_DEFAULT, 0), /* Host power save */
	//CAM_REG_TYPE_INIT (ep3000z, 41, 4, CAM_DESC_DEFAULT, 0), /* time format, read only */
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
