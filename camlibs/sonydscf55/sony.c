/* Sony DSC-F55 & MSAC-SR1 - gPhoto2 camera library
 * Copyright 2001, 2002, 2004 Raymond Penners <raymond@dotsphinx.com>
 * Copyright 2000 Mark Davies <mdavies@dial.pipex.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _DEFAULT_SOURCE

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include "sony.h"

#define GP_MODULE "sony55"

/**
 * Constants
 */
#define SONY_CONVERSE_RETRY 5

#define SONY_INVALID_CHECKSUM	0x40
#define SONY_INVALID_SEQUENCE	0x41
#define SONY_RESET_SEQUENCE	0x42
#define SONY_RESEND_PACKET	0x43

#define SONY_ESCAPE_CHAR		0x7d
#define SONY_START_CHAR		0xc0
#define SONY_END_CHAR		0xc1


static const unsigned char START_PACKET = 192;
static const unsigned char END_PACKET = 193;

static const char ESC_START_STRING[] = { 0x7d, 0xe0 };
static const char ESC_END_STRING[] = { 0x7d, 0xe1 };
static const char ESC_ESC_STRING[] = { 0x7d, 0x5d };

static const unsigned char PacketCodes[2] = { 192, 193 };


#if defined(USE_ALL_TYPES)
static unsigned char EmailImage[] =
    { 0, 2, 2, 0, 16, '/', 'M', 'S', 'S', 'O', 'N', 'Y', '/', 'I', 'M',
'C',
	'I', 'F', '1', '0', '0'
};
#endif


static unsigned char IdentString[] =
    { 0, 1, 1, 'S', 'O', 'N', 'Y', ' ', ' ', ' ', ' ', ' ' };
static unsigned char EmptyPacket[] = { 0 };	/* null packet */
static unsigned char SetTransferRate[] = { 0, 1, 3, 0 };
static unsigned char SendImageCount[] = { 0, 2, 1 };
static unsigned char StillImage[] =
    { 0, 2, 2, 0, 14, '/', 'D', 'C', 'I', 'M', '/', '1', '0', '0', 'M',
'S',
	'D', 'C', 'F'
};
static unsigned char MpegImage[] =
    { 0, 2, 2, 0, 16, '/', 'M', 'S', 'S', 'O', 'N', 'Y', '/', 'M', 'O',
'M',
	'L', '0', '0', '0', '1'
};
static unsigned char SelectImage[] = { 0, 2, 48, 0, 0, 0, 0 };
static unsigned char SendImage[] = { 0, 2, '1', 0, 1, 0, 0 };
static unsigned char SendNextImagePacket[] = { 0, 2, '1', 0 };
static unsigned char SendThumbnail[] = { 0, 2, '0', 0 };

#if 0
static unsigned char SelectCamera[] = { 0, 1, 2 };
static unsigned char DownloadComplete[] = { 0, 2, '0', 255 };
static unsigned char X10Camera[] = { 0, 1, 5 };

static unsigned char X5Camera[] = { 0, 2, 1 };
static unsigned char X13Camera[] = { 0, 2, 18 };

#endif


/*
 * This array contains the expected packet sequence code to be applied/
 * checked for.
 */
static const unsigned char sony_sequence[] =
    { 14, 0, 32, 34, 66, 68, 100, 102, 134, 136, 168, 170, 202, 204, 236,
238,
	255
};


#ifdef __linux__
static const int baud_rate = 115200;
#else
# ifdef __sun__
static const int baud_rate = 38400;
# else
static const int baud_rate = 9600;
# endif
#endif





/**
 * Returns transfer rate ID
 */
static int
sony_baud_to_id(long baud)
{
	int r;

	switch (baud) {
	case 115200:
		r = 4;
		break;
	case 57600:
		r = 3;
		break;		/* FIXME ??? */
	case 38400:
		r = 2;
		break;		/* works on sun */
	case 19200:
		r = 1;
		break;		/* works on sun */
	default:
	case 9600:
		r = 0;
		break;		/* works on sun */
	}
	return r;
}


/**
 * Reads a byte
 */
static int
sony_read_byte(Camera * camera, unsigned char *b)
{
	int n = gp_port_read(camera->port, (char *)b, 1);
	if (n != 1)
		return GP_ERROR;
	else
		return GP_OK;
}



/**
 * Returns the checksum for a packet
 */
static unsigned char
sony_packet_checksum(Packet * p)
{
	unsigned short int o = 0;
	unsigned long int sum = 0;

	sum = 0;

	while (o < p->length)
		sum += p->buffer[o++];

	return 256 - (sum & 255);
}

/**
 * Returns TRUE iff the packet is valid
 */
static int
sony_packet_validate(Camera * camera, Packet * p)
{
	unsigned char c = sony_packet_checksum(p);

	if (c != p->checksum) {
		GP_DEBUG(
				"sony_packet_validate: invalid checksum");
		return SONY_INVALID_CHECKSUM;
	}

	if (129 == p->buffer[0]) {
		GP_DEBUG(
				"sony_packet_validate: resend packet");
		return SONY_RESEND_PACKET;
	}

	if (sony_sequence[camera->pl->sequence_id] != p->buffer[0]) {
		GP_DEBUG(
				"sony_packet_validate: invalid sequence");
		return SONY_INVALID_SEQUENCE;
	}

	return GP_OK;
}

/**
 * Constructs a packet.
 */
static int
sony_packet_make(Camera * camera, Packet * p, unsigned char *buffer,
		 unsigned short int length)
{
	p->length = 0;

	while (length--)
		p->buffer[p->length++] = *(buffer++);

	if (255 == sony_sequence[++(camera->pl->sequence_id)])
		camera->pl->sequence_id = 0;

	p->buffer[0] = sony_sequence[camera->pl->sequence_id++];

	if (255 == sony_sequence[camera->pl->sequence_id])
		camera->pl->sequence_id = 0;

	p->checksum = sony_packet_checksum(p);

	return TRUE;
}

/**
 * Reads a packet.
 */
static int sony_packet_read(Camera * camera, Packet * pack)
{
	unsigned int n;
	unsigned char byte = 0;
	static Packet p;

	GP_DEBUG(
			"sony_packet_read()");
	p.length = 0;

	for (n = 0; n < 2; n++) {
		for (byte = 0; byte != (unsigned char) PacketCodes[n];) {
			if (sony_read_byte(camera, &byte) == GP_ERROR) {
				return FALSE;
			}

			if (n > 0) {
				if (SONY_ESCAPE_CHAR == byte) {
					unsigned char extra;

					sony_read_byte(camera, &extra);

					switch (extra) {
					case 1:
					case 7:
					case 0xe1:
					case 0xe0:
						extra &= 0xcf;
						p.buffer[p.length++] =
						    extra;
						continue;
					case 0x5d:
						p.buffer[p.length++] =
						    byte;
						continue;
					default:
						p.buffer[p.length++] =
						    byte;
						p.buffer[p.length++] =
						    extra;
						continue;
					}
				} else
					p.buffer[p.length++] = byte;
			}
		}
	}

	p.length -= 2;
	p.checksum = p.buffer[p.length];

	memcpy(pack, &p, sizeof(Packet));
	return TRUE;
}

/**
 * Sends a packet
 */
static int
sony_packet_write(Camera * camera, Packet * p)
{
	unsigned short int count;
	int rc;

	GP_DEBUG(
			"sony_packet_write()");

        /**
	 * This usleep is necessary when using (relatively) fast CPUs,
	 * as P4 2Ghz. Without it gphoto2 stops with a segmentation fault
	 * and the --debug message sony_packet_validate: invalid sequence
	 * Failed to read packet during transfer. The delay is somewhat
	 * arbitrary. (alberto-g@(remove_me)inventati.org)
	 **/
	usleep(10000);

	rc = gp_port_write(camera->port, (char *)&START_PACKET, 1);

	p->buffer[p->length] = p->checksum;

	for (count = 0; count < p->length + 1 && rc != GP_ERROR; count++) {
		switch ((unsigned char) p->buffer[count]) {
		case SONY_ESCAPE_CHAR:
			rc = gp_port_write(camera->port, ESC_ESC_STRING, 2);
			break;

		case SONY_START_CHAR:
			rc = gp_port_write(camera->port, ESC_START_STRING, 2);
			break;

		case SONY_END_CHAR:
			rc = gp_port_write(camera->port, ESC_END_STRING, 2);
			break;

		default:
			rc = gp_port_write(camera->port,
					   (char *) &p->buffer[count], 1);
			break;
		}
	}
	if (rc != GP_ERROR)
		rc = gp_port_write(camera->port, (char *) &END_PACKET, 1);
	return rc;
}

/**
 * Communicates packets
 */
static int
sony_converse(Camera * camera, Packet * out, unsigned char *str, int len)
{
	Packet ps;
	char old_sequence = 33;
	int sequence_count = 0;
	int invalid_sequence = 0;
	int count;
	int rc;

	GP_DEBUG( "sony_converse()");
	sony_packet_make(camera, &ps, str, len);

	for (count = 0; count < SONY_CONVERSE_RETRY; count++) {
		rc = sony_packet_write(camera, &ps);

		if (rc == GP_OK) {
			if (sony_packet_read(camera, out)) {
				switch (sony_packet_validate(camera, out)) {
				case SONY_INVALID_CHECKSUM:
					if (invalid_sequence) {
						sony_packet_make(camera,
								 &ps, str,
								 len);
						break;
					}

					GP_DEBUG ("Checksum invalid");
					ps.buffer[0] = 129;
					ps.checksum =
					    sony_packet_checksum(&ps);
					break;

				case SONY_INVALID_SEQUENCE:
					if (camera->pl->model
					    != SONY_MODEL_DSC_F55) {
						invalid_sequence = 1;
						sony_packet_make(camera,
								 &ps, str,
								 len);
						break;
					}

					if (old_sequence == out->buffer[0])
						sequence_count++;
					else if (0 == sequence_count)
						old_sequence =
						    out->buffer[0];

					if (sequence_count == 4) {
						GP_DEBUG ("Attempting to reset sequence id - image may be corrupt.");
						camera->pl->sequence_id = 0;

						while (sony_sequence
						       [camera->pl->
							sequence_id] !=
						       old_sequence)
							camera->pl->
							    sequence_id++;

						return GP_OK;
					}
					GP_DEBUG ("Invalid Sequence");
					ps.buffer[0] = 129;
					ps.checksum =
					    sony_packet_checksum(&ps);
					break;

				case SONY_RESET_SEQUENCE:
					camera->pl->sequence_id = 0;
					return GP_OK;

				case SONY_RESEND_PACKET:
					GP_DEBUG ("Resending Packet");
					break;

				case GP_OK:
					return GP_OK;

				default:
					GP_DEBUG ("Unknown Error");
					break;
				}
			} else {
				/* printf("Incomplete packet\n"); */
				ps.buffer[0] = 129;
				ps.checksum = sony_packet_checksum(&ps);
			}
		}
	}

	GP_DEBUG(
			"Failed to read packet during transfer.");

	return GP_ERROR;
}


/**
 * Sets baud rate
 */
static int
sony_baud_port_set(Camera * camera, long baud)
{
	gp_port_settings settings;

	gp_port_get_settings(camera->port, &settings);
	settings.serial.speed = baud;
	gp_port_set_settings(camera->port, settings);

	usleep(70000);

	return GP_OK;
}

/**
 * Sets baud rate
 */
static int
sony_baud_set(Camera * camera, long baud)
{
	Packet dp;
	int rc;

	GP_DEBUG( "sony_baud_set(%ld)",
			baud);

	if (camera->pl->current_baud_rate != baud) {
		/* FIXME */
		SetTransferRate[3] = sony_baud_to_id(baud);

		rc = sony_converse(camera, &dp, SetTransferRate, 4);
		if (rc == GP_OK) {
			sony_baud_port_set(camera, baud);
			rc = sony_converse(camera, &dp, EmptyPacket, 1);
			if (rc == GP_OK) {
				camera->pl->current_baud_rate = baud;
			}
			usleep(100000);	/* 50000 was good too, jw */

		}
	}
	else {
		rc = GP_OK;
	}
	return rc;
}

/**
 * Port initialization
 */
static int
sony_init_port (Camera *camera)
{
	gp_port_settings settings;
	int rc;

	rc = gp_port_set_timeout (camera->port, 5000);
	if (rc == GP_OK) {

		gp_port_get_settings(camera->port, &settings);

		settings.serial.speed = 9600;
		settings.serial.bits = 8;
		settings.serial.parity = 0;
		settings.serial.stopbits = 1;

		rc = gp_port_set_settings(camera->port, settings);
		if (rc == GP_OK) {
			rc = gp_port_flush(camera->port, 0);
		}
	}
	return rc;
}

/**
 * Establish first contact (remember the prime directive? :)
 */
static int
sony_init_first_contact (Camera *camera)
{
	int count = 0;
	Packet dp;
	int rc = GP_ERROR;

	for (count = 0; count < 3; count++) {
		camera->pl->sequence_id = 0;

		rc = sony_converse(camera, &dp, IdentString, 12);
		if (rc == GP_OK) {
			GP_DEBUG(
					"Init OK");
			break;
		}
		usleep(2000);
		GP_DEBUG(
				"Init - Fail %u", count + 1);
	}
	return rc;
}

/**
 * Device supports MPEG?
 */
static int
sony_is_mpeg_supported (Camera * camera)
{
	return camera->pl->model == SONY_MODEL_DSC_F55;
}

/**
 * Initialize camera
 */
int
sony_init (Camera * camera, SonyModel model)
{
	int rc;
	camera->pl->model = model;
	camera->pl->current_baud_rate = -1;
	camera->pl->current_mpeg_mode = -1;

	rc = sony_init_port (camera);
	if (rc == GP_OK)
		rc = sony_init_first_contact (camera);

	return rc;
}

/**
 * Reset the camera sequence count and baud rate.
 */
int
sony_exit(Camera * camera)
{
	Packet dp;
	int rc = GP_ERROR;

	rc = sony_baud_set(camera, 9600);
	while (rc == GP_OK && camera->pl->sequence_id > 0) {
		rc = sony_converse(camera, &dp, EmptyPacket, 1);
	}

	return rc;
}


static int
sony_set_file_mode(Camera * camera, SonyFileType file_type)
{
	int rc = GP_OK;
	Packet dp;
	if (file_type == SONY_FILE_MPEG) {
		if (camera->pl->current_mpeg_mode != 1) {
			rc = sony_converse(camera, &dp, MpegImage, 21);
			if (rc == GP_OK) {
				camera->pl->current_mpeg_mode = 1;
			}
		}
	}
	else {
		if (camera->pl->current_mpeg_mode != 0) {
			rc = sony_converse(camera, &dp, StillImage, 19);
			if (rc == GP_OK) {
				camera->pl->current_mpeg_mode = 0;
			}
		}
	}
	return rc;
}


/**
 * Return count of images taken.
 */
int
sony_file_count(Camera * camera, SonyFileType file_type, int *count)
{
	Packet dp;
	int rc, nr;

	GP_DEBUG( "sony_file_count()");
	if (file_type == SONY_FILE_MPEG && (! sony_is_mpeg_supported(camera))) {
		*count = 0;
		return GP_OK;
	}
	*count = -1;
	rc = sony_converse(camera, &dp, SetTransferRate, 4);
	if (rc != GP_OK)
		return rc;
	rc = sony_set_file_mode(camera, file_type);
	if (rc != GP_OK)
		return rc;
	rc = sony_converse(camera, &dp, SendImageCount, 3);
	if (rc != GP_OK)
		return rc;
	nr = dp.buffer[5] | (dp.buffer[4]<<8);
	GP_DEBUG ("count = %d", nr);
	*count = nr;
	return GP_OK;
}



/**
 * Fetches file name.
 */
int
sony_file_name_get(Camera *camera, int imageid, SonyFileType mpeg, char buf[13])
{
	Packet dp;
	int rc;

	GP_DEBUG( "sony_file_name_get()");
	rc = sony_set_file_mode(camera, mpeg);
	if (rc != GP_OK)
		return rc;
	sony_baud_set(camera, baud_rate);
	/* FIXME: Not nice, changing global data like this. */
	SelectImage[3] = (imageid >> 8);
	SelectImage[4] = imageid & 0xff;
	rc = sony_converse(camera, &dp, SelectImage, 7);
	if (rc != GP_OK)
		return rc;
	memcpy(buf, &dp.buffer[5], 8);
	buf[8] = '.';
	memcpy(buf+9, &dp.buffer[5+8], 3);
	buf[12] = 0;
	return rc;
}


/**
 * Is it an MPEG file?
 */
int
sony_is_mpeg_file_name(const char * file_name)
{
	return strncmp(file_name,"MOV",3)==0;
}



/**
 * Fetches an image.
 */
static int
sony_file_get(Camera * camera, int imageid, int file_type,
	      CameraFile * file, GPContext *context)
{
	int sc;			/* count of bytes to skip at start of packet */
	Packet dp;
	int rc;
	char buffer[128];

	GP_DEBUG( "sony_file_get()");

	rc = sony_set_file_mode(camera, file_type);
	if (rc != GP_OK) {
		return rc;
	}

	if (gp_context_cancel(context) == GP_CONTEXT_FEEDBACK_CANCEL) {
		return GP_ERROR_CANCEL;
	}

	rc = gp_file_clean(file);
	if (rc == GP_OK) {
		gp_file_set_mime_type (file, GP_MIME_JPEG);
		/* The file name we specify here is not important,
		 * since the calling function, gp_filesystem_get_file_impl,
		 * overwrites it with the correct one.
		 */
		sprintf(buffer, SONY_FILE_NAME_FMT, imageid);

		sony_baud_set(camera, baud_rate);

		rc = sony_set_file_mode(camera, file_type);
		if (rc == GP_OK) {
			if (file_type == SONY_FILE_THUMBNAIL) {
				sc = 0x247;
				SelectImage[3] = (imageid >> 8);
				SelectImage[4] = imageid & 0xff;
				sony_converse(camera, &dp, SelectImage, 7);

				if (camera->pl->model
				    != SONY_MODEL_DSC_F55) {
					gp_file_append(file,
						       "\xff\xd8\xff", 3);
				}

				for (;;) {
					if (gp_context_cancel(context)
					    == GP_CONTEXT_FEEDBACK_CANCEL) {

						rc = GP_ERROR_CANCEL;
						break;
					}
					gp_context_idle(context);

					sony_converse(camera, &dp,
						      SendThumbnail, 4);

					gp_file_append(file,
						       (char *) dp.buffer +
						       sc, dp.length - sc);
					sc = 7;

					if (3 == dp.buffer[4])
						break;
				}
			} else {
				sc = 11;

				SendImage[3] = (imageid >> 8);
				SendImage[4] = imageid;
				sony_converse(camera, &dp, SendImage, 7);

				for (;;) {
					if (gp_context_cancel(context)
					    == GP_CONTEXT_FEEDBACK_CANCEL) {

						rc = GP_ERROR_CANCEL;
						break;
					}
					gp_context_idle(context);

					gp_file_append(file,
						       (char *) dp.buffer +
						       sc, dp.length - sc);

					if (file_type == SONY_FILE_EXIF) {
						const char *fdata;
						unsigned long fsize;

						gp_file_get_data_and_size
							(file, &fdata, &fsize);
						/* FIXME: 4096 is a rather */
						/* arbitrary value */
						if (fsize > 4096)
							break;
					}

					sc = 7;

					if (3 == dp.buffer[4])
						break;

					sony_converse(camera, &dp,
						      SendNextImagePacket,
						      4);
				}
			}
		}
		sony_baud_set(camera, 9600);

		if (rc != GP_OK) {
			gp_file_clean(file);
		}
	}
	return rc;
}

/**
 * Fetches a thumbnail image.
 */
int
sony_thumbnail_get(Camera * camera, int imageid, CameraFile * file, GPContext *context)
{
	return sony_file_get(camera, imageid, SONY_FILE_THUMBNAIL, file, context);
}

/**
 * Fetches an image.
 */
int
sony_image_get(Camera * camera, int imageid, CameraFile * file, GPContext *context)
{
	return sony_file_get(camera, imageid, SONY_FILE_IMAGE, file, context);
}

/**
 * Fetches EXIF information.
 */
int
sony_exif_get(Camera * camera, int imageid, CameraFile * file, GPContext *context)
{
	return sony_file_get(camera, imageid, SONY_FILE_EXIF, file, context);
}

/**
 * Fetches an Mpeg.
 */
int
sony_mpeg_get(Camera * camera, int imageid, CameraFile * file, GPContext *context)
{
	return sony_file_get(camera, imageid, SONY_FILE_MPEG, file, context);
}


/**
 * Fetches image details.
 */
int
sony_image_info(Camera * camera, int imageid, SonyFileType file_type,
		CameraFileInfo * info, GPContext *context)
{
	unsigned int l = 0;
	int rc;
	Packet dp;

	rc = sony_set_file_mode(camera, file_type);
	if (rc != GP_OK) {
		return rc;
	}

	if (gp_context_cancel(context) == GP_CONTEXT_FEEDBACK_CANCEL) {
		return GP_ERROR_CANCEL;
	}

	SelectImage[3] = (imageid >> 8);
	SelectImage[4] =  imageid & 0xff;
	rc = sony_converse(camera, &dp, SelectImage, 7);
	if (rc == GP_OK) {
		l = (l << 8) | dp.buffer[16];
		l = (l << 8) | dp.buffer[17];
		l = (l << 8) | dp.buffer[18];
		l = (l << 8) | dp.buffer[19];

		info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
		info->file.size = l;

		info->preview.fields = GP_FILE_INFO_TYPE;

		if (file_type == SONY_FILE_MPEG) {
			strcpy (info->file.type, GP_MIME_AVI);
		}
		else {
			strcpy (info->file.type, GP_MIME_JPEG);
		}
	}
	return rc;
}



/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
