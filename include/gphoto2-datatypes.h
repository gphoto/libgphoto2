/* 	Header file for gPhoto 0.5-Dev

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

/* Data Structures
   ---------------------------------------------------------------- */

/* Return Values */
#define	GP_OK				 0
#define GP_ERROR			-1
#define GP_ERROR_NONCRITICAL		-2

/* Physical Connection Types */
typedef enum {
	GP_PORT_NONE		= 1 << 0,
	GP_PORT_SERIAL		= 1 << 1,
	GP_PORT_PARALLEL	= 1 << 2,
	GP_PORT_USB		= 1 << 3,
	GP_PORT_IEEE1394	= 1 << 4,
	GP_PORT_NETWORK		= 1 << 5
} CameraPortType;

/* Some macros for determining what type of ports the camera supports */
#define SERIAL_SUPPORTED(_a)	((_a >> 1)&&0x01)
#define PARALLEL_SUPPORTED(_a)	((_a >> 2)&&0x01)
#define USB_SUPPORTED(_a)	((_a >> 3)&&0x01)
#define IEEE1394_SUPPORTED(_a)	((_a >> 4)&&0x01)
#define NETWORK_SUPPORTED(_a)	((_a >> 5)&&0x01)

typedef enum {
	GP_CAPTURE_IMAGE	= 1 << 0,
	GP_CAPTURE_AUDIO 	= 1 << 1,
	GP_CAPTURE_VIDEO	= 1 << 2
} CameraCaptureType;

typedef enum {
	GP_WIDGET_WINDOW,
	GP_WIDGET_SECTION,
	GP_WIDGET_TEXT,
	GP_WIDGET_RANGE,
	GP_WIDGET_TOGGLE,
	GP_WIDGET_RADIO,
	GP_WIDGET_MENU,
	GP_WIDGET_BUTTON,
	GP_WIDGET_NONE
} CameraWidgetType;

struct CameraWidget;

struct CameraWidget {

	/* Publicly accessible
	   ------------------------------ */
	CameraWidgetType type;
	char		 label[32];

	/* Private (don't access)
	   ------------------------------ */
	/* Current value of the widget */
	float		 value;
	char		 value_string[128];

	/* For Radio and Menu */
	char 		 choice[32][64];
	int		 choice_count;

	/* For Range */
	float		 min;
	float 		 max;
	float		 increment;

	/* Child info */
	struct CameraWidget*	children[64];
	int 			children_count;

};

typedef struct CameraWidget CameraWidget;

typedef struct {
	CameraCaptureType type;
	int duration;
	int quality;
} CameraCaptureInfo;

typedef struct {
	CameraPortType type;	
	char name[128];
	char path[128];
		/* path to serial port device 			 */
		/* For serial port, "/dev/ttyS0" or variants	 */
		/* For parallel port, "/dev/lpt0" or variants	 */
		/* For usb, "usb"				 */
		/* For ieee1394, "ieee1394"			 */
		/* For network, the host's address (ip or fqdn)  */

	int speed;
		/* Speed to use	(serial)			 */
} CameraPortInfo;

typedef struct {
	char model[128];

		/* can the library support the following: */

	int port;
		/* an OR of GP_PORT_* for whatever devices the	 */
		/* the library can use.				 */

	int speed[64];
		/* if serial==1, baud rates that this camera	 */
		/* supports. terminate list with a zero 	 */


	int config;
		/* Camera can be configured remotely 		 */

	int file_delete;
		/* Camera can delete files 			 */

	int file_preview;
		/* Camera can get file previews (thumbnails) 	 */

	int file_put;
		/* Camera can receive files			 */

	int capture;
		/* Camera can do a capture (take picture) 	 */

} CameraAbilities;

typedef struct {
	char model[128]; 		   /* Name of the camera */

	CameraPortInfo port_settings;		/* Port settings */

	int debug;	          /* Debugging output 0=off 1=on */
} CameraInit;

typedef struct {
	char		type[64];
		/* mime-type of file ("image/jpg", "image/tiff", etc..,) */

	char		name[64];
		/* Suggested name for the file */

	long int	size;
		/* Size of the image data*/

	char*		data;
		/* Image data */

		/* For chunk-based transfers */
	int		bytes_read;

} CameraFile;

typedef struct {
	char	name[128];
} CameraFolderInfo;

typedef struct {
	char name[32];
	char value[128];
} CameraSetting;

typedef struct {
	char text[32*1024];
} CameraText;

/* Define some prototype function pointers to camera functions */
struct Camera;

typedef int (*c_id)		 (char *);
typedef int (*c_abilities)	 (CameraAbilities*,int*);
typedef int (*c_init)		 (struct Camera*, CameraInit*);
typedef int (*c_exit)		 (struct Camera*);
typedef int (*c_folder_list)	 (struct Camera*, char*, CameraFolderInfo*);
typedef int (*c_folder_set)	 (struct Camera*, char*);
typedef int (*c_file_count)	 (struct Camera*);
typedef int (*c_file_get)	 (struct Camera*, CameraFile*, int);
typedef int (*c_file_get_preview)(struct Camera*, CameraFile*, int);
typedef int (*c_file_put)	 (struct Camera*, CameraFile*);
typedef int (*c_capture)	 (struct Camera*, CameraFile*, CameraCaptureInfo *);
typedef int (*c_file_delete)	 (struct Camera*, int);
typedef int (*c_file_lock)	 (struct Camera*, int);
typedef int (*c_file_unlock)	 (struct Camera*, int);
typedef int (*c_config_get)	 (struct Camera*, CameraWidget*);
typedef int (*c_config_set)	 (struct Camera*, CameraSetting*, int);
typedef int (*c_summary)	 (struct Camera*, CameraText*);
typedef int (*c_manual)		 (struct Camera*, CameraText*);
typedef int (*c_about)		 (struct Camera*, CameraText*);

/* Function pointers to the current library functions */
typedef struct {
	c_id			id;
	c_abilities		abilities;
	c_init			init;
	c_exit			exit;
	c_folder_list		folder_list;
	c_folder_set		folder_set;
	c_file_count		file_count;
	c_file_get		file_get;
	c_file_get_preview	file_get_preview;
	c_file_put		file_put;
	c_file_delete		file_delete;
	c_file_lock		file_lock;
	c_file_unlock		file_unlock;
	c_config_get		config_get;
	c_config_set		config_set;
	c_capture		capture;
	c_summary		summary;
	c_manual		manual;
	c_about			about;
} CameraFunctions;

/* The Camera structure */
struct Camera {
	char		model[128];

	CameraPortInfo *port;

	int 		debug;

	void*		library_handle;

	CameraAbilities *abilities;
	CameraFunctions *functions;

	void 		*camlib_data;
	void 		*frontend_data;
};

typedef struct Camera Camera;
