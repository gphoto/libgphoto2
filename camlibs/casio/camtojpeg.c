/* camtojpeg.c
 *
 * Copyright (c) 2004 Michael Haardt
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

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>

#include "cam2jpgtab.h"
#include "jpegtab_f.h"
#include "camtojpeg.h"

static unsigned int u16(const unsigned char *buf)
{
  return ((unsigned int)(*buf)<<8) | ((unsigned int)*(buf+1));
}

static unsigned int u32(const unsigned char *buf)
{
  return ((unsigned int)(*buf)<<24) | ((unsigned int)*(buf+1)<<16)
         |((unsigned int)*(buf+2)<<8) | ((unsigned int)*(buf+3));
}

int QVcamtojpeg(const unsigned char *cam, long int camSize, unsigned char **jpeg, long int *jpegSize)
{
  int ysize;
  int usize;
  int vsize;
  unsigned char *dst;

  ysize = u16(cam + 2);
  usize = u16(cam + 4);
  vsize = u16(cam + 6);

  *jpeg=malloc(*jpegSize=
  (
    sizeof(soi)+sizeof(app0)
    +sizeof(dqt0)+64
    +sizeof(dqt1)+64
    +sizeof(sof)+sizeof(dht)
    +sizeof(sos_y)+ysize
    +sizeof(sos_u)+usize
    +sizeof(sos_v)+vsize
    +sizeof(eoi)
  ));

  dst=*jpeg;
  memcpy(dst,soi,sizeof(soi));     dst+=sizeof(soi);
  memcpy(dst,app0,sizeof(app0));   dst+=sizeof(app0);

  cam+=8;

  memcpy(dst,dqt0,sizeof(dqt0));   dst+=sizeof(dqt0);
  memcpy(dst,cam,64);              dst+=64;            cam+=64;

  memcpy(dst,dqt1,sizeof(dqt1));   dst+=sizeof(dqt1);
  memcpy(dst,cam,64);              dst+=64;            cam+=64;

  memcpy(dst,sof,sizeof(sof));     dst+=sizeof(sof);
  memcpy(dst,dht,sizeof(dht));     dst+=sizeof(dht);

  memcpy(dst,sos_y,sizeof(sos_y)); dst+=sizeof(sos_y);
  memcpy(dst,cam,ysize);           dst+=ysize;         cam+=ysize;

  memcpy(dst,sos_u,sizeof(sos_u)); dst+=sizeof(sos_u);
  memcpy(dst,cam,usize);           dst+=usize;         cam+=usize;

  memcpy(dst,sos_v,sizeof(sos_v)); dst+=sizeof(sos_v);
  memcpy(dst,cam,vsize);           dst+=vsize;         cam+=vsize;

  memcpy(dst,eoi,sizeof(eoi));

  return GP_OK;
}

int QVfinecamtojpeg(const unsigned char *cam, long int camSize, unsigned char **jpeg, long int *jpegSize)
{
  int size;
  static const unsigned char c[1] = { 1 };
  unsigned char *dst;

  size = u32(cam + 4);

  *jpeg=malloc(*jpegSize=
  (
    sizeof(soi)+sizeof(app_f)
    +sizeof(dqt_f)+64+1+64
    +sizeof(sof_f)+sizeof(dht_f)
    +sizeof(sos_f)+size
    +sizeof(eoi)
  ));

  dst=*jpeg;
  memcpy(dst,soi,sizeof(soi));     dst+=sizeof(soi);
  memcpy(dst,app_f,sizeof(app_f)); dst+=sizeof(app_f);

  cam+=8;

  memcpy(dst,dqt_f,sizeof(dqt_f)); dst+=sizeof(dqt_f);
  memcpy(dst,cam,64);              dst+=64;            cam+=64;

  memcpy(dst,c,sizeof(c));         dst+=sizeof(c);
  memcpy(dst,cam,64);              dst+=64;            cam+=64;

  memcpy(dst,sof_f,sizeof(sof_f)); dst+=sizeof(sof_f);
  memcpy(dst,dht_f,sizeof(dht_f)); dst+=sizeof(dht_f);

  memcpy(dst,sos_f,sizeof(sos_f)); dst+=sizeof(sos_f);
  memcpy(dst,cam,size);            dst+=size;          cam+=size;

  memcpy(dst,eoi,sizeof(eoi));

  return GP_OK;
}
