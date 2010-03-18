/* Direct IO to USB Mass storage devices port library for Linux
 * 
 *   Copyright (c) 2010 Hans de Goede <hdegoede@redhat.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "config.h"
#include <gphoto2/gphoto2-port-library.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_LOCKDEV
#  include <lockdev.h>
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

/* From Linux asm/fcntl.h */
#ifndef O_DIRECT
#define O_DIRECT        00040000        /* direct disk access hint */
#endif

#define CHECK(result) {int r=(result); if (r<0) return (r);}

struct _GPPortPrivateLibrary {
	int fd;       /* Device handle */
};

GPPortType
gp_port_library_type () 
{
	return GP_PORT_USB_DISK_DIRECT;
}

static int
gp_port_usbdiskdirect_lock (GPPort *port, const char *path)
{
#ifdef HAVE_LOCKDEV
	int pid;

	gp_log (GP_LOG_DEBUG, "gphoto2-port-usbdiskdirect",
		"Trying to lock '%s'...", path);

	pid = dev_lock (path);
	if (pid) {
		if (port) {
			if (pid > 0)
				gp_port_set_error (port, _("Device '%s' is "
					"locked by pid %d"), path, pid);
			else
				gp_port_set_error (port, _("Device '%s' could "
					"not be locked (dev_lock returned "
					"%d)"), path, pid);
		}
		return GP_ERROR_IO_LOCK;
	}
#else
# ifdef __GCC__
#  warning No locking library found. 
#  warning You will run into problems if you use
#  warning gphoto2 with a usbdiskdirect picframe in 
#  warning combination with Konqueror (KDE) or Nautilus (GNOME).
#  warning This will *not* concern USB cameras.
# endif
#endif

	return GP_OK;
}

static int
gp_port_usbdiskdirect_unlock (GPPort *port, const char *path)
{
#ifdef HAVE_LOCKDEV
	int pid;

	pid = dev_unlock (path, 0);
	if (pid) {
		if (port) {
			if (pid > 0)
				gp_port_set_error (port, _("Device '%s' could "
					"not be unlocked as it is locked by "
					"pid %d."), path, pid);
			else
				gp_port_set_error (port, _("Device '%s' could "
					"not be unlocked (dev_unlock "
					"returned %d)"), path, pid);
		}
		return GP_ERROR_IO_LOCK;
	}
#endif /* !HAVE_LOCKDEV */

	return GP_OK;
}

static const char *
gp_port_usbdiskdirect_resolve_symlink (const char *link)
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
gp_port_usbdiskdirect_get_usb_id (const char *disk,
	unsigned short *vendor_id, unsigned short *product_id)
{
	FILE *f;
	char c, *s, buf[32], path[PATH_MAX + 1];

	snprintf (path, sizeof (path), "/sys/block/%s", disk);
	snprintf (path, sizeof (path), "%s/../../../../../modalias",
		  gp_port_usbdiskdirect_resolve_symlink(path));

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
	GPPortInfo info;
	unsigned short vendor_id, product_id;

	dir = opendir ("/sys/block");
	if (dir == NULL)
		return GP_OK;

	while ((dirent = readdir (dir))) {
		char path[4096];
		if (dirent->d_name[0] != 's' ||
		    dirent->d_name[1] != 'd' ||
		    dirent->d_name[2] < 'a' ||
		    dirent->d_name[2] > 'z')
			continue;

		if (gp_port_usbdiskdirect_get_usb_id (dirent->d_name,
				&vendor_id, &product_id) != GP_OK)
			continue; /* Not a usb device */

		gp_port_info_new (&info);
		gp_port_info_set_type (info, GP_PORT_USB_DISK_DIRECT);
		snprintf (path, sizeof (path),
			  "usbdiskdirect:/dev/%s",
			  dirent->d_name);
		gp_port_info_set_path (info, path);
		gp_port_info_set_name (info, _("USB Mass Storage direct IO"));
		CHECK (gp_port_info_list_append (list, info))
	}
	return GP_OK;
}

static int
gp_port_usbdiskdirect_init (GPPort *port)
{
	port->pl = calloc (1, sizeof (GPPortPrivateLibrary));
	if (!port->pl)
		return GP_ERROR_NO_MEMORY;

	port->pl->fd = -1;

	return GP_OK;
}

static int
gp_port_usbdiskdirect_exit (GPPort *port)
{
	if (!port)
		return GP_ERROR_BAD_PARAMETERS;

	if (port->pl) {
		free (port->pl);
		port->pl = NULL;
	}

	return GP_OK;
}

static int
gp_port_usbdiskdirect_open (GPPort *port)
{
	int result, i;
	const int max_tries = 5;
	const char *path = port->settings.usbdiskdirect.path;

	result = gp_port_usbdiskdirect_lock (port, path);
	if (result != GP_OK) {
		for (i = 0; i < max_tries; i++) {
			result = gp_port_usbdiskdirect_lock (port, path);
			if (result == GP_OK)
				break;
			gp_log (GP_LOG_DEBUG, "gphoto2-port-usbdiskdirect",
				"Failed to get a lock, trying again...");
			sleep (1);
		}
		CHECK (result)
	}
	port->pl->fd = open (path, O_RDWR|O_DIRECT|O_SYNC);
	if (port->pl->fd == -1) {
		gp_port_usbdiskdirect_unlock (port, path);
		gp_port_set_error (port, _("Failed to open '%s' (%m)."), path);
		return GP_ERROR_IO;
	}

	return GP_OK;
}

static int
gp_port_usbdiskdirect_close (GPPort *port)
{
	if (!port || port->pl->fd == -1)
		return GP_OK;

	if (close (port->pl->fd) == -1) {
		gp_port_set_error (port, _("Could not close "
			"'%s' (%m)."), port->settings.usbdiskdirect.path);
		return GP_ERROR_IO;
	}
	port->pl->fd = -1;

	CHECK (gp_port_usbdiskdirect_unlock (port,
					port->settings.usbdiskdirect.path))

	return GP_OK;
}

static int gp_port_usbdiskdirect_seek (GPPort *port, int offset, int whence)
{
	off_t ret;

	if (!port)
		return GP_ERROR_BAD_PARAMETERS;

	/* The device needs to be opened for that operation */
	if (port->pl->fd == -1)
		CHECK (gp_port_usbdiskdirect_open (port))

	ret = lseek (port->pl->fd, offset, whence);
	if (ret == -1) {
		gp_port_set_error (port, _("Could not seek to offset: %x on "
			"'%s' (%m)."), offset,
			port->settings.usbdiskdirect.path);
		return GP_ERROR_IO;
	}

	return ret;
}

static int
gp_port_usbdiskdirect_write (GPPort *port, const char *bytes, int size)
{
	int ret;

	if (!port)
		return GP_ERROR_BAD_PARAMETERS;

	/* The device needs to be opened for that operation */
	if (port->pl->fd == -1)
		CHECK (gp_port_usbdiskdirect_open (port))

	ret = write (port->pl->fd, bytes, size);
	if (ret < 0) {
		gp_port_set_error (port, _("Could not write to '%s' (%m)."),
				   port->settings.usbdiskdirect.path);
		return GP_ERROR_IO;
	}

	return ret;
}

static int
gp_port_usbdiskdirect_read (GPPort *port, char *bytes, int size)
{
	int ret;

	if (!port)
		return GP_ERROR_BAD_PARAMETERS;

	/* The device needs to be opened for that operation */
	if (port->pl->fd == -1)
		CHECK (gp_port_usbdiskdirect_open (port))

	ret = read (port->pl->fd, bytes, size);
	if (ret < 0) {
		gp_port_set_error (port, _("Could not read from '%s' (%m)."),
				   port->settings.usbdiskdirect.path);
		return GP_ERROR_IO;
	}

	return ret;
}

static int
gp_port_usbdiskdirect_update (GPPort *port)
{
	if (!port)
		return GP_ERROR_BAD_PARAMETERS;

	memcpy (&port->settings, &port->settings_pending,
		sizeof (port->settings));

	return GP_OK;
}

static int
gp_port_usbdiskdirect_find_device(GPPort *port, int idvendor, int idproduct)
{
	unsigned short vendor_id, product_id;
	const char *disk;

	if (!port)
		return GP_ERROR_BAD_PARAMETERS;

	disk = strrchr (port->settings.usbdiskdirect.path, '/');
	if (!disk)
		return GP_ERROR_BAD_PARAMETERS;
	disk++;

	CHECK (gp_port_usbdiskdirect_get_usb_id (disk,
						 &vendor_id, &product_id))
	if (vendor_id != idvendor || product_id != idproduct)
		return GP_ERROR_IO_USB_FIND;

	return GP_OK;
}

GPPortOperations *
gp_port_library_operations ()
{
	GPPortOperations *ops;

	ops = malloc (sizeof (GPPortOperations));
	if (!ops)
		return (NULL);
	memset (ops, 0, sizeof (GPPortOperations)); 

	ops->init   = gp_port_usbdiskdirect_init;
	ops->exit   = gp_port_usbdiskdirect_exit;
	ops->open   = gp_port_usbdiskdirect_open;
	ops->close  = gp_port_usbdiskdirect_close;
	ops->seek   = gp_port_usbdiskdirect_seek;
	ops->read   = gp_port_usbdiskdirect_read;
	ops->write  = gp_port_usbdiskdirect_write;
	ops->update = gp_port_usbdiskdirect_update;
	ops->find_device = gp_port_usbdiskdirect_find_device;

	return (ops);
}
