/*
 * Jenopt JD11 Camera Driver
 * Copyright (C) 1999-2001 Marcus Meissner <marcus@jet.franken.de> 
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#include "libgphoto2/bayer.h"

#include "serial.h"
#include "decomp.h"

#if 0
static int
dread(GPPort *port, caddr_t buf, int xsize) {
    int i;
    int ret = gp_port_read(port,buf,xsize);

    if (ret == -1) {
	perror("dread");
	return -1;
    }
    fprintf(stderr,"dread[%d]:",ret);
    for (i=0;i<ret;i++) fprintf(stderr,"%02x,",buf[i]);
    fprintf(stderr,"\n");
    return ret;
}
static int
dwrite(GPPort*port, caddr_t buf, int xsize) {
    int i;
    int ret = gp_port_write(port,buf,xsize);

    if (ret == -1) {
	perror("dwrite");
	return -1;
    }
    fprintf(stderr,"dwrite[%d/%d]:",xsize,ret);
    for (i=0;i<xsize;i++) fprintf(stderr,"%02x,",buf[i]);
    fprintf(stderr,"\n");
    return ret;
}
#endif

#define READ gp_port_read
#define WRITE gp_port_write

static int _send_cmd(GPPort *port,unsigned short cmd) {
    unsigned char buf[2];
    buf[0] = cmd>>8;
    buf[1] = cmd&0xff;
    return WRITE(port,buf,2);
}

static void _read_cmd(GPPort *port,unsigned short *xcmd) {
	unsigned char buf[2];
	int	i = 0;
	*xcmd = 0x4242;
	do {
		if (2==READ(port,buf,2)) {
			if (buf[0]==0xff)
				break;
			continue;
		}
	} while (i++<10);
	*xcmd = (buf[0]<<8)|buf[1];
}

#if 0
static void _dump_buf(unsigned char *buf,int size) {
	int i;
	fprintf(stderr,"[");
	for (i=0;i<size;i++)
		fprintf(stderr,"%02x ",buf[i]);
	fprintf(stderr,"]\n");
}
#endif

int jd11_ping(GPPort *port) {
	unsigned short xcmd;
	char	buf[1];
	int	ret;

	while (1==READ(port,buf,1))
	    	/* drain input queue before PING */;
	ret=_send_cmd(port,0xff08);
	if (ret!=GP_OK)
	    return ret;
	_read_cmd(port,&xcmd);
	if (xcmd==0xfff1)
	    return GP_OK;
	return GP_ERROR_IO;
}

int
jd11_get_rgb(GPPort *port,float *red, float *green, float *blue) {
	char	buf[10];
	int	ret=GP_OK,tries=0,curread=0;

	_send_cmd(port,0xffa7);
	while ((curread<10) && (tries++<30)) {
	    ret=READ(port,buf+curread,sizeof(buf)-curread);
	    if (ret < 0)
		continue;
	    if (ret == 0)
		break;
	    curread+=ret;
	}
	if(curread<10) {
	    fprintf(stderr,"%d returned bytes on float query.\n",ret);
	    return GP_ERROR_IO;
	}
	/*_dump_buf(buf,10);*/
	*green	= buf[1]+buf[2]*0.1+buf[3]*0.01;
	*red	= buf[4]+buf[5]*0.1+buf[6]*0.01;
	*blue	= buf[7]+buf[8]*0.1+buf[9]*0.01;
	return GP_OK;
}

int
jd11_set_rgb(GPPort *port,float red, float green, float blue) {
	unsigned char	buf[20];

	_send_cmd(port,0xffa7);
	buf[0] = 0xff;
	buf[1] = (int)green;
	buf[2] = ((int)(green*10))%10;
	buf[3] = ((int)(green*100))%10;
	buf[4] = (int)red;
	buf[5] = ((int)(red*10))%10;
	buf[6] = ((int)(red*100))%10;
	buf[7] = (int)blue;
	buf[8] = ((int)(blue*10))%10;
	buf[9] = ((int)(blue*100))%10;
	/*_dump_buf(buf,10);*/
	return WRITE(port,buf,10);
}

int
jd11_select_index(GPPort *port) {	/* select index */
	unsigned short xcmd;

	_send_cmd(port,0xffa4);
	_read_cmd(port,&xcmd);
	if (xcmd!=0xff01)
	    return GP_ERROR_IO;
	return GP_OK;
}

int
jd11_select_image(GPPort *port,int nr) {	/* select image <nr> */
	unsigned short xcmd;

	_send_cmd(port,0xffa1);_send_cmd(port,0xff00|nr);
	_read_cmd(port,&xcmd);
	if (xcmd != 0xff01)
	    return GP_ERROR_IO;
	return GP_OK;
}

int
jd11_set_bulb_exposure(GPPort *port,int i) {
	unsigned short xcmd;

	if ((i<1) || (i>9))
	    return GP_ERROR_BAD_PARAMETERS;

	_send_cmd(port,0xffa9);
	_send_cmd(port,0xff00|i);
	_read_cmd(port,&xcmd);
	return GP_OK;
}

#if 0
static void jd11_TestADC(GPPort *port) {
	unsigned short xcmd;

	_send_cmd(port,0xff75);
	_read_cmd(port,&xcmd);
	fprintf(stderr,"TestADC: done, xcmd=%x\n",xcmd);
}
static void cmd_TestDRAM(GPPort *port) {
	unsigned short xcmd;

	_send_cmd(port,0xff72);
	_read_cmd(port,&xcmd);
	fprintf(stderr,"TestDRAM: done.\n");
}

/* doesn't actually flash */
static void cmd_TestFLASH(GPPort *port) {
	unsigned short xcmd;

	_send_cmd(port,0xff73);
	_read_cmd(port,&xcmd);
	fprintf(stderr,"TestFLASH: xcmd = %x.\n",xcmd);
}

/* some kind of selftest  ... shuts the shutters, beeps... only stops by 
 * powercycle. */
static void cmd_79(GPPort *port) {
	unsigned short xcmd;

	_send_cmd(port,0xff79);
	_read_cmd(port,&xcmd);
	fprintf(stderr,"79: done, xcmd =%x\n",xcmd);
}
#endif


static int
jd11_imgsize(GPPort *port) {
	char	buf[20];
	int	ret;
	int	i=0,curread=0;

	_send_cmd(port,0xfff0);
	do {
		ret=READ(port,&buf[curread],10-curread);
		if (ret>0)
		    curread+=ret;
		usleep(1000);
	} while ((i++<20) && (curread<10));
	/*_dump_buf(buf,curread);*/
	if (!curread) /* We get 0 bytes return for 0 images. */
	    return 0;
	ret=strtol(&buf[2],NULL,16);
	return ret;
}

static int
getpacket(GPPort *port,unsigned char *buf, int expect) {
        int curread = 0, csum = 0;
	int tries = 0;
	if (expect == 200)
	    expect++;
	while (tries++<5) {
		int i=0,ret;

		do {
			ret=READ(port,buf+curread,expect-curread);
			if (ret>0) {
				curread+=ret;
				i=0;
				continue;
			}
			usleep(100);
		} while ((i++<2) && (curread<expect));
		if (curread!=expect) {
		    if (!curread)
			return 0;
		    _send_cmd(port,0xfff3);
		    curread = csum = 0;
		    continue;
		}
		/*printf("curread is %d\n",curread);*/
		/*printf("PACKET:");_dump_buf(buf,curread);*/
		for (i=0;i<curread-1;i++)
			csum+=buf[i];
		if (buf[curread-1]==(csum&0xff) || (curread!=201))
			return curread-1;
		fprintf(stderr,"BAD CHECKSUM %x vs %x, trying resend...\n",buf[curread-1],csum&0xff);
		_send_cmd(port,0xfff3);
		curread = csum = 0;
		/*return curread-1;*/
	}
	fprintf(stderr,"Giving up after 5 tries.\n");
	return 0;
}

int
jd11_erase_all(GPPort *port) {
	return _send_cmd(port,0xffa6);
}

int
jd11_get_image_preview(Camera *camera,int nr, char **data, int *size) {
	int	xsize,curread=0,ret=0;
	char	*indexbuf,*src,*dst;
	int	y;
	char	header[200];
	GPPort	*port = camera->port;

	if (nr < 0) return GP_ERROR_BAD_PARAMETERS;
	ret = jd11_select_index(port);
	if (ret != GP_OK)
	    return ret;
	xsize = jd11_imgsize(port);
	if (nr > xsize/(64*48)) {
	    fprintf(stderr,"ERROR: nr %d is larger than %d\n",nr,xsize/64/48);
	    return GP_ERROR_BAD_PARAMETERS;
	}
	xsize = (xsize / (64*48)) * (64*48);
	indexbuf = malloc(xsize);
	if (!indexbuf) return GP_ERROR_NO_MEMORY;
	_send_cmd(port,0xfff1);
	while (1) {
	    	int readsize = xsize-curread;
		if (readsize>200) readsize = 200;
		ret=getpacket(port,indexbuf+curread,readsize);
		if (ret==0)
			break;
		gp_camera_progress(camera,(1.0*curread)/xsize);
		curread+=ret;
		if (ret<200)
			break;
		_send_cmd(port,0xfff1);
	}
	strcpy(header,"P5\n# gPhoto2 JD11 thumbnail image\n64 48 255\n");
	*size = 64*48+strlen(header);
	*data = malloc(*size);
	if (!*data) return GP_ERROR_NO_MEMORY;
	strcpy(*data,header);
	src = indexbuf+(nr*64*48);
	dst = (*data)+strlen(header);

	for (y=0;y<48;y++) {
	    int x,off = 64*y;
	    for (x=0;x<64;x++)
		dst[47*64-off+(63-x)] = src[off+x];
	}
	free(indexbuf);
	return GP_OK;
}

int
jd11_file_count(GPPort *port,int *count) {
    int		xsize,curread=0,ret=0;
    char	tmpbuf[202];

    ret = jd11_select_index(port);
    if (ret != GP_OK)
	return ret;
    xsize = jd11_imgsize(port);
    if (!xsize) { /* shortcut, no reading needed */
	*count = 0;
	return GP_OK;
    }
    *count = xsize/(64*48);
    xsize = *count * (64*48);
    _send_cmd(port,0xfff1);
    while (curread <= xsize) {
	int readsize = xsize-curread;
	if (readsize>200) readsize = 200;
	ret=getpacket(port,tmpbuf,readsize);
	if (ret==0)
	    break;
	curread+=ret;
	if (ret<200)
	    break;
	_send_cmd(port,0xfff1);
    }
    return GP_OK;
}

static int
serial_image_reader(Camera *camera,int nr,unsigned char ***imagebufs,int *sizes) {
    int	picnum,curread,ret=0;
    GPPort *port = camera->port;

    jd11_select_image(port,nr);
    *imagebufs = (unsigned char**)malloc(3*sizeof(char**));
    for (picnum=0;picnum<3;picnum++) {
	curread=0;
	sizes[picnum] = jd11_imgsize(port);
	(*imagebufs)[picnum]=(unsigned char*)malloc(sizes[picnum]+400);
	_send_cmd(port,0xfff1);
	while (curread<sizes[picnum]) {
	    int readsize = sizes[picnum]-curread;
	    if (readsize > 200) readsize = 200;
	    ret=getpacket(port,(*imagebufs)[picnum]+curread,readsize);
	    if (ret==0)
		break;
	    gp_camera_progress(camera,1.0*picnum/3.0+(curread*1.0)/sizes[picnum]/3.0);
	    curread+=ret;
	    if (ret<200)
		break;
	    _send_cmd(port,0xfff1);
	}
    }
    return GP_OK;
}


int
jd11_get_image_full(Camera *camera,int nr, char **data, int *size,int raw) {
    unsigned char	*s,*uncomp[3],**imagebufs;
    int			ret,sizes[3];
    char		header[200];
    int 		h;

    ret = serial_image_reader(camera,nr,&imagebufs,sizes);
    if (ret!=GP_OK)
	return ret;
    uncomp[0] = malloc(320*480);
    uncomp[1] = malloc(320*480/2);
    uncomp[2] = malloc(320*480/2);
    if (sizes[0]!=115200) {
	    picture_decomp_v1(imagebufs[0],uncomp[0],320,480);
	    picture_decomp_v1(imagebufs[1],uncomp[1],320,480/2);
	    picture_decomp_v1(imagebufs[2],uncomp[2],320,480/2);
    } else {
	    picture_decomp_v2(imagebufs[0],uncomp[0],320,480);
	    picture_decomp_v2(imagebufs[1],uncomp[1],320,480/2);
	    picture_decomp_v2(imagebufs[2],uncomp[2],320,480/2);
    }
    strcpy(header,"P6\n# gPhoto2 JD11 thumbnail image\n640 480 255\n");
    *size = 640*480*3+strlen(header);
    *data = malloc(*size);
    strcpy(*data,header);
    if (!raw) {
	unsigned char *bayerpre;
	s = bayerpre = malloc(640*480);
	/* note that picture is upside down and left<->right mirrored */
	for (h=480;h--;) {
	    int w;
	    for (w=320;w--;) {
		if (h&1) {
		    /* G B G B G B G B G */
		    *s++ = uncomp[2][(h/2)*320+w];
		    *s++ = uncomp[0][h*320+w];
		} else {
		    /* R G R G R G R G R */
		    *s++ = uncomp[0][h*320+w];
		    *s++ = uncomp[1][(h/2)*320+w];
		}
	    }
	}
	gp_bayer_decode(bayerpre,640,480,*data+strlen(header),BAYER_TILE_RGGB);
	free(bayerpre);
    } else {
	s=(*data)+strlen(header);
	for (h=480;h--;) { /* upside down */
	    int w;
	    for (w=640;w--;) { /* right to left */
		/* and images are in green red blue */
		*s++=uncomp[1][(h/2)*320+(w/2)];
		*s++=uncomp[0][h*320+(w/2)];
		*s++=uncomp[2][(h/2)*320+(w/2)];
	    }
	}
    }
    free(uncomp[0]);free(uncomp[1]);free(uncomp[2]);
    free(imagebufs[0]);free(imagebufs[1]);free(imagebufs[2]);free(imagebufs);
    return GP_OK;
}
