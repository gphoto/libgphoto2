#ifndef _GP_PORT_H_
#define _GP_PORT_H_

#ifdef OS2
#include <gphoto2_port-portability-os2.h>
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

#include "gphoto2-port-result.h"

#ifndef TRUE
#define TRUE (0==0)
#endif

#ifndef FALSE
#define FALSE (1==0)
#endif

#define GP_PORT_MAX_BUF_LEN 4096             /* max length of receive buffer */

/* Specify the types of devices */
typedef enum {
    GP_PORT_NONE        =      0,
    GP_PORT_SERIAL      = 1 << 0,
    GP_PORT_PARALLEL    = 1 << 1,             /* <- Not supported yet */
    GP_PORT_USB         = 1 << 2,
    GP_PORT_IEEE1394    = 1 << 3,             /* <- Not supported yet */
    GP_PORT_NETWORK     = 1 << 4,             /* <- Not supported yet */
} gp_port_type;

typedef gp_port_type GPPortType;


/* Device struct
   -------------------------------------------------------------- */

/* Put the settings together in a union */
typedef union {
        gp_port_serial_settings         serial;
        gp_port_parallel_settings       parallel;
        gp_port_network_settings        network;
        gp_port_usb_settings            usb;
        gp_port_ieee1394_settings       ieee1394;
} GPPortSettings;

/* Don't use - DEPRECATED */
typedef GPPortSettings gp_port_settings;

enum {
        GP_PORT_USB_ENDPOINT_IN,
        GP_PORT_USB_ENDPOINT_OUT
};

typedef struct _GPPort           GPPort;
typedef struct _GPPortOperations GPPortOperations;
typedef struct _GPPortPrivateLibrary GPPortPrivateLibrary;
typedef struct _GPPortPrivateCore    GPPortPrivateCore;

struct _GPPortOperations {
        int (*init)     (GPPort *);
        int (*exit)     (GPPort *);
        int (*open)     (GPPort *);
        int (*close)    (GPPort *);
        int (*read)     (GPPort *, char *, int);
        int (*write)    (GPPort *, char *, int);
        int (*update)   (GPPort *);

        /* Pointers to devices. Please note these are stubbed so there is
         no need to #ifdef GP_PORT_* anymore. */

        /* for serial devices */
        int (*get_pin)   (GPPort *, int, int*);
        int (*set_pin)   (GPPort *, int, int);
        int (*send_break)(GPPort *, int);
        int (*flush)     (GPPort *, int);

        /* for USB devices */
        int (*find_device)(GPPort * dev, int idvendor, int idproduct);
        int (*clear_halt) (GPPort * dev, int ep);
        int (*msg_write)  (GPPort * dev, int request, int value, int index,
				char *bytes, int size);
        int (*msg_read)   (GPPort * dev, int request, int value, int index,
				char *bytes, int size);

};

struct _GPPort {

	GPPortType type;

        GPPortOperations *ops;

        GPPortSettings settings;
        GPPortSettings settings_pending;

        int device_fd;
#ifdef WIN32
        HANDLE device_handle;
#else
        void *device_handle;
#endif
	void *device;
        int timeout; /* in milli seconds */

	GPPortPrivateLibrary *pl;
	GPPortPrivateCore    *pc;
};

/* DEPRECATED */
typedef GPPort gp_port;
typedef GPPortOperations gp_port_operations;

/* Core functions
   -------------------------------------------------------------- */

        int gp_port_new         (GPPort **dev, gp_port_type type);
                /* Create a new device of type "type"
                        return values:
                                  successful: valid gp_port struct
                                unsuccessful: < 0
                */

        int gp_port_free        (GPPort *dev);
                /* Frees an IO device from memory
                        return values:
                                  successful: GP_OK
                                unsuccessful: < 0
                */

        int gp_port_open        (GPPort *dev);
                /* Open the device for reading and writing
                        return values:
                                  successful: GP_OK
                                unsuccessful: GP_ERROR
                */

        int gp_port_close       (GPPort *dev);
                /* Close the device to prevent reading and writing
                        return values:
                                  successful: GP_OK
                                unsuccessful: GP_ERROR
                */

       int gp_port_timeout_set  (GPPort *dev, int millisec_timeout);
                /* Sets the read/write timeout
                                  successful: GP_OK
                                unsuccessful: GP_ERROR
                */

       int gp_port_timeout_get  (GPPort *dev, int *millisec_timeout);
                /* Sets the read/write timeout
                                  successful: GP_OK
                                unsuccessful: GP_ERROR
                */

       int gp_port_settings_set (GPPort *dev,
                                 gp_port_settings settings);
                /* Sets the settings
                                  successful: GP_OK
                                unsuccessful: GP_ERROR
                */


       int gp_port_settings_get (GPPort *dev,
                                 gp_port_settings *settings);
                /* Returns settings in "settings"
                                  successful: GP_OK
                                unsuccessful: GP_ERROR
                */

       int gp_port_write                (GPPort *dev, char *bytes, int size);
                /* Writes "bytes" of size "size" to the device
                        return values:
                                  successful: GP_OK
                                unsuccessful: GP_ERROR
                */

       int gp_port_read         (GPPort *dev, char *bytes, int size);
                /* Reads "size" bytes in to "bytes" from the device
                        return values:
                                  successful: number of bytes read
                                unsuccessful: GP_ERROR
                */


/* Serial and Parallel specific functions
   -------------------------------------------------------------- */

        int gp_port_pin_get   (GPPort *dev, int pin, int *level);
                /* Give the status of pin from dev
                        pin values:
                                 see PIN_ constants in the various .h files
                        return values:
                                  successful: status
                                unsuccessful: GP_ERROR
                */

        int gp_port_pin_set   (GPPort *dev, int pin, int level);
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

        int gp_port_send_break (GPPort *dev, int duration);
                /* send a break (duration is in milli seconds) */

        int gp_port_flush (GPPort *dev, int direction);
                /* Flush either an input or output line */
                /* Input line is 0, Output is 1         */
                /* (think STDIO/STDOUT file descriptors */

/* USB specific functions
   -------------------------------------------------------------- */

        /* must port libusb to other platforms for this to drop-in */
        int gp_port_usb_find_device (GPPort * dev, int idvendor, int idproduct);
        int gp_port_usb_clear_halt  (GPPort * dev, int ep);
        int gp_port_usb_msg_write   (GPPort * dev, int request, int value,
		int index, char *bytes, int size);
        int gp_port_usb_msg_read    (GPPort * dev, int request, int value,
		int index, char *bytes, int size);




#endif /* _GP_PORT_H_ */


