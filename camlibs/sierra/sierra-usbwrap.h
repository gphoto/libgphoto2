/*
 * sierra_usbwrap.h
 */

#include "gphoto2.h"

int usb_wrap_write_packet(gp_port* dev, char* sierra_msg, int sierra_len);
int usb_wrap_read_packet(gp_port* dev, char* sierra_response, int sierra_len);

