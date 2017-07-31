/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camera.c
 *
 * Copyright (c) 2015-2017 Marcus Meissner <marcus@jet.franken.de>
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
#include <gphoto2/gphoto2-port-library.h>
#include <gphoto2/gphoto2-port-portability.h>

#ifdef HAVE_LIBEXIF
#  include <libexif/exif-data.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <string.h>
#include <stdint.h>

#include "vcamera.h"

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-log.h>

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

#define CHECK(result) {int r=(result); if (r<0) return (r);}

static int ptp_inject_interrupt(vcamera*cam, int when, uint16_t code, int nparams, uint32_t param1, uint32_t transid);

static uint32_t get_32bit_le(unsigned char *data) {
	return	data[0]	| (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint16_t get_16bit_le(unsigned char *data) {
	return	data[0]	| (data[1] << 8);
}

static uint8_t get_8bit_le(unsigned char *data) {
	return data[0];
}

static int8_t get_i8bit_le(unsigned char *data) {
	return data[0];
}


static int put_64bit_le(unsigned char *data, uint64_t x) {
	data[0] = x & 0xff;
	data[1] = (x>>8) & 0xff;
	data[2] = (x>>16) & 0xff;
	data[3] = (x>>24) & 0xff;
	data[4] = (x>>32) & 0xff;
	data[5] = (x>>40) & 0xff;
	data[6] = (x>>48) & 0xff;
	data[7] = (x>>56) & 0xff;
	return 8;
}

static int put_32bit_le(unsigned char *data, uint32_t x) {
	data[0] = x & 0xff;
	data[1] = (x>>8) & 0xff;
	data[2] = (x>>16) & 0xff;
	data[3] = (x>>24) & 0xff;
	return 4;
}

static int put_16bit_le(unsigned char *data, uint16_t x) {
	data[0] = x & 0xff;
	data[1] = (x>>8) & 0xff;
	return 2;
}

static int put_8bit_le(unsigned char *data, uint8_t x) {
	data[0] = x;
	return 1;
}

static int put_string(unsigned char *data, char *str) {
	int i;

	if (strlen(str)>255)
		gp_log (GP_LOG_ERROR, "put_string", "string length is longer than 255 characters");

	data[0] = strlen (str);
	for (i=0;i<data[0];i++)
		put_16bit_le(data+1+2*i,str[i]);

	return 1+strlen(str)*2;
}

static char * get_string(unsigned char *data) {
	int	i, len;
	char	*x;

	len = data[0];
	x = malloc(len+1);
	x[len] = 0;

	for (i=0;i<len;i++)
		x[i] = get_16bit_le(data+1+2*i);

	return x;
}


static int put_16bit_le_array(unsigned char *data, uint16_t *arr, int cnt) {
	int x = 0, i;

	x += put_32bit_le (data,cnt);
	for (i=0;i<cnt;i++)
		x += put_16bit_le (data+x, arr[i]);
	return x;
}

static int put_32bit_le_array(unsigned char *data, uint32_t *arr, int cnt) {
	int x = 0, i;

	x += put_32bit_le (data,cnt);
	for (i=0;i<cnt;i++)
		x += put_32bit_le (data+x, arr[i]);
	return x;
}

static void
ptp_senddata(vcamera *cam, uint16_t code, unsigned char *data, int bytes) {
	unsigned char	*offset;
	int size = bytes + 12;

	if (!cam->inbulk) {
		cam->inbulk = malloc(size);
	} else {
		cam->inbulk = realloc(cam->inbulk,cam->nrinbulk+size);
	}
	offset = cam->inbulk + cam->nrinbulk;
	cam->nrinbulk += size;

	put_32bit_le(offset,size);
	put_16bit_le(offset+4,0x2);
	put_16bit_le(offset+6,code);
	put_32bit_le(offset+8,cam->seqnr);
	memcpy(offset+12,data,bytes);
}

static void
ptp_response(vcamera *cam, uint16_t code, int nparams, ...) {
	unsigned char	*offset;
	int 		i, x = 0;
	va_list		args;

	if (!cam->inbulk) {
		cam->inbulk = malloc(12 + nparams*4);
	} else {
		cam->inbulk = realloc(cam->inbulk,cam->nrinbulk+12+nparams*4);
	}
	offset = cam->inbulk + cam->nrinbulk;
	cam->nrinbulk += 12+nparams*4;
	x += put_32bit_le(offset+x,12+nparams*4);
	x += put_16bit_le(offset+x,0x3);
	x += put_16bit_le(offset+x,code);
	x += put_32bit_le(offset+x,cam->seqnr);

	va_start(args, nparams);
	for (i=0;i<nparams;i++)
		x += put_32bit_le (offset+x, va_arg(args, uint32_t));
	va_end(args);

	cam->seqnr++;
}

#define PTP_RC_OK					0x2001
#define PTP_RC_GeneralError 				0x2002
#define PTP_RC_SessionNotOpen          		 	0x2003
#define PTP_RC_OperationNotSupported    		0x2005
#define PTP_RC_InvalidStorageId				0x2008
#define PTP_RC_InvalidObjectHandle			0x2009
#define PTP_RC_DevicePropNotSupported			0x200A
#define PTP_RC_InvalidObjectFormatCode			0x200B
#define PTP_RC_StoreFull				0x200C
#define PTP_RC_ObjectWriteProtected			0x200D
#define PTP_RC_AccessDenied				0x200F
#define PTP_RC_NoThumbnailPresent			0x2010
#define PTP_RC_StoreNotAvailable			0x2013
#define PTP_RC_SpecificationByFormatUnsupported         0x2014
#define PTP_RC_InvalidParentObject			0x201A
#define PTP_RC_InvalidDevicePropFormat			0x201B
#define PTP_RC_InvalidParameter				0x201D
#define PTP_RC_SessionAlreadyOpened     		0x201E

#define CHECK_PARAM_COUNT(x)											\
	if (ptp->nparams != x) {										\
		gp_log (GP_LOG_ERROR, __FUNCTION__,"params should be %d, but is %d", x, ptp->nparams);		\
		ptp_response (cam, PTP_RC_GeneralError, 0);							\
		return 1;											\
	}

#define CHECK_SEQUENCE_NUMBER()											\
	if (ptp->seqnr != cam->seqnr) {										\
		/* not clear if normal cameras react like this */						\
		gp_log (GP_LOG_ERROR, __FUNCTION__, "seqnr %d was sent, expected was %d", ptp->seqnr, cam->seqnr);\
		ptp_response (cam,PTP_RC_GeneralError,0);							\
		return 1;												\
	}

#define CHECK_SESSION()								\
	if (!cam->session) {							\
		gp_log (GP_LOG_ERROR,__FUNCTION__,"session is not open");	\
		ptp_response(cam, PTP_RC_SessionNotOpen, 0);			\
		return 1;							\
	}

static int ptp_opensession_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_closesession_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_deviceinfo_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getnumobjects_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getobjecthandles_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getstorageids_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getstorageinfo_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getobjectinfo_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getobject_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getthumb_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_deleteobject_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getdevicepropdesc_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getdevicepropvalue_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_setdevicepropvalue_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_setdevicepropvalue_write_data(vcamera *cam, ptpcontainer *ptp, unsigned char*data, unsigned int len);
static int ptp_initiatecapture_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_vusb_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_nikon_setcontrolmode_write(vcamera *cam, ptpcontainer *ptp);

static struct ptp_function {
	int	code;
	int	(*write)(vcamera *cam, ptpcontainer *ptp);
	int	(*write_data)(vcamera *cam, ptpcontainer *ptp, unsigned char *data, unsigned int size);
} ptp_functions_generic[] = {
	{0x1001,	ptp_deviceinfo_write, 		NULL			},
	{0x1002,	ptp_opensession_write, 		NULL			},
	{0x1003,	ptp_closesession_write, 	NULL			},
	{0x1004,	ptp_getstorageids_write, 	NULL			},
	{0x1005,	ptp_getstorageinfo_write, 	NULL			},
	{0x1006,	ptp_getnumobjects_write, 	NULL			},
	{0x1007,	ptp_getobjecthandles_write, 	NULL			},
	{0x1008,	ptp_getobjectinfo_write, 	NULL			},
	{0x1009,	ptp_getobject_write, 		NULL			},
	{0x100A,	ptp_getthumb_write, 		NULL			},
	{0x100B,	ptp_deleteobject_write, 	NULL			},
	{0x100E,	ptp_initiatecapture_write, 	NULL			},
	{0x1014,	ptp_getdevicepropdesc_write, 	NULL			},
	{0x1015,	ptp_getdevicepropvalue_write, 	NULL			},
	{0x1016,	ptp_setdevicepropvalue_write, 	ptp_setdevicepropvalue_write_data	},
	{0x9999,	ptp_vusb_write, 		NULL			},
};

static struct ptp_function ptp_functions_nikon_dslr[] = {
	{0x90c2,	ptp_nikon_setcontrolmode_write, NULL			},
};

static struct ptp_map_functions {
	vcameratype		type;
	struct ptp_function	*functions;
	unsigned int		nroffunctions;
} ptp_functions[] = {
	{GENERIC_PTP,	ptp_functions_generic,		sizeof(ptp_functions_generic)/sizeof(ptp_functions_generic[0])},
	{NIKON_D750,	ptp_functions_nikon_dslr,	sizeof(ptp_functions_nikon_dslr)/sizeof(ptp_functions_nikon_dslr[0])},
};

typedef union _PTPPropertyValue {
	char            *str;   /* common string, malloced */
	uint8_t         u8;
	int8_t          i8;
	uint16_t        u16;
	int16_t         i16;
	uint32_t        u32;
	int32_t         i32;
	uint64_t        u64;
	int64_t         i64;
	/* XXXX: 128 bit signed and unsigned missing */
	struct array {
		uint32_t        count;
		union _PTPPropertyValue *v;     /* malloced, count elements */
	} a;
} PTPPropertyValue;

struct _PTPPropDescRangeForm {
        PTPPropertyValue        MinimumValue;
        PTPPropertyValue        MaximumValue;
        PTPPropertyValue        StepSize;
};
typedef struct _PTPPropDescRangeForm PTPPropDescRangeForm;

/* Property Describing Dataset, Enum Form */

struct _PTPPropDescEnumForm {
        uint16_t                NumberOfValues;
        PTPPropertyValue        *SupportedValue;        /* malloced */
};
typedef struct _PTPPropDescEnumForm PTPPropDescEnumForm;

/* Device Property Describing Dataset (DevicePropDesc) */

struct _PTPDevicePropDesc {
        uint16_t                DevicePropertyCode;
        uint16_t                DataType;
        uint8_t                 GetSet;
        PTPPropertyValue        FactoryDefaultValue;
        PTPPropertyValue        CurrentValue;
        uint8_t                 FormFlag;
        union   {
                PTPPropDescEnumForm     Enum;
                PTPPropDescRangeForm    Range;
        } FORM;
};
typedef struct _PTPDevicePropDesc PTPDevicePropDesc;

static int ptp_battery_getdesc(vcamera*,PTPDevicePropDesc*);
static int ptp_battery_getvalue(vcamera*,PTPPropertyValue*);
static int ptp_imagesize_getdesc(vcamera*,PTPDevicePropDesc*);
static int ptp_imagesize_getvalue(vcamera*,PTPPropertyValue*);
static int ptp_datetime_getdesc(vcamera*,PTPDevicePropDesc*);
static int ptp_datetime_getvalue(vcamera*,PTPPropertyValue*);
static int ptp_datetime_setvalue(vcamera*,PTPPropertyValue*);
static int ptp_shutterspeed_getdesc(vcamera*,PTPDevicePropDesc*);
static int ptp_shutterspeed_getvalue(vcamera*,PTPPropertyValue*);
static int ptp_shutterspeed_setvalue(vcamera*,PTPPropertyValue*);
static int ptp_fnumber_getdesc(vcamera*,PTPDevicePropDesc*);
static int ptp_fnumber_getvalue(vcamera*,PTPPropertyValue*);
static int ptp_fnumber_setvalue(vcamera*,PTPPropertyValue*);
static int ptp_exposurebias_getdesc(vcamera*,PTPDevicePropDesc*);
static int ptp_exposurebias_getvalue(vcamera*,PTPPropertyValue*);
static int ptp_exposurebias_setvalue(vcamera*,PTPPropertyValue*);

static struct ptp_property {
	int	code;
	int	(*getdesc )(vcamera *cam, PTPDevicePropDesc*);
	int	(*getvalue)(vcamera *cam, PTPPropertyValue*);
	int	(*setvalue)(vcamera *cam, PTPPropertyValue*);
} ptp_properties[] = {
	{0x5001,	ptp_battery_getdesc, ptp_battery_getvalue, NULL },
	{0x5003,	ptp_imagesize_getdesc, ptp_imagesize_getvalue, NULL },
	{0x5007,	ptp_fnumber_getdesc, ptp_fnumber_getvalue, ptp_fnumber_setvalue },
	{0x5010,	ptp_exposurebias_getdesc, ptp_exposurebias_getvalue, ptp_exposurebias_setvalue },
	{0x500d,	ptp_shutterspeed_getdesc, ptp_shutterspeed_getvalue, ptp_shutterspeed_setvalue },
	{0x5011,	ptp_datetime_getdesc, ptp_datetime_getvalue, ptp_datetime_setvalue },
};

struct ptp_dirent {
	uint32_t		id;
	char 			*name;
	char 			*fsname;
	struct stat		stbuf;
	struct ptp_dirent 	*parent;
	struct ptp_dirent 	*next;
};

static struct ptp_dirent *first_dirent = NULL;
static uint32_t	ptp_objectid = 0;

static void
read_directories(char *path, struct ptp_dirent *parent) {
	struct ptp_dirent	*cur;
	gp_system_dir		dir;
	gp_system_dirent	de;

	dir = gp_system_opendir(path);
	if (!dir) return;
	while ((de=gp_system_readdir(dir))) {
		if (!strcmp(gp_system_filename(de),".")) continue;
		if (!strcmp(gp_system_filename(de),"..")) continue;

		cur = malloc(sizeof(struct ptp_dirent));
		if (!cur) break;
		cur->name = strdup(gp_system_filename(de));
		cur->fsname = malloc(strlen(path)+1+strlen(gp_system_filename(de))+1);
		strcpy(cur->fsname,path);
		strcat(cur->fsname,"/");
		strcat(cur->fsname,gp_system_filename(de));
		cur->id = ptp_objectid++;
		cur->next = first_dirent;
		cur->parent = parent;
		first_dirent = cur;
		if (-1 == stat(cur->fsname, &cur->stbuf))
			continue;
		if (S_ISDIR(cur->stbuf.st_mode))
			read_directories(cur->fsname, cur); /* recurse! */
	}
	gp_system_closedir(dir);
}

static void
free_dirent(struct ptp_dirent *ent) {
	free (ent->name);
	free (ent->fsname);
	free (ent);
}

static void
read_tree(char *path) {
	struct	ptp_dirent *root = NULL, *dir, *dcim = NULL;

	if (first_dirent)
		return;

	first_dirent = malloc(sizeof(struct ptp_dirent));
	first_dirent->name = strdup("");
	first_dirent->fsname = strdup(path);
	first_dirent->id = ptp_objectid++;
	first_dirent->next = NULL;
	stat(first_dirent->fsname, &first_dirent->stbuf); /* assuming it works */
	root = first_dirent;
	read_directories(path,first_dirent);

	/* See if we have a DCIM directory, if not, create one. */
	dir = first_dirent;
	while (dir) {
		if (!strcmp(dir->name,"DCIM") && dir->parent && !dir->parent->id)
			dcim = dir;
		dir = dir->next;
	}
	if (!dcim) {
		dcim = malloc(sizeof(struct ptp_dirent));
		dcim->name = strdup("");
		dcim->fsname = strdup(path);
		dcim->id = ptp_objectid++;
		dcim->next = first_dirent;
		dcim->parent = root;
		stat(dcim->fsname, &dcim->stbuf); /* assuming it works */
		first_dirent = dcim;
	}
}

static int
ptp_nikon_setcontrolmode_write(vcamera *cam, ptpcontainer *ptp) {
	CHECK_PARAM_COUNT(1);

	if ((ptp->params[0] != 0) && (ptp->params[0] != 1)) {
		gp_log (GP_LOG_ERROR,__FUNCTION__,"controlmode must not be 0 or 1, is %d", ptp->params[0]);
		ptp_response (cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	if (cam->session) {
		gp_log (GP_LOG_ERROR,__FUNCTION__,"session is already open");
		ptp_response (cam, PTP_RC_SessionAlreadyOpened, 0);
		return 1;
	}
	cam->session = ptp->params[0];
	ptp_response (cam,PTP_RC_OK,0);
	return 1;
}

static int
ptp_opensession_write(vcamera *cam, ptpcontainer *ptp) {
	CHECK_PARAM_COUNT(1);

	if (ptp->params[0] == 0) {
		gp_log (GP_LOG_ERROR,__FUNCTION__,"session must not be 0, is %d", ptp->params[0]);
		ptp_response (cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	if (cam->session) {
		gp_log (GP_LOG_ERROR,__FUNCTION__,"session is already open");
		ptp_response (cam, PTP_RC_SessionAlreadyOpened, 0);
		return 1;
	}
	cam->session = ptp->params[0];
	ptp_response (cam,PTP_RC_OK,0);
	return 1;
}

static int
ptp_closesession_write(vcamera *cam, ptpcontainer *ptp) {
	CHECK_PARAM_COUNT(0);
	CHECK_SEQUENCE_NUMBER();

	if (!cam->session) {
		gp_log (GP_LOG_ERROR,__FUNCTION__,"session is not open");
		ptp_response(cam, PTP_RC_SessionAlreadyOpened, 0);
		return 1;
	}
	cam->session = 0;
	ptp_response(cam,PTP_RC_OK,0);
	return 1;
}

static int
ptp_deviceinfo_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char	*data;
	int		x = 0, i, cnt, vendor;
	uint16_t	*opcodes, *devprops;
	uint16_t	imageformats[1];
	uint16_t	events[5];

	CHECK_PARAM_COUNT(0);

	/* Session does not need to be open for GetDeviceInfo */

	/* Getdeviceinfo is special. it can be called with transid 0 outside of the session. */
	if ((ptp->seqnr != 0) && (ptp->seqnr != cam->seqnr)) {
		/* not clear if normal cameras react like this */						\
		gp_log (GP_LOG_ERROR, __FUNCTION__, "seqnr %d was sent, expected was %d", ptp->seqnr, cam->seqnr);\
		ptp_response(cam,PTP_RC_GeneralError,0);							\
		return 1;
	}
	data = malloc(2000);

	x += put_16bit_le(data+x,100);		/* StandardVersion */
	switch (cam->type) {
	case NIKON_D750:
		x += put_32bit_le(data+x,0xa);		/* VendorExtensionID */
		x += put_16bit_le(data+x,100);		/* VendorExtensionVersion */
		break;
	default:
		x += put_32bit_le(data+x,0);		/* VendorExtensionID */
		x += put_16bit_le(data+x,0);		/* VendorExtensionVersion */
		break;
	}
	x += put_string(data+x,"G-V: 1.0;");	/* VendorExtensionDesc */
	x += put_16bit_le(data+x,0);		/* FunctionalMode */

	cnt = 0;
	for (i = 0;i<sizeof(ptp_functions)/sizeof(ptp_functions[0]);i++) {
		if (ptp_functions[i].type == GENERIC_PTP) {
			cnt += ptp_functions[i].nroffunctions;
			continue;
		}
		if (ptp_functions[i].type == cam->type) {
			vendor = i;
			cnt += ptp_functions[i].nroffunctions;
		}
	}
	opcodes = malloc(cnt*sizeof(uint16_t));

	for (i=0;i<ptp_functions[0].nroffunctions;i++)
		opcodes[i] = ptp_functions[0].functions[i].code;

	if (cam->type != GENERIC_PTP) {
		for (i=0;i<ptp_functions[vendor].nroffunctions;i++)
			opcodes[i+ptp_functions[0].nroffunctions] = ptp_functions[vendor].functions[i].code;
	}

	x += put_16bit_le_array(data+x,opcodes,cnt);	/* OperationsSupported */
	free (opcodes);

	events[0] = 0x4002;
	events[1] = 0x4003;
	events[2] = 0x4006;
	events[3] = 0x400a;
	events[4] = 0x400d;
	x += put_16bit_le_array(data+x,events,sizeof(events)/sizeof(events[0]));	/* EventsSupported */

	devprops = malloc(sizeof(ptp_properties)/sizeof(ptp_properties[0])*sizeof(uint16_t));
	for (i=0;i<sizeof(ptp_properties)/sizeof(ptp_properties[0]);i++)
		devprops[i] = ptp_properties[i].code;
	x += put_16bit_le_array(data+x,devprops,sizeof(ptp_properties)/sizeof(ptp_properties[0]));/* DevicePropertiesSupported */
	free (devprops);

	imageformats[0] = 0x3801;
	x += put_16bit_le_array(data+x,imageformats,1);	/* CaptureFormats */

	imageformats[0] = 0x3801;
	x += put_16bit_le_array(data+x,imageformats,1);	/* ImageFormats */

	x += put_string(data+x,"GP");	/* Manufacturer */
	x += put_string(data+x,"VC");	/* Model */
	x += put_string(data+x,"2.5.11");/* DeviceVersion */
	x += put_string(data+x,"0.1");	/* DeviceVersion */
	x += put_string(data+x,"1");	/* SerialNumber */

	ptp_senddata(cam,0x1001,data,x);
	free (data);
	ptp_response(cam,PTP_RC_OK,0);
	return 1;
}

static int
ptp_getnumobjects_write(vcamera *cam, ptpcontainer *ptp) {
	int			cnt;
	struct ptp_dirent	*cur;
	uint32_t		mode = 0;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();

	if (ptp->nparams < 1) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "parameter count %d", ptp->nparams);
		ptp_response (cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	if ((ptp->params[0] != 0xffffffff) && (ptp->params[0] != 0x00010001)) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "storage id 0x%08x unknown", ptp->params[0]);
		ptp_response (cam, PTP_RC_InvalidStorageId, 0);
		return 1;
	}
	if (ptp->nparams >= 2) {
		if (ptp->params[1] != 0) {
			gp_log (GP_LOG_ERROR,__FUNCTION__, "currently can not handle OFC selection (0x%04x)", ptp->params[1]);
			ptp_response (cam, PTP_RC_SpecificationByFormatUnsupported, 0);
			return 1;
		}
	}
	if (ptp->nparams >= 3) {
		mode = ptp->params[2];
		if ((mode != 0) && (mode != 0xffffffff)) {
			cur = first_dirent;
			while (cur) {
				if (cur->id == mode) break;
				cur = cur->next;
			}
			if (!cur) {
				gp_log (GP_LOG_ERROR,__FUNCTION__, "requested subtree of (0x%08x), but no such handle", mode);
				ptp_response (cam, PTP_RC_InvalidObjectHandle, 0);
				return 1;
			}
			if (!S_ISDIR(cur->stbuf.st_mode)) {
				gp_log (GP_LOG_ERROR,__FUNCTION__, "requested subtree of (0x%08x), but this is no asssocation", mode);
				ptp_response (cam, PTP_RC_InvalidParentObject, 0);
				return 1;
			}
		}
	}

	cnt = 0; cur = first_dirent;
	while (cur) {
		if (cur->id) { /* do not include 0 entry */
			switch (mode) {
			case 0:	/* all objects recursive on device */
				cnt++;
				break;
			case 0xffffffff: /* only root dir */
				if (cur->parent->id == 0)
					cnt++;
				break;
			default: /* single level directory below this handle */
				if (cur->parent->id == mode)
					cnt++;
				break;
			}
		}
		cur = cur->next;
	}

	ptp_response (cam, PTP_RC_OK, 1, cnt);
	return 1;
}

static int
ptp_getobjecthandles_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char 		*data;
	int			x = 0, cnt;
	struct ptp_dirent	*cur;
	uint32_t		mode = 0;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();

	if (ptp->nparams < 1) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "parameter count %d", ptp->nparams);
		ptp_response (cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	if ((ptp->params[0] != 0xffffffff) && (ptp->params[0] != 0x00010001)) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "storage id 0x%08x unknown", ptp->params[0]);
		ptp_response (cam, PTP_RC_InvalidStorageId, 0);
		return 1;
	}
	if (ptp->nparams >= 2) {
		if (ptp->params[1] != 0) {
			gp_log (GP_LOG_ERROR,__FUNCTION__, "currently can not handle OFC selection (0x%04x)", ptp->params[1]);
			ptp_response (cam, PTP_RC_SpecificationByFormatUnsupported, 0);
			return 1;
		}
	}
	if (ptp->nparams >= 3) {
		mode = ptp->params[2];
		if ((mode != 0) && (mode != 0xffffffff)) {
			cur = first_dirent;
			while (cur) {
				if (cur->id == mode) break;
				cur = cur->next;
			}
			if (!cur) {
				gp_log (GP_LOG_ERROR,__FUNCTION__, "requested subtree of (0x%08x), but no such handle", mode);
				ptp_response (cam, PTP_RC_InvalidObjectHandle, 0);
				return 1;
			}
			if (!S_ISDIR(cur->stbuf.st_mode)) {
				gp_log (GP_LOG_ERROR,__FUNCTION__, "requested subtree of (0x%08x), but this is no asssocation", mode);
				ptp_response (cam, PTP_RC_InvalidParentObject, 0);
				return 1;
			}
		}
	}

	cnt = 0; cur = first_dirent;
	while (cur) {
		if (cur->id) { /* do not include 0 entry */
			switch (mode) {
			case 0:	/* all objects recursive on device */
				cnt++;
				break;
			case 0xffffffff: /* only root dir */
				if (cur->parent->id == 0)
					cnt++;
				break;
			default: /* single level directory below this handle */
				if (cur->parent->id == mode)
					cnt++;
				break;
			}
		}
		cur = cur->next;
	}

	data = malloc(4+4*cnt);
	x = put_32bit_le(data + x,cnt);
	cur = first_dirent;
	while (cur) {
		if (cur->id) { /* do not include 0 entry */
			switch (mode) {
			case 0:	/* all objects recursive on device */
				x += put_32bit_le(data+x, cur->id);
				break;
			case 0xffffffff: /* only root dir */
				if (cur->parent->id == 0)
					x += put_32bit_le(data+x, cur->id);
				break;
			default: /* single level directory below this handle */
				if (cur->parent->id == mode)
					x += put_32bit_le(data+x, cur->id);
				break;
			}
		}
		cur = cur->next;
	}
	ptp_senddata(cam,0x1007,data,x);
	free (data);
	ptp_response(cam,PTP_RC_OK,0);
	return 1;
}

static int
ptp_getstorageids_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char 	*data;
	int		x = 0;
	uint32_t	sids[1];

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(0);

	data = malloc(200);

	sids[0] = 0x00010001;
	x = put_32bit_le_array (data, sids, 1);

	ptp_senddata (cam, 0x1004, data, x);
	free (data);
	ptp_response(cam,PTP_RC_OK,0);
	return 1;
}

static int
ptp_getstorageinfo_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char 	*data;
	int		x = 0;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	if (ptp->params[0] != 0x00010001) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid storage id 0x%08x", ptp->params[0]);
		ptp_response(cam,PTP_RC_InvalidStorageId,0);
		return 1;
	}

	data = malloc(200);
	x += put_16bit_le (data+x, 3);	/* StorageType: Fixed RAM */
	x += put_16bit_le (data+x, 3);	/* FileSystemType: Generic Hierarchical */
	x += put_16bit_le (data+x, 2);	/* AccessCapability: R/O with object deletion */
	x += put_64bit_le (data+x, 0x42424242);	/* MaxCapacity */
	x += put_64bit_le (data+x, 0x21212121);	/* FreeSpaceInBytes */
	x += put_32bit_le (data+x, 150);	/* FreeSpaceInImages ... around 150 */
	x += put_string (data+x, "GPVC Storage");	/* StorageDescription */
	x += put_string (data+x, "GPVCS Label");	/* VolumeLabel */

	ptp_senddata (cam, 0x1005, data, x);
	free (data);
	ptp_response(cam, PTP_RC_OK, 0);
	return 1;
}

static int
ptp_getobjectinfo_write(vcamera *cam, ptpcontainer *ptp) {
	struct ptp_dirent	*cur;
	unsigned char		*data;
	int			x = 0;
	uint16_t 		ofc, thumbofc = 0;
	int			thumbwidth = 0, thumbheight = 0, thumbsize = 0;
	int			imagewidth = 0, imageheight = 0, imagebitdepth = 0;
	struct tm		*tm;
	time_t			xtime;
	char			xdate[40];

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	cur = first_dirent;
	while (cur) {
		if (cur->id == ptp->params[0])
			break;
		cur = cur->next;
	}
	if (!cur) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid object id 0x%08x", ptp->params[0]);
		ptp_response(cam,PTP_RC_InvalidObjectHandle,0);
		return 1;
	}
	data = malloc(2000);
	x += put_32bit_le (data+x, 0x00010001);	/* StorageID */
	/* ObjectFormatCode */
	ofc = 0x3000;
	if (S_ISDIR(cur->stbuf.st_mode)) {
		ofc = 0x3001;
	} else {
		if (strstr(cur->name,".JPG") || strstr(cur->name,".jpg"))
			ofc = 0x3801;
		if (strstr(cur->name,".GIF") || strstr(cur->name,".gif"))
			ofc = 0x3807;
		if (strstr(cur->name,".PNG") || strstr(cur->name,".png"))
			ofc = 0x380B;
		if (strstr(cur->name,".DNG") || strstr(cur->name,".dng"))
			ofc = 0x3811;
		if (strstr(cur->name,".TXT") || strstr(cur->name,".txt"))
			ofc = 0x3004;
		if (strstr(cur->name,".HTML") || strstr(cur->name,".html"))
			ofc = 0x3005;
		if (strstr(cur->name,".MP3") || strstr(cur->name,".mp3"))
			ofc = 0x3009;
		if (strstr(cur->name,".AVI") || strstr(cur->name,".avi"))
			ofc = 0x300A;
		if (	strstr(cur->name,".MPG") || strstr(cur->name,".mpg") ||
			strstr(cur->name,".MPEG") || strstr(cur->name,".mpeg")
		)
			ofc = 0x300B;
	}

#ifdef HAVE_LIBEXIF
	if (ofc == 0x3801) {			/* We are jpeg ... look into the exif data */
		ExifData	*ed;
		ExifEntry	*e;
		int		fd;
		unsigned char	*filedata;

		filedata = malloc(cur->stbuf.st_size);
		fd =  open(cur->fsname,O_RDONLY);
		if (fd == -1) {
			free (filedata);
			free (data);
			gp_log (GP_LOG_ERROR,__FUNCTION__, "could not open %s", cur->fsname);
			ptp_response(cam,PTP_RC_GeneralError,0);
			return 1;
		}
		if (cur->stbuf.st_size != read(fd, filedata, cur->stbuf.st_size)) {
			free (filedata);
			free (data);
			close (fd);
			gp_log (GP_LOG_ERROR,__FUNCTION__, "could not read data of %s", cur->fsname);
			ptp_response(cam,PTP_RC_GeneralError,0);
			return 1;
		}
		close (fd);

		ed = exif_data_new_from_data ((unsigned char*)filedata, cur->stbuf.st_size);
		if (ed) {
			if (ed->data) {
				thumbofc = 0x3808;
				thumbsize = ed->size;
			}
			e = exif_data_get_entry (ed, EXIF_TAG_PIXEL_X_DIMENSION);
			if (e) {
				gp_log (GP_LOG_DEBUG, __FUNCTION__ , "pixel x dim format is %d", e->format);
				if (e->format == EXIF_FORMAT_SHORT) {
					imagewidth = exif_get_short (e->data, exif_data_get_byte_order (ed));
				}
			}
			e = exif_data_get_entry (ed, EXIF_TAG_PIXEL_Y_DIMENSION);
			if (e) {
				gp_log (GP_LOG_DEBUG, __FUNCTION__ , "pixel y dim format is %d", e->format);
				if (e->format == EXIF_FORMAT_SHORT) {
					imageheight = exif_get_short (e->data, exif_data_get_byte_order (ed));
				}
			}
			/* FIXME: potentially could find out more about thumbnail too */
		}
		exif_data_unref (ed);
		free (filedata);
	}
#endif
	x += put_16bit_le (data+x, ofc);
	x += put_16bit_le (data+x, 0); 			/* ProtectionStatus, no protection */
	x += put_32bit_le (data+x, cur->stbuf.st_size); /* ObjectCompressedSize */
	x += put_16bit_le (data+x, thumbofc); 		/* ThumbFormat */
	x += put_32bit_le (data+x, thumbsize); 		/* ThumbCompressedSize */
	x += put_32bit_le (data+x, thumbwidth); 	/* ThumbPixWidth */
	x += put_32bit_le (data+x, thumbheight);	/* ThumbPixHeight */
	x += put_32bit_le (data+x, imagewidth); 	/* ImagePixWidth */
	x += put_32bit_le (data+x, imageheight);	/* ImagePixHeight */
	x += put_32bit_le (data+x, imagebitdepth); 	/* ImageBitDepth */
	x += put_32bit_le (data+x, cur->parent->id); 	/* ParentObject */
	/* AssociationType */
	if (S_ISDIR(cur->stbuf.st_mode)) {
		x += put_16bit_le (data+x, 1); /* GenericFolder */
		x += put_32bit_le (data+x, 0); /* unused */
	} else {
		x += put_16bit_le (data+x, 0); /* Undefined */
		x += put_32bit_le (data+x, 0); /* Undefined */
	}
	x += put_32bit_le (data+x, 0); 		/* SequenceNumber */
	x += put_string (data+x, cur->name); 	/* Filename */

	xtime = cur->stbuf.st_ctime;
	tm = gmtime(&xtime);
	sprintf(xdate,"%04d%02d%02dT%02d%02d%02d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	x += put_string (data+x, xdate);	/* CreationDate */
	xtime = cur->stbuf.st_mtime;
	tm = gmtime(&xtime);
	sprintf(xdate,"%04d%02d%02dT%02d%02d%02d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	x += put_string (data+x, xdate);	/* ModificatioDate */

	x += put_string (data+x, "keyword");	/* Keywords */

	ptp_senddata (cam, 0x1008, data, x);
	free (data);
	ptp_response(cam, PTP_RC_OK, 0);
	return 1;
}

static int
ptp_getobject_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char 		*data;
	struct ptp_dirent	*cur;
	int fd;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	cur = first_dirent;
	while (cur) {
		if (cur->id == ptp->params[0]) break;
		cur = cur->next;
	}
	if (!cur) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid object id 0x%08x", ptp->params[0]);
		ptp_response(cam,PTP_RC_InvalidObjectHandle,0);
		return 1;
	}
	data = malloc(cur->stbuf.st_size);
	fd =  open(cur->fsname,O_RDONLY);
	if (fd == -1) {
		free (data);
		gp_log (GP_LOG_ERROR,__FUNCTION__, "could not open %s", cur->fsname);
		ptp_response(cam,PTP_RC_GeneralError,0);
		return 1;
	}
	if (cur->stbuf.st_size != read(fd, data, cur->stbuf.st_size)) {
		free (data);
		close (fd);
		gp_log (GP_LOG_ERROR,__FUNCTION__, "could not read data of %s", cur->fsname);
		ptp_response(cam,PTP_RC_GeneralError,0);
		return 1;
	}
	close (fd);

	ptp_senddata (cam, 0x1009, data, cur->stbuf.st_size);
	free (data);
	ptp_response (cam, PTP_RC_OK, 0);
	return 1;
}

static int
ptp_getthumb_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char 		*data;
	struct ptp_dirent	*cur;
	int			fd;
#ifdef HAVE_LIBEXIF
        ExifData		*ed;
#endif

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	cur = first_dirent;
	while (cur) {
		if (cur->id == ptp->params[0]) break;
		cur = cur->next;
	}
	if (!cur) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid object id 0x%08x", ptp->params[0]);
		ptp_response(cam,PTP_RC_InvalidObjectHandle,0);
		return 1;
	}
	data = malloc(cur->stbuf.st_size);
	fd =  open(cur->fsname,O_RDONLY);
	if (fd == -1) {
		free (data);
		gp_log (GP_LOG_ERROR,__FUNCTION__, "could not open %s", cur->fsname);
		ptp_response(cam,PTP_RC_GeneralError,0);
		return 1;
	}
	if (cur->stbuf.st_size != read(fd, data, cur->stbuf.st_size)) {
		free (data);
		close (fd);
		gp_log (GP_LOG_ERROR,__FUNCTION__, "could not read data of %s", cur->fsname);
		ptp_response(cam,PTP_RC_GeneralError,0);
		return 1;
	}
	close (fd);

#ifdef HAVE_LIBEXIF
	ed = exif_data_new_from_data ((unsigned char*)data, cur->stbuf.st_size);
	if (!ed) {
		gp_log (GP_LOG_ERROR, __FUNCTION__, "Could not parse EXIF data");
		free (data);
		ptp_response(cam,PTP_RC_NoThumbnailPresent,0);
		return 1;
	}
	if (!ed->data) {
		gp_log (GP_LOG_ERROR, __FUNCTION__, "EXIF data does not contain a thumbnail");
		free (data);
		ptp_response(cam,PTP_RC_NoThumbnailPresent,0);
		exif_data_unref (ed);
		return 1;
	}
	/*
	 * We found a thumbnail in EXIF data! Those
	 * thumbnails are always JPEG. Set up the file.
	 */
	ptp_senddata (cam, 0x100A, ed->data, ed->size);
	exif_data_unref (ed);

	ptp_response (cam, PTP_RC_OK, 0);
#else
	gp_log (GP_LOG_ERROR, __FUNCTION__, "Cannot get thumbnail without libexif, lying about missing thumbnail");
	ptp_response(cam,PTP_RC_NoThumbnailPresent,0);
#endif
	free (data);
	return 1;
}

static int
ptp_initiatecapture_write(vcamera *cam, ptpcontainer *ptp) {
	struct ptp_dirent	*cur, *newcur, *dir, *dcim = NULL;
	static int		capcnt = 98;
	char			buf[10];

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(2);

	if ((ptp->params[0] != 0) && (ptp->params[0] != 0x00010001)) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid storage id 0x%08x", ptp->params[0]);
		ptp_response (cam, PTP_RC_StoreNotAvailable, 0);
		return 1;
	}
	if ((ptp->params[1] != 0) && (ptp->params[1] != 0x3801)) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid objectformat code id 0x%04x", ptp->params[1]);
		ptp_response (cam, PTP_RC_InvalidObjectFormatCode, 0);
		return 1;
	}
	if (capcnt > 150) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "Declaring store full at picture 151");
		ptp_response (cam, PTP_RC_StoreFull, 0);
		return 1;
	}

	cur = first_dirent;
	while (cur) {
		if (strstr (cur->name, ".jpg") || strstr (cur->name, ".JPG"))
			break;
		cur = cur->next;
	}
	if (!cur) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "I do not have a JPG file in the store, can not proceed");
		ptp_response (cam, PTP_RC_GeneralError, 0);
		return 1;
	}
	dir = first_dirent;
	while (dir) {
		if (!strcmp(dir->name,"DCIM") && dir->parent && !dir->parent->id)
			dcim = dir;
		dir = dir->next;
	}

	cur = first_dirent;
	while (cur) {
		if (strstr (cur->name, ".jpg") || strstr (cur->name, ".JPG"))
			break;
		cur = cur->next;
	}
	if (!cur) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "I do not have a JPG file in the store, can not proceed");
		ptp_response (cam, PTP_RC_GeneralError, 0);
		return 1;
	}
	dir = first_dirent;
	while (dir) {
		if (!strcmp(dir->name,"DCIM") && dir->parent && !dir->parent->id)
			dcim = dir;
		dir = dir->next;
	}
	/* nnnGPHOT directories, where nnn is 100-999. (See DCIM standard.) */
	sprintf(buf, "%03dGPHOT", 100 + ((capcnt / 100) % 900));
	dir = first_dirent;
	while (dir) {
		if (!strcmp (dir->name, buf) && (dir->parent == dcim))
			break;
		dir = dir->next;
	}
	if (!dir) {
		dir 		= malloc (sizeof(struct ptp_dirent));
		dir->id		= ++ptp_objectid;
		dir->fsname	= "virtual";
		dir->stbuf	= dcim->stbuf; /* only the S_ISDIR flag is used */
		dir->parent	= dcim;
		dir->next	= first_dirent;
		dir->name	= strdup (buf);
		first_dirent	= dir;
		/* Emit ObjectAdded event for the created folder */
		ptp_inject_interrupt (cam, 80, 0x4002, 1, ptp_objectid, cam->seqnr);	/* objectadded */
	}
	if (capcnt++ == 150) {
		/* The start of the operation succeeds, but the memory runs full during it. */
		ptp_inject_interrupt (cam, 100, 0x400A, 1, ptp_objectid, cam->seqnr);	/* storefull */
		ptp_response (cam, PTP_RC_OK, 0);
		return 1;
	}

	newcur 		= malloc (sizeof(struct ptp_dirent));
	newcur->id	= ++ptp_objectid;
	newcur->fsname	= strdup(cur->fsname);
	newcur->stbuf	= cur->stbuf;
	newcur->parent	= dir;
	newcur->next	= first_dirent;
	newcur->name	= malloc(8+3+1+1);
	sprintf(newcur->name,"GPH_%04d.JPG", capcnt++);
	first_dirent	= newcur;

	ptp_inject_interrupt (cam, 100, 0x4002, 1, ptp_objectid, cam->seqnr);	/* objectadded */
	ptp_inject_interrupt (cam, 120, 0x400d, 0, 0, cam->seqnr);		/* capturecomplete */
	ptp_response (cam, PTP_RC_OK, 0);
	return 1;
}

static int
ptp_deleteobject_write(vcamera *cam, ptpcontainer *ptp) {
	struct ptp_dirent	*cur, *xcur;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();

	if (ptp->nparams < 1) {
		gp_log (GP_LOG_ERROR, __FUNCTION__, "parameter count %d", ptp->nparams);
		ptp_response (cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	if (ptp->params[0] == 0xffffffff) { /* delete all mode */
		gp_log (GP_LOG_DEBUG, __FUNCTION__, "delete all");
		cur = first_dirent;

		while (cur) {
			xcur = cur->next;
			free_dirent(cur);
			cur = xcur;
		}
		first_dirent = NULL;
		ptp_response (cam, PTP_RC_OK, 0);
		return 1;
	}

	if ((ptp->nparams == 2) && (ptp->params[1] != 0)) {
		gp_log (GP_LOG_ERROR, __FUNCTION__, "single object delete, but ofc is 0x%08x", ptp->params[1]);
		ptp_response (cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	/* for associations this even means recursive deletion */

	cur = first_dirent;
	while (cur) {
		if (cur->id == ptp->params[0]) break;
		cur = cur->next;
	}
	if (!cur) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid object id 0x%08x", ptp->params[0]);
		ptp_response(cam,PTP_RC_InvalidObjectHandle,0);
		return 1;
	}
	if (S_ISDIR(cur->stbuf.st_mode)) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "FIXME: not yet deleting directories");
		ptp_response(cam,PTP_RC_ObjectWriteProtected,0);
		return 1;
	}
	if (cur == first_dirent) {
		first_dirent = cur->next;
		free_dirent (cur);
	} else {
		xcur = first_dirent;
		while (xcur) {
			if (xcur->next == cur) {
				xcur->next = xcur->next->next;
				free_dirent (cur);
				break;
			}
			xcur = xcur->next;
		}
	}
	ptp_response (cam, PTP_RC_OK, 0);
	return 1;
}


static int
put_propval (unsigned char *data, uint16_t type, PTPPropertyValue *val) {
	switch (type) {
	case 0x1:	return put_8bit_le (data, val->i8);
	case 0x2:	return put_8bit_le (data, val->u8);
	case 0x3:	return put_16bit_le (data, val->i16);
	case 0x4:	return put_16bit_le (data, val->u16);
	case 0x6:	return put_32bit_le (data, val->u32);
	case 0xffff:	return put_string (data, val->str);
	default:	gp_log (GP_LOG_ERROR, __FUNCTION__, "unhandled datatype %d", type);
			return 0;
	}
	return 0;
}

static int
get_propval (unsigned char *data, unsigned int len, uint16_t type, PTPPropertyValue *val) {
#define CHECK_SIZE(x) if (len < x) return 0;
	switch (type) {
	case 0x1:	CHECK_SIZE(1);
			val->i8 = get_i8bit_le (data);
			return 1;
	case 0x2:	CHECK_SIZE(1);
			val->u8 =  get_8bit_le (data);
			return 1;
	case 0x3:	CHECK_SIZE(2);
			val->i16 =  get_16bit_le (data);
			return 1;
	case 0x4:	CHECK_SIZE(2);
			val->u16 =  get_16bit_le (data);
			return 1;
	case 0x6:	CHECK_SIZE(4);
			val->u32 =  get_32bit_le (data);
			return 1;
	case 0xffff:	{
			int slen;
			CHECK_SIZE(1);
			slen = get_8bit_le (data);
			CHECK_SIZE(1+slen*2);
			val->str = get_string (data);
			return 1+slen*2;
			}
	default:	gp_log (GP_LOG_ERROR, __FUNCTION__, "unhandled datatype %d", type);
			return 0;
	}
	return 0;
#undef CHECK_SIZE
}

static int
ptp_getdevicepropdesc_write(vcamera *cam, ptpcontainer *ptp) {
	int			i, x = 0;
	unsigned char		*data;
	PTPDevicePropDesc	desc;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	for (i=0;i<sizeof (ptp_properties)/sizeof (ptp_properties[0]);i++) {
		if (ptp_properties[i].code == ptp->params[0])
			break;
	}
	if (i == sizeof (ptp_properties)/sizeof (ptp_properties[0])) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "deviceprop 0x%04x not found", ptp->params[0]);
		ptp_response (cam, PTP_RC_DevicePropNotSupported, 0);
		return 1;
	}
	data = malloc(2000);
	ptp_properties[i].getdesc (cam, &desc);

	x += put_16bit_le (data+x, desc.DevicePropertyCode);
	x += put_16bit_le (data+x, desc.DataType);
	x += put_8bit_le  (data+x, desc.GetSet);
	x += put_propval  (data+x, desc.DataType, &desc.FactoryDefaultValue);
	x += put_propval  (data+x, desc.DataType, &desc.CurrentValue);
	x += put_8bit_le  (data+x, desc.FormFlag);
	switch (desc.FormFlag) {
	case 0:	break;
	case 1:	/* range */
		x += put_propval (data+x, desc.DataType, &desc.FORM.Range.MinimumValue);
		x += put_propval (data+x, desc.DataType, &desc.FORM.Range.MaximumValue);
		x += put_propval (data+x, desc.DataType, &desc.FORM.Range.StepSize);
		break;
	case 2:	/* ENUM */
		x += put_16bit_le (data+x, desc.FORM.Enum.NumberOfValues);
		for (i=0;i<desc.FORM.Enum.NumberOfValues;i++)
			x += put_propval (data+x, desc.DataType, &desc.FORM.Enum.SupportedValue[i]);
		break;
	}

	ptp_senddata (cam, 0x1014, data, x);
	free (data);
	ptp_response (cam, PTP_RC_OK, 0);
	return 1;
}

static int
ptp_getdevicepropvalue_write(vcamera *cam, ptpcontainer *ptp) {
	unsigned char*		data;
	int			i, x = 0;
	PTPPropertyValue	val;
	PTPDevicePropDesc	desc;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	for (i=0;i<sizeof(ptp_properties)/sizeof(ptp_properties[0]);i++) {
		if (ptp_properties[i].code == ptp->params[0])
			break;
	}
	if (i == sizeof (ptp_properties)/sizeof (ptp_properties[0])) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "deviceprop 0x%04x not found", ptp->params[0]);
		ptp_response (cam, PTP_RC_DevicePropNotSupported, 0);
		return 1;
	}
	data = malloc(2000);
	ptp_properties[i].getdesc (cam, &desc);
	ptp_properties[i].getvalue (cam, &val);
	x = put_propval (data+x, desc.DataType, &val);

	ptp_senddata (cam, 0x1015, data, x);
	free (data);

	ptp_response (cam, PTP_RC_OK, 0);
	return 1;
}

static int
ptp_setdevicepropvalue_write(vcamera *cam, ptpcontainer *ptp) {
	int			i;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	for (i=0;i<sizeof(ptp_properties)/sizeof(ptp_properties[0]);i++) {
		if (ptp_properties[i].code == ptp->params[0])
			break;
	}
	if (i == sizeof (ptp_properties)/sizeof (ptp_properties[0])) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "deviceprop 0x%04x not found", ptp->params[0]);
		ptp_response (cam, PTP_RC_DevicePropNotSupported, 0);
		return 1;
	}
	/* so ... we wait for the data phase */
	return 1;
}

/* magic opcode for our driver, to inject commands */
static int
ptp_vusb_write(vcamera *cam, ptpcontainer *ptp) {
	static int		capcnt = 98;
	static int		timeout = 1;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	if (ptp->nparams < 1) {
		gp_log (GP_LOG_ERROR, __FUNCTION__, "parameter count %d", ptp->nparams);
		ptp_response (cam, PTP_RC_InvalidParameter, 0);
		return 1;
	}
	if (ptp->nparams >= 2) {
		timeout = ptp->params[1];
		gp_log (GP_LOG_DEBUG, __FUNCTION__, "new timeout %d", timeout);
	} else
		timeout++;

	switch (ptp->params[0]) {
	case 0:	{/* add a new image after 1 second */
		struct ptp_dirent	*cur, *newcur, *dir, *dcim = NULL;
		char			buf[10];

		cur = first_dirent;
		while (cur) {
			if (strstr (cur->name, ".jpg") || strstr (cur->name, ".JPG"))
				break;
			cur = cur->next;
		}
		if (!cur) {
			gp_log (GP_LOG_ERROR,__FUNCTION__, "I do not have a JPG file in the store, can not proceed");
			ptp_response (cam, PTP_RC_GeneralError, 0);
			return 1;
		}
		dir = first_dirent;
		while (dir) {
			if (!strcmp(dir->name,"DCIM") && dir->parent && !dir->parent->id)
				dcim = dir;
			dir = dir->next;
		}

		cur = first_dirent;
		while (cur) {
			if (strstr (cur->name, ".jpg") || strstr (cur->name, ".JPG"))
				break;
			cur = cur->next;
		}
		if (!cur) {
			gp_log (GP_LOG_ERROR,__FUNCTION__, "I do not have a JPG file in the store, can not proceed");
			ptp_response (cam, PTP_RC_GeneralError, 0);
			return 1;
		}
		dir = first_dirent;
		while (dir) {
			if (!strcmp(dir->name,"DCIM") && dir->parent && !dir->parent->id)
				dcim = dir;
			dir = dir->next;
		}
		/* nnnGPHOT directories, where nnn is 100-999. (See DCIM standard.) */
		sprintf(buf, "%03dGPHOT", 100 + ((capcnt / 100) % 900));
		dir = first_dirent;
		while (dir) {
			if (!strcmp (dir->name, buf) && (dir->parent == dcim))
				break;
			dir = dir->next;
		}
		if (!dir) {
			dir 		= malloc (sizeof(struct ptp_dirent));
			dir->id		= ++ptp_objectid;
			dir->fsname	= "virtual";
			dir->stbuf	= dcim->stbuf; /* only the S_ISDIR flag is used */
			dir->parent	= dcim;
			dir->next	= first_dirent;
			dir->name	= strdup (buf);
			first_dirent	= dir;
			/* Emit ObjectAdded event for the created folder */
			ptp_inject_interrupt (cam, 80, 0x4002, 1, ptp_objectid, cam->seqnr);	/* objectadded */
		}

		newcur 		= malloc (sizeof(struct ptp_dirent));
		newcur->id	= ++ptp_objectid;
		newcur->fsname	= strdup(cur->fsname);
		newcur->stbuf	= cur->stbuf;
		newcur->parent	= dir;
		newcur->next	= first_dirent;
		newcur->name	= malloc(8+3+1+1);
		sprintf(newcur->name,"GPH_%04d.JPG", capcnt++);
		first_dirent	= newcur;

		ptp_inject_interrupt (cam, timeout, 0x4002, 1, ptp_objectid, cam->seqnr);	/* objectadded */
		ptp_response (cam, PTP_RC_OK, 0);
		break;
	}
	case 1:	{/* remove 1 image from directory */
		struct ptp_dirent	**pcur, *cur;

		pcur = &first_dirent;
		while (*pcur) {
			if (strstr ((*pcur)->name, ".jpg") || strstr ((*pcur)->name, ".JPG"))
				break;
			pcur = &((*pcur)->next);
		}
		if (!*pcur) {
			gp_log (GP_LOG_ERROR,__FUNCTION__, "I do not have a JPG file in the store, can not proceed");
			ptp_response (cam, PTP_RC_GeneralError, 0);
			return 1;
		}
		ptp_inject_interrupt (cam, timeout, 0x4003, 1, (*pcur)->id, cam->seqnr);	/* objectremoved */
		cur = *pcur;
		*pcur = (*pcur)->next;
		free (cur->name);
		free (cur->fsname);
		free (cur);
		ptp_response (cam, PTP_RC_OK, 0);
		break;
	}
	case 2:	/* capture complete */
		ptp_inject_interrupt (cam, timeout, 0x400d, 0, 0, cam->seqnr);	/* capturecomplete */
		ptp_response (cam, PTP_RC_OK, 0);
		break;
	default:
		gp_log (GP_LOG_ERROR, __FUNCTION__, "unknown action %d", ptp->params[0]);
		ptp_response (cam, PTP_RC_OK, 0);
		break;
	}
	return 1;
}

static int
ptp_setdevicepropvalue_write_data(vcamera *cam, ptpcontainer *ptp, unsigned char *data, unsigned int len) {
	int			i;
	PTPPropertyValue	val;
	PTPDevicePropDesc	desc;

	CHECK_SEQUENCE_NUMBER();
	CHECK_SESSION();
	CHECK_PARAM_COUNT(1);

	for (i=0;i<sizeof(ptp_properties)/sizeof(ptp_properties[0]);i++) {
		if (ptp_properties[i].code == ptp->params[0])
			break;
	}
	if (i == sizeof (ptp_properties)/sizeof (ptp_properties[0])) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "deviceprop 0x%04x not found", ptp->params[0]);
		/* we emitted the response already in _write */
		return 1;
	}
	if (!ptp_properties[i].setvalue) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "deviceprop 0x%04x is not settable", ptp->params[0]);
		ptp_response (cam, PTP_RC_AccessDenied, 0);
		return 1;
	}
	ptp_properties[i].getdesc(cam, &desc);
	if (!get_propval (data, len, desc.DataType, &val)) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "deviceprop 0x%04x is not retrievable", ptp->params[0]);
		ptp_response (cam, PTP_RC_InvalidDevicePropFormat, 0);
		return 1;
	}
	ptp_properties[i].setvalue (cam, &val);
	ptp_response (cam, PTP_RC_OK, 0);
	return 1;
}


/**************************  Properties *****************************************************/
static int
ptp_battery_getdesc (vcamera* cam, PTPDevicePropDesc *desc) {
	desc->DevicePropertyCode	= 0x5001;
	desc->DataType			= 2;	/* uint8 */
	desc->GetSet			= 0;	/* Get only */
	desc->FactoryDefaultValue.u8	= 50;
	desc->CurrentValue.u8		= 50;
        desc->FormFlag			= 0x01; /* range */
	desc->FORM.Range.MinimumValue.u8= 0;
	desc->FORM.Range.MaximumValue.u8= 100;
	desc->FORM.Range.StepSize.u8	= 1;
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5001, 0xffffffff);
	return 1;
}

static int
ptp_battery_getvalue (vcamera* cam, PTPPropertyValue *val) {
	val->u8 = 50;
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5001, 0xffffffff);
	return 1;
}

static int
ptp_imagesize_getdesc (vcamera* cam, PTPDevicePropDesc *desc) {
	desc->DevicePropertyCode		= 0x5003;
	desc->DataType				= 0xffff;	/* STR */
	desc->GetSet				= 0;		/* Get only */
	desc->FactoryDefaultValue.str		= strdup("640x480");
	desc->CurrentValue.str			= strdup("640x480");
        desc->FormFlag				= 0x02; /* enum */
	desc->FORM.Enum.NumberOfValues 		= 3;
	desc->FORM.Enum.SupportedValue 		= malloc(3*sizeof(desc->FORM.Enum.SupportedValue[0]));
	desc->FORM.Enum.SupportedValue[0].str	= strdup("640x480");
	desc->FORM.Enum.SupportedValue[1].str	= strdup("1024x768");
	desc->FORM.Enum.SupportedValue[2].str	= strdup("2048x1536");

	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5003, 0xffffffff);
	return 1;
}

static int
ptp_imagesize_getvalue (vcamera* cam, PTPPropertyValue *val) {
	val->str = strdup("640x480");
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5003, 0xffffffff);
	return 1;
}

static int
ptp_shutterspeed_getdesc (vcamera* cam, PTPDevicePropDesc *desc) {
	desc->DevicePropertyCode		= 0x500D;
	desc->DataType				= 0x0006;	/* UINT32 */
	desc->GetSet				= 1;		/* Get/Set */
	if (!cam->shutterspeed) cam->shutterspeed = 100; /* 1/100 * 10000 */
	desc->FactoryDefaultValue.u32		= cam->shutterspeed;
	desc->CurrentValue.u32			= cam->shutterspeed;
        desc->FormFlag				= 0x02; /* enum */
	desc->FORM.Enum.NumberOfValues 		= 9;
	desc->FORM.Enum.SupportedValue 		= malloc(desc->FORM.Enum.NumberOfValues*sizeof(desc->FORM.Enum.SupportedValue[0]));
	desc->FORM.Enum.SupportedValue[0].u32	= 10000;
	desc->FORM.Enum.SupportedValue[1].u32	= 1000;
	desc->FORM.Enum.SupportedValue[2].u32	= 500;
	desc->FORM.Enum.SupportedValue[3].u32	= 200;
	desc->FORM.Enum.SupportedValue[4].u32	= 100;
	desc->FORM.Enum.SupportedValue[5].u32	= 50;
	desc->FORM.Enum.SupportedValue[6].u32	= 25;
	desc->FORM.Enum.SupportedValue[7].u32	= 12;
	desc->FORM.Enum.SupportedValue[8].u32	= 1;

	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x500D, 0xffffffff);
	return 1;
}

static int
ptp_shutterspeed_getvalue (vcamera* cam, PTPPropertyValue *val) {
	val->u32 = cam->shutterspeed;
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x500d, 0xffffffff);
	return 1;
}

static int
ptp_shutterspeed_setvalue (vcamera* cam, PTPPropertyValue *val) {
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x500d, 0xffffffff);
	gp_log (GP_LOG_DEBUG, __FUNCTION__, "got %d as value", val->u32);
	cam->shutterspeed = val->u32;
	return 1;
}

static int
ptp_fnumber_getdesc (vcamera* cam, PTPDevicePropDesc *desc) {
	desc->DevicePropertyCode		= 0x5007;
	desc->DataType				= 0x0004;	/* UINT16 */
	desc->GetSet				= 1;		/* Get/Set */
	if (!cam->fnumber) cam->fnumber = 280; /* 2.8 * 100 */
	desc->FactoryDefaultValue.u16		= cam->fnumber;
	desc->CurrentValue.u16			= cam->fnumber;
        desc->FormFlag				= 0x02; /* enum */
	desc->FORM.Enum.NumberOfValues 		= 18;
	desc->FORM.Enum.SupportedValue 		= malloc(desc->FORM.Enum.NumberOfValues*sizeof(desc->FORM.Enum.SupportedValue[0]));
	desc->FORM.Enum.SupportedValue[0].u16	= 280;
	desc->FORM.Enum.SupportedValue[1].u16	= 350;
	desc->FORM.Enum.SupportedValue[2].u16	= 400;
	desc->FORM.Enum.SupportedValue[3].u16	= 450;
	desc->FORM.Enum.SupportedValue[4].u16	= 500;
	desc->FORM.Enum.SupportedValue[5].u16	= 560;
	desc->FORM.Enum.SupportedValue[6].u16	= 630;
	desc->FORM.Enum.SupportedValue[7].u16	= 710;
	desc->FORM.Enum.SupportedValue[8].u16	= 800;
	desc->FORM.Enum.SupportedValue[9].u16	= 900;
	desc->FORM.Enum.SupportedValue[10].u16	= 1000;
	desc->FORM.Enum.SupportedValue[11].u16	= 1100;
	desc->FORM.Enum.SupportedValue[12].u16	= 1300;
	desc->FORM.Enum.SupportedValue[13].u16	= 1400;
	desc->FORM.Enum.SupportedValue[14].u16	= 1600;
	desc->FORM.Enum.SupportedValue[15].u16	= 1800;
	desc->FORM.Enum.SupportedValue[16].u16	= 2000;
	desc->FORM.Enum.SupportedValue[17].u16	= 2200;

	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5007, 0xffffffff);
	return 1;
}

static int
ptp_fnumber_getvalue (vcamera* cam, PTPPropertyValue *val) {
	val->u16 = cam->fnumber;
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5007, 0xffffffff);
	return 1;
}

static int
ptp_fnumber_setvalue (vcamera* cam, PTPPropertyValue *val) {
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5007, 0xffffffff);
	gp_log (GP_LOG_DEBUG, __FUNCTION__, "got %d as value", val->u16);
	cam->fnumber = val->u16;
	return 1;
}

static int
ptp_exposurebias_getdesc (vcamera* cam, PTPDevicePropDesc *desc) {
	desc->DevicePropertyCode		= 0x5010;
	desc->DataType				= 0x0003;	/* INT16 */
	desc->GetSet				= 1;		/* Get/Set */
	if (!cam->exposurebias) cam->exposurebias = 0; /* 0.0 */
	desc->FactoryDefaultValue.i16		= cam->exposurebias;
	desc->CurrentValue.i16			= cam->exposurebias;
        desc->FormFlag				= 0x02; /* enum */
	desc->FORM.Enum.NumberOfValues 		= 13;
	desc->FORM.Enum.SupportedValue 		= malloc(desc->FORM.Enum.NumberOfValues*sizeof(desc->FORM.Enum.SupportedValue[0]));
	desc->FORM.Enum.SupportedValue[0].i16	= -3000;
	desc->FORM.Enum.SupportedValue[1].i16	= -2500;
	desc->FORM.Enum.SupportedValue[2].i16	= -2000;
	desc->FORM.Enum.SupportedValue[3].i16	= -1500;
	desc->FORM.Enum.SupportedValue[4].i16	= -1000;
	desc->FORM.Enum.SupportedValue[5].i16	= -500;
	desc->FORM.Enum.SupportedValue[6].i16	= 0;
	desc->FORM.Enum.SupportedValue[7].i16	= 500;
	desc->FORM.Enum.SupportedValue[8].i16	= 1000;
	desc->FORM.Enum.SupportedValue[9].i16	= 1500;
	desc->FORM.Enum.SupportedValue[10].i16	= 2000;
	desc->FORM.Enum.SupportedValue[11].i16	= 2500;
	desc->FORM.Enum.SupportedValue[12].i16	= 3000;

	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5010, 0xffffffff);
	return 1;
}

static int
ptp_exposurebias_getvalue (vcamera* cam, PTPPropertyValue *val) {
	val->i16 = cam->exposurebias;
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5010, 0xffffffff);
	return 1;
}

static int
ptp_exposurebias_setvalue (vcamera* cam, PTPPropertyValue *val) {
	ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5010, 0xffffffff);
	gp_log (GP_LOG_DEBUG, __FUNCTION__, "got %d as value", val->i16);
	cam->exposurebias = val->i16;
	return 1;
}


static int
ptp_datetime_getdesc (vcamera* cam, PTPDevicePropDesc *desc) {
	struct tm		*tm;
	time_t			xtime;
	char			xdate[40];

	desc->DevicePropertyCode	= 0x5011;
	desc->DataType			= 0xffff;	/* string */
	desc->GetSet			= 1;		/* get only */
	time(&xtime);
	tm = gmtime(&xtime);
	sprintf(xdate,"%04d%02d%02dT%02d%02d%02d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	desc->FactoryDefaultValue.str	= strdup (xdate);
	desc->CurrentValue.str		= strdup (xdate);
        desc->FormFlag			= 0; /* no form */
	/*ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5011, 0xffffffff);*/
	return 1;
}

static int
ptp_datetime_getvalue (vcamera* cam, PTPPropertyValue *val) {
	struct tm		*tm;
	time_t			xtime;
	char			xdate[40];

	time(&xtime);
	tm = gmtime(&xtime);
	sprintf(xdate,"%04d%02d%02dT%02d%02d%02d",tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	val->str = strdup (xdate);
	/*ptp_inject_interrupt (cam, 1000, 0x4006, 1, 0x5011, 0xffffffff);*/
	return 1;
}


static int
ptp_datetime_setvalue (vcamera* cam, PTPPropertyValue *val) {
	gp_log (GP_LOG_DEBUG, __FUNCTION__, "got %s as value", val->str);
	return 1;
}


/********************************************************************************************/

static int vcam_init(vcamera* cam) {
	return GP_OK;
}

static int vcam_exit(vcamera* cam) {
	return GP_OK;
}

static int vcam_open(vcamera* cam, const char *port) {
	char *s = strchr(port,':');

	if (s) {
		if (s[1] == '>') { /* record mode */
			cam->fuzzf = fopen(s+2,"wb");
			cam->fuzzmode = FUZZMODE_PROTOCOL;
		} else {
			cam->fuzzf = fopen(s+1,"rb");
			cam->fuzzpending = 0;
			cam->fuzzmode = FUZZMODE_NORMAL;
		}
		if (cam->fuzzf == NULL) perror(s+1);
	}
	return GP_OK;
}

static int vcam_close(vcamera* cam) {
	if (cam->fuzzf) {
		fclose (cam->fuzzf);
		cam->fuzzf = NULL;
		cam->fuzzpending = 0;
	}
	return GP_OK;
}

static void
vcam_process_output(vcamera *cam) {
	ptpcontainer	ptp;
	int		i, j;

	if (cam->nroutbulk < 4)
		return; /* wait for more data */

	ptp.size = get_32bit_le (cam->outbulk);
	if (ptp.size > cam->nroutbulk)
		return; /* wait for more data */

	if (ptp.size < 12) {	/* No ptp command can be less than 12 bytes */
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR, __FUNCTION__, "input size was %d, minimum is 12", ptp.size);
		ptp_response (cam, PTP_RC_GeneralError, 0);
		memmove (cam->outbulk, cam->outbulk+ptp.size, cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	/* ptp:  4 byte size, 2 byte opcode, 2 byte type, 4 byte serial number */
	ptp.type  = get_16bit_le (cam->outbulk+4);
	ptp.code  = get_16bit_le (cam->outbulk+6);
	ptp.seqnr = get_32bit_le (cam->outbulk+8);

	if ((ptp.type != 1) && (ptp.type != 2)) { /* We want either CMD or DATA phase. */
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR, __FUNCTION__, "expected CMD or DATA, but type was %d", ptp.type);
		ptp_response (cam, PTP_RC_GeneralError, 0);
		memmove (cam->outbulk, cam->outbulk+ptp.size, cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	if ((ptp.code & 0x7000) != 0x1000) {
		/* not clear if normal cameras react like this */
		gp_log (GP_LOG_ERROR, __FUNCTION__, "OPCODE 0x%04x does not start with 0x1 or 0x9", ptp.code);
		ptp_response (cam, PTP_RC_GeneralError, 0);
		memmove (cam->outbulk, cam->outbulk+ptp.size, cam->nroutbulk-ptp.size);
		cam->nroutbulk -= ptp.size;
		return;
	}
	if (ptp.type == 1) {
		if ((ptp.size - 12) % 4) {
			/* not clear if normal cameras react like this */
			gp_log (GP_LOG_ERROR, __FUNCTION__, "SIZE-12 is not divisible by 4, but is %d", ptp.size-12);
			ptp_response (cam, PTP_RC_GeneralError, 0);
			memmove (cam->outbulk, cam->outbulk+ptp.size, cam->nroutbulk-ptp.size);
			cam->nroutbulk -= ptp.size;
			return;
		}
		if ((ptp.size - 12) / 4 >= 6) {
			/* not clear if normal cameras react like this */
			gp_log (GP_LOG_ERROR, __FUNCTION__, "(SIZE-12)/4 is %d, exceeds maximum arguments", (ptp.size-12)/4);
			ptp_response (cam, PTP_RC_GeneralError, 0);
			memmove (cam->outbulk, cam->outbulk+ptp.size, cam->nroutbulk-ptp.size);
			cam->nroutbulk -= ptp.size;
			return;
		}
		ptp.nparams = (ptp.size - 12)/4;
		for (i=0;i<ptp.nparams;i++)
			ptp.params[i] = get_32bit_le (cam->outbulk+12+i*4);
	}

	cam->nroutbulk -= ptp.size;

	/* call the opcode handler */
	for (j=0;j<sizeof(ptp_functions)/sizeof(ptp_functions[0]);j++) {
		struct ptp_function *funcs = ptp_functions[j].functions;

		if ((ptp_functions[j].type != GENERIC_PTP) && (ptp_functions[j].type != cam->type))
			continue;

		for (i=0;i<ptp_functions[j].nroffunctions;i++) {
			if (funcs[i].code == ptp.code) {
				if (ptp.type == 1) {
					funcs[i].write (cam, &ptp);
					memcpy(&cam->ptpcmd, &ptp, sizeof(ptp));
				} else {
					if (funcs[i].write_data == NULL) {
						gp_log (GP_LOG_ERROR, __FUNCTION__, "opcode 0x%04x received with dataphase, but no dataphase expected", ptp.code);
						ptp_response (cam, PTP_RC_GeneralError, 0);
					} else {
						funcs[i].write_data (cam, &cam->ptpcmd, cam->outbulk+12, ptp.size-12);
					}
				}
				return;
			}
		}
	}
	gp_log (GP_LOG_ERROR,__FUNCTION__,"received an unsupported opcode 0x%04x", ptp.code);
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
	/* in fuzzing mode, be less strict with unknown opcodes */
	ptp_response (cam, PTP_RC_OperationNotSupported, 0);
#else
	ptp_response (cam, PTP_RC_OK, 0);
#endif
}

static int
vcam_read(vcamera*cam, int ep, unsigned char *data, int bytes) {
	unsigned int	toread = bytes;

	if (cam->fuzzf) {
		unsigned int hasread;

		memset(data,0,toread);
		if (cam->fuzzmode == FUZZMODE_PROTOCOL) {
			fwrite(cam->inbulk, 1, toread, cam->fuzzf);
			/* fallthrough */
		} else {
			/* for reading fuzzer data */
			if (cam->fuzzpending) {
				toread = cam->fuzzpending;
				if (toread > bytes) toread = bytes;
				cam->fuzzpending -= toread;
				hasread = fread (data, 1, toread, cam->fuzzf);
			} else {
				hasread = fread (data, 1, 4, cam->fuzzf);
				if (hasread != 4)
					return 0;

				toread = data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);

				if (toread > bytes) {
					cam->fuzzpending = toread - bytes;
					toread = bytes;
				}
				if (toread <= 4)
					return toread;

				toread -= 4;

				hasread = fread(data + 4, 1, toread, cam->fuzzf);

				hasread += 4; /* readd size */
			}
#if 0
			for (i=0;i<toread;i++)
				data[i] ^= cam->inbulk[i];
#endif
			toread = hasread;

			return toread;
		}
	}

	/* Emulated PTP camera stuff */

	if (toread > cam->nrinbulk)
		toread = cam->nrinbulk;

	memcpy (data, cam->inbulk, toread);
	memmove (cam->inbulk, cam->inbulk + toread, (cam->nrinbulk - toread));
	cam->nrinbulk -= toread;
	return toread;
}

static int vcam_write(vcamera*cam, int ep, const unsigned char *data, int bytes) {
	/*gp_log_data("vusb", data, bytes, "data, vcam_write");*/
	if (!cam->outbulk) {
		cam->outbulk = malloc(bytes);
	} else {
		cam->outbulk = realloc(cam->outbulk,cam->nroutbulk + bytes);
	}
	memcpy(cam->outbulk + cam->nroutbulk, data, bytes);
	cam->nroutbulk += bytes;

	vcam_process_output(cam);

	return bytes;
}

struct ptp_interrupt {
	unsigned char		*data;
	int 			size;
	struct timeval		triggertime;
	struct ptp_interrupt	*next;
};

static struct ptp_interrupt *first_interrupt;

static int
ptp_inject_interrupt(vcamera*cam, int when, uint16_t code, int nparams, uint32_t param1, uint32_t transid) {
	struct ptp_interrupt	*interrupt, **pint;
	struct timeval		now;
	unsigned char		*data;
	int			x = 0;

	gp_log (GP_LOG_DEBUG, __FUNCTION__, "generate interrupt 0x%04x, %d params, param1 0x%08x, timeout=%d", code, nparams, param1, when);

	gettimeofday (&now, NULL);
	now.tv_usec += (when % 1000)*1000;
	now.tv_sec += when / 1000;
	if (now.tv_usec > 1000000) {
		now.tv_usec -= 1000000;
		now.tv_sec++;
	}

	data = malloc (0x10);
	x += put_32bit_le (data+x, 0x10);
	x += put_16bit_le (data+x, 4);
	x += put_16bit_le (data+x, code);
	x += put_32bit_le (data+x, transid);
	x += put_32bit_le (data+x, param1);

	interrupt = malloc (sizeof(struct ptp_interrupt));
	interrupt->data		= data;
	interrupt->size		= x;
	interrupt->triggertime	= now;
	interrupt->next		= NULL;

	/* Insert into list, sorted by trigger time, next triggering one first */
	pint = &first_interrupt;
	while (*pint) {
		if (now.tv_sec > (*pint)->triggertime.tv_sec) {
			pint = &((*pint)->next);
			continue;
		}
		if (	(now.tv_sec == (*pint)->triggertime.tv_sec) &&
			(now.tv_usec > (*pint)->triggertime.tv_usec)) {
			pint = &((*pint)->next);
			continue;
		}
		interrupt->next = *pint;
		*pint = interrupt;
		break;
	}
	if (!*pint) /* single entry */
		*pint = interrupt;
	return 1;
}

static int
vcam_readint(vcamera*cam, unsigned char *data, int bytes, int timeout) {
	struct timeval		now, end;
	int 			newtimeout, tocopy;
	struct ptp_interrupt	*pint;

	if (!first_interrupt) {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
		usleep (timeout*1000);
#endif
		return GP_ERROR_TIMEOUT;
	}
	gettimeofday (&now, NULL);
	end = now;
	end.tv_usec += (timeout % 1000)*1000;
	end.tv_sec += timeout / 1000;
	if (end.tv_usec > 1000000) {
		end.tv_usec -= 1000000;
		end.tv_sec++;
	}
	if (first_interrupt->triggertime.tv_sec > end.tv_sec) {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
		usleep (1000*timeout);
#endif
		return GP_ERROR_TIMEOUT;
	}
	if (	(first_interrupt->triggertime.tv_sec == end.tv_sec) &&
		(first_interrupt->triggertime.tv_usec > end.tv_usec)
	) {
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
		usleep (1000*timeout);
#endif
		return GP_ERROR_TIMEOUT;
	}
	newtimeout = (first_interrupt->triggertime.tv_sec - now.tv_sec)*1000 + (first_interrupt->triggertime.tv_usec - now.tv_usec)/1000;
	if (newtimeout > timeout)
		gp_log (GP_LOG_ERROR, __FUNCTION__, "miscalculated? %d vs %d", timeout, newtimeout);
	tocopy = first_interrupt->size;
	if (tocopy > bytes)
		tocopy = bytes;
	memcpy (data, first_interrupt->data, tocopy);
	pint = first_interrupt;
	first_interrupt = first_interrupt->next;
	free (pint->data);
	free (pint);
	return tocopy;
}

vcamera*
vcamera_new(vcameratype type) {
	vcamera *cam;

	cam = calloc(1,sizeof(vcamera));
	if (!cam) return NULL;

	read_tree(VCAMERADIR);

	cam->init = vcam_init;
	cam->exit = vcam_exit;
	cam->open = vcam_open;
	cam->close = vcam_close;

	cam->read = vcam_read;
	cam->readint = vcam_readint;
	cam->write = vcam_write;

	cam->type = type;
	cam->seqnr = 0;

	return cam;
}
