/* pdc640.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sourceforge.net>
 * Copyright 2002 Marcus Meissner <marcus@jet.franken.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>
#include <bayer.h>

#include "jd350e.h"
#include "dlink350f.h"

#define GP_MODULE "pdc640"

#define PDC640_PING  "\x01"
#define PDC640_SPEED "\x69\x0b"

#define PDC640_MAXTRIES 3

#define CHECK_RESULT(result) {int __r = (result); if (__r < 0) return (__r);}

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


typedef enum{
	pdc640,
	jd350e,
	dlink350f
} Model;

typedef int postproc_func(int,int,unsigned char*);

struct _CameraPrivateLibrary{
	Model			model;
	BayerTile		bayer_tile;
	postproc_func*	postprocessor;
	char*			filespec;
};

static int flip_vertical (int width, int height, unsigned char *rgb);
static int flip_both     (int width, int height, unsigned char *rgb);

static struct {
	const char*				model;
	int vendor,product;
	struct _CameraPrivateLibrary	pl;
} models[] = {
	{"Polaroid Fun Flash 640", 0, 0, {
		 	pdc640,
			BAYER_TILE_RGGB,
			NULL, /* add postprocessor here! */
			"pdc640%04i.ppm"
		}
	},
	{"Novatech Digital Camera CC30", 0, 0, {
		 	pdc640,
			BAYER_TILE_RGGB,
			NULL, /* add postprocessor here! */
			"pdc640%04i.ppm"
		}
	},
	{"Jenoptik JD350 entrance", 0x5da, 0x1006, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&jd350e_postprocessing,
			"jd350e%04i.ppm"
		}
	},
	{"Jenoptik JD350 video", 0xd96, 0x0000, {
		 	jd350e,
			BAYER_TILE_BGGR,
			NULL /*&jd350e_postprocessing*/,
			"jd350v%04i.ppm"
		}
	},
	{"ScanHex SX-35a", 0x797, 0x8901, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&jd350e_postprocessing,
			"jd350v%04i.ppm"
		}
	},
	{"ScanHex SX-35b", 0x797, 0x8909, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&jd350e_postprocessing,
			"jd350v%04i.ppm"
		}
	},
	{"ScanHex SX-35c", 0x797, 0x8911, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&jd350e_postprocessing,
			"jd350v%04i.ppm"
		}
	},
	{"ScanHex SX-35d", 0x84d, 0x1001, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&jd350e_postprocessing,
			"jd350v%04i.ppm"
		}
	},
	{"Typhoon StyloCam", 0x797, 0x801a, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&flip_vertical,
			"stylo%04i.ppm"
		}
	},
	{"Trust PowerC@m 350FS", 0x6d6, 0x002e, {
		 	jd350e,
			BAYER_TILE_RGGB,
			&trust350fs_postprocessing,
			"trust%04i.ppm"
		}
	},
	{"Trust PowerC@m 350FT", 0x6d6, 0x002d, {
			jd350e,
			BAYER_TILE_RGGB,
			&trust350fs_postprocessing,
			"trust%04i.ppm"
		}
	},
	{"Clever CAM 360", 0x797, 0x8001, {
			jd350e,
			BAYER_TILE_BGGR,
			&jd350e_postprocessing_and_flip,
			"scope%04i.ppm"
		}
	},
	/* http://www.meade.com/sportsoptics/catalog/captureview/index.html */
	{"GrandTek ScopeCam", 0x797, 0x801c, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&jd350e_postprocessing_and_flip,
			"scope%04i.ppm"
		}
	},
	/* http://www.sipixdigital.com/cameras/stylecam/ */
	/* Ids glanced from downloaded driver / .inf file. */
	{"SiPix Stylecam", 0xd64, 0x1001, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&flip_both,
			"style%04i.ppm"
		}
	},
	/* http://www.umax.de/digicam/AstraPix320S.htm */
	/* reportedly has same ids as SiPix StyleCam and also
	 * looks identical. */
	{"UMAX AstraPix 320s", 0xd64, 0x1001, {
		 	jd350e,
			BAYER_TILE_BGGR,
			&flip_both,
			"astra%04i.ppm"
		}
	},

	/* http://www.dlink.com/products/usb/dsc350/ */
	/* ids from driver download */
	{"D-Link DSC 350F", 0xd64, 0x1021, {
		 	dlink350f,
			BAYER_TILE_BGGR,
			&dlink_dsc350f_postprocessing_and_flip_both,
			"dlink%04i.ppm"
		}
	},
	{NULL,}
};

static int
pdc640_read_packet (GPPort *port, unsigned char *buf, int buf_size)
{
	int i;
	char checksum, c;

	/* Calculate the checksum */
	for (i = 0, checksum = 0; i < buf_size; i++)
		buf[i] = 0;

	/* Read the packet */
	CHECK_RESULT (gp_port_read (port, (char*)buf, buf_size));

	/* Calculate the checksum */
	for (i = 0, checksum = 0; i < buf_size; i++)
		checksum += buf[i];

	/* Read the checksum */
	CHECK_RESULT (gp_port_read (port, &c, 1));
	GP_DEBUG ("Checksum: %d calculated, %d received", checksum, c);
	if (checksum != c)
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}

static int
pdc640_transmit (GPPort *port, unsigned char *cmd, int cmd_size,
				   unsigned char *buf, int buf_size)
{
	int r, tries;

	if (port->type == GP_PORT_USB) {
		unsigned char xbuf[64];
		unsigned char xcmd[4];
		int checksum;

		/* USB has always just 3 bytes + 1 byte checksum */
		memset(xcmd,0,4);memcpy(xcmd,cmd,cmd_size);
		checksum = (xcmd[0]^0x34)+(xcmd[1]^0xcb)+0x14f+(xcmd[2]^0x67);
		xcmd[3] = checksum & 0xff;

		/* Wait until we get back the echo of the command. */
		r = gp_port_usb_msg_read( port, 0x10, xcmd[0]|(xcmd[1]<<8),xcmd[2]|(xcmd[3]<<8),(char*)xbuf,sizeof(xbuf));
			/* Sometimes we want to read here, sometimes not.
		if (r < GP_OK)
			return r;
			 */
		if (buf && buf_size) {
			/* bulk read things. The JD350v can read in 1 block,
			 * the Stylocam returns only 64byte blocks here.
			 */
			int curr = 0, readsize = (buf_size + 63) & ~63;
			while (curr < readsize) {
				r = gp_port_read( port, (char*)buf + curr, readsize - curr);
				if (r < GP_OK) break;
				curr += r;
			}
		}
		return r;
	} else {
		char c;

		/* In event of a checksum or timing failure, retry */
		for (tries = 0; tries < PDC640_MAXTRIES; tries++) {
			/*
			 * The first byte returned is always the same as the first byte
			 * of the command.
			 */
			CHECK_RESULT (gp_port_write (port, (char*)cmd, cmd_size));
			r = gp_port_read (port, &c, 1);
			if ((r < 0) || (c != cmd[0]))
				continue;

			if (buf) {
				r = pdc640_read_packet (port, buf, buf_size);
				if (r < 0)
					continue;
			}

			return (GP_OK);
		}
		return (GP_ERROR_CORRUPTED_DATA);
	}
}


static int
pdc640_transmit_pic (GPPort *port, char cmd, int width, int thumbnail,
		     unsigned char *buf, int buf_size)
{
	unsigned char cmd1[] = {0x61, 0x00};
	unsigned char cmd2[] = {0x15, 0x00, 0x00, 0x00, 0x00};

	/* First send the command ... */
	cmd1[1] = cmd;
	CHECK_RESULT (pdc640_transmit (port, cmd1, 2, NULL, 0));

	if (port->type == GP_PORT_USB) {
		cmd2[1] = (buf_size + 63) >> 6;
		cmd2[2] = ((buf_size + 63) >> 6) >> 8;
		return pdc640_transmit (port, cmd2, 4, buf, buf_size);
	} else {
		int i, packet_size, result, size, ofs;
		unsigned char *data;

		/* Set how many scanlines worth of data to get at once */
		cmd2[4] = 0x06;

		/* Packet size is a multiple of the image width */
		packet_size = width * cmd2[4];

		/* Allocate memory to temporarily store received data */
		data = malloc (packet_size);
		if (!data)
			return (GP_ERROR_NO_MEMORY);

		/* Now get the packets */
		ofs = 0;
		result = GP_OK;
		for (i = 0; i < buf_size; i += packet_size) {
			/* Read the packet */
			result = pdc640_transmit (port, cmd2, 5,
						data, packet_size);
			if (result < 0)
				break;

			/* Copy from temp buffer -> actual */
			size = packet_size;
			if (size > buf_size - i)
				size = buf_size - i;
			memcpy(buf + i, data, size);

			/* Move to next offset */
			ofs += cmd2[4];
			cmd2[2] = ofs & 0xFF;
			cmd2[1] = (ofs >> 8) & 0xFF;
		}
		free (data);
		return (result);
	}
}

static int
pdc640_transmit_packet (GPPort *port, char cmd, unsigned char *buf, int buf_size) {
	unsigned char cmd1[] = {0x61, 0x00};
	/* Send the command and get the packet */
	cmd1[1] = cmd;
	CHECK_RESULT (pdc640_transmit (port, cmd1, 2, NULL, 0));

	if (port->type == GP_PORT_USB) {
		unsigned char cmd2[] = {0x15, 0x00, 0x00, 0x00};

		cmd2[1] = ((buf_size+63)/64) & 0xff;
		cmd2[2] = (((buf_size+63)/64) >> 8) & 0xff;
		return pdc640_transmit (port, cmd2, 4, buf, buf_size);
	} else {
		unsigned char cmd2[] = {0x15, 0x00, 0x00, 0x00, 0x01};

		return pdc640_transmit (port, cmd2, 5, buf, buf_size);
	}
}


static int
pdc640_ping_low (GPPort *port)
{
	unsigned char cmd[] = {0x01};

	CHECK_RESULT (pdc640_transmit (port, cmd, 1, NULL, 0));

	return (GP_OK);
}

#if 0
static int
pdc640_push_button (GPPort *port)
{
	char cmd[] = {0xfd};

	CHECK_RESULT (pdc640_transmit (port, cmd, 1, NULL, 0));

	return (GP_OK);
}
#endif

static int
pdc640_ping_high (GPPort *port)
{
	unsigned char cmd[] = {0x41};

	CHECK_RESULT (pdc640_transmit (port, cmd, 1, NULL, 0));

	return (GP_OK);
}

static int
pdc640_speed (GPPort *port, int speed)
{
	unsigned char cmd[] = {0x69, 0x00};

	cmd[1] = (speed / 9600) - 1;
	CHECK_RESULT (pdc640_transmit (port, cmd, 2, NULL, 0));

	return (GP_OK);
}

#if 0
/* read version I think */
static int
pdc640_unknown5 (GPPort *port)
{
	char cmd[] = {0x05};
	char buf[3];

	CHECK_RESULT (pdc640_transmit (port, cmd, 1, buf, 3));
	if ((buf[0] != 0x33) || (buf[1] != 0x02) || (buf[2] != 0x35))
		return (GP_ERROR_CORRUPTED_DATA);

	return (GP_OK);
}
#endif

#if 0
/* get calibration param */
static int
pdc640_unknown20 (GPPort* port)
{
	unsigned char buf[128];

	CHECK_RESULT (pdc640_transmit_packet (port, 0x20, buf, 128));

	return (GP_OK);
}
#endif

static int
pdc640_caminfo (GPPort *port, int *numpic)
{
	unsigned char buf[1280];

	CHECK_RESULT (pdc640_transmit_packet (port, 0x40, buf, 1280));
	*numpic = buf[2]; /* thats the only useful info :( */
	return (GP_OK);
}

static int
pdc640_setpic (GPPort *port, char n)
{
	unsigned char cmd[2] = {0xf6, 0x00};

	cmd[1] = n;
	if (port->type == GP_PORT_USB) {
		/* USB does not like a bulkread afterwards */
		return pdc640_transmit (port, cmd, 2, NULL, 0);
	} else {
		unsigned char buf[8];

		return pdc640_transmit (port, cmd, 2, buf, 7);
	}
}

static int
pdc640_picinfo (GPPort *port, char n,
		int *size_pic,   int *width_pic,   int *height_pic,
		int *size_thumb, int *width_thumb, int *height_thumb,
		int *compression_type )
{
	unsigned char buf[64];

	CHECK_RESULT (pdc640_setpic (port, n));
	CHECK_RESULT (pdc640_transmit_packet (port, 0x80, buf, 32));

	/* Check image number matches */
	if (buf[0] != n)
		return (GP_ERROR_CORRUPTED_DATA);

	/* Picture size, width and height */
	*size_pic   = buf[2] | (buf[3] << 8) | (buf[4] << 16);
	*width_pic  = buf[5] | (buf[6] << 8);
	*height_pic = buf[7] | (buf[8] << 8);

	/* Compression Type */
	*compression_type = buf[9];

	*size_thumb   = buf[25] | (buf[26] << 8) | (buf[27] << 16);

	*width_thumb  = buf[28] | (buf[29] << 8);
	*height_thumb  = buf[30] | (buf[31] << 8);

	/* Even though it should the be the correct size, the Typhoon
	 * Stylocam returns junk in there. So calculate for ourselves.
	 */
	if (*size_thumb > *width_thumb  * *height_thumb)
		*size_thumb = *width_thumb  * *height_thumb;
	return (GP_OK);
}

static int
pdc640_processtn (int width, int height, unsigned char **data, int size) {
	unsigned char *newdata;
	int y;

	/* Sanity checks */
	if ((data == NULL) || (size < width * height))
		return (GP_ERROR_CORRUPTED_DATA);

	/* Allocate a new buffer */
	newdata = malloc(size);
	if (!newdata)
		return (GP_ERROR_NO_MEMORY);

	/* Flip the thumbnail */
	for (y = 0; y < height; y++) {
		memcpy(&newdata[(height - y - 1) * width],
			&((*data)[y * width]), width);
	}

	/* Set new buffer */
	free (*data);
	*data = newdata;

	return (GP_OK);
}

static int
pdc640_getbit (unsigned char *data, int *ofs, int size, int *bit) {
	static char c;
	int b;

	/* Check if next byte required */
	if (*bit == 0) {
		if (*ofs >= size)
			return (-1);

		c = data[*ofs];
		(*ofs)++;
	}

	/* Get current bit value */
	b = (c >> *bit) & 1;

	/* Then move onto the next bit */
	(*bit)++;
	if (*bit >= 8)
		*bit = 0;

	return (b);
}

static int
pdc640_deltadecode (int width, int height, unsigned char **rawdata, int *rawsize)
{
	char col1, col2;
	unsigned char *data;
	int rawofs, x, y, ofs, bit, ones;
	int size;
	int e, d, o, val;

	GP_DEBUG ("pdc640_deltacode ()");

	/* Create a buffer to store RGB data in */
	size = width * height;
	data = malloc (size * sizeof (char));
	if (!data)
		return (GP_ERROR_NO_MEMORY);

	/* Delta decode scanline by scanline */
	rawofs = 0;
	for (y = height-1; y >= 0; y--) {
		/* Word alignment */
		if (rawofs & 1)
			rawofs++;

		/* Sanity check */
		if (rawofs >= *rawsize) {
			free (data);
			return (GP_ERROR_CORRUPTED_DATA);
		}

		/* Offset into the uncompressed data */
		ofs = y * width;

		/* Get the first two pixel values */
		col1 = (*rawdata)[rawofs];
		col2 = (*rawdata)[rawofs + 1];
		rawofs += 2;
		data[ofs + 0] = col1 << 1;
		data[ofs + 1] = col2 << 1;

		/* Work out the remaining pixels */
		bit = 0;
		for (x = 2; x < width; x++) {
			/* Count number of ones */
			ones = 0;
			while (pdc640_getbit(*rawdata, &rawofs, *rawsize, &bit) == 1)
				ones++;

			/*
			 * Get the delta value
			 * (size dictated by number of ones)
			 */
			val = 0;
			for (o = 0, e = 0, d = 1; o < ones; o++, d <<= 1) {
				e = pdc640_getbit(*rawdata, &rawofs, *rawsize, &bit);
				if (e == 1) val += d;
			}
			if (e == 0) val += 1 - d; /* adjust for negatives */

			/* Adjust the corresponding pixel value */
			if (x & 1)
				val = (col2 += val);
			else
				val = (col1 += val);

			data[ofs + x] = val << 1;
		}
	}

	/* Set new buffer */
	free (*rawdata);
	*rawdata = data;
	*rawsize = size;

	return (GP_OK);
}

static int
pdc640_getpic (Camera *camera, int n, int thumbnail, int justraw,
		unsigned char **data, int *size)
{
	char cmd, ppmheader[100];
	int size_pic, width_pic, height_pic;
	int size_thumb, width_thumb, height_thumb;
	int height, width, outsize, result, pmmhdr_len;
	int compression_type;
	unsigned char *outdata;

	/* Get the size of the picture */
	CHECK_RESULT (pdc640_picinfo (camera->port, n,
				&size_pic,   &width_pic,   &height_pic,
				&size_thumb, &width_thumb, &height_thumb,
				&compression_type));

	/* Evaluate parameters */
	if (thumbnail) {
		GP_DEBUG ("Size %d, width %d, height %d, comptype %d",
			size_thumb, width_thumb, height_thumb,
			(compression_type>>2) & 3);

		*size = size_thumb;
		width = width_thumb;
		height = height_thumb;
		if ((compression_type >> 2) & 3)
			cmd = 0x02;
		else
			cmd = 0x03;
	} else {
		GP_DEBUG ("Size %d, width %d, height %d, comptype %d",
			size_pic, width_pic, height_pic, compression_type & 3);

		*size = size_pic;
		width = width_pic;
		height = height_pic;
		switch (compression_type & 3) {
		case 1:
		case 2:	cmd = 0x10;	/* delta compressed */
			break;
		case 0:	cmd = 0x00;	/* uncompressed */
			break;
		default:		/* unknown compression type */
			GP_DEBUG ("Unknown compression type %d", compression_type & 3);
			return (GP_ERROR_CORRUPTED_DATA);
		}
	}

	/* Sanity check */
	if ((*size <= 0) || (width <= 0) || (height <= 0))
		return (GP_ERROR_CORRUPTED_DATA);

	/* Allocate the memory, including 64byte align */
	*data = malloc ((*size + 64) * sizeof (char));
	if (!*data)
		return (GP_ERROR_NO_MEMORY);

	/* Get the raw picture */
	CHECK_RESULT (pdc640_setpic (camera->port, n));
	CHECK_RESULT (pdc640_transmit_pic (camera->port, cmd, width, thumbnail,
					*data, *size));

	if (thumbnail || (compression_type == 0 )) {
		/* Process uncompressed data */
		CHECK_RESULT (pdc640_processtn (width, height,
						data, *size));
	} else if (compression_type & 3) {
		/* Image data is delta encoded so decode it */
		CHECK_RESULT (pdc640_deltadecode (width, height,
						  data, size));
	}

	/* Just wanted the raw camera data */
	if (justraw)
		return(GP_OK);

	GP_DEBUG ("Bayer decode...");
	sprintf (ppmheader, "P6\n"
			    "# CREATOR: gphoto2, pdc640/jd350e library\n"
			    "%d %d\n"
			    "255\n", width, height);

	/* Allocate memory for Interpolated ppm image */
	pmmhdr_len = strlen(ppmheader);
	outsize = width * height * 3 + pmmhdr_len + 1;
	outdata = malloc(outsize);
	if (!outdata)
		return (GP_ERROR_NO_MEMORY);

	/* Set header */
	strcpy((char *)outdata, ppmheader);

	/* Decode and interpolate the Bayer Mask */
	result = gp_bayer_decode(*data, width, height,
				&outdata[pmmhdr_len], camera->pl->bayer_tile );
	if (result < 0) {
		free (outdata);
		return (result);
	}

	/* Call a specific postprocessor if available*/
	if( camera->pl->postprocessor ){
		result = camera->pl->postprocessor(width,height,&outdata[pmmhdr_len]);
		if (result < 0) {
			free (outdata);
			return (result);
		}
	}

	/* Fix up data pointers */
	free (*data);
	*data = outdata;
	*size = outsize;

	return (GP_OK);
}

static int
pdc640_delpic (GPPort *port)
{
	unsigned char cmd[2] = {0x59, 0x01};

	CHECK_RESULT (pdc640_transmit (port, cmd, 2, NULL, 0));

	return (GP_OK);
}

static int
pdc640_takepic (GPPort *port)
{
	unsigned char cmd[2] = {0x2D, 0x00};

	CHECK_RESULT (pdc640_transmit (port, cmd, 2, NULL, 0));

	return (GP_OK);
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "pdc640/jd350e");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].model; i++) {
		memset(&a, 0, sizeof(a));
		strcpy (a.model, models[i].model);

		if (models[i].vendor) {
			a.status = GP_DRIVER_STATUS_TESTING;
			a.port = GP_PORT_USB | GP_PORT_SERIAL;
			a.usb_vendor = models[i].vendor;
			a.usb_product = models[i].product;
			a.speed[0] = 0;
		} else {
			a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
			a.port = GP_PORT_SERIAL;
			a.speed[0] = 0;
		}

		a.operations        = GP_OPERATION_CAPTURE_IMAGE;
		a.file_operations   = GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;

		CHECK_RESULT (gp_abilities_list_append (list, a));
	}
	return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	int n, size;
	unsigned char *data;

	/*
	 * Get the number of the picture from the filesystem and increment
	 * since we need a range starting with 1.
	 */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, filename,
						context));
	n++;

	/* Get the picture */
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		CHECK_RESULT (pdc640_getpic (camera, n, 0, 0, &data, &size));
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_PPM));
		break;
	case GP_FILE_TYPE_RAW:
		CHECK_RESULT (pdc640_getpic (camera, n, 0, 1, &data, &size));
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_RAW));
		break;
	case GP_FILE_TYPE_PREVIEW:
		CHECK_RESULT (pdc640_getpic (camera, n, 1, 0, &data, &size));
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_PPM));
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, (char*)data, size));

	return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	unsigned char cmd[2] = {0x59, 0x00};

	CHECK_RESULT (pdc640_transmit (camera->port, cmd, 2, NULL, 0));

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder, const char *file,
		  void *data, GPContext *context)
{
	Camera *camera = data;
	int n, count;

	/* We can only delete the last picture */
	CHECK_RESULT (n = gp_filesystem_number (camera->fs, folder, file,
						context));
	n++;

	CHECK_RESULT (pdc640_caminfo (camera->port, &count));
	if (count != n)
		return (GP_ERROR_NOT_SUPPORTED);

	CHECK_RESULT (pdc640_delpic (camera->port));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Download program for GrandTek 98x based cameras. "
		"Originally written by Chris Byrne <adapt@ihug.co.nz>, "
		"and adapted for gphoto2 by Lutz Mueller "
		"<lutz@users.sf.net>."
		"Protocol enhancements and postprocessing "
		"for Jenoptik JD350e by Michael Trawny "
		"<trawny99@users.sourceforge.net>. "
		"Bugfixes by Marcus Meissner <marcus@jet.franken.de>."));

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	int num, numpic;

	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	/* First get the current number of images */
	CHECK_RESULT (pdc640_caminfo (camera->port, &numpic));

	/* Take a picture */
	CHECK_RESULT (pdc640_takepic (camera->port));

	/* Wait a bit for the camera */
	sleep(4);

	/* Picture will be the last one in the camera */
	CHECK_RESULT (pdc640_caminfo (camera->port, &num));
	if (num <= numpic)
		return (GP_ERROR);

	/* Set the filename */
        sprintf (path->name, camera->pl->filespec, num);
        strcpy (path->folder, "/");

	CHECK_RESULT (gp_filesystem_append (camera->fs, "/", path->name,
					    context));

        return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	int n;
	Camera *camera = data;

	/* Fill the list */
	CHECK_RESULT (pdc640_caminfo (camera->port, &n));
	CHECK_RESULT (gp_list_populate (list, camera->pl->filespec, n));

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n, dummy;
	int size_pic, size_thumb;
	int width_pic, width_thumb, height_pic, height_thumb;

	CHECK_RESULT (n = gp_filesystem_number (fs, folder, file, context));
	n++;

	CHECK_RESULT (pdc640_picinfo (camera->port, n,
				&size_pic,   &width_pic,   &height_pic,
				&size_thumb, &width_thumb, &height_thumb,
				&dummy ));

	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_WIDTH |
			    GP_FILE_INFO_HEIGHT | GP_FILE_INFO_TYPE;
	info->file.width  = width_pic;
	info->file.height = height_pic;
/*
	info->file.size   = size_pic;
*/
	info->file.size   = width_pic * height_pic * 3;
	strcpy (info->file.type, GP_MIME_PPM);

	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_WIDTH |
			       GP_FILE_INFO_HEIGHT | GP_FILE_INFO_TYPE;
	info->preview.width  = width_thumb;
	info->preview.height = height_thumb;
	info->preview.size   = size_thumb * 3;
	strcpy (info->preview.type, GP_MIME_PPM);

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
};

int
camera_init (Camera *camera, GPContext *context)
{
	int result, i;
	GPPortSettings settings;
	CameraAbilities abilities;

	/*
	 * First of all, tell gphoto2 about the functions we
	 * implement (especially camera_exit so that everything
	 * gets correctly cleaned up even in case of error).
	 */
	camera->functions->about   = camera_about;
	camera->functions->capture = camera_capture;
	camera->functions->exit    = camera_exit;

	CHECK_RESULT (gp_camera_get_abilities(camera,&abilities) );
	camera->pl = 0;
	for( i=0; models[i].model; i++ ){
		if (!strcmp(models[i].model, abilities.model)) {
			GP_DEBUG ("Model: %s", abilities.model);
			camera->pl = malloc( sizeof(struct _CameraPrivateLibrary) );
			if( camera->pl ){
				*(camera->pl) = models[i].pl;
				break;
			}
			else{
				return (GP_ERROR_NO_MEMORY);
			}
		}
	}
	if( ! camera->pl ){
		return (GP_ERROR_NOT_SUPPORTED);
	}
	/* Tell the filesystem where to get lists and info */
	CHECK_RESULT (gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));

	if (camera->port->type == GP_PORT_SERIAL) {
		/* Open the port */
		CHECK_RESULT (gp_port_get_settings (camera->port, &settings));
		settings.serial.speed = 9600;
		CHECK_RESULT (gp_port_set_settings (camera->port, settings));

		/* Start with a low timeout (so we don't have to wait if already initialized) */
		CHECK_RESULT (gp_port_set_timeout (camera->port, 1000));

		/* Is the camera at 9600? */
		result = pdc640_ping_low (camera->port);
		if (result == GP_OK)
			CHECK_RESULT (pdc640_speed (camera->port, 115200));

		/* Switch to 115200 */
		settings.serial.speed = 115200;
		CHECK_RESULT (gp_port_set_settings (camera->port, settings));

		/* Is the camera at 115200? */
		CHECK_RESULT (pdc640_ping_high (camera->port));

		/* Switch to a higher timeout */
		CHECK_RESULT (gp_port_set_timeout (camera->port, 5000));
	}
	return (GP_OK);
}

static int
flip_both (int width, int height, unsigned char *rgb)
{
	unsigned char *end, c;

	end = rgb + ((width * height) * 3);
	while (rgb < end) {
		c = *rgb;
		*rgb++ = *--end;
		*end = c;
	}

	return GP_OK;
}

static int
flip_vertical (int width, int height, unsigned char* rgb) {
	int		i;
	unsigned char	*buf;

	buf = malloc(width*3);
	if (!buf) return GP_ERROR_NO_MEMORY;
	for (i=0;i<height/2;i++) {
		memcpy(buf,rgb+i*width*3,width*3);
		memcpy(rgb+i*width*3,rgb+(height-i-1)*width*3,width*3);
		memcpy(rgb+(height-i-1)*width*3,buf,width*3);
	}
	free(buf);
	return GP_OK;
}
