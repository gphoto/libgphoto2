/* olympus_desc.c:
 *
 * Olympus-specific code: David Selmeczi <david@esr.elte.hu>
 * Original code:
 *  Copyright (C) 2002 Patrick Mansfield <patman@aracnet.com>
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
#include <stdio.h>
#include <sys/types.h>
#include <malloc.h>
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
#  define _(String) dgettext (PACKAGE, String)
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
static const ValueNameType oly3040_reg_01_val_names[] = {
	/* 
	 * These values suck. why didn't they make these maskable, or use
	 * a nibble for each part?  Just use a the combined resolution + size.
	 * It would be good to try and break these into two selections.
	 */
	{ { 0x01 }, "640x480-Normal" },
	{ { 0x04 }, "640x480-Fine" },
	{ { 0x05 }, "1024x768-Normal" },
	{ { 0x06 }, "1024x768-Fine" },
	{ { 0x07 }, "1280x960-Normal" },
	{ { 0x08 }, "1280x960-Fine" },
	{ { 0x09 }, "1600x1200-Normal" },
	{ { 0x0a }, "1600x1200-Fine" },
/* Only for E-100RS
	{ { 0x0b }, "1360x1024-Normal" },
	{ { 0x0c }, "1360x1024-Fine" },
*/
	{ { 0x02 }, "2048x1536-Normal" },
	{ { 0x03 }, "2048x1536-Fine" },
	{ { 0x21 }, "640x480-TIFF" },
	{ { 0x22 }, "1024x768-TIFF" },
	{ { 0x23 }, "1280x960-TIFF" },
	{ { 0x24 }, "1600x1200-TIFF" },
	{ { 0x25 }, "2048x1536-TIFF" },
	{ { 0x26 }, "1360x1024-TIFF" },
};
static const RegisterDescriptorType oly3040_reg_01[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK, 
		"resolution", N_("Resolution plus Size"),
		VAL_NAME_INIT (oly3040_reg_01_val_names)
	}
};

/*
 * Register 2: Date and time.
 */
static const ValueNameType oly3040_reg_02_val_names[] = {
	/* 
	 * Dummy value, since we need at least one of these to
	 * display anything.
	 */
	{ { 0x00 }, "Dummy" },
};
static const RegisterDescriptorType oly3040_reg_02[] = { 
	{
		GP_WIDGET_DATE, GP_REG_NO_MASK, 
		"date-time", N_("Date and time (GMT)"),
		VAL_NAME_INIT (oly3040_reg_02_val_names)
	}
};

/*
 * Register 3: shutter speed, camera must be in S or M mode.
 * In S mode, aperture is compensated
 * Possibly could be any other value in the range of 1250 and 16M
 */
#ifdef RANGE_FOR_SHUTTER
static const ValueNameType oly3040_reg_03_val_names[] = { 
	{
		{ range: { 0, 16000000, 1000 } }, NULL 
	}
};
static const RegisterDescriptorType oly3040_reg_03[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK, 
		"shutter", N_("Shutter Speed microseconds (0 auto)"),
		VAL_NAME_INIT (oly3040_reg_03_val_names)
	}
};
#else
static const ValueNameType oly3040_reg_03_val_names[] = { 
	{ {       0 }, "Auto" },
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
static const RegisterDescriptorType oly3040_reg_03[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"shutter", N_("Shutter Speed (in seconds)"),
		VAL_NAME_INIT (oly3040_reg_03_val_names)
	}
};
#endif

/*
 * Register 5: aperature settings. Works only in A or M mode
 * In A mode, shutter speed is compensated
 */
static const ValueNameType oly3040_reg_05_val_names[] = {
	{ { 0 }, "Auto" },
	{ { 1 }, "F1.8" },
	{ { 2 }, "F2.0" },
	{ { 3 }, "F2.3" },
	{ { 4 }, "F2.6" },
	{ { 5 }, "F2.8" },
	{ { 6 }, "F3.2" },
	{ { 7 }, "F3.6" },
	{ { 8 }, "F4.0" },
	{ { 9 }, "F4.5" },
	{ { 10 }, "F5.0" },
	{ { 11 }, "F5.6" },
	{ { 12 }, "F6.3" },
	{ { 13 }, "F7" },
	{ { 14 }, "F8" },
	{ { 15 }, "F9" },
	{ { 16 }, "F10" },
};
static const RegisterDescriptorType oly3040_reg_05[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"aperature", N_("Aperature Settings"),
		VAL_NAME_INIT (oly3040_reg_05_val_names)
	}
};

/*
 * Register 6: color mode
 */
static const ValueNameType oly3040_reg_06_val_names[] = {
	{ { 0 }, "Normal" },
	{ { 1 }, "B/W" },
	{ { 2 }, "Sepia" },
	{ { 3 }, "White board" },
	{ { 4 }, "Black board" },
};
static const RegisterDescriptorType oly3040_reg_06[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK, 
		"color", N_("Color Mode"),
		VAL_NAME_INIT (oly3040_reg_06_val_names)
	}
};

/*
 * Register 7: flash settings
 */
static const ValueNameType oly3040_reg_07_val_names[] = {
	{ { 0 }, "Auto" },
	{ { 1 }, "Force" },
	{ { 2 }, "Off" },
	{ { 3 }, "Anti-redeye" },
};
static const RegisterDescriptorType oly3040_reg_07[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"flash", N_("Flash Settings"), 
		VAL_NAME_INIT (oly3040_reg_07_val_names)
	}
};

/*
 * Register 20: white balance.
 */
static const ValueNameType oly3040_reg_20_val_names[] = {
	{ { 0x00 }, "Auto" },
	{ { 0x01 }, "Daylight" },
	{ { 0x02 }, "Fluorescent" },
	{ { 0x03 }, "Tungsten" },
	{ { 0xff }, "Cloudy" },
};
static const RegisterDescriptorType oly3040_reg_20[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"whitebalance", N_("White Balance"),
		VAL_NAME_INIT (oly3040_reg_20_val_names)
	}
};


/*
 * Register 33: focus mode.
 */
static const ValueNameType oly3040_reg_33_val_names[] = {
	{ { 0x01 }, "Macro" },
	{ { 0x02 }, "Auto" },
	{ { 0x03 }, "Manual" },
};
static const RegisterDescriptorType oly3040_reg_33[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"focus-mode", N_("Focus Mode"),
		VAL_NAME_INIT (oly3040_reg_33_val_names)
	}
};

/*
 * Register 34: LCD mode / write-only through SIERRA_SUBACTION / ACTION_LCD_MODE
 * This register always contains 1 (OFF).
 * Note: Always swicth off LCD before disconnecting the camera. Otherwise the camera
 * controls won't work, and you will need to replug the cable and switch off the LCD.
 * (This _is_ a documented "feature".)
 */

static const ValueNameType oly3040_reg_34_val_names[] = {
	{ { 0x01 }, "Off" },
	{ { 0x02 }, "Monitor" },
	{ { 0x03 }, "Normal" },
};
static const RegisterDescriptorType oly3040_reg_34[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK, 
		"lcd-mode", N_("LCD Mode"),
		VAL_NAME_INIT (oly3040_reg_34_val_names)
	}
};


/*
 * Register 35: lcd brightness.
 */
static const ValueNameType oly3040_reg_35_val_names[] = {
	{ { range: { 0, 7 } }, NULL },
};
static const RegisterDescriptorType oly3040_reg_35[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"lcd-brightness", N_("LCD Brightness"),
		VAL_NAME_INIT (oly3040_reg_35_val_names)
	}
};

/*
 * Register 24: camera power save (idle time before entering power save mode)
 */
static const ValueNameType oly3040_reg_24_val_names[] = {
	{ { range: { 30, 600, 30  } }, NULL },
};
static const RegisterDescriptorType oly3040_reg_24[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"camera-power-save", N_("Camera power save (seconds)"),
		VAL_NAME_INIT (oly3040_reg_24_val_names)
	}
};

/*
 * Register 23: camera power save when connected to PC
 * (idle time before entering power save mode)
 */
static const ValueNameType oly3040_reg_23_val_names[] = {
	{ { range: { 30, 600, 30  } }, NULL },
};
static const RegisterDescriptorType oly3040_reg_23[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"hsot-power-save", N_("Host power save (seconds)"),
		VAL_NAME_INIT (oly3040_reg_23_val_names)
	}
};
/*
 * Register 38: lcd auto shut off time
 */
static const ValueNameType oly3040_reg_38_val_names[] = {
	{ { range: { 30, 600, 30  } }, NULL },
};
static const RegisterDescriptorType oly3040_reg_38[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"lcd-auto-shutoff", N_("LCD Auto Shut Off (seconds)"),
		VAL_NAME_INIT (oly3040_reg_38_val_names)
	}
};

/*
 * Register 53: language setting. No meaning on Olympus
 */
/*
static const ValueNameType oly3040_reg_53_val_names[] = {
	{ { 0x03 }, "English" },
	{ { 0x04 }, "French" },
	{ { 0x05 }, "German" },
};
static const RegisterDescriptorType oly3040_reg_53[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"language", N_("Language"),
		VAL_NAME_INIT (oly3040_reg_53_val_names)
	}
};
*/

/*
 * Register 69: exposure compensation.
 */
static const ValueNameType oly3040_reg_69_val_names[] = {
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
static const RegisterDescriptorType oly3040_reg_69[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"exp", N_("Exposure Compensation"),
		VAL_NAME_INIT (oly3040_reg_69_val_names)
	}
};

/*
 * Register 70: exposure meter.
 */
static const ValueNameType oly3040_reg_70_val_names[] = {
	{ { 0x03 }, "Spot" },
	{ { 0x05 }, "Matrix" },
};
static const RegisterDescriptorType oly3040_reg_70[] = { 
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"exp-meter", N_("Exposure Metering"), 
		VAL_NAME_INIT (oly3040_reg_70_val_names)
	}
};

/*
 * Register 71: optical zoom value.
 */
static const ValueNameType oly3040_reg_71_val_names[] = {
	{
		{ range: { 7.3, 21.0, .1 } }, NULL
	}
};
static const RegisterDescriptorType oly3040_reg_71[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK, 
		"zoom", N_("Zoom (in millimeters)"),
		VAL_NAME_INIT (oly3040_reg_71_val_names)
	}
};

/*
 * Register 72: digital zoom
 */
static const ValueNameType oly3040_reg_72_val_names[] = {
	{ { 0x0000 }, "Off" },
	{ { 0x0208 }, "1.5x" },
	{ { 0x0008 }, "2x" },
	{ { 0x0408 }, "2.5x" },
};
static const RegisterDescriptorType oly3040_reg_72[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"dzoom", N_("Digital zoom"),
		VAL_NAME_INIT (oly3040_reg_72_val_names)
	}
};

/*
 * Register 85: ISO speed, read only
 */
static const ValueNameType oly3040_reg_85_val_names[] = {
	{ { 0 }, "Auto" },
	{ { 1 }, "100" },
	{ { 2 }, "200" },
	{ { 3 }, "400" },
};
static const RegisterDescriptorType oly3040_reg_85[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"iso", N_("ISO Speed"),
		VAL_NAME_INIT (oly3040_reg_85_val_names)
	}
};

/*
 * Register 103: focus position (only applicable in manual focus mode)
 *     1 to 120: macro positions
 *   121 to 240: normal positions
 */
static const ValueNameType oly3040_reg_103_val_names[] = {
	{ { range: { 1, 240, 1 } }, NULL },
};
static const RegisterDescriptorType oly3040_reg_103[] = { 
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK, 
		"focus-pos", N_("Focus position"),
		VAL_NAME_INIT (oly3040_reg_103_val_names)
	}
};

/*
 * Register 41: time format, read only
 */
static const ValueNameType oly3040_reg_41_val_names[] = {
	{ { 0 }, "Off" },
	{ { 1 }, "yymmdd" },
	{ { 2 }, "mmddyy" },
	{ { 3 }, "ddmmyy" },
};
static const RegisterDescriptorType oly3040_reg_41[] = { 
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK, 
		"timefmt", N_("Time format"),
		VAL_NAME_INIT (oly3040_reg_41_val_names)
	}
};


/*
 * All of the register used to modify picture settings. The register value
 * received from the camera is stored into this data area, so it cannot be
 * a const.
 */
static CameraRegisterType oly3040_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (oly3040, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (oly3040, 03, 4, CAM_DESC_DEFAULT, 0), /* shutter */
	CAM_REG_TYPE_INIT (oly3040, 05, 4, CAM_DESC_DEFAULT, 0), /* aperature (f-stop) */
	CAM_REG_TYPE_INIT (oly3040, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (oly3040, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (oly3040, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
	CAM_REG_TYPE_INIT (oly3040, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
	CAM_REG_TYPE_INIT (oly3040,103, 4, CAM_DESC_DEFAULT, 0), /* focus position */
	CAM_REG_TYPE_INIT (oly3040, 69, 8, CAM_DESC_DEFAULT, 0), /* exposure compensation */
	CAM_REG_TYPE_INIT (oly3040, 70, 4, CAM_DESC_DEFAULT, 0), /* exposure metering */
	CAM_REG_TYPE_INIT (oly3040, 71, 8, CAM_DESC_DEFAULT, 0), /* optical zoom */
	CAM_REG_TYPE_INIT (oly3040, 72, 4, CAM_DESC_DEFAULT, 0), /* digital zoom + lense +  AE lock */
	CAM_REG_TYPE_INIT (oly3040, 85, 4, CAM_DESC_DEFAULT, 0), /* ISO Speed, read only  */
};

/*
 * All of the register used to modify camera settings.
 */
static const CameraRegisterType oly3040_cam_regs[] = {
	CAM_REG_TYPE_INIT (oly3040, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (oly3040, 34, 4, CAM_DESC_SUBACTION, 
			  SIERRA_ACTION_LCD_MODE), /* lcd mode */
	CAM_REG_TYPE_INIT (oly3040, 35, 4, CAM_DESC_DEFAULT, 0), /* LCD brightness */
	CAM_REG_TYPE_INIT (oly3040, 38, 4, CAM_DESC_DEFAULT, 0), /* LCD auto shutoff */
	CAM_REG_TYPE_INIT (oly3040, 24, 4, CAM_DESC_DEFAULT, 0), /* Camera power save */
	CAM_REG_TYPE_INIT (oly3040, 23, 4, CAM_DESC_DEFAULT, 0), /* Host power save */
	CAM_REG_TYPE_INIT (oly3040, 41, 4, CAM_DESC_DEFAULT, 0), /* time format, read only */
};

static const CameraRegisterSetType oly3040_desc[] = {
		{ 
			N_("Picture Settings"), 
			SIZE_ADDR (CameraRegisterType, oly3040_pic_regs)
		},
		{ 
			N_("Camera Settings"), 
			SIZE_ADDR (CameraRegisterType, oly3040_cam_regs)
		},
};

static const char oly3040_manual[] =
N_(
"Some notes about Olympus cameras:\n"
"(1) Camera Configuration:\n"
"    A value of 0 will take the default one (auto).\n"
"(2) Olympus C-3040Z (and possibly also the C-2040Z\n"
"    and others) have a USB PC Control mode. In order\n"
"    to use this mode, the camera must be switched \n"
"    into 'USB PC control mode'. To get to the menu \n"
"    for switching modes, turn on the camera, open \n"
"    the memory card access door and then press and \n"
"    hold both of the menu and LCD buttons until the \n"
"    camera control menu appears. Set it to ON. \n"
"(3) If you switch the 'LCD mode' to 'Monitor' or \n"
"    'Normal', don't forget to switch it back to 'Off' \n"
"    before disconnectig. Otherwise you cannot use \n"
"    the camera's buttons. If you end up with this \n"
"    state, you should reconnect the camera to the \n"
"    PC and switch LCD to 'Off'."
);

static const char default_manual[] = 
N_(
"Default sierra driver:\n\n"
"    This is the default sierra driver, it\n"
"    should be capable of supporting the download\n"
"    and browsing of pictures on your camera.\n\n"
"    Camera configuration (or preferences)\n"
"    settings are based on the Olympus 3040,\n"
"    and are likely incomplete. If you verify\n"
"    that the configuration settings are\n"
"    complete for your camera, or can contribute\n"
"    code to support complete configuration,\n"
"    please contact gphoto-devel@gphoto.net\n"
);

const CameraDescType oly3040_cam_desc = { oly3040_desc, oly3040_manual };
const CameraDescType sierra_default_cam_desc = { oly3040_desc, default_manual };
