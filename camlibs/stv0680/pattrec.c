/*
 * pattrec - demosaic by Y/Cr/Cb with pattern recognition
 * Copyright (C) 2000 Eric Brombaugh <emeb@goodnet.com>                         
 *
 * Redistributed under the GPL with STV0680 driver by kind permission.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   
 */

/*
 *  pattrec.c - demosaic by Y/Cr/Cb method with pattern recognition
 *  09-02-00 E. Brombaugh
 *
 *  14-12-2000 - trivial modification allowing caller to specify
 *               image size. (Adam Harrison <adam@antispin.org>)                
 */

#include <stdio.h>
#include <math.h>

#define RED 0
#define GREEN 1
#define BLUE 2
#define ISEVEN(a) (((a)&1)!=1)
#define GETADD(w, x, y) ((3*(x))+(w*3*(y)))
#define MAX(x, y) (((x)>(y))?(x):(y))
#define MIN(x, y) (((x)<(y))?(x):(y))

/* pattern recognition average */
float patt_rec_avg(int w, float *fgfx, int x, int y)
{
  float d[4], s[4], avg, temp, out;
  int i, done, patt;
  
  /* get the four directions */
  d[3]= fgfx[GETADD(w, x-1,y)+GREEN]; /* west  */
  d[2] = fgfx[GETADD(w, x,y-1)+GREEN]; /* north */
  d[1] = fgfx[GETADD(w, x+1,y)+GREEN]; /* east  */
  d[0] = fgfx[GETADD(w, x,y+1)+GREEN]; /* south */
  s[3] = d[3];
  s[2] = d[2];
  s[1] = d[1];
  s[0] = d[0];
  
  /* compute the average value of four directions */
  avg = (d[0]+d[1]+d[2]+d[3])/4.0;
  
  /* sort the four directions so that s[0]>s[1]>s[2]>s[3] */
  do
  {
    done = 1;
    for(i=0;i<3;i++)
    {
      if(s[i]<s[i+1])
      {
        temp = s[i];
        s[i] = s[i+1];
        s[i+1] = temp;
        done = 0;
      }
    }
  }
  while(!done);

  /* compute the pattern as a 4-bit binary */
  patt = 0;
  for(i=0;i<4;i++)
    patt = (patt<<1) + ((d[3-i]>avg) ? 1 : 0);
  
  switch(patt)
  {
     case 0:
       /* all equal */
       return avg;
       break;
       
     case 1:
     case 2:
     case 4:
     case 7:
     case 8:
     case 11:
     case 13:
     case 14:
       /* edge */
       /* average of middle two values */
       return (s[1]+s[2])/2.0;
       break;
     
     case 5:
     case 10:
       /* stripe */
       /* average of outer ring */
       temp = (fgfx[GETADD(w, x-2,y+1)+GREEN] +      /* wsw */
               fgfx[GETADD(w, x-2,y-1)+GREEN] +      /* wnw */
               fgfx[GETADD(w, x-1,y-2)+GREEN] +      /* nnw */
               fgfx[GETADD(w, x+1,y-2)+GREEN] +      /* nne */
               fgfx[GETADD(w, x+2,y-1)+GREEN] +      /* ene */
               fgfx[GETADD(w, x+2,y+1)+GREEN] +      /* ese */
               fgfx[GETADD(w, x+1,y+2)+GREEN] +      /* sse */
               fgfx[GETADD(w, x-1,y+2)+GREEN])/8.0;  /* ssw */
       
       /* compute weighted difference of inner and outer rings */
       avg = 2*avg - temp;
       break;
     
     case 3:
     case 12:
       /* sw/ne corner */
       /* average sw/ne outer diagonal pts */
       temp = (fgfx[GETADD(w, x-2,y+1)+GREEN] +      /* wsw */
               fgfx[GETADD(w, x+1,y-2)+GREEN] +      /* nne */
               fgfx[GETADD(w, x+2,y-1)+GREEN] +      /* ene */
               fgfx[GETADD(w, x-1,y+2)+GREEN])/4.0;  /* ssw */

       /* compute weighted difference of median and outer points */
       avg = (s[1]+s[2]) - temp;
       break;
       
     case 6:
     case 9:
       /* se/nw corner */
       /* average se/nw outer diagonal pts */
       temp = (fgfx[GETADD(w, x-2,y-1)+GREEN] +      /* wnw */
               fgfx[GETADD(w, x-1,y-2)+GREEN] +      /* nnw */
               fgfx[GETADD(w, x+2,y+1)+GREEN] +      /* ese */
               fgfx[GETADD(w, x+1,y+2)+GREEN])/8.0;  /* sse */
       
       /* compute weighted difference of median and outer points */
       avg = (s[1]+s[2]) - temp;
       break;
       
     case 15:
     default:
       /* shouldn't happen */
  }
   
       
  /* clip to middle two values */
  out = MIN(s[1],avg);
  out = MAX(s[2],out);

  return out;
}

int bayer2color[] = {1,0,2,1};

/* saturate and round to 8-bit int */
int satrnd(float in)
{
  int out;
  out = in + 0.5;
  out = out > 255 ? 255 : out;
  out = out < 0 ? 0 : out;
  return out;
}

float *fgfx;

/* the main routine */
int pattrec(int w, int h, unsigned char *igfx)
{
  int i, x, y;
  int bayer;
  float avg;

  if((fgfx = malloc(sizeof(float) * w * h * 3)) == NULL)
	return -1;

  /* start by copying data to a float array to keep things simple */
  for(i=0;i<(w*h*3);i++)
    fgfx[i] = igfx[i];

  /* interpolate green at all red & blue points, calculate Cr & Cb */
  for(y=4;y<(h-2);y++)
  {
    for(x=2;x<(w-2);x++)
    {
      /* compute bayer pixel number 0:g1, 1:r, 2:b, 3:g2 */
      bayer = ((ISEVEN(y)) ? 0 : 2) + ((ISEVEN(x)) ? 0 : 1);

      switch(bayer)
      {
        case 1: /* RED  */
        case 2: /* BLUE */
          fgfx[GETADD(w, x,y)+GREEN] = patt_rec_avg(w, fgfx, x, y);

          if(bayer==1)
            fgfx[GETADD(w, x,y)+RED] -= fgfx[GETADD(w, x,y)+GREEN];
          else
            fgfx[GETADD(w, x,y)+BLUE] -= fgfx[GETADD(w, x,y)+GREEN];

        case 0: /* GREEN 1 */
        case 3: /* GREEN 2 */
      }
    }
  }

  /* Spatially filter Cr/Cb */
  for(y=4;y<(h-2);y++)
  {
    for(x=2;x<(w-2);x++)
    {
      /* compute bayer pixel number 0:g1, 1:r, 2:b, 3:g2 */
      bayer = ((ISEVEN(y)) ? 0 : 2) + ((ISEVEN(x)) ? 0 : 1);

      switch(bayer)
      {
        case 0: /* GREEN 1 */
          fgfx[GETADD(w, x,y)+RED] = (fgfx[GETADD(w, x-1,y)+RED]+
                                   fgfx[GETADD(w, x+1,y)+RED])/2.0;
          fgfx[GETADD(w, x,y)+BLUE] = (fgfx[GETADD(w, x,y-1)+BLUE]+
                                    fgfx[GETADD(w, x,y+1)+BLUE])/2.0;
          break;

        case 1: /* RED  */
          fgfx[GETADD(w, x,y)+BLUE] = (fgfx[GETADD(w, x-1,y-1)+BLUE]+
                                    fgfx[GETADD(w, x-1,y+1)+BLUE]+
                                    fgfx[GETADD(w, x+1,y+1)+BLUE]+
                                    fgfx[GETADD(w, x+1,y-1)+BLUE])/4.0;
          break;

        case 2: /* BLUE */
          fgfx[GETADD(w, x,y)+RED] = (fgfx[GETADD(w, x-1,y-1)+RED]+
                                   fgfx[GETADD(w, x-1,y+1)+RED]+
                                   fgfx[GETADD(w, x+1,y+1)+RED]+
                                   fgfx[GETADD(w, x+1,y-1)+RED])/4.0;

          break;

        case 3: /* GREEN 2 */
          fgfx[GETADD(w, x,y)+RED] = (fgfx[GETADD(w, x,y-1)+RED]+
                                   fgfx[GETADD(w, x,y+1)+RED])/2.0;
          fgfx[GETADD(w, x,y)+BLUE] = (fgfx[GETADD(w, x-1,y)+BLUE]+
                                    fgfx[GETADD(w, x+1,y)+BLUE])/2.0;
      }
    }
  }

  /* Restore R/B from Y,Cr/Cb */
  for(y=4;y<(h-2);y++)
  {
    for(x=2;x<(w-2);x++)
    {
      fgfx[GETADD(w, x,y)+RED] += fgfx[GETADD(w, x,y)+GREEN];
      fgfx[GETADD(w, x,y)+BLUE] += fgfx[GETADD(w, x,y)+GREEN];
    }
  }

  /* copy floating point data back to integer array */
  for(i=0;i<(h*w*3);i++)
    igfx[i] = satrnd(fgfx[i]);

  free(fgfx);

  return 0;
}
