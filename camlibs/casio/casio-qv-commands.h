/* casio-qv-commands.h
 *
 * Copyright 2001 Lutz Mueller
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __CASIO_QV_COMMANDS_H__
#define __CASIO_QV_COMMANDS_H__

#include <gphoto2/gphoto2-library.h>

int QVping     (Camera *camera);

/* Battery level in volts */
int QVbattery  (Camera *camera, float *battery);
int QVrevision (Camera *camera, long int *revision);
int QVnumpic   (Camera *camera);
int QVstatus   (Camera *camera, char *status);
int QVdelete   (Camera *camera, int n);
int QVprotect  (Camera *camera, int n, int on);
int QVpicattr  (Camera *camera, int n, unsigned char *attr);
int QVshowpic  (Camera *camera, int n);
int QVsetpic   (Camera *camera);
int QVsize     (Camera *camera, long int *size);
int QVgetYCCpic(Camera *camera, unsigned char **data, unsigned long int *size);
int QVgetCAMpic(Camera *camera, unsigned char **data, unsigned long int *size, int fine);
int QVcapture  (Camera *camera);
int QVreset    (Camera *camera);
int QVsetspeed (Camera *camera, int speed);

#endif /* __CASIO_QV_COMMANDS_H__ */
