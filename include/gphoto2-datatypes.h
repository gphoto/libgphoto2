/* 	Header file for gPhoto 0.5-Dev

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

/* Constants 
   ---------------------------------------------------------------- */

/* Return values. 
   Return values below -99 (starting with -100) are defined by the 
   individual camera library. 
   
   Don't forget to add the corresponding error descriptions in 
   libgphoto2/core.c. */ 
#define GP_OK			  0
#define GP_ERROR		 -1	/* generic			*/
#define GP_ERROR_NONCRITICAL	 -2	/* deprecated	 		*/
#define GP_ERROR_BAD_PARAMETERS	 -3	/* for checking function-param.	*/
#define GP_ERROR_IO		 -4	/* IO problem			*/
#define GP_ERROR_CORRUPTED_DATA	 -5	/* Corrupted data		*/
#define GP_ERROR_FILE_EXISTS	 -6	/* File exists			*/
#define GP_ERROR_NO_MEMORY	 -7	/* Insufficient memory		*/
#define GP_ERROR_MODEL_NOT_FOUND -8	/* Model not found		*/
#define GP_ERROR_NOT_SUPPORTED	 -9	/* Some op. is unsupported	*/
#define GP_ERROR_DIRECTORY_NOT_FOUND -10/* Directory not found		*/
#define GP_ERROR_FILE_NOT_FOUND	-11	/* File not found		*/

#define GP_NUM_ERRORS	12

/* Debugging levels for gp_init */
#define GP_DEBUG_NONE			0
#define GP_DEBUG_LOW			1
#define	GP_DEBUG_MEDIUM			2
#define GP_DEBUG_HIGH			3


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
	GP_CAPTURE_NONE		= 0,
	GP_CAPTURE_IMAGE	= 1 << 0,
	GP_CAPTURE_VIDEO	= 1 << 1,
	GP_CAPTURE_PREVIEW	= 1 << 2
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
	GP_WIDGET_DATE,
	GP_WIDGET_NONE
} CameraWidgetType;

/* Window prompt return values */
typedef enum {
	GP_PROMPT_CANCEL	= -1,
	GP_PROMPT_OK		= 0
} CameraPromptValue;

/* Confirm return values */
typedef enum {
	GP_CONFIRM_YES,
	GP_CONFIRM_YESTOALL,
	GP_CONFIRM_NO,
	GP_CONFIRM_NOTOALL
} CameraConfirmValue;

/* Folder list entries */
typedef enum {
	GP_LIST_FOLDER,
	GP_LIST_FILE
} CameraListType;



/* Structures
   ---------------------------------------------------------------- */

#define WIDGET_CHOICE_MAX	32

struct Camera;
struct CameraWidget;

/* CameraWidget structure */
typedef struct CameraWidget {
	CameraWidgetType type;
	char    label[32];
	
	/* Current value of the widget */
        char   *value_string;
        int     value_int;
        float   value_float;

	/* For Radio and Menu */
	char 	choice[WIDGET_CHOICE_MAX][64];
	int	choice_count;

	/* For Range */
	float	min;
	float 	max;
	float	increment;

	/* Child info */
	struct CameraWidget	*children[64];
	int 	         	 children_count;

	/* Widget was changed */
	int 	changed;

        /* Reference count */
        int     ref_count;

	/* Unique identifier */
	int	id;

	/* Callback */
	int (*callback)(struct Camera*, struct CameraWidget*);

} CameraWidget;

/* Capture information structure */
typedef struct {
	CameraCaptureType type;
	int duration;
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

	CameraCaptureType capture;
		/* Camera can do the specified kinds of capture	 */

	int usb_vendor;
	int usb_product;
		/* Vendor and product id's for USB cameras 	 */
		/* Terminated with a 0 for both			 */

	/* Don't touch below. for core use */
	char library[1024];
	char id[64];

} CameraAbilities;

typedef struct {
	int count;
	CameraAbilities **abilities;
} CameraAbilitiesList;

/* Camera file structure used for transferring files*/
typedef struct {
	char		type[64];
		/* mime-type of file ("image/jpg", "image/tiff", etc..,) */

	char		name[64];
		/* filename */
	
	long int	size;
	char		*data;
	int		bytes_read;

        int 		session;

	int		ref_count;
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

struct Camera;

/* Camera function pointers */
typedef int (*c_id)		 (CameraText *);
typedef int (*c_abilities)	 (CameraAbilitiesList *);
typedef int (*c_init)		 (struct Camera*);
typedef int (*c_exit)		 (struct Camera*);
typedef int (*c_folder_list)	 (struct Camera*, CameraList*, char*);
typedef int (*c_file_list)	 (struct Camera*, CameraList*, char*);
typedef int (*c_file_get)	 (struct Camera*, CameraFile*, char*, char*);
typedef int (*c_file_get_preview)(struct Camera*, CameraFile*, char*, char*);
typedef int (*c_file_config_get) (struct Camera*, CameraWidget**, char*, char*);
typedef int (*c_file_config_set) (struct Camera*, CameraWidget*, char*, char*);
typedef int (*c_folder_config_get)(struct Camera*, CameraWidget**, char*);
typedef int (*c_folder_config_set)(struct Camera*, CameraWidget*, char*);
typedef int (*c_config_get)	 (struct Camera*, CameraWidget**);
typedef int (*c_config_set)	 (struct Camera*, CameraWidget*);
typedef int (*c_file_put)	 (struct Camera*, CameraFile*, char*);
typedef int (*c_file_delete)	 (struct Camera*, char*, char*);
typedef int (*c_capture)	 (struct Camera*, CameraFile*, CameraCaptureInfo*);
typedef int (*c_config)		 (struct Camera*);
typedef int (*c_summary)	 (struct Camera*, CameraText*);
typedef int (*c_manual)		 (struct Camera*, CameraText*);
typedef int (*c_about)		 (struct Camera*, CameraText*);
typedef char *(*c_result_as_string) (struct Camera*, int);

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
	c_file_config_get	file_config_get;
	c_file_config_set	file_config_set;
	c_folder_config_get	folder_config_get;
	c_folder_config_set	folder_config_set;
	c_config_get		config_get;
	c_config_set		config_set;
	c_file_put		file_put;
	c_file_delete		file_delete;
	c_config		config;
	c_capture		capture;
	c_summary		summary;
	c_manual		manual;
	c_about			about;
	c_result_as_string	result_as_string;
} CameraFunctions;

typedef struct Camera {
	char		model[128];
		
	CameraPortInfo	*port;

	int 		debug;

        int             ref_count;

	void		*library_handle;

	CameraAbilities *abilities;
	CameraFunctions *functions;

	void 		*camlib_data;
	void 		*frontend_data;

	int 		session;
} Camera;


/* Function Pointers
   ---------------------------------------------------------------- */

typedef int (*CameraStatus)		(Camera*, char*);
typedef int (*CameraMessage)		(Camera*, char*);
typedef int (*CameraConfirm)		(Camera*, char*);
typedef int (*CameraProgress)		(Camera*, CameraFile*, float);
typedef int (*CameraPrompt)		(Camera*, CameraWidget*);
typedef int (*CameraWidgetCallback) 	(Camera*, CameraWidget*);
