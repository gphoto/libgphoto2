#include "config.h"
#include <sys/types.h>

#include "getuint.h"

u_short
get_u_short(buf)
     u_char 	*buf;
{
  return ((u_short)buf[0] << 8) | buf[1];
}

u_int
get_u_int(buf)
        u_char  *buf;
{
        u_int   t;

        t = (((u_int)buf[0] << 8) | buf[1]) << 16;;
        t |= ((u_int)buf[2] << 8) | buf[3];;
        return t;
}

