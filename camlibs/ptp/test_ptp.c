

/* test_ptp */

#include <stdio.h>
#include <usb.h>
#include "ptp.h"

typedef struct _PTP_USB PTP_USB;
struct _PTP_USB {
	usb_dev_handle* handle;
	int inep;
	int outep;
};

static short
ptp_read_func (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;

	result=usb_bulk_read(ptp_usb->handle, ptp_usb->inep, bytes, size, 3000);
	if (result==0) result=usb_bulk_read(ptp_usb->handle, ptp_usb->inep, bytes, size, 3000);
	if (result >= 0)
		return (PTP_RC_OK);
	else return PTP_ERROR_IO;
}

static short
ptp_write_func (unsigned char *bytes, unsigned int size, void *data)
{
	int result;
	PTP_USB *ptp_usb=(PTP_USB *)data;

	result=usb_bulk_write(ptp_usb->handle, ptp_usb->outep, bytes, size, 3000);
	if (result >= 0)
		return (PTP_RC_OK);
	else return PTP_ERROR_IO;

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
	ptp_params->data=&ptp_usb;
	ptp_params->transaction_id=1;
	ptp_params->byteorder = PTP_DL_LE;
	ptp_usb.inep=inep;
	ptp_usb.outep=outep;

	if ((device_handle=usb_open(dev))){
	if (!device_handle) {
		perror("usb_open");
		exit;
	}
	ptp_usb.handle=device_handle;
	
	ptp_opensession (ptp_params, 1);

	sleep(4);
	printf("Checking event ep\n");
	ret=usb_bulk_read(device_handle, eventep, buf, 16384, 100);
	if (ret<=0) {
		perror ("bulk_read():");
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

int main(int argc, char ** argv)
{
  struct usb_bus *bus;
  struct usb_device *dev;
  int vendor, product;

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

  printf("bus/device  idVendor/idProduct\n");
  for (bus = usb_busses; bus; bus = bus->next) {
    for (dev = bus->devices; dev; dev = dev->next) {
      printf("%s/%s     %04X/%04X\n", bus->dirname, dev->filename,
	dev->descriptor.idVendor, dev->descriptor.idProduct);
      if (dev->descriptor.idVendor==vendor &&
	      dev->descriptor.idProduct==product)
	      	talk(dev, strtol(argv[3], NULL, 16),
		strtol(argv[4], NULL, 16), strtol(argv[5], NULL, 16));
    }
  }

  return 0;
}

