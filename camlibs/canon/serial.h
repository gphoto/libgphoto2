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

int canon_serial_change_speed(gpio_device *gdev, int speed);
int canon_serial_init(Camera *camera, const char *devname);
int canon_serial_close(gpio_device *gdev);
int canon_serial_restore(Camera *camera);
int canon_serial_send(Camera *camera, const unsigned char *buf, int len, int sleep);
int canon_serial_get_byte(gpio_device *gdev);
int canon_serial_get_cts(gpio_device *gdev);

void serial_flush_input(gpio_device *gdev);
void serial_flush_output(gpio_device *gdev);
void serial_set_timeout(gpio_device *gdev, int to);

#endif /* _SERIAL_H */

/****************************************************************************
 *
 * End of file: serial.h
 *
 ****************************************************************************/
