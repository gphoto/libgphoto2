/*
 * psa50.h - Canon PowerShot A50 "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
 */

#ifndef PSA50_H
#define PSA50_H

/* Defines for error handling */
#define NOERROR		0
#define ERROR_RECEIVED	1
#define ERROR_ADDRESSED	2
#define FATAL_ERROR	3
#define ERROR_LOWBATT	4

/* Battery status values:

 * hopefully obsolete values first - we now just use the bit 
 * that makes the difference

  obsolete #define CAMERA_ON_AC       16
  obsolete #define CAMERA_ON_BATTERY  48
*/

#define CAMERA_POWER_OK     6
#define CAMERA_POWER_BAD    4

#define CAMERA_MASK_BATTERY  32


struct canon_dir {
  const char *name; /* NULL if at end */
  unsigned int size;
  time_t date;
  unsigned char attrs; /* File attributes, see "Protocols" for details */
  int is_file;
  void *user;	/* user-specific data */
};


/**
 * Various Powershot camera types
 */
typedef enum {
	CANON_PS_A5,
	  CANON_PS_A5_ZOOM,
	  CANON_PS_A50,
	  CANON_PS_S10,
	  CANON_PS_S20,
	  CANON_PS_A70,
  	  CANON_PS_S100,
          CANON_PS_S300,
	  CANON_PS_G1,
	  CANON_PS_G2,
          CANON_PS_A10,
          CANON_PS_A20,
          CANON_EOS_D30,
          CANON_PS_PRO90_IS
} canonCamModel;


struct _CameraPrivateLibrary
{
	canonCamModel model;
	int speed;        /* The speed we're using for this camera */
	char ident[32];   /* Model ID string given by the camera */
	char owner[32];   /* Owner name */
	char firmwrev[4]; /* Firmware revision */
	int debug;
	int dump_packets;
	int A5;
	char psa50_id[200];	/* some models may have a lot to report */
	int canon_comm_method;
	unsigned char psa50_eot[8];

	int receive_error;
	int first_init;  /* first use of camera   1 = yes 0 = no */
	int uploading;   /* 1 = yes ; 0 = no */
	int slow_send;   /* to send data via serial with a usleep(1) 
					  * between each byte 1 = yes ; 0 = no */ 

/*
 * Directory access may be rather expensive, so we cache some information.
 * The first variable in each block indicates whether the block is valid.
 */

	int cached_ready;
	int cached_disk;
	char *cached_drive; /* usually something like C: */
	int cached_capacity;
	int cached_available;
	int cached_dir;
	struct canon_dir *cached_tree;
	int cached_images;
	char **cached_paths; /* only used by A5 */
};


/*
 * Our driver now supports both USB and serial communications
 */
#define CANON_SERIAL_RS232 0
#define CANON_USB 1

#define DIR_CREATE 0
#define DIR_REMOVE 1


typedef unsigned long u32;
#ifndef byteswap32
#ifdef __sparc
#define byteswap32(val) ({ u32 x = val; x = (x << 24) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) | (x >> 24); x; })
#else
#define byteswap32(val) val
#endif
#endif

#define CANON_USB_FUNCTION_GET_FILE		1
#define CANON_USB_FUNCTION_IDENTIFY_CAMERA	2
#define CANON_USB_FUNCTION_GET_TIME		3
#define CANON_USB_FUNCTION_SET_TIME		4
#define CANON_USB_FUNCTION_MKDIR		5
#define CANON_USB_FUNCTION_CAMERA_CHOWN		6
#define CANON_USB_FUNCTION_RMDIR		7
#define CANON_USB_FUNCTION_DISK_INFO		8
#define CANON_USB_FUNCTION_FLASH_DEVICE_IDENT	9
#define CANON_USB_FUNCTION_POWER_STATUS		10
#define CANON_USB_FUNCTION_GET_DIRENT		11
#define CANON_USB_FUNCTION_DELETE_FILE		12
#define CANON_USB_FUNCTION_SET_ATTR		13

struct canon_usb_cmdstruct 
{
	int num;
	char *description;
	char cmd1, cmd2;
	int cmd3;
	int return_length;
};

static const struct canon_usb_cmdstruct canon_usb_cmd[] = {
	{CANON_USB_FUNCTION_GET_FILE,		"Get file",		0x01, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_IDENTIFY_CAMERA,	"Identify camera",	0x01, 0x12, 0x201,	0x9c},
	{CANON_USB_FUNCTION_GET_TIME,		"Get time",		0x03, 0x12, 0x201,	0x60},
	{CANON_USB_FUNCTION_SET_TIME,		"Set time",		0x04, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_MKDIR,		"Make directory",	0x05, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_CAMERA_CHOWN,	"Change camera owner",	0x05, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_RMDIR,		"Remove directory",	0x06, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_DISK_INFO,		"Disk info request",	0x09, 0x11, 0x201,	0x5c},
	{CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,	"Flash device ident",	0x0a, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_POWER_STATUS,	"Power supply status",	0x0a, 0x12, 0x201,	0x58},
	{CANON_USB_FUNCTION_GET_DIRENT,		"Get directory entrys",	0x0b, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_DELETE_FILE,	"Delete file",		0x0d, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_SET_ATTR,		"Set file attribute",	0x0e, 0x11, 0x201,	0x54},
	{ 0 }
};


/*
 * All functions returning a pointer have malloc'ed the data. The caller must
 * free() it when done.
 */

/**
 * Switches the camera on, detects the model and sets its speed
 */
int psa50_ready(Camera *camera);

/**
 *
 */
char *psa50_get_disk(Camera *camera);

/**
 *
 */
int psa50_get_battery(Camera *camera, int *pwr_status, int *pwr_source);

/**
 *
 */
int psa50_disk_info(Camera *camera, const char *name,int *capacity,int *available);

/**
 *
 */
struct canon_dir *psa50_list_directory(Camera *camera, const char *name);
void psa50_free_dir(Camera *camera, struct canon_dir *list);
int psa50_get_file(Camera *camera, const char *name, unsigned char **data, int *length);
unsigned char *psa50_get_thumbnail(Camera *camera, const char *name,int *length);
int psa50_put_file(Camera *camera, CameraFile *file, char *destname, char *destpath);
int psa50_set_file_attributes(Camera *camera, const char *file, const char *dir, unsigned char attrs);
int psa50_delete_file(Camera *camera, const char *name, const char *dir);
int psa50_end(Camera *camera);
int psa50_off(Camera *camera);
time_t psa50_get_time(Camera *camera);
int psa50_set_time(Camera *camera);
int psa50_directory_operations(Camera *camera, const char *path, int action);
int psa50_get_owner_name(Camera *camera);
int psa50_set_owner_name(Camera *camera, const char *name);
void psa50_error_type(Camera *camera);

#endif
