#ifndef _MUSTEK_CORE_H
#define _MUSTEK_CORE_H

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
#endif
