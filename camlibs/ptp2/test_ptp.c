



/* test_ptp */

#include <stdio.h>
#include <stdlib.h>
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

#ifndef USB_CLASS_PTP
#define USB_CLASS_PTP		6
#endif

#define CR(result,error) {				\
			if((result)!=PTP_RC_OK) {	\
				fprintf(stderr,error);	\
				return;			\
			}				\
}

/* requested actions */
#define ACT_LIST_DEVICES	01
#define ACT_LIST_PROPERTIES	02

			
typedef struct _PTP_USB PTP_USB;
struct _PTP_USB {
	usb_dev_handle* handle;
	int inep;
	int outep;
	int intep;
};

/* some functions declarations to avoid warnings */

//void talk (struct usb_device*, int , int , int );
void usage(char* progname);
void help(char* progname);
struct usb_bus* init_usb(void);
void init_ptp_usb (PTPParams*, PTP_USB*, struct usb_device*);
void list_devices(short force);
void list_properties (int dev, int bus, short force);


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
	{PTP_DPC_ExposureIndex,		N_("Exposure Index (film speed ISO)")},
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
	{PTP_DPC_EK_ColorTemperature,	N_("EK Color Temperature")},
	{PTP_DPC_EK_DateTimeStampFormat,N_("EK Date Time Stamp Format")},
	{PTP_DPC_EK_BeepMode,		N_("EK Beep Mode")},
	{PTP_DPC_EK_VideoOut,		N_("EK Video Out")},
	{PTP_DPC_EK_PowerSaving,	N_("EK Power Saving")},
	{PTP_DPC_EK_UI_Language,	N_("EK UI Language")},
	{0,NULL}
};


void
usage(char* progname)
{
	printf("USAGE: %s [OPTION]\n\n",progname);
}

void
help(char* progname)
{
	printf("USAGE: %s [OPTION]\n\n",progname);
	printf("Options:\n"
	"	-h, --help		Print this help message\n"
	"	-l, --list-devices	List all PTP devices\n"
	"	-p, --list-properties	List all PTP device properties "
					"(e.g. focus mode)\n"
	"	-f, --force		force\n"
	"	-B, --bus=BUS-NUMBER	USB bus number\n"
	"	-D, --dev=DEV-NUMBER	USB assigned device number\n"
	"\n");
}


static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;

	result=usb_bulk_read(ptp_usb->handle, ptp_usb->inep, bytes, size,10000);
	if (result==0)
	result=usb_bulk_read(ptp_usb->handle, ptp_usb->inep, bytes, size,10000);
	if (result >= 0)
		return (PTP_RC_OK);
	else 
	{
		perror("usb_bulk_read");
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
		perror("usb_bulk_write");
		return PTP_ERROR_IO;
	}
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
PTPParams* ptp_params;

	ptp_params=malloc(sizeof(PTPParams));

	ptp_params->write_func=ptp_write_func;
	ptp_params->read_func=ptp_read_func;
	ptp_params->error_func=NULL;
	ptp_params->debug_func=NULL;
	ptp_params->sendreq_func=ptp_usb_sendreq;
	ptp_params->senddata_func=ptp_usb_senddata;
	ptp_params->getresp_func=ptp_usb_getresp;
	ptp_params->getdata_func=ptp_usb_getdata;
	ptp_params->data=&ptp_usb;
	ptp_params->transaction_id=1;
	ptp_params->byteorder = PTP_DL_LE;
	ptp_usb.inep=inep;
	ptp_usb.outep=outep;
	ptp_usb.intep=eventep;

	if ((device_handle=usb_open(dev))){
	if (!device_handle) {
		perror("usb_open");
		exit;
	}
	ptp_usb.handle=device_handle;
	
	ret=ptp_opensession (ptp_params, 1);
	if (ret!=PTP_RC_OK) return;

	sleep(3);
	printf("Checking event ep\n");

//	ret=usb_bulk_read(device_handle, eventep, buf, 16384, 5000);

	ret=ptp_check_int (buf, 16384, ptp_params->data);
	if (ret<=0) {
		perror ("bulk_read()");
	} else {
	printf ("READ %i bytes\n",ret);
	for (i=0;i<=ret;i++) {
		printf ("%x ",buf[i]);
	}
	printf("\n");
	}


	ptp_closesession (ptp_params);
	exit;
	} else {
		printf("DUPA\n");
		exit;
	}
}

#endif

void
init_ptp_usb (PTPParams* ptp_params, PTP_USB* ptp_usb, struct usb_device* dev)
{
	usb_dev_handle *device_handle;

	ptp_params->write_func=ptp_write_func;
	ptp_params->read_func=ptp_read_func;
	ptp_params->error_func=NULL;
	ptp_params->debug_func=NULL;
	ptp_params->sendreq_func=ptp_usb_sendreq;
	ptp_params->senddata_func=ptp_usb_senddata;
	ptp_params->getresp_func=ptp_usb_getresp;
	ptp_params->getdata_func=ptp_usb_getdata;
	ptp_params->data=ptp_usb;
	ptp_params->transaction_id=0;
	ptp_params->byteorder = PTP_DL_LE;

	if ((device_handle=usb_open(dev))){
		if (!device_handle) {
			perror("usb_open()");
			exit(0);
		}
		ptp_usb->handle=device_handle;
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
	PTPParams ptp_params;
	PTP_USB ptp_usb;
	int found=0;


	bus=init_usb();
  	for (; bus; bus = bus->next)
    	for (dev = bus->devices; dev; dev = dev->next) {
		/* if it's a PTP device try to talk to it */
		if ((dev->config->interface->altsetting->bInterfaceClass==
			USB_CLASS_PTP)||force)
		if (dev->descriptor.bDeviceClass!=USB_CLASS_HUB)
		{
			int n,ret;
			struct usb_endpoint_descriptor *ep;
			//int inep=0, outep=0, intep=0;
			PTPDeviceInfo deviceinfo;

			if (!found){
				printf("Listing devices...\n");
				printf("bus/dev\tvendorID/prodID\tdevice model");
				found=1;
			}
      			printf("\n%s/%s\t0x%04X/0x%04X\t",
				bus->dirname, dev->filename,
				dev->descriptor.idVendor,
				dev->descriptor.idProduct);

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
			init_ptp_usb(&ptp_params, &ptp_usb, dev);
			ret=ptp_getdeviceinfo (&ptp_params, &deviceinfo);
			if (ret==PTP_RC_OK) {
				printf("%s",deviceinfo.Model);
			}
		}
	}
	if (!found) printf("\nFound no PTP devices");
	printf("\n\n");
}

const char*
get_property_description(uint16_t dpc);
const char*
get_property_description(uint16_t dpc)
{
	int i;

	for (i=0; ptp_device_properties[i].txt!=NULL; i++)
		if (ptp_device_properties[i].dpc==dpc)
			return (ptp_device_properties[i].txt);
	return NULL;
}

void
list_properties (int busn, int devn, short force)
{
	PTPParams ptp_params;
	PTP_USB ptp_usb;
	PTPDeviceInfo deviceinfo;
	struct usb_device *dev;
	const char* dpd;
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

	init_ptp_usb(&ptp_params, &ptp_usb, dev);
	CR(ptp_getdeviceinfo (&ptp_params, &deviceinfo),"Could not get"
		"device info\n");
	printf("%s\n",deviceinfo.Model);
	for (i=0; i<deviceinfo.DevicePropertiesSupported_len;i++){
		dpd=get_property_description(deviceinfo.
					DevicePropertiesSupported[i]);
		if (dpd!=NULL) 
			printf("%s\n",dpd);
		else
			printf("0x%04x\n",deviceinfo.
					DevicePropertiesSupported[i]);
	}
}

/* main program  */

int
main(int argc, char ** argv)
{
	int busn=0,devn=0;
	int action=0;
	short force=0;
	/* parse options */
	int option_index = 0,opt;
	static struct option loptions[] = {
		{"help",0,0,'h'},
		{"dev",1,0,'D'},
		{"bus",1,0,'B'},
		{"force",0,0,'f'},
		{"list-devices",0,0,'l'},
		{"list-properties",0,0,'p'},
		{0,0,0,0}
	};
	
	while(1) {
		opt = getopt_long (argc, argv, "hlpfD:B:", loptions, &option_index);
		if (opt==-1) break;
	
		switch (opt) {
		case 'h':
			help(argv[0]);
			break;
		case 'l':
			action=ACT_LIST_DEVICES;
			break;
		case 'D':
			devn=strtol(optarg,NULL,10);
			break;
		case 'B':
			busn=strtol(optarg,NULL,10);
			break;
		case 'p':
			action=ACT_LIST_PROPERTIES;
			break;
		case 'f':
			force=~force;
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
		usage(argv[0]);
		return 0;
	}
	switch (action) {
		case ACT_LIST_DEVICES:
			list_devices(force);
			break;
		case ACT_LIST_PROPERTIES:
			list_properties(busn,devn,force);
			break;
	}

	return 0;
}



