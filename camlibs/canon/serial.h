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

int canon_serial_change_speed(GPPort *gdev, int speed);
int canon_serial_init(Camera *camera, const char *devname);
int canon_serial_send(Camera *camera, const unsigned char *buf, int len, int sleep);
int canon_serial_get_byte(GPPort *gdev);
int canon_serial_get_cts(GPPort *gdev);

int canon_usb_camera_init(Camera *camera);

void serial_flush_input(GPPort *gdev);
void serial_flush_output(GPPort *gdev);
void serial_set_timeout(GPPort *gdev, int to);

#endif /* _SERIAL_H */

/****************************************************************************
 *
 * End of file: serial.h
 *
 ****************************************************************************/
