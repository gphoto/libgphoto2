/* 	Header file for gPhoto 0.5-Dev

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

/* Constants 
   ---------------------------------------------------------------- */

/* Return Values */
#define	GP_OK				 0
#define GP_ERROR			-1
#define GP_ERROR_NONCRITICAL		-2

/* Macros
   ---------------------------------------------------------------- */

/* Determining what kind of port a camera supports. 
   For use on CameraAbilities -> port */
#define SERIAL_SUPPORTED(p)	(p & (1 << 0))
#define PARALLEL_SUPPORTED(p)	(p & (1 << 1))
#define USB_SUPPORTED(p)	(p & (1 << 2))
#define IEEE1394_SUPPORTED(p)	(p & (1 << 3))
#define NETWORK_SUPPORTED(p)	(p & (1 << 4))

/* Enumerations
   ---------------------------------------------------------------- */

/* Physical Connection Types */
typedef enum {
	GP_PORT_NONE		= 0,
	GP_PORT_SERIAL		= 1 << 0,
	GP_PORT_PARALLEL	= 1 << 1,
	GP_PORT_USB		= 1 << 2,
	GP_PORT_IEEE1394	= 1 << 3,
	GP_PORT_NETWORK		= 1 << 4
} CameraPortType;

/* Capture Type */
typedef enum {
	GP_CAPTURE_IMAGE	= 1 << 0,
	GP_CAPTURE_AUDIO 	= 1 << 1,
	GP_CAPTURE_VIDEO	= 1 << 2
} CameraCaptureType;

/* Widget types */
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

/* Folder list entries */
typedef enum {
	GP_LIST_FOLDER,
	GP_LIST_FILE
} CameraListType;

/* Structures
   ---------------------------------------------------------------- */

/* CameraWidget structure */
typedef struct CameraWidget {
	CameraWidgetType type;
	char		 label[32];
	
	/* Current value of the widget */
	char		 value[128];

	/* For Radio and Menu */
	char 		 choice[32][64];
	int		 choice_count;

	/* For Range */
	float		 min;
	float 		 max;
	float		 increment;

	/* Child info */
	struct CameraWidget	*children[64];
	int 			children_count;
} CameraWidget;

/* Capture information structure */
typedef struct {
	CameraCaptureType type;
	int duration;
	int quality;
} CameraCaptureInfo;

/* Port information/settings */
typedef struct {
	CameraPortType type;	
	char name[128];
	char path[128];
		/* path to serial port device 			 */
		/* For serial port, "/dev/ttyS0" or variants	 */
		/* For parallel port, "/dev/lpt0" or variants	 */
		/* For usb, "usb"				 */
		/* For ieee1394, "ieee1394"			 */
		/* For network, "network"  			 */

	/* Serial-specific members */
	int speed;

	/* Network-specific members */
	char host[128];
	int  host_port;
} CameraPortInfo;

/* Functions supported by the cameras */

typedef struct {
	char model[128];

	int port;
		/* an OR of GP_PORT_* for whatever devices the	 */
		/* the library can use.				 */

	int speed[64];
		/* Supported serial baud rates, terminated with  */
		/* a 0.						 */

	int config;
		/* Camera can be configured	 		 */

	int file_delete;
		/* Camera can delete files 			 */

	int file_preview;
		/* Camera can get file previews (thumbnails) 	 */

	int file_put;
		/* Camera can receive files			 */

	int capture;
		/* Camera can do a capture			 */

	/* Don't touch below. for core use */
	char library[1024];
	char id[64];

} CameraAbilities;

typedef struct {
	int count;
	CameraAbilities **abilities;
} CameraAbilitiesList;


/* Initialization data to the camera */
typedef struct {
	char model[128]; 		   /* Name of the camera */

	CameraPortInfo port_settings;		/* Port settings */

	int debug;	          /* Debugging output 0=off 1=on */
} CameraInit;

/* Camera file structure used for transferring files*/
typedef struct {
	char		type[64];
		/* mime-type of file ("image/jpg", "image/tiff", etc..,) */

	char		name[64];
		/* filename */
	
	long int	size;
	char		*data;
	int		bytes_read;
} CameraFile;

/* Entry in a folder on the camera */
typedef struct {
	CameraListType type;
	char name[128];
	char value[128];
} CameraListEntry;

typedef struct {
	char folder[1024];
	int  count;
	CameraListEntry entry[1024];
} CameraList;

/* Camera filesystem emulation structs */
typedef struct {
	char name[128];
} CameraFilesystemFile;

typedef struct {
	int count;
	char name[128];
	CameraFilesystemFile **file;
} CameraFilesystemFolder;

typedef struct {
	int count;
	CameraFilesystemFolder **folder ;
} CameraFilesystem;

/* Settings for the config_set function */
typedef struct {
	char name[32];
	char value[128];
} CameraSetting;

/* Text transferred to/from the camera */
typedef struct {
	char text[32*1024];
} CameraText;

/* Camera object data */
struct Camera;
typedef int (*c_id)		 (CameraText *);
typedef int (*c_abilities)	 (CameraAbilitiesList *);
typedef int (*c_init)		 (struct Camera*, CameraInit*);
typedef int (*c_exit)		 (struct Camera*);
typedef int (*c_folder_list)	 (struct Camera*, CameraList*, char*);
typedef int (*c_file_list)	 (struct Camera*, CameraList*, char*);
typedef int (*c_file_get)	 (struct Camera*, CameraFile*, char*, char*);
typedef int (*c_file_get_preview)(struct Camera*, CameraFile*, char*, char*);
typedef int (*c_file_put)	 (struct Camera*, CameraFile*, char*);
typedef int (*c_file_delete)	 (struct Camera*, char*, char*);
typedef int (*c_capture)	 (struct Camera*, CameraFile*, CameraCaptureInfo *);
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
	c_file_list		file_list;
	c_file_get		file_get;
	c_file_get_preview	file_get_preview;
	c_file_put		file_put;
	c_file_delete		file_delete;
	c_config_get		config_get;
	c_config_set		config_set;
	c_capture		capture;
	c_summary		summary;
	c_manual		manual;
	c_about			about;
} CameraFunctions;

typedef struct Camera {
	char		model[128];
		
	CameraPortInfo	*port;

	int 		debug;

	void		*library_handle;

	CameraAbilities *abilities;
	CameraFunctions *functions;

	void 		*camlib_data;
	void 		*frontend_data;
} Camera;
