/****************************************************************/
/* pccam300.h - Gphoto2 library for the Creative PC-CAM 300     */
/*                                                              */
/*                                                              */
/* Authors: Till Adam <till@adam-lilienthal.de>                 */
/*          Miah Gregory <mace@darksilence.net>                 */
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


#ifndef __PCCAM300_H__
#define __PCCAM300_H__

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

int pccam300_init (GPPort *port, GPContext *context);
int pccam300_close (GPPort *port, GPContext *context);
int pccam300_get_file_list (GPPort *port, GPContext *context);
int pccam300_delete_file (GPPort *port, GPContext *context, int index);
int pccam300_delete_all (GPPort *port, GPContext *context);
int pccam300_get_file (GPPort *port, GPContext *context, int index,
		       unsigned char **data, unsigned int *size,
		       unsigned int *type);
int pccam300_get_mem_info (GPPort *port, GPContext *context, int *totalmem,
			   int *freemem);
int pccam300_get_filecount (GPPort *port, int *filecount);
int pccam300_get_filesize (GPPort *port, unsigned int index,
			   unsigned int *filesize);

enum {
	PCCAM300_MIME_JPEG,
	PCCAM300_MIME_WAV,
	PCCAM300_MIME_AVI
};


#endif /* __PCCAM300_H__ */
