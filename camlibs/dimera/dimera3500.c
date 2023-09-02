/*
 * Program:	gPhoto2 driver for Mustek VDC 3500/Relisys Dimera 3500
 *
 * Note:
 * 		This software was created with
 * 		the help of proprietary information belonging
 * 		to StarDot Technologies.
 *
 * Author:
 * 		Brian Beattie http://www.beattie-home.net
 *
 * Contributors:
 *		Chuck Homic <chuck@vvisions.com>
 *			Converting raw camera images to RGB
 *		Dan Fandrich <dan@coneharvesters.com>			  Dan
 *			Information on protocol, raw image format, gphoto2 port
 *		Gregg Berg <gberg@covconn.net>				  GDB
 *			Rewrite of Dimera_Get_Full_Image()		  GDB
 *
 */

#define _DEFAULT_SOURCE

#include "config.h"

#include "mesalib.h"
#include "dimeratab.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <gphoto2/gphoto2.h>

#include "libgphoto2/i18n.h"


#define GP_MODULE "dimera"

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define IMAGE_NAME_TEMPLATE "dimera%02i.ppm"
#define RAM_IMAGE_TEMPLATE "temp.ppm"	/* actually, sometimes this is a PGM */

/* PNM headers */
#define VIEWFIND_SZ	(128*96)
#define VIEW_TYPE	251
#define DEFAULT_EXPOSURE	(50000/30)	/* 1/30 of a second */
#define MAX_EXPOSURE	(50000/4)
#define MIN_EXPOSURE	1

static const char Dimera_thumbhdr[] =
"P5\n# Dimera 3500 Thumbnail written by gphoto2\n64 48\n255\n";

static const char Dimera_viewhdr[] =
"P5\n# Dimera 3500 Viewfinder written by gphoto2\n128 96\n15\n";

static const char Dimera_finehdr[] =
"P6\n# Dimera 3500 Image written by gphoto2\n640 480\n255\n";

static const char Dimera_stdhdr[] =
"P6\n# Dimera 3500 Image written by gphoto2\n320 240\n255\n";

/* Forward references */

static uint8_t *
Dimera_Get_Full_Image (int picnum, long *size, int *width, int *height,
			Camera *camera, GPContext *context);

static uint8_t *
Dimera_Get_Thumbnail( int picnum, long *size, Camera *camera );

static uint8_t *
Dimera_Preview( long *size, Camera *camera, GPContext *context );

/* Gphoto2 */

struct _CameraPrivateLibrary {
        unsigned exposure;
        int auto_exposure;
        int auto_flash;
};


static const char *models[] = {
        "Mustek:VDC-3500",
        "Relisys:Dimera 3500",
        "Trust:DC-3500",
        NULL
};


int camera_id (CameraText *id) {

	strcpy(id->text, "dimera3500");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	int x=0;
	CameraAbilities a;

	while (models[x]) {

		memset (&a, 0, sizeof(a));
		strcpy(a.model, models[x]);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
		a.port     = GP_PORT_SERIAL;
		a.speed[0] = 9600;
		a.speed[1] = 14400;
		a.speed[2] = 19200;
		a.speed[3] = 38400;
		a.speed[4] = 57600;
		a.speed[5] = 76800;
		a.speed[6] = 115200;
		a.speed[7] = 0;
		a.operations = GP_OPERATION_CAPTURE_IMAGE | GP_OPERATION_CONFIG;
		a.file_operations  = GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;

		gp_abilities_list_append(list, a);

		x++;
	}

	return (GP_OK);
}

static int camera_exit(Camera *camera, GPContext *context) {

	return mesa_port_close(camera->port);
}

static int file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list, void *data, GPContext *context) {

	Camera *camera = data;
	int count;

	/* We only support root folder */
	if (strcmp (folder, "/"))
	{
		gp_context_error (context, _("Only root folder is supported - "
			"you requested a file listing for folder '%s'."), folder);
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	}

	count = mesa_get_image_count (camera->port);
	if (count < 0)
	{
		gp_context_error (context, _("Problem getting number of images"));
		return (count);
	}

	/*
	 * Create pseudo file names for each picture in the camera, including
	 * the temporary picture in RAM, which, unfortunately, might not always
	 * be available.
	 *
	 * We don't add anything to the filesystem here - the filesystem does
	 * that for us.
	 */
	CHECK (gp_filesystem_append (fs, "/", RAM_IMAGE_TEMPLATE, context));
	return (gp_list_populate (list, IMAGE_NAME_TEMPLATE, count));
}

static int
conversion_chuck (const unsigned int width, const unsigned int height,
	const unsigned char *src,unsigned char *dst
) {
	unsigned int x, y;
	int p1, p2, p3, p4;
	int red, green, blue;

	/*
	 * Added by Chuck -- 12-May-2000
	 * Convert the colors based on location, and my wacky color table:
	 *
	 * Convert the 4 cells into a single RGB pixel.
	 * Colors are encoded as follows:
	 *
	 * 		000000000000000000........33
	 * 		000000000011111111........11
	 * 		012345678901234567........89
	 * 		----------------------------
	 * 	000     RGRGRGRGRGRGRGRGRG........RG
	 * 	001     GBGBGBGBGBGBGBGBGB........GB
	 * 	002     RGRGRGRGRGRGRGRGRG........RG
	 * 	003     GBGBGBGBGBGBGBGBGB........GB
	 * 	...     ............................
	 * 	238     RGRGRGRGRGRGRGRGRG........RG
	 * 	239     GBGBGBGBGBGBGBGBGB........GB
	 *
	 * NOTE. Above is 320x240 resolution.  Just expand for 640x480.
	 *
	 * Use neighboring cells to generate color values for each pixel.
	 * The neighbors are the (x-1, y-1), (x, y-1), (x-1, y), and (x, y).
	 * The exception is when x or y are zero then the neighbors used are
	 * the +1 cells.
	 */

	for (y = 0;y < height; y++)
		for (x = 0;x < width; x++) {
			p1 = ((y==0?y+1:y-1)*width) + (x==0?x+1:x-1);
			p2 = ((y==0?y+1:y-1)*width) +  x;
			p3 = ( y            *width) + (x==0?x+1:x-1);
			p4 = ( y            *width) +  x;

			switch (((y & 1) << 1) + (x & 1)) {
			case 0: /* even row, even col, red */
				blue  = blue_table [src[p1]];
				green = green_table[src[p2]];
				green+= green_table[src[p3]];
				red   = red_table  [src[p4]];
				break;
			case 1: /* even row, odd col, green */
				green = green_table[src[p1]];
				blue  = blue_table [src[p2]];
				red   = red_table  [src[p3]];
				green+= green_table[src[p4]];
				break;
			case 2: /* odd row, even col, green */
				green = green_table[src[p1]];
				red   = red_table  [src[p2]];
				blue  = blue_table [src[p3]];
				green+= green_table[src[p4]];
				break;
			case 3: /* odd row, odd col, blue */
			default:
				red   = red_table  [src[p1]];
				green = green_table[src[p2]];
				green+= green_table[src[p3]];
				blue  = blue_table [src[p4]];
				break;
			}
			*dst++ = (unsigned char) red;
			*dst++ = (unsigned char) (green/2);
			*dst++ = (unsigned char) blue;
		}
	return (GP_OK);
}


static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	CameraFileType type, CameraFile *file, void *user_data, GPContext *context
) {

	Camera *camera = user_data;
	int num, width, height;
	uint8_t *data, *newdata;
	long int size;

	/* Retrieve the number of the photo on the camera */
	if (strcmp(filename, RAM_IMAGE_TEMPLATE) == 0)
		/* Magic file name specifies magic image number */
		num = RAM_IMAGE_NUM;
	else
		num = gp_filesystem_number(camera->fs, "/", filename, context);

	if (num < 0)
		return num;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		data = Dimera_Get_Full_Image (num, &size,
					      &width, &height, camera,
					      context);
		if (!data)
			return GP_ERROR;
		gp_file_set_mime_type (file, GP_MIME_PPM);
		if (width == 640)
			gp_file_append (file, Dimera_finehdr, strlen(Dimera_finehdr));
		else
			gp_file_append (file, Dimera_stdhdr, strlen(Dimera_stdhdr));
		newdata = malloc(size*3);
		if (!newdata) return (GP_ERROR_NO_MEMORY);
		conversion_chuck (width, height, data, newdata);
		gp_file_append (file, (char *)newdata, size*3);
		free (newdata);
		free (data);
		break;
		break;
	case GP_FILE_TYPE_RAW:
		data = Dimera_Get_Full_Image (num, &size,
					      &width, &height, camera,
					      context);
		if (!data)
			return GP_ERROR;
		gp_file_set_data_and_size (file, (char *)data, size); /* will take over data ptr ownership */
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_adjust_name_for_mime_type (file);
		break;
	case GP_FILE_TYPE_PREVIEW:
		data = Dimera_Get_Thumbnail (num,  &size, camera);
		if (!data)
			return GP_ERROR;
		gp_file_set_data_and_size (file, (char *)data, size); /* will take over data ptr ownership */
		gp_file_set_mime_type (file, GP_MIME_PGM);
		gp_file_adjust_name_for_mime_type (file);
		break;
	default:
		gp_context_error (context, _("Image type is not supported"));
		return (GP_ERROR_NOT_SUPPORTED);
	}
	return GP_OK;
}

static int get_info_func (CameraFilesystem *fs, const char *folder, const char *filename, CameraFileInfo *info, void *data, GPContext *context) {

	Camera *camera = data;
	int num, std_res;

	num = gp_filesystem_number (fs, folder, filename, context);
	if (num < 0)
		return num;

	if ( (std_res = mesa_read_image_info( camera->port, num, NULL )) < 0 )
	{
		gp_log(GP_LOG_ERROR, "dimera/dimera3500", "Can't get Image Info");
		gp_context_error (context, _("Problem getting image information"));
		return std_res;
	}

	info->preview.fields = GP_FILE_INFO_ALL;
	strcpy(info->preview.type, GP_MIME_PGM);
	info->preview.size = MESA_THUMB_SZ + sizeof( Dimera_thumbhdr ) - 1;
	info->preview.width = 64;
	info->preview.height = 48;

	info->file.fields = GP_FILE_INFO_TYPE|GP_FILE_INFO_PERMISSIONS|GP_FILE_INFO_WIDTH|GP_FILE_INFO_HEIGHT|GP_FILE_INFO_SIZE;
	strcpy(info->file.type, GP_MIME_PPM);
	info->file.permissions = GP_FILE_PERM_READ;

	if (std_res) {
	    info->file.width = 320;
	    info->file.height = 240;
	} else {
	    info->file.width = 640;
	    info->file.height = 480;
	}
	info->file.size = info->file.height*info->file.width*3 + sizeof( Dimera_finehdr ) - 1;

	return GP_OK;
}

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context) {

	if (type != GP_CAPTURE_IMAGE)
	{
		gp_context_error (context, _("Capture type is not supported"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (camera->pl->auto_flash) {
		CHECK (mesa_snap_picture( camera->port, camera->pl->exposure*4 ));
	}
	else {
		CHECK (mesa_snap_image( camera->port, camera->pl->exposure*4 ));
	}

	/*
	 * User must download special RAM_IMAGE_TEMPLATE file.
	 */
	strncpy (path->folder, "/", sizeof (path->folder));
	strncpy (path->name, RAM_IMAGE_TEMPLATE, sizeof (path->name));

	return (GP_OK);
}

static int camera_capture_preview(Camera *camera, CameraFile *file, GPContext *context) {
        long size;
	uint8_t *data;

	gp_file_set_mime_type (file, GP_MIME_PGM);

        data = Dimera_Preview( &size, camera, context);
        if (!data)
                return GP_ERROR;
	return gp_file_set_data_and_size (file, (char *)data, size);
}

static int camera_summary (Camera *camera, CameraText *summary, GPContext *context) {
	int num, eeprom_capacity, hi_pics_max, lo_pics_max;
	struct mesa_id Id;
	char version_string[MESA_VERSION_SZ];
	char battery_string[80];
	struct mesa_feature features;
	uint8_t eeprom_info[MESA_EEPROM_SZ];
	/* Table of EEPROM capacities in Mb based on part compatibility ID */
	static uint8_t const eeprom_size_table[14] = {
		/* ID    Mb */
		/* 00 */ 0,
		/* 01 */ 8,
		/* 02 */ 8,
		/* 03 */ 0,
		/* 04 */ 0,
		/* 05 */ 8,
		/* 06 */ 16,
		/* 07 */ 0,
		/* 08 */ 1,
		/* 09 */ 2,
		/* 0A */ 4,
		/* 0B */ 1,
		/* 0C */ 2,
		/* 0D */ 4
	};

	num = mesa_get_image_count(camera->port);
	if (num < 0)
	{
		gp_context_error (context, _("Problem getting number of images"));
		return num;
	}

	mesa_send_id( camera->port, &Id );
	mesa_version(camera->port, version_string);
	mesa_read_features(camera->port, &features);
	mesa_eeprom_info(camera->port, 1, eeprom_info);
	eeprom_capacity = 0;
	if (eeprom_info[4] == 0xc9) {
		if (eeprom_info[11] < sizeof(eeprom_size_table))
			eeprom_capacity = eeprom_size_table[eeprom_info[11]];
	}
	/* Estimate the number of pictures that this size of flash can hold */
	hi_pics_max = eeprom_capacity / 2;
	lo_pics_max = (eeprom_capacity * 13) / 8;

	if (features.feature_bits_lo & AC_PRESENT)
		battery_string[0] = '\0';
	else
		snprintf( battery_string, sizeof(battery_string),
			_(" (battery is %d%% full)"),
			mesa_battery_check(camera->port));

	snprintf( summary->text, sizeof(summary->text),
			_("Dimera 3500 ver. %s %d/%d %d:%d.\n"
			"%d pictures used of approximately %d (high res) or %d (low res).\n"
			"Camera features: "
			  "%s, %s, %s, %s.\n"
			"Flash is %s, is %s and is %s.\n"
			"Resolution is set to %s.\n"
			"Camera is %s powered %s.\n"),

			version_string, Id.year, Id.week, Id.man, Id.ver,
			num, hi_pics_max, lo_pics_max,
			(features.feature_bits_lo & HAVE_FLASH) ?
				_("Flash") : _("No Flash"),
			(features.feature_bits_lo & DUAL_IRIS) ?
				_("Dual Iris") : _("No Dual Iris"),
			(features.feature_bits_lo & HAVE_RES_SW) ?
				_("Resolution Switch") : _("No Resolution Switch"),
			(features.feature_bits_hi & NO_PWR_LIGHT) ?
				_("No Power Light") : "Power Light",
			(features.feature_bits_lo & FLASH_ON) ?
				_("ON") : _("OFF"),
			(features.feature_bits_lo & FLASH_READY) ?
				_("ready") : _("Not ready"),
			(features.feature_bits_lo & FLASH_FILL) ?
				_("in fill mode") : _("Not in fill mode"),
			(features.feature_bits_lo & LOW_RES) ?
				_("low (320x240)") : _("high (640x480)"),
			(features.feature_bits_lo & AC_PRESENT) ?
				_("externally") : _("internally"),
			battery_string
	);

	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual, GPContext *context) {

	strcpy(manual->text, _(
	"* Image glitches or problems communicating are\n"
	"  often caused by a low battery.\n"
	"* Images captured remotely on this camera are stored\n"
	"  in temporary RAM and not in the flash memory card.\n"
	"* Exposure control when capturing images can be\n"
	"  configured manually or set to automatic mode.\n"
	"* Image quality is currently lower than it could be.\n"
	));

	return GP_OK;
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context) {
	strcpy(about->text, _(
		"gPhoto2 Mustek VDC-3500/Relisys Dimera 3500\n"
		"This software was created with the\n"
		"help of proprietary information belonging\n"
		"to StarDot Technologies.\n"
		"\n"
		"Author:\n"
		"  Brian Beattie  <URL:http://www.beattie-home.net>\n"
		"Contributors:\n"
		"  Chuck Homic <chuck@vvisions.com>\n"
		"     Converting raw camera images to RGB\n"
		"  Dan Fandrich <dan@coneharvesters.com>\n"
		"     Information on protocol, raw image format,\n"
		"     gphoto2 port\n"
		));
	return GP_OK;
}



/* Download a thumbnail image from the camera and return it in a malloced
buffer with PGM headers */
static uint8_t *
Dimera_Get_Thumbnail( int picnum, long *size, Camera *camera )
{
	int32_t		r;
	uint8_t *image;

	if ( !(image = (unsigned char *) malloc( MESA_THUMB_SZ +
			sizeof( Dimera_thumbhdr ) - 1 )) )
	{
		gp_log(GP_LOG_ERROR, "dimera/dimera3500",  "Get Thumbnail, allocation failed" );
		*size = 0;
		return NULL;
	}

	/* set image size */
	*size = MESA_THUMB_SZ + sizeof( Dimera_thumbhdr ) - 1;

	/* set image header */
	memcpy( image, Dimera_thumbhdr, sizeof( Dimera_thumbhdr ) - 1 );

	if ( (r = mesa_read_thumbnail( camera->port, picnum, image +
			sizeof( Dimera_thumbhdr ) - 1 )) < 0 )
	{
		gp_log(GP_LOG_ERROR, "dimera/dimera3500",  "Get Thumbnail, read of thumbnail failed" );
		free( image );
		*size = 0;
		return NULL;
	}
	/*return (0 != (r&0x1000000));*/
	return image;
}

/* Download a raw Bayer image from the camera and return it in a malloced
buffer */
static uint8_t *
Dimera_Get_Full_Image (int picnum, long *size, int *width, int *height,
			Camera *camera, GPContext *context)
{
	static struct mesa_image_arg	ia;
	int32_t				r;
	uint8_t			*b, *rbuffer = NULL;
	int				hires, s, retry;
	unsigned int id;

	*size = 0;
	*width = 0;
	*height = 0;

	if ( picnum != RAM_IMAGE_NUM )
	{
		GP_DEBUG("Getting Image Info");
		if ( (r = mesa_read_image_info( camera->port, picnum, NULL )) < 0 )
		{
			gp_log(GP_LOG_ERROR, "dimera/dimera3500", "Can't get Image Info");
			gp_context_error (context, _("Problem getting image information"));
			return NULL;
		}
		if ( r )
		{
			hires = FALSE;
			*height = 240;
			*width = 320;
		} else {
			hires = TRUE;
			*height = 480;
			*width = 640;
		}

		GP_DEBUG("Loading Image");
		if ( mesa_load_image( camera->port, picnum ) != GP_OK )
		{
			gp_log(GP_LOG_ERROR, "dimera/dimera3500", "Image Load failed");
			gp_context_error (context, _("Problem reading image from flash"));
			return NULL;
		}

	} else {
			/* load the snapped image */
		hires = TRUE;
		*height = 480;
		*width = 640;
	}

	*size = *height * *width;	/* 8 bits per pixel in raw CCD format */

	GP_DEBUG("Downloading Image");

	rbuffer = (uint8_t *)malloc( *size );
	if ( rbuffer == NULL )
	{
		gp_context_error (context, _("Out of memory"));
		return NULL;
	}

	ia.start = 28;
	ia.send = 4;
	ia.skip = 0;
	ia.repeat = ( hires ? 160 : 80 );
	ia.row_cnt = 40;
	ia.inc1 = 1;
	ia.inc2 = 128;
	ia.inc3 = ia.inc4 = 0;

	/* due to reports of mesa_read_image not working for some cameras */
	/* this is changed to use mesa_read_row */
#if 0
	id = gp_context_progress_start (context, *height + 4,
					_("Downloading image..."));
	for ( ia.row = 4, b = rbuffer; ia.row < (height + 4) ;
			ia.row += ia.row_cnt, b += s )
	{
		GP_DEBUG("Downloading Image");
		for ( retry = 10;; )
		{

			s = mesa_read_image( camera->port, b, &ia );
			if( s > 0)
				break;

			if ( (s == GP_ERROR_TIMEOUT || s == GP_ERROR_CORRUPTED_DATA) && --retry > 0)
			{
				GP_DEBUG( "Dimera_Get_Full_Image: retrans");
				continue;
			}
			GP_DEBUG(
				"Dimera_Get_Full_Image: read error %d (retry %d)",s,retry);
				/* retry count exceeded, or other error */
			free( rbuffer );
			*size = 0;
			gp_context_error (context, _("Problem downloading image"));
			return NULL;
		}
		gp_context_progress_update (context, id, ia.row);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
		{
			free( rbuffer );
			*size = 0;
			gp_context_error (context, _("User canceled download"));
			return NULL;
		}
	}
#else
	id = gp_context_progress_start (context, *height + 4,
					_("Downloading image..."));
	for ( ia.row = 4, b = rbuffer; ia.row < (*height + 4) ;
			ia.row++, b += s )
	{
		GP_DEBUG("Downloading Image");
		for ( retry = 10;; )
		{

			s = mesa_read_row( camera->port, b, &ia );
			if( s > 0)
				break;

			if ( (s == GP_ERROR_TIMEOUT || s == GP_ERROR_CORRUPTED_DATA) && --retry > 0)
			{
				GP_DEBUG("Dimera_Get_Full_Image: retrans");
				continue;
			}
			GP_DEBUG(
				"Dimera_Get_Full_Image: read error %d (retry %d)",s,retry);
				/* retry count exceeded, or other error */
			free( rbuffer );
			*size = 0;
			gp_context_error (context, _("Problem downloading image"));
			return NULL;
		}
		gp_context_progress_update (context, id, ia.row);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
		{
			free( rbuffer );
			*size = 0;
			return NULL;
		}
	}
	gp_context_progress_stop (context, id);
#endif

	return (rbuffer);
}



#ifdef TOO_FINICKY_AUTO_EXPOSURE
/*
Table of exposure curves of the form:
    y = M*log(x) + b
where y is the average brightness of an image [0..15]
      x is the exposure setting [1..50000]
      M and b are constants for each curve
Each scene is made to fit one of these characteristic curves.
The appropriate shutter speed (exposure) value can be calculated once the
correct curve is known, using the equation:
   x = exp((y + b) / M)
where y is the desired average brightness (probably 8).

We need more, and more accurate, curves here to make this work.  You'd need
a better way than trial and error to generate a family of characteristic
curves.
*/
static struct { float M; float b; }
exposure_tables[] = {
	{2.6, -11},
	{2.8, -15},
	{2.9, -19},
	{1.5, -9},
	{0.82, -5},
	{0, 0}
};

/*
 * Calculate a new exposure value for this picture
 * exposure is 0..50000 and brightness is 0..255
 */
static unsigned calc_new_exposure(unsigned exposure, unsigned brightness) {
	int i;

	/* Choose the correct curve for this camera scene */
	for (i=0; exposure_tables[i].M; ++i) {
		GP_DEBUG( "brightness %d, %f ",
			brightness,
			exp((double)((brightness/16.0) - exposure_tables[i].b) / exposure_tables[i].M));
		if (exp((double)((brightness/16.0) - exposure_tables[i].b) / exposure_tables[i].M) > exposure)
			/* We found the best curve for this scene */
			break;
	}
	i = MAX(i-1, 0);

	/* Using this curve, calculate the optimum exposure */
	return MAX(MIN_EXPOSURE,MIN(MAX_EXPOSURE,(unsigned) exp((double)(8.0 - exposure_tables[i].b) / exposure_tables[i].M)));

}

#else
/* Use linear interpolation to choose a better exposure level for this scene */
static unsigned calc_new_exposure(unsigned exposure, unsigned brightness) {
	return MAX(MIN_EXPOSURE,MIN(MAX_EXPOSURE,(exposure * 128L) / brightness));
}
#endif

/* Download a live image from the camera and return it in a malloced
buffer with PGM headers */
static uint8_t *
Dimera_Preview( long *size, Camera *camera, GPContext *context )
{
	uint8_t buffer[VIEWFIND_SZ/2], *p;
	int		i;
	uint8_t *image;
	uint32_t exposure_total;
	unsigned brightness;

	if ( !(image = (unsigned char *) malloc( VIEWFIND_SZ +
			sizeof( Dimera_viewhdr ) - 1 )) )
	{
		gp_log(GP_LOG_ERROR, "dimera/dimera3500",  "Get Preview, allocation failed" );
		gp_context_error (context, _("Out of memory"));
		return NULL;
	}

	/* set image size */
	*size = VIEWFIND_SZ + sizeof( Dimera_viewhdr ) - 1;

	/* set image header */
	memcpy( image, Dimera_viewhdr, sizeof( Dimera_viewhdr ) - 1 );

	if ( mesa_snap_view( camera->port, buffer, TRUE, 0, 0, 0, camera->pl->exposure,
			VIEW_TYPE) < 0 )
	{
		gp_log(GP_LOG_ERROR, "dimera/dimera3500",  "Get Preview, mesa_snap_view failed" );
		free (image);
		gp_context_error (context, _("Problem taking live image"));
		return NULL;
	}

	/* copy the buffer, splitting the pixels up */
	exposure_total = 0;
	for ( p = image + sizeof( Dimera_viewhdr ) - 1, i = 0;
			i < (VIEWFIND_SZ/2) ; i++ )
	{
		*p++ = buffer[i] >> 4;
		*p++ = buffer[i] & 0xf;
		exposure_total += (buffer[i] >> 4) + (buffer[i] & 0xf);
	}

	/* Automatic exposure control */

	/* Current picture brightness, where 0 is dark and 255 is bright */
	brightness = exposure_total / (VIEWFIND_SZ / 16);

	GP_DEBUG(
		"Average pixel brightness %f, Current exposure value: %d",
		brightness / 16.0, camera->pl->exposure);

	if (camera->pl->auto_exposure && (brightness < 96 || brightness > 160)) {
		/* Picture brightness needs to be corrected for next time */
		camera->pl->exposure = calc_new_exposure(camera->pl->exposure, brightness);
		GP_DEBUG( "New exposure value: %d", camera->pl->exposure);
	}

	return image;
}

static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *t, *section;
	char str[16];

	GP_DEBUG ("camera_get_config()");

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);

	gp_widget_new (GP_WIDGET_SECTION, _("Exposure"), &section);
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TOGGLE, _("Automatic exposure adjustment on preview"), &t);
	gp_widget_set_value (t, &camera->pl->auto_exposure);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Exposure level on preview"), &t);
	sprintf(str, "%d", camera->pl->exposure);
	gp_widget_set_value (t, str);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_SECTION, _("Flash"), &section);
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TOGGLE, _("Automatic flash on capture"), &t);
	gp_widget_set_value (t, &camera->pl->auto_flash);
	gp_widget_append (section, t);

	return GP_OK;
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *w;
	char *wvalue;
	int val;
	char str[16];

	GP_DEBUG ("camera_set_config()");

	gp_widget_get_child_by_label (window, _("Exposure level on preview"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		camera->pl->exposure = MAX(MIN_EXPOSURE,MIN(MAX_EXPOSURE,atoi(wvalue)));
		gp_setting_set ("dimera3500", "exposure", wvalue);
		GP_DEBUG ("set exposure");
	}

	gp_widget_get_child_by_label (window, _("Automatic exposure adjustment on preview"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &val);
		camera->pl->auto_exposure = val;
		sprintf(str, "%d", val);
		gp_setting_set ("dimera3500", "auto_exposure", str);
		GP_DEBUG ("set auto_exposure");
	}

	gp_widget_get_child_by_label (window, _("Automatic flash on capture"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &val);
		camera->pl->auto_flash = val;
		sprintf(str, "%d", val);
		gp_setting_set ("dimera3500", "auto_flash", str);
		GP_DEBUG ("set auto_flash");
	}

	GP_DEBUG ("done configuring driver.");

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func
};

int camera_init (Camera *camera, GPContext *context) {

	GPPortSettings settings;
        int ret, selected_speed;
        char buf[1024];

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->capture              = camera_capture;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;
        camera->functions->get_config           = camera_get_config;
        camera->functions->set_config           = camera_set_config;

	/* Get the settings and remember the selected speed */
	gp_port_get_settings (camera->port, &settings);
	selected_speed = settings.serial.speed;

	/* TODO: camera->pl can probably be allocated at the end to simplify */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
	{
		gp_context_error (context, _("Out of memory"));
		return (GP_ERROR_NO_MEMORY);
	}

        /* Set the default exposure */
        if (gp_setting_get ("dimera3500", "exposure", buf) == GP_OK)
            camera->pl->exposure = atoi(buf);
        else
            camera->pl->exposure = DEFAULT_EXPOSURE;

        /* Set automatic exposure enable setting for capture preview mode */
        if (gp_setting_get ("dimera3500", "auto_exposure", buf) == GP_OK)
            camera->pl->auto_exposure = atoi(buf);
        else
            camera->pl->auto_exposure = 1;

        /* Set flag to use flash, if necessary, when capturing picture */
        if (gp_setting_get ("dimera3500", "auto_flash", buf) == GP_OK)
            camera->pl->auto_flash = atoi(buf);
        else
            camera->pl->auto_flash = 1;

        GP_DEBUG("Opening port");
        if ( (ret = mesa_port_open(camera->port)) != GP_OK)
        {
                gp_log(GP_LOG_ERROR, "dimera/dimera3500", "Camera Open Failed");
		free (camera->pl);
		camera->pl = NULL;
		gp_context_error (context, _("Problem opening port"));
                return ret;
        }

        GP_DEBUG("Resetting camera");
        if ( (ret = mesa_reset(camera->port)) != GP_OK )
        {
                gp_log(GP_LOG_ERROR, "dimera/dimera3500", "Camera Reset Failed");
		free (camera->pl);
		camera->pl = NULL;
		gp_context_error (context, _("Problem resetting camera"));
                return ret;
        }

        GP_DEBUG("Setting speed");
        if ( (ret = mesa_set_speed(camera->port, selected_speed)) != GP_OK )
        {
                gp_log(GP_LOG_ERROR, "dimera/dimera3500", "Camera Speed Setting Failed");
		free (camera->pl);
		camera->pl = NULL;
		gp_context_error (context, _("Problem setting camera communication speed"));
                return ret;
        }


        GP_DEBUG("Checking for modem");
        switch ( ret = mesa_modem_check(camera->port) )
        {
        case GP_ERROR_IO:
        case GP_ERROR_TIMEOUT:
                gp_log(GP_LOG_ERROR, "dimera/dimera3500", "No or Unknown Response");
		free (camera->pl);
		camera->pl = NULL;
		gp_context_error (context, _("No response from camera"));
                return GP_ERROR_TIMEOUT;
        case GP_ERROR_MODEL_NOT_FOUND:
                gp_log(GP_LOG_ERROR, "dimera/dimera3500", "Probably a modem");
		free (camera->pl);
		camera->pl = NULL;
		gp_context_error (context, _("Looks like a modem, not a camera"));
                return GP_ERROR_MODEL_NOT_FOUND;
        case GP_OK:
                break;
	default:
		/* Hopefully, gp_camera_set_error was called for this error */
		return ret;
        }
	/* Tell the filesystem where to get listings and info from */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	return (GP_OK);
}
