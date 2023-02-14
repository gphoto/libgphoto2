/* library.c
 *
 * Copyright (C) 2001,2002 Hubert Figuiere <hfiguiere@teaser.fr>
 * Copyright (C) 2000,2001,2002 Scott Fritzinger
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

/*
  Kodak DC 240/280/3400/5000 driver.
  Maintainer:
       Hubert Figuiere <hfiguiere@teaser.fr>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include "libgphoto2/gphoto2-endian.h"

#include "libgphoto2/i18n.h"

#include "dc240.h"
#include "library.h"


#define GP_MODULE "dc240"

/* do not sleep during fuzzing */
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
# define usleep(x)
#endif

/* legacy from dc240.h */
/*
  #define COMM1	0x00
  #define READY	0x10
  #define ACK	0xd1
  #define PACK1	0xd2
*/
/* nice. errors all have upper nibble of 'e' */
/*
  #define NAK	0xe1
  #define COMM0	0xe2
  #define PACK0	0xe3
  #define CANCL	0xe4
*/

/* system codes */
enum {
    DC240_SC_COMPLETE = 0x0,
    DC240_SC_ACK = 0xd1,
    DC240_SC_CORRECT = 0xd2,
    DC240_SC_FIRST_ERROR = 0xe0, /* not really an real code */
    DC240_SC_NAK = 0xe1,
    DC240_SC_ERROR = 0xe2,
    DC240_SC_ILLEGAL = 0xe3,
    DC240_SC_CANCEL = 0xe4,
    DC240_SC_BUSY = 0xf0
};

static unsigned char *dc240_packet_new   (int command_byte);
static int   dc240_packet_write (Camera *camera, unsigned char *packet, int size,
				 int read_response);
static int   dc240_packet_read  (Camera *camera, unsigned char *packet, int size);



static unsigned char *
dc240_packet_new (int command_byte) {

    unsigned char *p = malloc(8);
    memset (p, 0, 8);
    p[0] = command_byte;
    p[7] = 0x1a;
    return p;
}

static unsigned char *
dc240_packet_new_path (const char *folder, const char *filename) {
    unsigned char *p;
    char buf[1024];
    size_t x;
    unsigned char cs = 0;

    p = malloc(sizeof(char)*60);
    if (!p)
        return NULL;

    strcpy(buf, folder);

    if (buf[strlen(buf)-1] != '/')
        strcat(buf, "/");

    if (filename)
        strcat(buf, filename);
    else
        strcat(buf, "*.*");

    for (x=0; x<strlen(buf); x++) {
        buf[x] = (buf[x] == '/'? '\\' : buf[x]);
        cs = cs ^ (unsigned char)buf[x];
    }

    memset(p, 0, sizeof(char)*60);
    p[0] = 0x80;
    memcpy(&p[1], buf, sizeof(char)*strlen(buf));
    p[59] = cs;
    return (p);
}


static int dc240_packet_write (Camera *camera, unsigned char *packet, int size, int read_response) {

    /* Writes the packet and returns the result */

    int x=0;
    char in[2];

write_again:
    /* If retry, give camera some recup time */
    if (x > 0) {
        usleep(SLEEP_TIMEOUT * 1000);
    }

    /* Return error if too many retries */
    x++;
    if (x > RETRIES) {
        return (GP_ERROR_TIMEOUT);
    }

    if (gp_port_write(camera->port, (char*)packet, size) < GP_OK) {
        goto write_again;
    }

    /* Read in the response from the camera if requested */
    while (read_response) {
	int ret;
        if ((ret=gp_port_read(camera->port, in, 1)) >= GP_OK) {
            /* On error, read again */
	    read_response = 0;
            break;
	}
        if (ret == GP_ERROR_IO_READ) return ret; /* e.g. device detached? */
    }

    return GP_OK;
}

static int
dc240_packet_read (Camera *camera, unsigned char *packet, int size)
{

    int retval = gp_port_read(camera->port, (char*)packet, size);

    /*
     * If we try to read data about a non-picture file, we get back
     * a single DC240_SC_ERROR byte.  So, if we're trying to read a packet with
     * size greater than one, but we only get one byte, and it is a
     * DC240_SC_ERROR error, return failure
     */
    if ((size > 1) && (retval == 1) && (packet[0] == DC240_SC_ERROR))
	return GP_ERROR_NOT_SUPPORTED;

    if (retval < GP_OK)
	return retval;
    else
	return GP_OK;
}

static int dc240_packet_write_ack (Camera *camera)
{
    int retval;
    unsigned char c;

    c = DC240_SC_CORRECT;
    retval = gp_port_write(camera->port, (char*)&c, 1);
    if (retval < GP_OK)
	return retval;
    return GP_OK;
}

static int dc240_packet_write_nak (Camera *camera)
{
    int retval;
    unsigned char c;

    c = DC240_SC_ILLEGAL;
    retval = gp_port_write(camera->port, (char*)&c, 1);
    if (retval < GP_OK)
	return retval;
    return GP_OK;
}

static int dc240_wait_for_completion (Camera *camera) {
    unsigned char p[8];
    int retval = GP_OK;
    int x=0, done=0;

    /* Wait for command completion */
    while ((x++ < 25)&&(!done)) {
	retval = dc240_packet_read(camera, p, 1);
	switch (retval) {
	case GP_ERROR:
	    GP_DEBUG ("GP_ERROR\n");
	    return (GP_ERROR);
	    break;
	case GP_ERROR_TIMEOUT:
	    GP_DEBUG ("GP_ERROR_TIMEOUT\n");
	    /* in busy state, GP_ERROR_IO_READ can happened */
	    break;
	default:
	    done = 1;
	}
    }

    if (x==25) {
	return (GP_ERROR_TIMEOUT);
    }

    return GP_OK;
}

/*
  Implement the wait state aka busy case
  See 2.7.2 in the DC240 Host interface Specification
 */
static int dc240_wait_for_busy_completion (Camera *camera)
{
    enum {
	BUSY_RETRIES = 100
    };
    unsigned char p[8];
    int retval = 0;
    int x=0, done=0;

    /* Wait for command completion */
    while ((x++ < BUSY_RETRIES)&&(!done)) {
	retval = dc240_packet_read(camera, p, 1);
	switch (retval) {
	case GP_ERROR:
	    return retval;
	    break;
	case GP_ERROR_IO_READ:
	case GP_ERROR_TIMEOUT:
	    /* in busy state, GP_ERROR_IO_READ can happened */
	    break;
	default:
	    if (*p != DC240_SC_BUSY) {
		done = 1;
	    }
	}
    }
    if (x == BUSY_RETRIES)
	return (GP_ERROR);
    return retval;
}


static int dc240_packet_exchange (Camera *camera, CameraFile *file,
                           unsigned char *cmd_packet, unsigned char *path_packet,
                           int *size, int block_size, GPContext *context)
{
    /* Reads in multi-packet data, appending it to the "file". */
    unsigned char check_sum;
    int i;
    int num_packets=1, num_bytes, retval = GP_OK;
    int x=0, retries=0;
    float t;
    unsigned char packet[HPBS+2];
    unsigned int id;

    if (*size > 0) {
        /* Known size to begin with */
        t = (float)*size / (float)(block_size);
        num_packets = (int)t;
        if (t - (float)num_packets > 0) {
            num_packets++;
	}
    } else {
        /* Calculate size in 1st packet */
        num_packets = 2;
    }

read_data_write_again:
    /* Write command/path packets */
    if (cmd_packet) {
	retval = dc240_packet_write(camera, cmd_packet, 8, 1);
        if (retval < GP_OK)
            return retval;
    }

    if (path_packet) {
	retval = dc240_packet_write(camera, path_packet, 60, 1);
        if (retval < GP_OK)
            return retval;
    }

    id = gp_context_progress_start (context, num_packets, _("Getting data..."));
    while (x < num_packets) {
read_data_read_again:
	gp_context_progress_update (context, id, x);

        /* Read the response/data */
        retval = dc240_packet_read(camera, packet, block_size+2);

	if (retval == GP_ERROR_NOT_SUPPORTED)
	    return retval;

        if ((retval == GP_ERROR) || (retval == GP_ERROR_TIMEOUT)) {
            /* ERROR reading response/data */
            if (retries++ > RETRIES) {
		gp_context_progress_stop (context, id);
                return (GP_ERROR_TIMEOUT);
	    }

            if (x==0) {
                /* If first packet didn't come, write command again */
                goto read_data_write_again;
            } else {
                /* Didn't get data packet. Send NAK for retry */
                dc240_packet_write_nak(camera);
                goto read_data_read_again;
            }
        }

	/* Validate checksum */
	check_sum = 0;
	for (i = 1; i < block_size + 1; i++) {
	    check_sum ^= packet [i];
	}
	if ( block_size > 1 && check_sum != packet [i]) {
	    dc240_packet_write_nak (camera);
	    goto read_data_read_again;
	}

        /* Check for error in command/path */
        if (packet[0] > DC240_SC_FIRST_ERROR) {
	    gp_context_progress_stop (context, id);
            return GP_ERROR;
	}

        /* Check for end of data */
        if (packet[0] == DC240_SC_COMPLETE) {
	    gp_context_progress_stop (context, id);
            return GP_OK;
	}

        /* Write packet was OK */
        if (dc240_packet_write_ack(camera) < GP_OK)
            goto read_data_read_again;

        /* Set size for folder/file list command from 1st packet */
        if (cmd_packet && (cmd_packet[0]==0x99) && (x==0)) {
            *size = (packet[1] * 256 +
                     packet[2])* 20 + 2;
            t = (float)*size / (float)(block_size);
            num_packets = (int)t;
            if (t - (float)num_packets > 0)
                num_packets++;
        }

        /* Copy in data */
        if (x == num_packets)
            num_bytes = *size - ((num_packets-1) * block_size);
        else
            num_bytes = block_size;
        gp_file_append(file, (char*)&packet[1], num_bytes);

        /* Increment packet count, reset retries */
        x++;
        retries = 0;
    }
    gp_context_progress_stop (context, id);
    /* Read in command completed */
    return dc240_wait_for_completion(camera);
}


/*
  See 5.1.37
 */
int dc240_open (Camera *camera)
{
    int retval = GP_OK;
    unsigned char *p = dc240_packet_new(0x96);

    GP_DEBUG ("dc240_open\n");

    retval = dc240_packet_write(camera, p, 8, 1);
    if (retval != GP_OK) {
	GP_DEBUG ("dc240_open: write returned %d\n", retval);
	goto fail;
    }

    retval = dc240_wait_for_completion(camera);
    if (retval < GP_OK) {
	GP_DEBUG ("dc240_open: wait returned %d\n", retval);
	goto fail;
    }

 fail:
    free(p);
    return retval;
}

int dc240_close (Camera *camera, GPContext *context)
{
    unsigned char *p = dc240_packet_new(0x97);
    int retval, size = -1;

    retval = dc240_packet_exchange(camera, NULL, p, NULL, &size, -1, context);

    free(p);
    return retval;
}

static int dc240_get_file_size (Camera *camera, const char *folder, const char *filename, int thumb, GPContext *context) {

    CameraFile *f;
    unsigned char *p1, *p2;
    int size = 256, offset;
    unsigned long int fsize;
    const unsigned char *fdata;

    offset = (thumb? 92:104);

    gp_file_new(&f);
    p1 = dc240_packet_new(0x91);
    p2 = dc240_packet_new_path(folder, filename);
    if (dc240_packet_exchange(camera, f, p1, p2, &size, 256, context) < 0)
        size = 0;
    else {
	int ret;
	ret = gp_file_get_data_and_size (f, (const char**)&fdata, &fsize);
	if (ret < GP_OK) return ret;
	if (!fdata || (fsize < 4)) return GP_ERROR;
        size = (fdata[offset]   << 24) |
               (fdata[offset+1] << 16) |
               (fdata[offset+2] << 8 ) |
               (fdata[offset+3]);
    }

    gp_file_free(f);
    free(p1);
    free(p2);

    return (size);
}

int dc240_set_speed (Camera *camera, int speed)
{
    int retval;
    unsigned char *p = dc240_packet_new(0x41);
    gp_port_settings settings;

    GP_DEBUG ("dc240_set_speed\n");

    gp_port_get_settings(camera->port, &settings);

    switch (speed) {
    case 9600:
        p[2] = 0x96;
        p[3] = 0x00;
        settings.serial.speed = 9600;
        break;
    case 19200:
        p[2] = 0x19;
        p[3] = 0x20;
        settings.serial.speed = 19200;
        break;
    case 38400:
        p[2] = 0x38;
        p[3] = 0x40;
        settings.serial.speed = 38400;
        break;
    case 57600:
        p[2] = 0x57;
        p[3] = 0x60;
        settings.serial.speed = 57600;
        break;
    case 0: /* Default */
    case 115200:
        p[2] = 0x11;
        p[3] = 0x52;
        settings.serial.speed = 115200;
        break;
    /* how well supported is 230.4?
    case 230400:
         p[2] = 0x23;
         p[3] = 0x04;
         settings.serial.speed = 230400;
         break;
    */
    default:
        retval = GP_ERROR;
	goto fail;
    }

    retval = dc240_packet_write(camera, p, 8, 1);
    if (retval != GP_OK)
	goto fail;

    retval = gp_port_set_settings (camera->port, settings);
    if (retval != GP_OK)
        goto fail;

    usleep(300 * 1000);
    retval = dc240_wait_for_completion(camera);
    if (retval != GP_OK)
	goto fail;
    /* Speed change went OK. */
 fail:
    free (p);
    return retval;
}

static struct _type_to_camera {
    uint16_t status_type;
    char * name;
} type_to_camera [] = {
    { 4, "DC210" },
    { 5, "DC240" },
    { 6, "DC280" },
    { 7, "DC5000" },
    { 8, "DC3400" },
    { 0, "Unknown" }   /* unknown type. MUST be last */
};



const char *dc240_convert_type_to_camera (uint16_t type)
{
    int i = 0;

    while (type_to_camera[i].status_type != 0) {
	if (type_to_camera[i].status_type == type)
	    return type_to_camera[i].name;
	i++;
    }
    /* not found */
    return type_to_camera[i].name;
}

const char *dc240_get_battery_status_str (uint8_t status)
{
    switch (status) {
    case 0:
	return _("OK");
	break;
    case 1:
	return _("Weak");
	break;
    case 2:
	return _("Empty");
	break;
    default:
	break;
    }
    return _("Invalid");
}

const char *dc240_get_ac_status_str (uint8_t status)
{
    switch (status) {
    case 0:
	return _("Not used");
	break;
    case 1:
	return _("In use");
	break;
    default:
	break;
    }
    return _("Invalid");
}


const char * dc240_get_memcard_status_str(uint8_t status)
{
    if (status & 0x80) {
	if ((status & 0x10) == 0) {
	    if (status & 0x08)
		return _("Card is open");
	    return _("Card is not open");
	}
	return _("Card is not formatted");
    }
    return _("No card");
}


/*
  Feed manually the structure from data.
 */
static int dc240_load_status_data_to_table (const unsigned char *fdata, DC240StatusTable *table)
{
    if (fdata [0] != 0x01)
	return GP_ERROR;
    GP_DEBUG ("Camera Type = %d, %s\n", fdata[1], dc240_convert_type_to_camera (fdata[1]));
    table->cameraType = fdata[1];
    table->fwVersInt = fdata[2];
    table->fwVersDec = fdata[3];
    GP_DEBUG ("Firmware version = %d, %d\n", fdata[2], fdata[3]);
    table->romVers32Int = fdata[4];
    table->romVers32Dec = fdata[5];
    table->romVers8Int = fdata[6];
    table->romVers8Dec = fdata[7];
    table->battStatus = fdata[8];
    table->acAdapter = fdata[9];
    table->strobeStatus = fdata[10];
    table->memCardStatus = fdata[11];
    table->videoFormat = fdata[12];
    table->quickViewMode = fdata[13]; /* DC280 */
    table->numPict = be16atoh(&fdata[14]);
    strncpy (table->volumeID, (char*)&fdata[16], 11);
    table->powerSave = fdata[27]; /* DC280 */
    strncpy (table->cameraID, (char*)&fdata[28], 32);
    table->remPictLow = be16atoh(&fdata[60]);
    table->remPictMed = be16atoh(&fdata[62]);
    table->remPictHigh = be16atoh(&fdata[64]);
    table->totalPictTaken = be16atoh(&fdata[66]);
    table->totalStrobeFired = be16atoh(&fdata[68]);
    table->langType = fdata[70]; /* DC 280 */
    table->beep = fdata[71];
    table->fileType = fdata[78];
    table->pictSize = fdata[79];
    table->imgQuality = fdata[80];
    table->ipChainDisable = fdata[81]; /* reserved on DC280. WTF is it ? */
    table->imageIncomplete = fdata[82];
    table->timerMode = fdata[83];
    table->year = be16atoh(&fdata[88]);
    table->month = fdata[90];
    table->day = fdata[91];
    table->hour = fdata[92];
    table->minute = fdata[93];
    table->second = fdata[94];
    table->tenmSec = fdata[95];
    table->strobeMode = fdata[97];
    table->exposureComp  = (fdata[98] * 100) + (fdata[99]);
    table->aeMode = fdata[100];
    table->focusMode = fdata[101];
    table->afMode = fdata[102];
    table->awbMode = fdata[103];
/* table->zoomMag =  ????? */
    table->exposureMode = fdata[129];
    table->sharpControl = fdata[131];
/* table->expTime = */
    table->fValue = (fdata[136] * 100) + (fdata[137]);
    table->imageEffect = fdata[138];
    table->dateTimeStamp = fdata[139];
    strncpy (table->borderFileName, (char*)&fdata[140], 11);
    table->exposureLock = fdata[152];
    table->isoMode = fdata[153]; /* DC280 */

    return GP_OK;
}

/*
  Return the camera status table.
  See 5.1.29
 */
int dc240_get_status (Camera *camera, DC240StatusTable *table, GPContext *context)
{
    CameraFile *file;
    unsigned char *p = dc240_packet_new(0x7F);
    int retval;
    const char *fdata;
    unsigned long int fsize;
    int size = 256;

    gp_file_new (&file);
    GP_DEBUG ("enter dc240_get_status() \n");
    retval = dc240_packet_exchange(camera, file, p, NULL, &size, 256, context);

    if (retval == GP_OK) {
	retval = gp_file_get_data_and_size (file, &fdata, &fsize);
	if (retval != GP_OK) goto exit;
	if (fsize != 256) {
	    GP_DEBUG ("wrong status packet size ! Size is %ld", fsize);
	    retval = GP_ERROR;
	    goto exit;
	}
	if (fdata [0] != 0x01) { /* see 2.6 for why 0x01 */
	    GP_DEBUG ("not a status table. Is %d", fdata [0]);
	    retval = GP_ERROR;
	    goto exit;
	}
	dc240_load_status_data_to_table ((uint8_t *)fdata, table);
    }
exit:
    gp_file_free(file);
    free (p);

    return (retval);
}

int dc240_get_directory_list (Camera *camera, CameraList *list, const char *folder,
                             unsigned char attrib, GPContext *context) {

    CameraFile *file;
    unsigned int x, y=0, z;
    int size=256;
    char buf[64];
    unsigned char *p1 = dc240_packet_new(0x99);
    unsigned char *p2 = dc240_packet_new_path(folder, NULL);
    const char *fdata;
    unsigned long int fsize;
    int ret;
    int num_of_entries = 0; /* number of entries in the listing */
    unsigned int total_size = 0; /* total useful size of the listing */

    gp_file_new(&file);
    ret = dc240_packet_exchange(camera, file, p1, p2, &size, 256, context);
    if (ret < 0) {
	gp_file_free (file);
        return ret;
    }
    free(p1);
    free(p2);

    /* Don't expect to have a fully useful buffer. */
    ret = gp_file_get_data_and_size (file, &fdata, &fsize);
    if (ret < 0) {
	gp_file_free (file);
        return ret;
    }
    if ((size < 1) || !fdata) {
	gp_file_free (file);
        return GP_ERROR;
    }

    /* numbers in DC 240 are Big-Endian. */
    /* Conversion below should be endian neutral. */
    num_of_entries = be16atoh(&fdata [0]) + 1;
    total_size = 2 + (num_of_entries * 20);
    GP_DEBUG ("number of file entries : %d, size = %ld", num_of_entries, fsize);
    if (total_size > fsize) {
        GP_DEBUG ("total_size %d > fsize %ld", total_size, fsize);
	gp_file_free (file);
        return GP_ERROR;
    }
    for (x = 2; x < total_size; x += 20) {
        if ((fdata[x] != '.') && (attrib == (unsigned char)fdata[x+11]))  {
            /* Files have attrib 0x00, Folders have attrib 0x10 */
            if (attrib == 0x00) {
                strncpy(buf, &fdata[x], 8);    /* Copy over filename */
                buf[8] = 0;                         /* NULL terminate */
                strcat(buf, ".");                   /* Append dot */
                strcat(buf, &fdata[x+8]);      /* Append extension */
		GP_DEBUG ("found file: %s", buf);
            } else {
                strncpy(buf, &fdata[x], 8);    /* Copy over folder name */
                z=0;
                while ((buf[z] != 0x20)&&(z<8))     /* Chop trailing spaces */
                    z++;
                buf[z] = 0;                         /* NULL terminate */
		GP_DEBUG ("found folder: %s", buf);
            }
            gp_list_append(list, buf, NULL);
            y++;
        }
    }
    gp_file_free(file);
    return (GP_OK);
}

int dc240_file_action (Camera *camera, int action, CameraFile *file,
                       const char *folder, const char *filename, GPContext *context) {

    int size=0, thumb=0, retval=GP_OK;
    unsigned char *cmd_packet, *path_packet;

    cmd_packet  = dc240_packet_new(action);
    path_packet = dc240_packet_new_path(folder, filename);

    switch (action) {
    case DC240_ACTION_PREVIEW:
        cmd_packet[4] = 0x02;
        thumb = 1;
	/* fallthrough */
    case DC240_ACTION_IMAGE:
        if ((size = dc240_get_file_size(camera, folder, filename, thumb, context)) < GP_OK) {
            retval = GP_ERROR;
            break;
        }
        retval = dc240_packet_exchange(camera, file, cmd_packet, path_packet, &size, HPBS, context);
        break;
    case DC240_ACTION_DELETE:
        size = -1;
        retval = dc240_packet_exchange(camera, file, cmd_packet, path_packet, &size, -1, context);
        break;
    default:
        free(cmd_packet);
        free(path_packet);
        return (GP_ERROR);
    }

    free(cmd_packet);
    free(path_packet);

    if (file)
	gp_file_set_mime_type (file, GP_MIME_JPEG);
    return (retval);
}

/*
  See 5.1.27 in the DC240 Host Spec.
  Capture a picture on the flash card and retrieve its full
  name.
  See also 5.1.19
 */
int dc240_capture (Camera *camera, CameraFilePath *path, GPContext *context)
{
    CameraFile *file;
    int size = 256;
    int ret = GP_OK;
    unsigned char *p = dc240_packet_new(0x7C);
    const char *fdata;
    unsigned long int fsize;

    /* Take the picture to Flash memory */
    ret = dc240_packet_write(camera, p, 8, 1);
    free (p);
    if (ret != GP_OK) {
	return ret;
    }

    gp_context_status (context, "Waiting for completion...");
    ret = dc240_wait_for_completion(camera);
    if (ret != GP_OK) {
	return ret;
    }

    ret = dc240_wait_for_busy_completion(camera);
    if (ret != GP_OK) {
	return ret;
    }

    /*
     get the last picture taken location (good firmware developers)
     */
    gp_file_new(&file);
    p = dc240_packet_new(0x4C); /* 5.1.19 Send last taken image name */
    ret = dc240_packet_exchange(camera, file, p, NULL, &size, 256, context);
    free(p);
    if (ret != GP_OK) {
	path->name[0] = 0;
	path->folder[0] = 0;
	gp_file_unref (file);
	return ret;
    }

    /* this part assumes that the response is of fixed size */
    /* according to the specs, this is the case since only the */
    /* numbering changes */
    gp_file_get_data_and_size (file, &fdata, &fsize);
    strncpy (path->folder, fdata, 14);
    path->folder [14] = 0;
    path->folder [0] = '/';
    path->folder [5] = '/';
    strncpy (path->name, &(fdata[15]), 13);
    path->name[13] = 0;
    gp_file_unref (file);

    return GP_OK;
}

int dc240_packet_set_size (Camera *camera, short int size) {

    unsigned char *p = dc240_packet_new(0x2A);

    p[2] = (size >> 8) & 0xFF;
    p[3] = (size     ) & 0xFF;

    if (dc240_packet_write(camera, p, 8, 1) == GP_ERROR)
            return (GP_ERROR);

    if (dc240_wait_for_completion(camera)==GP_ERROR)
            return (GP_ERROR);
    free(p);
    return (GP_OK);
}
