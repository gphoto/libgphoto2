#ifndef _GP_PORT_H_
#define _GP_PORT_H_

#ifdef OS2
#include <gphoto2-portability-os2.h>
#include <os2.h>
#endif

/* Include the portability layer */
#include "gphoto2-portability.h"

/* Include the various headers for other devices */
#include "gphoto2-port-serial.h"
#include "gphoto2-port-parallel.h"
#include "gphoto2-port-network.h"
#include "gphoto2-port-usb.h"
#include "gphoto2-port-ieee1394.h"

#ifndef TRUE
#define TRUE (0==0)
#endif

#ifndef FALSE
#define FALSE (1==0)
#endif

/* Defines and enums
   --------------------------------------------------------------
   Note: this is a base set of return values.                      */

/* Return values */
/* Note: This lib should be allocated return values of 0 to -99 */

#define GP_OK		        0

#define	GP_ERROR               -1
#define GP_ERROR_TIMEOUT       -2


/* Debugging definitions for init */
#define GP_DEBUG_NONE		0
#define GP_DEBUG_LOW		1
#define GP_DEBUG_MEDIUM 	2
#define GP_DEBUG_HIGH 	        3

#define GP_PORT_MAX_BUF_LEN 4096 		/* max length of receive buffer */

/* Specify the types of devices */
typedef enum {
    GP_PORT_NONE        =      0,
    GP_PORT_SERIAL      = 1 << 0,
    GP_PORT_PARALLEL    = 1 << 1,             /* <- Not supported yet */
    GP_PORT_USB         = 1 << 2,
    GP_PORT_IEEE1394    = 1 << 3,             /* <- Not supported yet */
    GP_PORT_NETWORK     = 1 << 4,             /* <- Not supported yet */
} gp_port_type;


/* Device struct
   -------------------------------------------------------------- */

typedef struct {
	gp_port_type type;
	char name[64];
	char path[64];
	
	/* not used yet */
	int  argument_needed;
	char argument_description[128];
	char argument[128];

	/* don't touch */
	char library_filename[1024];
} gp_port_info;


/* Put the settings together in a union */
typedef union {
	gp_port_serial_settings	        serial;
        gp_port_parallel_settings	parallel;
        gp_port_network_settings	network;
	gp_port_usb_settings 	        usb;
	gp_port_ieee1394_settings 	ieee1394;
} gp_port_settings;

enum {
        GP_PORT_USB_IN_ENDPOINT,
	GP_PORT_USB_OUT_ENDPOINT
};

struct gp_port;
typedef struct gp_port gp_port;
struct gp_port_operations {
	int (*init)	(gp_port *);
	int (*exit)	(gp_port *);
	int (*open)	(gp_port *);
	int (*close)	(gp_port *);
	int (*read)	(gp_port *, char *, int);
	int (*write)	(gp_port *, char *, int);
	int (*update)   (gp_port *);

        /* Pointers to devices. Please note these are stubbed so there is
         no need to #ifdef GP_PORT_* anymore. */

	/* for serial and parallel devices */
	int (*get_pin)	 (gp_port *, int);
	int (*set_pin)	 (gp_port *, int, int);
	int (*send_break)(gp_port *, int);

	/* for USB devices */
	int (*find_device)(gp_port * dev, int idvendor, int idproduct);
	int (*clear_halt) (gp_port * dev, int ep);
	int (*msg_write)  (gp_port * dev, int value, char *bytes, int size);
	int (*msg_read)   (gp_port * dev, int value, char *bytes, int size);

};

typedef struct gp_port_operations gp_port_operations;

/* Function pointers for the dynamic libraries */
typedef int		 (*gp_port_ptr_type) 	();
typedef int 		 (*gp_port_ptr_list) 	(gp_port_info*, int *);
typedef gp_port_operations* (*gp_port_ptr_operations) ();

/* Specify the device information */
struct gp_port {
	/* This struct is available via wrappers. don't modify 
	   directly. */
        gp_port_type type;

	gp_port_operations *ops;

	gp_port_settings settings;
	gp_port_settings settings_pending;

	gp_port_settings settings_saved;

	int device_fd;
#ifdef WIN32
	HANDLE device_handle;
#else
	void *device_handle;
#endif
	int timeout; /* in milli seconds */

	void *library_handle;

	int debug_level;
};

/* Core functions
   -------------------------------------------------------------- */

	void gp_port_debug_printf (int target_debug_level, int debug_level, char *format, ...);
		/* issues debugging messages */

        char *gp_port_result_as_string (int result);
                /* Returns a string describing the error */

	int gp_port_init (int debug_level);
		/* Initializes the library.
			return values:
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

	int gp_port_count_get ();
		/* Get a count of available devices
			return values:
				  successful: valid gp_port_list struct
				unsuccessful: GP_ERROR
		*/

	int gp_port_info_get (int device_number, gp_port_info *info);
		/* Get information about a device
			return values:
				  successful: valid gp_port_list struct
				unsuccessful: GP_ERROR
		*/

gp_port *gp_port_new		(gp_port_type type);
		/* Create a new device of type "type"
			return values:
				  successful: valid gp_port struct
				unsuccessful: GP_ERROR
		*/

	int gp_port_free      	(gp_port *dev);
		/* Frees an IO device from memory
			return values:
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

	int gp_port_debug_set (gp_port *dev, int debug_level);
		/* 
			Set the debugging level specific to a device 
		*/

	int gp_port_open       	(gp_port *dev);
		/* Open the device for reading and writing
			return values:
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

	int gp_port_close      	(gp_port *dev);
		/* Close the device to prevent reading and writing
			return values:
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

       int gp_port_timeout_set 	(gp_port *dev, int millisec_timeout);
		/* Sets the read/write timeout
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

       int gp_port_timeout_get 	(gp_port *dev, int *millisec_timeout);
		/* Sets the read/write timeout
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

       int gp_port_settings_set	(gp_port *dev,
				 gp_port_settings settings);
		/* Sets the settings
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/


       int gp_port_settings_get	(gp_port *dev,
				 gp_port_settings *settings);
		/* Returns settings in "settings"
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

       int gp_port_write 		(gp_port *dev, char *bytes, int size);
		/* Writes "bytes" of size "size" to the device
			return values:
				  successful: GP_OK
				unsuccessful: GP_ERROR
		*/

       int gp_port_read		(gp_port *dev, char *bytes, int size);
		/* Reads "size" bytes in to "bytes" from the device
			return values:
				  successful: number of bytes read
				unsuccessful: GP_ERROR
		*/


/* Serial and Parallel specific functions
   -------------------------------------------------------------- */

	int gp_port_pin_get   (gp_port *dev, int pin);
		/* Give the status of pin from dev
			pin values:
				 see PIN_ constants in the various .h files	
			return values:
				  successful: status
				unsuccessful: GP_ERROR
		*/

	int gp_port_pin_set   (gp_port *dev, int pin, int level);
		/* set the status of pin from dev to level
			pin values:
				 see PIN_ constants in the various .h files	
			level values:
					0 for off
					1 for on
			return values:
				  successful: status
				unsuccessful: GP_ERROR
		*/

	int gp_port_send_break (gp_port *dev, int duration);
		/* send a break (duration is in seconds) */

/* USB specific functions
   -------------------------------------------------------------- */

        /* must port libusb to other platforms for this to drop-in */
	int gp_port_usb_find_device (gp_port * dev, int idvendor, int idproduct);
	int gp_port_usb_clear_halt  (gp_port * dev, int ep);
	int gp_port_usb_msg_write   (gp_port * dev, int value, char *bytes, int size);
	int gp_port_usb_msg_read    (gp_port * dev, int value, char *bytes, int size);




#endif /* _GP_PORT_H_ */


