/****************************************************************************
 *
 * File: serial.h
 *
 ****************************************************************************/

#ifndef _SERIAL_H
#define _SERIAL_H

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

int canon_serial_change_speed(gp_port *gdev, int speed);
int canon_serial_init(Camera *camera, const char *devname);
int canon_serial_send(Camera *camera, const unsigned char *buf, int len, int sleep);
int canon_serial_get_byte(gp_port *gdev);
int canon_serial_get_cts(gp_port *gdev);

int canon_usb_camera_init(Camera *camera);

void serial_flush_input(gp_port *gdev);
void serial_flush_output(gp_port *gdev);
void serial_set_timeout(gp_port *gdev, int to);

#endif /* _SERIAL_H */

/****************************************************************************
 *
 * End of file: serial.h
 *
 ****************************************************************************/
