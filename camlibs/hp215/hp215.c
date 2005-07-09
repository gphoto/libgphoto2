/*****************************************************************************
 *  Copyright (C) 2002  Jason Surprise <thesurprises1@attbi.com>             *
 *                2003  Enno Bartels   <ennobartels@t-online.de>             *
 *****************************************************************************
 *                                                                           *
 *  This program is free software; you can redistribute it and/or            *
 *  modify it under the terms of the GNU Library General Public              *
 *  License as published by the Free Software Foundation; either             *
 *  version 2 of the License, or (at your option) any later version.         *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *  Library General Public License for more details.                         *
 *                                                                           *
 *  You should have received a copy of the GNU Library General Public        *
 *  License along with this program; see the file COPYING.LIB.  If not,      *
 *  write to the Free Software Foundation, Inc., 59 Temple Place -           *
 *  Suite 330,  Boston, MA 02111-1307, USA.                                  *
 *                                                                           *
 *****************************************************************************/


/*
    Old Maintainer : Enno Bartels <ennobartels@t-online.de> in 2002/2003
    Original author : Jason Surprise <thesurprises1@attbi.com> in 2002.
    Checksum author: Roberto Ragusa <r.ragusa@libero.it> in 2002/2003. 

    Ported to libgphoto2: Marcus Meissner <marcus@jet.franken.de> in 2005.

    Camera commands:
     Global comands
                           Bytes   | 0    1cmd 2    3    4    5    6    7    8    9    10   11   12   13   14   15
    -------------------------------+----------------------------------------------------------------------------------
     init camera              8    | 0x02 0xce 0x80 0x8a 0x84 0x8d 0x83 0x03
     get camera date/time     8    | 0x02 0xc1 0x80 0x8b 0x84 0x8e 0x8d 0x03
     Get number of pics      16    | 0x02 0xc6 0x88 0x80 0x80 0x83 0x84 0x38 0x2f 0x30 0x32 0x88 0x84 0x8e 0x8b 0x03
     Delete all pics         12    | 0x02 0xb1 0x84 0x8f 0x8f 0x8f 0x8f 0x86 0x80 0x8a 0x86 0x03


     Single picture comands:
     Allways 12 bytes long
                                   | 0    1cmd 2    3    4    5    6    7    8    9    10    11 
    -------------------------------+---------------------------------------------------------------
     Request for a preview         | 0x02 0xb3 0x84 0x80 ...                                 0x03
     Request for a picture         | 0x02 0xb4 0x84 0x80 ...                                 0x03
     Request date/time for preview | 0x02 0xc5 0x84 0x80 ...                                 0x03
     Request for deleting one pic  | 0x02 0xb1 0x84 0x80 ...                                 0x03

     0x02 = Startcode for a command
     0x03 = Endcode   for a command
 */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include <gphoto2-port-log.h>
#include <gphoto2-library.h>
#include <gphoto2-result.h>

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
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "hp215.h"

char monNameShort[12][30]; 

/*****************************************************************************
 *  Function name    : hp_gen_cmd                                            *
 *****************************************************************************/
/*! \fn int hp_gen_cmd (int cmd, int num, unsigned char *buffer)

    \brief  Generate a camera commando.
    \param  cmd    (i|-) Command 
    \param  num    (i|-) Number of the picture (1..n)
    \param  buffer (i|-)  
    \n\n

    For picture/thumbnail and pic date/time request, the following
    algorithm is used to generate the cmd to send to the camera for each
    picture.
  
    Thanks to Roberto Ragusa for figuring out the algorithm for this.  I
    would have never figured it out..

<pre>
                                   | b    u    f    f    e    r
                                   | 0    1cmd 2    3    4    5    6    7    8    9    10    11 
    -------------------------------+---------------------------------------------------------------
     Request for a preview         | 0x02 0xb3 0x84 0x80 ...                                 0x03
     Request for a picture         | 0x02 0xb4 0x84 0x80 ...                                 0x03
     Request date/time for preview | 0x02 0xc5 0x84 0x80 ...                                 0x03
     Request for deleting one pic  | 0x02 0xb1 0x84 0x80 ...                                 0x03


    Picture number:
    ----------------
    buffer[4] = ((num&0xf00)>>8) | 0x80;
    buffer[5] = ((num&0xf0 )>>4) | 0x80;
    buffer[6] = ((num&0xf  )   ) | 0x80;



    Rest:
    -----
    x4 = buffer[4] & 0xf;
    x5 = buffer[5] & 0xf;
    x6 = buffer[6] & 0xf;

    
    Request for a preview        : myx7=0xf  myx8=0x2  myx9=0xe  myx10=0x8
    Request for a picture        : myx7=0x3  myx8=0xa  myx9=0xa  myx10=0x9
    Request date/time for preview: myx7=0x3  myx8=0xa  myx9=0x9  myx10=0x5
    Request for delete one pict. : myx7=0x7  myx8=0x9  myx9=0xa  myx10=0x8
 

    myx7  ^= x6 ^ ((x5<<1)&0xf) ^ x5 ^ ((x4<<1)&0xf) ^ x4 ^ x4>>2;
    myx8  ^= (x6>>3) ^ ((x5<<1)&0xf) ^ (x5>>3) ^ x5 ^ ((x4<<2)&0xf) ^ ((x4<<1)&0xf) ^ x4;
    myx9  ^= ((x6<<1)&0xf) ^ ((x5>>3<<1)&0xf) ^ ((x5<<1)&0xf) ^ x5 ^ ((x4<<1)&0xf) ^ x4;
    myx10 ^= x6 ^ x5 ^ (x5>>3);

    buffer[7]  = myx7  | 0x80;
    buffer[8]  = myx8  | 0x80;
    buffer[9]  = myx9  | 0x80;
    buffer[10] = myx10 | 0x80;

    buffer[11] = 0x03;

</pre>
*/

static int 
hp_gen_cmd (int cmd, int num, unsigned char *buffer)
{
    int x4, x5, x6;
    int myx7, myx8, myx9, myx10;

    buffer[0] = 0x02;
    buffer[1] = cmd;
    buffer[2] = 0x84;
    buffer[3] = 0x80;
    buffer[4] = ((num&0xf00)>>8) | 0x80;
    buffer[5] = ((num&0xf0 )>>4) | 0x80;
    buffer[6] = ((num&0xf  )   ) | 0x80;

    x4 = buffer[4]&0xf;
    x5 = buffer[5]&0xf;
    x6 = buffer[6]&0xf;

    /* Kind of command */
    switch (cmd)
    {
      case 0xb3: /* Request for a preview */
                 myx7=0xf,myx8=0x2,myx9=0xe,myx10=0x8;
                 break;

      case 0xb4: /* Request for a picture */
                 myx7=0x3,myx8=0xa,myx9=0xa,myx10=0x9;
                 break;

      case 0xc5: /* Request date/time for preview */
                 myx7=0x3,myx8=0xa,myx9=0x9,myx10=0x5;
                 break;

      case 0xb1: /* Request delete a single pic */
                 myx7=0x7,myx8=0x9,myx9=0xa,myx10=0x8;
                 break;

      default:   /* unknown command */
                 gp_log (GP_LOG_ERROR, "hp215", "ERROR: Unknown command %x!\n", cmd);
                 return GP_ERROR;
    }

    /* What is happening here ? */
    myx7  ^= x6^((x5<<1)&0xf)^x5^((x4<<1)&0xf)^x4^x4>>2;
    myx8  ^= (x6>>3)^((x5<<1)&0xf)^(x5>>3)^x5^((x4<<2)&0xf)^((x4<<1)&0xf)^x4;
    myx9  ^= ((x6<<1)&0xf)^((x5>>3<<1)&0xf)^((x5<<1)&0xf)^x5^((x4<<1)&0xf)^x4;
    myx10 ^= x6^x5^(x5>>3);

    buffer[7]  = myx7  | 0x80;
    buffer[8]  = myx8  | 0x80;
    buffer[9]  = myx9  | 0x80;
    buffer[10] = myx10 | 0x80;

    buffer[11] = 0x03;
    return GP_OK;
}


static int 
hp_send_ack (Camera *cam)
{
    unsigned char  byte = ACK;
    int ret;

    gp_log (GP_LOG_DEBUG, "hp215", "Sending ACK ... ");
    ret = gp_port_write (cam->port, &byte, 1);
    if (ret < GP_OK)
	gp_log (GP_LOG_ERROR, "hp215", "ACK sending failed with %d\n", ret);
    return ret;
}


static int 
hp_rcv_ack (Camera *cam)
{
    int           ret;
    unsigned char byte = '\0';

    gp_log (GP_LOG_DEBUG, "hp215", "Receiving ACK ... ");
    ret = gp_port_read (cam->port, &byte, 1);
    if (ret < GP_OK)
	return ret;
    if (byte == ACK)
      return GP_OK;
    gp_log (GP_LOG_DEBUG, "hp215", "Expected ACK, but read %02x", byte);
    return GP_ERROR_IO;
}


/*****************************************************************************
 *  Function name    : hp_get_timeDate_cam                                   *
 *****************************************************************************/
/*! \fn int hp_get_timeDate_cam (t_camera *cam)

    \brief  Get date and time.
    \param  cam   (i|-) Pointer to the usb device ?!
    \n\n

    This function will read the date and time of the camera.
    \n\n\n
*/
static int 
hp_get_timeDate_cam (Camera *cam)
{
	int           i, ret;
	unsigned char msg[0x6b];
	unsigned char buffer[] = HP_CMD_DATETIME;
	t_date        date;

	memset(msg, 0, sizeof(msg));

	/* Sending date/time command */
	gp_log (GP_LOG_DEBUG, "hp215", "Sending date/time command ... ");
	i = gp_port_write (cam->port, buffer, 8);
	if (i < GP_OK)
		return i;

	gp_log (GP_LOG_DEBUG, "hp215", "OK!\n");
	/* Check receive ACK */
	if (hp_rcv_ack (cam))
		return GP_ERROR_IO;

	gp_log (GP_LOG_DEBUG, "hp215", "Receiving date/time msg ...");

	/* Reading date/time answer */
	i = gp_port_read (cam->port, msg, 0x6b);
	if (i != 0x6b) {
		gp_log (GP_LOG_ERROR, "hp215", "FAILED: Could not receive 0x6b bytes, but %d!\n", i);
		return GP_ERROR_IO;
	}

	/* Check start/stop value of the answer */
	if (msg[0]!=2) {
		gp_log (GP_LOG_DEBUG, "hp215", "FAILED: Start code missing!\n");
		return GP_ERROR_IO;
	}

	if (msg[0x6a]!=3) {
		gp_log (GP_LOG_DEBUG, "hp215", "FAILED: Start code missing!\n");
		return GP_ERROR_IO;
	}

	gp_log (GP_LOG_DEBUG, "hp215", "OK!\n");
	ret = hp_send_ack(cam);
	if (ret < GP_OK)
		return ret;

	/* Filter time and date out of the message */
	date.day   = (msg[8]-48)*10 + (msg[9]-48);
	date.month = (msg[5]-48)*10 + (msg[6]-48) - 1;
	date.year  = 2000 + (msg[11]-48)*10 + (msg[12]-48);

	date.hour  = (msg[14]-48)*10 + (msg[15]-48);
	date.min   = (msg[17]-48)*10 + (msg[18]-48);

	sprintf (buffer, _("Current camera time:  %04d-%02d-%02d  %02d:%02d\n"), 
		date.year, date.month, date.day, date.hour, date.min
	);
	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	/*
	 * Fill out the summary with some information about the current 
	 * state of the camera (like pictures taken, etc.).
	 */

	return (GP_OK);
}


static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	/*
	 * If you would like to tell the user some information about how 
	 * to use the camera or the driver, this is the place to do.
	 */

	return (GP_OK);
}


static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("hp215\n"
				"Marcus Meissner <marcus@jet.franken.de>\n"
				"Driver to access the HP Photosmart 215 camera.\n"
				"Merged from the standalone hp215 program.\n"
				"This driver allows download of images and previews, and deletion of images.\n"
	));
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int           ret;
	unsigned char msg[0x01000];
	unsigned char buffer[12];
        int image_no;
	unsigned char cmd;

        image_no = gp_filesystem_number(fs, folder, filename, context);
        if(image_no < 0)
                return image_no;
	image_no++;

	memset(msg, 0, sizeof(msg));
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		cmd = 0xb4;
		break;
	case GP_FILE_TYPE_PREVIEW:
		cmd = 0xb3;
		break;
	default:
		return GP_ERROR_BAD_PARAMETERS;
	}

	hp_gen_cmd (cmd, image_no, buffer); 
	ret = gp_port_write (camera->port, buffer, 0x0c);
	if (ret < GP_OK)
		return ret;

	if (hp_rcv_ack(camera))
		return GP_ERROR_IO;

	ret = gp_port_read (camera->port, msg, 0x87);
	if (ret!=0x87)
	{
		gp_log (GP_LOG_ERROR, "hp215", "FAILED: Second ack failed!\n");
		return GP_ERROR_IO;
	}
	/* Check start code */
	if (msg[0]!=2) {
		gp_log (GP_LOG_ERROR, "hp215", "FAILED: Start code missing!\n");
		return GP_ERROR_IO;
	}

	/* Check stop code */
	if (msg[ret-1]!=3) {
		gp_log (GP_LOG_ERROR, "hp215", "FAILED: Stop code missing!\n");
		return GP_ERROR_IO;
	}

	ret = hp_send_ack(camera);
	if (ret < GP_OK)
		return ret;

	gp_file_set_mime_type (file, GP_MIME_JPEG);

	/* Read preview in 4096 byte parts (0x1000) */
	while (1)
	{
		ret = gp_port_read (camera->port, msg, 0x1000);
		/* Check if preview reading is complete*/
		if ((ret==1) && (msg[0]==0x04)) {
			gp_log (GP_LOG_DEBUG, "hp215", "Image data complete!\n");
			break;
		}

		/* Check if there was an error */
		if (ret==-1) {
			gp_log (GP_LOG_ERROR, "hp215", "Warning: Image data may be corrupted...\n");
			break;
		}
		gp_file_append (file, msg, ret);
	}
	return GP_OK;
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int ret;
	unsigned char  msg[0x1000];
	unsigned char  buffer[12];
        int image_no;

        image_no = gp_filesystem_number(fs, folder, filename, context);
        if(image_no < 0)
                return image_no;
	image_no++;

	memset(msg, 0, sizeof(msg));
	hp_gen_cmd (0xb1, image_no, buffer); 
	ret = gp_port_write (camera->port, buffer, 0x0c);
	if (ret < GP_OK)
		return ret;
	if (hp_rcv_ack(camera))
		return GP_ERROR_IO;
	ret = gp_port_read (camera->port, msg, 10);
	if (ret!=10)
		return GP_ERROR_IO;
	ret = hp_send_ack(camera);
	if (ret < GP_OK)
		return ret;
	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	unsigned char buffer[] = HP_CMD_DELETE_PICS;
	unsigned char msg[10];
	int      ret;

	memset (msg, 0, sizeof(msg));

	ret = gp_port_write (camera->port, buffer, 12);
	if (ret < GP_OK)
		return GP_ERROR_IO;
	if (hp_rcv_ack(camera))
		return GP_ERROR_IO;
	ret = gp_port_read (camera->port, msg, sizeof(msg));
	if (ret != sizeof(msg))
		return GP_ERROR_IO;
	return hp_send_ack(camera);
}


static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int           ret;
	unsigned char msg[0x01000];
	unsigned char buffer[12];
	int           offset = 13;
        int image_no;

        image_no = gp_filesystem_number(fs, folder, filename, context);
        if(image_no < 0)
                return image_no;
	image_no++;

	memset (msg, 0, sizeof(msg));
	hp_gen_cmd (0xc5, image_no, buffer); 
	ret = gp_port_write (camera->port, buffer, 0x0c);
	if (ret < GP_OK)
		return ret;
	if (hp_rcv_ack (camera))
		return GP_ERROR_IO;
	ret = gp_port_read (camera->port, msg, 0x3e);
	if (ret != 0x3e) {
		gp_log (GP_LOG_DEBUG, "hp215", "0x3e bytes expected, but only %d read in get_info_func\n", ret);
		return GP_ERROR_IO;
	}
	/* Check start  code of the getten message */
	if (msg[0] != 0x02)
		return GP_ERROR_IO;

	/* Check stop code of the getten message */
	if (msg[ret-1] != 0x03)
		return GP_ERROR_IO;

	ret = hp_send_ack(camera);
	if (ret < GP_OK)
		return ret;

	return GP_OK;
#if 0
	/* Copy getten date into date placeholder */
	strcpy ((camera->pic[cnt+1])->date_time, &msg[0x0d]); 

	/* Calculate day,month, year, hour and minute of the photo */
	(camera->pic[cnt+1])->date.day   = (msg[3+offset]-48)*10 + (msg[4+offset]-48);
	(camera->pic[cnt+1])->date.month = (msg[0+offset]-48)*10 + (msg[1+offset]-48);
	(camera->pic[cnt+1])->date.year  = 2000 + (msg[6+offset]-48)*10 + (msg[7+offset]-48);
	(camera->pic[cnt+1])->date.hour  = (msg[ 9+offset]-48)*10 + (msg[10+offset]-48);
	(camera->pic[cnt+1])->date.min   = (msg[12+offset]-48)*10 + (msg[13+offset]-48);

	/* Get the file info here and write it into <info> */

	return (GP_OK);
#endif
}


/*
 
   000013 B> 00000000: 02c6 8880 8083 8438 2f30 3288 848e 8b03 .......8/02.....
  
   000014 B< 00000000: 06                                      .
  
   000015 B< 00000000: 02c6 aae0 e050 686f 746f 416c 6275 6d00 .....PhotoAlbum.
             00000010: 3a32 3600 8080 0180 8182 8c81 8080 8080 :26.............
             00000020: 8080 8080 e480 8080 8080 8080 8087 8084 ................
             00000030: 8a03                                    ..
  
   000016 B> 00000000: 06                                      .

    The first cmd in the trace is sent to the camera.  It appears to
    change depending on some factors.  For example, it seems to use the
    last 4 digits of the date returned from the camera as part of the cmd
    that is sent (the 8/02 above is from 7/18/02 that was returned from
    the camera in hp_get_timeDate_cam.  However, I have found that I
    can use the above command as is regardless of the date/time and still
    receive the PhotoAlbum correctly, so I've implemented it as a static
    call.
  
    From observation, the 43+44+45th lower words of the returned msg contains
    the number of pictures in the camera.
 */

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	int           ret, count, i;
	unsigned char msg[0x32];
	unsigned char buffer[] = HP_CMD_GET_PHOTO_ALBUM;

	/* Initalize message */
	memset (msg, 0, sizeof(msg));

	/* Sending photo album request command */
	gp_log (GP_LOG_DEBUG, "hp215", "Sending photo album request ... ");
	i = gp_port_write (camera->port, buffer, 16);
	if(i < GP_OK)
		return i;
	if (hp_rcv_ack(camera))
		return GP_ERROR_IO;

	/* Reading - Receiving photo album */
	gp_log (GP_LOG_DEBUG, "hp215", "Receiving photo album ... ");
	i = gp_port_read (camera->port, msg, 0x32);
	/* Check the length of the getten message */
	if(i != 0x32) {
		gp_log (GP_LOG_DEBUG, "hp215", "FAILED: Could not receive 0x32 bytes, just %d!\n", i);
		return GP_ERROR_IO;
	}

	/* Check start code */
	if (msg[0] != 2) {
		gp_log (GP_LOG_DEBUG, "hp215", "FAILED: Start code missing!\n");
		return GP_ERROR_IO;
	}

	/* Check stop code */
	if (msg[0x31] != 3) {
		gp_log (GP_LOG_DEBUG, "hp215", "FAILED: Stop code missing!\n");
		return GP_ERROR_IO;
	}

	ret = hp_send_ack (camera);
	if (ret < GP_OK) {
		gp_log (GP_LOG_DEBUG, "hp215", "FAILED: send ack failed!\n");
		return ret;
	}
	/* Calculate the number of pictures from the gotten message */
	count = 256*(msg[42]&0x7f) + 16*(msg[43]&0x7f) + (msg[44]&0x7f);
        return gp_list_populate(list, "image%i.jpg", count);
}

int
camera_id (CameraText *id) {
	strcpy(id->text, "hp215");
	return (GP_OK);
}


int
camera_abilities (CameraAbilitiesList *list) {
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "HP:PhotoSmart 215");
	a.status		= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port			= GP_PORT_USB;
	a.usb_vendor		= 0x3f0;	/* HP */
	a.usb_product		= 0x6202;	/* Photosmart 215 */
	a.operations		= GP_OPERATION_NONE;
	a.file_operations	= GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
	a.folder_operations	= GP_FOLDER_OPERATION_DELETE_ALL;
	return gp_abilities_list_append(list, a);
}


int
camera_init (Camera *camera, GPContext *context) 
{
	int           ret;
	unsigned char msg[10];
	unsigned char buffer[] = HP_CMD_INIT;
	GPPortSettings settings;

        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, delete_file_func, camera);
	gp_filesystem_set_folder_funcs (camera->fs, NULL, delete_all_func, NULL, NULL, camera);

	gp_port_get_settings (camera->port, &settings);
	settings.usb.inep  = 0x83;
	settings.usb.outep = 0x04;
	gp_port_set_settings (camera->port, settings);
/*
    This function does the initialisation sequence.
    The following init sequence is visible in all traces I've done, so it
    is most likely needed before any other command is sent

   000005	B>  00000000:  02ce 808a 848d 8303                   ........
   000006	B<  00000000:  06                                    .
   000007	B<  00000000:  02ce 82e0 e08a 8985 8803              ..........
   000008	B>  00000000:  06                                    .
 */

	memset(msg, 0, sizeof(msg));

	gp_log (GP_LOG_DEBUG, "hp215", "Sending init sequence ... ");

	ret = gp_port_write (camera->port, buffer, 8);
	if (ret < GP_OK)
		return ret;
	if (hp_rcv_ack (camera))
		return GP_ERROR_IO;
	gp_log( GP_LOG_DEBUG, "hp215", "Expecting init sequence ... ");
	ret = gp_port_read (camera->port, msg, 10);
	if (ret != 10) {
		gp_log (GP_LOG_ERROR, "hp215", "ERROR: Init failed. %d bytes received instead of 10!\n", ret);
		return GP_ERROR_IO;
	}
	return hp_send_ack (camera);
}
