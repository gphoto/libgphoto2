/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camera.c
 *
 * Copyright (c) 2015 Marcus Meissner <marcus@jet.franken.de>
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

#include <stdlib.h>
#include <unistd.h>
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

static uint32_t get_32bit_le(unsigned char *data) {
	return	data[0]	| (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static uint16_t get_16bit_le(unsigned char *data) {
	return	data[0]	| (data[1] << 8);
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

static int put_string(unsigned char *data, char *str) {
	int i;

	if (strlen(str)>255)
		gp_log (GP_LOG_ERROR, "put_string", "string length is longer than 255 characters");

	data[0] = strlen (str);
	for (i=0;i<data[0];i++)
		put_16bit_le(data+1+2*i,str[i]);

	return 1+strlen(str)*2;
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
ptp_response(vcamera *cam, uint16_t code, int nparams) {
	unsigned char	*offset;
	int 		x = 0;

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
	/* FIXME: put params */

	cam->seqnr++;
}

#define PTP_RC_OK					0x2001
#define PTP_RC_GeneralError 				0x2002
#define PTP_RC_SessionNotOpen          		 	0x2003
#define PTP_RC_OperationNotSupported    		0x2005
#define PTP_RC_InvalidStorageId				0x2008
#define PTP_RC_InvalidObjectHandle			0x2009
#define PTP_RC_SpecificationByFormatUnsupported         0x2014
#define PTP_RC_InvalidParentObject			0x201A
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
static int ptp_getobjecthandles_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getstorageids_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getstorageinfo_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getobjectinfo_write(vcamera *cam, ptpcontainer *ptp);
static int ptp_getobject_write(vcamera *cam, ptpcontainer *ptp);

struct ptp_function {
	int	code;
	int	(*write)(vcamera *cam, ptpcontainer *ptp);
} ptp_functions[] = {
	{0x1001,	ptp_deviceinfo_write		},
	{0x1002,	ptp_opensession_write		},
	{0x1003,	ptp_closesession_write		},
	{0x1004,	ptp_getstorageids_write		},
	{0x1005,	ptp_getstorageinfo_write	},
	{0x1007,	ptp_getobjecthandles_write	},
	{0x1008,	ptp_getobjectinfo_write		},
	{0x1009,	ptp_getobject_write		},
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
	DIR			*dir;
	struct dirent		*de;

	dir = opendir(path);
	if (!dir) return;
	while ((de=readdir(dir))) {
		if (!strcmp(de->d_name,".")) continue;
		if (!strcmp(de->d_name,"..")) continue;

		cur = malloc(sizeof(struct ptp_dirent));
		if (!cur) break;
		cur->name = strdup(de->d_name);
		cur->fsname = malloc(strlen(path)+1+strlen(de->d_name)+1);
		strcpy(cur->fsname,path);
		strcat(cur->fsname,"/");
		strcat(cur->fsname,de->d_name);
		cur->id = ptp_objectid++;
		cur->next = first_dirent;
		cur->parent = parent;
		first_dirent = cur;
		if (-1 == stat(cur->fsname, &cur->stbuf))
			continue;
		if (S_ISDIR(cur->stbuf.st_mode))
			read_directories(cur->fsname, cur); /* recurse! */
	}
	closedir(dir);
}

static void
read_tree(char *path) {
	first_dirent = malloc(sizeof(struct ptp_dirent));
	first_dirent->name = strdup("");
	first_dirent->fsname = strdup(path);
	first_dirent->id = ptp_objectid++;
	first_dirent->next = NULL;
	read_directories(path,first_dirent);
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
	int		x = 0, i;
	uint16_t	*opcodes;
	uint16_t	imageformats[1];

	CHECK_PARAM_COUNT(0);

	/* Session does not need to be open for GetDeviceInfo */

	/* Getdeviceinfo is special. it can be called with transid 0 outside of the session. */
	if ((ptp->seqnr != 0) && (ptp->seqnr != cam->seqnr)) {
		/* not clear if normal cameras react like this */						\
		gp_log (GP_LOG_ERROR,"vcam_process_output", "seqnr %d was sent, expected was %d", ptp->seqnr, cam->seqnr);\
		ptp_response(cam,PTP_RC_GeneralError,0);							\
		return 1;
	}
	data = malloc(2000);

	x += put_16bit_le(data+x,100);		/* StandardVersion */
	x += put_32bit_le(data+x,0);		/* VendorExtensionID */
	x += put_16bit_le(data+x,0);		/* VendorExtensionVersion */
	x += put_string(data+x,"GPhoto-VirtualCamera: 1.0;");	/* VendorExtensionDesc */
	x += put_16bit_le(data+x,0);		/* FunctionalMode */

	opcodes = malloc(sizeof(ptp_functions)/sizeof(ptp_functions[0])*sizeof(uint16_t));
	for (i=0;i<sizeof(ptp_functions)/sizeof(ptp_functions[0]);i++)
		opcodes[i] = ptp_functions[i].code;
	x += put_16bit_le_array(data+x,opcodes,sizeof(ptp_functions)/sizeof(ptp_functions[0]));	/* OperationsSupported */
	x += put_16bit_le_array(data+x,NULL,0);		/* EventsSupported */
	x += put_16bit_le_array(data+x,NULL,0);		/* DevicePropertiesSupported */
	x += put_16bit_le_array(data+x,NULL,0);		/* CaptureFormats */

	imageformats[0] = 0x3801;
	x += put_16bit_le_array(data+x,imageformats,1);	/* ImageFormats */

	x += put_string(data+x,"GPhoto");		/* Manufacturer */
	x += put_string(data+x,"VirtualCamera");	/* Model */
	x += put_string(data+x,"2.5.9");		/* DeviceVersion */
	x += put_string(data+x,"0.1");			/* DeviceVersion */
	x += put_string(data+x,"1");			/* SerialNumber */

	ptp_senddata(cam,0x1001,data,x);
	free (data);
	ptp_response(cam,PTP_RC_OK,0);
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

	data = malloc(200);

	if (ptp->params[0] != 0x00010001) {
		gp_log (GP_LOG_ERROR,__FUNCTION__, "invalid storage id 0x%08x", ptp->params[1]);
		ptp_response(cam,PTP_RC_InvalidStorageId,0);
		return 1;
	}

	x += put_16bit_le (data+x, 3);	/* StorageType: Fixed RAM */
	x += put_16bit_le (data+x, 3);	/* FileSystemType: Generic Hierarchical */
	x += put_16bit_le (data+x, 2);	/* AccessCapability: R/O with object deletion */
	x += put_64bit_le (data+x, 0x42424242);	/* MaxCapacity */
	x += put_64bit_le (data+x, 0x21212121);	/* FreeSpaceInBytes */
	x += put_32bit_le (data+x, 1000);	/* FreeSpaceInImages */
	x += put_string (data+x, "GPhoto Virtual Camera Storage");	/* StorageDescription */
	x += put_string (data+x, "GPhoto Virtual Camera Storage Label");	/* VolumeLabel */

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
	uint16_t 		ofc;
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
		else
			ofc = 0x3000;
	}
	x += put_16bit_le (data+x, ofc);
	x += put_16bit_le (data+x, 0); 	/* ProtectionStatus, no protection */
	x += put_32bit_le (data+x, cur->stbuf.st_size); 	/* ObjectCompressedSize */
	x += put_16bit_le (data+x, 0); 	/* ThumbFormat */
	x += put_32bit_le (data+x, 0); 	/* ThumbCompressedSize */
	x += put_32bit_le (data+x, 0); 	/* ThumbPixWidth */
	x += put_32bit_le (data+x, 0); 	/* ThumbPixHeight */
	x += put_32bit_le (data+x, 0); 	/* ImagePixWidth */
	x += put_32bit_le (data+x, 0); 	/* ImagePixHeight */
	x += put_32bit_le (data+x, 0); 	/* ImageBitDepth */
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


/********************************************************************************************/

static int vcam_init(vcamera* cam) {
	return GP_OK;
}

static int vcam_exit(vcamera* cam) {
	return GP_OK;
}

static int vcam_open(vcamera* cam) {
	return GP_OK;
}

static int vcam_close(vcamera* cam) {
	return GP_OK;
}

static void
vcam_process_output(vcamera *cam) {
	ptpcontainer	ptp;
	int		i;

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

	cam->nroutbulk -= ptp.size;

	/* call the opcode handler */

	for (i=0;i<sizeof (ptp_functions) / sizeof (ptp_functions[0]);i++) {
		if (ptp_functions[i].code == ptp.code) {
			ptp_functions[i].write (cam,&ptp);
			return;
		}
	}
	gp_log (GP_LOG_ERROR,__FUNCTION__,"received an unsupported opcode 0x%04x", ptp.code);
	ptp_response (cam, PTP_RC_OperationNotSupported, 0);
}

static int vcam_read(vcamera*cam, int ep, char *data, int bytes) {
	int	toread = bytes;

	if (toread > cam->nrinbulk)
		toread = cam->nrinbulk;
	memcpy (data, cam->inbulk, toread);
	memmove (cam->inbulk, cam->inbulk + toread, (cam->nrinbulk - toread));
	cam->nrinbulk -= toread;
	return toread;
}

static int vcam_write(vcamera*cam, int ep, const char *data, int bytes) {
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

vcamera*
vcamera_new(void) {
	vcamera *cam;

	cam = calloc(1,sizeof(vcamera));
	if (!cam) return NULL;

	read_tree("/usr/share/libgphoto2/vcamera");

	cam->init = vcam_init;
	cam->exit = vcam_exit;
	cam->open = vcam_open;
	cam->close = vcam_close;

	cam->read = vcam_read;
	cam->write = vcam_write;

	cam->seqnr = 0;

	return cam;
}
