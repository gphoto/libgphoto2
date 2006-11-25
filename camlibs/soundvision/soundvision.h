#ifndef soundvision_H
#define soundvision_H

#include <_stdint.h>

#define SOUNDVISION_START_TRANSACTION 0x0001

#define SOUNDVISION_SETPC2            0x0004          

#define SOUNDVISION_GET_MEM_TOTAL     0x0065

#define SOUNDVISION_GET_MEM_FREE      0x0069

#define SOUNDVISION_TAKEPIC3          0x0092

#define SOUNDVISION_SETPC1            0x0094

#define SOUNDVISION_DELETE            0x0100
#define SOUNDVISION_GET_PIC           0x0101
#define SOUNDVISION_GET_PIC_SIZE      0x0102
#define SOUNDVISION_GET_NUM_PICS      0x0103

#define SOUNDVISION_DELETE_ALL2       0x0105
#define SOUNDVISION_GET_VERSION       0x0106

#define SOUNDVISION_GET_NAMES         0x0108
#define SOUNDVISION_PUT_FILE          0x0109
#define SOUNDVISION_GET_THUMB_SIZE    0x010A
#define SOUNDVISION_GET_THUMB         0x010B

#define SOUNDVISION_STATUS	      0x0114
#define SOUNDVISION_INIT2             0x0115

#define SOUNDVISION_DONE_TRANSACTION  0x01FF

#define SOUNDVISION_AGFACL18          0
#define SOUNDVISION_TIGERFASTFLICKS   1
#define SOUNDVISION_IXLA              2

struct _CameraPrivateLibrary {
	GPPort *gpdev;

        int device_type;
        int reset_times,odd_command;
	int num_pictures;
	char *file_list;

};

/* commands.c */
int32_t soundvision_send_command(uint32_t command, uint32_t argument,
				                                          CameraPrivateLibrary *dev);
int soundvision_reset(CameraPrivateLibrary *dev, char *revision, char *status);

int soundvision_get_revision(CameraPrivateLibrary *dev,char *revision);


int soundvision_get_status(CameraPrivateLibrary *dev, char *status);

int soundvision_photos_taken(CameraPrivateLibrary *dev);

int soundvision_get_file_list(CameraPrivateLibrary *dev);

int soundvision_delete_picture(CameraPrivateLibrary *dev, const char *filename);

int soundvision_get_thumb_size(CameraPrivateLibrary *dev, const char *filename);
int soundvision_get_thumb(CameraPrivateLibrary *dev, const char *filename, 
		   unsigned char *data,int size);
int soundvision_get_pic_size(CameraPrivateLibrary *dev, const char *filename);
int soundvision_get_pic(CameraPrivateLibrary *dev, const char *filename, 
		   unsigned char *data,int size);

/* agfa_cl18.c */

int agfa_capture(CameraPrivateLibrary *dev, CameraFilePath *path);
int agfa_delete_picture(CameraPrivateLibrary *dev, const char *filename);
int agfa_get_file_list(CameraPrivateLibrary *dev);
int agfa_get_pic(CameraPrivateLibrary *dev, const char *filename,
		                    unsigned char *data,int size);
int agfa_get_pic_size(CameraPrivateLibrary *dev, const char *filename);
int agfa_get_thumb(CameraPrivateLibrary *dev, const char *filename,
		                      unsigned char *data,int size);
int agfa_get_thumb_size(CameraPrivateLibrary *dev, const char *filename); 
   
   
   
   
   

/* tiger_fastflicks.c */
int tiger_set_pc_mode(CameraPrivateLibrary *dev);
int tiger_upload_file(CameraPrivateLibrary *dev, const char *filename,
		      const char *data, long data_size);
int tiger_delete_picture(CameraPrivateLibrary *dev, const char *filename);   
int tiger_capture(CameraPrivateLibrary *dev, CameraFilePath *path);   
int tiger_get_mem(CameraPrivateLibrary *dev, int *num_pics, int *mem_total, int *mem_free);   
int tiger_get_file_list(CameraPrivateLibrary *dev);
int tiger_get_pic(CameraPrivateLibrary *dev, const char *filename,
		                    unsigned char *data,int size);
int tiger_get_pic_size(CameraPrivateLibrary *dev, const char *filename);
int tiger_get_thumb(CameraPrivateLibrary *dev, const char *filename,
		                      unsigned char *data,int size);   
int tiger_get_thumb_size(CameraPrivateLibrary *dev, const char *filename); 
#endif

