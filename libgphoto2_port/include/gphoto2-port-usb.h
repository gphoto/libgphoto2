#ifndef _GP_PORT_USB_H_
#define _GP_PORT_USB_H_

/* USB port specific settings */
typedef struct {
	int  inep;
	int  outep;
	int  config;
	int  interface;
	int  altsetting;
} gp_port_usb_settings;

extern struct gp_port_operations gp_port_usb_operations;

#endif /* _GP_PORT_USB_H_ */
