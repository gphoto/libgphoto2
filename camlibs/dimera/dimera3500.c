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
#include <unistd.h>
#include <math.h>
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
#define RAM_IMAGE_TEMPLATE "temp.ppm"
#define PNM_MIME_TYPE "image/x-portable-anymap"

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
Dimera_Get_Full_Image( int picnum, int *size, Camera *camera );

static u_int8_t *
Dimera_Get_Thumbnail( int picnum, int *size, Camera *camera );

static u_int8_t *
Dimera_Preview( int *size, Camera *camera );

/* Gphoto2 */

typedef struct {
        gp_port *dev;
        unsigned exposure; 
        int auto_exposure; 
        int auto_flash; 
        CameraFilesystem *fs;
} DimeraStruct;


static char *models[] = {
        "Relisys Dimera 3500",
        "Mustek VDC-3500",
        NULL
};

/* Create pseudo file names for each picture in the camera, except
the temporary picture in RAM, which might not always be available. */
static int populate_filesystem(gp_port *port, CameraFilesystem *fs) {
        int count = mesa_get_image_count(port);

        if (count < 0)
        	return count;

        /* Populate the filesystem */
        gp_filesystem_populate(fs, "/", IMAGE_NAME_TEMPLATE, count);

	return GP_OK;
}


int camera_id (CameraText *id) {

	strcpy(id->text, "dimera3500");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	int x=0;
	CameraAbilities *a;

	while (models[x]) {

		a = gp_abilities_new();

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


int camera_init (Camera *camera) {

	DimeraStruct *cam;

	debuglog("camera_init()");

	/* First, set up all the function pointers */
	camera->functions->id 			= camera_id;
	camera->functions->abilities 		= camera_abilities;
	camera->functions->init 		= camera_init;
	camera->functions->exit 		= camera_exit;
	camera->functions->folder_list_folders 	= camera_folder_list_folders;
	camera->functions->folder_list_files	= camera_folder_list_files;
	camera->functions->file_get_info	= camera_file_get_info;
	camera->functions->file_set_info	= camera_file_set_info;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	//camera->functions->folder_put_file	= camera_folder_put_file;
	//camera->functions->file_delete 	= camera_file_delete;
	//camera->functions->folder_delete_all 	= camera_folder_delete_all;
	//camera->functions->config		= camera_config;
	//camera->functions->get_config   	= camera_get_config;
	//camera->functions->set_config   	= camera_set_config;
	//camera->functions->folder_get_config 	= camera_folder_get_config;
	//camera->functions->folder_set_config 	= camera_folder_set_config;
	//camera->functions->file_get_config 	= camera_file_get_config;
	//camera->functions->file_set_config 	= camera_file_set_config;
	camera->functions->capture 		= camera_capture;
	camera->functions->capture_preview 	= camera_capture_preview;
	camera->functions->summary		= camera_summary;
	camera->functions->manual 		= camera_manual;
	camera->functions->about 		= camera_about;
	//camera->functions->result_as_string 	= camera_result_as_string;

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
	if (mesa_port_open(&cam->dev, camera->port->path) != GP_OK)
	{
		ERROR("Camera Open Failed");
		return GP_ERROR;
	}

	/* Create the filesystem */
	gp_filesystem_new(&cam->fs);

	debuglog("Resetting camera");
	if ( mesa_reset(cam->dev) != GP_OK )
	{
		ERROR("Camera Reset Failed");
		return GP_ERROR;
	}

	mesa_set_speed(cam->dev, camera->port->speed);

	debuglog("Checking for modem");
	switch ( mesa_modem_check(cam->dev) )
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

	/* Create pseudo file names for each picture */
	return populate_filesystem(cam->dev, cam->fs);
}

int camera_exit(Camera *camera) {

	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;

	gp_filesystem_free(cam->fs);
	return mesa_port_close(cam->dev);
}

int camera_folder_list_folders(Camera *camera, const char *folder, CameraList *list) {
	/* there are never any subfolders */

	return (GP_OK);
}

int camera_folder_list_files(Camera *camera, const char *folder, CameraList *list) {
	int x;
	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;
	const char *name;

	/* Update filesystem in case images were added */
	populate_filesystem(cam->dev, cam->fs);

	for (x=0; x<gp_filesystem_count(cam->fs, folder); x++) {
		gp_filesystem_name (cam->fs, folder, x, &name);
		gp_list_append(list, name, NULL);
	}
	return GP_OK;
}

int camera_file_get (Camera *camera, const char *folder, const char *filename, CameraFile *file) {

	int size, num;
	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;

	gp_frontend_progress(camera, NULL, 0.00);

	strcpy(file->name, filename);
	strcpy(file->type, PNM_MIME_TYPE);

	/* Retrieve the number of the photo on the camera */
	if (strcmp(filename, RAM_IMAGE_TEMPLATE) == 0)
		/* Magic file name specifies magic image number */
		num = RAM_IMAGE_NUM;
	else
		num = gp_filesystem_number(cam->fs, "/", filename);

	if (num < 0)
		return num;

	file->data = Dimera_Get_Full_Image(num, &size, camera);
	if (!file->data)
			return GP_ERROR;
	file->size = size;

	return GP_OK;
}

int camera_file_get_info (Camera *camera, const char *folder, const char *filename, CameraFileInfo *info) {
	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;
	int num, std_res;

	num = gp_filesystem_number(cam->fs, folder, filename);
	if (num < 0)
		return num;

	if ( (std_res = mesa_read_image_info( cam->dev, num, NULL )) < 0 )
	{
		ERROR("Can't get Image Info");
		return std_res;
	}

	info->preview.fields = GP_FILE_INFO_ALL;
	strcpy(info->preview.type, PNM_MIME_TYPE);
	strcpy(info->preview.name, filename);
	info->preview.permissions = GP_FILE_PERM_READ;
	info->preview.size = MESA_THUMB_SZ + sizeof( Dimera_thumbhdr ) - 1;
	info->preview.width = 64;
	info->preview.height = 48;

	info->file.fields = GP_FILE_INFO_ALL;
	strcpy(info->file.type, PNM_MIME_TYPE);
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

int camera_file_set_info (Camera *camera, const char *folder, const char *filename, CameraFileInfo *info) {
	    return GP_ERROR_NOT_SUPPORTED;
}

int camera_file_get_preview (Camera *camera, const char *folder, const char *filename, CameraFile *file) {
        DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;
        int size, num;

        /* Retrieve the number of the photo on the camera */
        num = gp_filesystem_number(cam->fs, "/", filename);
		if (num < 0)
			return num;

        strcpy(file->name, filename);
        strcpy(file->type, PNM_MIME_TYPE);

        file->data = Dimera_Get_Thumbnail(num, &size, camera);
        if (!file->data)
                return GP_ERROR;
        file->size = size;

        return GP_OK;
}

int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) {
        DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;

	if (capture_type == GP_OPERATION_CAPTURE_IMAGE) {
		if (cam->auto_flash) {
			if ( mesa_snap_picture( cam->dev, cam->exposure*4 ) != GP_OK)
				return GP_ERROR;
		}
		else {
			if ( mesa_snap_image( cam->dev, cam->exposure*4 ) != GP_OK)
				return GP_ERROR;
		}

		/* User must download special RAM_IMAGE_TEMPLATE file */
		strcpy(path->folder, "/");
		strcpy(path->name, RAM_IMAGE_TEMPLATE);
		return ( GP_OK );
	}
	return (GP_ERROR);
}

int camera_capture_preview(Camera *camera, CameraFile *file) {
        int size;

        strcpy(file->name, RAM_IMAGE_TEMPLATE);
        strcpy(file->type, PNM_MIME_TYPE);

        file->data = Dimera_Preview(&size, camera);
        if (!file->data)
                return GP_ERROR;
        file->size = size;

        return GP_OK;
}

int camera_summary (Camera *camera, CameraText *summary) {
	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;
	int num, eeprom_capacity, hi_pics_max, lo_pics_max;
	struct mesa_id Id;
	char version_string[MESA_VERSION_SZ];
	char battery_string[50];
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

	num = mesa_get_image_count(cam->dev);
	if (num < 0)
		return num;

	mesa_send_id( cam->dev, &Id );
	mesa_version(cam->dev, version_string);
	mesa_read_features(cam->dev, &features);
	mesa_eeprom_info(cam->dev, 1, eeprom_info);
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
			mesa_battery_check(cam->dev));

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

int camera_manual (Camera *camera, CameraText *manual) {

        strcpy(manual->text,
	"  Poor image quality or problems communicating are\n"
	"often caused by a low battery.\n"
	"  Images captured remotely on this camera are stored\n"
	"in temporary RAM and not in the flash memory card.\n"
	"  Exposure control when capturing all images is\n"
	"automatically set by the capture preview function.\n"
	);

        return GP_OK;
}

int camera_about (Camera *camera, CameraText *about) {
	strcpy(about->text,
		"gPhoto2 Mustek VDC 3500/Relisys Dimera 3500\n"
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



static u_int8_t *
Dimera_Get_Thumbnail( int picnum, int *size, Camera *camera )
{
	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;
	int32_t		r;
	u_int8_t *image;

	if ( !(image = (unsigned char *) malloc( MESA_THUMB_SZ +
			sizeof( Dimera_thumbhdr ) - 1 )) )
	{
		ERROR( "Get Thumbnail, allocation failed" );
		*size = 0;
		return 0;
	}

	/* set image size */
	*size = MESA_THUMB_SZ + sizeof( Dimera_thumbhdr ) - 1;

	/* set image header */
	memcpy( image, Dimera_thumbhdr, sizeof( Dimera_thumbhdr ) - 1 );

	if ( (r = mesa_read_thumbnail( cam->dev, picnum, image +
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

static u_int8_t *
Dimera_convert_raw( u_int8_t *rbuffer, int height, int width,
		int *size )
{
	int		x, y;
	int		p1, p2, p3, p4;
	int		red, green, blue;
	u_int8_t	*b,*oimage;

	if ( (oimage = (u_int8_t *)malloc( height*width*3 +
			sizeof( Dimera_finehdr ) - 1 )) == NULL )
	{
		ERROR( "Dimera_convert_raw: alloc failed" );
		return 0;
	}
	if( width == 640 )
	{
		strcpy( oimage, Dimera_finehdr );
	} else {
		strcpy( oimage, Dimera_stdhdr );
	}

	b = oimage + sizeof( Dimera_finehdr ) - 1;

	/*
	   Added by Chuck -- 12-May-2000
	   Convert the colors based on location, and my wacky
	   color table
	*/
/*									  GDB
	Convert the 4 cells into a single RGB pixel. 			  GDB
 									  GDB
	Colors are encoded as follows:					  GDB
 									  GDB	
			000000000000000000........33			  GDB
			000000000011111111........11			  GDB
			012345678901234567........89			  GDB
			----------------------------			  GDB
		000	RGRGRGRGRGRGRGRGRG........RG			  GDB
		001	GBGBGBGBGBGBGBGBGB........GB			  GDB
		002	RGRGRGRGRGRGRGRGRG........RG			  GDB
		003	GBGBGBGBGBGBGBGBGB........GB			  GDB
		...	............................			  GDB
		238	RGRGRGRGRGRGRGRGRG........RG			  GDB
		239	GBGBGBGBGBGBGBGBGB........GB			  GDB
									  GDB
	NOTE. Above is 320x240 resolution.  Just expand for 640x480.	  GDB
									  GDB
	Use neighboring cells to generate color values for each pixel.	  GDB
	The neighbors are the (x-1, y-1), (x, y-1), (x-1, y), and (x, y)  GDB
	The exception is when x or y are zero then the neighors used are  GDB
	the +1 cells.							  GDB
 									  GDB*/
	for (y=0;y<height;y++) for (x=0;x<width;x++) {
		p1 = ((y==0?y+1:y-1)*width) + (x==0?x+1:x-1);
		p2 = ((y==0?y+1:y-1)*width) + x;
		p3 = (y*width) + (x==0?x+1:x-1);
		p4 = (y*width) + x;

		switch ( ((y & 1) << 1) + (x & 1) ) {			/*GDB*/
			case 0: /* even row, even col, red */		/*GDB*/
				blue  = blue_table[rbuffer[ p1 ]];
				green = green_table[rbuffer[ p2 ]];
				green+= green_table[rbuffer[ p3 ]];
				red   = red_table[rbuffer[ p4 ]];
				break;
			case 1: /* even row, odd col, green */		/*GDB*/
				green = green_table[rbuffer[ p1 ]];
				blue  = blue_table[rbuffer[ p2 ]];
				red   = red_table[rbuffer[ p3 ]];
				green+= green_table[rbuffer[ p4 ]];
				break;
			case 2:	/* odd row, even col, green */		/*GDB*/
				green = green_table[rbuffer[ p1 ]];
				red   = red_table[rbuffer[ p2 ]];
				blue  = blue_table[rbuffer[ p3 ]];
				green+= green_table[rbuffer[ p4 ]];
				break;
			case 3:	/* odd row, odd col, blue */		/*GDB*/
			default:
				red   = red_table[rbuffer[ p1 ]];
				green = green_table[rbuffer[ p2 ]];
				green+= green_table[rbuffer[ p3 ]];
				blue  = blue_table[rbuffer[ p4 ]];
				break;
		}
		*b++=(unsigned char)red;
		*b++=(unsigned char)(green/2);
		*b++=(unsigned char)blue;
	}
	

	*size = height*width*3 + sizeof( Dimera_finehdr ) - 1;
	return oimage;
}

static u_int8_t *
Dimera_Get_Full_Image( int picnum, int *size, Camera *camera )
{
	DimeraStruct *cam = (DimeraStruct*)camera->camlib_data;
	static struct mesa_image_arg	ia;
	int32_t				r;
	u_int8_t			*b, *rbuffer, *final_image;
	int				height, width, hires, s, retry;

	*size = 0;
	if ( picnum != RAM_IMAGE_NUM )
	{
		update_status( "Getting Image Info" );
		if ( (r = mesa_read_image_info( cam->dev, picnum, NULL )) < 0 )
		{
			ERROR("Can't get Image Info");
			return NULL;
		}
		if ( r )
		{
			hires = FALSE;
			height = 240;
			width = 320;
		} else {
			hires = TRUE;
			height = 480;
			width = 640;
		}

		update_status( "Loading Image" );
		if ( mesa_load_image( cam->dev, picnum ) != GP_OK )
		{
			ERROR("Image Load failed");
			return NULL;
		}

	} else {
			/* load the snapped image */
		hires = TRUE;
		height = 480;
		width = 640;
	}
	update_status( "Downloading Image" );

	rbuffer = (u_int8_t *)malloc( height*width );
	if ( rbuffer == NULL )
	{
		return 0;
	}

	ia.start = 28;
	ia.send = 4;
	ia.skip = 0;
	ia.repeat = ( hires ? 160 : 80 );
	ia.row_cnt = 40;
	ia.inc1 = 1;
	ia.inc2 = 128;
	ia.inc3 = ia.inc4 = 0;

	gp_frontend_progress(camera, NULL, 0 );

	/* due to reports of mesa_read_image not working for some cameras */
	/* this is changed to use mesa_read_row */
#if 0
	for ( ia.row = 4, b = rbuffer; ia.row < (height + 4) ;
			ia.row += ia.row_cnt, b += s )
	{
		update_status( "Downloading Image" );
		for ( retry = 10;; )
		{

			s = mesa_read_image( cam->dev, b, &ia );
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
		gp_frontend_progress(camera, NULL, 100 * ia.row / (height + 4) );
	}
#else
	for ( ia.row = 4, b = rbuffer; ia.row < (height + 4) ;
			ia.row++, b += s )
	{
		update_status( "Downloading Image" );
		for ( retry = 10;; )
		{

			s = mesa_read_row( cam->dev, b, &ia );
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
		gp_frontend_progress(camera, NULL, 100 * ia.row / (height + 4) );
	}
#endif
	final_image = Dimera_convert_raw( rbuffer, height, width, size );
	free( rbuffer );
	return final_image;
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

	if ( mesa_snap_view( cam->dev, buffer, TRUE, 0, 0, 0, cam->exposure,
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
		/* Picture brightness needs to be corrected */
		cam->exposure = calc_new_exposure(cam->exposure, brightness);
		gp_debug_printf(GP_DEBUG_LOW, "dimera", "New exposure value: %d", cam->exposure);
	}

	return image;
}

