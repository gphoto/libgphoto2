/*
 * Jenopt JD11 Camera Driver
 * Copyright 1999-2001 Marcus Meissner <marcus@jet.franken.de>
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

#define _DEFAULT_SOURCE

#include "config.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#include <libgphoto2/bayer.h>
#include "libgphoto2/i18n.h"

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

    if (ret < GP_OK) {
	perror("dwrite");
	return -1;
    }
    fprintf(stderr,"dwrite[%d/%d]:",xsize,ret);
    for (i=0;i<xsize;i++) fprintf(stderr,"%02x,",buf[i]);
    fprintf(stderr,"\n");
    return ret;
}
#endif

#define READ(port,buf,len) gp_port_read(port,(char*)(buf),len)
#define WRITE(port,buf,len) gp_port_write(port,(char*)(buf),len)

static int _send_cmd(GPPort *port,unsigned short cmd) {
    unsigned char buf[2];
    buf[0] = cmd>>8;
    buf[1] = cmd&0xff;
    return WRITE(port,buf,2);
}

static int _read_cmd(GPPort *port,unsigned short *xcmd) {
	unsigned char buf[2];
	int	i = 0,ret;
	*xcmd = 0x4242;
	do {
		if (1==(ret=READ(port,buf,1))) {
			if (buf[0]==0xff) {
			    if (1==READ(port,buf+1,1)) {
				*xcmd = (buf[0] << 8)| buf[1];
				return GP_OK;
			    }
			}
		} else {
		    return ret;
		}
	} while (i++<10);
	return GP_ERROR_IO;
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


static int _send_cmd_2(GPPort *port,unsigned short cmd, unsigned short *xcmd) {
    unsigned char buf[2];
    int ret, tries = 3;
    *xcmd = 0x4242;
    while (tries--) {
	int i = 0;
	buf[0] = cmd>>8;
	buf[1] = cmd&0xff;
	ret = WRITE(port,buf,2);
	do {
		if (1==(ret=READ(port,buf,1))) {
			if (buf[0]==0xff) {
			    if (1==READ(port,buf+1,1)) {
				*xcmd = (buf[0] << 8)| buf[1];
				return GP_OK;
			    }
			}
		} else {
		    return ret;
		}
	} while (i++<3);
    }
    return GP_ERROR_IO;
}

int jd11_ping(GPPort *port) {
	unsigned short xcmd;
	char	buf[1];
	int	ret,tries = 3;

	while (tries--) {
	    ret = GP_ERROR_IO;
	    while (1==READ(port,buf,1))
		    /* drain input queue before PING */;
	    ret=_send_cmd_2(port,0xff08,&xcmd);
	    if ((ret>=GP_OK) && (xcmd==0xfff1))
		return GP_OK;
	}
	return ret;
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
	int ret;

	ret = _send_cmd_2(port,0xffa4,&xcmd);
	if (ret < GP_OK)
	    return ret;
	if (xcmd!=0xff01)
	    return GP_ERROR_IO;
	return GP_OK;
}

int
jd11_select_image(GPPort *port,int nr) {	/* select image <nr> */
	unsigned short xcmd;

	_send_cmd(port,0xffa1);
	_send_cmd(port,0xff00|nr);
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

/* This function reads all thumbnails at once and initializes the whole
 * camera filesystem. This can be done, because finding out how much
 * pictures are on the camera is done by reading the whole preview picture
 * stream anyway.
 * And since the file infos are static mostly, why not just set them too at
 * the same time.
 */
int
jd11_index_reader(GPPort *port, CameraFilesystem *fs, GPContext *context) {
    int		i, id, count, xsize, curread=0, ret=0;
    unsigned char	*indexbuf;

    ret = jd11_select_index(port);
    if (ret != GP_OK)
	return ret;
    xsize = jd11_imgsize(port);
    if (!xsize) { /* shortcut, no reading needed */
	return GP_OK;
    }
    count = xsize/(64*48);
    xsize = count * (64*48);
    indexbuf = malloc(xsize);
    if (!indexbuf) return GP_ERROR_NO_MEMORY;
    id = gp_context_progress_start (context, xsize,
				    _("Downloading thumbnail..."));
    _send_cmd(port,0xfff1);
    while (curread < xsize) {
	    int readsize = xsize-curread;
	    if (readsize>200) readsize = 200;
	    ret=getpacket(port,indexbuf+curread,readsize);
	    if (ret==0)
		    break;
	    curread+=ret;
	    if (ret<200)
		    break;
	    gp_context_progress_update (context, id, curread);
	    if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
		/* What to do...Just free the stuff we allocated for now.*/
		free(indexbuf);
		return GP_ERROR_CANCEL;
	    }
	    _send_cmd(port,0xfff1);
    }
    gp_context_progress_stop (context, id);
    for (i=0;i<count;i++) {
	CameraFile	*file;
	char		fn[20];
	unsigned char *src;
	unsigned char thumb[64*48];
	int y;
	CameraFileInfo	info;

	ret = gp_file_new(&file);
	if (ret!=GP_OK) {
	    free(indexbuf);
	    return ret;
	}
	sprintf(fn,"image%02i.pgm",i);
	gp_file_set_mime_type(file, GP_MIME_PGM);
	gp_file_append(file, THUMBHEADER, strlen(THUMBHEADER));
	src = indexbuf+(i*64*48);
	for (y=0;y<48;y++) {
	    int x,off = 64*y;
	    for (x=0;x<64;x++)
		thumb[47*64-off+(63-x)] = src[off+x];
	}
	ret = gp_file_append(file,(char*)thumb,sizeof(thumb));
	if (ret != GP_OK) {
    		free(indexbuf);
		gp_file_free (file);
		return ret;
	}
	ret = gp_filesystem_append(fs, "/", fn, context);
	if (ret != GP_OK) {
		/* should perhaps remove the entry again */
    		free(indexbuf);
		gp_file_free (file);
		return ret;
	}
	ret = gp_filesystem_set_file_noop(fs, "/", fn, GP_FILE_TYPE_PREVIEW, file, context);
	if (ret != GP_OK) {
    		free(indexbuf);
		return ret;
	}

	memset (&info, 0, sizeof (info));
	/* we also get the fs info for free, so just set it */
	info.file.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE;
	strcpy(info.file.type,GP_MIME_PNM);
	info.file.width		= 640;
	info.file.height	= 480;
	info.file.size		= 640*480*3+strlen(IMGHEADER);
	info.preview.fields = GP_FILE_INFO_TYPE |
			GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
			GP_FILE_INFO_SIZE;
	strcpy(info.preview.type,GP_MIME_PGM);
	info.preview.width	= 64;
	info.preview.height	= 48;
	info.preview.size	= 64*48+strlen(THUMBHEADER);
	ret = gp_filesystem_set_info_noop(fs, "/", fn, info, context);
    }
    free(indexbuf);
    return GP_OK;
}

static int
serial_image_reader(Camera *camera,CameraFile *file,int nr,unsigned char ***imagebufs,int *sizes, GPContext *context) {
    int	picnum,curread,ret=0;
    GPPort *port = camera->port;
    unsigned int id;

    jd11_select_image(port,nr);
    *imagebufs = (unsigned char**)malloc(3*sizeof(unsigned char*));
    for (picnum=0;picnum<3;picnum++) {
	curread=0;
	sizes[picnum] = jd11_imgsize(port);
	(*imagebufs)[picnum]=(unsigned char*)malloc(sizes[picnum]+400);
	_send_cmd(port,0xfff1);
	id = gp_context_progress_start (context, sizes[picnum],
			_("Downloading data..."));
	while (curread<sizes[picnum]) {
	    int readsize = sizes[picnum]-curread;
	    if (readsize > 200) readsize = 200;
	    ret=getpacket(port,(*imagebufs)[picnum]+curread,readsize);
	    if (ret==0)
		break;
	    curread+=ret;
	    if (ret<200)
		break;
	    gp_context_progress_update (context, id, curread);
	    if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL) {
		int j;
		/* What to do ... Just free the stuff we allocated for now. */
		for (j=0;j<picnum;j++)
		    free((*imagebufs)[picnum]);
		free(*imagebufs);
		return GP_ERROR_CANCEL;
	    }
	    _send_cmd(port,0xfff1);
	}
	gp_context_progress_stop (context, id);
    }
    return GP_OK;
}


int
jd11_get_image_full(
    Camera *camera, CameraFile*file, int nr, int raw, GPContext *context
) {
    unsigned char	*s,*uncomp[3],**imagebufs;
    int			ret,sizes[3];
    unsigned char	*data;
    int 		h;

    ret = serial_image_reader(camera,file,nr,&imagebufs,sizes, context);
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
    gp_file_append(file, IMGHEADER, strlen(IMGHEADER));
    data = malloc(640*480*3);
    if (!data) {
        free(uncomp[0]);free(uncomp[1]);free(uncomp[2]);
        free(imagebufs[0]);free(imagebufs[1]);free(imagebufs[2]);free(imagebufs);
        return GP_ERROR_NO_MEMORY;
    }

    if (!raw) {
	unsigned char *bayerpre;
	s = bayerpre = malloc(640*480);
	/* note that picture is upside down and left<->right mirrored */
	for (h=480;h--;) {
	    int w;
	    for (w=320;w--;) {
		if (h&1) {
		    /* G B G B G B G B G */
		    *s++ = uncomp[0][h*320+w];
		    *s++ = uncomp[2][(h/2)*320+w];
		} else {
		    /* R G R G R G R G R */
		    *s++ = uncomp[1][(h/2)*320+w];
		    *s++ = uncomp[0][h*320+w];
		}
	    }
	}
	/*gp_bayer_decode(bayerpre,640,480,data,BAYER_TILE_GRBG);*/
	gp_ahd_decode(bayerpre,640,480,data,BAYER_TILE_GRBG);
	free(bayerpre);
    } else {
	s=data;
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
    gp_file_append(file, (char*)data, 640*480*3);
    free(data);
    return GP_OK;
}
