/* pccam600.h
 *  
 *  This library is free software; you can redistribute and/or
 *  modify it inder the terms of the GNU Lesser Genreral Public
 *  License as publiced by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warrenty of
 *  MERCHANTABULITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  lesser General Public License for more details.
 *
 *  Author: Peter Kajberg <pbk@odense.kollegienet.dk>
 */

#ifndef __PCCAM600_H__
#define __PCCAM600_H__

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

typedef struct {
  unsigned char state; /*delete or ok*/
  unsigned char unk1;
  unsigned char quality;
  unsigned char name[9];
  unsigned char unk2[4];

  unsigned char unk3[10];
  unsigned char unk4[2];
  unsigned char unk5;
  unsigned char size[2];
  unsigned char unk6;
} FileEntry;

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

int pccam600_init(GPPort *port, GPContext *context);
int pccam600_close(GPPort *port, GPContext *context);
int pccam600_get_file_list(GPPort *port, GPContext *context);
int pccam600_delete_file(GPPort *port, GPContext *context, int index);
int pccam600_read_data(GPPort *port, unsigned char *buffer);
int pccam600_get_file(GPPort *port, GPContext *context, int index);
int pccam600_get_mem_info(GPPort *port, GPContext *context, int *totalmem,
			  int *freemem);
#endif /* __PCCAM600_H__ */

