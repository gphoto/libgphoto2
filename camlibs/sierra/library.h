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

#define	RETRIES			10

/* Packet functions */
void sierra_dump_packet (Camera *camera, char *packet);
int sierra_valid_packet (Camera *camera, char *packet);
int sierra_write_packet (Camera *camera, char *packet);
int sierra_read_packet  (Camera *camera, char *packet);
int sierra_build_packet (Camera *camera, char type, char subtype, int data_length, char *packet);

/* Communications functions */
int sierra_set_speed		(Camera *camera, int speed);
int sierra_delete_picture	(Camera *camera, int picture_number);
int sierra_end_session		(Camera *camera);
int sierra_write_ack		(Camera *camera);
int sierra_write_nak		(Camera *camera);
int sierra_ping			(Camera *camera);
int sierra_set_int_register 	(Camera *camera, int reg, int value);
int sierra_get_int_register 	(Camera *camera, int reg, int *value);
int sierra_set_string_register	(Camera *camera, int reg, char *s, int length);
int sierra_get_string_register	(Camera *camera, int reg, int file_number,
				 CameraFile *file, char *s, int *length);
int sierra_folder_set		(Camera *camera, char *folder);
int sierra_file_count		(Camera *camera);
int sierra_delete		(Camera *camera, int picture_number);
