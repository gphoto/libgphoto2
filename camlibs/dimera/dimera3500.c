/* 
 * Program:	gPhoto2 driver for Mustek VDC 3500/Relisys Dimera 3500
 *
 * Version: 	$Id$
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
 * History:
 * $Log$
 * Revision 1.31  2001/12/06 01:45:49  dfandrich
 * 	* configure.in
 * 	* libgphoto2_port/m4/stdint.m4
 * 	* camlibs/dimera/dimera3500.c
 * 	* camlibs/dimera/mesalib.h
 * 	* camlibs/panasonic/dc.c
 * 	* camlibs/panasonic/dc.h
 * 	* camlibs/panasonic/dc1000.c
 * 	* camlibs/panasonic/dc1580.c
 * 	* camlibs/panasonic/l859/l859.c
 * 	* camlibs/panasonic/l859/l859.h: add AC_NEED_STDINT_H to configure.in
 * 	  and change camera libraries to use C99-style size-specific integer types
 *
 * Revision 1.30  2001/11/16 02:26:32  dfandrich
 * Propagating more error codes to the caller.
 *
 * Revision 1.29  2001/11/08 08:09:22  lutz
 * Small compile fix (you don't set permissions on thumbnails).
 *
 * Revision 1.28  2001/11/03 14:30:21  marcusmeissner
 * If we check for 'ENABLE_NLS', we need to include <config.h>.
 * Fix 'redefinition of _' compile warnings due to broken copy & pasted macro
 * in the camlibs.
 *
 * Revision 1.27  2001/10/31 08:29:53  dfandrich
 * * camlibs/dimera/ *.c: use renamed get/set functions, gp_camera_set_error
 *
 * Revision 1.26  2001/10/20 18:05:55  lutz
 * 2001-10-20 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/digita:
 *         * camlibs/dimera: Prepare for elimination of camera->camlib_data.
 *         * camlibs/konica: Some additional debugging messages
 *
 * Revision 1.25  2001/10/20 12:36:11  lutz
 * 2001-10-20 Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs: Instead of camera->port_info->speed, use
 *           settings.serial.speed. That lets us ...
 *         * libgphoto2/gphoto2-camera.[c,h]: ... remove camera->port_info
 *
 * [... elided log entries ...]
 *
 * Revision 1.3  2001/06/28 22:03:42  dfandrich
 * Integrated Brian Beattie's patch to use mesa_read_row instead of
 * mesa_read_image for picture downloading.  Hopefully, this will
 * allow downloading on newer cameras.
 *
 *		2001/06/06 - Added serial baud rate setting.		  Dan
 *			     Improved camera_summary.
 *
 *		2001/03/10 - Port to gphoto2				  Dan
 *
 *		2000/09/06 - Rewrite of Dimera_Get_Full_Image()		  GDB
 *			* Reordered call to mesa_read_image_info()	  GDB
 *			* Minor revision & doc Dimera_covert_raw()	  GDB	
 */

#include <config.h>

#include <gphoto2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <_stdint.h>
#include "mesalib.h"
#include "dimeratab.h"

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

/* Legacy macros */
/*#define ERROR( s ) { \
	fprintf( stderr, "%s: %s\n", __FUNCTION__, s ); \
	error_dialog( s ); }*/
#define ERROR(s) gp_debug_printf(GP_DEBUG_LOW, "dimera", "%s", (s))
#define debuglog(s) gp_debug_printf(GP_DEBUG_LOW, "dimera", "%s", (s))

#define update_status(s) ERROR(s)

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

#define IMAGE_NAME_TEMPLATE "dimera%02i.ppm"
#define RAM_IMAGE_TEMPLATE "temp.ppm"	/* actually, sometimes this is a PGM */

/* PNM headers */
#define VIEWFIND_SZ	(128*96)
#define VIEW_TYPE	251
#define DEFAULT_EXPOSURE	(50000/30)	/* 1/30 of a second */
#define MAX_EXPOSURE	(50000/4)
#define MIN_EXPOSURE	1

static char     Dimera_thumbhdr[] =
"P5\n# Dimera 3500 Thumbnail\n64 48\n255\n";

static char     Dimera_viewhdr[] = 
"P5\n# Dimera 3500 Viewfinder\n128 96\n15\n";

static char     Dimera_finehdr[] =
"P6\n# Dimera 3500 Image\n640 480\n255\n";

static char     Dimera_stdhdr[] =
"P6\n# Dimera 3500 Image\n320 240\n255\n";

/* Forward references */

static uint8_t *
Dimera_Get_Full_Image (int picnum, int *size, Camera *camera,
		       int *width, int *height);

static uint8_t *
Dimera_Get_Thumbnail( int picnum, int *size, Camera *camera );

static uint8_t *
Dimera_Preview( int *size, Camera *camera );

/* Gphoto2 */

struct _CameraPrivateLibrary {
        unsigned exposure; 
        int auto_exposure; 
        int auto_flash; 
};


static char *models[] = {
        "Mustek VDC-3500",
        "Relisys Dimera 3500",
        "Trust DC-3500",
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
		a.operations = GP_OPERATION_CAPTURE_IMAGE;
		a.file_operations  = GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;

		gp_abilities_list_append(list, a);

		x++;
	}

	return (GP_OK);
}

static int camera_exit(Camera *camera) {

	return mesa_port_close(camera->port);
}

static int file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list, void *data) {

	Camera *camera = data;
	int count;

	/* We only support root folder */
	if (strcmp (folder, "/"))
	{
		gp_camera_set_error(camera, _("Only root directory is supported"));
		return (GP_ERROR_DIRECTORY_NOT_FOUND);
	}

	count = mesa_get_image_count (camera->port);
	if (count < 0)
	{
		gp_camera_set_error(camera, _("Problem getting number of images"));
		return (count);
	}

	/*
	 * Create pseudo file names for each picture in the camera, except
	 * the temporary picture in RAM, which might not always be available.
	 *
	 * We don't add anything to the filesystem here - the filesystem does
	 * that for us.
	 */
	return (gp_list_populate (list, IMAGE_NAME_TEMPLATE, count));
}

static int get_file_func (CameraFilesystem *fs, const char *folder, const char *filename, CameraFileType type, CameraFile *file, void *user_data) {

	Camera *camera = user_data;
	int num, width, height;
	char *data;
	long int size;

	/* Retrieve the number of the photo on the camera */
	if (strcmp(filename, RAM_IMAGE_TEMPLATE) == 0)
		/* Magic file name specifies magic image number */
		num = RAM_IMAGE_NUM;
	else
		num = gp_filesystem_number(camera->fs, "/", filename);

	if (num < 0)
		return num;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
	case GP_FILE_TYPE_RAW:
		data = Dimera_Get_Full_Image (num, (int*) &size, camera,
					      &width, &height);
		break;
	case GP_FILE_TYPE_PREVIEW:
		data = Dimera_Get_Thumbnail (num, (int*) &size, camera);
		break;
	default:
		gp_camera_set_error(camera, _("Image type is not supported"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (!data)
		return GP_ERROR;

	gp_file_set_name (file, filename);
	gp_file_set_data_and_size (file, data, size);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:

		/* Let gphoto2 do the conversion */
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_set_color_table (file, red_table,   256,
					       green_table, 256,
					       blue_table,  256);
		gp_file_set_width_and_height (file, width, height);
		if (width == 640)
			gp_file_set_header (file, Dimera_finehdr);
		else
			gp_file_set_header (file, Dimera_stdhdr);
		gp_file_set_conversion_method (file,
					GP_FILE_CONVERSION_METHOD_CHUCK);
		gp_file_convert (file, GP_MIME_PPM);
		break;

	case GP_FILE_TYPE_PREVIEW:
		gp_file_set_mime_type (file, GP_MIME_PGM);
		gp_file_adjust_name_for_mime_type (file);
		break;
	case GP_FILE_TYPE_RAW:
		gp_file_set_mime_type (file, GP_MIME_RAW); 
		gp_file_adjust_name_for_mime_type (file);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	return GP_OK;
}

static int get_info_func (CameraFilesystem *fs, const char *folder, const char *filename, CameraFileInfo *info, void *data) {

	Camera *camera = data;
	int num, std_res;

	num = gp_filesystem_number (fs, folder, filename);
	if (num < 0)
		return num;

	if ( (std_res = mesa_read_image_info( camera->port, num, NULL )) < 0 )
	{
		ERROR("Can't get Image Info");
		gp_camera_set_error(camera, _("Problem getting image information"));
		return std_res;
	}

	info->preview.fields = GP_FILE_INFO_ALL;
	strcpy(info->preview.type, GP_MIME_PGM);
	info->preview.size = MESA_THUMB_SZ + sizeof( Dimera_thumbhdr ) - 1;
	info->preview.width = 64;
	info->preview.height = 48;

	info->file.fields = GP_FILE_INFO_ALL;
	strcpy(info->file.type, GP_MIME_PPM);
	strcpy(info->file.name, filename);
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

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path) {

	if (type != GP_CAPTURE_IMAGE)
	{
		gp_camera_set_error(camera, _("Capture type is not supported"));
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (camera->pl->auto_flash) {
		CHECK (mesa_snap_picture( camera->port, camera->pl->exposure*4 ));
	}
	else {
		CHECK (mesa_snap_image( camera->port, camera->pl->exposure*4 ));
	}

	/* User must download special RAM_IMAGE_TEMPLATE file */
	strcpy(path->folder, "/");
	strcpy(path->name, RAM_IMAGE_TEMPLATE);
	return ( GP_OK );
}

static int camera_capture_preview(Camera *camera, CameraFile *file) {
        long int size;
	char *data;

	gp_file_set_name (file, RAM_IMAGE_TEMPLATE);
	gp_file_set_mime_type (file, GP_MIME_PGM);

        data = Dimera_Preview((int*) &size, camera);
        if (!data)
                return GP_ERROR;
	gp_file_set_data_and_size (file, data, size);

        return GP_OK;
}

static int camera_summary (Camera *camera, CameraText *summary) {
	int num, eeprom_capacity, hi_pics_max, lo_pics_max;
	struct mesa_id Id;
	char version_string[MESA_VERSION_SZ];
	char battery_string[80];
	struct mesa_feature features;
	uint8_t eeprom_info[MESA_EEPROM_SZ];
	/* Table of EEPROM capacities in Mb based on part compatibility ID */
	static uint8_t const eeprom_size_table[14] = {
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
		gp_camera_set_error(camera, _("Problem getting number of images"));
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
	hi_pics_max = eeprom_capacity / 2; 
	lo_pics_max = (eeprom_capacity * 13) / 8;

	if (features.feature_bits_lo & AC_PRESENT)
		battery_string[0] = '\0';
	else
		snprintf( battery_string, sizeof(battery_string),
			_(" (battery is %d%% full)"),
			mesa_battery_check(camera->port));

	snprintf( summary->text, sizeof(summary->text),
			_("Dimera 3500 ver. %s %d/%d %d:%d\n"
			"%d pictures used of approximately %d (high res) or %d (low res)\n"
			"Camera features: "
			  "%s, %s, %s, %s\n"
			"Flash is %s, is %s and is %s\n"
			"Resolution is set to %s\n"
			"Camera is %s powered%s\n"),

			version_string, Id.year, Id.week, Id.man, Id.ver,
			num, hi_pics_max, lo_pics_max,
			(features.feature_bits_lo & HAVE_FLASH) ?
				_("Flash") : _("NO Flash"),
			(features.feature_bits_lo & DUAL_IRIS) ?
				_("Dual Iris") : _("NO Dual Iris"),
			(features.feature_bits_lo & HAVE_RES_SW) ?
				_("Resolution Switch") : _("NO Resolution Switch"),
			(features.feature_bits_hi & NO_PWR_LIGHT) ?
				_("NO Power Light") : "Power Light",
			(features.feature_bits_lo & FLASH_ON) ?
				_("ON") : _("OFF"),
			(features.feature_bits_lo & FLASH_READY) ?
				_("ready") : _("NOT ready"),
			(features.feature_bits_lo & FLASH_FILL) ?
				_("in fill mode") : _("NOT in fill mode"),
			(features.feature_bits_lo & LOW_RES) ?
				_("low (320x240)") : _("high (640x480)"),
			(features.feature_bits_lo & AC_PRESENT) ?
				_("externally") : _("internally"),
			battery_string
	);

	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, _(
	"  Image glitches or problems communicating are\n"
	"often caused by a low battery.\n"
	"  Images captured remotely on this camera are stored\n"
	"in temporary RAM and not in the flash memory card.\n"
	"  Exposure control when capturing all images is\n"
	"automatically set by the capture preview function.\n"
	"  Image quality is currently lower than it could be.\n"
	));

	return GP_OK;
}

static int camera_about (Camera *camera, CameraText *about) {
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
Dimera_Get_Thumbnail( int picnum, int *size, Camera *camera )
{
	int32_t		r;
	uint8_t *image;

	if ( !(image = (unsigned char *) malloc( MESA_THUMB_SZ +
			sizeof( Dimera_thumbhdr ) - 1 )) )
	{
		ERROR( "Get Thumbnail, allocation failed" );
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
		ERROR( "Get Thumbnail, read of thumbnail failed" );
		free( image );
		*size = 0;
		return NULL;
	}
	//return (0 != (r&0x1000000));
	return image;
}

/* Download a raw Bayer image from the camera and return it in a malloced
buffer */
static uint8_t *
Dimera_Get_Full_Image (int picnum, int *size, Camera *camera,
		       int *width, int *height)
{
	static struct mesa_image_arg	ia;
	int32_t				r;
	uint8_t			*b, *rbuffer = NULL;
	int				hires, s, retry;

	*size = 0;
	*width = 0;
	*height = 0;

	if ( picnum != RAM_IMAGE_NUM )
	{
		update_status( _("Getting Image Info") );
		if ( (r = mesa_read_image_info( camera->port, picnum, NULL )) < 0 )
		{
			ERROR("Can't get Image Info");
			gp_camera_set_error(camera, _("Problem getting image information"));
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

		update_status( _("Loading Image") );
		if ( mesa_load_image( camera->port, picnum ) != GP_OK )
		{
			ERROR("Image Load failed");
			gp_camera_set_error(camera, _("Problem reading image from flash"));
			return NULL;
		}

	} else {
			/* load the snapped image */
		hires = TRUE;
		*height = 480;
		*width = 640;
	}

	*size = *height * *width;	/* 8 bits per pixel in raw CCD format */

	update_status( _("Downloading Image") );

	rbuffer = (uint8_t *)malloc( *size );
	if ( rbuffer == NULL )
	{
		gp_camera_set_error(camera, _("Out of memory"));
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
	for ( ia.row = 4, b = rbuffer; ia.row < (height + 4) ;
			ia.row += ia.row_cnt, b += s )
	{
		update_status( _("Downloading Image") );
		for ( retry = 10;; )
		{

			s = mesa_read_image( camera->port, b, &ia );
			if( s > 0)
				break;

			if ( (s == GP_ERROR_TIMEOUT || s == GP_ERROR_CORRUPTED_DATA) && --retry > 0)
			{
				update_status( _("Retransmitting") );
				gp_debug_printf(GP_DEBUG_LOW, "dimera", "Dimera_Get_Full_Image: retrans"); 
				continue;
			}
			gp_debug_printf(GP_DEBUG_LOW, "dimera",
				"Dimera_Get_Full_Image: read error %d (retry %d)",s,retry);
				/* retry count exceeded, or other error */
			free( rbuffer );
			*size = 0;
			gp_camera_set_error(camera, _("Problem downloading image"));
			return NULL;
		}
		gp_camera_progress(camera, ia.row / (*height + 4) );
	}
#else
	for ( ia.row = 4, b = rbuffer; ia.row < (*height + 4) ;
			ia.row++, b += s )
	{
		update_status( _("Downloading Image") );
		for ( retry = 10;; )
		{

			s = mesa_read_row( camera->port, b, &ia );
			if( s > 0)
				break;

			if ( (s == GP_ERROR_TIMEOUT || s == GP_ERROR_CORRUPTED_DATA) && --retry > 0)
			{
				update_status( _("Retransmitting") );
				gp_debug_printf(GP_DEBUG_LOW, "dimera", "Dimera_Get_Full_Image: retrans"); 
				continue;
			}
			gp_debug_printf(GP_DEBUG_LOW, "dimera",
				"Dimera_Get_Full_Image: read error %d (retry %d)",s,retry);
				/* retry count exceeded, or other error */
			free( rbuffer );
			*size = 0;
			gp_camera_set_error(camera, _("Problem downloading image"));
			return NULL;
		}
		gp_camera_progress(camera, ia.row / (*height + 4) );
	}
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
		gp_debug_printf(GP_DEBUG_LOW, "dimera", "brightness %d, %f ", 
			brightness,
			exp((double)((brightness/16.0) - exposure_tables[i].b) / exposure_tables[i].M));
		if (exp((double)((brightness/16.0) - exposure_tables[i].b) / exposure_tables[i].M) > exposure)
			/* We found the best curve for this scene */
			break;
	}
	i = max(i-1, 0);

	/* Using this curve, calculate the optimum exposure */
	return max(MIN_EXPOSURE,min(MAX_EXPOSURE,(unsigned) exp((double)(8.0 - exposure_tables[i].b) / exposure_tables[i].M)));

}

#else
/* Use linear interpolation to choose a better exposure level for this scene */
static unsigned calc_new_exposure(unsigned exposure, unsigned brightness) {
	return max(MIN_EXPOSURE,min(MAX_EXPOSURE,(exposure * 128L) / brightness));
}
#endif

/* Download a live image from the camera and return it in a malloced
buffer with PGM headers */
static uint8_t *
Dimera_Preview( int *size, Camera *camera )
{
	uint8_t buffer[VIEWFIND_SZ/2], *p;
	int		i;
	uint8_t *image;
	uint32_t exposure_total;
	unsigned brightness;

	if ( !(image = (unsigned char *) malloc( VIEWFIND_SZ +
			sizeof( Dimera_viewhdr ) - 1 )) )
	{
		ERROR( "Get Preview, allocation failed" );
		gp_camera_set_error(camera, _("Out of memory"));
		return NULL;
	}

	/* set image size */
	*size = VIEWFIND_SZ + sizeof( Dimera_viewhdr ) - 1;

	/* set image header */
	memcpy( image, Dimera_viewhdr, sizeof( Dimera_viewhdr ) - 1 );

	if ( mesa_snap_view( camera->port, buffer, TRUE, 0, 0, 0, camera->pl->exposure,
			VIEW_TYPE) < 0 )
	{
		ERROR( "Get Preview, mesa_snap_view failed" );
		free (image);
		gp_camera_set_error(camera, _("Problem taking live image"));
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

	/* Current picture brightness, where 0 is is dark and 255 is bright */
	brightness = exposure_total / (VIEWFIND_SZ / 16);

	gp_debug_printf(GP_DEBUG_LOW, "dimera",
		"Average pixel brightness %f, Current exposure value: %d",
		brightness / 16.0, camera->pl->exposure);

	if (camera->pl->auto_exposure && (brightness < 96 || brightness > 160)) {
		/* Picture brightness needs to be corrected for next time */
		camera->pl->exposure = calc_new_exposure(camera->pl->exposure, brightness);
		gp_debug_printf(GP_DEBUG_LOW, "dimera", "New exposure value: %d", camera->pl->exposure);
	}

	return image;
}

int camera_init (Camera *camera) {

	GPPortSettings settings;
        int ret, selected_speed;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->capture              = camera_capture;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	/* Get the settings and remember the selected speed */
	gp_port_get_settings (camera->port, &settings);
	selected_speed = settings.serial.speed;

	/* TODO: camera->pl can probably be allocated at the end to simplify */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
	{
		gp_camera_set_error(camera, _("Out of memory"));
		return (GP_ERROR_NO_MEMORY);
	}

        /* Set the default exposure */
	camera->pl->exposure = DEFAULT_EXPOSURE;

        /* Enable automatic exposure setting for capture preview mode */
        camera->pl->auto_exposure = 1;

        /* Use flash, if necessary, when capturing picture */
	camera->pl->auto_flash = 1;

        debuglog("Opening port");
        if ( (ret = mesa_port_open(camera->port)) != GP_OK)
        {
                ERROR("Camera Open Failed");
		free (camera->pl);
		camera->pl = NULL;
		gp_camera_set_error(camera, _("Problem opening port"));
                return ret;
        }

        debuglog("Resetting camera");
        if ( (ret = mesa_reset(camera->port)) != GP_OK )
        {
                ERROR("Camera Reset Failed");
		free (camera->pl);
		camera->pl = NULL;
		gp_camera_set_error(camera, _("Problem resetting camera"));
                return ret;
        }

        debuglog("Setting speed");
        if ( (ret = mesa_set_speed(camera->port, selected_speed)) != GP_OK )
        {
                ERROR("Camera Speed Setting Failed");
		free (camera->pl);
		camera->pl = NULL;
		gp_camera_set_error(camera, _("Problem setting camera communication speed"));
                return ret;
        }


        debuglog("Checking for modem");
        switch ( ret = mesa_modem_check(camera->port) )
        {
        case GP_ERROR_IO:
        case GP_ERROR_TIMEOUT:
                ERROR("No or Unknown Response");
		free (camera->pl);
		camera->pl = NULL;
		gp_camera_set_error(camera, _("No response from camera"));
                return GP_ERROR_TIMEOUT;
        case GP_ERROR_MODEL_NOT_FOUND:
                ERROR("Probably a modem");
		free (camera->pl);
		camera->pl = NULL;
		gp_camera_set_error(camera, _("Looks like a modem, not a camera"));
                return GP_ERROR_MODEL_NOT_FOUND;
        case GP_OK:
                break;
	default:
		/* Hopefully, gp_camera_set_error was called for this error */
		return ret;
        }

	/* Tell the filesystem where to get listings and info from */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	return (GP_OK);
}
