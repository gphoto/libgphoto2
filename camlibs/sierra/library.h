#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

/* Short comm. */
#define NUL		0x00
#define ENQ		0x05
#define ACK		0x06
#define DC1		0x11
#define NAK		0x15
#define TRM		0xff

/* Packet types */
#define TYPE_COMMAND	0x1b
#define TYPE_DATA	0x02
#define TYPE_DATA_END	0x03

/* Sub-types */
#define SUBTYPE_COMMAND_FIRST	0x53
#define SUBTYPE_COMMAND		0x43

/* Maximum length of the data field within a packet */
#define MAX_DATA_FIELD_LENGTH   2048

#define	RETRIES			10

/* Camera specific error identifiers.
   The corresponding error descriptions are in sierra.c */
#define GP_ERROR_BAD_CONDITION  -1000 /* Bad condition for executing commands */

/* Packet functions */
int sierra_valid_packet (Camera *camera, char *packet);
int sierra_write_packet (Camera *camera, char *packet);
int sierra_read_packet  (Camera *camera, char *packet);
int sierra_build_packet (Camera *camera, char type, char subtype, int data_length, char *packet);

/* Communications functions */
int sierra_set_speed		  (Camera *camera, int speed);
int sierra_end_session		  (Camera *camera);
int sierra_write_ack		  (Camera *camera);
int sierra_write_nak		  (Camera *camera);
int sierra_ping			  (Camera *camera);
int sierra_get_memory_left        (Camera *camera, int *memory);
int sierra_check_battery_capacity (Camera *camera);
int sierra_set_int_register 	  (Camera *camera, int reg, int value);
int sierra_get_int_register 	  (Camera *camera, int reg, int *value);
int sierra_set_string_register	  (Camera *camera, int reg, const char *s, long int length);
int sierra_get_string_register	  (Camera *camera, int reg, int file_number,
				   CameraFile *file, char *s, int *length);
int sierra_delete		  (Camera *camera, int picture_number);
int sierra_delete_all             (Camera *camera);
int sierra_capture		  (Camera *camera, CameraCaptureType type, 
				   CameraFilePath *filepath);
int sierra_capture_preview 	  (Camera *camera, CameraFile *file);
int sierra_change_folder          (Camera *camera, const char *folder);

/* Filesystem functions */
int sierra_list_files         (Camera *camera, const char *folder, CameraList *list);
int sierra_list_folders       (Camera *camera, const char *folder, CameraList *list);
int sierra_get_picture_folder (Camera *camera, char **folder);
int sierra_upload_file        (Camera *camera, CameraFile *file);
