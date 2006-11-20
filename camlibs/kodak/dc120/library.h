
#ifndef _DC120_LIBRARY_H_
#define _DC120_LIBRARY_H_


#define CAMERA_EPOC              852094800

#define DC120_ACTION_IMAGE      0
#define DC120_ACTION_PREVIEW    1
#define DC120_ACTION_DELETE     2   

/* Status structure. Copied from dc210, thus may not fit. */
typedef struct {
    char    camera_type_id;               /* 1 */
    char    firmware_major;               /* 2 */
    char    firmware_minor;               /* 3 */
    char    batteryStatusId;     	        /* 8 battery status */
    char    acStatusId;                   /* 9 */
    unsigned long time;	        	/* 12-15 */
    char    af_mode;  	     	        /* 16 */
    char    zoom_mode;     	     	/* 16 */
    char    flash_charged;		/* 18 */
    char    compression_mode_id;        	/* 19 */
    char    flash_mode;	        	/* 20 */
    char    exposure_compensation;	/* 21 */
    char    light_value;                  /* 22 measured light */
    char    manual_exposure;              /* 23 */
    unsigned long exposure_time;          /* 24-27*/
    char    shutter_delay;                /* 29 */
    char    memory_card;                  /* 30 */
    char    front_cover;                  /* 31 */
    char    date_format;                  /* 32 */
    char    time_format;                  /* 33 */
    char    distance_format;              /* 34 */
    unsigned short taken_pict_mem;        /* 36-37 */
    unsigned short remaining_pic_mem[4];  /* 46-53 */
    unsigned short taken_pict_card;       /* 56-57 */
    unsigned short remaining_pic_card[4]; /* 66-73 */
    unsigned long  album_count;           /* 122-125 */
    char    card_id[32];      	        /* 77-88 */
    char    camera_id[32];      	        /* 90-122 */
} Kodak_dc120_status;

int   dc120_set_speed    (Camera *camera, int speed);
int   dc120_get_status   (Camera *camera, Kodak_dc120_status *status, GPContext *context);
int   dc120_get_albums	 (Camera *camera, int from_card, CameraList *list, GPContext *context);
int   dc120_get_filenames(Camera *camera, int from_card, int album_number, CameraList *list, GPContext *context);
int   dc120_file_action	 (Camera *camera, int action, int from_card, int album_number, 
			  int file_number, CameraFile *file, GPContext *context);
int dc120_capture (Camera *camera, CameraFilePath *path, GPContext *context);


#endif /* _DC120_LIBRARY_H_ */
