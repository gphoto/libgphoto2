#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <usb.h>

#include "ptp_usb.h"

struct usb_device *device;
int vendor, product;

PTPParams* ptp_params;
PTP_USB ptp_usb;

PTPResult
ptp_read (PTPReq* req, unsigned int size, void *data); 
PTPResult
ptp_write (PTPReq* req, unsigned int size, void *data); 
void error(char *x,...);


query (usb_dev_handle* handle) {
	PTPObjectHandles* ptp_objecthandles=malloc(sizeof(PTPObjectHandles));
	PTPObjectInfo* ptp_objectinfo;
	int ret,i, file;
	char filename[256];
	char* object;

	ptp_params->io_write = ptp_write;
	ptp_params->io_read = ptp_read;
	ptp_params->ptp_error=error;
	ptp_params->io_data=&ptp_usb;
	ptp_params->id=1;

	ptp_usb.handle=handle;
	ptp_usb.ep=0x01;


// Open session number 1
	if (ptp_opensession(ptp_params, 1)==PTP_RC_OK) printf("INIT OK\n");
	else exit(-1);
// getindex
	if (ptp_getobjecthandles(ptp_params, ptp_objecthandles)==PTP_RC_OK) 
		printf("GETINDEX OK\nArray of %i elements\n",ptp_objecthandles->n);
		else exit(-1);


	for (i=0;i<ptp_objecthandles->n;i++)
	{
		printf("Dwnloading object %i\n",i);
		ptp_objectinfo=malloc(sizeof(PTPObjectInfo));
		ptp_getobjectinfo(ptp_params, ptp_objecthandles, i, ptp_objectinfo);

		object=malloc(ptp_objectinfo->ObjectCompressedSize+PTP_REQ_HDR_LEN);
		ret=ptp_getobject(ptp_params, ptp_objecthandles,
			ptp_objectinfo, i, object);
		if (ret=PTP_RC_OK) printf("OBJECT GET OK!!!\n"); else exit(-1);
		sprintf(filename,"image%i.jpg",i);
		file=open(filename, O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU|S_IRGRP);
		write(file, object+PTP_REQ_HDR_LEN, 
		ptp_objectinfo->ObjectCompressedSize);
		close(file);
		free(object);
		free(ptp_objectinfo);
	}

	
	if (ptp_closesession(ptp_params)==PTP_RC_OK) printf ("CLOSE OK\n");

	exit(-1);

}

usb_dev_handle* open_device(struct usb_device *device) {
	usb_dev_handle *handle;
	
	if (!(handle = usb_open(device))) {
		printf("open device failed!\n");
		return 0;
	}
	printf ("device opened!\n");
	return handle;
}



int scan_bus (struct usb_bus* bus) {
	struct usb_device* roottree = bus->devices;
	struct usb_device *device;

	for( device = roottree;device;device=device->next) {
		if ((device->descriptor.idVendor == vendor) &&
			(device->descriptor.idProduct == product)) {
			usb_dev_handle *device_handle;

			printf("found device %s on bus %s (idVendor 0x%x idProduct 0x%x)\n",
			device->filename, device->bus->dirname,
			device->descriptor.idVendor,
			device->descriptor.idProduct );
			// XXX
			if (device_handle=open_device(device)) {
				// XXX
				query(device_handle);
			}
		} else {
			printf ("No match!\n");
		}
	}
}

PTPResult
ptp_read (PTPReq* req, unsigned int size, void *data) 
{
	PTP_USB *usb;
	int ret;

	usb=(PTP_USB*)data;

	ret=usb_bulk_read(usb->handle, usb->ep, (char *)req,
		size, PTP_USB_TIMEOUT);

	if (ret<0) return PTP_ERROR;
	return PTP_OK;
}


PTPResult
ptp_write (PTPReq* req, unsigned int size, void *data) 
{
	PTP_USB *usb;
	int ret;

	usb=(PTP_USB*)data;

	ret=usb_bulk_write(usb->handle, usb->ep, (char *)req,
		size, PTP_USB_TIMEOUT);

	if (ret<0) return PTP_ERROR;
	return PTP_OK;
}

void error(char *x,...) {
	char mesg[4096];
	va_list ap;

	va_start(ap, x);
	vsnprintf(mesg,4095, x, ap);
	fprintf(stderr,"%s:\n",mesg);
	perror("");
	va_end(ap);
	fflush(NULL);
}


main (int argc, char** argv)  {
	struct usb_bus* bus;

	ptp_params=malloc(sizeof(PTPParams));


	usb_init();
	usb_find_busses();
	usb_find_devices();
	
	if (argc<2) {
		vendor=0x40a;
		product=0x500;
	} else {
		vendor=strtol(argv[1], NULL, 16);
		product=strtol(argv[2], NULL, 16);
	}

	for( bus = usb_busses; bus; bus = bus->next ) {
		printf("found bus %s\n", bus->dirname );
		scan_bus (bus);

	}
}
