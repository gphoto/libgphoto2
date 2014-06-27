/* sierra_desc.h:
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

/*
 * Define a set of structures to describe the register layouts of a
 * digital camera, based on the Nikon Coolpix 880 (sierra). Only camera
 * configuration/settings are handled here.
 *
 * The lowest level structs are first, followed by the higher levels.
 *
 * These structures can't be in one single struct, since they are variable
 * length, and each level has a slightly different set of functionallity.
 *
 * Generally, we have a CameraRegisterSetType with pointers to
 * CameraRegisterType registers; each register in turn has pointers to
 * RegisterDescriptorType's; each RegisterDescriptorType has pointers to
 * ValueNameType's; the ValueNameType is a value and corresponding
 * description (string).
 */

#ifndef __SIERRA_DESC_H__
#define __SIERRA_DESC_H__

#define	SIZE_ADDR(type, var)	(sizeof(var)/sizeof(type)), (type*) var

#define	VAL_NAME_INIT(name)	\
		SIZE_ADDR(ValueNameType, name)

#define	CAM_REG_TYPE_INIT(prefix, reg, rsize, get_set, action)	\
	{ reg, rsize, 0, { get_set, action }, \
		SIZE_ADDR(RegisterDescriptorType, prefix##_reg_##reg), \
	}

#define GP_REG_NO_MASK  ~0      /* XXX long or int or ? */

typedef enum {
	CAM_DESC_DEFAULT,	/* use standard sierra get/set int/string
				   functions, depending on reg_len */
	CAM_DESC_SUBACTION,	/* use standard get int/string, but use the
				   sierr_sub_action to set values */
} RegGetSetType;

typedef struct {
	RegGetSetType method;
	int	action;	/* if we are CAM_DESC_ACTION, the action to use.
			 * The sub-action is stored in the name-value
			 * field. This is probably not good enough for the
			 * standard action (calling sierra_action versus
			 * calling sierra_sub_action).
			 */
} GetSetType;

typedef struct {
	union {
		/* 
		 * The register value masked with regs_mask gives the
		 * register value to compare or use here.  Register values
		 * are always int, and must be converted to/from float for
		 * range values.  The proper value to user here is based
		 * on the reg_len size and the reg_widget_type.
		 *
		 * For fields like the exp compensation, or the zoom,
		 * where the first x bytes are used, and it looks like the
		 * rest are a fixed value: just mask them out, and use the
		 * values read from the camera.
		 */
		int64_t value;	/* GP_WIDGET_RADIO */
		float range[3];	/* GP_WIDGET_RANGE these are
				   min/max/stepping; if stepping is zero,
				   a stepping of one is used. */
		CameraWidgetCallback callback; /* GP_WIDGET_BUTTON */
	} u;
	/* 
	 * Range uses no name - the name is implicitly the value. XXX Move name
	 * up into a struct with value.
	 */
	char *name;	/* name matching the value */
} ValueNameType;

typedef struct {
	CameraWidgetType reg_widget_type;
	uint32_t regs_mask;
	char *regs_short_name;		/* for command line use */
	char *regs_long_name;		/* for gui/curses use */
	uint32_t reg_val_name_cnt;	/* number of regs_value_names entries */
	ValueNameType *regs_value_names;	/* list of reg value/names */
} RegisterDescriptorType;

/* 
 * XXX check usage of reg_value, maybe change it to char x[8], and
 * normally type cast its usage.
 */
typedef struct {
	uint32_t reg_number; 	/* register number */
	uint32_t reg_len;	/* size in bytes of data at register */
	uint64_t reg_value; 	/* stored value, has no valid initial value;
				 * The size would have to be increased, or
				 * this space must be allocated for
				 * register values longer than 8 bytes. */
	GetSetType	reg_get_set;
	uint32_t reg_desc_cnt;	/* number of reg_desc entries */
	RegisterDescriptorType *reg_desc; /* list of reg descriptors */
} CameraRegisterType;

typedef struct CameraRegisterSet {
	char *window_name;
	uint32_t reg_cnt;
	CameraRegisterType *regs;
} CameraRegisterSetType;

typedef struct CameraDesc {
	const CameraRegisterSetType *regset;
	const char * const manual;
	const SierraFlags flags;
} CameraDescType;

/*
 * oly-desc.c:
 */
extern const CameraDescType sierra_default_cam_desc;
extern const CameraDescType olysp500uz_cam_desc;
extern const CameraDescType oly750uz_cam_desc;
extern const CameraDescType oly3040_cam_desc;
extern const CameraDescType oly3000z_cam_desc;

/*
 * nikon-desc.c:
 */
extern const CameraDescType cp880_cam_desc;
extern const CameraDescType cp995_cam_desc;
extern const CameraDescType cp2500_cam_desc;
extern const CameraDescType cp4300_cam_desc;

/*
 * epson-desc.c:
 */
extern const CameraDescType ep3000z_cam_desc;

#endif /* __SIERRA_DESC_H__ */
