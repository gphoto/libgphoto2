/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 * Copyright (C) 2000 Philippe Marzouk <pmarzouk@bigfoot.com>
 * Copyright (C) 2000 Edouard Lafargue <Edouard.Lafargue@bigfoot.com>
 * Copyright (C) 1999 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright (C) 1999 Scott Fritzinger <scottf@unr.edu>
 *
 * Based on work by:
 * Copyright (C) 1999 Beat Christen <spiff@longstreet.ch>
 * 	for the toshiba gPhoto library.
 *                   
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

#include <config.h>
#include <gphoto2-port-library.h>

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#if HAVE_TERMIOS_H
#include <termios.h>
#ifndef CRTSCTS
#define CRTSCTS  020000000000
#endif
#else
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <sgtty.h>
#endif

#if HAVE_BAUDBOY
#  include <baudboy.h>
#elif HAVE_TTYLOCK
#  include <ttylock.h>
#elif HAVE_LOCKDEV
#  include <lockdev.h>
#endif

#include <gphoto2-port-result.h>
#include <gphoto2-port-log.h>
#include <gphoto2-port.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
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

/* Linux */
#ifdef __linux
/* devfs is accounted for in the implementation */
#define GP_PORT_SERIAL_PREFIX   "/dev/ttyS%i"
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       32
#endif

/* BSD */
#if defined(__FreeBSD__) || defined(__NetBSD__)
#define GP_PORT_SERIAL_PREFIX   "/dev/tty0%i"
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       32
#endif

/* Darwin */
#if defined(__APPLE__)
/* This is the Keyspan USB serial adapter device (UNTESTED) */
#define GP_PORT_SERIAL_PREFIX   "/dev/tty.KeyUSA28X%i"
#define GP_PORT_SERIAL_RANGE_LOW        111
#define GP_PORT_SERIAL_RANGE_HIGH       1112
#endif

/* Solaris */
#ifdef sun
#define GP_PORT_SERIAL_PREFIX "/dev/tty%c"
#define GP_PORT_SERIAL_RANGE_LOW        'a'
#define GP_PORT_SERIAL_RANGE_HIGH       'z'
#endif

/* BeOS */
#ifdef beos
/* ????????????? */
#define GP_PORT_SERIAL_PREFIX NULL
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       0
#endif

/* Windows */
#ifdef WIN32
#define GP_PORT_SERIAL_PREFIX   "COM%i:"
#define GP_PORT_SERIAL_RANGE_LOW        1
#define GP_PORT_SERIAL_RANGE_HIGH       4
#endif

#ifdef OS2
#define GP_PORT_SERIAL_PREFIX   "COM%i"
#define GP_PORT_SERIAL_RANGE_LOW   1
#define GP_PORT_SERIAL_RANGE_HIGH  4
#endif

/* Others? */

/* Default */
#ifndef GP_PORT_SERIAL_PREFIX
#define GP_PORT_SERIAL_PREFIX   "/dev/cua%i"
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       0
#endif

struct _GPPortPrivateLibrary {
	int fd;       /* Device handle */
	int baudrate; /* Current speed */
};

static int gp_port_serial_check_speed (GPPort *dev);

GPPortType
gp_port_library_type () 
{
        return (GP_PORT_SERIAL);
}

static int
gp_port_serial_lock (GPPort *dev, const char *path)
{
#if HAVE_LOCKDEV
	int pid;
#endif

	gp_log (GP_LOG_DEBUG, "gphoto2-port-serial",
		"Trying to lock '%s'...", path);

#if HAVE_TTYLOCK || HAVE_BAUDBOY
	if (ttylock ((char*) path)) {
		if (dev)
			gp_port_set_error (dev, _("Could not lock device "
						  "'%s'"), path);
		return (GP_ERROR_IO_LOCK);
	}
#elif HAVE_LOCKDEV
	pid = dev_lock (path);
	if (pid) {
		if (dev) {
			if (pid > 0)
				gp_port_set_error (dev, _("Device '%s' is "
					"locked by pid %d"), path, pid);
			else
				gp_port_set_error (dev, _("Device '%s' could "
					"not be locked (dev_lock returned "
					"%d)"), path, pid);
		}
		return (GP_ERROR_IO_LOCK);
	}
#else
#warning No locking library found. 
#warning You will run into problems if you 
#warning use gphoto2 in combination with
#warning Konqueror (KDE) or Nautilus (GNOME).
#endif

	return (GP_OK);
}

static int
gp_port_serial_unlock (GPPort *dev, const char *path)
{
#if HAVE_TTYLOCK || HAVE_BAUDBOY
	if (ttyunlock ((char*) path)) {
		if (dev)
			gp_port_set_error (dev, _("Device '%s' could not be "
					   "unlocked."), path);
		return (GP_ERROR_IO_LOCK);
	}
#elif HAVE_LOCKDEV

	int pid;

	pid = dev_unlock (path, 0);
	if (pid) {
		if (dev) {
			if (pid > 0)
				gp_port_set_error (dev, _("Device '%s' could "
					"not be unlocked as it is locked by "
					"pid %d."), path, pid);
			else
				gp_port_set_error (dev, _("Device '%s' could "
					"not be unlocked (dev_unlock "
					"returned %d)"), path, pid);
		}
		return (GP_ERROR_IO_LOCK);
	}
#endif /* !HAVE_LOCKDEV */

	return (GP_OK);
}

int
gp_port_library_list (GPPortInfoList *list)
{
	GPPortInfo info;
	char path[1024], prefix[1024];
        int x, fd;
#ifdef __linux
        struct stat s;
#endif
#ifdef OS2
        int r, fh, option;
#endif

        /* Copy in the serial port prefix */
        strcpy (prefix, GP_PORT_SERIAL_PREFIX);

	/* On Linux systems, check for devfs */
#ifdef __linux
        if (!stat ("/dev/tts", &s))
		strcpy (prefix, "/dev/tts/%i");
#endif

        for (x=GP_PORT_SERIAL_RANGE_LOW; x<=GP_PORT_SERIAL_RANGE_HIGH; x++) {
		sprintf (path, prefix, x);

		/* OS/2 seems to need an additional check */
#ifdef OS2
		r = DosOpen (path, &fh, &option, 0, 0, 1,
			     OPEN_FLAGS_FAIL_ON_ERROR |
			     OPEN_SHARE_DENYREADWRITE, 0);
		DosClose(fh);
		if (r)
			continue;
#endif

		/* First of all, try to lock the device */
		if (gp_port_serial_lock (NULL, path) < 0)
			continue;
			
		/* Device locked. Try to open the device. */
		fd = open (path, O_RDONLY | O_NDELAY);
		if (fd < 0) {
			gp_port_serial_unlock (NULL, path);
			continue;
		}

		/*
		 * Device locked and opened. Close it, unlock it, and add
		 * it to the list.
		 */
		close (fd);
		gp_port_serial_unlock (NULL, path);
		info.type = GP_PORT_SERIAL;
		strncpy (info.path, "serial:", sizeof (info.path));
		strncat (info.path, path, sizeof (info.path));
		snprintf (info.name, sizeof (info.name),
			  _("Serial Port %i"), x);
		CHECK (gp_port_info_list_append (list, info));
        }

	/* Generic support */
	info.type = GP_PORT_SERIAL;
	strncpy (info.path, "^serial", sizeof (info.path));
	memset (info.name, 0, sizeof (info.name));
	CHECK (gp_port_info_list_append (list, info));

        return (GP_OK);
}

static int
gp_port_serial_init (GPPort *dev)
{
	if (!dev)
		return (GP_ERROR_BAD_PARAMETERS);

	dev->pl = malloc (sizeof (GPPortPrivateLibrary));
	if (!dev->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (dev->pl, 0, sizeof (GPPortPrivateLibrary));

	/* There is no default speed. */
	dev->pl->baudrate = -1;

        return GP_OK;
}

static int
gp_port_serial_exit (GPPort *dev)
{
	if (!dev)
		return (GP_ERROR_BAD_PARAMETERS);

	if (dev->pl) {
		free (dev->pl);
		dev->pl = NULL;
	}

        return GP_OK;
}

static int
gp_port_serial_open (GPPort *dev)
{
	int result, max_tries = 5, i;
#ifdef OS2
	int fd;
#endif
	char *port;

	/* Ports are named "serial:/dev/whatever/port" */
	port = strchr (dev->settings.serial.port, ':');
	if (!port)
		return GP_ERROR_UNKNOWN_PORT;
	port++;

	result = gp_port_serial_lock (dev, port);
	if (result != GP_OK) {
		for (i = 0; i < max_tries; i++) {
			result = gp_port_serial_lock (dev, port);
			if (result == GP_OK)
				break;
			gp_log (GP_LOG_DEBUG, "gphoto2-port-serial",
				"Failed to get a lock, trying again...");
			sleep (1);
		}
		if (result != GP_OK)
			return (result);
	}

#if defined(__FreeBSD__) || defined(__APPLE__)
        dev->pl->fd = open (port, O_RDWR | O_NOCTTY | O_NONBLOCK);
#elif OS2
        fd = open (port, O_RDWR | O_BINARY);
	dev->pl->fd = open (port, O_RDWR | O_BINARY);
        close(fd);
#else
        dev->pl->fd = open (port, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
#endif
        if (dev->pl->fd == -1) {
		gp_port_set_error (dev, _("Failed to open '%s' (%m)"), port);
		dev->pl->fd = 0;
                return GP_ERROR_IO_OPEN;
        }

        return GP_OK;
}

static int
gp_port_serial_close (GPPort *dev)
{
	int result;
	const char *path;

	if (!dev)
		return (GP_OK);

	if (dev->pl->fd) {
		if (close (dev->pl->fd) == -1) {
			gp_port_set_error (dev, _("Could not close device "
					   "(%s)"));
	                return GP_ERROR_IO_CLOSE;
	        }
		dev->pl->fd = 0;
	}

	/* Unlock the port */
	path = strchr (dev->settings.serial.port, ':');
	path++;
	result = gp_port_serial_unlock (dev, path);
	if (result < 0)
		return (result);

        return GP_OK;
}

static int
gp_port_serial_write (GPPort *dev, char *bytes, int size)
{
        int len, ret;

	if (!dev)
		return (GP_ERROR_BAD_PARAMETERS);

	/* The device needs to be opened for that operation */
	if (!dev->pl->fd)
		CHECK (gp_port_serial_open (dev));

	/* Make sure we are operating at the specified speed */
	CHECK (gp_port_serial_check_speed (dev));

        len = 0;
        while (len < size) {
		
		/*
		 * Make sure we write all data while handling
		 * the harmless errors
		 */
		ret = write (dev->pl->fd, bytes, size - len);
                if (ret == -1) {
                        switch (errno) {
                        case EAGAIN:
                        case EINTR:
                                ret = 0;
                                break;
                        default:
				gp_port_set_error (dev, _("Could not write "
					"to port (%m)"));
                                return GP_ERROR_IO_WRITE;
                        }
		}
		len += ret;
        }

        /* wait till all bytes are really sent */
#ifndef OS2
#if HAVE_TERMIOS_H
        tcdrain (dev->pl->fd);
#else
        ioctl (dev->pl->fd, TCDRAIN, 0);
#endif
#endif
        return GP_OK;
}


static int
gp_port_serial_read (GPPort *dev, char *bytes, int size)
{
        struct timeval timeout;
        fd_set readfs;          /* file descriptor set */
        int readen = 0, now;

	if (!dev)
		return (GP_ERROR_BAD_PARAMETERS);

	/* The device needs to be opened for that operation */
	if (!dev->pl->fd)
		CHECK (gp_port_serial_open (dev));

	/* Make sure we are operating at the specified speed */
	CHECK (gp_port_serial_check_speed (dev));

        FD_ZERO (&readfs);
        FD_SET (dev->pl->fd, &readfs);

        while (readen < size) {

		/* Set timeout value within input loop */
                timeout.tv_usec = (dev->timeout % 1000) * 1000;
                timeout.tv_sec = (dev->timeout / 1000); 

		/* Any data available? */
                if (!select (dev->pl->fd + 1, &readfs, NULL, NULL, &timeout))
                        return GP_ERROR_IO_TIMEOUT;
		if (!FD_ISSET (dev->pl->fd, &readfs))
			return (GP_ERROR_IO_TIMEOUT);

		/* Read the bytes */
		now = read (dev->pl->fd, bytes, size - readen);
		if (now < 0)
			return GP_ERROR_IO_READ;
		bytes += now;
		readen += now;
        }

        return readen;
}

static int
gp_port_serial_get_pin (GPPort *dev, int pin, int *level)
{
        int j, bit;

        switch(pin) {
                case PIN_RTS:
                        bit = TIOCM_RTS;
                        break;
                case PIN_DTR:
                        bit = TIOCM_DTR;
                        break;
                case PIN_CTS:
                        bit = TIOCM_CTS;
                        break;
                case PIN_DSR:
                        bit = TIOCM_DSR;
                        break;
                case PIN_CD:
                        bit = TIOCM_CD;
                        break;
                case PIN_RING:
                        bit = TIOCM_RNG;
                        break;
                default:
                        return GP_ERROR_IO_PIN;
        }

        if (ioctl(dev->pl->fd, TIOCMGET, &j) < 0) {
                perror("gp_port_serial_status (Getting hardware status bits)");
                return GP_ERROR_IO_PIN;
        }
        *level = j & bit;
        return (GP_OK);
}

/*
* Set the status of lines in the serial port
*
* level is 0 for off and 1 for on
*
*/
static int
gp_port_serial_set_pin (GPPort *dev, int pin, int level)
{
        int bit,request;

        switch(pin) {
                case PIN_RTS:
                        bit = TIOCM_RTS;
                        break;
                case PIN_DTR:
                        bit = TIOCM_DTR;
                        break;
                case PIN_CTS:
                        bit = TIOCM_CTS;
                        break;
                case PIN_DSR:
                        bit = TIOCM_DSR;
                        break;
                case PIN_CD:
                        bit = TIOCM_CD;
                        break;
                case PIN_RING:
                        bit = TIOCM_RNG;
                        break;
                default:
                        return GP_ERROR_IO_PIN;
        }

        switch(level) {
                case 0:
                        request = TIOCMBIS;
                        break;
                case 1:
                        request = TIOCMBIC;
                        break;
                default:
                        return GP_ERROR_IO_PIN;
        }

        if (ioctl (dev->pl->fd, request, &bit) <0) {
            perror("ioctl(TIOCMBI[CS])");
            return GP_ERROR_IO_PIN;
        }

        return GP_OK;
}

static int
gp_port_serial_flush (GPPort *dev, int direction)
{
	/* The device needs to be opened for that operation */
	if (!dev->pl->fd)
		CHECK (gp_port_serial_open (dev));

	/* Make sure we are operating at the specified speed */
	CHECK (gp_port_serial_check_speed (dev));

#if HAVE_TERMIOS_H
	if (tcflush (dev->pl->fd, direction ? TCOFLUSH : TCIFLUSH) < 0)
		return (GP_ERROR_IO_SERIAL_FLUSH);
#else
#warning SERIAL FLUSH NOT IMPLEMENTED FOR NON TERMIOS SYSTEMS!
	return GP_ERROR_IO_SERIAL_FLUSH;
#endif

	return (GP_OK);
}

static speed_t
gp_port_serial_baudconv (int baudrate)
{
#define BAUDCASE(x)     case (x): { ret = B##x; break; }
        speed_t ret;

        switch (baudrate) {

                /* POSIX defined baudrates */
                BAUDCASE(0);
                BAUDCASE(50);
                BAUDCASE(75);
                BAUDCASE(110);
                BAUDCASE(134);
                BAUDCASE(150);
                BAUDCASE(200);
                BAUDCASE(300);
                BAUDCASE(600);
                BAUDCASE(1200);
                BAUDCASE(1800);
                BAUDCASE(2400);
                BAUDCASE(4800);
                BAUDCASE(9600);
                BAUDCASE(19200);
                BAUDCASE(38400);

                /* non POSIX values */
#ifdef B7200
                BAUDCASE(7200);
#endif
#ifdef B14400
                BAUDCASE(14400);
#endif
#ifdef B28800
                BAUDCASE(28800);
#endif
#ifdef B57600
                BAUDCASE(57600);
#endif
#ifdef B115200
                BAUDCASE(115200);
#endif
#ifdef B230400
                BAUDCASE(230400);
#endif
        default:
		ret = (speed_t) baudrate;
		gp_log (GP_LOG_DEBUG, "gphoto2-port-serial", "Baudrate %d "
			"unknown - using as is", baudrate);
        }

        return ret;
#undef BAUDCASE
}

static int
gp_port_serial_check_speed (GPPort *dev)
{
#ifdef OS2
	ULONG rc;
	ULONG   ulParmLen = 2;     /* Maximum size of the parameter packet */
#else
#ifdef HAVE_TERMIOS_H
	struct termios tio;
#else
	struct sgttyb ttyb;
#endif
#endif

	/*
	 * We need an open device in order to set the speed. If there is no
	 * open device, postpone setting of speed.
	 */
	if (!dev->pl->fd)
		return (GP_OK);

	/* If baudrate is up to date, do nothing */
	if (dev->pl->baudrate == dev->settings.serial.speed)
		return (GP_OK);

	gp_log (GP_LOG_DEBUG, "gphoto2-port-serial",
		"Setting baudrate to %d...", dev->settings.serial.speed);

#ifdef OS2
	rc = DosDevIOCtl (dev->pl->fd,       /* Device handle               */
			  0x0001,            /* Serial-device control       */
			  0x0043,            /* Sets bit rate               */
			  (PULONG) &(dev->settings.serial.speed),
			  sizeof(baudrate),  /* Max. size of parameter list */
			  &ulParmLen,        /* Size of parameter packet    */
			  NULL,              /* No data packet              */
			  0,                 /* Maximum size of data packet */
			  NULL);             /* Size of data packet         */
	if(rc != 0)
		printf("DosDevIOCtl baudrate error:%d\n",rc);
#else /* !OS2 */
#ifdef HAVE_TERMIOS_H
        if (tcgetattr(dev->pl->fd, &tio) < 0) {
		gp_port_set_error (dev, _("Could not set the baudrate to %d"),
				   dev->settings.serial.speed);
                return GP_ERROR_IO_SERIAL_SPEED;
        }
        tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;

        /* Set into raw, no echo mode */
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__)
        tio.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL |
                         IXANY | IXON | IXOFF | INPCK | ISTRIP);
#else
        tio.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | IUCLC |
                         IXANY | IXON | IXOFF | INPCK | ISTRIP);
#endif
        tio.c_iflag |= (BRKINT | IGNPAR);
        tio.c_oflag &= ~OPOST;
        tio.c_lflag &= ~(ICANON | ISIG | ECHO | ECHONL | ECHOE |
                         ECHOK | IEXTEN);
        tio.c_cflag &= ~(CRTSCTS | PARENB | PARODD);
        tio.c_cflag |= CLOCAL | CREAD;

        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;

        cfsetispeed (&tio, gp_port_serial_baudconv (dev->settings.serial.speed));
        cfsetospeed (&tio, gp_port_serial_baudconv (dev->settings.serial.speed));

        if (tcsetattr (dev->pl->fd, TCSANOW, &tio) < 0) {
                perror("tcsetattr");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
        if (fcntl(dev->pl->fd, F_SETFL, 0) < 0) {    /* clear O_NONBLOCK */
                perror("fcntl F_SETFL");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
#else /* !HAVE_TERMIOS_H */
        if (ioctl (dev->pl->fd, TIOCGETP, &ttyb) < 0) {
                perror("ioctl(TIOCGETP)");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
        ttyb.sg_ispeed = dev->settings.serial.speed;
        ttyb.sg_ospeed = dev->settings.serial.speed;
        ttyb.sg_flags = 0;

        if (ioctl(dev->pl->fd, TIOCSETP, &ttyb) < 0) {
                perror("ioctl(TIOCSETP)");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
#endif
#endif

	dev->pl->baudrate = dev->settings.serial.speed;
        return GP_OK;
}

static int
gp_port_serial_update (GPPort *dev)
{
	memcpy (&dev->settings, &dev->settings_pending, sizeof (dev->settings));

	/*
	 * The speed is set prior the next read or write operation
	 * (if needed)
	 */

	return GP_OK;
}

static int
gp_port_serial_send_break (GPPort *dev, int duration)
{
	/* The device needs to be opened for that operation */
	if (!dev->pl->fd)
		CHECK (gp_port_serial_open (dev));

	/* Make sure we are operating at the specified speed */
	CHECK (gp_port_serial_check_speed (dev));

        /* Duration is in milliseconds */
#if HAVE_TERMIOS_H
        tcsendbreak (dev->pl->fd, duration / 310);
        tcdrain (dev->pl->fd);
#else
#  warning SEND BREAK NOT IMPLEMENTED FOR NON TERMIOS SYSTEMS!
	return GP_ERROR_IO_SERIAL_BREAK;
#endif

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

        ops->init   = gp_port_serial_init;
        ops->exit   = gp_port_serial_exit;
        ops->open   = gp_port_serial_open;
        ops->close  = gp_port_serial_close;
        ops->read   = gp_port_serial_read;
        ops->write  = gp_port_serial_write;
        ops->update = gp_port_serial_update;
        ops->get_pin = gp_port_serial_get_pin;
        ops->set_pin = gp_port_serial_set_pin;
        ops->send_break = gp_port_serial_send_break;
        ops->flush  = gp_port_serial_flush;

        return (ops);
}
