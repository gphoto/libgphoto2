/****************************************************************/
/* library.h  - Gphoto2 library for accessing the Panasonic     */
/*              Coolshot KXL-600A & KXL-601A digital cameras.   */
/*                                                              */
/* Copyright 2001 Chris Pinkham                                 */
/*                                                              */
/* Author: Chris Pinkham <cpinkham@infi.net>                    */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

int coolshot_sm           (Camera *camera);
int coolshot_sb           (Camera *camera, int speed);
int coolshot_enq			(Camera *camera);
int coolshot_file_count		(Camera *camera);
int coolshot_request_image (Camera *camera, CameraFile *file, char *buf, int *len, int number, GPContext *context);
int coolshot_request_thumbnail (Camera *camera, CameraFile *file, char *buf, int *len, int number, GPContext *context);
int coolshot_build_thumbnail (char *data, int *size);
