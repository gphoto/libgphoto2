/* Short comm. */
#define NUL		0x00
#define ENQ		0x05
#define ACK		0x06
#define DC1		0x11
#define NAK		0x15
#define TRM		0xff

/* Packet types */
#define TYPE_COMMAND	0x1b
#define TYPE_SEQUENCE	0x02
#define TYPE_END	0x03

/* Sub-types */
#define SUBTYPE_COMMAND_FIRST	0x53
#define SUBTYPE_COMMAND		0x43

char *fujitsu_build_packet (char type, char subtype, int length);

int fujitsu_process_packet (char *packet);
