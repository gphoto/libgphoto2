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

/*
 * gphoto driver for the Mustek MDC800 Digital Camera. The driver
 * supports rs232 and USB.
 */

#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#include "libgphoto2/i18n.h"

#include "core.h"
#include "print.h"


/*----------------------  Function for Communication ------------------------ */

/*
 * Send the Initial Command. If the device is rs232 this
 * function will probe for the baudrate.
 */
static int
mdc800_sendInitialCommand (Camera *camera, unsigned char* answer)
{
	int	baud_rates[3]={19200,57600,115200};
	int	rate,ret;
	unsigned char	command [8]={COMMAND_BEGIN,COMMAND_INIT_CONNECT,0,0,0,COMMAND_END,0,0};

	if (camera->port->type == GP_PORT_USB)
	    return mdc800_io_sendCommand_with_retry(camera->port,command,answer,8,1,1);
	for (rate=0;rate<3;rate++)
	{
	    GPPortSettings settings;

	    ret = gp_port_get_settings(camera->port,&settings);
	    if (ret != GP_OK) return ret;
	    settings.serial.speed = baud_rates[rate];
	    ret = gp_port_set_settings(camera->port, settings);
	    if (ret != GP_OK) return ret;
	    if (GP_OK==mdc800_io_sendCommand_with_retry(camera->port,command,answer,8,1,1)) {
		printCoreNote("RS232 Baudrate probed at %d.\n",baud_rates[rate]);
		return GP_OK;
	    }
	    printCoreError("Probing RS232 Baudrate with %d fails.\n",baud_rates[rate]);
	}
	printCoreError ("Probing failed completely.\n");
	return GP_ERROR_IO;
}

/*
 * Opens the Camera:
 *     (camera is already opened by gphoto2)
 * (1) send Initial command
 */
int mdc800_openCamera (Camera *camera)
{
	unsigned char answer [8];
	int  i,ret;

	if (camera->port->type == GP_PORT_USB) {
		printCoreNote ("Device Registered as USB.\n");
	} else {
		printCoreNote ("Device Registered as RS232. \n");
	}

	camera->pl = malloc(sizeof(struct _CameraPrivateLibrary));
	if (!camera->pl)
	    return GP_ERROR_NO_MEMORY;
	camera->pl->system_flags_valid = 0;
	camera->pl->memory_source = -1;

	/* Send initial command */
	ret = mdc800_sendInitialCommand(camera,answer);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_openCamera) can't send initial command.\n");
		return ret;
	}
	printCoreNote ("Firmware info (last 5 Bytes) : ");
	for (i=0; i<8; i++)
		printCoreNote ("%i ", answer[i]);
	printCoreNote ("\n");
	camera->pl->system_flags_valid=0;
	camera->pl->memory_source=-1;
	ret = mdc800_setDefaultStorageSource(camera);
	if (ret != GP_OK) {
		printCoreError ("(mdc800_openCamera) can't set Storage Source.\n");
		return ret;
	}
	return GP_OK;
}

int mdc800_closeCamera (Camera *camera)
{
    if (camera->pl) {
	free(camera->pl);
	camera->pl = NULL;
    }
    return GP_OK;
}


/*
 * Sets the camera speed to the defined value:
 * 0: 19200, 1:57600, 2: 115200
 */
int mdc800_changespeed (Camera *camera,int new)
{
	int baud_rate [3]={19200,57600,115200};
	GPPortSettings settings;
	int ret;
	unsigned int oldrate;

	printFnkCall ("(mdc800_changespeed) called.\n");
	if (camera->port->type != GP_PORT_SERIAL) /* USB ... */
	    return GP_OK;

	gp_port_get_settings(camera->port,&settings);

	if (settings.serial.speed == baud_rate[new]) /* nothing to do */
	    return GP_OK;

	for (oldrate=0;oldrate<sizeof(baud_rate)/sizeof(baud_rate[0]);oldrate++)
	    if (baud_rate[oldrate] == settings.serial.speed)
		break;

	/* We did not find it? Can't happen? */
	if (oldrate == sizeof(baud_rate)/sizeof(baud_rate[0]))
	    return GP_ERROR_IO;

	/* Setting communication speed */
	ret = mdc800_io_sendCommand(camera->port,COMMAND_CHANGE_RS232_BAUD_RATE,new,oldrate,0,0,0);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_changespeed) can't send first command.\n");
		return GP_ERROR_IO;
	}
	settings.serial.speed = baud_rate[new];
	ret = gp_port_set_settings(camera->port,settings);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_changespeed) Changing Baudrate fails.\n");
		return ret;
	}
	/* Second Command */
	ret = mdc800_io_sendCommand(camera->port,COMMAND_CHANGE_RS232_BAUD_RATE,new,new,0,0,0);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_changespeed) can't send second command.\n");
		return ret;
	}
	printCoreNote ("Set Baudrate to %d\n", baud_rate[new]);
	return GP_OK;
}

/*
 * Get the current Bautrate
 */
int mdc800_getSpeed (Camera *camera, int *speed)
{
	int rate, ret, baud_rate [3]={19200,57600,115200};
	GPPortSettings settings;

	if (camera->port->type != GP_PORT_SERIAL)
	    return GP_ERROR_IO;

	ret = gp_port_get_settings(camera->port,&settings);
	if (ret!=GP_OK) return ret;

	for (rate=0;rate<3;rate++)
	    if (settings.serial.speed == baud_rate[rate])
		break;
	if (rate == 3)
	    return GP_ERROR_IO;
	*speed = rate;
	return GP_OK;
}


/*
 * Sets Target
 * 1: image, 2:thumbnail, 3:video, 4:not setting
 */
int mdc800_setTarget (Camera *camera,int value)
{
	return mdc800_io_sendCommand(camera->port,COMMAND_SET_TARGET,value,0,0,0,0);
}


/*
 * Loads a thumbnail from the cam .
 */
int mdc800_getThumbnail (Camera *camera,int index, void **data, int *size)
{
	int ret;

	printFnkCall ("(mdc800_getThumbNail) called for %i . \n",index);

	*size = 4096;
	*data = malloc(4096);
	if (!*data)
	    return GP_ERROR_NO_MEMORY;
	ret = mdc800_io_sendCommand(camera->port,COMMAND_GET_THUMBNAIL,index/100,(index%100)/10,index%10,*data,4096);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_getThumbNail) can't get Thumbnail.\n");
		return ret;
	}
	mdc800_correctImageData(*data,1,0,camera->pl->memory_source == 1);
	return GP_OK;
}



/*
 * Load an Image from the cam ..
 */
int mdc800_getImage (Camera *camera, int nr, void **data, int *size)
{
	unsigned char buffer[3];
	int imagequality=-1;
	int imagesize=0;
	int ret;

	printFnkCall ("(mdc800_getImage) called for %i . \n",nr);

	ret = mdc800_setTarget (camera,1);
	if (GP_OK != ret) {
		printCoreError ("(mdc800_getImage) can't set Target. \n");
		return ret;
	}
	ret = mdc800_io_sendCommand(camera->port,COMMAND_GET_IMAGE_SIZE,nr/100,(nr%100)/10,nr%10,buffer,3);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_getImage) request for Imagesize of %i fails.\n",nr);
		return ret;
	}

	imagesize = buffer[0]*65536+buffer[1]*256+buffer[2];
	printCoreNote ("Imagesize of %i is %i ",nr,imagesize);
	switch (imagesize/1024)
	{
	case 48:
		imagequality=0;
		printCoreNote ("(Economic Quality 506x384)\n");
		break;
	case 128:
		imagequality=1;
		printCoreNote ("(Standard Quality 1012x768)\n");
		break;
	case 320:
		imagequality=2;
		printCoreNote ("(High Quality 1012x768)\n");
		break;
	case 4:
		printCoreNote ("(ThumbNail ? 112x96)\n");
		imagequality=-1;
		break;
	default:
		printCoreNote ("(not detected)\n");
		return 0;
	}
	*size = imagesize;
	*data = malloc(imagesize);
	ret = mdc800_io_sendCommand(camera->port,COMMAND_GET_IMAGE,nr/100,(nr%100)/10,nr%10,*data,imagesize);
	if (ret != GP_OK) {
		printCoreError ("(mdc800_getImage) request fails for Image %i.\n",nr);
		return 0;
	}
	mdc800_correctImageData(*data,imagequality == -1,imagequality,camera->pl->memory_source == 1);
	return GP_OK;
}



/* ------	SystemStatus of the Camera ------------------------------------------/ */


/* Load the Status from the camera if necessary */
int mdc800_getSystemStatus (Camera *camera)
{
	int ret = GP_OK;
	int tries = 3;
	if (camera->pl->system_flags_valid)
		return GP_OK;
	fprintf(stderr,"mdc800_getSystemStatus entered...\n");
	while (tries--) {
	    ret = mdc800_io_sendCommand(camera->port,COMMAND_GET_SYSTEM_STATUS,0,0,0,camera->pl->system_flags,4);
	    if (ret == GP_OK)
		break;
	}
	if (ret!=GP_OK)
	{
		printCoreError ("(mdc800_getSystemStatus) request fails.\n");
		return ret;
	}
	fprintf(stderr,"mdc800_getSystemStatus leaving.\n");
	camera->pl->system_flags_valid=1;
	return GP_OK;
}


int mdc800_isCFCardPresent(Camera *camera)
{
	mdc800_getSystemStatus(camera);
	if (camera->pl->system_flags_valid)
		return ((camera->pl->system_flags[0]&1) == 0);
	else
	{
		printCoreError ("(mdc800_isCFCardPresent) detection fails.\n");
		return 0;
	}
}


int mdc800_isBatteryOk(Camera *camera)
{
	mdc800_getSystemStatus(camera);
	return ((camera->pl->system_flags[0]&4) == 0)?1:0;
}


/*
 * Gets CamMode.
 * 0: Camera, 1: Playback, 2:VCam
 */
int mdc800_getMode(Camera *camera)
{
	mdc800_getSystemStatus(camera);
	if ((camera->pl->system_flags[1]&16) == 0)
		return ((camera->pl->system_flags[1]&32) == 0)?1:0;
	else
		return 2;
}


/*
 * Return status of Flashlight. The
 * answer is one of MDC800_FLASHLIGHT_ Mask.
 */
int mdc800_getFlashLightStatus(Camera *camera)
{
	mdc800_getSystemStatus(camera);
	return (camera->pl->system_flags[3]&7);
}


int mdc800_isLCDEnabled(Camera *camera)
{
	mdc800_getSystemStatus(camera);
	return ((camera->pl->system_flags[1]&4) == 4);
}


int mdc800_isMenuOn(Camera *camera)
{
	mdc800_getSystemStatus(camera);
	return ((camera->pl->system_flags[1]&1) == 1);
}

int mdc800_isAutoOffEnabled(Camera *camera)
{
	mdc800_getSystemStatus(camera);
	return ((camera->pl->system_flags[1]&8) == 8);
}

/* ----- Other fine functions------------------------------------------------- */


/*
 * Sets Source of Images :
 * 0: FlashCard, 1: Internal Memory
 */
int mdc800_setStorageSource (Camera *camera,int flag)
{
	int ret;
	if (flag == camera->pl->memory_source)
		return GP_OK;

	/* Check whether FlashCard is present */
	if ((flag == 0) && mdc800_isCFCardPresent (camera))
	{
		printCoreNote ("There's is no FlashCard in the Camera !\n");
		return GP_OK;
	}
	printFnkCall ("(mdc800_setStorageSource) called with flag=%i\n",flag);
	ret=mdc800_io_sendCommand(camera->port,COMMAND_SET_STORAGE_SOURCE,flag,0,0,0,0);
	if (ret != GP_OK)
	{
		if (flag)
		{
			printCoreError ("Can't set InternalMemory as Input!\n");
		}
		else
		{
			printCoreError ("Can't set FlashCard as Input!\n");
		}
		return ret;
	}

	printCoreNote ("Storage Source set to ");
	if (flag) {
		printCoreNote ("Internal Memory.\n");
	} else {
		printCoreNote ("Compact Flash Card.\n");
	}
	camera->pl->system_flags_valid=0;
	camera->pl->memory_source=flag;
	return GP_OK;
}


/*
 * Sets the default storage source.
 * Default means, that if there's a FlashCard, the Flashcard
 * is used, else the InternalMemory
 *
 * If camera->pl->memory_source is set ( after driver has been closed ),
 * this value will be used.
 */
int mdc800_setDefaultStorageSource (Camera *camera)
{
	int ret,source;

	if (camera->pl->memory_source != -1)
	{
		source=camera->pl->memory_source;
		camera->pl->memory_source=-1;
	}
	else
	{
		source=mdc800_isCFCardPresent (camera)?0:1;
	}
	ret=mdc800_setStorageSource (camera,source);
	if (ret!=GP_OK)
	{
		printCoreError ("(mdc800_setDefaultStorageSource) Setting Storage Source fails\n");
		return ret;
	}
	return GP_OK;
}


/*
 *	Returns what StorageSource is selected by the driver
 * 0: FlashCard, 1: Internal
 */
int mdc800_getStorageSource(Camera *camera)
{
	if (camera->pl->memory_source == -1)
		mdc800_setDefaultStorageSource(camera);
	return camera->pl->memory_source;
}



/*
 * Sets Camera to Camera- or PlaybackMode
 * m: 0 Camera, 1 Playback, 2:VCam (USB)
 */
int mdc800_setMode (Camera *camera,int m)
{
	int last=mdc800_getMode (camera);
	int ret;
/*
	if (mdc800_getMode () == m)
		return 1;
*/
	switch (m)
	{
		case 0:
			ret = mdc800_io_sendCommand(camera->port,COMMAND_SET_CAMERA_MODE,0,0,0,0,0);
			if (ret != GP_OK)
			{
				printCoreError ("(mdc800_setMode) setting Camera Mode fails\n");
				return ret;
			}
			if (last != m)
				printCoreNote ("Mode set to Camera Mode.\n");
			break;

		case 1:
			ret = mdc800_io_sendCommand(camera->port,COMMAND_SET_PLAYBACK_MODE,0,0,0,0,0);
			if (ret != GP_OK)
			{
				printCoreError ("(mdc800_setMode) setting Playback Mode fails\n");
				return ret;
			}
			if (last != m)
				printCoreNote ("Mode set to Payback Mode.\n");
			break;

	}
	camera->pl->system_flags_valid=0;
	return GP_OK;
}


/*
 * Sets up Flashlight. The waitForCommit waits a long
 * time, to give the camera enough time to load the
 * flashlight.
 */
int mdc800_setFlashLight (Camera* camera,int value)
{
	int command=0,redeye_flag=0,ret;

	if (mdc800_getFlashLightStatus (camera) == value)
		return GP_OK;

	redeye_flag=(value&MDC800_FLASHLIGHT_REDEYE) != 0;

	if ((value&MDC800_FLASHLIGHT_ON) != 0)
		command=COMMAND_SET_FLASHMODE_ON;
	else if ((value&MDC800_FLASHLIGHT_OFF) != 0)
	{
		command=COMMAND_SET_FLASHMODE_OFF;
		redeye_flag=0;
	}
	else
		command=COMMAND_SET_FLASHMODE_AUTO;

	camera->pl->system_flags_valid=0;
	ret = mdc800_io_sendCommand(camera->port,command,redeye_flag,0,0,0,0);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_setFlashLight) sending command fails.\n");
		return ret;
	}
	printCoreNote ("%s", mdc800_getFlashLightString(value));
	printCoreNote ("\n");
	return GP_OK;
}

/*
 * Gets a String with the Text of the Flashlight-Status
 * depending on value
 */
char* mdc800_getFlashLightString (int value)
{
	switch (value)
	{
		case ( MDC800_FLASHLIGHT_REDEYE | MDC800_FLASHLIGHT_AUTO ) :
			return _("FlashLight : Auto (RedEye Reduction)");
		case MDC800_FLASHLIGHT_AUTO :
			return _("FlashLight : Auto");
		case ( MDC800_FLASHLIGHT_REDEYE | MDC800_FLASHLIGHT_ON ) :
			return _("FlashLight : On (RedEye Reduction)");
		case MDC800_FLASHLIGHT_ON :
			return _("FlashLight : On");
		case MDC800_FLASHLIGHT_OFF :
			return _("FlashLight : Off");
	}
	return _("FlashLight : undefined");
}


/*
 * Enable/Disable the LCD
 */
int mdc800_enableLCD (Camera*camera,int enable)
{
	int ret,command;
	if (enable == mdc800_isLCDEnabled (camera))
		return GP_OK;

	if (enable)
		command=COMMAND_SET_LCD_ON;
	else
		command=COMMAND_SET_LCD_OFF;

	camera->pl->system_flags_valid=0;
	ret = mdc800_io_sendCommand (camera->port,command,0,0,0,0,0);
	if (ret!=GP_OK)
	{
		printCoreError ("(mdc800_enableLCD) can't enable/disable LCD\n");
		return ret;
	}
	if (enable)
	{
		printCoreNote ("LCD is enabled\n");
	}
	else
	{
		printCoreNote ("LCD is disabled\n");
	}
	return GP_OK;
}


/*
 * Shows the specified Image, the Camera has to
 * be in Playback Mode !
 */
int mdc800_playbackImage (Camera *camera,int nr )
{
	int ret;
	ret = mdc800_getMode (camera);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_showImage) camera must be in Playback Mode !");
		return ret;
	}

	ret = mdc800_io_sendCommand (camera->port,COMMAND_PLAYBACK_IMAGE,nr/100,(nr%100)/10,nr%10,0,0);
	if (ret!=GP_OK)
	{
		printCoreError ("(mdc800_showImage) can't playback Image %i \n",nr);
		return ret;
	}
	return GP_OK;
}

/*
 * With this function you can get information about, how many
 * pictures can be stored in the free memory of the camera.
 *
 * h: High Quality, s: Standard Quality, e: Economy Quality
 * If one of these Pointers are 0 the will be ignored.
 */
int mdc800_getRemainFreeImageCount (Camera *camera,int* h,int* s,int *e)
{
	unsigned char data [6];
	int ret;

	ret = mdc800_io_sendCommand (camera->port,COMMAND_GET_REMAIN_FREE_IMAGE_COUNT,0,0,0,data,6);
	if (ret != GP_OK)
	{
		printCoreError ("(mdc800_getRemainFreeImageCount) Error sending Command.\n");
		return ret;
	}

	if (h)
		*h=(int)((data[0]/16)*1000)+((data[0]%16)*100)+((data[1]/16)*10)+(data[1]%16);
	if (s)
		*s=(int)((data[2]/16)*1000)+((data[2]%16)*100)+((data[3]/16)*10)+(data[3]%16);
	if (e)
		*e=(int)((data[4]/16)*1000)+((data[4]%16)*100)+((data[5]/16)*10)+(data[5]%16);
	return GP_OK;
}


/*
 * Get Image Quallity
 * 0: Economic, 1:Standard, 2:High
 */
int mdc800_getImageQuality (Camera *camera, unsigned char *retval)
{
	int ret;
	ret = mdc800_io_sendCommand (camera->port,COMMAND_GET_IMAGE_QUALITY,0,0,0,retval,1);
	if (ret!=GP_OK)
	{
		printCoreError ("(mdc800_getImageQuality) fails.\n");
		return ret;
	}
	return GP_OK;
}


/*
 *	Set Image Quality, return GP_OK if ok.
 */
int mdc800_setImageQuality (Camera *camera,int v)
{
    return mdc800_io_sendCommand (camera->port,COMMAND_SET_IMAGE_QUALITY,v,0,0,0,0);
}



/*
 * Set the WhiteBalance value
 * 1:auto ,2:indoor, 4:indoor with flashlight, 8:outdoor
 */
int mdc800_setWB (Camera *camera, int v)
{
    return mdc800_io_sendCommand(camera->port,COMMAND_SET_WB,v,0,0,0,0);
}


/*
 * Return the Exposure settings and W.B.
 */
int mdc800_getWBandExposure (Camera *camera,int* exp, int* wb)
{
	unsigned char retval[2];
	int ret;

	/* What's that here is a real difference between USB and RS232 */
	int toggle= (camera->port->type == GP_PORT_USB);

	ret = mdc800_io_sendCommand(camera->port,COMMAND_GET_WB_AND_EXPOSURE,0,0,0,retval,2);
	if (ret == GP_OK)
	{
		(*exp)= retval[toggle]-2;
		(*wb)= retval[1-toggle];
		return 1;
	}
	printCoreError ("(mdc800_getWBandExposure) fails.\n");
	return 0;
}


/*
 * Sets the Exposure Value
 */
int mdc800_setExposure (Camera *camera,int v)
{
	return mdc800_io_sendCommand(camera->port,COMMAND_SET_EXPOSURE,v+2,0,0,0,0);
}

/*
 * Sets the Exposure Mode
 * 0: MTRX 1:CNTR
 */
int mdc800_setExposureMode (Camera *camera,int m)
{
	return mdc800_io_sendCommand (camera->port,COMMAND_SET_EXPOSURE_MODE,m,0,0,0,0);
}


/*
 * return the Exposure Mode or -1
 */
int mdc800_getExposureMode (Camera *camera,int *retval)
{
	unsigned char cretval;
	int ret;
	ret = mdc800_io_sendCommand (camera->port,COMMAND_GET_EXPOSURE_MODE,0,0,0,&cretval,1);
	if (ret == GP_OK)
	    *retval = cretval;
	return ret;
}

/*
 * Enable, Disable the Menu
 */
int mdc800_enableMenu (Camera *camera,int enable)
{
	char command=enable?COMMAND_SET_MENU_ON:COMMAND_SET_MENU_OFF;

	if (enable == mdc800_isMenuOn (camera))
		return GP_OK;
	camera->pl->system_flags_valid=0;
	return mdc800_io_sendCommand (camera->port,command,0,0,0,0,0);
}

int mdc800_number_of_pictures (Camera *camera, int *nrofpics)
{
	unsigned char answer [2];
	int ret;

	printFnkCall ("(mdc800_number_of_pictures) called.\n");

	ret= mdc800_setTarget (camera, 1);
	if (ret != GP_OK)
	{
		printAPIError ("(mdc800_number_of_pictures) can't set Target\n");
		return ret;
	}

/*
	if (!mdc800_setMode (1))
	{
		printError ("(mdc800_number_of_pictures) can't set Mode\n");
		mdc800_close ();
		return 0;
	}
*/

	ret = mdc800_io_sendCommand(camera->port,COMMAND_GET_NUM_IMAGES,0,0,0,answer,2);
	if (ret != GP_OK)
	{
		printAPIError ("(mdc800_getNumberOfImages) request Number of Pictures fails.\n");
		return ret;
	}
	*nrofpics=answer[0]*256+answer [1];
	return GP_OK;
}
