/* This pseudo-C file is uploaded to Coverity for modeling. */
/* from ptp2 */

typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef void *PTPParams;

uint16_t htod16p (PTPParams *params, uint16_t var) { __coverity_tainted_data_return__(); }
uint16_t dtoh16p (PTPParams *params, uint16_t var) { __coverity_tainted_data_return__(); }
uint16_t htole16(uint16_t var) { __coverity_tainted_data_return__(); }
uint16_t htobe16(uint16_t var) { __coverity_tainted_data_return__(); }
void htod16ap (PTPParams *params, unsigned char *a, uint16_t val) { __coverity_tainted_data_argument__(a); }
void dtoh16ap (PTPParams *params, unsigned char *a, uint16_t val) { __coverity_tainted_data_argument__(a); }

uint32_t htod32p (PTPParams *params, uint32_t var) { __coverity_tainted_data_return__(); }
uint32_t dtoh32p (PTPParams *params, uint32_t var) { __coverity_tainted_data_return__(); }
uint32_t htole32 (uint32_t var) { __coverity_tainted_data_return__(); }
uint32_t htobe32 (uint32_t var) { __coverity_tainted_data_return__(); }
void htod32ap (PTPParams *params, unsigned char *a, uint32_t val) { __coverity_tainted_data_argument__(a); }
void dtoh32ap (PTPParams *params, unsigned char *a, uint32_t val) { __coverity_tainted_data_argument__(a); }

uint64_t dtoh64p (PTPParams *params, uint64_t var) { __coverity_tainted_data_return__(); }
uint64_t htod64p (PTPParams *params, uint64_t var) { __coverity_tainted_data_return__(); }

void htod64ap (PTPParams *params, unsigned char *a, uint64_t val) { __coverity_tainted_data_argument__(a); }
void dtoh64ap (PTPParams *params, unsigned char *a, uint64_t val) { __coverity_tainted_data_argument__(a); }

/* gphoto port reading from devices */
typedef void *GPPort;
int gp_port_read        (GPPort *port, char *data, int size) {
	__coverity_tainted_data_argument__(data);
}

int gp_port_usb_msg_read    (GPPort *port, int request, int value, int index, char *bytes, int size) {
	__coverity_tainted_data_argument__(bytes);
}

int gp_port_usb_msg_interface_read    (GPPort *port, int request, int value, int index, char *bytes, int size) {
	__coverity_tainted_data_argument__(bytes);
}

int gp_port_usb_msg_class_read    (GPPort *port, int request, int value, int index, char *bytes, int size) {
	__coverity_tainted_data_argument__(bytes);
}

int gp_port_send_scsi_cmd (GPPort *port, int to_dev,
                                char *cmd, int cmd_size,
                                char *sense, int sense_size,
                                char *data, int data_size) {
	if (!to_dev) {
		__coverity_tainted_data_argument__(data);
		__coverity_tainted_data_argument__(sense);
	}
}

void gp_log_data (const char *domain, const char *data, unsigned int size) {
	/* considered a tainted sink ... but is not one. */
}

