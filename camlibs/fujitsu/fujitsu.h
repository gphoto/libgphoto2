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

#define	RETRIES			10

/* Packet functions */
void fujitsu_dump_packet (char *packet);
int fujitsu_valid_packet (char *packet);
int fujitsu_write_packet (gpio_device *dev, char *packet);
int fujitsu_read_packet (gpio_device *dev, char *packet);
int fujitsu_build_packet (char type, char subtype, int data_length, char *packet);

/* Communications functions */
int fujitsu_set_speed(gpio_device *dev, int speed);
int fujitsu_ack(gpio_device *dev);
int fujitsu_ping(gpio_device *dev);
int fujitsu_set_int_register (gpio_device *dev, int reg, int value);
int fujitsu_get_int_register (gpio_device *dev, int reg);
