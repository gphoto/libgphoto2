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
void fujitsu_dump_packet (Camera *camera, char *packet);
int fujitsu_valid_packet (Camera *camera, char *packet);
int fujitsu_write_packet (Camera *camera, char *packet);
int fujitsu_read_packet  (Camera *camera, char *packet);
int fujitsu_build_packet (Camera *camera, char type, char subtype, int data_length, char *packet);

/* Communications functions */
int fujitsu_set_speed		(Camera *camera, int speed);
int fujitsu_delete_picture	(Camera *camera, int picture_number);
int fujitsu_end_session		(Camera *camera);
int fujitsu_ack			(Camera *camera);
int fujitsu_ping		(Camera *camera);
int fujitsu_set_int_register 	(Camera *camera, int reg, int value);
int fujitsu_get_int_register 	(Camera *camera, int reg, int *value);
int fujitsu_set_string_register (Camera *camera, int reg, char *s, int length);
int fujitsu_get_string_register (Camera *camera, int reg, CameraFile *file, char *s, int *length);
