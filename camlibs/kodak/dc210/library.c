/*
  DC 210/215 driver library.
  by Hubert Figuiere <hfiguiere@teaser.fr>
  port of gphoto 0.4.x driver.

 */

#include "dc210.h"
#include "config.h"
#include "library.h"

#include <gphoto2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#ifdef HAVE_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SELECT_H */

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#    ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#    else HAVE_SYS_TIME_H
#    include <time.h>
#    endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */


static int kodak_dc210_read (DC210Data * dd, unsigned char *buf, int nbytes);
static int kodak_dc210_write_byte (DC210Data * dd, char b);
static unsigned char kodak_dc210_checksum(char *packet,int length);


static int kodak_dc210_send_command (DC210Data * dd, char command, char arg1, char arg2, char arg3, char arg4)
{
    unsigned char ack;
    int success = TRUE;
    
    /* try to write 8 bytes in sequence to the camera */
    success = success && kodak_dc210_write_byte ( dd, command );
    success = success && kodak_dc210_write_byte ( dd, 0x00 );
    success = success && kodak_dc210_write_byte ( dd, arg1 );
    success = success && kodak_dc210_write_byte ( dd, arg2 );
    success = success && kodak_dc210_write_byte ( dd, arg3 );
    success = success && kodak_dc210_write_byte ( dd, arg4 );
    success = success && kodak_dc210_write_byte ( dd, 0x00 );
    success = success && kodak_dc210_write_byte ( dd, 0x1A );
    
    /* if the command was sent successfully to the camera, continue */
    if (success)
    {
        /* read ack from camera */
        success = kodak_dc210_read( dd, &ack, 1 );
        
        if (success)
        {
            /* make sure the ack is okay */
            if (ack != DC_COMMAND_ACK)
            {
                fprintf(stderr,"kodak_dc210_send_command - bad ack from camera\n");
                success = FALSE;
            }
        }
        else
        {
            fprintf(stderr,"kodak_dc210_send_command - failed to read ack from camera\n");
            success = FALSE;
        }
    }
    else
    {
        fprintf(stderr,"kodak_dc210_send_command - failed to send command to camera\n");
        success = FALSE;
    }
    
    return(success);
}

/*
  tell the camera to change the port speed,
  then set it.
 */
int kodak_dc210_set_port_speed(DC210Data * dd, int speed)
{
    int success = 0;
    int arg1, arg2;

    gp_port_settings settings;

    if (gp_port_settings_get (dd->dev, &settings) == GP_ERROR) {
        return GP_ERROR;
    }
    settings.serial.speed = speed;

    switch (speed) {
    case 9600:
        arg1 = 0x96;
        arg2 = 0x00;
        break;
    case 19200:
        arg1 = 0x19;
        arg2 = 0x20;
        break;
    case 38400:
        arg1 = 0x38;
        arg2 = 0x40;
        break;
    case 57600:
        arg1 = 0x57;
        arg2 = 0x60;
        break;
    case 115200:
        arg1 = 0x11;
        arg2 = 0x52;
        break;
    default:
        /* speed not supported */
        return GP_ERROR;
    }

    success = kodak_dc210_send_command(dd, DC_SET_SPEED, arg1, arg2, 0x00, 0x00);
    if (success) {
        if (gp_port_settings_set (dd->dev, settings) == GP_ERROR) {
            return GP_ERROR;
        }
    }
    else {
        /* unable to change the speed */
        return GP_ERROR;
    }
    return GP_OK;
}


int kodak_dc210_command_complete (DC210Data * dd)
{
    int status = DC_BUSY;
    int success = TRUE;
    
    /* Wait for the camera to be ready. */
    do
    {
        success = kodak_dc210_read(dd, (char *)&status, 1);
    }
    while (success && (status == DC_BUSY));
    
    if (success)
    {
        if (status != DC_COMMAND_COMPLETE)
        {
            if (status == DC_ILLEGAL_PACKET)
            {
                fprintf(stderr,"kodak_dc210_command_complete - illegal packet received from host\n");
            }
            else
            {
                fprintf(stderr,"kodak_dc210_command_complete - command not completed\n");
            }
            
            /* status was not command complete - raise an error */
            success = FALSE;
        }
    }
    else
    {
        fprintf(stderr,"kodak_dc210_command_complete - failed to read status byte from camera\n");
        success = FALSE;
    }
    
    return(success);
}

static int kodak_dc210_read (DC210Data * dd, unsigned char *buf, int nbytes )
{
    return (gp_port_read(dd->dev, buf, nbytes));
}


static int kodak_dc210_write_byte (DC210Data * dd, char b )
{
    return (gp_port_read(dd->dev, &b, 1));
}

int kodak_dc210_read_packet (DC210Data * dd, char *packet, int length)
{
    int success = TRUE;
    unsigned char sent_checksum;
    unsigned char computed_checksum;
    unsigned char control_byte;
    
  /* Read the control byte from the camera - and packet coming from
     the camera must ALWAYS be 0x01 according to Kodak Host Protocol. */
    if (kodak_dc210_read(dd, &control_byte, 1))
    {
        if (control_byte == PKT_CTRL_RECV)
        {
            if (kodak_dc210_read(dd, packet, length))
            {
                /* read the checksum from the camera */
                if (kodak_dc210_read(dd, &sent_checksum, 1))
                {
                    computed_checksum = kodak_dc210_checksum(packet, length);
                    
                    if (sent_checksum != computed_checksum)
                    {
                        fprintf(stderr,"kodak_dc210_read_packet: bad checksum %d != %d\n",computed_checksum,sent_checksum);
                        kodak_dc210_write_byte(dd, DC_ILLEGAL_PACKET);
                        success = FALSE;
                    }
                    else
                    {
                        kodak_dc210_write_byte(dd, DC_CORRECT_PACKET);
                        success = TRUE;
                    }
                }
                else
                {
                    fprintf(stderr,"kodak_dc210_read_packet: failed to read checksum byte from camera\n");
                    success = FALSE;
                }
            }
            else
            {
                fprintf(stderr,"kodak_dc210_read_packet: failed to read paket from camera\n");
                success = FALSE;
            }
        }
        else
        {
            fprintf(stderr,"kodak_dc210_read_packet - packet control byte invalid %x\n",control_byte);
            success = FALSE;
        }
    }
    else
    {
        fprintf(stderr,"kodak_dc210_read_packet - failed to read control byte from camera\n");
        success = FALSE;
    }
    
    return success;
}

static unsigned char kodak_dc210_checksum(char *packet,int length)
{
/* Computes the *primitave* kodak checksum using XOR - yuck. */
    unsigned char chksum = 0;
    int i;
    
    for (i = 0 ; i < length ; i++)
    {
        chksum ^= packet[i];
    }
    
    return chksum;
}


/*
  open the camera. The port should already be open....
  set the speed to 115200
 */
int kodak_dc210_open_camera (DC210Data * dd)
{
    int ret;

    ret = kodak_dc210_send_command (dd, DC_INITIALIZE,0x00,0x00,0x00,0x00);
    ret = kodak_dc210_command_complete (dd);
    ret = kodak_dc210_set_port_speed (dd, 115200);
    
    return ret;
}

/* Close the camera */
int kodak_dc210_close_camera (DC210Data * dd) 
{
    return kodak_dc210_set_port_speed(dd, 9600);
}


int kodak_dc210_number_of_pictures (DC210Data * dd) 
{
    int numPics = 0;
    struct kodak_dc210_status status;
    
    kodak_dc210_get_camera_status (dd, &status);
    
    numPics = status.num_pictures;
    
    return(numPics);
}


/*
  Capture a picture and return the index of the picture 
  in the gphoto form (1 based).
 */
int kodak_dc210_capture(DC210Data * dd)
{
    int ret;

    ret = kodak_dc210_send_command (dd, DC_TAKE_PICTURE,0x00,0x00,0x00,0x00);
    ret = kodak_dc210_command_complete (dd);

    return kodak_dc210_number_of_pictures(dd);
}

static int kodak_dc210_get_picture_info (DC210Data * dd,
                                  int picNum,
                                  struct kodak_dc210_picture_info *picInfo)
{
    unsigned char packet[256];
    
    picNum--;

    /* send picture info command, receive data, send ack */
    kodak_dc210_send_command (dd, DC_PICTURE_INFO, 0x00, (char)picNum, 0x00, 0x00);
    kodak_dc210_read_packet (dd, packet, 256);
    kodak_dc210_command_complete (dd);
    
    picInfo->resolution    = packet[3];
    picInfo->compression   = packet[4];
    picInfo->pictureNumber = (packet[6] << 8) + packet[7];
    
    picInfo->fileSize      = ((long)packet[8] << 24) + 
        ((long)packet[9]  << 16) + 
        ((long)packet[10] << 8) + 
        (long)packet[11];
    
    picInfo->elapsedTime   = ((long)packet[12] << 24) + 
        ((long)packet[13] << 16) + 
        ((long)packet[14] << 8) + 
        (long)packet[15];
    
    strncpy(picInfo->fileName, packet + 32, 12);
    /* TODO handle error codes */
    return GP_OK;
}

int kodak_dc210_get_camera_status (DC210Data * dd,
                                   struct kodak_dc210_status *status)
{
    unsigned char packet[256];
    int success = TRUE;
    
    if (kodak_dc210_send_command (dd, DC_STATUS, 0x00,0x00,0x00,0x00))
    {
        if (kodak_dc210_read_packet (dd, packet, 256))
        {
            kodak_dc210_command_complete (dd);
            
            memset((char*)status,0,sizeof(struct kodak_dc210_status));
            
            status->camera_type_id        = packet[1];
            status->firmware_major        = packet[2];
            status->firmware_minor        = packet[3];
            status->batteryStatusId       = packet[8];
            status->acStatusId            = packet[9];
            
            /* seconds since unix epoc */
            status->time = CAMERA_EPOC + (
                packet[12] * 0x1000000 + 
                packet[13] * 0x10000 + 
                packet[14] * 0x100 + 
                packet[15]) / 2;
            
            status->zoomMode              = packet[15];
            status->flash_charged         = packet[17];
            status->compression_mode_id   = packet[18];
            status->flash_mode            = packet[19];
            status->exposure_compensation = packet[20];
            status->picture_size          = packet[21];
            status->file_Type             = packet[22];
            status->totalPicturesTaken    = packet[25] * 0x100 + packet[26];
            status->totalFlashesFired     = packet[27] * 0x100 + packet[28];
            status->num_pictures          = packet[56] * 0x100 + packet[57];
            strncpy(status->camera_ident,packet + 90,32); 
        }
        else
        {
            fprintf(stderr,"kodak_dc210_get_camera_status: send command failed\n");
            success = FALSE;
        }
    }
    else
    {
        fprintf(stderr,"kodak_dc210_get_camera_status: send command failed\n");
        success = FALSE;
    }
    
    return success;
}


int kodak_dc210_get_thumbnail (DC210Data * dd, int picNum, CameraFile *file) 
{
    struct kodak_dc210_picture_info picInfo;
    int i,j;
    int numRead = 0;
    int success = TRUE;
    int fileSize = 20736;
    int blockSize = 1024;
    char *picData;
    char *imData;
    char bmpHeader[] = {
        0x42, 0x4d, 0x36, 0x24, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x48, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


    /* get information (size, name, etc) about the picture */
    kodak_dc210_get_picture_info (dd, picNum, &picInfo);

    /* the protocol expects 0..n-1, but this function is supplied 1..n */
    picNum--;
    
    /* allocate space for the thumbnail data */
    picData = (char *)malloc(fileSize + blockSize);
    
    /* allocate space for the image data */
    imData = (char *)malloc(fileSize + 54);
    
    
    if (kodak_dc210_send_command(dd, DC_PICTURE_THUMBNAIL, 0x00, (char)picNum, 0x01, 0x00))
    {
        while (success && (numRead < fileSize))
        {
            if (kodak_dc210_read_packet(dd, picData+numRead, blockSize))
            {
                numRead += blockSize;
	    }
            else
            {
                fprintf(stderr,"kodak_dc210_get_thumbnail - bad packet read from camera\n");
                success = FALSE;
            }
        }
        
        /* get to see if the thumbnail was retreived */
        if (success)
        {
            kodak_dc210_command_complete(dd);
/*            update_progress(100);*/
            
            memcpy(imData,bmpHeader, sizeof(bmpHeader));
            
            /* reverse the thumbnail data */
            /* not only is the data reversed but the image is flipped
             * left to right
             */
            for (i = 0; i < 72; i++) {
                for (j = 0; j < 96; j++) {
                    imData[i*96*3+j*3+54] = picData[(71-i)*96*3+j*3+2];
                    imData[i*96*3+j*3+54+1] = picData[(71-i)*96*3+j*3+1];
                    imData[i*96*3+j*3+54+2] = picData[(71-i)*96*3+j*3];
                }
            }

	    gp_file_append(file, imData, fileSize + 54);
		
	    strcpy(file->type, "image/bmp");
	    strncpy(file->name, picInfo.fileName, 12);
	    file->name[12] = 0;
        }
    }
    else
    {
        fprintf(stderr,"kodak_dc210_get_thumbnail: failed to get thumbnail command to camera\n");
        success = FALSE;
    }
    
    free (imData);
    free (picData);
    
    return GP_OK;
}



int kodak_dc210_get_picture (DC210Data * dd, int picNum, CameraFile *file) 
/*
  int kodak_dc210_get_picture (DC210Data * dd, int thumbnail, const char *folder, const char *filename, 
  CameraFile *file)
*/
{
    int blockSize;
    int fileSize;
    int numRead;
    int numBytes;
    struct kodak_dc210_picture_info picInfo;
    char *picData;
    
    /* get information (size, name, etc) about the picture */
    kodak_dc210_get_picture_info (dd, picNum, &picInfo);
    
    /* DC210 addresses pictures 0..n-1 instead of 1..n */
    picNum--;
    
    /* send the command to start transfering the picture */
    kodak_dc210_send_command(dd, DC_PICTURE_DOWNLOAD, 0x00, (char)picNum, 0x00, 0x00);
    
    fileSize = picInfo.fileSize;
    blockSize = 1024;
    picData = (char *)malloc(blockSize);
    numRead = 0;
    
/*        update_progress(0);*/
    while (numRead < fileSize)
    {
	kodak_dc210_read_packet(dd, picData, blockSize);
	numRead += blockSize;
	if (numRead > fileSize) {
	    numBytes = fileSize % blockSize;
	}
	else {
	    numBytes = blockSize;
	}
	
	gp_file_append(file, picData, numBytes);

    }
    free (picData);
    fprintf(stderr,"%d/%d\n",numRead,fileSize);
    kodak_dc210_command_complete (dd);

    strcpy(file->type, "image/jpeg");
    strncpy(file->name, picInfo.fileName, 12);
    file->name[12] = 0;

    return GP_OK;
}

int kodak_dc210_delete_picture (DC210Data * dd, int picNum) 
{
    picNum--;
    
    kodak_dc210_send_command (dd,DC_ERASE_IMAGE_IN_CARD,0x00,(char)picNum,0x00,0x00);
    kodak_dc210_command_complete (dd);
    return (1);
}


#if 0
struct Image *kodak_dc210_get_preview () 
{
  int numPicBefore;
  int numPicAfter;
  struct Image *im = NULL;

  /* find out how many pictures are in the camera so we can
     make sure a picture was taken later */
  numPicBefore = kodak_dc210_number_of_pictures();

  /* take a picture -- it returns the picture number taken */
  numPicAfter = kodak_dc210_take_picture();

  /* if a picture was taken then get the picture from the camera and
     then delete it */
  if (numPicBefore + 1 == numPicAfter)
  {
    im = kodak_dc210_get_picture (numPicAfter,0);
    kodak_dc210_delete_picture (numPicAfter);
  }

  return(im);
}
#endif

