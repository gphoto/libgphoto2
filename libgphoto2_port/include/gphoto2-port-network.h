#ifndef _GP_PORT_NETWORK_H_
#define _GP_PORT_NETWORK_H_

/* socket specific settings */
typedef struct {
	char address[20];
} gp_port_network_settings;

extern struct gp_port_operations gp_port_network_operations;

#endif /* _GP_PORT_NETWORK_H_ */




