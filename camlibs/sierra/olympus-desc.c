/* olympus_desc.c:
 *
 * Olympus 3040 specific code (most of this file):
 * 	David Selmeczi <david@esr.elte.hu>
 * Olympus 3000z code, based on data from:
 * 	Till Kamppeter <till.kamppeter@gmx.net>
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
 * Camera descriptor for Olympus C-3040Z, 3000Z (and somday maybe others).
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
 *
 * These values suck. why didn't they make these maskable, or use a nibble
 * for each part?  Just use a the combined resolution + size.  It would be
 * good to try and break these into two selections.
 */
static const ValueNameType oly3040_reg_01_val_names[] = {
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
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"resolution", N_("Resolution plus Size"),
		VAL_NAME_INIT (oly3040_reg_01_val_names)
	}
};

static const ValueNameType oly750uz_reg_01_val_names[] = {
	{ { 0x01 }, "SQ2-640x480-NORMAL" },
	{ { 0x02 }, "SQ1-2048x1536-NORMAL" },
	{ { 0x03 }, "SQ1-2048x1536-HIGH" },
	{ { 0x04 }, "SQ2-640x480-HIGH" },
	{ { 0x05 }, "SQ2-1024x768-NORMAL" },
	{ { 0x06 }, "SQ2-1024x768-HIGH" },
	{ { 0x07 }, "SQ1-1280x960-NORMAL" },
	{ { 0x08 }, "SQ1-1280x960-HIGH" },
	{ { 0x09 }, "SQ1-1600x1200-NORMAL" },
	{ { 0x0a }, "SQ1-1600x1200-HIGH" },
	{ { 0x0d }, "HQ-2288x1712" },
	{ { 0x0e }, "SHQ-2288x1712" },
	{ { 0x0f }, "3:2-HQ-2288x1520" },
	{ { 0x10 }, "3:2-SHQ-2288x1620" },
	{ { 0x13 }, "LG-HQ-3200x2400" },
	{ { 0x14 }, "LG-SHQ-3200x2400" },
	{ { 0x24 }, "TIFF-1600x1200" },
	{ { 0x25 }, "TIFF-2048x1536" },
	{ { 0x26 }, "TIFF-2218x1712" },
	{ { 0x27 }, "3:2-TIFF-2218x1712" },
};
static const RegisterDescriptorType oly750uz_reg_01[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"resolution", N_("Resolution plus Size"),
		VAL_NAME_INIT (oly750uz_reg_01_val_names)
	}
};

static const ValueNameType olysp500uz_reg_01_val_names[] = {
	{ { 0x00 }, "SQ2-640x480-NORMAL" },
	{ { 0x01 }, "SQ2-1024x768-NORMAL" },
	{ { 0x02 }, "SQ2-1280x960-NORMAL" },
	{ { 0x03 }, "SQ1-1600x1200-NORMAL" },
	{ { 0x04 }, "SQ1-2048x1536-NORMAL" },
	{ { 0x05 }, "SQ1-2288x1712-NORMAL" },
	{ { 0x06 }, "SQ1-2592x1944-NORMAL" },
	{ { 0x07 }, "RAW?-2816x2112-NORMAL" },
	{ { 0x100 }, "SQ2-640x480-HIGH" },
	{ { 0x101 }, "SQ2-1024x768-HIGH" },
	{ { 0x102 }, "SQ2-1280x960-HIGH" },
	{ { 0x103 }, "SQ1-1600x1200-HIGH" },
	{ { 0x104 }, "SQ1-2048x1536-HIGH" },
	{ { 0x105 }, "SQ1-2288x1712-HIGH" },
	{ { 0x106 }, "SQ1-2592x1944-HIGH" },
	{ { 0x1007 }, "HQ?-2816x1880-NORMAL" },
	{ { 0x10007 }, "HQ?-2816x1880-NORMAL" },
	{ { 0x10107 }, "HQ?-2816x1880-3:2" },
	{ { 0x107 }, "HQ?-2816x2112-3:2" },
};
static const RegisterDescriptorType olysp500uz_reg_01[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"resolution", N_("Resolution plus Size"),
		VAL_NAME_INIT (olysp500uz_reg_01_val_names)
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
 * Register 3: shutter speed, some cameras must be in S or M mode.
 *
 * In S mode, aperture is compensated.
 *
 * Possibly could be any other value in the range of 1250 and 16M
 */
static const ValueNameType oly3040_reg_03_val_names[] = {
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
	{ {  200000 }, "1/4" }, /* XXX patman should this be 25000? */
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

static const ValueNameType oly750uz_reg_03_val_names[] = {
	{ {       0 }, N_("Auto") },
	{ {    1000 }, "1/1000" },
	{ {    1250 }, "1/800" },
	{ {    1538 }, "1/650" },
	{ {    2000 }, "1/500" },
	{ {    2500 }, "1/400" },
	{ {    3125 }, "1/320" },
	{ {    4000 }, "1/250" },
	{ {    5000 }, "1/200" },
	{ {    6250 }, "1/160" },
	{ {    8000 }, "1/125" },
	{ {   10000 }, "1/100" },
	{ {   12500 }, "1/80" },
	{ {   16666 }, "1/60" },
	{ {   20000 }, "1/50" },
	{ {   25000 }, "1/40" },
	{ {   33333 }, "1/30" },
	{ {   40000 }, "1/25" },
	{ {   50000 }, "1/20" },
	{ {   66667 }, "1/15" },
	{ {   76923 }, "1/13" },
	{ {  100000 }, "1/10" },
	{ {  125000 }, "1/8" },
	{ {  166667 }, "1/6" },
	{ {  200000 }, "1/5" },
	{ {  250000 }, "1/4" },
	{ {  333333 }, "1/3" },
	{ {  400000 }, "1/2.5" },
	{ {  500000 }, "1/2" },
	{ {  625000 }, "1/1.6" },
	{ {  769230 }, "1/1.3" },
	{ { 1000000 }, "1" },
	{ { 1300000 }, "1.3" },
	{ { 1600000 }, "1.6" },
	{ { 2000000 }, "2" },
	{ { 2500000 }, "2.5" },
	{ { 3200000 }, "3.2" },
	{ { 4000000 }, "4" },
	{ { 5000000 }, "5" },
	{ { 6000000 }, "6" },
	{ { 8000000 }, "8" },
	{ {10000000 }, "10" },
	{ {13000000 }, "13" },
	{ {16000000 }, "16" },
};
static const RegisterDescriptorType oly750uz_reg_03[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"shutter", N_("Shutter Speed (in seconds)"),
		VAL_NAME_INIT (oly750uz_reg_03_val_names)
	}
};

static const ValueNameType olysp500uz_reg_03_val_names[] = {
	{ {       0 }, N_("Auto") },
	{ {     250 }, "1/4000" },
	{ {     313 }, "1/3200" },
	{ {     400 }, "1/2500" },
	{ {     500 }, "1/2000" },
	{ {     625 }, "1/1600" },
	{ {     800 }, "1/1250" },
	{ {    1000 }, "1/1000" },
	{ {    1250 }, "1/800" },
	{ {    1538 }, "1/650" },
	{ {    2000 }, "1/500" },
	{ {    2500 }, "1/400" },
	{ {    3125 }, "1/320" },
	{ {    4000 }, "1/250" },
	{ {    5000 }, "1/200" },
	{ {    6250 }, "1/160" },
	{ {    8000 }, "1/125" },
	{ {   10000 }, "1/100" },
	{ {   12500 }, "1/80" },
	{ {   16666 }, "1/60" },
	{ {   20000 }, "1/50" },
	{ {   25000 }, "1/40" },
	{ {   33333 }, "1/30" },
	{ {   40000 }, "1/25" },
	{ {   50000 }, "1/20" },
	{ {   66667 }, "1/15" },
	{ {   76923 }, "1/13" },
	{ {  100000 }, "1/10" },
	{ {  125000 }, "1/8" },
	{ {  166667 }, "1/6" },
	{ {  200000 }, "1/5" },
	{ {  250000 }, "1/4" },
	{ {  333333 }, "1/3" },
	{ {  400000 }, "1/2.5" },
	{ {  500000 }, "1/2" },
	{ {  625000 }, "1/1.6" },
	{ {  769230 }, "1/1.3" },
	{ { 1000000 }, "1" },
	{ { 1300000 }, "1.3" },
	{ { 1600000 }, "1.6" },
	{ { 2000000 }, "2" },
	{ { 2500000 }, "2.5" },
	{ { 3200000 }, "3.2" },
	{ { 4000000 }, "4" },
	{ { 5000000 }, "5" },
	{ { 6000000 }, "6" },
	{ { 8000000 }, "8" },
	{ {10000000 }, "10" },
	{ {13000000 }, "13" },
	{ {16000000 }, "16" },
};
static const RegisterDescriptorType olysp500uz_reg_03[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"shutter", N_("Shutter Speed (in seconds)"),
		VAL_NAME_INIT (olysp500uz_reg_03_val_names)
	}
};
/*
 * Past behaviour is that you have to pick a supported value, you can't
 * arbitrarily set this to some value and have it work, so using a range
 * does not work well, since only about 30 of some 16 million values are
 * valid.
 */
#if 0
static const ValueNameType olyrange_reg_03_val_names[] = {
	{
		{ .range = { 0, 16000000, 100 } }, NULL
	}
};
static const RegisterDescriptorType olyrange_reg_03[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"shutter", N_("Shutter Speed microseconds (0 auto)"),
		VAL_NAME_INIT (olyrange_reg_03_val_names)
	}
};
#endif

/*
 * Olympus 3040 Register 5: aperture settings. Works only in A or M mode
 * In A mode, shutter speed is compensated
 */
static const ValueNameType oly3040_reg_05_val_names[] = {
	{ { 0 }, N_("Auto") },
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
		"aperture", N_("Aperture Settings"),
		VAL_NAME_INIT (oly3040_reg_05_val_names)
	}
};

/*
 * Olympus 3000z Register 5: aperture settings. Unknown if this comment
 * applies: Works only in A or M mode In A mode, shutter speed is
 * compensated
 */
static const ValueNameType oly3000z_reg_05_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, "F2.2" },
	{ { 2 }, "F2.5" },
	{ { 3 }, "F2.8" },
	{ { 4 }, "F3.2" },
	{ { 5 }, "F3.6" },
	{ { 6 }, "F4.0" },
	{ { 7 }, "F4.5" },
	{ { 8 }, "F5.0" },
	{ { 9 }, "F5.6" },
	{ { 10 }, "F6.3" },
	{ { 11 }, "F7.0" },
	{ { 12 }, "F8.0" },
	{ { 13 }, "F9.0" },
	{ { 14 }, "F10.0" },
	{ { 15 }, "F11.0" },
	{ { 16 }, "F2.0" }, /* Odd */
};
static const RegisterDescriptorType oly3000z_reg_05[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"aperture", N_("Aperture Settings"),
		VAL_NAME_INIT (oly3000z_reg_05_val_names)
	}
};

/*
 * Olympus 750uz Register 5: aperture settings.
 */
static const ValueNameType oly750uz_reg_05_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 3 }, "F2.8" },
	{ { 4 }, "F3.2" },
	{ { 5 }, "F3.5" },
	{ { 6 }, "F4.0" },
	{ { 7 }, "F4.5" },
	{ { 8 }, "F5.0" },
	{ { 9 }, "F5.6" },
	{ { 10 }, "F6.3" },
	{ { 11 }, "F7.0" },
	{ { 12 }, "F8.0" },
};
static const RegisterDescriptorType oly750uz_reg_05[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"aperture", N_("Aperture Settings"),
		VAL_NAME_INIT (oly750uz_reg_05_val_names)
	}
};

/*
 * Olympus SP-500uz Register 5: aperture settings.
 */
#if 0
static const ValueNameType olysp500uz_reg_05_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 28 }, "F2.8" },
	{ { 32 }, "F3.2" },
	{ { 35 }, "F3.5" },
	{ { 40 }, "F4.0" },
	{ { 45 }, "F4.5" },
	{ { 50 }, "F5.0" },
	{ { 56 }, "F5.6" },
	{ { 63 }, "F6.3" },
	{ { 80 }, "F8.0" },
	{ { 90 }, "F9.0" },
};
static const RegisterDescriptorType olysp500uz_reg_05[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"aperture", N_("Aperture Settings"),
		VAL_NAME_INIT (olysp500uz_reg_05_val_names)
	}
};
#endif

/*
 * Register 6: color mode
 */
static const ValueNameType oly3040_reg_06_val_names[] = {
	{ { 0 }, N_("Normal") },
	{ { 1 }, N_("B/W") },
	{ { 2 }, N_("Sepia") },
	{ { 3 }, N_("White board") },
	{ { 4 }, N_("Black board") },
};
static const RegisterDescriptorType oly3040_reg_06[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"color", N_("Color or Function Mode"),
		VAL_NAME_INIT (oly3040_reg_06_val_names)
	}
};

/*
 * Olyumpus 3040 Register 7: flash settings
 */
static const ValueNameType oly3040_reg_07_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, N_("Force") },
	{ { 2 }, N_("Off") },
	{ { 3 }, N_("Anti-redeye") },
};
static const RegisterDescriptorType oly3040_reg_07[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"flash", N_("Flash Settings"),
		VAL_NAME_INIT (oly3040_reg_07_val_names)
	}
};

/*
 * Olumpus 750uz Register 7: flash settings
 */
static const ValueNameType oly750uz_reg_07_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, N_("Force") },
	{ { 2 }, N_("Off") },
	{ { 3 }, N_("Anti-redeye") },
	{ { 4 }, N_("Slow") },
	/* This is always 4 for slow 1, 2, or slow with redeye */
};
static const RegisterDescriptorType oly750uz_reg_07[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"flash", N_("Flash Settings"),
		VAL_NAME_INIT (oly750uz_reg_07_val_names)
	}
};

/*
 * Olumpus SP-500uz Register 7: flash settings
 */
static const ValueNameType olysp500uz_reg_07_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, N_("Force") },
	{ { 2 }, N_("Off") },
	{ { 3 }, N_("Anti-redeye") },
	{ { 4 }, N_("Slow") },
	/* This is always 4 for slow 1, 2, or slow with redeye */
	{ { 6 }, N_("Anti-redeye Fill") },
};

static const RegisterDescriptorType olysp500uz_reg_07[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"flash", N_("Flash Settings"),
		VAL_NAME_INIT (olysp500uz_reg_07_val_names)
	}
};

/*
 * Olympus 3040: Register 20: white balance.
 */
static const ValueNameType oly3040_reg_20_val_names[] = {
	{ { 0x00 }, N_("Auto") },
	{ { 0x01 }, N_("Daylight") },
	{ { 0x02 }, N_("Fluorescent") },
	{ { 0x03 }, N_("Tungsten") },
	{ { 0xff }, N_("Cloudy") },
};
static const RegisterDescriptorType oly3040_reg_20[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"whitebalance", N_("White Balance"),
		VAL_NAME_INIT (oly3040_reg_20_val_names)
	}
};

/*
 * Olympus 3000z: Register 20: white balance. Only difference seems to be
 * that cloudy is 0x04, not 0xff.
 */
static const ValueNameType oly3000z_reg_20_val_names[] = {
	{ { 0x00 }, N_("Auto") },
	{ { 0x01 }, N_("Daylight") },
	{ { 0x02 }, N_("Fluorescent") },
	{ { 0x03 }, N_("Tungsten") },
	{ { 0x04 }, N_("Cloudy") },
};
static const RegisterDescriptorType oly3000z_reg_20[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"whitebalance", N_("White Balance"),
		VAL_NAME_INIT (oly3000z_reg_20_val_names)
	}
};

/*
 * Olympus 750uz: Register 20: white balance. The main difference is that
 * we have the fluorescent 1 2 and 3:
 */
static const ValueNameType oly750uz_reg_20_val_names[] = {
	{ { 0x00 }, N_("Auto") },
	{ { 0x01 }, N_("Daylight") },
	{ { 0x03 }, N_("Tungsten") },
	{ { 0x04 }, N_("Fluorescent-1-home-6700K") },
	{ { 0x05 }, N_("Fluorescent-2-desk-5000K") },
	{ { 0x06 }, N_("Fluorescent-3-office-4200K") },
	{ { 0xff }, N_("Cloudy") },
};
static const RegisterDescriptorType oly750uz_reg_20[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"whitebalance", N_("White Balance"),
		VAL_NAME_INIT (oly750uz_reg_20_val_names)
	}
};

/*
 * Olympus SP 500uz: Register 20: white balance. The main difference is that
 * we have the fluorescent 1 2 and 3:
 */
static const ValueNameType olysp500uz_reg_20_val_names[] = {
	{ { 0x00 }, N_("Auto") },
	{ { 0x01 }, N_("Daylight") },
	{ { 0x03 }, N_("Tungsten") },
	{ { 0x04 }, N_("Fluorescent-1-home-6700K") },
	{ { 0x05 }, N_("Fluorescent-2-desk-5000K") },
	{ { 0x06 }, N_("Fluorescent-3-office-4200K") },
	{ { 0x09 }, N_("Dusk") },
	{ { 0x0a }, N_("Preset") },
	{ { 0xff }, N_("Cloudy") },
};

static const RegisterDescriptorType olysp500uz_reg_20[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"whitebalance", N_("White Balance"),
		VAL_NAME_INIT (olysp500uz_reg_20_val_names)
	}
};

/*
 * Register 33: focus mode.
 */
static const ValueNameType oly3040_reg_33_val_names[] = {
	{ { 0x01 }, N_("Macro") },
	{ { 0x02 }, N_("Auto") },
	{ { 0x03 }, N_("Manual") },
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
 * Note: Always switch off LCD before disconnecting the camera. Otherwise the camera
 * controls won't work, and you will need to replug the cable and switch off the LCD.
 * (This _is_ a documented "feature".)
 */

static const ValueNameType oly3040_reg_34_val_names[] = {
	{ { 0x01 }, N_("Off") },
	{ { 0x02 }, N_("Monitor") },
	{ { 0x03 }, N_("Normal") },
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
	{ { .range = { 0, 7 } }, NULL },
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
	{ { .range = { 30, 600, 30  } }, NULL },
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
	{ { .range = { 30, 600, 30  } }, NULL },
};
static const RegisterDescriptorType oly3040_reg_23[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"host-power-save", N_("Host power save (seconds)"),
		VAL_NAME_INIT (oly3040_reg_23_val_names)
	}
};
/*
 * Register 38: lcd auto shut off time
 */
static const ValueNameType oly3040_reg_38_val_names[] = {
	{ { .range = { 30, 600, 30  } }, NULL },
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
	{ { 0x03 }, N_("English") },
	{ { 0x04 }, N_("French") },
	{ { 0x05 }, N_("German") },
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
	{ { 0x03 }, N_("Spot") },
	{ { 0x05 }, N_("Matrix") },
};
static const RegisterDescriptorType oly3040_reg_70[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"exp-meter", N_("Exposure Metering"),
		VAL_NAME_INIT (oly3040_reg_70_val_names)
	}
};

/*
 * Olympus SP-500uz Register 70: exposure meter.
 */
static const ValueNameType olysp500uz_reg_70_val_names[] = {
	{ { 0x02 }, N_("Center Weighted") },
	{ { 0x03 }, N_("Spot") },
	{ { 0x04 }, N_("ESP") },
	{ { 0x05 }, N_("Matrix") },
};

static const RegisterDescriptorType olysp500uz_reg_70[] = {
	{
		GP_WIDGET_RADIO, GP_REG_NO_MASK,
		"exp-meter", N_("Exposure Metering"),
		VAL_NAME_INIT (olysp500uz_reg_70_val_names)
	}
};

/*
 * Oly 750 uz Register 71: optical zoom value.
 */
static const ValueNameType oly750uz_reg_71_val_names[] = {
	{
		{ .range = { 6.3, 63.0, .1 } }, NULL
	}
};
static const RegisterDescriptorType oly750uz_reg_71[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"zoom", N_("Zoom (in millimeters)"),
		VAL_NAME_INIT (oly750uz_reg_71_val_names)
	}
};

/*
 * Oly SP-500 uz Register 71: optical zoom value.
 */
#if 0
static const ValueNameType olysp500uz_reg_71_val_names[] = {
	{
		{ .range = { 6.3, 63.0, .3 } }, NULL
	}
};
static const RegisterDescriptorType olysp500uz_reg_71[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"zoom", N_("Zoom (in millimeters)"),
		VAL_NAME_INIT (olysp500uz_reg_71_val_names)
	}
};
#endif

/*
 * Oly 3040 Register 71: optical zoom value.
 */
static const ValueNameType oly3040_reg_71_val_names[] = {
	{
		{ .range = { 7.3, 21.0, .1 } }, NULL
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
	{ { 0x0000 }, N_("Off") },
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
 * Register 85: ISO speed, read only on some cameras
 */
static const ValueNameType oly3040_reg_85_val_names[] = {
	{ { 0 }, N_("Auto") },
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
 * Olympus 750uz Register 85: ISO speed, writable
 */
static const ValueNameType oly750uz_reg_85_val_names[] = {
	{ { 0 }, N_("Auto") },
	{ { 1 }, "100" },
	{ { 2 }, "200" },
	{ { 3 }, "400" },
	{ { 4 }, "50" },
};
static const RegisterDescriptorType oly750uz_reg_85[] = {
	{
		GP_WIDGET_MENU, GP_REG_NO_MASK,
		"iso", N_("ISO Speed"),
		VAL_NAME_INIT (oly750uz_reg_85_val_names)
	}
};

/*
 * Register 103: focus position (only applicable in manual focus mode)
 *     1 to 120: macro positions
 *   121 to 240: normal positions
 */
static const ValueNameType oly3040_reg_103_val_names[] = {
	{ { .range = { 1, 240, 1 } }, NULL },
};
static const RegisterDescriptorType oly3040_reg_103[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"focus-pos", N_("Focus position"),
		VAL_NAME_INIT (oly3040_reg_103_val_names)
	}
};

/*
 * Olyumpus 750uz Register 103: focus position
 *     27 to 120: if in macro mode
 *      0 to 120: non-macro mode
 */
static const ValueNameType oly750uz_reg_103_val_names[] = {
	{ { .range = { 1, 120, 1 } }, NULL },
};
static const RegisterDescriptorType oly750uz_reg_103[] = {
	{
		GP_WIDGET_RANGE, GP_REG_NO_MASK,
		"focus-pos", N_("Focus position"),
		VAL_NAME_INIT (oly750uz_reg_103_val_names)
	}
};

/*
 * Register 41: time format, read only
 */
static const ValueNameType oly3040_reg_41_val_names[] = {
	{ { 0 }, N_("Off") },
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
 * Olympus 3040: All of the register used to modify picture settings. The
 * register value received from the camera is stored into this data area,
 * so it cannot be a const.
 */
static CameraRegisterType oly3040_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (oly3040, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (oly3040, 03, 4, CAM_DESC_DEFAULT, 0), /* shutter */
	CAM_REG_TYPE_INIT (oly3040, 05, 4, CAM_DESC_DEFAULT, 0), /* aperture (f-stop) */
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
 * Olympus 3000z: All of the register used to modify picture settings.
 * Note that most of these are the same as the 3040.
 */
static CameraRegisterType oly3000z_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (oly3040, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (oly3040, 03, 4, CAM_DESC_DEFAULT, 0), /* shutter */
	CAM_REG_TYPE_INIT (oly3000z, 05, 4, CAM_DESC_DEFAULT, 0), /* aperture (f-stop) */
	CAM_REG_TYPE_INIT (oly3040, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (oly3040, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (oly3000z, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
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
static CameraRegisterType oly3040_cam_regs[] = {
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

/*
 * Olympus 750UZ: All of the register used to modify picture settings.
 */
static CameraRegisterType oly750uz_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (oly750uz, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (oly750uz, 03, 4, CAM_DESC_DEFAULT, 0), /* shutter */
	CAM_REG_TYPE_INIT (oly750uz, 05, 4, CAM_DESC_DEFAULT, 0), /* aperture (f-stop) */
	CAM_REG_TYPE_INIT (oly3040, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (oly750uz, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (oly750uz, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
	CAM_REG_TYPE_INIT (oly3040, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
	/*
	 * Note: in "super macro" focus mode, the camera can only auto
	 * focus. Could not figure out how to set or get super macro mode.
	 */
	CAM_REG_TYPE_INIT (oly750uz,103, 4, CAM_DESC_DEFAULT, 0), /* focus position */
	CAM_REG_TYPE_INIT (oly3040, 69, 8, CAM_DESC_DEFAULT, 0), /* exposure compensation */
	CAM_REG_TYPE_INIT (oly3040, 70, 4, CAM_DESC_DEFAULT, 0), /* exposure metering */
	/*
	 * Could not figure out how or if mulit-metering can be set for
	 * the exposure metering.
	 */
	CAM_REG_TYPE_INIT (oly750uz, 71, 8, CAM_DESC_DEFAULT, 0), /* optical zoom */
	CAM_REG_TYPE_INIT (oly3040, 72, 4, CAM_DESC_DEFAULT, 0), /* digital zoom + lense +  AE lock */
	CAM_REG_TYPE_INIT (oly750uz, 85, 4, CAM_DESC_DEFAULT, 0), /* ISO Speed, read only  */
};

/*
 * All of the register used to modify camera settings.
 *
 * All the same as 3040.
 */
static CameraRegisterType oly750uz_cam_regs[] = {
	CAM_REG_TYPE_INIT (oly3040, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (oly3040, 34, 4, CAM_DESC_SUBACTION,
			  SIERRA_ACTION_LCD_MODE), /* lcd mode */
	CAM_REG_TYPE_INIT (oly3040, 35, 4, CAM_DESC_DEFAULT, 0), /* LCD brightness */
	CAM_REG_TYPE_INIT (oly3040, 38, 4, CAM_DESC_DEFAULT, 0), /* LCD auto shutoff */
	CAM_REG_TYPE_INIT (oly3040, 24, 4, CAM_DESC_DEFAULT, 0), /* Camera power save */
	CAM_REG_TYPE_INIT (oly3040, 23, 4, CAM_DESC_DEFAULT, 0), /* Host power save */
	CAM_REG_TYPE_INIT (oly3040, 41, 4, CAM_DESC_DEFAULT, 0), /* time format, read only */
};

/*
 * Olympus SP-500UZ: All of the register used to modify picture settings.
 */
static CameraRegisterType olysp500uz_pic_regs[] =  {
	/* camera prefix, register number, size of register */
	CAM_REG_TYPE_INIT (olysp500uz, 01, 4, CAM_DESC_DEFAULT, 0), /* resolution/size */
	CAM_REG_TYPE_INIT (olysp500uz, 03, 4, CAM_DESC_DEFAULT, 0), /* shutter */
	CAM_REG_TYPE_INIT (oly750uz, 05, 4, CAM_DESC_DEFAULT, 0), /* aperture (f-stop) */
	CAM_REG_TYPE_INIT (oly3040, 06, 4, CAM_DESC_DEFAULT, 0), /* color mode */
	CAM_REG_TYPE_INIT (olysp500uz, 07, 4, CAM_DESC_DEFAULT, 0), /* flash */
	CAM_REG_TYPE_INIT (olysp500uz, 20, 4, CAM_DESC_DEFAULT, 0), /* white balance */
	CAM_REG_TYPE_INIT (oly3040, 33, 4, CAM_DESC_DEFAULT, 0), /* focus mode */
	/*
	 * Note: in "super macro" focus mode, the camera can only auto
	 * focus. Could not figure out how to set or get super macro mode.
	 */
	CAM_REG_TYPE_INIT (oly3040,103, 4, CAM_DESC_DEFAULT, 0), /* focus position */
	CAM_REG_TYPE_INIT (oly3040, 69, 8, CAM_DESC_DEFAULT, 0), /* exposure compensation */
	CAM_REG_TYPE_INIT (olysp500uz, 70, 4, CAM_DESC_DEFAULT, 0), /* exposure metering */
	/*
	 * Could not figure out how or if mulit-metering can be set for
	 * the exposure metering.
	 */
	CAM_REG_TYPE_INIT (oly750uz, 71, 8, CAM_DESC_DEFAULT, 0), /* optical zoom */
	CAM_REG_TYPE_INIT (oly3040, 72, 4, CAM_DESC_DEFAULT, 0), /* digital zoom + lense +  AE lock */
	CAM_REG_TYPE_INIT (oly750uz, 85, 4, CAM_DESC_DEFAULT, 0), /* ISO Speed, read only  */
};

/*
 * All of the register used to modify camera settings.
 *
 * All the same as 3040.
 */
static CameraRegisterType olysp500uz_cam_regs[] = {
	CAM_REG_TYPE_INIT (oly3040, 02, 4, CAM_DESC_DEFAULT, 0), /* date-time */
	CAM_REG_TYPE_INIT (oly3040, 34, 4, CAM_DESC_SUBACTION,
			  SIERRA_ACTION_LCD_MODE), /* lcd mode */
	CAM_REG_TYPE_INIT (oly3040, 35, 4, CAM_DESC_DEFAULT, 0), /* LCD brightness */
	CAM_REG_TYPE_INIT (oly3040, 38, 4, CAM_DESC_DEFAULT, 0), /* LCD auto shutoff */
	CAM_REG_TYPE_INIT (oly3040, 24, 4, CAM_DESC_DEFAULT, 0), /* Camera power save */
	CAM_REG_TYPE_INIT (oly3040, 23, 4, CAM_DESC_DEFAULT, 0), /* Host power save */
	CAM_REG_TYPE_INIT (oly3040, 41, 4, CAM_DESC_DEFAULT, 0), /* time format, read only */
};

static const CameraRegisterSetType olysp500uz_desc[] = {
		{
			N_("Picture Settings"),
			SIZE_ADDR (CameraRegisterType, olysp500uz_pic_regs)
		},
		{
			N_("Camera Settings"),
			SIZE_ADDR (CameraRegisterType, olysp500uz_cam_regs)
		},
};

static const CameraRegisterSetType oly750uz_desc[] = {
		{
			N_("Picture Settings"),
			SIZE_ADDR (CameraRegisterType, oly750uz_pic_regs)
		},
		{
			N_("Camera Settings"),
			SIZE_ADDR (CameraRegisterType, oly750uz_cam_regs)
		},
};
static const CameraRegisterSetType oly3000z_desc[] = {
		{
			N_("Picture Settings"),
			SIZE_ADDR (CameraRegisterType, oly3000z_pic_regs)
		},
		{
			/*
			 * Assumed that these are all the same as the 3040
			 */
			N_("Camera Settings"),
			SIZE_ADDR (CameraRegisterType, oly3040_cam_regs)
		},
};

static const char oly3040_manual[] =
N_(
"Some notes about Olympus cameras:\n"
"(1) Camera Configuration:\n"
"    A zero value will take the default one (auto).\n"
"(2) Olympus C-3040Z (and possibly also the C-2040Z\n"
"    and others) have a USB PC Control mode. To switch\n"
"    into 'USB PC control mode', turn on the camera, open\n"
"    the memory card access door and then press and\n"
"    hold both of the menu and LCD buttons until the\n"
"    camera control menu appears. Set it to ON.\n"
"(3) If you switch the 'LCD mode' to 'Monitor' or\n"
"    'Normal', don't forget to switch it back to 'Off'\n"
"    before disconnecting. Otherwise you can't use\n"
"    the camera buttons. If you end up in this\n"
"    state, you should reconnect the camera to the\n"
"    PC, then switch LCD to 'Off'."
);

static const char oly750uz_manual[] =
N_(
"Olympus 750 Ultra Zoom:\n"
"(1) Olympus 750UZ has a USB PC Control mode. To switch\n"
"    into 'USB PC control mode', turn on the camera, open\n"
"    the memory card access door and then press and\n"
"    hold both the 'OK' and 'quickview' buttons until the\n"
"    camera control menu appears. Set it to control mode.\n"
"(2) If you switch the 'LCD mode' to 'Monitor' or\n"
"    'Normal', don't forget to switch it back to 'Off'\n"
"    before disconnecting. Otherwise you can't use\n"
"    the camera buttons. If you end up in this\n"
"    state, you should reconnect the camera to the\n"
"    PC, then switch LCD to 'Off'."
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
"    please contact the developer mailing list.\n"
);
const CameraDescType olysp500uz_cam_desc = { olysp500uz_desc, oly750uz_manual,
	SIERRA_EXT_PROTO, };
const CameraDescType oly750uz_cam_desc = { oly750uz_desc, oly750uz_manual,
	SIERRA_EXT_PROTO, };
const CameraDescType oly3040_cam_desc = { oly3040_desc, oly3040_manual,
	SIERRA_EXT_PROTO, };
const CameraDescType oly3000z_cam_desc = { oly3000z_desc, oly3040_manual,
	SIERRA_EXT_PROTO, };
const CameraDescType sierra_default_cam_desc = { oly3040_desc, default_manual,
	0, };
