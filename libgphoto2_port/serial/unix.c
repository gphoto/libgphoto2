/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gphoto2-port-serial.c - Serial IO functions

   Modifications:
   Copyright (C) 2000 Philippe Marzouk <pmarzouk@bigfoot.com>
   Copyright (C) 2000 Edouard Lafargue <Edouard.Lafargue@bigfoot.com>
   Copyright (C) 1999 Johannes Erdfelt <johannes@erdfelt.com>
   Copyright (C) 1999 Scott Fritzinger <scottf@unr.edu>

   Based on work by:
   Copyright (C) 1999 Beat Christen <spiff@longstreet.ch>
   for the toshiba gPhoto library.

   The GPIO Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GPIO Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GPIO Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#if HAVE_TERMIOS_H
#include <termios.h>
#define CRTSCTS  020000000000
#else
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <sgtty.h>
#endif

#include "../include/gphoto2-port-serial.h"
#include "../include/gphoto2-port.h"

#ifdef HAVE_TERMIOS_H
static struct termios term_old;
#else
static struct sgttyb term_old;
#endif

/* Serial prototypes
   ------------------------------------------------------------------ */
int             gp_port_serial_init(gp_port *dev);
int             gp_port_serial_exit(gp_port *dev);

int             gp_port_serial_open(gp_port *dev);
int             gp_port_serial_close(gp_port *dev);

int             gp_port_serial_read(gp_port *dev, char *bytes, int size);
int             gp_port_serial_write(gp_port *dev, char *bytes, int size);

int             gp_port_serial_update (gp_port *dev);

/* Specific */
int             gp_port_serial_get_pin(gp_port *dev, int pin, int *level);
int             gp_port_serial_set_pin(gp_port *dev, int pin, int level);
int             gp_port_serial_send_break (gp_port *dev, int duration);
int             gp_port_serial_flush(gp_port *dev, int direction);


/* private */
int             gp_port_serial_set_baudrate(gp_port *dev);
static speed_t  gp_port_serial_baudconv(int rate);

/* Dynamic library functions
   --------------------------------------------------------------------- */

gp_port_type gp_port_library_type () {

        return (GP_PORT_SERIAL);
}

gp_port_operations *gp_port_library_operations () {

        gp_port_operations *ops;

        ops = (gp_port_operations*)malloc(sizeof(gp_port_operations));
        memset(ops, 0, sizeof(gp_port_operations));

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

int gp_port_library_list (gp_port_info *list, int *count) {


        char buf[1024], prefix[1024];
        int x, fd;
#ifdef __linux
        /* devfs */
        struct stat s;
#endif
#ifdef OS2
        int rc,fh,option;
#endif

        /* Copy in the serial port prefix */
        strcpy(prefix, GP_PORT_SERIAL_PREFIX);
#ifdef __linux
        /* devfs */
        if (stat("/dev/tts", &s)==0)
            strcpy(prefix, "/dev/tts/%i");

#endif
        for (x=GP_PORT_SERIAL_RANGE_LOW; x<=GP_PORT_SERIAL_RANGE_HIGH; x++) {
            sprintf(buf, prefix, x);
#ifdef OS2
           rc = DosOpen(buf,&fh,&option,0,0,1,OPEN_FLAGS_FAIL_ON_ERROR|OPEN_SHARE_DENYREADWRITE,0);
           DosClose(fh);
           if(rc==0) {
#endif
                fd = open (buf, O_RDONLY | O_NDELAY);
                if (fd != -1) {
                        close(fd);
                        list[*count].type = GP_PORT_SERIAL;
                        strcpy(list[*count].path, "serial:");
                        strcat(list[*count].path, buf);
                        sprintf(buf, "Serial Port %i", x);
                        strcpy(list[*count].name, buf);
                        /* list[*count].argument_needed = 0; */
                        *count += 1;
                }
#ifdef OS2
           }
#endif
        }

        return (GP_OK);
}

/* Serial API functions
   ------------------------------------------------------------------ */

int gp_port_serial_init (gp_port *dev) {
        /* save previous setttings in to dev->settings_saved */
#if HAVE_TERMIOS_H
        if (tcgetattr(dev->device_fd, &term_old) < 0) {
                perror("tcgetattr2");
                /*return GP_ERROR_IO_INIT;*/
        }
#else
        if (ioctl(dev->device_fd, TIOCGETP, &term_old) < 0) {
                perror("ioctl(TIOCGETP)");
                return GP_ERROR_IO_INIT;
        }
#endif
        return GP_OK;
}

int gp_port_serial_exit (gp_port *dev) {
        /* ... */
        return GP_OK;
}

int gp_port_serial_open(gp_port * dev)
{
    char *port = strchr(dev->settings.serial.port, ':');
    if (!port)
        return GP_ERROR_IO_UNKNOWN_PORT;
    port++;
#ifdef __FreeBSD__
        dev->device_fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
#elif OS2
        int fd;
        fd = open(port, O_RDWR | O_BINARY);
        dev->device_fd = open(port, O_RDWR | O_BINARY);
        close(fd);
        //printf("fd %d for %s\n",dev->device_fd,dev->settings.serial.port);
#else
        dev->device_fd = open(port, O_RDWR | O_NOCTTY | O_SYNC | O_NONBLOCK);
#endif
        if (dev->device_fd == -1) {
                fprintf(stderr, "gp_port_serial_open: failed to open ");
                perror(port);
                return GP_ERROR_IO_OPEN;
        }

/*      if (ioctl (dev->device_fd, TIOCMBIC, &RTS) <0) {
                perror("ioctl(TIOCMBIC)");
                return GP_ERROR;
        } */
        return GP_OK;
}

int gp_port_serial_close(gp_port * dev)
{
        if (close(dev->device_fd) == -1) {
                perror("gp_port_serial_close: tried closing device file descriptor");
                return GP_ERROR_IO_CLOSE;
        }
        return GP_OK;
}

int gp_port_serial_write(gp_port * dev, char *bytes, int size)
{
        int len, ret;

        len = 0;
        while (len < size) {    /* Make sure we write all data while handling */
                /* the harmless errors */
                if ((ret = write(dev->device_fd, bytes, size - len)) == -1)
                        switch (errno) {
                        case EAGAIN:
                        case EINTR:
                                ret = 0;
                                break;
                        default:
                                perror("gp_port_serial_write");
                                return GP_ERROR_IO_WRITE;
                        }
                len += ret;
        }

        /* wait till all bytes are really sent */
#ifndef OS2
#if HAVE_TERMIOS_H
        tcdrain(dev->device_fd);
#else
        ioctl(dev->device_fd, TCDRAIN, 0);
#endif
#endif
        return GP_OK;
}


int gp_port_serial_read(gp_port * dev, char *bytes, int size)
{
        struct timeval timeout;
        fd_set readfs;          /* file descriptor set */
        int readen = 0;
        int rc;

        FD_ZERO(&readfs);
        FD_SET(dev->device_fd, &readfs);

        while (readen < size) {
                /* set timeout value within input loop */
                timeout.tv_usec = (dev->timeout % 1000) * 1000;
                timeout.tv_sec = (dev->timeout / 1000);  /* = 0
                                                          * if dev->timeout < 1000
                                                          */


                rc = select(dev->device_fd + 1, &readfs, NULL, NULL, &timeout);
/*              if ( (rc == 0) && (readen == 0)) { */
                /* Timeout before reading anything */
/*                printf("gp_port_serial_read (timeout)\n"); */
/*                return GP_ERROR_TIMEOUT; */
/*              } */
                if (0 == rc) {
                        return GP_ERROR_IO_TIMEOUT;
                }
                if (FD_ISSET(dev->device_fd, &readfs)) {
                        int now = read(dev->device_fd, bytes, size - readen);

                        if (now < 0) {
                                perror("gp_port_serial_read (read fails)");
                                return GP_ERROR_IO_READ;
                        } else {
                                bytes += now;
                                readen += now;
                        }
                } else {
                        perror("gp_port_serial_read (tty timeout)");
                        return GP_ERROR_IO_TIMEOUT;
                }
        }
        return readen;
}

/*
 * Get the status of the lines of the serial port
 *
 */
int gp_port_serial_get_pin(gp_port * dev, int pin, int *level)
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

        if (ioctl(dev->device_fd, TIOCMGET, &j) < 0) {
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
int gp_port_serial_set_pin(gp_port * dev, int pin, int level)
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

        if (ioctl (dev->device_fd, request, &bit) <0) {
            perror("ioctl(TIOCMBI[CS])");
            return GP_ERROR_IO_PIN;
        }

        return GP_OK;
}

int gp_port_serial_flush (gp_port * dev, int direction)
{

#if HAVE_TERMIOS_H
    int q;

    switch (direction) {
    case 0:
        q = TCIFLUSH;
        break;
    case 1:
        q = TCOFLUSH;
        break;
    default:
        return GP_ERROR_IO_SERIAL_FLUSH;
    }

    if (tcflush (dev->device_fd, q) < 0)
        return (GP_ERROR_IO_SERIAL_FLUSH);

    return GP_OK;
#else
#warning SERIAL FLUSH NOT IMPLEMENTED FOR NON TERMIOS SYSTEMS!
    return GP_ERROR_IO_SERIAL_FLUSH;
#endif
}


/*
 * This function will apply the settings to
 * the device. The device has to be opened
 */
int gp_port_serial_update(gp_port * dev)
{
        memcpy(&dev->settings, &dev->settings_pending, sizeof(dev->settings));

        if (dev->device_fd != 0) {
                if (gp_port_serial_close(dev))
                        return GP_ERROR_IO_CLOSE;
                if (gp_port_serial_open(dev))
                        return GP_ERROR_IO_OPEN;

                return gp_port_serial_set_baudrate(dev);
        }
        return GP_OK;
}

/*
   Serial port specific helper functions
   ----------------------------------------------------------------
 */

/* Called to set the baud rate */
int gp_port_serial_set_baudrate(gp_port * dev)
{
#if HAVE_TERMIOS_H
        struct termios tio;

        if (tcgetattr(dev->device_fd, &tio) < 0) {
                perror("tcgetattr1");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
        tio.c_cflag = (tio.c_cflag & ~CSIZE) | CS8;

        /* Set into raw, no echo mode */
#if defined(__FreeBSD__) || defined(__NetBSD__)
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

        cfsetispeed(&tio, gp_port_serial_baudconv(dev->settings.serial.speed));
        cfsetospeed(&tio, gp_port_serial_baudconv(dev->settings.serial.speed));

        if (tcsetattr(dev->device_fd, TCSANOW, &tio) < 0) {
                perror("tcsetattr");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
        if (fcntl(dev->device_fd, F_SETFL, 0) < 0) {    /* clear O_NONBLOCK */
                perror("fcntl F_SETFL");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
#else
        struct sgttyb ttyb;

        if (ioctl(dev->device_fd, TIOCGETP, &ttyb) < 0) {
                perror("ioctl(TIOCGETP)");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
        ttyb.sg_ispeed = dev->settings.serial.speed;
        ttyb.sg_ospeed = dev->settings.serial.speed;
        ttyb.sg_flags = 0;

        if (ioctl(dev->device_fd, TIOCSETP, &ttyb) < 0) {
                perror("ioctl(TIOCSETP)");
                return GP_ERROR_IO_SERIAL_SPEED;
        }
#endif

        return GP_OK;
}

/* Called to convert a int baud to the POSIX enum value */
static speed_t gp_port_serial_baudconv(int baud)
{
#define BAUDCASE(x)     case (x): { ret = B##x; break; }
        speed_t ret;

        ret = (speed_t) baud;
        switch (baud) {
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
                fprintf(stderr, "baudconv: baudrate %d is undefined; using as is\n", baud);
        }

        return ret;
#undef BAUDCASE
}

int gp_port_serial_send_break (gp_port *dev, int duration) {

        /* Duration is in seconds */

#if HAVE_TERMIOS_H
        tcsendbreak(dev->device_fd, duration / 3);
        tcdrain(dev->device_fd);
#else
        /* ioctl */
#warning SEND BREAK NOT IMPLEMENTED FOR NON TERMIOS SYSTEMS!
        return GP_ERROR_IO_SERIAL_BREAK;
#endif
        return GP_OK;
}
