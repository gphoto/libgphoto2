

/* test_ptp */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <usb.h>
#include "ptp.h"


typedef struct _PTP_USB PTP_USB;
struct _PTP_USB {
	usb_dev_handle* handle;
	int inep;
	int outep;
	int intep;
};


void talk (struct usb_device*, int , int , int );
void init_ptp_usb (PTPParams*, PTP_USB*, struct usb_device*);
void init_usb(void);
void usage(char* progname);
void help(char* progname);
void list_devices(void);


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

static short
ptp_check_int (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;

	result = usb_bulk_read(ptp_usb->handle, ptp_usb->intep, bytes, size, 3000);
	if (result==0) result = usb_bulk_read(ptp_usb->handle, ptp_usb->intep, bytes, size, 3000);
	return (result);
}


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
	ptp_params->transaction_id=1;
	ptp_params->byteorder = PTP_DL_LE;

	if ((device_handle=usb_open(dev))){
		if (!device_handle) {
			perror("usb_open()");
			exit(0);
		}
		ptp_usb->handle=device_handle;
	}
}

void
init_usb()
{
	usb_init();
	usb_find_busses();
	usb_find_devices();
}

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
	"	-l, --list-devices	List all PTP devices\n"
	"	-h, --help		Print this help message\n"
	"\n");
}

void
list_devices()
{
	struct usb_bus *bus;
	struct usb_device *dev;
	PTPParams ptp_params;
	PTP_USB ptp_usb;
	int found=0;


	init_usb();
  	for (bus = usb_busses; bus; bus = bus->next)
    	for (dev = bus->devices; dev; dev = dev->next) {
		/* if it's a PTP device try to talk to it */
		if (dev->config->interface->altsetting->bInterfaceClass==6)
		{
			int i,n,ret;
			struct usb_endpoint_descriptor *ep;
			int inep=0, outep=0, intep=0;
			PTPDeviceInfo deviceinfo;

			if (!found){
				printf("Listing devices\n");
				printf("bus/device  idVendor/idProduct\n");
				found=1;
			}
      			printf("%s/%s     %04X/%04X\n",
				bus->dirname, dev->filename,
				dev->descriptor.idVendor,
				dev->descriptor.idProduct);

			ep = dev->config->interface->altsetting->endpoint;
			n=dev->config->interface->altsetting->bNumEndpoints;
			/* find endpoints */
			for (i=0;i<n;i++) {
/*				printf ("ep %.2i Adr %.2x Attr %.2i\n",i,
					ep[i].bEndpointAddress,
					ep[i].bmAttributes);
*/
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
			init_ptp_usb(&ptp_params, &ptp_usb, dev);
			ret=ptp_getdeviceinfo (&ptp_params, &deviceinfo);
			if (ret==PTP_RC_OK) {
				printf("%s\n",deviceinfo.Model);
			}
		}
	}
	if (!found) printf("Found no PTP devices\n");
}


/* main program  */

int
main(int argc, char ** argv)
{
	/* parse options */
	int option_index = 0,opt;
	static struct option loptions[] = {
		{"help",0,0,'h'},
		{"list-devices",0,0,'l'},
		{0,0,0,0}
	};
	
	while(1) {
		opt = getopt_long (argc, argv, "hl", loptions, &option_index);
		if (opt==-1) break;
	
		switch (opt) {
		case 'h':
			help(argv[0]);
			break;
		case 'l':
			list_devices();
		case '?':
			break;
		default:
			fprintf(stderr,"getopt returned character code 0%o\n",
				opt);
			break;
		}
	}
	if (argc==1) usage(argv[0]);
	return 0;
}
