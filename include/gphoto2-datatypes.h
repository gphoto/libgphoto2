/* Constants
   ---------------------------------------------------------------- */

/* Return values. 
   Return values below -999 (starting with -1000) are defined by the 
   individual camera library. 
   
   Don't forget to add the corresponding error descriptions in 
   libgphoto2/core.c. */ 

#define GP_ERROR_BAD_PARAMETERS	     -100 /* for checking function-param. */
#define GP_ERROR_IO		     -101 /* IO problem			*/
#define GP_ERROR_CORRUPTED_DATA	     -102 /* Corrupted data		*/
#define GP_ERROR_FILE_EXISTS	     -103 /* File exists		*/
#define GP_ERROR_NO_MEMORY	     -104 /* Insufficient memory	*/
#define GP_ERROR_MODEL_NOT_FOUND     -105 /* Model not found		*/
#define GP_ERROR_NOT_SUPPORTED	     -106 /* Some op. is unsupported	*/
#define GP_ERROR_DIRECTORY_NOT_FOUND -107 /* Directory not found	*/
#define GP_ERROR_FILE_NOT_FOUND	     -108 /* File not found		*/
#define GP_ERROR_DIRECTORY_EXISTS    -109 /* Directory exists		*/

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
typedef gp_port_type CameraPortType;
        /* GP_PORT_SERIAL, GP_PORT_USB, GP_PORT_PARALLEL,
           GP_PORT_IEEE1394, GP_PORT_NETWORK */

/* Capture Type */
typedef enum {
	GP_OPERATION_NONE		= 0,
	GP_OPERATION_CAPTURE_IMAGE	= 1 << 0,
	GP_OPERATION_CAPTURE_VIDEO	= 1 << 1,
	GP_OPERATION_CAPTURE_AUDIO	= 1 << 2,
        GP_OPERATION_CAPTURE_PREVIEW	= 1 << 3,
	GP_OPERATION_CONFIG		= 1 << 4
} CameraOperation;

/* File operations */
typedef enum {
	GP_FILE_OPERATION_NONE		= 0,
	GP_FILE_OPERATION_DELETE	= 1 << 1,
	GP_FILE_OPERATION_CONFIG	= 1 << 2,
	GP_FILE_OPERATION_PREVIEW	= 1 << 3
} CameraFileOperation;

/* Folder operations */
typedef enum {
	GP_FOLDER_OPERATION_NONE	= 0,
	GP_FOLDER_OPERATION_DELETE_ALL	= 1 << 0,
	GP_FOLDER_OPERATION_PUT_FILE	= 1 << 1,
	GP_FOLDER_OPERATION_CONFIG	= 1 << 2
} CameraFolderOperation;

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

/* Camera capture choice */
typedef struct {
        int  type;
                /* One of the GP_CAPTURE_* types */
        char name[128];
                /* A string describing this resolution/setting */

        int duration;
                /* The duration of this capture event */
                /* Not needed in abilities */
} CameraCaptureSetting;

struct Camera;

/* Port information/settings */
#define CameraPortInfo gp_port_info

/* Functions supported by the cameras */

typedef struct {
        char model[128];
                /* Name of the camera model */

        int port;
                /* Supported transports for this camera.         */
                /* This is an OR of 1 or more GP_PORT_* types.   */

	int speed[64];
		/* Supported serial baud rates, terminated with  */
		/* a value of 0.      				 */

	CameraOperation operations;
		/* What operations the camera supports. This is	 */
		/* an OR of 1 or more GP_OPERATION_* types	 */
	
	CameraFileOperation file_operations;
		/* What file operations the camera supports.     */
		/* This is an OR of 1 or more                    */
		/* GP_FILE_OPERATION_* types                     */

	CameraFolderOperation folder_operations;
		/* What folder operations the camera supports.   */
		/* This is an OR of 1 or more                    */
		/* GP_FOLDER_OPERATION_* types                   */

	int usb_vendor;
	int usb_product;
		/* Vendor and product id's for USB cameras 	 */

	/* Don't touch below. for core use */
	char library[1024];
	char id[64];

} CameraAbilities;

typedef struct {
	int count;
	CameraAbilities **abilities;
} CameraAbilitiesList;

/* Valid camera file information fields */
typedef enum {
    GP_FILE_INFO_NONE            = 0,
    GP_FILE_INFO_TYPE            = 1 << 0,
    GP_FILE_INFO_NAME            = 1 << 1,
    GP_FILE_INFO_SIZE            = 1 << 2,
    GP_FILE_INFO_WIDTH           = 1 << 3,
    GP_FILE_INFO_HEIGHT          = 1 << 4,
    GP_FILE_INFO_PERMISSIONS     = 1 << 5,
    GP_FILE_INFO_ALL             = 0xFF
} CameraFileInfoFields;

/* Permissions for files on the camera */
typedef enum {
	GP_FILE_PERM_NONE	= 0,
	GP_FILE_PERM_READ       = 1 << 0,
	GP_FILE_PERM_DELETE     = 1 << 1,
	GP_FILE_PERM_ALL        = 0xFF
} CameraFilePermissions;

/* Camera file information */
typedef struct {

	/* An OR'ing of CameraFileInfoFields for valid fields */
	int    fields;

	/* The MIME type of this file */
	char   type[64];

	/* The permissions of this file. OR'ing of CameraFilePermissions. */
	/* Please don't use this field in previews. 			  */
	CameraFilePermissions permissions;

	/* The filename of this image */
	char   name[64];

	/* The size of this file */
	int    size;

	/* For Images or Movies... */
	/* The width of this image */
	int    width;
	/* The height of this image */
	int    height;

} CameraFileInfoStruct;

/* Information about a file and it's preview on the camera */
typedef struct {
	CameraFileInfoStruct 	preview;
	CameraFileInfoStruct 	file;
} CameraFileInfo;

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


/* Full path to a file on the camera */
typedef struct {
    char     name[128];
    char     folder[1024];
} CameraFilePath;

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

/* Text transferred to/from the camera */
typedef struct {
	char text[32*1024];
} CameraText;

/* CameraWidget structure */
struct CameraWidget;
typedef struct CameraWidget {
	CameraWidgetType type;
	char    label[32];

	/* Pointer to the parent */
	struct CameraWidget *parent;
	
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


struct Camera;

/* Camera function pointers */
typedef int (*c_id)		        (CameraText *);
typedef int (*c_abilities)	        (CameraAbilitiesList *);
typedef int (*c_init)		        (struct Camera*);
typedef int (*c_exit)		        (struct Camera*);
typedef int (*c_get_config)	        (struct Camera*, CameraWidget**);
typedef int (*c_set_config)	        (struct Camera*, CameraWidget*);
typedef int (*c_capture)	        (struct Camera*, int, CameraFilePath*);
typedef int (*c_capture_preview)        (struct Camera*, CameraFile*);
typedef int (*c_summary)	        (struct Camera*, CameraText*);
typedef int (*c_manual)		        (struct Camera*, CameraText*);
typedef int (*c_about)		        (struct Camera*, CameraText*);

typedef int (*c_folder_list_folders)	(struct Camera*, const char*, 
					 CameraList*);
typedef int (*c_folder_list_files)      (struct Camera*, const char*, 
					 CameraList*);
typedef int (*c_folder_get_config)      (struct Camera*, const char*, 
					 CameraWidget**);
typedef int (*c_folder_set_config)      (struct Camera*, const char*, 
					 CameraWidget*);
typedef int (*c_folder_put_file)        (struct Camera*, const char*, 
					 CameraFile*);
typedef int (*c_folder_delete_all)      (struct Camera*, const char*);

typedef int (*c_file_get_info) 		(struct Camera*, const char*, 
					 const char*, CameraFileInfo*);
typedef int (*c_file_set_info)	        (struct Camera*, const char*, 
					 const char*, CameraFileInfo*);
typedef int (*c_file_get_config)        (struct Camera*, const char*, 
					 const char*, CameraWidget**);
typedef int (*c_file_set_config)        (struct Camera*, const char*, 
					 const char*, CameraWidget*);
typedef int (*c_file_get)	        (struct Camera*, const char*, 
					 const char*, CameraFile*);
typedef int (*c_file_get_preview)       (struct Camera*, const char*, 
					 const char*, CameraFile*);
typedef int (*c_file_delete)		(struct Camera*, const char*, 
					 const char*);

typedef char *(*c_result_as_string)     (struct Camera*, int);

/* Function pointers to the current library functions */
typedef struct {
	c_id			id;
	c_abilities		abilities;
	c_init			init;
	c_exit			exit;
        c_get_config		get_config;
        c_set_config		set_config;
        c_capture		capture;
        c_capture_preview       capture_preview;
        c_summary		summary;
	c_manual		manual;
	c_about			about;

        c_folder_list_folders	folder_list_folders;
        c_folder_list_files	folder_list_files;
        c_folder_get_config	folder_get_config;
	c_folder_set_config	folder_set_config;
        c_folder_put_file	folder_put_file;
        c_folder_delete_all     folder_delete_all;

        c_file_get_info         file_get_info;
	c_file_set_info		file_set_info;
        c_file_get_config	file_get_config;
	c_file_set_config	file_set_config;
        c_file_get_preview	file_get_preview;
        c_file_get		file_get;
        c_file_delete    	file_delete;

	c_result_as_string	result_as_string;
} CameraFunctions;

typedef struct Camera {
	char		model[128];
		
	CameraPortInfo	*port;

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
