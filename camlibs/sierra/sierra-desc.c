/* sierra-desc.c:
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
#include "config.h"

#include <stdlib.h>
#include <_stdint.h>
#include <stdio.h>
#include <string.h>

#include <math.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "sierra.h"
#include "sierra-desc.h"
#include "library.h"

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

#define GP_MODULE "sierra"

/*
 * For some reason, round() does not get a prototype from math.h.
 */
extern double round(double x);

static int 
camera_cam_desc_get_value (ValueNameType *val_name_p, CameraWidgetType widge, 
			   uint32_t reg_len, void *buff, int mask, 
			   CameraWidget *child)
{
	if ((widge == GP_WIDGET_RADIO) || (widge == GP_WIDGET_MENU)) {
		gp_widget_add_choice (child, _(val_name_p->name));
		GP_DEBUG ("get value %15s:\t%lld (0x%04llx)", 
			  val_name_p->name, (long long)val_name_p->u.value,
			  (long long unsigned) val_name_p->u.value);
		/* XXX where is endian handled? */
		if ((mask & *(int*) buff) == val_name_p->u.value) {
			gp_widget_set_value (child, _(val_name_p->name));
		}
	} else if (widge == GP_WIDGET_DATE) {
		GP_DEBUG ("get value date/time %s", ctime((time_t *) buff));
		gp_widget_set_value (child, (int*) buff);
	} else if (widge == GP_WIDGET_RANGE) {
		float float_val, increment;

		increment = val_name_p->u.range[2];
		if (increment == 0.0) {
			/* 
			 * Use a default value.
			 */
			increment = 1.0;
		}
		GP_DEBUG ("get value range:\t%08g:%08g increment %g (via %g)", 
		       val_name_p->u.range[0], val_name_p->u.range[1],
		       increment, val_name_p->u.range[2]);
		gp_widget_set_range (child, val_name_p->u.range[0], 
				     val_name_p->u.range[1], increment);
		/*
		 * Kluge: store the value in buff as a 4 byte int, even
		 * though we might have read 8 bytes or more.
		 */
		float_val = (*(int*) buff) * increment;
		gp_widget_set_value (child, &float_val);
	} else if (widge == GP_WIDGET_BUTTON) {
		GP_DEBUG ("get button");
		gp_widget_set_value (child, val_name_p->u.callback);
	} else {
		GP_DEBUG ("get value bad widget type %d", widge);
		return (GP_ERROR);
	}

	return (GP_OK);
}

static int
camera_cam_desc_get_widget (Camera *camera, CameraRegisterType *reg_p, 
			    CameraWidget *section, GPContext *context)
{
	int ind, vind, ret, value;
	int mask;
	char buff[1024];
	CameraWidget *child;
	RegisterDescriptorType *reg_desc_p;

	GP_DEBUG ("register %d", reg_p->reg_number);
	if (reg_p->reg_len == 0) {
		/*
		 * This is 0 for GP_WIDGET_BUTTON (callbacks), since is no
		 * register for call backs. Frontends "get" the value, and
		 * call the function directly.
		 */
		ret = GP_OK;
	} else if (reg_p->reg_len == 4) {
		int rval;
		ret = sierra_get_int_register (camera, reg_p->reg_number,
					       &rval,
					       context);
		reg_p->reg_value = rval;
	} else if (reg_p->reg_len == 8) {
		/*
		 * reg_value is 8 bytes maximum. If you need a bigger
		 * value, change the reg_value size, or allocate space on
		 * the fly and make a union with reg_value and a void*.
		 */
		ret = sierra_get_string_register (camera, reg_p->reg_number, 
					  -1, NULL, (unsigned char *)buff, (unsigned int *)&value, context);
		if ((ret == GP_OK) && value != reg_p->reg_len) {
			GP_DEBUG ("Bad length result %d", value);
			return (GP_ERROR);
		}
		memcpy (&reg_p->reg_value, buff, reg_p->reg_len);
	} else {
		GP_DEBUG ("Bad register length %d", reg_p->reg_number);
		return (GP_ERROR);
	}
	GP_DEBUG ("... '%s'.", gp_result_as_string (ret));
        if (ret < 0) {
		return (ret);
	}

	for (ind = 0; ind < reg_p->reg_desc_cnt; ind++) {
		reg_desc_p = &reg_p->reg_desc[ind];
		mask = reg_desc_p->regs_mask;
		GP_DEBUG ("window name is %s", reg_desc_p->regs_long_name);
		gp_widget_new (reg_desc_p->reg_widget_type, 
			       _(reg_desc_p->regs_long_name), &child);
		gp_widget_set_name (child, reg_desc_p->regs_short_name);
		/*
		 * Setting the info for the preference settings does not
		 * make sense like it does for an icon button. This is
		 * used as the tool-tip field (mouse over hint that pops
		 * up after a second in gtkam).  We don't want this used
		 * at all; setting it to space doesn't work well, so just
		 * set it to the same as regs_long_name.
		 */
		gp_widget_set_info (child,  _(reg_desc_p->regs_long_name));
		GP_DEBUG ("reg_value 0x%016llx", (long long unsigned)reg_p->reg_value);
		for (vind = 0; vind < reg_desc_p->reg_val_name_cnt; vind++) {
			camera_cam_desc_get_value (&reg_desc_p->regs_value_names[vind],
				reg_desc_p->reg_widget_type, reg_p->reg_len,
				(char*) &reg_p->reg_value, mask, child);
		}
		/*
		 * For radio and menu values: if there has been no change, it
		 * means the value was not set, and so it is unknown.
		 */
		if (((reg_desc_p->reg_widget_type == GP_WIDGET_RADIO) || 
		     (reg_desc_p->reg_widget_type == GP_WIDGET_MENU)) && 
		      !gp_widget_changed (child)) {
			sprintf (buff, _("%lld (unknown)"), (long long)reg_p->reg_value);
			gp_widget_add_choice (child, buff);
			gp_widget_set_value (child, buff);
		}
		gp_widget_append (section, child);
	}
	return (GP_OK);
}

int
camera_get_config_cam_desc (Camera *camera, CameraWidget **window,
			    GPContext *context)
{
	CameraWidget *section;
	int indw, indr;
	const CameraDescType *cam_desc = NULL;

	GP_DEBUG ("*** camera_get_config_cam_desc");
	CHECK (camera_start (camera, context));
	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	cam_desc = camera->pl->cam_desc;
	for (indw = 0; indw < 2 /* XXX sizeof () */; indw++) {
		GP_DEBUG ("%s registers", cam_desc->regset[indw].window_name);
		gp_widget_new (GP_WIDGET_SECTION,
			       _(cam_desc->regset[indw].window_name), &section);
		gp_widget_append (*window, section);
		for (indr = 0; indr < cam_desc->regset[indw].reg_cnt;  indr++) {
			camera_cam_desc_get_widget (camera,
			      &cam_desc->regset[indw].regs[indr], section,
			      context);
		}
	}
	return (GP_OK);
}

static int
cam_desc_set_register (Camera *camera, CameraRegisterType *reg_p,
		       void *value, GPContext *context)
{
	if (reg_p->reg_get_set.method == CAM_DESC_DEFAULT) {
		if (reg_p->reg_len == 4) {
			/* XXX endian problem or not? */
			CHECK_STOP (camera, sierra_set_int_register
				(camera, reg_p->reg_number,
				*(int*)value, context));
		} else if (reg_p->reg_len <= 8) {
			CHECK_STOP (camera, sierra_set_string_register (camera,
					reg_p->reg_number, (char*) value,
					reg_p->reg_len, context));
		} else {
			/*
			 * Note that sizeof u.value is 8, so if you have a
			 * larger value to store, somehow change u.value
			 * size, or add a union.
			 */
			GP_DEBUG ("set value BAD LENGTH %d", reg_p->reg_len);
			return (GP_ERROR);
		}
	} else if (reg_p->reg_get_set.method == CAM_DESC_SUBACTION) {
		/*
		 * Right now, the only supported sub-action is for
		 * changing the LCD mode, the only other possible
		 * sub-actions are for testing, calibration, and
		 * protection mode. Just make sure the LCD mode setting
		 * works.
		 */
		CHECK_STOP (camera, sierra_sub_action (camera, 
			reg_p->reg_get_set.action, *(int*) value,
			context));
	} else {
		GP_DEBUG ("Unsupported register setting action %d",
			reg_p->reg_get_set.method);
		return (GP_ERROR);
	}
	return (GP_OK);
}

static int 
camera_cam_desc_set_value (Camera *camera, CameraRegisterType *reg_p,
			   RegisterDescriptorType *reg_desc_p,
			   ValueNameType *val_name_p, void *data,
			   GPContext *context)
{
	/*
	 * Return GP_OK if match and OK, 1 if no match, or < 0 on error.
	 */
	if ((reg_desc_p->reg_widget_type == GP_WIDGET_RADIO) || 
	    (reg_desc_p->reg_widget_type == GP_WIDGET_MENU)) {
		uint32_t new_value;

		GP_DEBUG ("set value comparing data '%s' with name '%s'", *(char**)data,
			  val_name_p->name);
		if (strcmp (*(char**)data, val_name_p->name) != 0) {
			return (1);
		}
		/*
		 * For masking, we might actually set the same register
		 * twice - once for each change in the mask, so we have to
		 * reset reg_value to the new value.
		 * 
		 * If we must to mask u.value, that is really a bug - the
		 * u.value should already be masked.
		 *
		 * For registers longer than 4 bytes, we are masking
		 * in whatever values were already in the upper 4 bytes -
		 * none of the current settings (new values) are longer
		 * than 4 bytes.
		 */
		new_value = (reg_p->reg_value & ~reg_desc_p->regs_mask) |
			(val_name_p->u.value & reg_desc_p->regs_mask);
		reg_p->reg_value = new_value;
		GP_DEBUG ("set new val 0x%x; reg val 0x%x; msk 0x%x; val 0x%x ",
			  new_value, (int)reg_p->reg_value,
			  reg_desc_p->regs_mask, (int)val_name_p->u.value);
		CHECK_STOP (camera, cam_desc_set_register (camera, reg_p,
				&new_value, context));
	} else if (reg_desc_p->reg_widget_type == GP_WIDGET_DATE) {
		GP_DEBUG ("set new date/time %s", ctime((time_t*)data));
		CHECK_STOP (camera, cam_desc_set_register (camera, reg_p,
				(int*)data, context));
	} else if (reg_desc_p->reg_widget_type == GP_WIDGET_RANGE) {
		int val[2];
		float increment;

		if (reg_p->reg_get_set.method != CAM_DESC_DEFAULT) {
			GP_DEBUG ("Setting range values using the non-default"
				  " register functions is not supported");
			return (GP_ERROR);
		}
		/*
		 * The value in *data is a float, convert it to an int.
		 *
		 * Currently no need to support any subaction here.
		 *
		 * Similiar to the masking above, if we have a register
		 * larger than 4 bytes, put the four bytes in the int to
		 * into the low four bytes of the value we want to set;
		 * fill the rest with whatever was read from the camera.
		 */
		increment = val_name_p->u.range[2];
		if (increment == 0.0) {
			increment = 1.0; /* default value */
		}
		GP_DEBUG ("set value range from %g inc %g", *(float*)data,
			  increment);
		val[0] = round ((*(float*) data) / increment);
		if (reg_p->reg_len == 4) {
			/* 
			 * not used but set it anyway 
			 */
			val[1] = 0;
		} else if (reg_p->reg_len == 8) {
			memcpy(&val[1], ((char*)&reg_p->reg_value)+sizeof(val[0]), sizeof(val[0]));
			/* val[1] = ((int *) &reg_p->reg_value)[1]; */
		} else if (reg_p->reg_len != 4 ) {
			GP_DEBUG ("Unsupported range with register length %d",
				  reg_p->reg_len);
			return (GP_ERROR);
		}
		GP_DEBUG ("set value range to %d (0x%x and 0x%x)", val[0],
			  val[0], val[1]);
		CHECK_STOP (camera, cam_desc_set_register (camera, reg_p, 
							   &val, context));
	} else {
		GP_DEBUG ("bad reg_widget_type type %d", 
			reg_desc_p->reg_widget_type);
		return (GP_ERROR);
	}
	return (GP_OK);
}

static int
camera_cam_desc_set_widget (Camera *camera, CameraRegisterType *reg_p,
			    CameraWidget *window, GPContext *context)
{
	int ind, vind, ret;
	union value_in {
		char *charp;
		int val;
		float flt;
	} value_in;
	CameraWidget *child;
	RegisterDescriptorType *reg_desc_p;

	GP_DEBUG ("register %d", reg_p->reg_number);

	for (ind = 0; ind < reg_p->reg_desc_cnt; ind++) {
		reg_desc_p = &reg_p->reg_desc[ind];
		GP_DEBUG ("window name is %s", reg_desc_p->regs_long_name);

		if ((gp_widget_get_child_by_label (window,
		     _(reg_desc_p->regs_long_name), &child) >= 0) &&
		     gp_widget_changed (child)) {
			gp_widget_get_value (child, &value_in);
			for (vind = 0; vind < reg_desc_p->reg_val_name_cnt;
			     vind++) {
				ret = camera_cam_desc_set_value (camera, reg_p,
					reg_desc_p,
					&reg_desc_p->regs_value_names[vind],
					&value_in, context);
				if (ret == GP_OK) {
					/*
					 * Something got set, mark the
					 * widget as changed, so the user
					 * can repeat any actions  - like
					 * going to the "next" picture by
					 * repeatedly hitting apply while
					 * in operation play mode.
					 *
					 * This has the side affect of
					 * changing whatever was just set
					 * twice when using the apply in
					 * gtkam - once when applied and
					 * once when OK.
					 */
					gp_widget_set_changed (child, 1);

				}
				if (ret <= 0) {
					/*
					 * Value was set (GP_OK is 0), or
					 * some an error occurred (< 0),
					 * don't bother checking any other
					 * value/name pairs.
					 */
					break;
				}
			}
		}
	}
	return (GP_OK);
}

int 
camera_set_config_cam_desc (Camera *camera, CameraWidget *window,
			    GPContext *context)
{
	int wind, rind;
	const CameraDescType *cam_desc = NULL;

	GP_DEBUG ("*** camera_set_config_cam_desc");
	CHECK (camera_start (camera, context));

	cam_desc = camera->pl->cam_desc;
	for (wind = 0; wind < 2 /* XXX sizeof () */; wind++) {
		GP_DEBUG ("%s registers", cam_desc->regset[wind].window_name);
		for (rind = 0; rind < cam_desc->regset[wind].reg_cnt;  rind++) {
			camera_cam_desc_set_widget (camera,
			      &cam_desc->regset[wind].regs[rind], window,
			      context);
		}
	}
	return (GP_OK);
}
