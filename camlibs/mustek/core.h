/*
 * Copyright 1999/2000 by Henning Zabel <henning@uni-paderborn.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef CAMLIBS_MUSTEK_CORE_H
#define CAMLIBS_MUSTEK_CORE_H

#include "io.h"
#include "mdc800_spec.h"
#include "image.h"

#define MDC800_FLASHLIGHT_REDEYE	1
#define MDC800_FLASHLIGHT_ON 		2
#define MDC800_FLASHLIGHT_OFF		4
#define MDC800_FLASHLIGHT_AUTO		0

struct _CameraPrivateLibrary {
	unsigned char system_flags[4];
	int  system_flags_valid;
	int  memory_source;
};

/* -------------------------------------------------------------------------- */

int mdc800_openCamera (Camera*);
int mdc800_closeCamera (Camera*);

int mdc800_changespeed (Camera*,int);
int mdc800_getSpeed (Camera*,int *);

/* - Camera must be open for these functions -------------------------------- */

int mdc800_setTarget (Camera *camera, int);


int mdc800_getThumbnail (Camera *cam, int nr, void **data,int *size);
int mdc800_getImage (Camera *cam,int nr, void **data, int *size);

/* ------- SystemStatus ---------------------------------------------------- */

int mdc800_getSystemStatus(Camera *);
int mdc800_isCFCardPresent(Camera *);
int mdc800_getMode(Camera*);
int mdc800_getFlashLightStatus(Camera*);
int mdc800_isLCDEnabled(Camera*);
int mdc800_isBatteryOk(Camera*);
int mdc800_isMenuOn(Camera*);
int mdc800_isAutoOffEnabled(Camera*);

int mdc800_getStorageSource(Camera*);

/* ------- Other Functions -------------------------------------------------- */

/*  Most of these Function depends on the Storage Source */

int mdc800_setDefaultStorageSource(Camera*);
int mdc800_setStorageSource (Camera *,int);
int mdc800_setMode (Camera *,int);
int mdc800_enableLCD (Camera*,int);
int mdc800_playbackImage (Camera*,int );
int mdc800_getRemainFreeImageCount (Camera*,int*,int* ,int*);
int mdc800_setFlashLight (Camera*,int );

char* mdc800_getFlashLightString (int);

int mdc800_getImageQuality (Camera*,unsigned char *retval);
int mdc800_setImageQuality (Camera*,int);

int mdc800_getWBandExposure (Camera*,int*, int*);
int mdc800_setExposure (Camera*,int);

int mdc800_setWB (Camera*,int);

int mdc800_setExposureMode (Camera*,int);
int mdc800_getExposureMode (Camera*,int *);

int mdc800_enableMenu (Camera *,int);

int mdc800_number_of_pictures (Camera *camera, int *nrofpics);

#endif /* !defined(CAMLIBS_MUSTEK_CORE_H) */
