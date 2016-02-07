/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 * Copyright 2000 Philippe Marzouk <pmarzouk@bigfoot.com>
 * Copyright 2000 Edouard Lafargue <Edouard.Lafargue@bigfoot.com>
 * Copyright 1999 Johannes Erdfelt <johannes@erdfelt.com>
 * Copyright 1999 Scott Fritzinger <scottf@unr.edu>
 *
 * Based on work by:
 * Copyright 1999 Beat Christen <spiff@longstreet.ch>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE
/* Solaris needs this */
#define __EXTENSIONS__

#include "config.h"
#include <gphoto2/gphoto2-port-library.h>


#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifdef HAVE_TERMIOS_H
#  include <termios.h>
#  ifndef CRTSCTS
#    define CRTSCTS  020000000000
#  endif
#else
#  include <sgtty.h>
#endif

#ifdef HAVE_TTYLOCK
#  include <ttylock.h>
#elif defined(HAVE_LOCKDEV)
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

#define CHECK(result) {int r=(result); if (r<0) return (r);}

/* Linux */
#ifdef __linux__
/* devfs is accounted for in the implementation */
#define GP_PORT_SERIAL_PREFIX   "/dev/ttyS%i"
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       32

#ifndef IXANY
#define IXANY  0004000
#endif

#endif

/* FreeBSD */
#ifdef __FreeBSD__
#if __FreeBSD_version < 600000
#define GP_PORT_SERIAL_PREFIX   "/dev/cuaa%x"
#else
#define GP_PORT_SERIAL_PREFIX   "/dev/cuad%x"
#endif
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       (0xf)
#endif

/* OpenBSD */
/* devices appear to go up to /dev/cuac7, but we just list the first 4 */
#ifdef __OpenBSD__
# define GP_PORT_SERIAL_PREFIX   "/dev/cua%02x"
# define GP_PORT_SERIAL_RANGE_LOW        0
# define GP_PORT_SERIAL_RANGE_HIGH       3
#endif

/* NetBSD */
#ifdef __NetBSD__
#define GP_PORT_SERIAL_PREFIX   "/dev/tty0%i"
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       32
#endif

/* Tru64 UNIX */
#ifdef __osf__
#define GP_PORT_SERIAL_PREFIX   "/dev/tty%02i"
#define GP_PORT_SERIAL_RANGE_LOW        0
#define GP_PORT_SERIAL_RANGE_HIGH       4
#endif

/* Darwin */
#ifdef __APPLE__
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

/* IRIX */
#if defined(__sgi)
#define GP_PORT_SERIAL_PREFIX "/dev/ttyd%i"
#define GP_PORT_SERIAL_RANGE_LOW       1 
#define GP_PORT_SERIAL_RANGE_HIGH     11
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
#if defined(HAVE_LOCKDEV)
	int pid;
#endif

	GP_LOG_D ("Trying to lock '%s'...", path);

#if defined(HAVE_TTYLOCK)
	if (ttylock ((char*) path)) {
		if (dev)
			gp_port_set_error (dev, _("Could not lock device "
						  "'%s'"), path);
		return (GP_ERROR_IO_LOCK);
	}
#define __HAVE_LOCKING	
#elif defined(HAVE_LOCKDEV)
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
#define __HAVE_LOCKING	
#endif

#ifndef __HAVE_LOCKING
# ifdef __GCC__
#  warning No locking library found. 
#  warning You will run into problems if you use
#  warning gphoto2 with a serial (RS232) camera in 
#  warning combination with Konqueror (KDE) or Nautilus (GNOME).
#  warning This will *not* concern USB cameras.
# endif
#endif

	return (GP_OK);
}

static int
gp_port_serial_unlock (GPPort *dev, const char *path)
{

#if defined(HAVE_TTYLOCK)
	if (ttyunlock ((char*) path)) {
		if (dev)
			gp_port_set_error (dev, _("Device '%s' could not be "
					   "unlocked."), path);
		return (GP_ERROR_IO_LOCK);
	}
#elif defined(HAVE_LOCKDEV)

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
	int x;
	struct stat s;
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
		char *xname;

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
		/* Very first of all, if the device node is not there,
		 * there is no need to try locking. */
		if ((stat (path, &s) == -1) && ((errno == ENOENT) || (errno == ENODEV)))
			continue;

		gp_port_info_new (&info);
		gp_port_info_set_type (info, GP_PORT_SERIAL);
		C_MEM (xname = malloc (strlen("serial:")+strlen(path)+1));
		strcpy (xname, "serial:");
		strcat (xname, path);
		gp_port_info_set_path (info, xname);
		free (xname);
		C_MEM (xname = malloc (100));
		snprintf (xname, 100, _("Serial Port %i"), x);
		gp_port_info_set_name (info, xname);
		free (xname);
		CHECK (gp_port_info_list_append (list, info));
	}

	/*
	 * Generic support. Append it to the list without checking for
	 * return values, because this entry will not be counted.
	 */
	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_SERIAL);
	gp_port_info_set_path (info, "serial:");
	gp_port_info_set_name (info, _("Serial Port Device"));
	CHECK (gp_port_info_list_append (list, info));

	gp_port_info_new (&info);
	gp_port_info_set_type (info, GP_PORT_SERIAL);
	gp_port_info_set_path (info, "^serial:");
	gp_port_info_set_name (info, "");
	gp_port_info_list_append (list, info); /* do not check */
	return GP_OK;
}

static int
gp_port_serial_init (GPPort *dev)
{
	C_PARAMS (dev);

	C_MEM (dev->pl = calloc (1, sizeof (GPPortPrivateLibrary)));

	/* There is no default speed. */
	dev->pl->baudrate = -1;

	return GP_OK;
}

static int
gp_port_serial_exit (GPPort *dev)
{
	C_PARAMS (dev);

	free (dev->pl);
	dev->pl = NULL;

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
	GPPortInfo	info;

	result = gp_port_get_info (dev, &info);
	if (result < GP_OK) return result;
	result = gp_port_info_get_path (info, &port);
	if (result < GP_OK) return result;
	GP_LOG_D ("opening %s", port);

	/* Ports are named "serial:/dev/whatever/port" */
	port = strchr (port, ':');
	if (!port)
		return GP_ERROR_UNKNOWN_PORT;
	port++;

	result = gp_port_serial_lock (dev, port);
	if (result != GP_OK) {
		for (i = 0; i < max_tries; i++) {
			result = gp_port_serial_lock (dev, port);
			if (result == GP_OK)
				break;
			GP_LOG_D ("Failed to get a lock, trying again...");
			sleep (1);
		}
		CHECK (result);
	}
	dev->pl->fd = -1;

#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__DragonFly__)
	dev->pl->fd = open (port, O_RDWR | O_NOCTTY | O_NONBLOCK);
#elif OS2
	fd = open (port, O_RDWR | O_BINARY);
	dev->pl->fd = open (port, O_RDWR | O_BINARY);
	close(fd);
#else
	if (dev->pl->fd == -1)
		dev->pl->fd = open (port, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
#endif
	if (dev->pl->fd == -1) {
		int saved_errno = errno;
		gp_port_set_error (dev, _("Failed to open '%s' (%s)."),
				   port, strerror(saved_errno));
		dev->pl->fd = 0;
		return GP_ERROR_IO;
	}

	return GP_OK;
}

static int
gp_port_serial_close (GPPort *dev)
{
	const char *path;

	if (!dev)
		return (GP_OK);

	if (dev->pl->fd) {
		if (close (dev->pl->fd) == -1) {
			int saved_errno = errno;
			gp_port_set_error (dev, _("Could not close "
						  "'%s' (%s)."),
					   dev->settings.serial.port,
					   strerror(saved_errno));
	                return GP_ERROR_IO;
	        }
		dev->pl->fd = 0;
	}

	/* Unlock the port */
	path = strchr (dev->settings.serial.port, ':');
	C_PARAMS (path);
	path++;
	CHECK (gp_port_serial_unlock (dev, path));

#if defined(__sgi)
	/*
	 * On IRIX, we need to set the baudrate each time after opening
	 * the port.
	 */
	dev->pl->baudrate = 0;
#endif

	return GP_OK;
}

static int
gp_port_serial_write (GPPort *dev, const char *bytes, int size)
{
	int len, ret;

	C_PARAMS (dev);

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
			int saved_errno = errno;
			switch (saved_errno) {
			case EAGAIN:
			case EINTR:
                                ret = 0;
                                break;
                        default:
				gp_port_set_error (dev, _("Could not write "
							  "to port (%s)"),
						   strerror(saved_errno));
                                return GP_ERROR_IO_WRITE;
                        }
		}
		len += ret;
        }

        /* wait till all bytes are really sent */
#ifndef OS2
#ifdef HAVE_TERMIOS_H
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

	C_PARAMS (dev);

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
                        return GP_ERROR_TIMEOUT;
		if (!FD_ISSET (dev->pl->fd, &readfs))
			return (GP_ERROR_TIMEOUT);

		if (dev->settings.serial.parity != GP_PORT_SERIAL_PARITY_OFF) {
		    unsigned char ffchar[1];
		    unsigned char nullchar[1];

		    ffchar[0] = 0xff;
		    nullchar[0] = 0;
		    now = read (dev->pl->fd, bytes, 1);
		    if (now < 0)
			    return GP_ERROR_IO_READ;
		    if (!memcmp(bytes,ffchar,1)) {
			now = read (dev->pl->fd, bytes, 1);
			if (now < 0)
				return GP_ERROR_IO_READ;

			/* Parity errors are signaled by the serial layer
			 * as 0xff 0x00 sequence.
			 *
			 * 0xff sent by the camera are escaped as
			 * 0xff 0xff sequence. 
			 *
			 * All other 0xff 0xXX sequences are errors.
			 *
			 * cf. man tcsetattr, description of PARMRK.
			 */
			if (!memcmp(bytes,nullchar,1)) {
			    gp_port_set_error (dev, _("Parity error."));
			    return GP_ERROR_IO_READ;
			}
			if (!memcmp(bytes,ffchar,1)) {
			    gp_port_set_error (dev, _("Unexpected parity response sequence 0xff 0x%02x."), ((unsigned char*)bytes)[0]);
			    return GP_ERROR_IO_READ;
			}
			/* Ok, we read 1 byte and it is 0xff */
			/* FALLTHROUGH */
		    }
		} else {
		    /* Just read the bytes */
		    now = read (dev->pl->fd, bytes, size - readen);
		    if (now < 0)
			    return GP_ERROR_IO_READ;
		}
		bytes += now;
		readen += now;
        }

        return readen;
}

#ifdef HAVE_TERMIOS_H
static int
get_termios_bit (GPPort *dev, GPPin pin, int *bit)
{
        switch (pin) {
        case GP_PIN_RTS:
                *bit = TIOCM_RTS;
                break;
        case GP_PIN_DTR:
                *bit = TIOCM_DTR;
                break;
        case GP_PIN_CTS:
                *bit = TIOCM_CTS;
                break;
        case GP_PIN_DSR:
                *bit = TIOCM_DSR;
                break;
        case GP_PIN_CD:
                *bit = TIOCM_CD;
                break;
        case GP_PIN_RING:
                *bit = TIOCM_RNG;
                break;
        default:
                gp_port_set_error (dev, _("Unknown pin %i."), pin);
                return GP_ERROR_IO;
        }
	return (GP_OK);
}
#endif

static int
gp_port_serial_get_pin (GPPort *dev, GPPin pin, GPLevel *level)
{
#ifdef HAVE_TERMIOS_H
	int j, bit;
#endif

	C_PARAMS (dev && level);

	*level = 0;

#ifdef HAVE_TERMIOS_H
	CHECK (get_termios_bit (dev, pin, &bit));
        if (ioctl (dev->pl->fd, TIOCMGET, &j) < 0) {
		int saved_errno = errno;
		gp_port_set_error (dev, _("Could not get level of pin %i "
					  "(%s)."),
				   pin, strerror(saved_errno));
                return GP_ERROR_IO;
        }
        *level = j & bit;
#else
# ifdef __GCC__
#  warning ACCESSING PINS IS NOT IMPLEMENTED FOR NON-TERMIOS SYSTEMS!
# endif
#endif

        return (GP_OK);
}

static int
gp_port_serial_set_pin (GPPort *dev, GPPin pin, GPLevel level)
{
#ifdef HAVE_TERMIOS_H
        int bit, request;
#endif

	C_PARAMS (dev);

#ifdef HAVE_TERMIOS_H
	CHECK (get_termios_bit (dev, pin, &bit));
        switch (level) {
	case GP_LEVEL_LOW:
		request = TIOCMBIS;
		break;
	default:
		request = TIOCMBIC;
		break;
        }
        if (ioctl (dev->pl->fd, request, &bit) < 0) {
		int saved_errno = errno;
		gp_port_set_error (dev, _("Could not set level of pin %i to "
					  "%i (%s)."),
				   pin, level, strerror(saved_errno));
		return GP_ERROR_IO;
	}
#else
# ifdef __GCC__
#  warning ACCESSING PINS IS NOT IMPLEMENTED FOR NON-TERMIOS SYSTEMS!
# endif
#endif

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

#ifdef HAVE_TERMIOS_H
	if (tcflush (dev->pl->fd, direction ? TCOFLUSH : TCIFLUSH) < 0) {
		int saved_errno = errno;
		gp_port_set_error (dev, _("Could not flush '%s' (%s)."),
				   dev->settings.serial.port,
				   strerror(saved_errno));
		return (GP_ERROR_IO);
	}
#else
# ifdef __GCC__
#  warning SERIAL FLUSH NOT IMPLEMENTED FOR NON TERMIOS SYSTEMS!
# endif
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
		GP_LOG_D ("Baudrate %d unknown - using as is", baudrate);
        }

        return ret;
#undef BAUDCASE
}

static int
gp_port_serial_check_speed (GPPort *dev)
{
	speed_t speed;
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

	GP_LOG_D ("Setting baudrate to %d...", dev->settings.serial.speed);
	speed = gp_port_serial_baudconv (dev->settings.serial.speed);

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
        tio.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL |
                         IXANY | IXON | IXOFF | INPCK | ISTRIP);
#ifdef IUCLC
        tio.c_iflag &= ~IUCLC;
#endif
        tio.c_iflag |= (BRKINT | IGNPAR);
        tio.c_oflag &= ~OPOST;
        tio.c_lflag &= ~(ICANON | ISIG | ECHO | ECHONL | ECHOE |
                         ECHOK | IEXTEN);
        tio.c_cflag &= ~(CRTSCTS | PARENB | PARODD);
        tio.c_cflag |= CLOCAL | CREAD;

        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;

	if (dev->settings.serial.parity != GP_PORT_SERIAL_PARITY_OFF) {
	    tio.c_iflag &= ~IGNPAR;
	    tio.c_iflag |= INPCK | PARMRK ;
	    tio.c_cflag |= PARENB;
	    if (dev->settings.serial.parity == GP_PORT_SERIAL_PARITY_ODD)
		tio.c_cflag |= PARODD;
	}

	/* Set the new speed. */
	cfsetispeed (&tio, speed);
	cfsetospeed (&tio, speed);
        if (tcsetattr (dev->pl->fd, TCSANOW, &tio) < 0) {
		GP_LOG_E ("Error on 'tcsetattr'.");
                return GP_ERROR_IO_SERIAL_SPEED;
        }

	/* Clear O_NONBLOCK. */
        if (fcntl (dev->pl->fd, F_SETFL, 0) < 0) {
		GP_LOG_E ("Error on 'fcntl'.");
                return GP_ERROR_IO_SERIAL_SPEED;
        }

	/*
	 * Verify if the speed change has been successful.
	 * This is needed because some Solaris systems just ignore unsupported
	 * speeds (like 115200) instead of complaining.
	 *
	 * Only perform the check if we really changed to some speed.
	 */
	if (speed != B0) {
		if (tcgetattr (dev->pl->fd, &tio)) {
			GP_LOG_E ("Error on 'tcgetattr'.");
			return (GP_ERROR_IO_SERIAL_SPEED);
		}
		if ((cfgetispeed (&tio) != speed) ||
		    (cfgetospeed (&tio) != speed)) {
			GP_LOG_E ("Cannot set baudrate to %d...",
				  dev->settings.serial.speed);
			return (GP_ERROR_NOT_SUPPORTED);
		}
	}

#else /* !HAVE_TERMIOS_H */
        if (ioctl (dev->pl->fd, TIOCGETP, &ttyb) < 0) {
                perror("ioctl(TIOCGETP)");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
        ttyb.sg_ispeed = dev->settings.serial.speed;
        ttyb.sg_ospeed = dev->settings.serial.speed;
        ttyb.sg_flags = 0;

        if (ioctl (dev->pl->fd, TIOCSETP, &ttyb) < 0) {
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

	CHECK (gp_port_serial_check_speed (dev));

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
#ifdef HAVE_TERMIOS_H
        tcsendbreak (dev->pl->fd, duration / 310);
        tcdrain (dev->pl->fd);
#else
# ifdef __GCC__
#  warning SEND BREAK NOT IMPLEMENTED FOR NON TERMIOS SYSTEMS!
# endif
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
