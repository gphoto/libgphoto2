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

int canon_serial_change_speed(int speed);
int canon_serial_init(const char *devname);
int canon_serial_close();
int canon_serial_restore();
int canon_serial_send(const unsigned char *buf, int len, int sleep);
int canon_serial_get_byte();
int canon_serial_get_cts(void);

void serial_flush_input(void);
void serial_flush_output(void);
void serial_set_timeout(int to);

#endif /* _SERIAL_H */

/****************************************************************************
 *
 * End of file: serial.h
 *
 ****************************************************************************/
