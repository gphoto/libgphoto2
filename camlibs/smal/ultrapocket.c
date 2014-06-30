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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <bayer.h>
#include <gamma.h>
#include <unistd.h>

#include "ultrapocket.h"
#include "smal.h"

#define GP_MODULE "Smal Ultrapocket"

#include <locale.h>
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

static const char *BayerTileNames[8] = {
    "RGGB",
    "GRBG",
    "BGGR",
    "GBRG",
    "RGGB_INTERLACED",
    "GRBG_INTERLACED",
    "BGGR_INTERLACED",
    "GBRG_INTERLACED"
};

static int
ultrapocket_command(GPPort *port, int iswrite, unsigned char *data, int datasize) {
    int ret;
    if (iswrite)
	ret = gp_port_write(port, (char *)data, datasize);
    else
        ret = gp_port_read(port, (char *)data, datasize);

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
	int ret = ultrapocket_command(port, 0, retdata, 0x1000);
	if (ret < GP_OK) {
	     free (rawdata);
	     gp_context_progress_stop(context, id);
	     return ret;
	}
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
	int ret = ultrapocket_command(port, 0, retdata, 0x8000);
	if (ret < GP_OK) {
    	    gp_context_progress_stop(context, id);
	    free (rawdata);
	    return ret;
	}
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
ultrapocket_getrawpicture(Camera *camera, GPContext *context, 
   unsigned char **pdata, int *size, const char *filename)
{
    char           ppmheader[200];
    unsigned char *rawdata,*outdata;
    int            width, height, result;
    size_t         outsize;
    int            imgstart = 0;
    int            pc, pmmhdr_len;
    BayerTile      tile;

    switch (camera->pl->up_type) {
     case BADGE_CARDCAM:
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
        return GP_ERROR;
    }

   tile = BAYER_TILE_BGGR;

   snprintf (ppmheader, sizeof(ppmheader), "P6\n"
	    "# CREATOR: gphoto2, ultrapocket library, raw,"
	    " assuming Bayer tile %s\n"
	    "%d %d\n"
	    "255\n", BayerTileNames[tile], width, height);

   /* Allocate memory for Interpolated ppm image */
   pmmhdr_len = strlen(ppmheader);
   outsize = ((long)width + 4) * height * 3 + pmmhdr_len;
   outdata = malloc(outsize);
   if (!outdata) {
     free(rawdata);
     return (GP_ERROR_NO_MEMORY);
   }

   /* Set header */
   strcpy((char *)outdata, ppmheader);

   /* Expand the Bayer tiles */
   result = gp_bayer_expand((rawdata+imgstart), width + 4, height,
			    &outdata[pmmhdr_len], tile);

   /* and chop the spare 4 pixels off the RHS */
   for (pc = 1; pc < height; pc++) {
      memmove(outdata + pmmhdr_len + ((long)width * pc * 3), 
              outdata + pmmhdr_len + (((long)width + 4) * pc * 3), 
              ((long)width) * 3);
   }
   /* modify outsize to reflect trim */
   outsize = ((long)width) * height * 3 + pmmhdr_len;
   
   free(rawdata);
   if (result < 0) {
      free (outdata);
      return (result);
   }

   *pdata = outdata;
   *size  = outsize;
   return GP_OK;
}

/* Different camera types (pocket digital/generic)
 * use a different protocol - have to differetiate.
 */
int
ultrapocket_getpicture(Camera *camera, GPContext *context, unsigned char **pdata, int *size, const char *filename)
{
    char	   *savelocale;
    char           ppmheader[200];
    unsigned char *rawdata,*outdata;
    int            width, height, result;
    size_t         outsize;
    int            imgstart = 0;
    int            pc, pmmhdr_len;
    BayerTile      tile;
#if DO_GAMMA
    unsigned char  gtable[256];
#endif

    switch (camera->pl->up_type) {
     case BADGE_CARDCAM:
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
	return GP_ERROR;
    }

   tile = BAYER_TILE_BGGR;

   savelocale = setlocale (LC_ALL, "C");
   snprintf (ppmheader, sizeof(ppmheader), "P6\n"
	    "# CREATOR: gphoto2, ultrapocket library,"
	    " assuming Bayer tile %s, interpolated"
#if DO_GAMMA
	    ", gamma %.2f"
#endif
	    "\n%d %d\n"
	    "255\n", BayerTileNames[tile], 
#if DO_GAMMA
	    GAMMA_NUMBER,
#endif
	    width, height);
   setlocale (LC_ALL, savelocale);

   /* Allocate memory for Interpolated ppm image */
   pmmhdr_len = strlen(ppmheader);
   outsize = ((long)width + 4) * height * 3 + pmmhdr_len;
   outdata = malloc(outsize);
   if (!outdata) {
     free (rawdata);
     return (GP_ERROR_NO_MEMORY);
   }

   /* Set header */
   strcpy((char *)outdata, ppmheader);

   /* Decode and interpolate the Bayer tiles */
   result = gp_bayer_decode((rawdata+imgstart), width+4, height,
			    &outdata[pmmhdr_len], tile);

   /* and chop the spare 4 pixels off the RHS */
   for (pc = 1; pc < height; pc++) {
      memmove(outdata + pmmhdr_len + ((long)width * pc * 3), 
              outdata + pmmhdr_len + (((long)width + 4) * pc * 3), 
              ((long)width) * 3);
   }
   /* modify outsize to reflect trim */
   outsize = ((long)width) * height * 3 + pmmhdr_len;

   free(rawdata);
   if (result < 0) {
      free (outdata);
      return (result);
   }

#if DO_GAMMA
   gp_gamma_fill_table(gtable, GAMMA_NUMBER);
   gp_gamma_correct_single(gtable, &outdata[pmmhdr_len], height * width);
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
ultrapocket_reset(Camera *camera)
{
   GPPortInfo oldpi;
   GPPort *port = camera->port;
   CameraAbilities cab;
   unsigned char cmdbuf[0x10];
   gp_camera_get_abilities(camera, &cab);
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
   sleep(4); /* This should do - _might_ need increasing */
   CHECK_RESULT(gp_port_get_info(port, &oldpi));
   CHECK_RESULT(gp_port_free(port));
   CHECK_RESULT(gp_port_new(&port));
   CHECK_RESULT(gp_port_set_info(port, oldpi));
   CHECK_RESULT(gp_port_usb_find_device(port, 
      cab.usb_vendor, cab.usb_product));
   CHECK_RESULT(gp_port_open(port));
   camera->port = port;
   return GP_OK;
}

static int
ultrapocket_skip(GPPort *port, int npackets)
{
   int old_timeout = 200;
   unsigned char retbuf[0x1000];

   gp_port_get_timeout(port, &old_timeout);
   gp_port_set_timeout(port, 100);
   for (; (npackets > 0) && gp_port_read(port, (char *)retbuf, 0x1000); npackets--);
   gp_port_set_timeout(port, old_timeout);
   return GP_OK;
}

static int
ultrapocket_sync(Camera *camera)
{
   GPPort *port = camera->port;
   unsigned char command[0x10];

   /* Who knows what's in all these packets? ... only SmAL! */

   if (camera->pl->up_type == BADGE_CARDCAM) {
#if 0
       memset(command, 0, 16);
       command[0] = 0x12;
       CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
       ultrapocket_skip(port, 8);
       /* -------------- */
#endif
       memset(command, 0, 16);
       command[0] = 0x31;
       command[1] = 0x01;
       CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
       ultrapocket_skip(port, 8);
       /* -------------- */
       memset(command, 0, 16);
       command[0] = 0x12;
       CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
       ultrapocket_skip(port, 8);
       /* -------------- */
       memset(command, 0, 16);
       command[0] = 0x31;
       command[1] = 0x01;
       CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
       ultrapocket_skip(port, 8);
   }
#if 0
       /* -------------- */
       memset(command, 0, 16);
       command[0] = 0x30;
       command[1] = 0x01;
       CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
       ultrapocket_skip(port, 16);
       /* -------------- */
       memset(command, 0, 16);
       command[0] = 0x29;
       command[1] = 0x01;
       CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
       ultrapocket_skip(port, 8);
       /* -------------- */
#endif
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
   int y;
   int np = 0;
   char fn[20];
   int picid;
   int reset_needed;
   
   CHECK_RESULT(ultrapocket_sync(camera));

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
   ultrapocket_skip(port, 7);

#if 0
   CHECK_RESULT(ultrapocket_sync(camera));
#endif
   if (reset_needed) {
      CHECK_RESULT(ultrapocket_reset(camera));
      port = *pport;
   }
#if 0
   CHECK_RESULT(ultrapocket_sync(camera));
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
 * Creative CardCam
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
     case BADGE_CARDCAM:
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

static int deletefile_generic(Camera *camera, const char *filename)
{
    unsigned char command[0x10] = { 0x22, 0x01, 0x00, 0x49, 0x4d, 0x47, 0,0,0,0, 0x2e, 0x52, 0x41, 0x57, 0x00, 0x00 };
    memcpy(command+6, filename+3, 4); /* the id of the image to delete */

    CHECK_RESULT(ultrapocket_command(camera->port, 1, command, 0x10));
    ultrapocket_skip(camera->port, 8);

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
     case BADGE_CARDCAM:
     case BADGE_FLATFOTO:
     case BADGE_GENERIC:
     case BADGE_ULTRAPOCKET:
     case BADGE_AXIA:
	return deletefile_generic(camera, filename);
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
int deleteall_generic(Camera *camera)
{
    unsigned char command[0x10];
    unsigned char retbuf[0x1000];
    int reset_needed = 0;
    GPPort *port = camera->port;

    memset(command, 0, 16);
    command[0] = 0x12;

    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));

    CHECK_RESULT(ultrapocket_command(port, 0, retbuf, 0x1000));
    reset_needed = (*(retbuf + 2) & UP_FLAG_NEEDS_RESET);
    ultrapocket_skip(camera->port, 7);

    if (reset_needed) {
	CHECK_RESULT(ultrapocket_reset(camera));
	port = camera->port;
    }

    memset(command, 0, 0x10);
    command[0] = 0x18;
    command[1] = 0x01;
    CHECK_RESULT(ultrapocket_command(port, 1, command, 0x10));
    ultrapocket_skip(camera->port, 8);

    return GP_OK;
}

int
ultrapocket_deleteall(Camera *camera)
{
    GPPort **pport = &camera->port;

    switch (camera->pl->up_type) {
     case BADGE_CARDCAM:
     case BADGE_FLATFOTO:
     case BADGE_GENERIC:
     case BADGE_ULTRAPOCKET:
     case BADGE_AXIA:
	return deleteall_generic(camera);
	break;
     case BADGE_LOGITECH_PD:
	return deleteall_logitech_pd(pport);
	break;
     default:
	break;
    }
    return GP_ERROR;
}
