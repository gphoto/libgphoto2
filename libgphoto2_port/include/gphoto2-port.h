/* gphoto2-port.h
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GPHOTO2_PORT_H__
#define __GPHOTO2_PORT_H__

#ifdef OS2
#include <gphoto2_port-portability-os2.h>
#include <os2.h>
#endif

/* Include the portability layer */
#include "gphoto2-portability.h"

/* Include the various headers for other devices */
#include "gphoto2-port-serial.h"
#include "gphoto2-port-usb.h"

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
    GP_PORT_USB         = 1 << 2,
} GPPortType;

/* Put the settings together in a union */
typedef union {
        gp_port_serial_settings         serial;
        gp_port_usb_settings            usb;
} GPPortSettings;

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

        int timeout; /* in milli seconds */

	GPPortPrivateLibrary *pl;
	GPPortPrivateCore    *pc;
};

/* DEPRECATED */
typedef GPPort gp_port;
typedef GPPortSettings gp_port_settings;
#include "gphoto2-port-info-list.h"

int gp_port_new         (GPPort **dev, GPPortType type);
int gp_port_free        (GPPort *dev);

int gp_port_open        (GPPort *dev);
int gp_port_close       (GPPort *dev);

int gp_port_timeout_set  (GPPort *dev, int millisec_timeout);
int gp_port_timeout_get  (GPPort *dev, int *millisec_timeout);

int gp_port_settings_set (GPPort *dev, GPPortSettings  settings);
int gp_port_settings_get (GPPort *dev, GPPortSettings *settings);

int gp_port_write                (GPPort *dev, char *bytes, int size);
int gp_port_read         (GPPort *dev, char *bytes, int size);

int gp_port_pin_get   (GPPort *dev, int pin, int *level);
int gp_port_pin_set   (GPPort *dev, int pin, int level);

int gp_port_send_break (GPPort *dev, int duration);
int gp_port_flush      (GPPort *dev, int direction);

int gp_port_usb_find_device (GPPort * dev, int idvendor, int idproduct);
int gp_port_usb_clear_halt  (GPPort * dev, int ep);
int gp_port_usb_msg_write   (GPPort * dev, int request, int value,
			     int index, char *bytes, int size);
int gp_port_usb_msg_read    (GPPort * dev, int request, int value,
			     int index, char *bytes, int size);

/* Error reporting */
int         gp_port_set_error (GPPort *port, const char *format, ...);
const char *gp_port_get_error (GPPort *port);

#endif /* __GPHOTO2_PORT_H__ */


