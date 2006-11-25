#ifndef _DC210_H_
#define _DC210_H_

typedef enum {
	DC210_TOGGLE_OFF = 0,
	DC210_TOGGLE_ON = 1
} dc210_toggle_type;

typedef enum {
	DC210_FULL_PICTURE = 0,
	DC210_CFA_THUMB = 1,
	DC210_RGB_THUMB = 2
} dc210_picture_type;

typedef enum {
	DC210_FILE_TYPE_JPEG = 3,
	DC210_FILE_TYPE_FPX = 4
} dc210_file_type_type;

typedef enum {
	DC210_FLASH_AUTO = 0,
	DC210_FLASH_FORCE = 1,
	DC210_FLASH_NONE = 2
} dc210_flash_type;

typedef enum {
	DC210_FILE_640 = 0,
	DC210_FILE_1152 = 1
} dc210_resolution_type;

typedef enum {
	DC210_LOW_COMPRESSION = 1,
	DC210_MEDIUM_COMPRESSION = 2,
	DC210_HIGH_COMPRESSION = 3

} dc210_compression_type;

typedef enum {
	DC210_ZOOM_58 = 0,
	DC210_ZOOM_51 = 1,
	DC210_ZOOM_41 = 2,
	DC210_ZOOM_34 = 3,
	DC210_ZOOM_29 = 4,
	DC210_ZOOM_MACRO = 37
} dc210_zoom_type;

typedef struct {

	unsigned char           open;
	int                     program;
	int                     space;

} dc210_card_status;

typedef struct {
	char                    camera_type_id;   
	char                    firmwareMajor;    
	char                    firmwareMinor;  
	char                    battery;     	
	char                    acstatus;       
	long int                time;	        
	dc210_zoom_type         zoom;     	
	char                    flash_charged;	
	dc210_compression_type  compression_type; 
        signed char             exp_compensation;
	dc210_flash_type        flash;
	char                    preflash;
	dc210_resolution_type   resolution;		
        dc210_file_type_type    file_type;		
	int                     totalPicturesTaken;     
	int                     totalFlashesFired;      
	int                     numPicturesInCamera;    
	dc210_card_status       card_status;
	int                     remainingLow;           
	int                     remainingMedium;        
	int                     remainingHigh;          
	int                     card_space;
	char                    album_name[12];         
} dc210_status;

typedef struct {
	char                    camera_type;
	dc210_file_type_type    file_type;
	dc210_resolution_type   resolution;
	dc210_compression_type  compression;
	int                     picture_number;
	int                     picture_size;
	int                     preview_size;
	int                     picture_time;
	char                    flash_used;
	dc210_flash_type        flash;
	char                    preflash;
	dc210_zoom_type         zoom;
	char                    f_number;
	char                    battery;
	int                     exposure_time;
	char                    image_name[13];
} dc210_picture_info ;

#undef DEBUG
#define TIMEOUT 500
#define RETRIES 5
#define GP_MODULE "kodak-dc210"

#ifdef DEBUG
#define DC210_DEBUG(msg, params...) fprintf(stderr, msg, ##params)
#else
#define DC210_DEBUG GP_DEBUG
#endif

/* I really don't get it, why I have a shift of seven hours in
   each direction to get/set the camara time correctly. I'm afraid
   it's dependent on the timezone  (and an outdated clib)*/

/* #define CAMERA_EPOC              852094800*/
#define CAMERA_GET_EPOC              852094800 - 25200
#define CAMERA_SET_EPOC              852094800 + 25200

/* Packet sizes */
#define DC210_CMD_DATA_SIZE         58
#define DC210_STATUS_SIZE          256
#define DC210_PICINFO_SIZE         256
#define DC210_CARD_BLOCK_SIZE      512
#define DC210_DIRLIST_SIZE         256
#define DC210_DOWNLOAD_BLOCKSIZE  1024

/* Commands */
#define DC210_SET_RESOLUTION        0x36 /* implemented */
#define DC210_SET_FILE_TYPE         0x37 /* implemented */
#define DC210_SET_SPEED             0x41 /* implemented */
#define DC210_GET_ALBUM_FILENAMES   0x4A /* returns information on all files in packets of size 256 */
#define DC210_GET_PICTURE	    0x64 /* implemented */
#define DC210_GET_PICINFO           0x65 /* implemented */
#define DC210_GET_THUMB             0x66 /* implemented */
#define DC210_SET_QUALITY           0x71 /* implemented */
#define DC210_SET_FLASH             0x72 /* implemented */
#define DC210_SET_FOCUS		    0x73 /* not acknowledged */
#define DC210_SET_DELAY		    0x74 /* implemented, but effect unknown */
#define DC210_SET_TIME		    0x75 /* implemented, but probably with timezone bug */
#define DC210_SET_ZOOM              0x78 /* implemented */
#define DC210_DELETE_PICTURE        0x7B /* implemented */
#define DC210_TAKE_PICTURE          0x7C /* implemented */
#define DC210_CHECK_BATTERY         0x7E /* implemented */
#define DC210_GET_STATUS            0x7F /* implemented */

#define DC210_SET_EXPOSURE          0x80 /* implemented */
#define DC210_RESET		    0x8A /* working, but not yet implemented */
#define DC210_FIRMWARE_MODE_SET	    0x8D /* working, but not yet implemented */

/* Filename commands */
#define DC210_CARD_GET_PICINFO	    0x91 /* implemented */
#define DC210_CARD_GET_SUMMARY	    0x92 /* not implemented */
#define DC210_CARD_READ_THUMB	    0x93 /* implemented */
#define DC210_CARD_FORMAT	    0x95 /* implemented */
#define DC210_OPEN_CARD		    0x96 /* implemented */
#define DC210_CLOSE_CARD	    0x97 /* implemented */
#define DC210_CARD_STATUS	    0x98 /* is working, returns packet of size 16 */
#define DC210_CARD_GET_DIRECTORY    0x99 /* not implemented */
#define DC210_CARD_READ_FILE	    0x9A /* implemented */
#define DC210_CARD_UNKNOWN_COMMAND1 0x9C /* not implemented */
#define DC210_CARD_FILE_DEL	    0x9D /* implemented */
#define DC210_CARD_UNKNOWN_COMMAND2 0x9E /* not implemented */

/* Responses */
#define DC210_COMMAND_COMPLETE      0x00
#define DC210_PACKET_FOLLOWING      0x01
#define DC210_CMD_PACKET_FOLLOWING  0x80
#define DC210_CAMERA_READY          0x08
#define DC210_COMMAND_ACK           0xD1
#define DC210_CORRECT_PACKET        0xD2
#define DC210_COMMAND_NAK           0xE1
#define DC210_EXECUTION_ERROR       0xE2
#define DC210_ILLEGAL_PACKET        0xE3
#define DC210_CANCEL		    0xE4
#define DC210_BUSY                  0xF0
  
/* initialization */
int dc210_init_port (Camera *camera);

/* set procedures */
int dc210_set_compression (Camera * camera, dc210_compression_type compression);
int dc210_set_flash (Camera * camera, dc210_flash_type flash, char preflash);
int dc210_set_zoom (Camera * camera, dc210_zoom_type zoom);
int dc210_set_file_type (Camera * camera, dc210_file_type_type file_type);
int dc210_set_resolution (Camera * camera, dc210_resolution_type res);
int dc210_set_speed (Camera *camera, int speed);
int dc210_set_delay (Camera * camera);
int dc210_set_exp_compensation (Camera * camera, signed int compensation);

/* information */
int dc210_get_status    (Camera *camera, dc210_status *status);
int dc210_get_picture_info (Camera *camera, dc210_picture_info *picinfo, unsigned int picno);
int dc210_get_picture_info_by_name (Camera *camera, dc210_picture_info *picinfo, const char * filename);
int dc210_get_filenames (Camera *camera, CameraList *list, GPContext *context);
int dc210_get_picture_number (Camera *camera, const char * filename);

/* picture actions */
int dc210_take_picture (Camera * camera, GPContext *context);
int dc210_capture       (Camera *camera, CameraFilePath *path, GPContext *context);

int dc210_download_last_picture (Camera * camera, CameraFile *file, dc210_picture_type type, GPContext *context);
int dc210_download_picture_by_name (Camera * camera, CameraFile *file, const char *filename, dc210_picture_type type, GPContext *context);

int dc210_delete_picture (Camera * camera, unsigned int picno);
int dc210_delete_last_picture (Camera * camera );
int dc210_delete_picture_by_name (Camera * camera, const char * filename );

/* other actions */
int dc210_open_card (Camera * camera);

/* callbacks */
int dc210_system_time_callback (Camera * camera, CameraWidget * widget, GPContext * context);
int dc210_format_callback(Camera * camera, CameraWidget * widget, GPContext * context);
#ifdef DEBUG
int dc210_debug_callback(Camera * camera, CameraWidget * widget, GPContext * context);
#endif

#endif /* _DC210_H_ */

