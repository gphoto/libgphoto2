/* ptpcam.c
 *
 * Copyright (C) 2001-2003 Mariusz Woloszyn <emsi@ipartners.pl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <usb.h>

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

#include "ptp.h"

/* some defines comes here */

/* USB interface class */
#ifndef USB_CLASS_PTP
#define USB_CLASS_PTP		6
#endif

/* USB control message data phase direction */
#ifndef USB_DP_HTD
#define USB_DP_HTD		(0x00 << 7)	// host to device
#endif
#ifndef USB_DP_DTH
#define USB_DP_DTH		(0x01 << 7)	// device to host
#endif

/* PTP class specific requests */
#ifndef USB_REQ_DEVICE_RESET
#define USB_REQ_DEVICE_RESET		0x66
#endif
#ifndef USB_REQ_GET_DEVICE_STATUS
#define USB_REQ_GET_DEVICE_STATUS	0x67
#endif

/* USB Feature selector HALT */
#ifndef USB_FEATURE_HALT
#define USB_FEATURE_HALT	0x00
#endif

/* Check value and Return on error */
#define CR(result,error) {						\
			if((result)!=PTP_RC_OK) {			\
				fprintf(stderr,"ERROR: "error);		\
				usb_release_interface(ptp_usb.handle,	\
		dev->config->interface->altsetting->bInterfaceNumber);	\
				return;					\
			}						\
}

/* Check value and Return -1 on error */
#define CRR(result) {							\
			if((result)!=PTP_RC_OK) {			\
				return -1;				\
			}						\
}

/* Check value and Report (PTP) Error if needed */
#define CRE(result) {							\
			uint16_t r;					\
			r=(result);					\
			if (r!=PTP_RC_OK)				\
				ptp_perror(&params,r);			\
}

/* Check value and Continue on error */
#define CC(result,error) {						\
			if((result)!=PTP_RC_OK) {			\
				fprintf(stderr,"ERROR: "error);		\
				usb_release_interface(ptp_usb.handle,	\
		dev->config->interface->altsetting->bInterfaceNumber);	\
				continue;					\
			}						\
}

/* error reporting macro */
#define ERROR(error) fprintf(stderr,"ERROR: "error);				

/* printing value type */
#define PTPCAM_PRINT_HEX	00
#define PTPCAM_PRINT_DEC	01

/* property value printing macros */
#define PRINT_PROPVAL_DEC(value)	\
		print_propval(dpd.DataType, value,			\
		PTPCAM_PRINT_DEC)

#define PRINT_PROPVAL_HEX(value)					\
		print_propval(dpd.DataType, value,			\
		PTPCAM_PRINT_HEX)


/* requested actions */
#define ACT_DEVICE_RESET	01
#define ACT_LIST_DEVICES	02
#define ACT_LIST_PROPERTIES	03
#define ACT_GETSET_PROPERTY	04


			
typedef struct _PTP_USB PTP_USB;
struct _PTP_USB {
	usb_dev_handle* handle;
	int inep;
	int outep;
	int intep;
};

/* some functions declarations to avoid warnings */

//void talk (struct usb_device*, int , int , int );
void usage(void);
void help(void);
struct usb_bus* init_usb(void);
void init_ptp_usb (PTPParams*, PTP_USB*, struct usb_device*);
void list_devices(short force);
void list_properties (int dev, int bus, short force);


// one global variable (yes, I know it sucks)

short verbose=0;


void
usage()
{
	printf("USAGE: ptpcam [OPTION]\n\n");
}

void
help()
{
	printf("USAGE: ptpcam [OPTION]\n\n");
	printf("Options:\n"
	"  -h, --help                   Print this help message\n"
	"  -B, --bus=BUS-NUMBER         USB bus number\n"
	"  -D, --dev=DEV-NUMBER         USB assigned device number\n"
	"  -r, --reset                  Reset the device\n"
	"  -l, --list-devices           List all PTP devices\n"
	"  -p, --list-properties        List all PTP device properties\n"
	"                               "
				"(e.g. focus mode, focus distance, etc.)\n"
	"  -s, --show-property=NUMBER   Display property details "
					"(or set its value,\n"
	"                               if used in conjunction with --val)\n"
	"  --set-property=NUMBER        Set property value (--val required)\n"
	"  --val=VALUE                  Property value\n"
	"  -f, --force                  Talk to non PTP devices\n"
	"  -v, --verbose                Be verbosive (print more debug)\n"
	"\n");
}


static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;

	result=usb_bulk_read(ptp_usb->handle, ptp_usb->inep, bytes, size,3000);
	if (result==0)
	result=usb_bulk_read(ptp_usb->handle, ptp_usb->inep, bytes, size,3000);
	if (result >= 0)
		return (PTP_RC_OK);
	else 
	{
		if (verbose) perror("usb_bulk_read");
		return PTP_ERROR_IO;
	}
}

static short
ptp_write_func (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;

	result=usb_bulk_write(ptp_usb->handle,ptp_usb->outep,bytes,size,3000);
	if (result >= 0)
		return (PTP_RC_OK);
	else 
	{
		if (verbose) perror("usb_bulk_write");
		return PTP_ERROR_IO;
	}
}

void
debug (void *data, const char *format, va_list args);
void
debug (void *data, const char *format, va_list args)
{
	if (verbose<2) return;
	vfprintf (stderr, format, args);
	fprintf (stderr,"\n");
	fflush(stderr);
}

void
error (void *data, const char *format, va_list args);
void
error (void *data, const char *format, va_list args)
{
//	if (!verbose) return;
	vfprintf (stderr, format, args);
	fprintf (stderr,"\n");
	fflush(stderr);
}

#if 0
static short
ptp_check_int (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;

	result = usb_bulk_read(ptp_usb->handle, ptp_usb->intep, bytes, size, 3000);
	if (result==0) result = usb_bulk_read(ptp_usb->handle, ptp_usb->intep, bytes, size, 3000);
	return (result);
}

#endif

#if 0
void talk (struct usb_device *dev, int inep, int outep, int eventep) {
char buf[65535];
int ret=-1;
int i;
usb_dev_handle *device_handle;
PTP_USB ptp_usb;
PTPParams* params;

	params=malloc(sizeof(PTPParams));

	params->write_func=ptp_write_func;
	params->read_func=ptp_read_func;
	params->error_func=NULL;
	params->debug_func=NULL;
	params->sendreq_func=ptp_usb_sendreq;
	params->senddata_func=ptp_usb_senddata;
	params->getresp_func=ptp_usb_getresp;
	params->getdata_func=ptp_usb_getdata;
	params->data=&ptp_usb;
	params->transaction_id=1;
	params->byteorder = PTP_DL_LE;
	ptp_usb.inep=inep;
	ptp_usb.outep=outep;
	ptp_usb.intep=eventep;

	if ((device_handle=usb_open(dev))){
	if (!device_handle) {
		perror("usb_open");
		exit;
	}
	ptp_usb.handle=device_handle;
	
	ret=ptp_opensession (params, 1);
	if (ret!=PTP_RC_OK) return;

	sleep(3);
	printf("Checking event ep\n");

//	ret=usb_bulk_read(device_handle, eventep, buf, 16384, 5000);

	ret=ptp_check_int (buf, 16384, params->data);
	if (ret<=0) {
		perror ("bulk_read()");
	} else {
	printf ("READ %i bytes\n",ret);
	for (i=0;i<=ret;i++) {
		printf ("%x ",buf[i]);
	}
	printf("\n");
	}


	ptp_closesession (params);
	exit;
	} else {
		printf("DUPA\n");
		exit;
	}
}

#endif

void
init_ptp_usb (PTPParams* params, PTP_USB* ptp_usb, struct usb_device* dev)
{
	usb_dev_handle *device_handle;

	params->write_func=ptp_write_func;
	params->read_func=ptp_read_func;
	params->error_func=error;
	params->debug_func=debug;
	params->sendreq_func=ptp_usb_sendreq;
	params->senddata_func=ptp_usb_senddata;
	params->getresp_func=ptp_usb_getresp;
	params->getdata_func=ptp_usb_getdata;
	params->data=ptp_usb;
	params->transaction_id=0;
	params->byteorder = PTP_DL_LE;

	if ((device_handle=usb_open(dev))){
		if (!device_handle) {
			perror("usb_open()");
			exit(0);
		}
		ptp_usb->handle=device_handle;
		usb_claim_interface(device_handle,
			dev->config->interface->altsetting->bInterfaceNumber);
	}
}

struct usb_bus*
init_usb()
{
	usb_init();
	usb_find_busses();
	usb_find_devices();
	return (usb_get_busses());
}

/*
   find_device() returns the pointer to a usb_device structure matching
   given busn, devicen numbers. If any or both of arguments are 0 then the
   first matching PTP device structure is returned. 
*/
struct usb_device*
find_device (int busn, int devicen, short force);
struct usb_device*
find_device (int busn, int devn, short force)
{
	struct usb_bus *bus;
	struct usb_device *dev;

	bus=init_usb();
	for (; bus; bus = bus->next)
	for (dev = bus->devices; dev; dev = dev->next)
	if ((dev->config->interface->altsetting->bInterfaceClass==
		USB_CLASS_PTP)||force)
	if (dev->descriptor.bDeviceClass!=USB_CLASS_HUB)
	{
		int curbusn, curdevn;

		curbusn=strtol(bus->dirname,NULL,10);
		curdevn=strtol(dev->filename,NULL,10);

		if (devn==0) {
			if (busn==0) return dev;
			if (curbusn==busn) return dev;
		} else {
			if ((busn==0)&&(curdevn==devn)) return dev;
			if ((curbusn==busn)&&(curdevn==devn)) return dev;
		}
	}
	return NULL;
}

void
find_endpoints(struct usb_device *dev, int* inep, int* outep, int* intep);
void
find_endpoints(struct usb_device *dev, int* inep, int* outep, int* intep)
{
	int i,n;
	struct usb_endpoint_descriptor *ep;

	ep = dev->config->interface->altsetting->endpoint;
	n=dev->config->interface->altsetting->bNumEndpoints;

	for (i=0;i<n;i++) {
	if (ep[i].bmAttributes==USB_ENDPOINT_TYPE_BULK)	{
		if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
			USB_ENDPOINT_DIR_MASK)
			*inep=ep[i].bEndpointAddress;
		if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==0)
			*outep=ep[i].bEndpointAddress;
		} else if (ep[i].bmAttributes==USB_ENDPOINT_TYPE_INTERRUPT){
			if ((ep[i].bEndpointAddress&USB_ENDPOINT_DIR_MASK)==
				USB_ENDPOINT_DIR_MASK)
				*intep=ep[i].bEndpointAddress;
		}
	}
}

void
list_devices(short force)
{
	struct usb_bus *bus;
	struct usb_device *dev;
	int found=0;


	bus=init_usb();
  	for (; bus; bus = bus->next)
    	for (dev = bus->devices; dev; dev = dev->next) {
		/* if it's a PTP device try to talk to it */
		if ((dev->config->interface->altsetting->bInterfaceClass==
			USB_CLASS_PTP)||force)
		if (dev->descriptor.bDeviceClass!=USB_CLASS_HUB)
		{
			int n;
			struct usb_endpoint_descriptor *ep;
			PTPParams params;
			PTP_USB ptp_usb;
			//int inep=0, outep=0, intep=0;
			PTPDeviceInfo deviceinfo;

			if (!found){
				printf("Listing devices...\n");
				printf("bus/dev\tvendorID/prodID\tdevice model\n");
				found=1;
			}
			ep = dev->config->interface->altsetting->endpoint;
			n=dev->config->interface->altsetting->bNumEndpoints;
			/* find endpoints */
/*			for (i=0;i<n;i++) {
				if (ep[i].bmAttributes==2) {
					if ((ep[i].bEndpointAddress&0x80)==0x80)
						inep=ep[i].bEndpointAddress;
					if ((ep[i].bEndpointAddress&0x80)==0)
						outep=ep[i].bEndpointAddress;
				} else if (ep[i].bmAttributes==3) {
					if ((ep[i].bEndpointAddress&0x80)==0x80)
						intep=ep[i].bEndpointAddress;
				}
			}
			ptp_usb.inep=inep;
			ptp_usb.outep=outep;
			ptp_usb.intep=intep;
*/
			find_endpoints(dev,&ptp_usb.inep,&ptp_usb.outep,
				&ptp_usb.intep);
			init_ptp_usb(&params, &ptp_usb, dev);

			CC(ptp_opensession (&params,1),
				"Could not open session!\n"
				"Try to reset the camera.\n");
			CC(ptp_getdeviceinfo (&params, &deviceinfo),
				"Could not get device info!\n");

      			printf("%s/%s\t0x%04X/0x%04X\t%s\n",
				bus->dirname, dev->filename,
				dev->descriptor.idVendor,
				dev->descriptor.idProduct, deviceinfo.Model);

			CC(ptp_closesession(&params),
				"Could not close session!\n");
			usb_release_interface(ptp_usb.handle,
			dev->config->interface->altsetting->bInterfaceNumber);
		}
	}
	if (!found) printf("\nFound no PTP devices\n");
	printf("\n");
}

const char*
get_property_description(PTPParams* params, uint16_t dpc);
const char*
get_property_description(PTPParams* params, uint16_t dpc)
{
	int i;
	// Device Property descriptions
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties[] = {
		{PTP_DPC_Undefined,		N_("PTP Undefined Property")},
		{PTP_DPC_BatteryLevel,		N_("Battery Level")},
		{PTP_DPC_FunctionalMode,	N_("Functional Mode")},
		{PTP_DPC_ImageSize,		N_("Image Size")},
		{PTP_DPC_CompressionSetting,	N_("Compression Setting")},
		{PTP_DPC_WhiteBalance,		N_("White Balance")},
		{PTP_DPC_RGBGain,		N_("RGB Gain")},
		{PTP_DPC_FNumber,		N_("F-Number")},
		{PTP_DPC_FocalLength,		N_("Focal Length")},
		{PTP_DPC_FocusDistance,		N_("Focus Distance")},
		{PTP_DPC_FocusMode,		N_("Focus Mode")},
		{PTP_DPC_ExposureMeteringMode,	N_("Exposure Metering Mode")},
		{PTP_DPC_FlashMode,		N_("Flash Mode")},
		{PTP_DPC_ExposureTime,		N_("Exposure Time")},
		{PTP_DPC_ExposureProgramMode,	N_("Exposure Program Mode")},
		{PTP_DPC_ExposureIndex,
					N_("Exposure Index (film speed ISO)")},
		{PTP_DPC_ExposureBiasCompensation,
					N_("Exposure Bias Compensation")},
		{PTP_DPC_DateTime,		N_("Date Time")},
		{PTP_DPC_CaptureDelay,		N_("Pre-Capture Delay")},
		{PTP_DPC_StillCaptureMode,	N_("Still Capture Mode")},
		{PTP_DPC_Contrast,		N_("Contrast")},
		{PTP_DPC_Sharpness,		N_("Sharpness")},
		{PTP_DPC_DigitalZoom,		N_("Digital Zoom")},
		{PTP_DPC_EffectMode,		N_("Effect Mode")},
		{PTP_DPC_BurstNumber,		N_("Burst Number")},
		{PTP_DPC_BurstInterval,		N_("Burst Interval")},
		{PTP_DPC_TimelapseNumber,	N_("Timelapse Number")},
		{PTP_DPC_TimelapseInterval,	N_("Timelapse Interval")},
		{PTP_DPC_FocusMeteringMode,	N_("Focus Metering Mode")},
		{PTP_DPC_UploadURL,		N_("Upload URL")},
		{PTP_DPC_Artist,		N_("Artist")},
		{PTP_DPC_CopyrightInfo,		N_("Copyright Info")},
		{0,NULL}
	};
	struct {
		uint16_t dpc;
		const char *txt;
	} ptp_device_properties_EK[] = {
		{PTP_DPC_EK_ColorTemperature,	N_("EK Color Temperature")},
		{PTP_DPC_EK_DateTimeStampFormat,
					N_("EK Date Time Stamp Format")},
		{PTP_DPC_EK_BeepMode,		N_("EK Beep Mode")},
		{PTP_DPC_EK_VideoOut,		N_("EK Video Out")},
		{PTP_DPC_EK_PowerSaving,	N_("EK Power Saving")},
		{PTP_DPC_EK_UI_Language,	N_("EK UI Language")},
		{0,NULL}
	};

	for (i=0; ptp_device_properties[i].txt!=NULL; i++)
		if (ptp_device_properties[i].dpc==dpc)
			return (ptp_device_properties[i].txt);

	if (params->deviceinfo.VendorExtensionID==PTP_VENDOR_EASTMAN_KODAK)
	for (i=0; ptp_device_properties_EK[i].txt!=NULL; i++)
		if (ptp_device_properties_EK[i].dpc==dpc)
			return (ptp_device_properties_EK[i].txt);
	return NULL;
}

void
list_properties (int busn, int devn, short force)
{
	PTPParams params;
	PTP_USB ptp_usb;
	struct usb_device *dev;
	const char* propdesc;
	int i;

	printf("Listing properties...\n");
#ifdef DEBUG
	printf("dev %i\tbus %i\n",devn,busn);
#endif
	dev=find_device(busn,devn,force);
	if (dev==NULL) {
		fprintf(stderr,"could not find any device matching given "
		"bus/dev numbers\n");
		exit(-1);
	}
	find_endpoints(dev,&ptp_usb.inep,&ptp_usb.outep,&ptp_usb.intep);

	init_ptp_usb(&params, &ptp_usb, dev);
	CR(ptp_opensession (&params,1),
		"Could not open session!\n");
	CR(ptp_getdeviceinfo (&params, &params.deviceinfo),
		"Could not get device info\n");
	printf("Quering: %s\n",params.deviceinfo.Model);
	for (i=0; i<params.deviceinfo.DevicePropertiesSupported_len;i++){
		propdesc=get_property_description(&params,
			params.deviceinfo.DevicePropertiesSupported[i]);
		if (propdesc!=NULL) 
			printf("0x%04x : %s\n",params.deviceinfo.
				DevicePropertiesSupported[i], propdesc);
		else
			printf("0x%04x : 0x%04x\n",params.deviceinfo.
				DevicePropertiesSupported[i],
				params.deviceinfo.
					DevicePropertiesSupported[i]);
	}
	CR(ptp_closesession(&params), "Could not close session!\n");
	usb_release_interface(ptp_usb.handle,
		dev->config->interface->altsetting->bInterfaceNumber);
}

short
print_propval (uint16_t datatype, void* value, short hex);
short
print_propval (uint16_t datatype, void* value, short hex)
{
	switch (datatype) {
		case PTP_DTC_INT8:
			printf("%hhi",*(char*)value);
			return 0;
		case PTP_DTC_UINT8:
			printf("%hhu",*(unsigned char*)value);
			return 0;
		case PTP_DTC_INT16:
			printf("%hi",*(int16_t*)value);
			return 0;
		case PTP_DTC_UINT16:
			if (hex==PTPCAM_PRINT_HEX)
				printf("0x%04hX (%hi)",*(uint16_t*)value,
					*(uint16_t*)value);
			else
				printf("%hi",*(uint16_t*)value);
			return 0;
		case PTP_DTC_INT32:
			printf("%i",*(int32_t*)value);
			return 0;
		case PTP_DTC_UINT32:
			if (hex==PTPCAM_PRINT_HEX)
				printf("0x%08X (%i)",*(uint32_t*)value,
					*(uint32_t*)value);
			else
				printf("%i",*(uint32_t*)value);
			return 0;
		case PTP_DTC_STR:
			printf("\"%s\"",(char *)value);
	}
	return -1;
}

uint16_t
set_property (PTPParams* params,
		uint16_t property, char* value, uint16_t datatype);
uint16_t
set_property (PTPParams* params,
		uint16_t property, char* value, uint16_t datatype)
{
	void* val=NULL;

	switch(datatype) {
	case PTP_DTC_INT8:
		val=malloc(sizeof(int8_t));
		*(int8_t*)val=(int8_t)strtol(value,NULL,0);
		break;
	case PTP_DTC_UINT8:
		val=malloc(sizeof(uint8_t));
		*(uint8_t*)val=(uint8_t)strtol(value,NULL,0);
		break;
	case PTP_DTC_INT16:
		val=malloc(sizeof(int16_t));
		*(int16_t*)val=(int16_t)strtol(value,NULL,0);
		break;
	case PTP_DTC_UINT16:
		val=malloc(sizeof(uint16_t));
		*(uint16_t*)val=(uint16_t)strtol(value,NULL,0);
		break;
	case PTP_DTC_INT32:
		val=malloc(sizeof(int32_t));
		*(int32_t*)val=(int32_t)strtol(value,NULL,0);
		break;
	case PTP_DTC_UINT32:
		val=malloc(sizeof(uint32_t));
		*(uint32_t*)val=(uint32_t)strtol(value,NULL,0);
		break;
	case PTP_DTC_STR:
		val=(void *)value;
	}
	return(ptp_setdevicepropvalue(params, property, val, datatype));
	free(val);
	return 0;
}

void
getset_property (int busn,int devn,uint16_t property,char* value,short force);
void
getset_property (int busn,int devn,uint16_t property,char* value,short force)
{
	PTPParams params;
	PTP_USB ptp_usb;
	struct usb_device *dev;
	PTPDevicePropDesc dpd;
	const char* propdesc;

#ifdef DEBUG
	printf("dev %i\tbus %i\n",devn,busn);
#endif
	dev=find_device(busn,devn,force);
	if (dev==NULL) {
		fprintf(stderr,"could not find any device matching given "
		"bus/dev numbers\n");
		exit(-1);
	}
	find_endpoints(dev,&ptp_usb.inep,&ptp_usb.outep,&ptp_usb.intep);

	init_ptp_usb(&params, &ptp_usb, dev);
	CR(ptp_opensession (&params,1),
		"Could not open session!\nTry to reset the camera.\n");
	CR(ptp_getdeviceinfo (&params, &params.deviceinfo),
		"Could not get device info\nTry to reset the camera.\n");
	propdesc=get_property_description(&params,property);
	printf("Camera: %s",params.deviceinfo.Model);
	if ((devn!=0)||(busn!=0)) 
		printf(" (bus %i, dev %i)\n",busn,devn);
	else
		printf("\n");
	if (!ptp_property_issupported(&params, property)||propdesc==NULL)
	{
		fprintf(stderr,"The dvice does not support this property!\n");
		CR(ptp_closesession(&params), "Could not close session!\n"
			"Try to reset the camera.\n");
		return;
	}
	printf("Property '%s'\n",propdesc);
	memset(&dpd,0,sizeof(dpd));
	CR(ptp_getdevicepropdesc(&params,property,&dpd),
		"Could not get device property description!\n"
		"Try to reset the camera.\n");
	printf ("Data type is 0x%04x\n",dpd.DataType);
	printf ("Current value is ");
	if (dpd.FormFlag==PTP_DPFF_Enumeration)
		PRINT_PROPVAL_DEC(dpd.CurrentValue);
	else 
		PRINT_PROPVAL_HEX(dpd.CurrentValue);
	printf("\n");
	printf ("Factory default value is ");
	if (dpd.FormFlag==PTP_DPFF_Enumeration)
		PRINT_PROPVAL_DEC(dpd.FactoryDefaultValue);
	else 
		PRINT_PROPVAL_HEX(dpd.FactoryDefaultValue);
	printf("\n");
	printf("The property is ");
	if (dpd.GetSet==PTP_DPGS_Get)
		printf ("read only");
	else
		printf ("settable");
	switch (dpd.FormFlag) {
	case PTP_DPFF_Enumeration:
		printf (", enumerated. Allowed values are:\n");
		{
			int i;
			for(i=0;i<dpd.FORM.Enum.NumberOfValues;i++){
				PRINT_PROPVAL_HEX(
				dpd.FORM.Enum.SupportedValue[i]);
				printf("\n");
			}
		}
		break;
	case PTP_DPFF_Range:
		printf (", within range:\n");
		PRINT_PROPVAL_DEC(dpd.FORM.Range.MinimumValue);
		printf(" - ");
		PRINT_PROPVAL_DEC(dpd.FORM.Range.MaximumValue);
		printf("; step size: ");
		PRINT_PROPVAL_DEC(dpd.FORM.Range.StepSize);
		printf("\n");
		break;
	case PTP_DPFF_None:
		printf(".\n");
	}
	if (value) {
		printf("Setting proprty value to '%s'\n",value);
		CRE(set_property(&params, property, value, dpd.DataType));
	}
	ptp_free_devicepropdesc(&dpd);
	CR(ptp_closesession(&params), "Could not close session!\n"
	"Try to reset the camera.\n");
	usb_release_interface(ptp_usb.handle,
		dev->config->interface->altsetting->bInterfaceNumber);
}

int
usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status);
int
usb_get_endpoint_status(PTP_USB* ptp_usb, int ep, uint16_t* status)
{
	 return (usb_control_msg(ptp_usb->handle,
		USB_DP_DTH|USB_RECIP_ENDPOINT, USB_REQ_GET_STATUS,
		USB_FEATURE_HALT, ep, (char *)status, 2, 3000));
}

int
usb_clear_stall_feature(PTP_USB* ptp_usb, int ep);
int
usb_clear_stall_feature(PTP_USB* ptp_usb, int ep)
{

	return (usb_control_msg(ptp_usb->handle,
		USB_RECIP_ENDPOINT, USB_REQ_CLEAR_FEATURE, USB_FEATURE_HALT,
		ep, NULL, 0, 3000));
}

int
usb_ptp_get_device_status(PTP_USB* ptp_usb, uint16_t* devstatus);
int
usb_ptp_get_device_status(PTP_USB* ptp_usb, uint16_t* devstatus)
{
	return (usb_control_msg(ptp_usb->handle,
		USB_DP_DTH|USB_TYPE_CLASS|USB_RECIP_INTERFACE,
		USB_REQ_GET_DEVICE_STATUS, 0, 0,
		(char *)devstatus, 4, 3000));
}

int
usb_ptp_device_reset(PTP_USB* ptp_usb);
int
usb_ptp_device_reset(PTP_USB* ptp_usb)
{
	return (usb_control_msg(ptp_usb->handle,
		USB_TYPE_CLASS|USB_RECIP_INTERFACE,
		USB_REQ_DEVICE_RESET, 0, 0, NULL, 0, 3000));
}

void
reset_device (int busn, int devn, short force);
void
reset_device (int busn, int devn, short force)
{
	PTPParams params;
	PTP_USB ptp_usb;
	struct usb_device *dev;
	uint16_t status;
	uint16_t devstatus[2] = {0,0};
	int ret;

#ifdef DEBUG
	printf("dev %i\tbus %i\n",devn,busn);
#endif
	dev=find_device(busn,devn,force);
	if (dev==NULL) {
		fprintf(stderr,"could not find any device matching given "
		"bus/dev numbers\n");
		exit(-1);
	}
	find_endpoints(dev,&ptp_usb.inep,&ptp_usb.outep,&ptp_usb.intep);

	init_ptp_usb(&params, &ptp_usb, dev);
	
	// get device status (devices likes that regardless of its result)
	usb_ptp_get_device_status(&ptp_usb,devstatus);
	
	// check the in endpoint status
	ret = usb_get_endpoint_status(&ptp_usb,ptp_usb.inep,&status);
	if (ret<0) perror ("usb_get_endpoint_status()");
	// and clear the HALT condition if happend
	if (status) {
		printf("Resetting input pipe!\n");
		ret=usb_clear_stall_feature(&ptp_usb,ptp_usb.inep);
		if (ret<0)perror ("usb_clear_stall_feature()");
	}
	status=0;
	// check the out endpoint status
	ret = usb_get_endpoint_status(&ptp_usb,ptp_usb.outep,&status);
	if (ret<0) perror ("usb_get_endpoint_status()");
	// and clear the HALT condition if happend
	if (status) {
		printf("Resetting output pipe!\n");
		ret=usb_clear_stall_feature(&ptp_usb,ptp_usb.outep);
		if (ret<0)perror ("usb_clear_stall_feature()");
	}
	status=0;
	// check the interrupt endpoint status
	ret = usb_get_endpoint_status(&ptp_usb,ptp_usb.intep,&status);
	if (ret<0)perror ("usb_get_endpoint_status()");
	// and clear the HALT condition if happend
	if (status) {
		printf ("Resetting interrupt pipe!\n");
		ret=usb_clear_stall_feature(&ptp_usb,ptp_usb.intep);
		if (ret<0)perror ("usb_clear_stall_feature()");
	}

	// get device status (now there should be some results)
	ret = usb_ptp_get_device_status(&ptp_usb,devstatus);
	if (ret<0) 
		perror ("usb_ptp_get_device_status()");
	else	{
		if (devstatus[1]==PTP_RC_OK) 
			printf ("Device status OK\n");
		else
			printf ("Device status 0x%04x\n",devstatus[1]);
	}
	
	// finally reset the device (that clears prevoiusly opened sessions)
	ret = usb_ptp_device_reset(&ptp_usb);
	if (ret<0)perror ("usb_ptp_device_reset()");
	// get device status (devices likes that regardless of its result)
	usb_ptp_get_device_status(&ptp_usb,devstatus);

	usb_release_interface(ptp_usb.handle,
		dev->config->interface->altsetting->bInterfaceNumber);
}

/* main program  */

int
main(int argc, char ** argv)
{
	int busn=0,devn=0;
	int action=0;
	short force=0;
	uint16_t property=0;
	char* value=NULL;
	/* parse options */
	int option_index = 0,opt;
	static struct option loptions[] = {
		{"help",0,0,'h'},
		{"bus",1,0,'B'},
		{"dev",1,0,'D'},
		{"reset",0,0,'r'},
		{"list-devices",0,0,'l'},
		{"list-properties",0,0,'p'},
		{"show-property",1,0,'s'},
		{"set-property",1,0,'s'},
		{"val",1,0,0},
		{"force",0,0,'f'},
		{"verbose",2,0,'v'},
		{0,0,0,0}
	};
	
	while(1) {
		opt = getopt_long (argc, argv, "hlpfrs:D:B:v::", loptions, &option_index);
		if (opt==-1) break;
	
		switch (opt) {
		case 0:
			if (!(strcmp("val",loptions[option_index].name)))
				value=strdup(optarg);
			break;
		case 'h':
			help();
			break;
		case 'B':
			busn=strtol(optarg,NULL,10);
			break;
		case 'D':
			devn=strtol(optarg,NULL,10);
			break;
		case 'f':
			force=~force;
			break;
		case 'v':
			if (optarg) 
				verbose=strtol(optarg,NULL,10);
			else
				verbose=1;
			printf("VERBOSE LEVEL  = %i\n",verbose);
			break;
		case 'r':
			action=ACT_DEVICE_RESET;
			break;
		case 'l':
			action=ACT_LIST_DEVICES;
			break;
		case 'p':
			action=ACT_LIST_PROPERTIES;
			break;
		case 's':
			action=ACT_GETSET_PROPERTY;
			property=strtol(optarg,NULL,16);
			break;
		case '?':
			break;
		default:
			fprintf(stderr,"getopt returned character code 0%o\n",
				opt);
			break;
		}
	}
	if (argc==1) {
		usage();
		return 0;
	}
	switch (action) {
		case ACT_DEVICE_RESET:
			reset_device(busn,devn,force);
			break;
		case ACT_LIST_DEVICES:
			list_devices(force);
			break;
		case ACT_LIST_PROPERTIES:
			list_properties(busn,devn,force);
			break;
		case ACT_GETSET_PROPERTY:
			getset_property(busn,devn,property,value,force);
			break;
	}

	return 0;
}
