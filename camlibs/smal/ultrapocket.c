/* ultrapocket.c
 *
 * Copyright (C) 2003 Lee Benfield <lee@benf.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2-port-log.h>
#include <gphoto2-library.h>
#include <gphoto2-result.h>
#include <bayer.h>
#include <gamma.h>

#include "ultrapocket.h"
#include "smal.h"

#define GP_MODULE "Smal Ultrapocket"

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
#  define _(String) (String)
#  define N_(String) (String)
#endif

static int
ultrapocket_command(GPPort *port, int iswrite, unsigned char *data, int datasize) {
    int ret;
    if (iswrite)
	ret = gp_port_write(port,data,datasize);
    else
	ret = gp_port_read(port,data,datasize);

    if (ret>=GP_OK)
	ret=GP_OK;
    return ret;
}

int
ultrapocket_exit(GPPort *port, GPContext *context) {
    return GP_OK;
}

/* A generic slimshot-alike camera */
/* we have to determine from the header what the RAW image is, QVGA or VGA */
static int
getpicture_generic(Camera *camera, GPContext *context, unsigned char **rd,int *retwidth, int *retheight, int *retimgstart, const char *filename) {
    GPPort *port = camera->port;
    unsigned char  command[0x10] = { 0x11, 0x01, 0x00, 0x49, 0x4d, 0x47, 0,0,0,0, 0x2e, 0x52, 0x41, 0x57, 0x00, 0x00 };
    unsigned char  retdata[0x1000];
    unsigned char  header[42];
    unsigned char *rawdata;
    int            width, height, imgstart;
    smal_img_type  styp;
    int            ptc,pc,id;
#if DO_GAMMA
    unsigned char  gtable[256];
#endif

    memcpy(command+6, filename+3, 4); /* the id of the image to transfer */

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    /* the first packet has the header in it, plus the start of the data */
    CHECK_RESULT(ultrapocket_command(port, 0, retdata, 0x1000));
    memcpy(header, retdata, 41);
    styp = header[3];

    switch (styp) {
     case TYPE_QVGA:
	/* 24 packets, including one already got */
	ptc = 24;
	width = 320;
	height = 240;
	imgstart = 0x29;
	break;
     case TYPE_VGA:
	/* 80 including one already got. */
	ptc = 80;
	width = 640;
	height = 480;
	imgstart = 0x29;
	break;
     case TYPE_QVGA_BH:
	ptc = 24;
	width = 320;
	height = 240;
	imgstart = 0x100;
	break;
     case TYPE_VGA_BH:
	ptc = 80;
	width = 640;
	height = 480;
	imgstart = 0x100;
	break;
     default:
	return GP_ERROR;
   }
    rawdata = malloc(0x1000 * ptc * sizeof(char));
    if (!rawdata) return (GP_ERROR_NO_MEMORY);

    /* show how far we've got on the current image */
    id = gp_context_progress_start(context, ptc-1, _("Downloading image..."));

    memcpy(rawdata, retdata, 0x1000 * sizeof(char));
    for (pc=1;pc<ptc;pc++) {
	CHECK_RESULT(ultrapocket_command(port, 0, retdata, 0x1000));
	gp_context_progress_update(context, id, pc);
	memcpy(rawdata + (pc * 0x1000), retdata, 0x1000 * sizeof(char));
    }
    gp_context_progress_stop(context, id);

    *retwidth = width;
    *retheight = height;
    *retimgstart = imgstart;

    *rd = rawdata;
    return GP_OK;
}

/*
 * Getting picture off a logitech pocket digital
 */
static int
getpicture_logitech_pd(Camera *camera, GPContext *context, unsigned char **rd, const char *filename) {
    GPPort *port = camera->port;
    unsigned char  command[0x10] = { 0x11, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    unsigned char  retdata[0x8000];
    unsigned char *rawdata;
    int            ptc,pc,id;
#if DO_GAMMA
    unsigned char  gtable[256];
#endif

    memcpy(command+3, filename, 11); /* the id of the image to transfer */

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    /* the first packet has the header in it, plus the start of the data */
    CHECK_RESULT(ultrapocket_command(port, 0, retdata, 0x8000));

    /* 640 x 480 =ish 10 * 0x8000 */
    ptc = 10;

    rawdata = malloc(0x8000 * ptc * sizeof(char));
    if (!rawdata) return (GP_ERROR_NO_MEMORY);

    /* show how far we've got on the current image */
    id = gp_context_progress_start(context, ptc-1, _("Downloading image..."));

    memcpy(rawdata, retdata, 0x8000 * sizeof(char));
    for (pc=1;pc<ptc;pc++) {
	CHECK_RESULT(ultrapocket_command(port, 0, retdata, 0x8000));
	gp_context_progress_update(context, id, pc);
	memcpy(rawdata + (pc * 0x8000), retdata, 0x8000 * sizeof(char));
    }
    gp_context_progress_stop(context, id);

    *rd = rawdata;
    return GP_OK;
}

/* Different camera types (pocket digital/generic)
 * use a different protocol - have to differetiate.
 */
int
ultrapocket_getpicture(Camera *camera, GPContext *context, unsigned char **pdata, int *size, const char *filename)
{
    char           ppmheader[100];
    unsigned char *rawdata,*outdata;
    int            width, height, outsize, result;
    int            imgstart = 0;
    int            pc,pmmhdr_len;
#if DO_GAMMA
    unsigned char  gtable[256];
#endif

    switch (camera->pl->up_type) {
     case BADGE_FLATFOTO:
     case BADGE_GENERIC:
     case BADGE_ULTRAPOCKET:
     case BADGE_AXIA:
	CHECK_RESULT(getpicture_generic(camera, context, &rawdata, &width, &height, &imgstart, filename));
	break;
     case BADGE_LOGITECH_PD:
	CHECK_RESULT(getpicture_logitech_pd(camera, context, &rawdata, filename));
	width = 640;
	height = 480;
	imgstart = 0x29;
	break;
     default:
	break;
    }

   sprintf (ppmheader, "P6\n"
	    "# CREATOR: gphoto2, ultrapocket library\n"
	    "%d %d\n"
	    "255\n", width, height);

   /* Allocate memory for Interpolated ppm image */
   pmmhdr_len = strlen(ppmheader);
   outsize = (width+4) * height * 3 + pmmhdr_len + 1;
   outdata = malloc(outsize);
   if (!outdata)
     return (GP_ERROR_NO_MEMORY);

   /* Set header */
   strcpy(outdata, ppmheader);

   /* Decode and interpolate the Bayer Mask */
   result = gp_bayer_decode((rawdata+imgstart), width+4, height,
			    &outdata[pmmhdr_len], 2 );

   /* and chop the spare 4 pixels off the RHS */
   for (pc=1;pc<height;pc++) {
      memmove(outdata + pmmhdr_len + (width * 3 * pc), outdata + pmmhdr_len + ((width+4) * 3 * pc), width*3);
   }
   /* modify outsize to refect trim */
   outsize = width * height * 3 + pmmhdr_len + 1;

   free(rawdata);
   if (result < 0) {
      free (outdata);
      return (result);
   }

#if DO_GAMMA
   gp_gamma_fill_table( gtable, 0.5 );
   gp_gamma_correct_single( gtable, &outdata[pmmhdr_len], height * width );
#endif

   *pdata = outdata;
   *size  = outsize;
   return GP_OK;
}

/*
 * Only need to reset the generic camera type, AFAICT -
 * the pocket digital never seems to need it.
 */
static int
ultrapocket_reset(GPPort **pport)
{
   GPPortInfo oldpi;
   GPPort *port = *pport;
   unsigned char cmdbuf[0x10];
   GP_DEBUG ("First connect since camera was used - need to reset cam");

   /*
    * this resets the ultrapocket.  Messy, but it's what the windows
    * software does.   We only reset if it's been plugged in since
    * last reset.
    */
   memset(cmdbuf, 0, 16);
   cmdbuf[0] = 0x28;
   cmdbuf[1] = 0x01;
   CHECK_RESULT(ultrapocket_command(port, 1, cmdbuf, 0x10));
   /* -------------- */
   sleep(2); /* This should do - _might_ need increasing */
   CHECK_RESULT(gp_port_get_info(port, &oldpi));
   CHECK_RESULT(gp_port_free(port));
   CHECK_RESULT(gp_port_new(&port));
   CHECK_RESULT(gp_port_set_info(port, oldpi));
   CHECK_RESULT(gp_port_usb_find_device (port, USB_VENDOR_ID_SMAL, USB_DEVICE_ID_ULTRAPOCKET));
   CHECK_RESULT(gp_port_open(port));
   *pport = port;
   return GP_OK;
}

/*
 * For most ultrapocket compatibles, call this.
 */
static int getpicsoverview_generic(
    Camera *camera, GPContext *context,
    int *numpics, CameraList *list
				   ) {
   GPPort **pport = &camera->port;
   GPPort *port = *pport;
   unsigned char command[0x10];
   unsigned char retbuf[0x1000];
   int x,y;
   int np = 0;
    char fn[20];
   int picid;
   int reset_needed;

   memset(command, 0, 16);
   command[0] = 0x12;

   CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

   CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));
   np = *(unsigned char*)(retbuf + 0x104);
   for (y=0;y<np;y++) {
      picid = retbuf[0x106+y*2] + (retbuf[0x107+y*2] << 8);
      sprintf(fn, "IMG%4.4d.PPM",picid);
      gp_list_append(list, fn, NULL);
   }
   reset_needed = (*(retbuf + 2) & UP_FLAG_NEEDS_RESET);
   for (x=0;x<7;x++) CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));

    /* Who knows what's in all these packets? ... only SmAL! */
    /* The windows driver gets all of this guff - since the camera functions just fine
     * without them, I'm going to ignore it for now */
#if COPY_WINDOWS_DRIVER
    /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x31;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x12;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x31;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x30;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<16;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x29;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
#endif

    if (reset_needed) {
	CHECK_RESULT(ultrapocket_reset(pport));
	port = *pport;
    }

#if COPY_WINDOWS_DRIVER
    memset(command, 0, 16);
    command[0] = 0x12;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x31;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x12;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
   /* -------------- */
    memset(command, 0, 16);
    command[0] = 0x31;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    for (x=0;x<8;x++)  { CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));}
    /* -------------- */
#endif

    *numpics = np;

    return GP_OK;
}

static int getpicsoverview_logitech_pd(
				       Camera *camera, GPContext *context,
				       int *numpics, CameraList *list
				       ) {
    GPPort *port = camera->port;
    unsigned char command[0x10];
    unsigned char retbuf[0x8000];
    int y;
    int np = 0;
    char fn[20];

    memset(command, 0, 16);
    command[0] = 0x12;

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x8000));
    np = *(unsigned char*)(retbuf + 0x105);
    for (y=0;y<np;y++) {
	memset(fn, 0, 20);
	memcpy(fn, retbuf+0x106+(y*0x10),11);
	/* bizzarely, the logitech camera returns a fname with a space
	 * in it, but requires a '.'. Go figure, eh?
	 */
	fn[7] = '.';
	gp_list_append(list, fn, NULL);
    }
    CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x8000));

    *numpics = np;

    return GP_OK;
}

/*
 * First, we send a request of the form
 * 12 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
 *
 * Fuji Slimshot + Axia etc
 * The camera responds with 8 * 0x1000 bytes
 *
 * Logitech Digital Pocket
 * The camera responds with 2 * 0x8000 bytes
 *
 */
int
ultrapocket_getpicsoverview(
    Camera *camera, GPContext *context,
    int *numpics, CameraList *list
) {
    switch (camera->pl->up_type) {
     case BADGE_FLATFOTO:
     case BADGE_GENERIC:
     case BADGE_ULTRAPOCKET:
     case BADGE_AXIA:
	return getpicsoverview_generic(camera, context, numpics, list);
	break;
     case BADGE_LOGITECH_PD:
	return getpicsoverview_logitech_pd(camera, context, numpics, list);
	break;
     default:
	break;
    }
    return GP_ERROR;
}

static int deletefile_generic(GPPort *port, const char *filename)
{
    unsigned char command[0x10] = { 0x22, 0x01, 0x00, 0x49, 0x4d, 0x47, 0,0,0,0, 0x2e, 0x52, 0x41, 0x57, 0x00, 0x00 };
    memcpy(command+6, filename+3, 4); /* the id of the image to delete */

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    return GP_OK;
}

static int deletefile_logitech_pd(GPPort *port, const char *filename)
{
    unsigned char  command[0x10] = { 0x11, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(command+3, filename, 11); /* the id of the image to delete */

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    return GP_OK;
}

/* The windows software doesn't allow one to delete a file without deleting previous
 * ones.
 *
 * Careful!
 *
 * This appears to be the same for Logitech pocket digital and the generic
 * camera.
 */
int
ultrapocket_deletefile(Camera *camera, const char *filename)
{
    GPPort *port = camera->port;

    switch (camera->pl->up_type) {
     case BADGE_FLATFOTO:
     case BADGE_GENERIC:
     case BADGE_ULTRAPOCKET:
     case BADGE_AXIA:
	return deletefile_generic(port, filename);
	break;
     case BADGE_LOGITECH_PD:
	return deletefile_logitech_pd(port, filename);
	break;
     default:
	break;
    }
    return GP_ERROR;

}

static
int deleteall_logitech_pd(GPPort **pport)
{
    unsigned char command[0x10];
    unsigned char retbuf[0x8000];
    GPPort *port = *pport;

    memset(command, 0, 16);
    command[0] = 0x12;

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x8000));
    CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x8000));

    /* as far as I can tell, the logitech pocket digital doesn't need a
     * reset - if it did, it would go here. */
    memset(command, 0, 0x10);
    command[0] = 0x18;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    return GP_OK;
}

static
int deleteall_generic(GPPort **pport)
{
    unsigned char command[0x10];
    unsigned char retbuf[0x1000];
    int x,reset_needed = 0;
    GPPort *port = *pport;

    memset(command, 0, 16);
    command[0] = 0x12;

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));
    reset_needed = (*(retbuf + 2) & UP_FLAG_NEEDS_RESET);
    for (x=0;x<7;x++) CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));

    if (reset_needed) {
	CHECK_RESULT(ultrapocket_reset(pport));
	port = *pport;
    }

    memset(command, 0, 0x10);
    command[0] = 0x18;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    return GP_OK;
}

int
ultrapocket_deleteall(Camera *camera)
{
    GPPort **pport = &camera->port;

    switch (camera->pl->up_type) {
     case BADGE_FLATFOTO:
     case BADGE_GENERIC:
     case BADGE_ULTRAPOCKET:
     case BADGE_AXIA:
	return deleteall_generic(pport);
	break;
     case BADGE_LOGITECH_PD:
	return deleteall_logitech_pd(pport);
	break;
     default:
	break;
    }
    return GP_ERROR;
}

