typedef struct {
	int debug;
	int folders;
	int speed;
	int first_packet;
	gpio_device* dev;
	char folder[128];
} FujitsuData;

void debug_print(FujitsuData *fd, char *message);
