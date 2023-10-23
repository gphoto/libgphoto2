/* nikon_desc.c:
 *
 * Copyright 2002 Patrick Mansfield <patman@aracnet.com>
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

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>

#include "libgphoto2/i18n.h"

#include "sierra.h"
#include "library.h"
#include "sierra-desc.h"

/*
 * Camera descriptor for Nikon Coolpix 880 (cp880).
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


/*
 * Register 1: resolution/size.
 */
static const ValueNameType cp880_reg_01_val_names[] = {
	/*
	 * These values suck. why didn't they make these maskable, or use
	 * a nibble for each part?  Just use a the combined resolution + size.
	 * It would be good to try and break these into two selections.
	 */
	{ { 0x01 }, "Basic-VGA" },	/* 640 x 480 */
	{ { 0x02 }, "Normal-VGA" },
	{ { 0x03 }, "Fine-VGA" },
	{ { 0x07 }, "Basic-XGA" },	/* 1024 x 768 */
	{ { 0x08 }, "Normal-XGA" },
	{ { 0x09 }, "Fine-XGA" },
	{ { 0x11 }, "Basic-Full" },
	{ { 0x12 }, "Normal-Full" },
	{ { 0x13 }, "Fine-Full" },
	{ { 0x23 }, "Hi-Full" },
};
static const RegisterDescriptorType cp880_reg_01[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"resolution", N_("Resolution plus Size"),
		VAL_NAME_INIT (cp880_reg_01_val_names)
	}
};

/*
 * Register 1: resolution/size.
 */
static const ValueNameType cp2500_reg_01_val_names[] = {
	/*
	 * These values suck. why didn't they make these maskable, or use
	 * a nibble for each part?  Just use a the combined resolution + size.
	 * It would be good to try and break these into two selections.
	 */
	{ { 0x01 }, "Basic-VGA" },	/* 640 x 480 */
	{ { 0x02 }, "Normal-VGA" },
	{ { 0x03 }, "Fine-VGA" },

	{ { 0x04 }, "Basic-SXGA" },	/* 1280 x 1024 */
	{ { 0x05 }, "Normal-SXGA" },
	{ { 0x06 }, "Fine-SXGA" },

	{ { 0x07 }, "Basic-XGA" },	/* 1024 x 768 */
	{ { 0x08 }, "Normal-XGA" },
	{ { 0x09 }, "Fine-XGA" },

	{ { 0x0a }, "Basic-UXGA" },	/* 1600 x 1200 aka 2 Megapixel */
	{ { 0x0b }, "Normal-UXGA" },
	{ { 0x0c }, "Fine-UXGA" },

};
static const RegisterDescriptorType cp2500_reg_01[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"resolution", N_("Resolution plus Size"),
		VAL_NAME_INIT (cp2500_reg_01_val_names)
	}
};

/*
 * Register 2: Date and time.
 */
static const ValueNameType cp880_reg_02_val_names[] = {
	/*
	 * Dummy value, since we need at least one of these to
	 * display anything.
	 */
	{ { 0x00 }, "Dummy" },
};
static const RegisterDescriptorType cp880_reg_02[] = {
	{
		GP_WIDGET_DATE, GP_REG_NO_MASK,
		"date-time", N_("Date and time (GMT)"),
		VAL_NAME_INIT (cp880_reg_02_val_names)
	}
};

/*
 * Register 3: shutter speed, cp880 must be in manual mode for this to
 * work.
 */
#ifdef RANGE_FOR_SHUTTER
static const ValueNameType cp880_reg_03_val_names[] = {
	{
		{ .range = { 0, 8000000 } }, NULL
	}
};
static const RegisterDescriptorType cp880_reg_03[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"shutter", N_("Shutter Speed microseconds (0 auto)"),
		VAL_NAME_INIT (cp880_reg_03_val_names)
	}
};
#endif
static const ValueNameType cp880_reg_03_val_names[] = {
	{ {       0 }, N_("Auto") },
	{ {    1000 }, "1/1000" },
	{ {    2000 }, "1/500" },
	{ {    4000 }, "1/250" },
	{ {    8000 }, "1/125" },
	{ {   16666 }, "1/60" },
	{ {   33333 }, "1/30" },
	{ {   66666 }, "1/15" },
	{ {  125000 }, "1/8" },
	{ {  250000 }, "1/4" },
	{ {  500000 }, "1/2" },
	{ { 1000000 }, "1" },
	{ { 2000000 }, "2" },
	{ { 3000000 }, "3" },
	{ { 4000000 }, "4" },
	{ { 5000000 }, "5" },
	{ { 6000000 }, "6" },
	{ { 7000000 }, "7" },
	{ { 8000000 }, "8" },
	/* { { -20 }, "bulb?" }, */

};
static const RegisterDescriptorType cp880_reg_03[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"shutter", N_("Shutter Speed (in seconds)"),
		VAL_NAME_INIT (cp880_reg_03_val_names)
	}
};

/*
 * Register 5: aperture settings (f-stop)
 */
static const ValueNameType cp880_reg_05_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, N_("Low") },
	{ { 2 }, N_("Medium") },
	/* { { 4 }, N_("High") }, not on a cp880 */
};
static const RegisterDescriptorType cp880_reg_05[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"aperture", N_("Aperture Settings"),
		VAL_NAME_INIT (cp880_reg_05_val_names)
	}
};

/*
 * Register 6: color mode (not tested on cp880)
 */
static const ValueNameType cp880_reg_06_val_names[] = {
	{ { 1 }, N_("Color") },
	{ { 2 }, N_("B/W") },
};
static const RegisterDescriptorType cp880_reg_06[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"color", N_("Color Mode"),
		VAL_NAME_INIT (cp880_reg_06_val_names)
	}
};

/*
 * Register 7: flash settings
 */
static const ValueNameType cp880_reg_07_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, N_("Force") },
	{ { 2 }, N_("Off") },
	{ { 3 }, N_("Anti-redeye") },
	{ { 4 }, N_("Slow-sync") },
};
static const RegisterDescriptorType cp880_reg_07[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"flash", N_("Flash Settings"),
		VAL_NAME_INIT (cp880_reg_07_val_names)
	}
};

/*
 * Register 19: Image adjustment.
 */
static const ValueNameType cp880_reg_19_val_names[] = {
	{ { 5 }, N_("Auto") },
	{ { 0 }, N_("Normal") },		/* also 0 if chose b+w */
	{ { 1 }, N_("Contrast+") },		/* cp880 more contrast */
	{ { 2 }, N_("Contrast-") },		/* cp880 less contrast */
	{ { 3 }, N_("Brightness+") },	/* cp880 lighten image */
	{ { 4 }, N_("Brightness-") },	/* cp880 darken image */
};
static const RegisterDescriptorType cp880_reg_19[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"image-adj", N_("Image Adjustment"), /* brightness/contrast */
		VAL_NAME_INIT (cp880_reg_19_val_names)
	}
};

/*
 * Register 20: white balance.
 *
 * Could not determine how the camera does the gradation in settings -
 * such as fine +-1,2,3, and FL1 FL2 FL3. Dumping all registers (1 to 90)
 * shows no changes when going (for example) from fine +3 to -3.
 *
 * Setting these via the camera to a +3/2/1 etc, and then changing them
 * via gphoto2 to another value changes the "type" but the value +3/2/1 is
 * the same, so there is some other register or setting that changes the
 * range of these values.
 *
 * It appears this needs the sierra extended protocol to be complete.
 */
static const ValueNameType cp880_reg_20_val_names[] = {
	{ { 0x00 }, N_("Auto") },
	{ { 0x01 }, N_("Fine") },	/* fine +3, +2, ... -3 */
	{ { 0x02 }, N_("Incandescent") }, /* incan +3 ... -3 */
	{ { 0x03 }, N_("Fluorescent") }, /* FL3, FL2, and FL1 */
	{ { 0x05 }, N_("Flash") },	/* speedlight/flash +3 ... -3 */
	{ { 0x06 }, N_("Preset") },	/* preset automatically by camera, prob range */
	{ { 0xff }, N_("Cloudy") }, /* cloudy +3 ... -3 */
};
static const RegisterDescriptorType cp880_reg_20[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"whitebalance", N_("White Balance"),
		VAL_NAME_INIT (cp880_reg_20_val_names)
	}
};

#ifdef REG_30_FAILS
/*
 * Register 30: LED mode, write only. Writing 0 and 1 do nothing, 2
 * retracts the lens and causes an error.
 */
static const ValueNameType cp880_reg_30_val_names[] = {
	{ { 0x00 }, N_("Off") },
	{ { 0x01 }, N_("On") },
	{ { 0x02 }, N_("Blink") },
};
static const RegisterDescriptorType cp880_reg_30[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"led", N_("LED Mode"),
		VAL_NAME_INIT (cp880_reg_30_val_names)
	}
};
#endif

/*
 * Register 33: focus mode. Can change these, but macro mode does not work
 * correctly - it does not zoom in or focus (macro-mode focus) correctly.
 * And, setting infinity using the cp880 also turns off the flash, here it
 * does not turn off the flash.
 */
static const ValueNameType cp880_reg_33_val_names[] = {
	{ { 0x01 }, N_("Macro") },
	{ { 0x02 }, N_("Normal") },
	{ { 0x03 }, N_("Infinity") },
};
static const RegisterDescriptorType cp880_reg_33[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"focus-mode", N_("Focus Mode"),
		VAL_NAME_INIT (cp880_reg_33_val_names)
	}
};

/*
 * Register 34: operation mode. This is only readable, an action with
 * subaction is required to change the mode.
 *
 * Note that on a cp880, sometimes the play mode ends up with a messed up
 * display or an error message, powering the camera off and on seems to
 * fix the problem.
 *
 * The use of next and previous should be part of a separate
 * interface, such that a user can continuously press next or previous to
 * cycle through the images (while the camera is in play mode).
 *
 * Use command 2 (via sierra_sub_action) with action
 * SIERRA_ACTION_LCD_MODE (8), with a sub-action below to change the mode.
 * So, read reg 34 to figure out the mode, then use sierra_action to
 * change the mode - the sierra action needs another arg to set the
 * secondary action for this. The register 34 values match those needed to
 * change the mode.
 *
 * Using a register get/set method of CAM_DESC_SUBACTION enables the
 * above kluge.
 */
static const ValueNameType cp880_reg_34_val_names[] = {
	{ { 0x01 }, N_("Off") },	/* turn off lcd while waiting to capture */
	{ { 0x02 }, N_("Record") },	/* turn on lcd while waiting to capture */
	{ { 0x03 }, N_("Play") },	/* slide-show like mode */
	{ { 0x06 }, N_("Preview Thumbnail") },	/* slide-show thumbnail */
	{ { 0x07 }, N_("Next") },			/* go to the next image */
	{ { 0x08 }, N_("Previous") },		/* go to previous image */
	/*
	{ { 0x00 }, "unknown 0x00" },
	{ { 0x04 }, "unknown 0x04" },
	{ { 0x05 }, "unknown 0x05" },
	{ { 0x09 }, "unknown 0x09" },
	{ { 0x0a }, "unknown 0x0a" },
	{ { 0x0b }, "unknown 0x0b" },
	{ { 0x0c }, "unknown 0x0c" },
	*/
};
static const RegisterDescriptorType cp880_reg_34[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"op-mode", N_("Operation Mode"),
		VAL_NAME_INIT (cp880_reg_34_val_names)
	}
};

/*
 * Register 35: lcd brightness.
 */
static const ValueNameType cp880_reg_35_val_names[] = {
	{ { .range = { 0, 7 } }, NULL },
};
static const RegisterDescriptorType cp880_reg_35[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"lcd-brightness", N_("LCD Brightness"),
		VAL_NAME_INIT (cp880_reg_35_val_names)
	}
};

/*
 * Register 38: lcd auto shut off time, not verified for cp880, unknown
 * what the maximum range should be.
 */
static const ValueNameType cp880_reg_38_val_names[] = {
	{ { .range = { 0, 255 /* XXX? */ } }, NULL },
};
static const RegisterDescriptorType cp880_reg_38[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"lcd-auto-shutoff", N_("LCD Auto Shut Off (seconds)"),
		VAL_NAME_INIT (cp880_reg_38_val_names)
	}
};

/*
 * Register 53: language setting. On the cp880, can set to japanese, but
 * cannot see any change in this register.
 *
 * It appears this needs the sierra extended protocol to be complete.
 */
static const ValueNameType cp880_reg_53_val_names[] = {
	{ { 0x03 }, N_("English") },
	{ { 0x04 }, N_("French") },
	{ { 0x05 }, N_("German") },
	/* { { 0x06 }, "Italian" },	not on cp880 */
	/* { { 0x08 }, "Spanish" },	not on cp880 */
	/* { { 0x0a }, "Dutch" },	not on cp880 */
	/* { { 0xnn }, "Japanese" },
	 *
	 * weird, can set to japanese, but can't see any change in any
	 * registers; reg 53 is 3 whether it is english or japanese.
	 */
};
static const RegisterDescriptorType cp880_reg_53[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"language", N_("Language"),
		VAL_NAME_INIT (cp880_reg_53_val_names)
	}
};

/*
 * Register 69: exposure compensation.
 *
 * The negative value is properly converted - the camera uses values
 * compatible with little endian machine (or it is converted by gphoto2?).
 *
 * The cp880 sucks here, the register values goes from -20 to 20, but
 * the only legal values are -20, -17, -13, ..., -7, -3, 0, 3, 7, ..., 20.
 * It doesn't round, and it is too hard to round using a table, so just
 * use a pull down menu rather than the more obvious range (slider).
 */
#ifdef RANGE_FOR_EXPOSURE
static const ValueNameType cp880_reg_69_val_names[] = {
	{
		{ .range = { -2.0, 2.0, .1 } }, NULL,
	}
};
static const RegisterDescriptorType cp880_reg_69[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"exp", N_("Exposure Compensation"),
		VAL_NAME_INIT (cp880_reg_69_val_names)
	}
};
#endif
static const ValueNameType cp880_reg_69_val_names[] = {
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
static const RegisterDescriptorType cp880_reg_69[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"exp", N_("Exposure Compensation"),
		VAL_NAME_INIT (cp880_reg_69_val_names)
	}
};

/*
 * Register 70: exposure meter.
 */
static const ValueNameType cp880_reg_70_val_names[] = {
	{ { 0x02 }, N_("Center-Weighted") },
	{ { 0x03 }, N_("Spot") },
	{ { 0x05 }, N_("Matrix") },
	{ { 0x06 }, N_("Spot-AF") },
};
static const RegisterDescriptorType cp880_reg_70[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"exp-meter", N_("Exposure Metering"),
		VAL_NAME_INIT (cp880_reg_70_val_names)
	}
};

/*
 * Register 71: optical zoom value. This does not work on the cp880.
 *
 * It appears this needs the sierra extended protocol to be complete.
 */
static const ValueNameType cp880_reg_71_val_names[] = {
	{
		{ .range = { 8.0, 20.0, .1 } }, NULL
	}
};
static const RegisterDescriptorType cp880_reg_71[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"zoom", N_("Zoom (in millimeters)"),
		VAL_NAME_INIT (cp880_reg_71_val_names)
	}
};

/*
 * Register 72: digital zoom, not fully working for cp880. This is likely
 * related to the optical zoom not working, as the digital zoom modifies
 * register 71 (when done from the camera). I can only set it to 2.0.
 *
 * It appears this needs the sierra extended protocol to be complete.
 *
 * The hi portion (mask 0xff00) has digital zoom values. On the cp880, it
 * looks like the regular zoom + the digital zoom value determine the
 * actual digital zoom used (the optical zoom must be at max before the
 * camera goes to a digital zoom).
 *
 * The low portion (mask 0x00ff) seems to have three values, none verified
 * on the cp880 - maybe should probably be split further.
 */
#ifdef OLD_WAY
static const ValueNameType cp880_reg_72_mask_hi_val_names[] = {
	{ { 0x0100 }, "1.25" },
	{ { 0x0200 }, "1.60" },
	{ { 0x0300 }, "2.00" },
	{ { 0x0400 }, "2.50" },
	{ { 0x0500 }, N_("none") },
};
static const ValueNameType cp880_reg_72_mask_lo_val_names[] = {
	{ { 0x0001 }, N_("AE-lock") },
	{ { 0x0002 }, N_("Fisheye") },
	{ { 0x0004 }, N_("Wide") },
};
static const RegisterDescriptorType cp880_reg_72[] = {
	{
		GP_WIDGET_RADIO, 0xff00,
		"dzoom", N_("Digital Zoom"),
		VAL_NAME_INIT (cp880_reg_72_mask_hi_val_names)
	},
	{
		GP_WIDGET_RADIO, 0x00ff,
		"misc-exp-lenses", N_("Misc exposure/lens settings"),
		VAL_NAME_INIT (cp880_reg_72_mask_lo_val_names)
	},
};
#endif
static const ValueNameType cp880_reg_72_mask_hi_val_names[] = {
	{ { 0x00 }, N_("off") },
	{ { 0x08 }, N_("on") },

};
static const ValueNameType cp880_reg_72_mask_lo_val_names[] = {
	{ { 0x00 }, N_("off") },
	{ { 0x01 }, N_("on") },
};

static const RegisterDescriptorType cp880_reg_72[] = {
	{
		GP_WIDGET_RADIO, 0x08,
		"dzoom", N_("Digital Zoom"),
		VAL_NAME_INIT (cp880_reg_72_mask_hi_val_names)
	},
	{
		GP_WIDGET_RADIO, 0x01,
		"ael", N_("Auto exposure lock"),
		VAL_NAME_INIT (cp880_reg_72_mask_lo_val_names)
	},
};

/*
 * All of the register used to modify picture settings. The register value
 * received from the camera is stored into this data area, so it cannot be
 * a const.
 */
static CameraRegisterType cp880_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (cp880, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (cp880, 03, 4, CAM_DESC_DEFAULT, 0), /* shutter */
	CAM_REG_TYPE_INIT (cp880, 05, 4, CAM_DESC_DEFAULT, 0), /* aperture (f-stop) */
	CAM_REG_TYPE_INIT (cp880, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (cp880, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (cp880, 19, 4, CAM_DESC_DEFAULT, 0), /* brightness/contrast */
	CAM_REG_TYPE_INIT (cp880, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
#ifdef REG_30_FAILS
	CAM_REG_TYPE_INIT (cp880, 30, 4, CAM_DESC_DEFAULT, 0), /* LED mode */
#endif
	CAM_REG_TYPE_INIT (cp880, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
	CAM_REG_TYPE_INIT (cp880, 69, 8, CAM_DESC_DEFAULT, 0), /* exposure compensation */
	CAM_REG_TYPE_INIT (cp880, 70, 4, CAM_DESC_DEFAULT, 0), /* exposure metering */
	CAM_REG_TYPE_INIT (cp880, 71, 8, CAM_DESC_DEFAULT, 0), /* optical zoom */
	CAM_REG_TYPE_INIT (cp880, 72, 4, CAM_DESC_DEFAULT, 0), /* digital zoom + lense +  AE lock */
};

/*
 * All of the register used to modify camera settings.
 */
static CameraRegisterType cp880_cam_regs[] = {
	CAM_REG_TYPE_INIT (cp880, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (cp880, 34, 4, CAM_DESC_SUBACTION,
			  SIERRA_ACTION_LCD_MODE), /* lcd mode */
	CAM_REG_TYPE_INIT (cp880, 35, 4, CAM_DESC_DEFAULT, 0), /* LCD brightness */
	CAM_REG_TYPE_INIT (cp880, 38, 4, CAM_DESC_DEFAULT, 0), /* LCD auto shutoff */
	CAM_REG_TYPE_INIT (cp880, 53, 4, CAM_DESC_DEFAULT, 0), /* language */
};

/*
 * All of the register used to modify picture settings. The register value
 * received from the camera is stored into this data area, so it cannot be
 * a const.
 */
static CameraRegisterType cp2500_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (cp2500, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (cp880, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (cp880, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (cp880, 19, 4, CAM_DESC_DEFAULT, 0), /* brightness/contrast */
	CAM_REG_TYPE_INIT (cp880, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
#ifdef REG_30_FAILS
	CAM_REG_TYPE_INIT (cp880, 30, 4, CAM_DESC_DEFAULT, 0), /* LED mode */
#endif
	CAM_REG_TYPE_INIT (cp880, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
	CAM_REG_TYPE_INIT (cp880, 69, 8, CAM_DESC_DEFAULT, 0), /* exposure compensation */
	CAM_REG_TYPE_INIT (cp880, 70, 4, CAM_DESC_DEFAULT, 0), /* exposure metering */
	CAM_REG_TYPE_INIT (cp880, 71, 8, CAM_DESC_DEFAULT, 0), /* optical zoom */
	CAM_REG_TYPE_INIT (cp880, 72, 4, CAM_DESC_DEFAULT, 0), /* digital zoom + lense +  AE lock */
};

/*
 * All of the register used to modify camera settings.
 */
static CameraRegisterType cp2500_cam_regs[] = {
	CAM_REG_TYPE_INIT (cp880, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (cp880, 34, 4, CAM_DESC_SUBACTION,
			  SIERRA_ACTION_LCD_MODE), /* lcd mode */
	CAM_REG_TYPE_INIT (cp880, 35, 4, CAM_DESC_DEFAULT, 0), /* LCD brightness */
	CAM_REG_TYPE_INIT (cp880, 53, 4, CAM_DESC_DEFAULT, 0), /* language */
};

/*
 * All of the register used to modify picture settings. The register value
 * received from the camera is stored into this data area, so it cannot be
 * a const.
 */
static CameraRegisterType cp4300_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (cp880, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (cp880, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (cp880, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (cp880, 19, 4, CAM_DESC_DEFAULT, 0), /* brightness/contrast */
	CAM_REG_TYPE_INIT (cp880, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
#ifdef REG_30_FAILS
	CAM_REG_TYPE_INIT (cp880, 30, 4, CAM_DESC_DEFAULT, 0), /* LED mode */
#endif
	CAM_REG_TYPE_INIT (cp880, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
	CAM_REG_TYPE_INIT (cp880, 69, 8, CAM_DESC_DEFAULT, 0), /* exposure compensation */
	CAM_REG_TYPE_INIT (cp880, 70, 4, CAM_DESC_DEFAULT, 0), /* exposure metering */
	CAM_REG_TYPE_INIT (cp880, 71, 8, CAM_DESC_DEFAULT, 0), /* optical zoom */
	CAM_REG_TYPE_INIT (cp880, 72, 4, CAM_DESC_DEFAULT, 0), /* digital zoom + lense +  AE lock */
};

/*
 * All of the register used to modify camera settings.
 */
static CameraRegisterType cp4300_cam_regs[] = {
	CAM_REG_TYPE_INIT (cp880, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (cp880, 34, 4, CAM_DESC_SUBACTION,
			  SIERRA_ACTION_LCD_MODE), /* lcd mode */
	CAM_REG_TYPE_INIT (cp880, 35, 4, CAM_DESC_DEFAULT, 0), /* LCD brightness */
	CAM_REG_TYPE_INIT (cp880, 53, 4, CAM_DESC_DEFAULT, 0), /* language */
};


static const CameraRegisterSetType cp880_desc[] = {
	{
		N_("Picture Settings"),
		SIZE_ADDR (CameraRegisterType, cp880_pic_regs)
	},
	{
		N_("Camera Settings"),
		SIZE_ADDR (CameraRegisterType, cp880_cam_regs)
	},
};

static const CameraRegisterSetType cp2500_desc[] = {
	{
		N_("Picture Settings"),
		SIZE_ADDR (CameraRegisterType, cp2500_pic_regs)
	},
	{
		N_("Camera Settings"),
		SIZE_ADDR (CameraRegisterType, cp2500_cam_regs)
	},
};

static const CameraRegisterSetType cp4300_desc[] = {
	{
		N_("Picture Settings"),
		SIZE_ADDR (CameraRegisterType, cp4300_pic_regs)
	},
	{
		N_("Camera Settings"),
		SIZE_ADDR (CameraRegisterType, cp4300_cam_regs)
	},
};

static const char cp880_manual[] =
N_(
"Nikon Coolpix 880:\n"
"    Camera configuration (or preferences):\n\n"
"        The optical zoom does not properly\n"
"        function.\n\n"
"        Not all configuration settings\n"
"        can be properly read or written, for\n"
"        example, the fine tuned setting of\n"
"        white balance, and the language settings.\n\n"
"        Put the camera in 'M' mode in order to\n"
"        to set the shutter speed.\n"
);

/*
 *  Note: use of the 995 has not been tested, it might not even be
 *  possible to control the camera via the USB port since it reportedly
 *  appears as a USB mass storage device.
 */
static const char cp995_manual[] =
N_(
"Nikon Coolpix 995:\n"
"    Camera configuration (preferences) for this\n"
"    camera are incomplete, contact the gphoto\n"
"    developer mailing list\n"
"    if you would like to contribute to this\n"
"    driver.\n\n"
"    The download should function correctly.\n"
);

const CameraDescType cp880_cam_desc = { cp880_desc, cp880_manual,
	SIERRA_EXT_PROTO, };
const CameraDescType cp995_cam_desc = { cp880_desc, cp995_manual,
	SIERRA_EXT_PROTO, };
const CameraDescType cp2500_cam_desc = { cp2500_desc, cp880_manual,
	SIERRA_EXT_PROTO, };
const CameraDescType cp4300_cam_desc = { cp4300_desc, cp880_manual,
	SIERRA_EXT_PROTO, };
