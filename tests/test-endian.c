
/* test-filesys.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gphoto2-endian.h>
#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>


#define CHECK(r) if (!(r)) { fprintf(stderr,"%s:%d: result unexpected.\n",__FILE__,__LINE__); exit(1); }

int
main ()
{
	unsigned char buf[16];
	unsigned long long u64;
	unsigned short u16;
	unsigned int u32;

	u16 = swap16(0x1234);
	CHECK(u16 == 0x3412);
	u32 = swap32(0x12345678);
	CHECK(u32 == 0x78563412);
	u64 = swap64(0x0123456789abcdefULL);
	CHECK(u64 == 0xefcdab8967452301ULL);

	u16 = htobe16(0x1234); memcpy(buf,&u16,2); CHECK((buf[0]==0x12)&&(buf[1]==0x34));
	u32 = htobe32(0x12345678); memcpy(buf,&u32,4); CHECK((buf[0]==0x12)&&(buf[1]==0x34)&&(buf[2]==0x56)&&(buf[3]==0x78));
	u64 = htobe64(0x0123456789abcdefULL); memcpy(buf,&u64,8);
	CHECK((buf[0]==0x01)&&(buf[1]==0x23)&&(buf[2]==0x45)&&(buf[3]==0x67));
	CHECK((buf[4]==0x89)&&(buf[5]==0xab)&&(buf[6]==0xcd)&&(buf[7]==0xef));

	u16 = htole16(0x1234); memcpy(buf,&u16,2); CHECK((buf[0]==0x34)&&(buf[1]==0x12));
	u32 = htole32(0x12345678); memcpy(buf,&u32,4); CHECK((buf[0]==0x78)&&(buf[1]==0x56)&&(buf[2]==0x34)&&(buf[3]==0x12));
	u64 = htole64(0x0123456789abcdefULL); memcpy(buf,&u64,8);
	CHECK((buf[7]==0x01)&&(buf[6]==0x23)&&(buf[5]==0x45)&&(buf[4]==0x67));
	CHECK((buf[3]==0x89)&&(buf[2]==0xab)&&(buf[1]==0xcd)&&(buf[0]==0xef));
	return (0);
}

/*
 * Local variables:
 *  compile-command: "gcc -pedantic -Wstrict-prototypes -O2 -g test-filesys.c -o test-filesys `gphoto2-config --cflags --libs` && export MALLOC_TRACE=test-filesys.log && ./test-filesys && mtrace ./test-filesys test-filesys.log"
 * End:
 */
