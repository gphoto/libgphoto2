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
	GP_PORT_NONE,
	GP_PORT_SERIAL,
	GP_PORT_PARALLEL,
	GP_PORT_USB,
	GP_PORT_IEEE1394,
	GP_PORT_NETWORK
} CameraPortType;

typedef enum {
	WIDGET_WINDOW,
	WIDGET_PAGE,
	WIDGET_TEXT,
	WIDGET_RANGE,
	WIDGET_TOGGLE,
	WIDGET_RADIO,
	WIDGET_MENU,
	WIDGET_BUTTON,
	WIDGET_NONE
} CameraWidgetType;

struct CameraWidget;

struct CameraWidget {

	/* Publicly accessible
	   ------------------------------ */
	CameraWidgetType type;
	char		 label[32];

	/* Current value of the widget */
	char		 value_string[32];
	float		 value_number;


	/* For Radio and Menu */
	char 		 choice[32];

	/* For Range */
	float		 min;
	float 		 max;
	float		 step;

	/* Function Callback */
	int 		(*callback)(struct CameraWidget *widget);
	/* Private (don't access)
	   ------------------------------ */
	struct CameraWidget**	children;
	int 			children_count;

};

typedef struct CameraWidget CameraWidget;

typedef struct {
	CameraPortType type;
        char name[64];
        char path[64];
} CameraPortInfo;

typedef struct {
	char path[128];
		/* path to serial port device 			 */
		/* For serial port, "/dev/ttyS0" or variants	 */
		/* For parallel port, "/dev/lpt0" or variants	 */
		/* For usb, "usb"				 */
		/* For ieee1394, "ieee1394"			 */
		/* For network, the address (ip or fqdn).	 */

	int speed;
		/* Speed to use	(serial)			 */
} CameraPortSettings;

typedef struct {
	char model[128];

		/* can the library support the following: */
	int serial;
	int parallel;
	int usb;
	int ieee1394;
	int network;
		/* set to 1 if supported, 0 if not.		 */

	int speed[64];
		/* if serial==1, baud rates that this camera	 */
		/* supports. terminate list with a zero 	 */

	int capture;
		/* Camera can do a capture (take picture) 	 */

	int config;
		/* Camera can be configures remotely 		 */

	int file_delete;
		/* Camera can delete files 			 */

	int file_preview;
		/* Camera can get file previews (thumbnails) 	 */

	int file_put;
		/* Camera can receive files			 */
} CameraAbilities;

typedef struct {
	char model[128]; 		   /* Name of the camera */

	CameraPortSettings port_settings; 	/* Port settings */

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

} CameraFile;

typedef struct {
	char	name[128];
} CameraFolderInfo;

typedef struct {
	char name[32];
	char value[32];
} CameraSetting;
