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
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <gphoto2.h>

#include "util.h"
#include "psa50.h"


/****  new stuff ********/
struct camera_to_usb {
	  char *name;
	  unsigned short idVendor;
	  unsigned short idProduct;
} camera_to_usb[] = {
		{ "Canon PowerShot S10", 0x04A9, 0x3041 },
		{ "Canon PowerShot S20", 0x04A9, 0x3043 },
		{ "Canon PowerShot G1", 0x04A9, 0x3048 },
		{ "Canon Digital IXUS", 0x4A9, 0x3047 },
		{ "Canon IXY DIGITAL", 0x4A9, 0x3046 },
		{ "Canon PowerShot S100", 0x4A9, 0x3045 }
};
/*******  new stuff *********/


/****************************************************************************
 *
 * static global storage area
 *
 ****************************************************************************/

// gp_port *gdev;
gp_port_settings settings;


void serial_flush_input(gp_port *gdev)
{
}

void serial_flush_output(gp_port *gdev)
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

int canon_serial_change_speed(gp_port *gdev, int speed)
{
  /* set speed */
  gp_port_settings_get(gdev, &settings);
  settings.serial.speed = speed;
  gp_port_settings_set(gdev, settings);
  
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
int canon_serial_get_cts(gp_port *gdev)
{
  int level;
  gp_port_pin_get(gdev,PIN_CTS, &level);
  return (level);
}

/*****************************************************************************
 * 
 * canon_usb_probe
 * 
 * adapted from the digita driver for gphoto2
 * 
 * looks for USB cameras
 * 
 * gdev a valid gp_port
 * i    an entry of camera_to_usb
 * 
 * return 1 on success, 0 on failure
 ****************************************************************************/
int canon_usb_probe(gp_port *gdev, int i)
{
	
  if (i >= sizeof(camera_to_usb) / sizeof(struct camera_to_usb))
    goto err;
  
  if (gp_port_usb_find_device(gdev, camera_to_usb[i].idVendor,
			      camera_to_usb[i].idProduct) == GP_OK) {
    //		printf("found '%s' @ %s/%s\n", camera_to_usb[i].name,
    //			   gdev->usb_device->bus->dirname, gdev->usb_device->filename);
    printf("found '%s' @\n", camera_to_usb[i].name);
    
    return 1;
  }
  
  
 err:
  fprintf(stderr, "unable to find any compatible USB cameras\n");
  
  return 0;		
}

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
  int ret;
  int i;
  char msg[65536];
  //    char mem;
  char buffer[65536];
  gp_port_settings settings;
  
  gp_debug_printf(GP_DEBUG_LOW,"canon","Initializing the camera.\n");
	
  //	gp_port_init();
	
  switch (canon_comm_method) {
  case CANON_USB:
    
    if ((ret = gp_port_new(&(cs->gdev), GP_PORT_USB) != GP_OK))
      return ret;
	
    for (i = 0; i < sizeof(camera_to_usb) / sizeof(struct camera_to_usb); 
	 i++) {
      fprintf(stderr, "canon: %s, %s\n", camera->model,
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
    
    gp_port_settings_set(cs->gdev, settings);
    if (gp_port_open(cs->gdev) != GP_OK) {
      fprintf(stderr,"Camera used by other USB device!\n");
      //exit(1);
      return -1;
    }
    
    gp_port_usb_msg_read(cs->gdev,0x55,msg,1);
    //      fprintf(stderr,"%c\n",msg[0]);
    gp_port_usb_msg_read(cs->gdev,0x1,msg,0x58);
    gp_port_usb_msg_write(cs->gdev,0x11,msg+0x48,0x10);
    gp_port_read(cs->gdev, buffer, 0x44);
    //      fprintf(stderr,"Antal b: %x\n",buffer[0]);
    if (buffer[0]==0x54)
      gp_port_read(cs->gdev, buffer, 0x40);
    return 0;
    /* #else
       return -1;*/
    
    return -1;
    break;
  case CANON_SERIAL_RS232:
  default:
    
    if (!devname) {
      fprintf(stderr, "INVALID device string (NULL)\n");
      return -1;
    }
    
    gp_debug_printf(GP_DEBUG_LOW,"canon","canon_init_serial(): Using serial port on %s\n", devname);
    
    if ((ret = gp_port_new(&(cs->gdev), GP_PORT_SERIAL)) != GP_OK)
      return ret;
    
    strcpy(settings.serial.port, devname);
    settings.serial.speed = 9600;
    settings.serial.bits = 8;
    settings.serial.parity = 0;
    settings.serial.stopbits = 1;
    
    gp_port_settings_set(cs->gdev, settings); /* Sets the serial device name */
    if ( gp_port_open(cs->gdev) == GP_ERROR) {      /* open the device */
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
int canon_serial_close(gp_port *gdev)
{
  gp_port_close(gdev);
  gp_port_free(gdev);
  
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
  
  gp_port_close(cs->gdev);
  
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
  
  if (cs->dump_packets == 1)
    dump_hex(camera,"canon_serial_send()", buf, len);
  
  /* the A50 does not like to get too much data in a row at 115200
   * The S10 and S20 do not have this problem */
  if (sleep>0 && cs->slow_send == 1) {
    for(i=0;i<len;i++) {
      gp_port_write(cs->gdev,(char*)buf,1);
      buf++;
      usleep(sleep);
    }
  } else {
    gp_port_write(cs->gdev,(char*)buf,len);
  }
  
  return 0;
}


/**
 * Sets the timeout, in miliseconds.
 */
void serial_set_timeout(gp_port *gdev, int to)
{
  //	struct canon_info *cs = (struct canon_info*)camera->camlib_data;
  
  gp_port_timeout_set(gdev,to);
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
int canon_serial_get_byte(gp_port *gdev)
{
  static unsigned char cache[512];
  static unsigned char *cachep = cache;
  static unsigned char *cachee = cache;
  int recv;
  
  /* if still data in cache, get it */
  if (cachep < cachee) {
    return (int) *cachep++;
  }
  
  recv = gp_port_read(gdev, cache, 1);
  if (recv == GP_ERROR || recv == GP_ERROR_IO_TIMEOUT)
    return -1;

  cachep = cache;
  cachee = cache + recv;
  
  if (recv) {
    return (int) *cachep++;
  }

  return -1;
}

/****************************************************************************
 *
 * End of file: serial.c
 *
 ****************************************************************************/
