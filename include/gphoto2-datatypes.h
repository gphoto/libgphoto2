/* 	Header file for gPhoto 0.5-Dev

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

/* Data Structures
   ---------------------------------------------------------------- */

/* Return Values */
#define	GP_OK				 0
#define GP_ERROR			-1
#define GP_ERROR_CRITICAL		-2

/* File Types */
typedef enum {
	GP_FILE_UNKNOWN,
	GP_FILE_JPEG,
	GP_FILE_TIFF,
	GP_FILE_FLASHPIX,
	GP_FILE_PPM,
	GP_FILE_WAV,
	GP_FILE_MPEG,
	GP_FILE_QUICKTIME
} CameraFileType;

/* Physical Connection Types */
typedef enum {
	GP_PORT_DIRECTORY,
	GP_PORT_SERIAL,
	GP_PORT_USB,
	GP_PORT_PARALLEL,
	GP_PORT_IRDA
} CameraConnectType;

typedef struct {
	CameraConnectType type;
		/* What kind of connection is it? 		 */

	char serial_port[128];
		/* path to serial port device 			 */

	int serial_baud;
		/* Baud rate for serial port camera		 */

	int usb_node;
		/* dummy for now. need way to specify which USB device */

	char directory_path[128];
		/* path for directory lib to index		 */
} CameraPortSettings;

typedef struct {
	char model[128];

		/* can the library support the following: */
	int serial;
	int usb;
	int parallel;
	int ieee1394;

	int serial_baud[64];
		/* if serial==1, baud rates that this camera	 */
		/* supports. terminate list with a zero 	 */

	int cancel;
		/* Camera operation can be cancelled in progress */

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

	int lock;
		/* Camera can lock (protect) pictures		 */

} CameraAbilities;

typedef struct {
	char model[128]; 		   /* Name of the camera */

	CameraPortSettings port_settings; 	/* Port settings */
} CameraInit;

typedef struct {
	CameraFileType	type;
		/* Type of file (GP_FILE_JPEG, GP_FILE_TIFF, ..) */

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
	int count;
	CameraFolderInfo info[512];
} CameraFolderList;

typedef struct {
	char name[32];
	char value[32];
} CameraSetting;

typedef struct {
	int count;
	CameraSetting setting[64];
} CameraConfig;
