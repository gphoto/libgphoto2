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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gphoto2-port-info-list.h>

/* For portability */
#include <gphoto2-port-portability.h>
#ifdef OS2
#include <gphoto2-port-portability-os2.h>
#include <os2.h>
#endif

#ifndef TRUE
#define TRUE (0==0)
#endif

#ifndef FALSE
#define FALSE (1==0)
#endif

#define GP_PORT_MAX_BUF_LEN 4096             /* max length of receive buffer */

struct _GPPortSettingsSerial {
	char port[128];
	int speed;
	int bits, parity, stopbits;
};
typedef struct _GPPortSettingsSerial GPPortSettingsSerial;

struct _GPPortSettingsUSB {
	int inep, outep;
	int config;
	int interface;
	int altsetting;
};
typedef struct _GPPortSettingsUSB GPPortSettingsUSB;

union _GPPortSettings {
	GPPortSettingsSerial serial;
	GPPortSettingsUSB usb;
};
typedef union _GPPortSettings GPPortSettings;

enum {
        GP_PORT_USB_ENDPOINT_IN,
        GP_PORT_USB_ENDPOINT_OUT
};

typedef struct _GPPortPrivateLibrary GPPortPrivateLibrary;
typedef struct _GPPortPrivateCore    GPPortPrivateCore;

struct _GPPort {

	/* For your convenience */
	GPPortType type;

        GPPortSettings settings;
        GPPortSettings settings_pending;

        int timeout; /* in milliseconds */

	GPPortPrivateLibrary *pl;
	GPPortPrivateCore    *pc;
};
typedef struct _GPPort           GPPort;

int gp_port_new         (GPPort **port);
int gp_port_free        (GPPort *port);

int gp_port_set_info    (GPPort *port, GPPortInfo  info);
int gp_port_get_info    (GPPort *port, GPPortInfo *info);

int gp_port_open        (GPPort *port);
int gp_port_close       (GPPort *port);

int gp_port_write       (GPPort *port, char *data, int size);
int gp_port_read        (GPPort *port, char *data, int size);

int gp_port_get_timeout  (GPPort *port, int *timeout);
int gp_port_set_timeout  (GPPort *port, int  timeout);

int gp_port_set_settings (GPPort *port, GPPortSettings  settings);
int gp_port_get_settings (GPPort *port, GPPortSettings *settings);

enum _GPPin {
	GP_PIN_RTS,
	GP_PIN_DTR,
	GP_PIN_CTS,
	GP_PIN_DSR,
	GP_PIN_CD,
	GP_PIN_RING
};
typedef enum _GPPin GPPin;

enum _GPLevel {
	GP_LEVEL_LOW  = 0,
	GP_LEVEL_HIGH = 1
};
typedef enum _GPLevel GPLevel;

int gp_port_get_pin   (GPPort *port, GPPin pin, GPLevel *level);
int gp_port_set_pin   (GPPort *port, GPPin pin, GPLevel level);

int gp_port_send_break (GPPort *port, int duration);
int gp_port_flush      (GPPort *port, int direction);

int gp_port_usb_find_device (GPPort *port, int idvendor, int idproduct);
int gp_port_usb_find_device_by_class (GPPort *port, int mainclass, int subclass, int protocol);
int gp_port_usb_clear_halt  (GPPort *port, int ep);
int gp_port_usb_msg_write   (GPPort *port, int request, int value,
			     int index, char *bytes, int size);
int gp_port_usb_msg_read    (GPPort *port, int request, int value,
			     int index, char *bytes, int size);

/* Error reporting */
int         gp_port_set_error (GPPort *port, const char *format, ...);
const char *gp_port_get_error (GPPort *port);

/* DEPRECATED */
typedef GPPort gp_port;
typedef GPPortSettings gp_port_settings;
#define PIN_CTS GP_PIN_CTS

#endif /* __GPHOTO2_PORT_H__ */


