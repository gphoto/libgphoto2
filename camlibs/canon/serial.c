/****************************************************************************
 *
 * File: serial.c
 *
 * Serial communication layer.
 *
 ****************************************************************************/

/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <gphoto2.h>

#include "util.h"
#include "psa50.h"


/****  new stuff ********/
#include <gpio.h>
struct camera_to_usb {
	  char *name;
	  unsigned short idVendor;
	  unsigned short idProduct;
} camera_to_usb[] = {
		{ "Canon PowerShot S10", 0x04A9, 0x3041 },
		{ "Canon PowerShot S20", 0x04A9, 0x3043 }
};
/*******  new stuff *********/


/****************************************************************************
 *
 * static global storage area
 *
 ****************************************************************************/

// gpio_device *gdev;
gpio_device_settings settings;


void serial_flush_input(gpio_device *gdev)
{
}

void serial_flush_output(gpio_device *gdev)
{
}

/*****************************************************************************
 *
 * canon_serial_change_speed
 *
 * change the speed of the communication.
 *
 * speed - the new speed
 *
 * Returns 1 on success.
 * Returns 0 on any error.
 *
 ****************************************************************************/

int canon_serial_change_speed(gpio_device *gdev, int speed)
{
         /* set speed */
	gpio_get_settings(gdev, &settings);
	settings.serial.speed = speed;
	gpio_set_settings(gdev, settings);
	
	usleep(70000);
	
    return 1;
}


/*****************************************************************************
 *
 * canon_serial_get_cts
 *
 * Gets the status of the CTS (Clear To Send) line on the serial port.
 *
 * CTS is "1" when the camera is ON, and "0" when it is OFF.
 *
 * Returns 1 on CTS high.
 * Returns 0 on CTS low.
 *
 ****************************************************************************/
int canon_serial_get_cts(gpio_device *gdev)
{
        return gpio_get_pin(gdev,PIN_CTS);
}

/*****************************************************************************
 * 
 * canon_usb_probe
 * 
 * adapted from the digita driver for gphoto2
 * 
 * looks for USB cameras
 * 
 * gdev a valid gpio_device
 * i    an entry of camera_to_usb
 * 
 * return 1 on success, 0 on failure
 ****************************************************************************/
#ifdef GPIO_USB
int canon_usb_probe(gpio_device *gdev, int i)
{
	
	if (i >= sizeof(camera_to_usb) / sizeof(struct camera_to_usb))
	  goto err;

	if (gpio_usb_find_device(gdev, camera_to_usb[i].idVendor,
							 camera_to_usb[i].idProduct) == GPIO_OK) {
		printf("found '%s' @ %d/%d\n", camera_to_usb->name,
			   gdev->usb_device->bus->busnum, gdev->usb_device->devicenum);
		return 1;
	}

	
err:
	fprintf(stderr, "unable to find any compatible USB cameras\n");
	
	return 0;		
}
#endif
/*****************************************************************************
 *
 * canon_serial_init
 *
 * Initializes the given serial or USB device.
 *
 * devname - the name of the device to open
 *
 * Returns 0 on success.
 * Returns -1 on any error.
 *
 ****************************************************************************/

int canon_serial_init(Camera *camera, const char *devname)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
#ifdef GPIO_USB
	int i;
    char msg[65536];
//    char mem;
    char buffer[65536];
#endif
    gpio_device_settings settings;

	debug_message(camera,"Initializing the camera.\n");
	
	gpio_init();
	
	switch (canon_comm_method) {
	 case CANON_USB:
#ifdef GPIO_USB
		cs->gdev = gpio_new(GPIO_DEVICE_USB);
		if (!cs->gdev) {
			return -1;
		}
		
		for (i = 0; i < sizeof(camera_to_usb) / sizeof(struct camera_to_usb); i++) {
			fprintf(stderr, "digita: %s, %s\n", camera->model,
					camera_to_usb[i].name);
			
			if (!strcmp(camera->model, camera_to_usb[i].name))
			  break;
		}
		
		if (!canon_usb_probe(cs->gdev, i))
		  return -1;
		
		settings.usb.inep = 0x81;
		settings.usb.outep = 0x02;
		settings.usb.config = 1;
		settings.usb.interface = 0;
		settings.usb.altsetting = 0;
		
        /*      canon_send = canon_usb_send;
		 canon_read = canon_usb_read; */
		
		gpio_set_settings(cs->gdev, settings);
		if (gpio_open(cs->gdev) < 0) {
			fprintf(stderr,"Camera used by other USB device!\n");
			//exit(1);
			return -1;
        }
		
        gpio_usb_msg_read(cs->gdev,0x55,msg,1);
        //      fprintf(stderr,"%c\n",msg[0]);
        gpio_usb_msg_read(cs->gdev,0x1,msg,0x58);
        gpio_usb_msg_write(cs->gdev,0x11,msg+0x48,0x10);
        gpio_read(cs->gdev, buffer, 0x44);
        //      fprintf(stderr,"Antal b: %x\n",buffer[0]);
        if (buffer[0]==0x54)
		    gpio_read(cs->gdev, buffer, 0x40);
        return 0;
        /* #else
		 return -1;*/
#else
        fprintf(stderr,"This computer does not support USB, please try to select 'RS-232'\n"
                       " in the configuration panel (Configure/Configure camera...).\n");
        return -1;
#endif
        break;
		case CANON_SERIAL_RS232:
		default:
		
		if (!devname) {
			fprintf(stderr, "INVALID device string (NULL)\n");
			return -1;
		}
		
		debug_message(camera,"canon_init_serial(): Using serial port on %s\n", devname);
		
		cs->gdev = gpio_new(GPIO_DEVICE_SERIAL);
		
		if (!cs->gdev) {
			return -1;
		}
		strcpy(settings.serial.port, devname);
		settings.serial.speed = 9600;
		settings.serial.bits = 8;
		settings.serial.parity = 0;
		settings.serial.stopbits = 1;
		
		gpio_set_settings(cs->gdev, settings); /* Sets the serial device name */
		if ( gpio_open(cs->gdev) == GPIO_ERROR) {      /* open the device */
			perror("Unable to open the serial port");
			return -1;
		}
		
		return 0;
   }
}

/****************************************************************************
 *
 * canon_serial_close
 *
 * Close and free the device on exit
 *
 *
 ****************************************************************************/
int canon_serial_close(gpio_device *gdev)
{
       gpio_close(gdev);
       gpio_free(gdev);

       return 0;
}

/*****************************************************************************
 *
 * canon_serial_restore
 *
 * Restores the saved settings for the serial device
 *
 * Returns 0 on success.
 * Returns -1 on any error.
 *
 ****************************************************************************/

int canon_serial_restore(Camera *camera)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
    gpio_close(cs->gdev);

    return 0;
}

/*****************************************************************************
 *
 * canon_serial_send
 *
 * Send the given buffer with given length over the serial line.
 *
 * buf   - the raw data buffer to send
 * len   - the length of the buffer
 * sleep - time in usec to wait between characters
 *
 * Returns 0 on success, -1 on error.
 *
 ****************************************************************************/

int canon_serial_send(Camera *camera, const unsigned char *buf, int len, int sleep)
{
	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	int i;
	
    dump_hex(camera,"canon_serial_send()", buf, len);

        /* the A50 does not like to get too much data in a row
         * The S10 and S20 do not have this problem */
    if (sleep>0 && cs->model == CANON_PS_A50) {
		for(i=0;i<len;i++) {
			gpio_write(cs->gdev,buf,1);
			buf++;
			usleep(sleep);
		}
    } else {
		gpio_write(cs->gdev,buf,len);
    }
	
    return 0;
}


/**
 * Sets the timeout, in miliseconds.
 */
void serial_set_timeout(gpio_device *gdev, int to)
{
//	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
	
	gpio_set_timeout(gdev,to);
}

/*****************************************************************************
 *
 * canon_serial_get_byte
 *
 * Get the next byte from the serial line.
 * Actually the fucntion reads chunks of data and keeps them in a cache.
 * Only one byte per call will be returned.
 *
 * Returns the byte on success, -1 on error.
 *
 ****************************************************************************/
int canon_serial_get_byte(gpio_device *gdev)
{
    static unsigned char cache[512];
    static unsigned char *cachep = cache;
    static unsigned char *cachee = cache;
    int recv;

    /* if still data in cache, get it */
    if (cachep < cachee)
    {
        return (int) *cachep++;
    }


    recv = gpio_read(gdev, cache, 1);
    if (recv == GPIO_ERROR || recv == GPIO_TIMEOUT)
        return -1;
        cachep = cache;
        cachee = cache + recv;

        if (recv)
        {
            return (int) *cachep++;
        }

    return -1;
}

/****************************************************************************
 *
 * End of file: serial.c
 *
 ****************************************************************************/
