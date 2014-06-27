/* Simple JPEG tag extractor and viewer
 *
 * Copyright 2003 Marcus Meissner <marcus@jet.franken.de>
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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define APP0	0xe0
#define APP1	0xe1
#define APP2	0xe2
#define DHT	0xc4
#define SOI	0xd8
#define DQT	0xdb
#define SOF0	0xc0
#define SOS	0xda
#define COM	0xfe

static int
rb(int suppress) {
	unsigned char c;

	if (1!=read(fileno(stdin),&c,1))
		return -1;
	if (!suppress)
		fprintf(stderr,"%02x",c);
	return c;
}

static int
rd(int len, char *data) {
	unsigned int c,i;

	for (i=0;i<len;i++) {
		c = rb(1);
		if (c == -1) return -1;
		if (c == 0xff) c = rb(1);
		if (c == -1) return -1;
		data[i] = c;
	}
	return len;
}

static int
getmarker(void) {
	int c;
	
	c = rb(0);
	if (c == -1) return -1;
	if (c!=0xff) return -1;
	c = rb(0);
	return c;
}

static int
getlength(void) {
	int x1,x2;
	
	x1 = rb(1);
	if (x1 == -1) return -1;
	x2 = rb(1);
	if (x2 == -1) return -1;
	x1 =  (x1 << 8) | x2;
	fprintf(stderr,"(%d)",x1);
	return x1;
}

static int
skipdata(int len, int suppress) {
	int x,i;

	for (i=0;i<len;i++) {
		x = rb(suppress);
		if (x == -1)
			return x;
		if (x == 0xff) {
			x = rb(suppress);
			if (x == -1)
				return x;
		}
	}
	return 1;
}

static int
haslength(int marker) {
	switch (marker) {
	case SOI:	return 0;
	case APP0:	return 1;
	case APP1:	return 1;
	case APP2:	return 1;
	case DHT:	return 1;
	case SOS:	return 1;
	case DQT:	return 1;
	case COM:	return 1;
	default:	return 1;
	}
}

static int
print_jfif() {
	int major, minor, density, x, y, r, tx, ty ;
	r = rb(1); if (r == -1) return -1;

	major = rb(1);
	if (major == -1) return -1;
	minor = rb(1);
	if (minor == -1) return -1;
	density = rb(1);
	if (density == -1) return -1;

	x = rb(1); if (x == -1) return -1;
	r = rb(1); if (r == -1) return -1;
	x = (x << 8) | r;

	y = rb(1); if (y == -1) return -1;
	r = rb(1); if (r == -1) return -1;
	y = (y << 8) | r;
	tx = rb(1); if (tx == -1) return -1;
	ty = rb(1); if (ty == -1) return -1;
	fprintf(stderr,"JFIF v%d.%d, dens %d, %dx%d, tn %dx%d\n", major, minor, density, x, y, tx, ty);
	return skipdata(tx * ty * 3,1);
}

int
main(int argc, char **argv) {
	int m, len;

	m = getmarker();
	if (m != SOI) {
		fprintf(stderr,":unknown, aborting.\n"); 
		return 1;
	}
	fprintf(stderr,":SOI\n");
	while (1) {
		m = getmarker();
		if (m == -1) {
			fprintf(stderr,":Error, end of file, aborting.\n");
			break;
		}
		if (haslength(m))
			len = getlength();
		else
			len = 0;
		switch (m) {
		case COM: {
			char *comment;
			
			comment = malloc(len - 2 + 1);
			comment[len-2]='\0';
			if (-1 == rd(len - 2, comment))
				break;
			fprintf(stderr,":COM(%s)\n",comment);
			break;
		}
		case SOS: {
			int i,ns,ss,se,ahl,cs,td;

			ns = rb(1);if (ns == -1) break;
			fprintf(stderr,":SOS ");
			for (i=0;i<ns;i++) {
				cs = rb(1);if (cs == -1) break;
				td = rb(1);if (td == -1) break;
				fprintf(stderr,"[%d,%d:%d]",cs,(td>>4),td&0xf);
			}
			ss = rb(1); if (ss == -1) break;
			se = rb(1); if (se == -1) break;
			ahl = rb(1); if (ahl == -1) break;
			fprintf(stderr,"ss=%d, se=%d, h=%d,l=%d\n",ss,se,(ahl>>4),ahl&0xf);
			break;
		}
		case SOF0: {
			int bitcnt,x,y,nf,i,c,hv,tq;
			m = rb(1); if (m == -1) break;
			bitcnt = m;
			y = rb(1); if (y == -1) break;
			m = rb(1); if (m == -1) break;
			y = (y<<8) | m;
			x = rb(1); if (x == -1) break;
			m = rb(1); if (m == -1) break;
			x = (x<<8) | m;
			nf = rb(1); if (nf == -1) break;

			fprintf(stderr,":SOF0,bitcnt=%d,x=%d,y=%d:\n",bitcnt,x,y);
			for (i=0;i<nf;i++) {
				c = rb(1); if (c == -1) break;
				hv = rb(1); if (hv == -1) break;
				tq = rb(1); if (tq == -1) break;
				fprintf(stderr,"\t %02x = %d:%d, tq=%d\n",c,(hv>>4),hv&0xf,tq);
			}
			break;
		}
		case DHT: {
			m = rb(1);
			if (m == -1) break;
			fprintf(stderr,":DHT(0x%02x)\n",m);
			skipdata(len - 2 - 1,1);
			break;
		}
		case DQT: {
			fprintf(stderr,":DQT\n");
			skipdata(len-2,1);
			break;
		}
		case APP1: case APP2:
		case APP0: {
			char name[5];
			fprintf(stderr,":APP0(");
			if (-1==rd(4,name))
				break;
			name[4] = '\0';
			fprintf(stderr,"%s):", name);
			if (!strcmp(name,"JFIF")) {
				print_jfif();
			} else {
				if (-1==skipdata(len - 2 - 4,0)) {
					break;
				}
				fprintf(stderr,"\n");
			}
			break;
		}
		default:
			fprintf(stderr,":Unknown marker\n");
			if (len) {
				if (-1==skipdata(len - 2,0)) {
					break;
				}
				fprintf(stderr,"\n");
			}
			break;
		}
	}
	return 0;
}
