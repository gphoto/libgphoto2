/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-usb.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 * Copyright 1999-2000 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright 2011,2015 Marcus Meissner <marcus@jet.franken.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"
#include <gphoto2/gphoto2-port-library.h>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <libusb.h>

#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-log.h>

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

#define C_GP(RESULT) do {\
	int _r=(RESULT);\
	if (_r<GP_OK) {\
		GP_LOG_E ("'%s' failed: %s (%d)", #RESULT, gp_port_result_as_string(_r), _r);\
		return _r;\
	}\
} while(0)

static const char *my_libusb_strerror(int r)
{
	switch (r) {
	case LIBUSB_SUCCESS:			return "Success";
	case LIBUSB_ERROR_IO:			return "Input/Output error";
	case LIBUSB_ERROR_INVALID_PARAM:	return "Invalid parameter";
	case LIBUSB_ERROR_ACCESS:		return "Access denied (insufficient permissions)";
	case LIBUSB_ERROR_NO_DEVICE:		return "No such device (it may have been disconnected)";
	case LIBUSB_ERROR_NOT_FOUND:		return "Entity not found";
	case LIBUSB_ERROR_BUSY:			return "Resource busy";
	case LIBUSB_ERROR_TIMEOUT:		return "Operation timed out";
	case LIBUSB_ERROR_OVERFLOW:		return "Overflow";
	case LIBUSB_ERROR_PIPE:			return "Pipe error";
	case LIBUSB_ERROR_INTERRUPTED:		return "System call interrupted (perhaps due to signal)";
	case LIBUSB_ERROR_NO_MEM:		return "Insufficient memory";
	case LIBUSB_ERROR_NOT_SUPPORTED:	return "Operation not supported or unimplemented on this platform";
	case LIBUSB_ERROR_OTHER:		return "Other error";
	default:				return "Unknown error";
	}
}

static int log_on_libusb_error_helper( int _r, const char* _func, const char* file, int line, const char* func ) {
	if (_r < LIBUSB_SUCCESS)
		gp_log_with_source_location(GP_LOG_ERROR, file, line, func,
					    "'%s' failed: %s (%d)", _func, my_libusb_strerror(_r), _r);
	return _r;
}
#define LOG_ON_LIBUSB_E( RESULT ) log_on_libusb_error_helper( (RESULT), #RESULT, __FILE__, __LINE__, __func__ )

#define C_LIBUSB(RESULT, DEFAULT_ERROR) do {\
	int _r = LOG_ON_LIBUSB_E( RESULT );\
	if (_r < LIBUSB_SUCCESS)\
		return translate_libusb_error(_r, DEFAULT_ERROR);\
} while (0)


static int translate_libusb_error( int error, int default_return )
{
	switch (error) {
	case LIBUSB_SUCCESS:			return GP_OK;
	case LIBUSB_ERROR_INVALID_PARAM:	return GP_ERROR_BAD_PARAMETERS;
	case LIBUSB_ERROR_NO_DEVICE:		return GP_ERROR_IO_USB_FIND;
	case LIBUSB_ERROR_TIMEOUT:		return GP_ERROR_TIMEOUT;
	case LIBUSB_ERROR_NO_MEM:		return GP_ERROR_NO_MEMORY;
	case LIBUSB_ERROR_NOT_SUPPORTED:	return GP_ERROR_NOT_SUPPORTED;

	case LIBUSB_ERROR_IO:
	case LIBUSB_ERROR_BUSY:
	case LIBUSB_ERROR_NOT_FOUND:
	case LIBUSB_ERROR_ACCESS:
	case LIBUSB_ERROR_OVERFLOW:
	case LIBUSB_ERROR_PIPE:
	case LIBUSB_ERROR_INTERRUPTED:
	default:
		return default_return; /* usually GP_ERROR_IO, but might be GP_ERROR_IO_READ/WRITE */
	}
}

struct _GPPortPrivateLibrary {
	libusb_context *ctx;
	libusb_device *d;
	libusb_device_handle *dh;

	int config;
	int interface;
	int altsetting;

	int detached;

	time_t				devslastchecked;
	int				nrofdevs;
	struct libusb_device_descriptor	*descs;
	libusb_device			**devs;

#define INTERRUPT_TRANSFERS 10
	struct libusb_transfer		*transfers[INTERRUPT_TRANSFERS];
	int 				nrofirqs;
	unsigned char			**irqs;
	int 				*irqlens;
};

GPPortType
gp_port_library_type (void)
{
	return (GP_PORT_USB);
}


static ssize_t
load_devicelist (GPPortPrivateLibrary *pl) {
	time_t	xtime;

	time(&xtime);
	if (xtime != pl->devslastchecked) {
		if (pl->nrofdevs)
			libusb_free_device_list (pl->devs, 1);
		free (pl->descs);
		pl->nrofdevs = 0;
		pl->devs = NULL;
		pl->descs = NULL;
	}
	if (!pl->nrofdevs) {
		int 	i;

		pl->nrofdevs = libusb_get_device_list (pl->ctx, &pl->devs);
		C_MEM (pl->descs = calloc (pl->nrofdevs, sizeof(pl->descs[0])));
		for (i=0;i<pl->nrofdevs;i++)
			LOG_ON_LIBUSB_E (libusb_get_device_descriptor(pl->devs[i], &pl->descs[i]));
	}
	time (&pl->devslastchecked);
	return pl->nrofdevs;
}

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo	info;
	int		nrofdevices = 0;
	int		d, i, i1, i2, unknownint;
	libusb_context	*ctx;
	libusb_device	**devs = NULL;
	int		nrofdevs = 0;
	struct libusb_device_descriptor	*descs;

	C_LIBUSB (libusb_init (&ctx), GP_ERROR_IO);

	/* TODO: make sure libusb_exit gets called in all error paths inside this function */

	/* generic matcher. This will catch passed XXX,YYY entries for instance. */
	C_GP (gp_port_info_new (&info));
	gp_port_info_set_type (info, GP_PORT_USB);
	gp_port_info_set_name (info, "");
	gp_port_info_set_path (info, "^usb:");
	gp_port_info_list_append (list, info); /* do not check return value, it might be -1 */

	nrofdevs = libusb_get_device_list (ctx, &devs);
	C_MEM (descs = calloc (nrofdevs, sizeof(descs[0])));
	for (i=0;i<nrofdevs;i++)
		LOG_ON_LIBUSB_E (libusb_get_device_descriptor(devs[i], &descs[i]));

	for (d = 0; d < nrofdevs; d++) {
		/* Devices which are definitely not cameras. */
		if (	(descs[d].bDeviceClass == LIBUSB_CLASS_HUB)		||
			(descs[d].bDeviceClass == LIBUSB_CLASS_HID)		||
			(descs[d].bDeviceClass == LIBUSB_CLASS_PRINTER)	||
			(descs[d].bDeviceClass == LIBUSB_CLASS_COMM)	||
			(descs[d].bDeviceClass == 0xe0)	/* wireless / bluetooth */
		)
			continue;
		/* excepts HUBs, usually the interfaces have the classes, not
		 * the device */
		unknownint = 0;
		for (i = 0; i < descs[d].bNumConfigurations; i++) {
			struct libusb_config_descriptor *config;

			if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (devs[d], i, &config))) {
				unknownint++;
				continue;
			}
			for (i1 = 0; i1 < config->bNumInterfaces; i1++)
				for (i2 = 0; i2 < config->interface[i1].num_altsetting; i2++) {
					const struct libusb_interface_descriptor *intf = &config->interface[i1].altsetting[i2]; 
					if (	(intf->bInterfaceClass == LIBUSB_CLASS_HID)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_PRINTER)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_COMM)	||
						(intf->bInterfaceClass == 0xe0)	/* wireless/bluetooth*/
					)
						continue;
					unknownint++;
				}
			libusb_free_config_descriptor (config);
		}
		/* when we find only hids, printer or comm ifaces  ... skip this */
		if (!unknownint)
			continue;
		/* Note: We do not skip USB storage. Some devices can support both,
		 * and the Ricoh erronously reports it.
		 */ 
		nrofdevices++;
	}

#if 0
	/* If we already added usb:, and have 0 or 1 devices we have nothing to do.
	 * This should be the standard use case.
	 */
	/* We never want to return just "usb:" ... also return "usb:XXX,YYY", and
	 * let upper layers filter out the usb: */
	if (nrofdevices <= 1) 
		return (GP_OK);
#endif

	/* Redo the same bus/device walk, but now add the ports with usb:x,y notation,
	 * so we can address all USB devices.
	 */
	for (d = 0; d < nrofdevs; d++) {
		char path[200];

		/* Devices which are definitely not cameras. */
		if (	(descs[d].bDeviceClass == LIBUSB_CLASS_HUB)		||
			(descs[d].bDeviceClass == LIBUSB_CLASS_HID)		||
			(descs[d].bDeviceClass == LIBUSB_CLASS_PRINTER)	||
			(descs[d].bDeviceClass == LIBUSB_CLASS_COMM)
		)
			continue;
		/* excepts HUBs, usually the interfaces have the classes, not
		 * the device */
		unknownint = 0;
		for (i = 0; i < descs[d].bNumConfigurations; i++) {
			struct libusb_config_descriptor *config;

			if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (devs[d], i, &config))) {
				unknownint++;
				continue;
			}
			for (i1 = 0; i1 < config->bNumInterfaces; i1++)
				for (i2 = 0; i2 < config->interface[i1].num_altsetting; i2++) {
					const struct libusb_interface_descriptor *intf = &config->interface[i1].altsetting[i2]; 
					if (	(intf->bInterfaceClass == LIBUSB_CLASS_HID)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_PRINTER)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_COMM))
						continue;
					unknownint++;
				}
			libusb_free_config_descriptor (config);
		}
		/* when we find only hids, printer or comm ifaces  ... skip this */
		if (!unknownint)
			continue;
		/* Note: We do not skip USB storage. Some devices can support both,
		 * and the Ricoh erronously reports it.
		 */ 
		C_GP (gp_port_info_new (&info));
		gp_port_info_set_type (info, GP_PORT_USB);
		gp_port_info_set_name (info, "Universal Serial Bus");
		snprintf (path,sizeof(path), "usb:%03d,%03d",
			libusb_get_bus_number (devs[d]),
			libusb_get_device_address (devs[d])
		);
		gp_port_info_set_path (info, path);
		C_GP (gp_port_info_list_append (list, info));
	}

	libusb_free_device_list (devs, 1);
	libusb_exit (ctx); /* should free all stuff above */
	free (descs);

	/* This will only be added if no other device was ever added.
	 * Users doing "usb:" usage will enter the regular expression matcher case. */
	if (nrofdevices == 0) {
		C_GP (gp_port_info_new (&info));
		gp_port_info_set_type (info, GP_PORT_USB);
		gp_port_info_set_name (info, "Universal Serial Bus");
		gp_port_info_set_path (info, "usb:");
		C_GP (gp_port_info_list_append (list, info));
	}
	return (GP_OK);
}

static int gp_libusb1_init (GPPort *port)
{
	C_MEM (port->pl = malloc (sizeof (GPPortPrivateLibrary)));
	memset (port->pl, 0, sizeof (GPPortPrivateLibrary));

	port->pl->config = port->pl->interface = port->pl->altsetting = -1;

	if (LOG_ON_LIBUSB_E (libusb_init (&port->pl->ctx))) {
		free (port->pl);
		port->pl = NULL;
		return GP_ERROR_IO;
	}
#if 0
	libusb_set_debug (port->pl->ctx, 255);
#endif
	return GP_OK;
}

static int
gp_libusb1_exit (GPPort *port)
{
	if (port->pl) {
		free (port->pl->descs);
		if (port->pl->nrofdevs)
			libusb_free_device_list (port->pl->devs, 1);
		libusb_exit (port->pl->ctx);
		free (port->pl);
		port->pl = NULL;
	}
	return GP_OK;
}

static int gp_libusb1_find_path_lib(GPPort *port);
static int gp_libusb1_queue_interrupt_urbs (GPPort *port);
static int
gp_libusb1_open (GPPort *port)
{
	int ret;

	GP_LOG_D ("()");
	C_PARAMS (port);

	if (!port->pl->d) {
		gp_libusb1_find_path_lib(port);
		C_PARAMS (port->pl->d);
	}

	C_LIBUSB (libusb_open (port->pl->d, &port->pl->dh), GP_ERROR_IO);
	if (!port->pl->dh) {
		int saved_errno = errno;
		gp_port_set_error (port, _("Could not open USB device (%s)."),
				   strerror(saved_errno));
		return GP_ERROR_IO;
	}
	ret = libusb_kernel_driver_active (port->pl->dh, port->settings.usb.interface);

#if 0
	if (strstr(name,"usbfs") || strstr(name,"storage")) {
		/* other gphoto instance most likely */
		gp_port_set_error (port, _("Camera is already in use."));
		return GP_ERROR_IO_LOCK;
	}
#endif

	switch (ret) {
	case 1: GP_LOG_D ("Device has a kernel driver attached (%d), detaching it now.", ret);
		ret = libusb_detach_kernel_driver (port->pl->dh, port->settings.usb.interface);
		if (ret < 0)
			gp_port_set_error (port, _("Could not detach kernel driver of camera device."));
		else
			port->pl->detached = 1;
	case 0:	/* not detached */
		break;
	default:
		gp_port_set_error (port, _("Could not query kernel driver of device."));
		break;
	}

	GP_LOG_D ("claiming interface %d", port->settings.usb.interface);
	if (LOG_ON_LIBUSB_E (libusb_claim_interface (port->pl->dh, port->settings.usb.interface))) {
		int saved_errno = errno;
		gp_port_set_error (port, _("Could not claim interface %d (%s). "
					   "Make sure no other program (%s) "
					   "or kernel module (such as %s) "
					   "is using the device and you have "
					   "read/write access to the device."),
				   port->settings.usb.interface,
				   strerror(saved_errno),
#ifdef __linux__
				   "gvfs-gphoto2-volume-monitor",
#else
#if defined(__APPLE__)
				   _("MacOS PTPCamera service"),
#else
				   _("unknown libgphoto2 using program"),
#endif
#endif
				   "sdc2xx, stv680, spca50x");
		return GP_ERROR_IO_USB_CLAIM;
	}

	gp_libusb1_queue_interrupt_urbs (port);

	return GP_OK;
}

static int
_close_async_interrupts(GPPort *port)
{
	int i, haveone;
	struct timeval tv;

	C_PARAMS (port);

	if (port->pl->dh == NULL)
		return GP_OK;

	/* Catch up on pending ones */
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	LOG_ON_LIBUSB_E (libusb_handle_events_timeout(port->pl->ctx, &tv));
	/* Now cancel and free the async transfers */
	for (i = 0; i < sizeof(port->pl->transfers)/sizeof(port->pl->transfers[0]); i++) {
		if (port->pl->transfers[i]) {
			GP_LOG_D("canceling transfer %d:%p (status %d)",i, port->pl->transfers[i], port->pl->transfers[i]->status);
			/* this happens if the transfer is completed for instance, but not reaped. we cannot cancel it. */
			if (LOG_ON_LIBUSB_E(libusb_cancel_transfer(port->pl->transfers[i])) < 0) {
				/* do not libusb_free_transfer (port->pl->transfers[i]); causes crashes */
				port->pl->transfers[i] = NULL;
			}
		}
	}
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	LOG_ON_LIBUSB_E (libusb_handle_events_timeout(port->pl->ctx, &tv));
	/* Do just one round ... this should be sufficient and avoids endless loops. */
	haveone = 0;
	for (i = 0; i < sizeof(port->pl->transfers)/sizeof(port->pl->transfers[0]); i++) {
		if (port->pl->transfers[i]) {
			GP_LOG_D("checking: transfer %d:%p status %d",i, port->pl->transfers[i], port->pl->transfers[i]->status);
			haveone = 1;
		}
	}
	if (haveone)
		LOG_ON_LIBUSB_E (libusb_handle_events(port->pl->ctx));
	return GP_OK;
}

static int
gp_libusb1_close (GPPort *port)
{
	C_PARAMS (port);

	if (port->pl->dh == NULL)
		return GP_OK;

	_close_async_interrupts(port);

	if (libusb_release_interface (port->pl->dh,
				   port->settings.usb.interface) < 0) {
		int saved_errno = errno;
		gp_port_set_error (port, _("Could not release interface %d (%s)."),
				   port->settings.usb.interface,
				   strerror(saved_errno));
		return GP_ERROR_IO;
	}

#if 0
	/* This confuses the EOS 5d camera and possible other EOSs. *sigh* */
	/* This is only for our very special Canon cameras which need a good
	 * whack after close, otherwise they get timeouts on reconnect.
	 */
	if (port->pl->d->descriptor.idVendor == 0x04a9) {
		if (usb_reset (port->pl->dh) < 0) {
			int saved_errno = errno;
			gp_port_set_error (port, _("Could not reset "
						   "USB port (%s)."),
					   strerror(saved_errno));
			return (GP_ERROR_IO);
		}
	}
#endif
	if (port->pl->detached) {
		if (LOG_ON_LIBUSB_E (libusb_attach_kernel_driver (port->pl->dh, port->settings.usb.interface)))
			gp_port_set_error (port, _("Could not reattach kernel driver of camera device."));
	}

	libusb_close (port->pl->dh);

	free (port->pl->irqs);
	port->pl->irqs = NULL;
	free (port->pl->irqlens);
	port->pl->irqlens = NULL;
	port->pl->nrofirqs = 0;
	port->pl->dh = NULL;
	return GP_OK;
}

static int
gp_libusb1_clear_halt_lib(GPPort *port, int ep)
{
	unsigned char internal_ep;

	C_PARAMS (port && port->pl->dh);

	switch (ep) {
	case GP_PORT_USB_ENDPOINT_IN :
		internal_ep = port->settings.usb.inep;
		break;
	case GP_PORT_USB_ENDPOINT_OUT :
		internal_ep = port->settings.usb.outep;
		break;
	case GP_PORT_USB_ENDPOINT_INT :
		internal_ep = port->settings.usb.intep;
		break;
	default:
		gp_port_set_error (port, "bad EndPoint argument 0x%x", ep);
		return GP_ERROR_BAD_PARAMETERS;
	}

	C_LIBUSB (libusb_clear_halt(port->pl->dh, internal_ep), GP_ERROR_IO_USB_CLEAR_HALT);

	return GP_OK;
}

static int
gp_libusb1_write (GPPort *port, const char *bytes, int size)
{
        int curwritten;

	C_PARAMS (port && port->pl->dh);

	C_LIBUSB (libusb_bulk_transfer (port->pl->dh, port->settings.usb.outep,
                           (unsigned char*)bytes, size, &curwritten, port->timeout),
                  GP_ERROR_IO_WRITE);

        return curwritten;
}

static int
gp_libusb1_read(GPPort *port, char *bytes, int size)
{
	int curread;

	C_PARAMS (port && port->pl->dh);

	C_LIBUSB (libusb_bulk_transfer (port->pl->dh, port->settings.usb.inep,
			(unsigned char*)bytes, size, &curread, port->timeout),
		  GP_ERROR_IO_READ );

        return curread;
}

static int
gp_libusb1_reset(GPPort *port)
{
	C_PARAMS (port && port->pl->dh);

	/* earlier libusb 1 versions get crashes otherwise */
	_close_async_interrupts(port);

	C_LIBUSB (libusb_reset_device (port->pl->dh), GP_ERROR_IO);

	return GP_OK;
}

static void LIBUSB_CALL
_cb_irq(struct libusb_transfer *transfer)
{
	struct _GPPortPrivateLibrary *pl = transfer->user_data;
	int i;

	GP_LOG_D("%p with status %d", transfer, transfer->status);

	if (	(transfer->status == LIBUSB_TRANSFER_CANCELLED) ||
		(transfer->status == LIBUSB_TRANSFER_TIMED_OUT) || /* on close */
		(transfer->status == LIBUSB_TRANSFER_NO_DEVICE) /* on removing camera */
	) {
		/* Only requeue the global transfers, not temporary ones */
		for (i = 0; i < sizeof(pl->transfers)/sizeof(pl->transfers[0]); i++) {
			if (pl->transfers[i] == transfer) {
				libusb_free_transfer (transfer);
				pl->transfers[i] = NULL;
				return;
			}
		}
		return;
	}

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		GP_LOG_D("transfer %p should be in LIBUSB_TRANSFER_COMPLETED, but is %d!", transfer, transfer->status);
		return;
	}
	if (transfer->actual_length) {
		GP_LOG_DATA ((char*)transfer->buffer, transfer->actual_length, "interrupt");

		pl->irqs 	= realloc(pl->irqs, sizeof(pl->irqs[0])*(pl->nrofirqs+1));
		pl->irqlens	= realloc(pl->irqlens, sizeof(pl->irqlens[0])*(pl->nrofirqs+1));

		pl->irqlens[pl->nrofirqs] = transfer->actual_length;
		pl->irqs[pl->nrofirqs] = malloc(transfer->actual_length);
		memcpy(pl->irqs[pl->nrofirqs],transfer->buffer,transfer->actual_length);
		pl->nrofirqs++;
	}

	GP_LOG_D("requeuing complete transfer %p", transfer);
	LOG_ON_LIBUSB_E(libusb_submit_transfer(transfer));
	return;
}

static int
gp_libusb1_queue_interrupt_urbs (GPPort *port)
{
	unsigned int i;

	for (i = 0; i < sizeof(port->pl->transfers)/sizeof(port->pl->transfers[0]); i++) {
		unsigned char *buf;
		port->pl->transfers[i] = libusb_alloc_transfer(0);

		buf = malloc(256); /* FIXME: safe size? */
		libusb_fill_interrupt_transfer(port->pl->transfers[i], port->pl->dh, port->settings.usb.intep,
			buf, 256, _cb_irq, port->pl, 0
		);
		port->pl->transfers[i]->flags |= LIBUSB_TRANSFER_FREE_BUFFER;
		LOG_ON_LIBUSB_E(libusb_submit_transfer (port->pl->transfers[i]));
		
	}
	return GP_OK;
}

static int
gp_libusb1_check_int (GPPort *port, char *bytes, int size, int timeout)
{
	int 		ret;
	struct timeval	tv;

	C_PARAMS (port && port->pl->dh && timeout >= 0);

	if (port->pl->nrofirqs)
		goto handleirq;

	tv.tv_sec = timeout/1000;
	tv.tv_usec = (timeout%1000)*1000;

	ret = LOG_ON_LIBUSB_E (libusb_handle_events_timeout(port->pl->ctx, &tv));

	if (port->pl->nrofirqs)
		goto handleirq;

	if (ret < LIBUSB_SUCCESS)
		return translate_libusb_error(ret, GP_ERROR_IO_READ);

	return GP_ERROR_TIMEOUT;

handleirq:
	if (size > port->pl->irqlens[0])
		size = port->pl->irqlens[0];
	memcpy(bytes, port->pl->irqs[0], size);

	memmove(port->pl->irqs,port->pl->irqs+1,sizeof(port->pl->irqs[0])*(port->pl->nrofirqs-1));
	memmove(port->pl->irqlens,port->pl->irqlens+1,sizeof(port->pl->irqlens[0])*(port->pl->nrofirqs-1));
	port->pl->nrofirqs--;
        return size;
}

static int
gp_libusb1_msg(GPPort *port, int request, int value, int index, char *bytes, int size, int flags, int default_error)
{
	int handled = 0;
	C_PARAMS (port && port->pl->dh);

	C_LIBUSB (handled = libusb_control_transfer (port->pl->dh, flags, request, value, index,
			(unsigned char*)bytes, size, port->timeout),
		  default_error);

	return handled;
}

static int
gp_libusb1_msg_write_lib(GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
	return gp_libusb1_msg (port, request, value, index, bytes, size,
					LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
					GP_ERROR_IO_WRITE);
}

static int
gp_libusb1_msg_read_lib(GPPort *port, int request, int value, int index,
	char *bytes, int size)
{
	return gp_libusb1_msg (port, request, value, index, bytes, size,
					LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
					GP_ERROR_IO_READ);
}

/* The next two functions support the nonstandard request types 0x41 (write) 
 * and 0xc1 (read), which are occasionally needed. 
 */

static int
gp_libusb1_msg_interface_write_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	return gp_libusb1_msg (port, request, value, index, bytes, size,
					LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
					GP_ERROR_IO_WRITE);
}


static int
gp_libusb1_msg_interface_read_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	return gp_libusb1_msg (port, request, value, index, bytes, size,
					LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
					GP_ERROR_IO_READ);
}


/* The next two functions support the nonstandard request types 0x21 (write) 
 * and 0xa1 (read), which are occasionally needed. 
 */

static int
gp_libusb1_msg_class_write_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	return gp_libusb1_msg (port, request, value, index, bytes, size,
					LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
					GP_ERROR_IO_WRITE);
}


static int
gp_libusb1_msg_class_read_lib(GPPort *port, int request, 
	int value, int index, char *bytes, int size)
{
	return gp_libusb1_msg (port, request, value, index, bytes, size,
					LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
					GP_ERROR_IO_READ);
}

/*
 * This function applys changes to the device.
 *
 * New settings are in port->settings_pending and the old ones
 * are in port->settings. Compare them first and only call
 * usb_set_configuration() and usb_set_altinterface() if needed
 * since some USB devices does not like it if this is done
 * more than necessary (Canon Digital IXUS 300 for one).
 *
 */
static int
gp_libusb1_update (GPPort *port)
{
	int ifacereleased = FALSE, changedone = FALSE;

	C_PARAMS (port && port->pl && port->pl->ctx);

	GP_LOG_D ("(old int=%d, conf=%d, alt=%d) port %s, (new int=%d, conf=%d, alt=%d) port %s",
		port->settings.usb.interface,
		port->settings.usb.config,
		port->settings.usb.altsetting,
		port->settings.usb.port,
		port->settings_pending.usb.interface,
		port->settings_pending.usb.config,
		port->settings_pending.usb.altsetting,
		port->settings_pending.usb.port
	);

/* do not overwrite it ... we need to set it.
	if (port->pl->interface == -1) port->pl->interface = port->settings.usb.interface;
	if (port->pl->config == -1) port->pl->config = port->settings.usb.config;
	if (port->pl->altsetting == -1) port->pl->altsetting = port->settings.usb.altsetting;
*/

	/* The portname can also be changed with the device still fully closed. */
	memcpy(&port->settings.usb.port, &port->settings_pending.usb.port,
		sizeof(port->settings.usb.port));

	if (!port->pl->dh) {
		GP_LOG_D("lowlevel libusb1 port not yet opened, no need for libusb changes");
		return GP_OK; /* the port might not be opened, yet. that is ok */
	}

	memcpy(&port->settings.usb, &port->settings_pending.usb,
		sizeof(port->settings.usb));

	/* The interface changed. release the old, claim the new ... */
	if (port->settings.usb.interface != port->pl->interface) {
		GP_LOG_D ("changing interface %d -> %d", port->pl->interface, port->settings.usb.interface);
		if (LOG_ON_LIBUSB_E (libusb_release_interface (port->pl->dh, port->pl->interface))) {
			/* Not a hard error for now. -Marcus */
		} else {
			GP_LOG_D ("claiming interface %d", port->settings.usb.interface);
			C_LIBUSB (libusb_claim_interface (port->pl->dh, port->settings.usb.interface),
				  GP_ERROR_IO_USB_CLAIM);
			port->pl->interface = port->settings.usb.interface;
		}
		changedone = TRUE;
	}
	if (port->settings.usb.config != port->pl->config) {
		GP_LOG_D ("changing config %d -> %d", port->pl->config, port->settings.usb.config);
		/* This can only be changed with the interface released. 
		 * This is a hard requirement since 2.6.12.
		 */
		if (LOG_ON_LIBUSB_E (libusb_release_interface (port->pl->dh, port->settings.usb.interface))) {
			ifacereleased = FALSE;
		} else {
			ifacereleased = TRUE;
		}
		if (LOG_ON_LIBUSB_E (libusb_set_configuration(port->pl->dh, port->settings.usb.config))) {
#if 0 /* setting the configuration failure is not fatal */
			int saved_errno = errno;
			gp_port_set_error (port,
					   _("Could not set config %d/%d (%s)"),
					   port->settings.usb.interface,
					   port->settings.usb.config,
					   strerror(saved_errno));
			return GP_ERROR_IO_UPDATE;	
#endif
			GP_LOG_E ("setting configuration from %d to %d failed, but continuing...", port->pl->config, port->settings.usb.config);
		}

		GP_LOG_D ("Changed usb.config from %d to %d", port->pl->config, port->settings.usb.config);

		if (ifacereleased) {
			GP_LOG_D ("claiming interface %d", port->settings.usb.interface);
			LOG_ON_LIBUSB_E (libusb_claim_interface (port->pl->dh, port->settings.usb.interface));
		}
		/*
		 * Copy at once if something else fails so that this
		 * does not get re-applied
		 */
		port->pl->config = port->settings.usb.config;
		changedone = TRUE;
	}

	/* This can be changed with interface claimed. (And I think it must be claimed.) */
	if (port->settings.usb.altsetting != port->pl->altsetting) {
		if (LOG_ON_LIBUSB_E (libusb_set_interface_alt_setting (port->pl->dh,
					port->settings.usb.interface, port->settings.usb.altsetting))) {
			int saved_errno = errno;
			gp_port_set_error (port,
					   _("Could not set altsetting from %d "
					     "to %d (%s)"),
					   port->pl->altsetting,
					   port->settings.usb.altsetting,
					   strerror(saved_errno));
			return GP_ERROR_IO_UPDATE;
		}

		GP_LOG_D ("Changed usb.altsetting from %d to %d", port->pl->altsetting, port->settings.usb.altsetting);
		port->pl->altsetting = port->settings.usb.altsetting;
		changedone = TRUE;
	}

	/* requeue the interrupts */
	if (changedone)
		gp_libusb1_queue_interrupt_urbs (port);
	return GP_OK;
}

static int
gp_libusb1_find_ep(libusb_device *dev, int config, int interface, int altsetting, int direction, int type)
{
	const struct libusb_interface_descriptor *intf;
	struct libusb_config_descriptor *confdesc;
	int i;

	if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (dev, config, &confdesc)))
		return -1;

	intf = &confdesc->interface[interface].altsetting[altsetting];
	for (i = 0; i < intf->bNumEndpoints; i++) {
		if (((intf->endpoint[i].bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == direction) &&
		    ((intf->endpoint[i].bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == type)) {
			unsigned char ret;
			ret = intf->endpoint[i].bEndpointAddress; /* intf is cleared after next line, copy epaddr! */
			libusb_free_config_descriptor (confdesc);
			return ret;
		}
	}
	libusb_free_config_descriptor (confdesc);
	return -1;
}

static int
gp_libusb1_find_first_altsetting(struct libusb_device *dev, int *config, int *interface, int *altsetting)
{
	int i, i1, i2;
	struct libusb_device_descriptor desc;

	if (LOG_ON_LIBUSB_E (libusb_get_device_descriptor (dev, &desc)))
		return -1;

	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct libusb_config_descriptor *confdesc;

		if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (dev, i, &confdesc)))
			return -1;

		for (i1 = 0; i1 < confdesc->bNumInterfaces; i1++)
			for (i2 = 0; i2 < confdesc->interface[i1].num_altsetting; i2++)
				if (confdesc->interface[i1].altsetting[i2].bNumEndpoints) {
					*config = i;
					*interface = i1;
					*altsetting = i2;
					libusb_free_config_descriptor (confdesc);

					return 0;
				}
		libusb_free_config_descriptor (confdesc);
	}
	return -1;
}

static int
gp_libusb1_find_path_lib(GPPort *port)
{
	char *s;
	int d, busnr = 0, devnr = 0;
	GPPortPrivateLibrary *pl;

	C_PARAMS (port);

	pl = port->pl;

	s = strchr (port->settings.usb.port,':');
	C_PARAMS (s && (s[1] != '\0'));
	C_PARAMS (sscanf (s+1, "%d,%d", &busnr, &devnr) == 2); /* usb:%d,%d */

	pl->nrofdevs = load_devicelist (port->pl);

	for (d = 0; d < pl->nrofdevs; d++) {
		struct libusb_config_descriptor *confdesc;
		int config = -1, interface = -1, altsetting = -1;

		if (busnr != libusb_get_bus_number (pl->devs[d]))
			continue;
		if (devnr != libusb_get_device_address (pl->devs[d]))
			continue;

		port->pl->d = pl->devs[d];

		GP_LOG_D ("Found path %s", port->settings.usb.port);

		/* Use the first config, interface and altsetting we find */
		gp_libusb1_find_first_altsetting(pl->devs[d], &config, &interface, &altsetting);

		if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (pl->devs[d], config, &confdesc)))
			continue;

		/* Set the defaults */
		port->settings.usb.config = confdesc->bConfigurationValue;
		port->settings.usb.interface = confdesc->interface[interface].altsetting[altsetting].bInterfaceNumber;
		port->settings.usb.altsetting = confdesc->interface[interface].altsetting[altsetting].bAlternateSetting;

		port->settings.usb.inep  = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.outep = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_OUT, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.intep = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_INTERRUPT);

		port->settings.usb.maxpacketsize = libusb_get_max_packet_size (pl->devs[d], port->settings.usb.inep);
		GP_LOG_D ("Detected defaults: config %d, interface %d, altsetting %d, "
			"inep %02x, outep %02x, intep %02x, class %02x, subclass %02x",
			port->settings.usb.config,
			port->settings.usb.interface,
			port->settings.usb.altsetting,
			port->settings.usb.inep,
			port->settings.usb.outep,
			port->settings.usb.intep,
			confdesc->interface[interface].altsetting[altsetting].bInterfaceClass,
			confdesc->interface[interface].altsetting[altsetting].bInterfaceSubClass
			);
		libusb_free_config_descriptor (confdesc);
		return GP_OK;
	}
#if 0
	gp_port_set_error (port, _("Could not find USB device "
		"(vendor 0x%x, product 0x%x). Make sure this device "
		"is connected to the computer."), idvendor, idproduct);
#endif
	return GP_ERROR_IO_USB_FIND;
}
static int
gp_libusb1_find_device_lib(GPPort *port, int idvendor, int idproduct)
{
	char *s;
	int d, busnr = 0, devnr = 0;
	GPPortPrivateLibrary *pl;

	C_PARAMS (port);

	pl = port->pl;

	s = strchr (port->settings.usb.port,':');
	if (s && (s[1] != '\0')) { /* usb:%d,%d */
		if (sscanf (s+1, "%d,%d", &busnr, &devnr) != 2) {
			devnr = 0;
			sscanf (s+1, "%d", &busnr);
		}
	}
	/*
	 * 0x0000 idvendor is not valid.
	 * 0x0000 idproduct is ok.
	 * Should the USB layer report that ? I don't know.
	 * Better to check here.
	 */
	if (!idvendor) {
		gp_port_set_error (port, _("The supplied vendor or product "
			"id (0x%x,0x%x) is not valid."), idvendor, idproduct);
		return GP_ERROR_BAD_PARAMETERS;
	}

	pl->nrofdevs = load_devicelist (port->pl);

	for (d = 0; d < pl->nrofdevs; d++) {
		struct libusb_config_descriptor *confdesc;
		int config = -1, interface = -1, altsetting = -1;

		if ((pl->descs[d].idVendor != idvendor) ||
		    (pl->descs[d].idProduct != idproduct))
			continue;

		if (busnr && (busnr != libusb_get_bus_number (pl->devs[d])))
			continue;
		if (devnr && (devnr != libusb_get_device_address (pl->devs[d])))
			continue;

		port->pl->d = pl->devs[d];

		GP_LOG_D ("Looking for USB device (vendor 0x%x, product 0x%x)... found.", idvendor, idproduct);

		/* Use the first config, interface and altsetting we find */
		gp_libusb1_find_first_altsetting(pl->devs[d], &config, &interface, &altsetting);

		if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (pl->devs[d], config, &confdesc)))
			continue;

		/* Set the defaults */
		if (confdesc->interface[interface].altsetting[altsetting].bInterfaceClass
		    == LIBUSB_CLASS_MASS_STORAGE) {
			GP_LOG_D ("USB device (vendor 0x%x, product 0x%x) is a mass"
				  " storage device, and might not function with gphoto2."
				  " Reference: %s", idvendor, idproduct, URL_USB_MASSSTORAGE);
		}
		port->settings.usb.config = confdesc->bConfigurationValue;
		port->settings.usb.interface = confdesc->interface[interface].altsetting[altsetting].bInterfaceNumber;
		port->settings.usb.altsetting = confdesc->interface[interface].altsetting[altsetting].bAlternateSetting;

		port->settings.usb.inep  = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.outep = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_OUT, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.intep = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_INTERRUPT);

		port->settings.usb.maxpacketsize = libusb_get_max_packet_size (pl->devs[d], port->settings.usb.inep);
		GP_LOG_D ("Detected defaults: config %d, interface %d, altsetting %d, "
			  "inep %02x, outep %02x, intep %02x, class %02x, subclass %02x",
			port->settings.usb.config,
			port->settings.usb.interface,
			port->settings.usb.altsetting,
			port->settings.usb.inep,
			port->settings.usb.outep,
			port->settings.usb.intep,
			confdesc->interface[interface].altsetting[altsetting].bInterfaceClass,
			confdesc->interface[interface].altsetting[altsetting].bInterfaceSubClass
			);
		libusb_free_config_descriptor (confdesc);
		return GP_OK;
	}
#if 0
	gp_port_set_error (port, _("Could not find USB device "
		"(vendor 0x%x, product 0x%x). Make sure this device "
		"is connected to the computer."), idvendor, idproduct);
#endif
	return GP_ERROR_IO_USB_FIND;
}

/* This function reads the Microsoft OS Descriptor and looks inside to
 * find if it is a MTP device. This is the similar to the way that
 * Windows Media Player 10 uses.
 * It is documented to some degree on various internet pages.
 */
static int
gp_libusb1_match_mtp_device(struct libusb_device *dev,int *configno, int *interfaceno, int *altsettingno)
{
	/* Marcus: Avoid this probing altogether, its too unstable on some devices */
	return 0;

#if 0
	char buf[1000], cmd;
	int ret,i,i1,i2, xifaces,xnocamifaces;
	usb_dev_handle *devh;

	/* All of them are "vendor specific" device class */
#if 0
	if ((desc.bDeviceClass!=0xff) && (desc.bDeviceClass!=0))
		return 0;
#endif
	if (dev->config) {
		xifaces = xnocamifaces = 0;
		for (i = 0; i < desc.bNumConfigurations; i++) {
			unsigned int j;

			for (j = 0; j < dev->config[i].bNumInterfaces; j++) {
				int k;
				xifaces++;

				for (k = 0; k < dev->config[i].interface[j].num_altsetting; k++) {
					struct usb_interface_descriptor *intf = &dev->config[i].interface[j].altsetting[k]; 
					if (	(intf->bInterfaceClass == LIBUSB_CLASS_HID)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_PRINTER)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_AUDIO)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_HUB)	||
						(intf->bInterfaceClass == LIBUSB_CLASS_COMM)	||
						(intf->bInterfaceClass == 0xe0)	/* wireless/bluetooth*/
					)
						xnocamifaces++;
				}
			}
		}
	}
	if (xifaces == xnocamifaces) /* only non-camera ifaces */
		return 0;

	devh = usb_open (dev);
	if (!devh)
		return 0;

	/*
	 * Loop over the device configurations and interfaces. Nokia MTP-capable 
	 * handsets (possibly others) typically have the string "MTP" in their 
	 * MTP interface descriptions, that's how they can be detected, before
	 * we try the more esoteric "OS descriptors" (below).
	 */
	if (dev->config) {
		for (i = 0; i < desc.bNumConfigurations; i++) {
			unsigned int j;

			for (j = 0; j < dev->config[i].bNumInterfaces; j++) {
				int k;
				for (k = 0; k < dev->config[i].interface[j].num_altsetting; k++) {
					buf[0] = '\0';
					ret = usb_get_string_simple(devh, 
						dev->config[i].interface[j].altsetting[k].iInterface, 
						(char *) buf, 
						1024);
					if (ret < 3)
						continue;
					if (strcmp((char *) buf, "MTP") == 0) {
						GP_LOG_D ("Configuration %d, interface %d, altsetting %d:\n", i, j, k);
						GP_LOG_D ("   Interface description contains the string \"MTP\"\n");
						GP_LOG_D ("   Device recognized as MTP, no further probing.\n");
						goto found;
					}
				}
			}
		}
	}
	/* get string descriptor at 0xEE */
	ret = usb_get_descriptor (devh, 0x03, 0xee, buf, sizeof(buf));
	if (ret > 0) GP_LOG_DATA (buf, ret, "get_MS_OSD");
	if (ret < 10) goto errout;
	if (!((buf[2] == 'M') && (buf[4]=='S') && (buf[6]=='F') && (buf[8]=='T')))
		goto errout;
	cmd = buf[16];
	ret = usb_control_msg (devh, USB_ENDPOINT_IN|USB_RECIP_DEVICE|LIBUSB_REQUEST_TYPE_VENDOR, cmd, 0, 4, buf, sizeof(buf), 1000);
	if (ret == -1) {
		GP_LOG_E ("control message says %d\n", ret);
		goto errout;
	}
	if (buf[0] != 0x28) {
		GP_LOG_E ("ret is %d, buf[0] is %x\n", ret, buf[0]);
		goto errout;
	}
	if (ret > 0) GP_LOG_DATA (buf, ret, "get_MS_ExtDesc");
	if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
		GP_LOG_E ("buf at 0x12 is %02x%02x%02x\n", buf[0x12], buf[0x13], buf[0x14]);
		goto errout;
	}
	ret = usb_control_msg (devh, USB_ENDPOINT_IN|USB_RECIP_DEVICE|LIBUSB_REQUEST_TYPE_VENDOR, cmd, 0, 5, buf, sizeof(buf), 1000);
	if (ret == -1) goto errout;
	if (buf[0] != 0x28) {
		GP_LOG_E ("ret is %d, buf[0] is %x\n", ret, buf[0]);
		goto errout;
	}
	if (ret > 0) GP_LOG_DATA (buf, ret, "get_MS_ExtProp");
	if ((buf[0x12] != 'M') || (buf[0x13] != 'T') || (buf[0x14] != 'P')) {
		GP_LOG_E ("buf at 0x12 is %02x%02x%02x\n", buf[0x12], buf[0x13], buf[0x14]);
		goto errout;
	}

found:
	usb_close (devh);

	/* Now chose a nice interface for us to use ... Just take the first. */

	if (desc.bNumConfigurations > 1)
		GP_LOG_E ("The device has %d configurations!\n", desc.bNumConfigurations);
	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct usb_config_descriptor *config =
			&dev->config[i];

		if (config->bNumInterfaces > 1)
			GP_LOG_E ("The configuration has %d interfaces!\n", config->bNumInterfaces);
		for (i1 = 0; i1 < config->bNumInterfaces; i1++) {
			struct usb_interface *interface =
				&config->interface[i1];

			if (interface->num_altsetting > 1)
				GP_LOG_E ("The interface has %d altsettings!\n", interface->num_altsetting);
			for (i2 = 0; i2 < interface->num_altsetting; i2++) {
				*configno = i;
				*interfaceno = i1;
				*altsettingno = i2;
				return 1;
			}
		}
	}
	return 1;
errout:
	usb_close (devh);
	return 0;
#endif
}

static int
gp_libusb1_match_device_by_class(struct libusb_device *dev, int class, int subclass, int protocol, int *configno, int *interfaceno, int *altsettingno)
{
	int i, i1, i2;
	struct libusb_device_descriptor desc;

	if (class == 666) /* Special hack for MTP devices with MS OS descriptors. */
		return gp_libusb1_match_mtp_device (dev, configno, interfaceno, altsettingno);

	if (LOG_ON_LIBUSB_E (libusb_get_device_descriptor(dev, &desc)))
		return 0;

	if (desc.bDeviceClass == class &&
	    (subclass == -1 ||
	     desc.bDeviceSubClass == subclass) &&
	    (protocol == -1 ||
	     desc.bDeviceProtocol == protocol))
		return 1;


	for (i = 0; i < desc.bNumConfigurations; i++) {
		struct libusb_config_descriptor *config;

		if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (dev, i, &config)))
			continue;

		for (i1 = 0; i1 < config->bNumInterfaces; i1++) {
			const struct libusb_interface *interface =
				&config->interface[i1];

			for (i2 = 0; i2 < interface->num_altsetting; i2++) {
				const struct libusb_interface_descriptor *altsetting =
					&interface->altsetting[i2];

				if (altsetting->bInterfaceClass == class &&
				    (subclass == -1 ||
				     altsetting->bInterfaceSubClass == subclass) &&
				    (protocol == -1 ||
				     altsetting->bInterfaceProtocol == protocol)) {
					*configno = i;
					*interfaceno = i1;
					*altsettingno = i2;

					libusb_free_config_descriptor (config);
					return 2;
				}
			}
		}
		libusb_free_config_descriptor (config);
	}
	return 0;
}

static int
gp_libusb1_find_device_by_class_lib(GPPort *port, int class, int subclass, int protocol)
{
	char *s;
	int d, busnr = 0, devnr = 0;
	GPPortPrivateLibrary *pl;

	C_PARAMS (port);

	pl = port->pl;

	s = strchr (port->settings.usb.port,':');
	if (s && (s[1] != '\0')) { /* usb:%d,%d */
		if (sscanf (s+1, "%d,%d", &busnr, &devnr) != 2) {
			devnr = 0;
			sscanf (s+1, "%d", &busnr);
		}
	}
	/*
	 * 0x00 class is not valid.
	 * 0x00 subclass and protocol is ok.
	 * Should the USB layer report that ? I don't know.
	 * Better to check here.
	 */
	C_PARAMS (class);

	pl->nrofdevs = load_devicelist (port->pl);
	for (d = 0; d < pl->nrofdevs; d++) {
		struct libusb_config_descriptor *confdesc;
		int i, ret, config = -1, interface = -1, altsetting = -1;

		if (busnr && (busnr != libusb_get_bus_number (pl->devs[d])))
			continue;
		if (devnr && (devnr != libusb_get_device_address (pl->devs[d])))
			continue;

		GP_LOG_D ("Looking for USB device (class 0x%x, subclass, 0x%x, protocol 0x%x)...",
			  class, subclass, protocol);

		ret = gp_libusb1_match_device_by_class(pl->devs[d], class, subclass, protocol, &config, &interface, &altsetting);
		if (!ret)
			continue;

		port->pl->d = pl->devs[d];
		GP_LOG_D ("Found USB class device (class 0x%x, subclass, 0x%x, protocol 0x%x)",
			  class, subclass, protocol);

		if (LOG_ON_LIBUSB_E (libusb_get_config_descriptor (pl->devs[d], config, &confdesc)))
			continue;

		/* Set the defaults */
		port->settings.usb.config = confdesc->bConfigurationValue;
		port->settings.usb.interface = confdesc->interface[interface].altsetting[altsetting].bInterfaceNumber;
		port->settings.usb.altsetting = confdesc->interface[interface].altsetting[altsetting].bAlternateSetting;

		port->settings.usb.inep  = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.outep = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_OUT, LIBUSB_TRANSFER_TYPE_BULK);
		port->settings.usb.intep = gp_libusb1_find_ep(pl->devs[d], config, interface, altsetting, LIBUSB_ENDPOINT_IN, LIBUSB_TRANSFER_TYPE_INTERRUPT);
		port->settings.usb.maxpacketsize = 0;
		GP_LOG_D ("inep to look for is %02x", port->settings.usb.inep);
		for (i=0;i<confdesc->interface[interface].altsetting[altsetting].bNumEndpoints;i++) {
			if (port->settings.usb.inep == confdesc->interface[interface].altsetting[altsetting].endpoint[i].bEndpointAddress) {
				port->settings.usb.maxpacketsize = confdesc->interface[interface].altsetting[altsetting].endpoint[i].wMaxPacketSize;
				break;
			}
		}
		GP_LOG_D ("Detected defaults: config %d, interface %d, altsetting %d, "
			  "idVendor ID %04x, idProduct %04x, inep %02x, outep %02x, intep %02x",
			port->settings.usb.config,
			port->settings.usb.interface,
			port->settings.usb.altsetting,
			pl->descs[d].idVendor,
			pl->descs[d].idProduct,
			port->settings.usb.inep,
			port->settings.usb.outep,
			port->settings.usb.intep
		);
		libusb_free_config_descriptor (confdesc);
		return GP_OK;
	}
#if 0
	gp_port_set_error (port, _("Could not find USB device "
		"(class 0x%x, subclass 0x%x, protocol 0x%x). Make sure this device "
		"is connected to the computer."), class, subclass, protocol);
#endif
	return GP_ERROR_IO_USB_FIND;
}

GPPortOperations *
gp_port_library_operations (void)
{
	GPPortOperations *ops;

	ops = calloc (1, sizeof (GPPortOperations));
	if (!ops)
		return (NULL);

	ops->init   = gp_libusb1_init;
	ops->exit   = gp_libusb1_exit;
	ops->open   = gp_libusb1_open;
	ops->close  = gp_libusb1_close;
	ops->read   = gp_libusb1_read;
	ops->reset  = gp_libusb1_reset;
	ops->write  = gp_libusb1_write;
	ops->check_int = gp_libusb1_check_int;
	ops->update = gp_libusb1_update;
	ops->clear_halt = gp_libusb1_clear_halt_lib;
	ops->msg_write  = gp_libusb1_msg_write_lib;
	ops->msg_read   = gp_libusb1_msg_read_lib;
	ops->msg_interface_write  = gp_libusb1_msg_interface_write_lib;
	ops->msg_interface_read   = gp_libusb1_msg_interface_read_lib;
	ops->msg_class_write  = gp_libusb1_msg_class_write_lib;
	ops->msg_class_read   = gp_libusb1_msg_class_read_lib;
	ops->find_device = gp_libusb1_find_device_lib;
	ops->find_device_by_class = gp_libusb1_find_device_by_class_lib;

	return (ops);
}
