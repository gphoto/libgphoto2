/* SCSI commands to USB Mass storage devices port library for Linux
 * 
 *   Copyright (c) 2010-2012 Hans de Goede <hdegoede@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the 
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE	/* for flock */

#include "config.h"
#include <gphoto2/gphoto2-port-library.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif
#ifdef HAVE_SYS_FILE_H
# include <sys/file.h>
# ifndef LOCK_SH
#  define LOCK_SH 1
# endif
# ifndef LOCK_EX
#  define LOCK_EX 2
# endif
# ifndef LOCK_NB
#  define LOCK_NB 4
# endif
# ifndef LOCK_UN
#  define LOCK_UN 4
# endif
#endif
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SCSI_SG_H
# include <scsi/sg.h>
#endif

#include <gphoto2/gphoto2-port-result.h>
#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port.h>

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

#define CHECK(result) {int r=(result); if (r<0) return (r);}

struct _GPPortPrivateLibrary {
	int fd;       /* Device handle */
};

GPPortType
gp_port_library_type () 
{
	return GP_PORT_USB_SCSI;
}

static int
gp_port_usbscsi_lock (GPPort *port)
{
#if HAVE_FLOCK
	GP_LOG_D ("Trying to lock '%s'...", port->settings.usbscsi.path);

	if (flock(port->pl->fd, LOCK_EX | LOCK_NB) != 0) {
		switch (errno) {
		case EWOULDBLOCK:
			gp_port_set_error (port,
				_("Device '%s' is locked by another app."),
				port->settings.usbscsi.path);
			return GP_ERROR_IO_LOCK;
		default:
			gp_port_set_error (port,
				_("Failed to lock '%s' (%m)."),
				port->settings.usbscsi.path);
			return GP_ERROR_IO;
		}
	}
#else
	GP_LOG_D ("Locking '%s' not possible, flock not availbale.", port->settings.usbscsi.path);
#endif
	return GP_OK;
}

static int
gp_port_usbscsi_unlock (GPPort *port)
{
#ifdef HAVE_FLOCK
	if (flock(port->pl->fd, LOCK_UN) != 0) {
		gp_port_set_error (port, _("Failed to unlock '%s' (%m)."),
				   port->settings.usbscsi.path);
		return GP_ERROR_IO;
	}
#endif
	return GP_OK;
}

static const char *
gp_port_usbscsi_resolve_symlink (const char *link)
{
	ssize_t ret;
	static char path[PATH_MAX + 1];
	char *slash, buf[PATH_MAX + 1];
	struct stat st;
	int len;

	snprintf (path, sizeof(path), "%s", link);

	do {
		ret = readlink (path, buf, PATH_MAX);
		if (ret < 0)
			return NULL;
		buf[ret] = 0;

		slash = strrchr (path, '/');
		if (buf[0] == '/' || slash == NULL) {
			snprintf (path, sizeof(path), "%s", buf);
		} else {
			*slash = 0;
			len = strlen (path);
			snprintf (path + len, sizeof (path) - len, "/%s", buf);
		}

		if (stat (path, &st))
			return NULL;
	} while (S_ISLNK(st.st_mode));

	return path;
}

static int 
gp_port_usbscsi_get_usb_id (const char *sg,
	unsigned short *vendor_id, unsigned short *product_id)
{
	FILE *f;
	char c, *s, buf[32], path[PATH_MAX + 1];
	const char *xpath;

	snprintf (path, sizeof (path), "/sys/class/scsi_generic/%s", sg);
	xpath = gp_port_usbscsi_resolve_symlink(path);
	if (xpath) {
		snprintf (path, sizeof (path), "%s/../../../../../modalias",
			  gp_port_usbscsi_resolve_symlink(path));
	} else {
		/* older kernels, perhaps also works on newer ones */
		snprintf (path, sizeof (path), "/sys/class/scsi_generic/%s/device/../../../modalias", sg);
	}

	f = fopen (path, "r");
	if (!f)
		return GP_ERROR_IO_SUPPORTED_USB;

	s = fgets (buf, sizeof(buf), f);
	fclose (f);

	if (!s)
		return GP_ERROR_IO_SUPPORTED_USB;

	if (sscanf (s, "usb:v%4hxp%4hx%c", vendor_id, product_id, &c) != 3 ||
	    c != 'd')
		return GP_ERROR_IO_SUPPORTED_USB;

	return GP_OK;
}

int
gp_port_library_list (GPPortInfoList *list)
{
	DIR *dir;
	struct dirent *dirent;
	int ret;
	GPPortInfo info;
	unsigned short vendor_id, product_id;

	dir = opendir ("/sys/class/scsi_generic");
	if (dir == NULL)
		return GP_OK;

	while ((dirent = readdir (dir))) {
		char path[4096];
		if (gp_port_usbscsi_get_usb_id (dirent->d_name,
				&vendor_id, &product_id) != GP_OK)
			continue; /* Not a usb device */

		gp_port_info_new (&info);
		gp_port_info_set_type (info, GP_PORT_USB_SCSI);
		snprintf (path, sizeof (path),
			  "usbscsi:/dev/%s",
			  dirent->d_name);
		gp_port_info_set_path (info, path);
		gp_port_info_set_name (info, _("USB Mass Storage raw SCSI"));
		ret = gp_port_info_list_append (list, info);
		if (ret < GP_OK) /* can only be out of memory */
			break;
	}
	closedir (dir);
	return GP_OK;
}

static int
gp_port_usbscsi_init (GPPort *port)
{
	C_MEM (port->pl = calloc (1, sizeof (GPPortPrivateLibrary)));

	port->pl->fd = -1;

	return GP_OK;
}

static int
gp_port_usbscsi_exit (GPPort *port)
{
	C_PARAMS (port);

	free (port->pl);
	port->pl = NULL;

	return GP_OK;
}

static int
gp_port_usbscsi_open (GPPort *port)
{
	int result, i;
	const int max_tries = 5;
	const char *path = port->settings.usbscsi.path;

	port->pl->fd = open (path, O_RDWR);
	if (port->pl->fd == -1) {
		gp_port_set_error (port, _("Failed to open '%s' (%m)."), path);
		return GP_ERROR_IO;
	}

	result = gp_port_usbscsi_lock (port);
	for (i = 0; i < max_tries && result == GP_ERROR_IO_LOCK; i++) {
		GP_LOG_D ("Failed to get a lock, trying again...");
		sleep (1);
		result = gp_port_usbscsi_lock (port);
	}
	if (result != GP_OK) {
		close (port->pl->fd);
		port->pl->fd = -1;
	}
	return result;
}

static int
gp_port_usbscsi_close (GPPort *port)
{
	int result;

	if (!port || port->pl->fd == -1)
		return GP_OK;

	result = gp_port_usbscsi_unlock (port);

	if (close (port->pl->fd) == -1) {
		gp_port_set_error (port, _("Could not close "
			"'%s' (%m)."), port->settings.usbscsi.path);
		return GP_ERROR_IO;
	}
	port->pl->fd = -1;

	return result;
}

static int gp_port_usbscsi_send_scsi_cmd (GPPort *port, int to_dev, char *cmd,
	int cmd_size, char *sense, int sense_size, char *data, int data_size)
{
#ifdef HAVE_SCSI_SG_H
	sg_io_hdr_t io_hdr;

	C_PARAMS (port);

	/* The device needs to be opened for that operation */
	if (port->pl->fd == -1)
		CHECK (gp_port_usbscsi_open (port))

	memset(sense, 0, sense_size);
	memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
	if (to_dev) {
		io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
	} else {
		memset (data, 0, data_size);
		io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
	}
	io_hdr.interface_id = 'S';
	io_hdr.cmdp = (unsigned char *)cmd;
	io_hdr.cmd_len = cmd_size;
	io_hdr.sbp = (unsigned char *)sense;
	io_hdr.mx_sb_len = sense_size;
	io_hdr.dxferp = (unsigned char *)data;
	io_hdr.dxfer_len = data_size;
	/*io_hdr.timeout = 1500;*/
	io_hdr.timeout = port->timeout;
	GP_LOG_D ("setting scsi command timeout to %d", port->timeout);
	if (io_hdr.timeout < 1500)
		io_hdr.timeout = 1500;

	if (ioctl (port->pl->fd, SG_IO, &io_hdr) < 0)
	{
		gp_port_set_error (port, _("Could not send scsi command to: "
			"'%s' (%m)."), port->settings.usbscsi.path);
		return GP_ERROR_IO;
	}
	return GP_OK;
#else
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

static int
gp_port_usbscsi_update (GPPort *port)
{
	C_PARAMS (port);

	memcpy (&port->settings, &port->settings_pending,
		sizeof (port->settings));

	return GP_OK;
}

static int
gp_port_usbscsi_find_device(GPPort *port, int idvendor, int idproduct)
{
	unsigned short vendor_id, product_id;
	const char *sg;

	C_PARAMS (port);

	sg = strrchr (port->settings.usbscsi.path, '/');
	C_PARAMS (sg);
	sg++;

	CHECK (gp_port_usbscsi_get_usb_id (sg, &vendor_id, &product_id))
	if (vendor_id != idvendor || product_id != idproduct)
		return GP_ERROR_IO_USB_FIND;

	return GP_OK;
}

GPPortOperations *
gp_port_library_operations ()
{
	GPPortOperations *ops;

	ops = calloc (1, sizeof (GPPortOperations));
	if (!ops)
		return (NULL);

	ops->init   = gp_port_usbscsi_init;
	ops->exit   = gp_port_usbscsi_exit;
	ops->open   = gp_port_usbscsi_open;
	ops->close  = gp_port_usbscsi_close;
	ops->send_scsi_cmd = gp_port_usbscsi_send_scsi_cmd;
	ops->update = gp_port_usbscsi_update;
	ops->find_device = gp_port_usbscsi_find_device;

	return (ops);
}
