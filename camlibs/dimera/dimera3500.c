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
 * Revision 1.21  2001/10/16 18:01:35  hun
 * added #include lines that should have already been there
 *
 * Revision 1.20  2001/10/11 22:01:25  lutz
 * 2001-10-11  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/canon/psa50.c
 *         * camlibs/dimera/dimera3500.c:
 *         * camlibs/kodak/dc120/library.c:
 *         * camlibs/kodak/dc240/library.c:
 *         * camlibs/kodak/dc3200/library.c:
 *         * camlibs/panasonic/dc1000.c:
 *         * camlibs/panasonic/dc1580.c:
 *         * camlibs/panasonic/l859/l859.c:
 *         * camlibs/sierra/library.c:
 *         * frontends/command-line/interface.c: Unify percentage handling and
 *         define 0.0 <= percentage <= 1.0.
 *
 * Revision 1.19  2001/10/08 08:18:29  lutz
 * 2001-10-08  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * include/gphoto2-frontend.h
 *         * libgphoto2/frontend.c
 *         * camlibs: gp_frontend_progress -> gp_camera_progress,
 *         gp_frontend_status -> gp_camera_status
 *
 * Revision 1.18  2001/08/31 01:33:34  dfandrich
 * Replaced references to PNM with the appropriate PPM or PGM
 *
 * Revision 1.17  2001/08/30 21:59:08  dfandrich
 * Fixed problem with saving raw images.  Stopped file name suffixes from
 * needlessly changing.
 *
 * Revision 1.16  2001/08/30 20:10:40  lutz
 * 2001-08-30  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/dimera/dimera3500.c (camera_file_get): Use
 *         gp_file_adjust_name_for_mime_type
 *         * libgphoto2/file.c (gp_file_adjust_name_for_mime_type):
 *         GP_MIME_RAW -> *.raw
 *
 * Revision 1.15  2001/08/30 10:47:51  lutz
 * 2001-08-29  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/barbie/?*: Use camera->port and camera->fs.
 *         * camlibs/dimera/dimera3500.c
 *         (populate_filesystem): Removed.
 *         (camera_folder_list_folders): Removed. We don't support folders anyways
 *         (camera_folder_list_files): Renamed to
 *         (file_list_func): Don't access the CameraFilesystem here. Just
 *         populate the list. The CameraFilesystem will get updated by libgphoto2.
 *         (camera_file_get_info): Renamed to
 *         (get_info_func): New
 *         (camera_file_set_info): Removed. Not supported.
 *         (camera_init): Correctly gp_filesystem_set_list_funcs and
 *         gp_filesystem_set_info_funcs so that the CameraFilesystem has full
 *         control over the information
 *         * include/gphoto2-list.h:
 *         * libgphoto2/list.c: (gp_list_populate): New. Please use this
 *         function to populate the list passed to you on file_list_func.
 *         * libgphoto2/filesys.c: Don't remove the dirty flag on folders
 *         on gp_filesystem_append (thanks, Dan!)
 *
 * Revision 1.14  2001/08/29 22:04:54  dfandrich
 * Now using predefined camera->port and camera->fs structures. Removed
 * unneeded include files. Now using predefined MIME types. Made all local
 * functions static. Updated camera_manual text.
 *
 * Revision 1.13  2001/08/29 17:52:57  lutz
 * 2001-08-29  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * gphoto2-library.h: Clean up this file.
 *         * gphoto2-camera.h: We don't need camera_init, camera_abilities, and
 *         camera_id here. That belongs into gphoto2-library.h.
 *         * libgphoto2/camera.c:
 *         * libgphoto2/core.c: Adapt
 *         * camlibs/?*: Move camera_init to the bottom of the file. Camera
 *         driver authors, could you please declare the functions above static?
 *         Except camera_id and camera_abilities.
 *
 * Revision 1.12  2001/08/29 10:09:57  lutz
 * 2001-08-29  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/sierra/?*:
 *         * camlibs/directory/directory.c (camera_folder_list_folders),
 *         (camera_folder_list_files), (camera_file_[get,set]_info): Removed.
 *         Use the camera->fs. Use camera->port.
 *         * camlibs/?*: camera->port->* -> camera->port_info->*
 *         * include/gphoto2-camera.h:
 *         * libgphoto2/camera.c: Open the port before accessing a camera,
 *         close it after.
 *
 * Revision 1.11  2001/08/26 16:06:40  lutz
 * 2001-08-26  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/dimera/dimera3500.c: explicitly set the conversion method
 *         to GP_FILE_CONVERSION_METHOD_CHUCK.
 *         * camlibs/digita/digita.c: Move the conversion raw -> ppm to ...
 *         * include/gphoto2-file.h:
 *         * libgphoto2/file.c: ... here.
 *         * include/gphoto2-filesystem.h:
 *         * libgphoto2/filesys.c: Implement "dirty" folders.
 *
 * Revision 1.10  2001/08/26 10:42:54  lutz
 * 2001-08-26  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/dimera3500.c: Move the raw->pnm conversion ...
 *         * include/gphoto2-file.h:
 *         * libgphto2/file.c: ... here. Now, other camera drivers can use this,
 *         too.
 *         * include/gphoto2-core.c:
 *         * libgphoto2/core.c: Small change to make the API consistent.
 *         * frontends/command-line/main.c: Add possibility for download of raw
 *         data.
 *
 * Revision 1.9  2001/08/25 18:14:25  lutz
 * 2001-08-25  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * include/gphoto2-file.h:
 *         * include/gphoto2-library.h:
 *         * include/gphoto2-camera.h: Support for raw data.
 *         * libpghoto2/file.c: Adjust to above changes.
 *         * libgphoto2/frontend.c: Kill a warning.
 *         * libgphoto2/libgphoto-2.0.pc.in: CFlags -> Cflags
 *         * libgphoto2/widget.c: Include gphoto2-result.h instead of gphoto2.h
 *         * camlibs/?*: Prepare support for download of raw data. Remove lots of
 *         redundant code (people just copied & pasted the code for file_get
 *         and file_get_preview...). I hope everything works as it did before.
 *         * frontend/command-line/?*: Prepare support for download of raw data
 *
 * Revision 1.8  2001/08/24 21:23:11  lutz
 * 2001-08-24  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * libgphoto2/abilities.c:
 *         * include/gphoto2-abilities.h: Let gp_abilities_new return an error
 *         code. This was the last one.
 *         * camlibs/?*: Adjust to above change.
 *         * camlibs/canon/canon.c: #if 0 some code to kill some warnings
 *
 * Revision 1.7  2001/08/23 12:02:20  lutz
 * 2001-08-24  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * camlibs/agfa-cl18/agfa.c: Use gp_file_* instead of directly
 *         accessing the struct.
 *         * camlibs/barbie/barbie.c: And here.
 *         * camlibs/directory/directory.c: And here.
 *         * camlibs/konica/library.c: And here.
 *         * camlibs/sierra/sierra.c: And here.
 *         * camlibs/panasonic/l859/l859.c: And here.
 *         * camlibs/dimera/dimera3500.c: And here.
 *         * frontends/command-line/main.c: And here.
 *         * libgphoto2/camera.c: And here.
 *         * libgphoto2/file.c:
 *         * include/gphoto2-file.h: Introduce some more gp_file_* functions.
 *
 * Revision 1.6  2001/08/22 20:59:57  hfiguiere
 * 	* camlibs/dimera/dimera3500.c: removed * / * embedded inside comments
 * 	that issued warnings and broke the build...
 * 	* camlibs/kodak/dc240/library.c (dc240_wait_for_busy_completion):
 * 	cast a const to a char to avoid a serious warning on Solaris sparc
 * 	gcc. Fix bug #454183
 *
 * Revision 1.5  2001/08/22 18:09:41  lutz
 * 2001-08-22  Lutz Müller <urc8@rz.uni-karlsruhe.de>
 *
 *         * * / *: Small parameter changes to make gphoto2 API more consistent.
 *         More to follow.
 *
 * Revision 1.4  2001/08/18 00:03:05  lutz
 * 2001-08-17  Lutz Müller  <urc8@rz.uni-karlsruhe.de>
 *
 *         * include/gphoto2-datatypes.h: Move the declaration of the lists to ...
 *         * include/gphoto2-lists.h: ... here. This is a first step towards
 *         cleaning up the include-mess. Realized that CameraListType isn't
 *         needed at all - removed.
 *         * include/gphoto2-filesys.h: Hide the actual contents of
 *         the CameraFilesystem ...
 *         * libgphoto2/filesys.c ... here. The normal user doesn't need to
 *         know what's inside.
 *         * libgphoto2/core.c:
 *         * libgphoto2/lists.c:
 *         * camlibs/ *: Reflect above changes
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

#include <gphoto2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mesalib.h"
#include "dimeratab.h"

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

static u_int8_t *
Dimera_Get_Full_Image (int picnum, int *size, Camera *camera,
		       int *width, int *height);

static u_int8_t *
Dimera_Get_Thumbnail( int picnum, int *size, Camera *camera );

static u_int8_t *
Dimera_Preview( int *size, Camera *camera );

/* Gphoto2 */

typedef struct {
        unsigned exposure; 
        int auto_exposure; 
        int auto_flash; 
} DimeraStruct;


static char *models[] = {
        "Relisys Dimera 3500",
        "Mustek VDC-3500",
        NULL
};


int camera_id (CameraText *id) {

	strcpy(id->text, "dimera3500");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	int x=0;
	CameraAbilities *a;

	while (models[x]) {

		gp_abilities_new(&a);

		strcpy(a->model, models[x]);
		a->port     = GP_PORT_SERIAL;
		a->speed[0] = 9600;
		a->speed[1] = 14400;
		a->speed[2] = 19200;
		a->speed[3] = 38400;
		a->speed[4] = 57600;
		a->speed[5] = 76800;
		a->speed[6] = 115200;
		a->speed[7] = 0;
		a->operations = GP_OPERATION_CAPTURE_IMAGE;
		a->file_operations  = GP_FILE_OPERATION_PREVIEW;
		a->folder_operations = GP_FOLDER_OPERATION_NONE;

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
		return (GP_ERROR_DIRECTORY_NOT_FOUND);

	count = mesa_get_image_count (camera->port);
	if (count < 0)
		return (count);

	/*
	 * Create pseudo file names for each picture in the camera, except
	 * the temporary picture in RAM, which might not always be available.
	 *
	 * We don't add anything to the filesystem here - the filesystem does
	 * that for us.
	 */
	return (gp_list_populate (list, IMAGE_NAME_TEMPLATE, count));
}

static int camera_file_get (Camera *camera, const char *folder, const char *filename, CameraFileType type, CameraFile *file) {

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
		return std_res;
	}

	info->preview.fields = GP_FILE_INFO_ALL;
	strcpy(info->preview.type, GP_MIME_PGM);
	strcpy(info->preview.name, filename);
	info->preview.permissions = GP_FILE_PERM_READ;
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

static int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) {
        DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;

	if (capture_type == GP_OPERATION_CAPTURE_IMAGE) {
		if (cam->auto_flash) {
			if ( mesa_snap_picture( camera->port, cam->exposure*4 ) != GP_OK)
				return GP_ERROR;
		}
		else {
			if ( mesa_snap_image( camera->port, cam->exposure*4 ) != GP_OK)
				return GP_ERROR;
		}

		/* User must download special RAM_IMAGE_TEMPLATE file */
		strcpy(path->folder, "/");
		strcpy(path->name, RAM_IMAGE_TEMPLATE);
		return ( GP_OK );
	}
	return (GP_ERROR);
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
	u_int8_t eeprom_info[MESA_EEPROM_SZ];
	/* Table of EEPROM capacities in Mb based on part compatibility ID */
	static u_int8_t const eeprom_size_table[14] = {
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
		return num;

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
		sprintf( battery_string, " (battery is %d%% full)",
			mesa_battery_check(camera->port));

	sprintf( summary->text, 
			"Dimera 3500 ver. %s %d/%d %d:%d\n"
			"%d pictures used of approximately %d (high res) or %d (low res)\n"
			"Camera features: "
			  " %sFlash, %sDual Iris, %sResolution Switch, %sPower Light\n"
			"Flash is %s, is %sready and is %sin fill mode\n"
			"Resolution is set to %s\n"
			"Camera is %sternally powered%s\n",
			version_string, Id.year, Id.week, Id.man, Id.ver,
			num, hi_pics_max, lo_pics_max,
			(features.feature_bits_lo & HAVE_FLASH) ? "" : "NO ",
			(features.feature_bits_lo & DUAL_IRIS) ? "" : "NO ",
			(features.feature_bits_lo & HAVE_RES_SW) ? "" : "NO ",
			(features.feature_bits_hi & NO_PWR_LIGHT) ? "NO " : "",
			(features.feature_bits_lo & FLASH_ON) ? "ON" : "OFF",
			(features.feature_bits_lo & FLASH_READY) ? "" : "NOT ",
			(features.feature_bits_lo & FLASH_FILL) ? "" : "NOT ",
			(features.feature_bits_lo & LOW_RES) ? "low (320x240)" : "high (640x480)",
			(features.feature_bits_lo & AC_PRESENT) ? "ex" : "in",
			battery_string
	);

	return GP_OK;
}

static int camera_manual (Camera *camera, CameraText *manual) {

        strcpy(manual->text,
	"  Image glitches or problems communicating are\n"
	"often caused by a low battery.\n"
	"  Images captured remotely on this camera are stored\n"
	"in temporary RAM and not in the flash memory card.\n"
	"  Exposure control when capturing all images is\n"
	"automatically set by the capture preview function.\n"
	"  Image quality is currently lower than it could be.\n"
	);

        return GP_OK;
}

static int camera_about (Camera *camera, CameraText *about) {
	strcpy(about->text,
		"gPhoto2 Mustek VDC-3500/Relisys Dimera 3500\n"
		"This software was created with the\n"
		"help of proprietary information belonging\n"
		"to StarDot Technologies.\n"
		"Author:\n"
		"Brian Beattie http://www.beattie-home.net\n"
		"Contributors:\n"
		"Chuck Homic <chuck@vvisions.com>\n"
		"     Converting raw camera images to RGB\n"
		"Dan Fandrich <dan@coneharvesters.com>\n"
		"     Information on protocol, raw image format,\n"
		"     gphoto2 port\n"
		);
	return GP_OK;
}



/* Download a thumbnail image from the camera and return it in a malloced
buffer with PGM headers */
static u_int8_t *
Dimera_Get_Thumbnail( int picnum, int *size, Camera *camera )
{
	int32_t		r;
	u_int8_t *image;

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
static u_int8_t *
Dimera_Get_Full_Image (int picnum, int *size, Camera *camera,
		       int *width, int *height)
{
	static struct mesa_image_arg	ia;
	int32_t				r;
	u_int8_t			*b, *rbuffer = NULL;
	int				hires, s, retry;

	*size = 0;
	*width = 0;
	*height = 0;

	if ( picnum != RAM_IMAGE_NUM )
	{
		update_status( "Getting Image Info" );
		if ( (r = mesa_read_image_info( camera->port, picnum, NULL )) < 0 )
		{
			ERROR("Can't get Image Info");
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

		update_status( "Loading Image" );
		if ( mesa_load_image( camera->port, picnum ) != GP_OK )
		{
			ERROR("Image Load failed");
			return NULL;
		}

	} else {
			/* load the snapped image */
		hires = TRUE;
		*height = 480;
		*width = 640;
	}

	*size = *height * *width;	/* 8 bits per pixel in raw CCD format */

	update_status( "Downloading Image" );

	rbuffer = (u_int8_t *)malloc( *size );
	if ( rbuffer == NULL )
	{
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
		update_status( "Downloading Image" );
		for ( retry = 10;; )
		{

			s = mesa_read_image( camera->port, b, &ia );
			if( s > 0)
				break;

			if ( (s == GP_ERROR_TIMEOUT || s == GP_ERROR_CORRUPTED_DATA) && --retry > 0)
			{
				update_status( "Retransmitting" );
				gp_debug_printf(GP_DEBUG_LOW, "dimera", "Dimera_Get_Full_Image: retrans"); 
				continue;
			}
			gp_debug_printf(GP_DEBUG_LOW, "dimera",
				"Dimera_Get_Full_Image: read error %d (retry %d)",s,retry);
				/* retry count exceeded, or other error */
			free( rbuffer );
			*size = 0;
			return 0;
		}
		gp_camera_progress(camera, ia.row / (height + 4) );
	}
#else
	for ( ia.row = 4, b = rbuffer; ia.row < (*height + 4) ;
			ia.row++, b += s )
	{
		update_status( "Downloading Image" );
		for ( retry = 10;; )
		{

			s = mesa_read_row( camera->port, b, &ia );
			if( s > 0)
				break;

			if ( (s == GP_ERROR_TIMEOUT || s == GP_ERROR_CORRUPTED_DATA) && --retry > 0)
			{
				update_status( "Retransmitting" );
				gp_debug_printf(GP_DEBUG_LOW, "dimera", "Dimera_Get_Full_Image: retrans"); 
				continue;
			}
			gp_debug_printf(GP_DEBUG_LOW, "dimera",
				"Dimera_Get_Full_Image: read error %d (retry %d)",s,retry);
				/* retry count exceeded, or other error */
			free( rbuffer );
			*size = 0;
			return 0;
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
static u_int8_t *
Dimera_Preview( int *size, Camera *camera )
{
	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;
	u_int8_t buffer[VIEWFIND_SZ/2], *p;
	int		i;
	u_int8_t *image;
	u_int32_t exposure_total;
	unsigned brightness;

	if ( !(image = (unsigned char *) malloc( VIEWFIND_SZ +
			sizeof( Dimera_viewhdr ) - 1 )) )
	{
		ERROR( "Get Preview, allocation failed" );
		return NULL;
	}

	/* set image size */
	*size = VIEWFIND_SZ + sizeof( Dimera_viewhdr ) - 1;

	/* set image header */
	memcpy( image, Dimera_viewhdr, sizeof( Dimera_viewhdr ) - 1 );

	if ( mesa_snap_view( camera->port, buffer, TRUE, 0, 0, 0, cam->exposure,
			VIEW_TYPE) < 0 )
	{
		ERROR( "Get Preview, mesa_snap_view failed" );
		free (image);
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
		brightness / 16.0, cam->exposure);

	if (cam->auto_exposure && (brightness < 96 || brightness > 160)) {
		/* Picture brightness needs to be corrected for next time */
		cam->exposure = calc_new_exposure(cam->exposure, brightness);
		gp_debug_printf(GP_DEBUG_LOW, "dimera", "New exposure value: %d", cam->exposure);
	}

	return image;
}

int camera_init (Camera *camera) {

        DimeraStruct *cam;
        int ret;

        debuglog("camera_init()");

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->file_get             = camera_file_get;
        //camera->functions->folder_put_file    = camera_folder_put_file;
        //camera->functions->file_delete        = camera_file_delete;
        //camera->functions->folder_delete_all  = camera_folder_delete_all;
        //camera->functions->config             = camera_config;
        //camera->functions->get_config         = camera_get_config;
        //camera->functions->set_config         = camera_set_config;
        //camera->functions->folder_get_config  = camera_folder_get_config;
        //camera->functions->folder_set_config  = camera_folder_set_config;
        //camera->functions->file_get_config    = camera_file_get_config;
        //camera->functions->file_set_config    = camera_file_set_config;
        camera->functions->capture              = camera_capture;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->summary              = camera_summary;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;
        //camera->functions->result_as_string   = camera_result_as_string;

        cam = (DimeraStruct*)malloc(sizeof(DimeraStruct));
        if (cam == NULL)
                return GP_ERROR;
        camera->camlib_data = cam;

        /* Set the default exposure */
        cam->exposure = DEFAULT_EXPOSURE;

        /* Enable automatic exposure setting for capture preview mode */
        cam->auto_exposure = 1;

        /* Use flash, if necessary, when capturing picture */
        cam->auto_flash = 1;

        debuglog("Opening port");
        if ( (ret = mesa_port_open(camera->port, camera->port_info->path)) != GP_OK)
        {
                ERROR("Camera Open Failed");
                return ret;
        }

        debuglog("Resetting camera");
        if ( (ret = mesa_reset(camera->port)) != GP_OK )
        {
                ERROR("Camera Reset Failed");
                return ret;
        }

        
        if ( (ret = mesa_set_speed(camera->port, camera->port_info->speed)) != GP_OK )
        {
                ERROR("Camera Speed Setting Failed");
                return ret;
        }


        debuglog("Checking for modem");
        switch ( mesa_modem_check(camera->port) )
        {
        case GP_ERROR_IO:
        case GP_ERROR_TIMEOUT:
                ERROR("No or Unknown Response");
                return GP_ERROR_TIMEOUT;
        case GP_ERROR_MODEL_NOT_FOUND:
                ERROR("Probably a modem");
                return GP_ERROR;
        case GP_OK:
                break;
        }

	/* Tell the filesystem where to get listings and info from */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_info_funcs (camera->fs, get_info_func, NULL, camera);

	return (GP_OK);
}
