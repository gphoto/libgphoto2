/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* camera.c
 *
 * Copyright (c) 2015 Marcus Meissner <marcus@jet.franken.de>
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
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <string.h>

#include "vcamera.h"

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

#define CHECK(result) {int r=(result); if (r<0) return (r);}

static int vcam_init(vcamera* cam) {
	return GP_OK;
}

static int vcam_exit(vcamera* cam) {
	return GP_OK;
}

static int vcam_open(vcamera* cam) {
	return GP_OK;
}

static int vcam_close(vcamera* cam) {
	return GP_OK;
}

static
vcam_process_input(vcamera *cam) {
}

static int vcam_read(vcamera*cam, int ep, char *data, int bytes) {
	int	toread = bytes;

	/* Read stuff to buffer */
	if (cam->nrinbulk > toread)
		toread = cam->nrinbulk;
	memcpy(data, cam->inbulk, toread);
	memmove(cam->inbulk + toread, cam->inbulk, (cam->nrinbulk - toread));
	cam->nrinbulk -= toread;
	return toread;
}

static int vcam_write(vcamera*cam, int ep, char *data, int bytes) {
	/* push stuff to buffer */
	if (!cam->outbulk) {
		cam->outbulk = malloc(bytes);
	} else {
		cam->outbulk = realloc(cam->outbulk,cam->nroutbulk + bytes);
	}
	memcpy(cam->outbulk + cam->nroutbulk, data, bytes);
	cam->nroutbulk += bytes;

	vcam_process_input(cam);

	return bytes;
}

vcamera*
vcamera_new(void) {
	vcamera *cam;

	cam = calloc(sizeof(vcamera),1);
	if (!cam) return NULL;

	cam->init = vcam_init;
	cam->exit = vcam_exit;
	cam->open = vcam_open;
	cam->close = vcam_close;

	cam->read = vcam_read;
	cam->write = vcam_write;

	return cam;
}
