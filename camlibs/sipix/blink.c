/* sipixblink.c
 *
 * Copyright 2002 Vincent Sanders <vince@kyllikki.org>
 * Copyright 2002 Marcus Meissner <marcus@jet.franken.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2 of the License
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
/*
 * This library is unfinished! -Marcus
 *
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

int
camera_id (CameraText *id)
{
	strcpy(id->text, "SiPix Blink");
	return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	strcpy(a.model, "SiPix:Blink");
	a.status		= GP_DRIVER_STATUS_EXPERIMENTAL;
	a.port			= GP_PORT_USB;
	a.usb_vendor		= 0x0851;
	a.usb_product		= 0x1542;
	a.operations		= GP_OPERATION_NONE;
	a.file_operations	= 0;
	a.folder_operations	= GP_FOLDER_OPERATION_NONE;
	gp_abilities_list_append(list, a);
	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	return (GP_OK);
}

static int
_get_number_images(Camera *camera) {
	int ret;
	unsigned char reply[8];

	ret = gp_port_usb_msg_read (camera->port, 0, 0, 0, reply, 2);
	if (ret < GP_OK)
		return ret;
	return reply[0] | (reply[1]<<8);
}

#if 0
static void
_mult_64words(short *result, short *in, short *mult) {
	int i;

	for (i=0;i<64;i++)
		result[i] = in[i] * mult[i];
}
#endif

static int
decomp_dct(
	unsigned short *outtable,
	unsigned char *input,
	unsigned short *table
) {
	int inpos = 1, outpos = 0;
	int xinp = input[0];
	int curinflags = (input[0] & 0x30) << 2;

	if (!input[inpos]) return inpos;

	while (1) {
		if (outpos >= 64) return inpos;
		if (inpos >= 128) return inpos;

		switch (curinflags) {
		case 0x00: /* Set position into the 8x8 DCT */
			xinp = input[inpos];
			curinflags = xinp & 0xc0;
			outpos = xinp & 0x3f;
			fprintf(stderr,"00:set outpos to %d\n", outpos);
			inpos++;
			break;
		case 0x40: /* Set 2 DCT values from 2x 3 bit data */
			xinp = input[inpos];
			curinflags = xinp & 0xc0;
			outtable[outpos+1] = (table+0)[xinp];
			outtable[outpos  ] = (table+256)[xinp];
			outpos += 2;
			inpos  += 1;
			fprintf(stderr,"40:input was %02x values are %x, %x\n", xinp, table[xinp], (table+256)[xinp]);
			break;
		case 0x80: /* Set 1 DCT value from 6bit compressed data */
			xinp = input[inpos];
			curinflags = xinp & 0xc0;
			outtable[outpos] = (table+512)[xinp];
			inpos++;
			outpos++;
			fprintf(stderr,"80:input was %02x, value is %x\n", xinp, (table+512)[xinp]);
			break;
		case 0xc0: /* Set 1 value in DCT, 9 bit */
			xinp=((input[inpos+0]&0x1f)<<7)|(input[inpos+1]&0x7f);
			if (xinp & 0x0800)
				xinp |= 0xff00;
			curinflags = input[inpos+1] & 0xc0;
			outtable[outpos] = xinp;
			outpos++;
			inpos += 2;
			fprintf(stderr,"80:input was %04x\n", xinp);
			break;
		default:
			break;
		}
		if (!input[inpos]) return inpos;
	}
}

static void
init_dct_uncomp_table(unsigned short *tables) {
	unsigned short *table1 = tables;
	unsigned short *table2 = tables + 256;
	unsigned short *table3 = tables + 512;
	int	i;

	for (i=0;i<256;i++) {
		unsigned short x;
		if (i & 4) { /* negative */
			x = i | 0xfff8;
		} else {
			x = i & 0x007;
		}
		table1[i] = x;

		if (i & 0x20) {
			x = (i >> 3) | 0xfff8;
		} else {
			x = (i >> 3) & 0x0007;
		}
		table2[i] = x;

		if (i & 0x20) {
			x = i | 0xffc0;
		} else {
			x = i & 0x003f;
		}
		table3[i] = x;
	}
}

static void db(unsigned char *b, int howmuch) {
	int i;
	for (i=0;i<howmuch;i++) fprintf(stderr,"%02x ",b[i]);
	fprintf(stderr,"\n");
}

static int
decompressor(unsigned char *input) {
	int	cur = 0;
	int	xword;
	short	intable[768];
	short	dct[64];
	int	xy;

	init_dct_uncomp_table(intable);

	db(input+cur,3);
	xword = ((input[cur] & 0x3f) << 7) | (input[cur+1] & 0x7f);
	cur += 3;

	fprintf(stderr,"xword is %x\n",xword);

	for (xy=0;xy<640/4;xy++) { /* assume 640 pixels width for now */
		db(input+cur,1);
		if ((input[cur] & 0xc0) == 0xc0) {
			db(input+cur+1,1);
			fprintf(stderr,"0xc0 case.\n");

			/* do stuff */

			cur+=2;
		} else {
			int i,j;
			memset(dct, 0, 32*4);

			cur += decomp_dct(dct, input+cur, intable)+1;

			/* Dump DCT */
			for (i=0;i<8;i++) {
				fprintf(stderr,"|");
				for (j=0;j<8;j++)
					fprintf(stderr,"%4d ",dct[i*8+j]);
				fprintf(stderr,"|\n");
			}

			/*_mult_64words(dct2, dct, [var15c - 0x6c]);*/
			/*IDCT_AAN(dct2);*/
			/* copy out */
		}
	}
	return cur;
}


#if 0

struct curbuf {
	unsigned char *dstptr;
	unsigned char *srcptr;
};

copy_blob(struct curbuf *arg0, struct curbuf *arg4, int off, int *lineoffsets) {
	int i;
	int *source, *dest;

	ecx = arg0->dstptr;
	edx = arg4->dstptr;

	eax = arg0->srcptr;
	esi = arg4->srcptr;

	dest = lineoffsets[off];

	if (off)
		source = lineoffsets[off-1]
	else
		source = lineoffsets[0];

	esi -= eax;

	for (i = 8; i--; ) {
		edi = source + (word)[esi+eax];
		ebx = dest + (word)[eax];
		eax+= 0x10;
		[ebx+ecx-7] = [edi+edx-7];
		[ebx+ecx-3] = [edi+edx-3];
	}
}

10001dcf(arg0, short *ecx, arg8, argc) { /* 10011af0 */
	int i,j;

	eax = arg0;
	edx = argC;
	ecx = (word)arg8;
	edx = [edx + ecx*4];

	esi = [eax];
	edi = [eax+4];

	/* copy out a 8x8 matrix */
	for (ebp=8;ebp--;) {
		eax = (word)[edi] + edx;

		edi += 0x10;
		ecx += 8; /* 16 bytes */

		for (j=0;j<8;j++)
			esi[eax+j] = (byte)100cf224( *(ecx+(8-j)) );
	}
}

static int
_decompressor(
) {
	char *buf;
	int cur;

	ebx = arg0;
	edx = [ebx+0x19c];
	eax = [ebx+0x138];
	cx  = [ebx+0x134];
	di  = [ebx+0x136];
	var148 = edx;
	edx = [ebx+0x7ba];
	var14c = edi;
	ebp = 0;
	edi--;
	buf = eax;
	var150 = ecx;
	cur = 6;
	var158 = ebp;
	if (edx == ebp) goto 100112af;
	if (cx <= bp) goto 10011262;
1001114e:
	ecx = (buf[cur] & 0x3f) << 7) | (buf[cur+1] & 0x7f);
	edx = var158; /* word -> dword zero ext. */
	if (edx != ecx) return 0;
	cur += 3;

	var154 = 0;
	if (var14c <= 0) goto 10011242;
	var15c = ebx+16c;
1001118e:
	al = [cur + buf];
	if ((al & 0xc0 ) == 0xc0) {
		call 10011c80([var15c],[ebx+di*4+0x16c],ebp,var148);
		cur += 2;
	} else {
		memset(dct1,0,2*8*8);
		cur = cur + decomp_dct( dct1, cur + buf, ebx) + 1;
		_mult_64words(&dct, dct1, [var15c - 0x6c]);
		IDCT_AAN(&dct);
		10001dcf([15c], &dct, ebp, var148);
	}
1001121c:
	var15c += 4;
	ebp++;
	edi = var154;
	var154++;
	if (var154 < var14c) goto 1001118e;

10011242:
	if (cur > [ebx+7bc]) goto 100113c9;
	var158++;
	if (var158 < var150) goto 1001114e;

10011262:
	al = [ebx+0x7c0];
	if (al != 9) goto 1001145e;

	eax = [ebx+0x144];
	cdq;
	eax -= edx;
	esi = eax;
	eax = [ebx+0x148];
	cdq;
	eax -= edx;
	edi = eax;
	edi >>= 1;
	esi >>= 1;
	var150 = malloc(edi * esi);
	if (!var150) return 0;
	goto 100113f3;

100112af:
	if (cx <= bp) goto 10011262;
100112b4:
	cl = [cur+buf];
	dl = [cur+buf+1];
	ecx & = 0x3f;
	edx & = 0x7f;
	ecx <<= 7;
	ecx |= edx;

	edx = var158;
	if (ecx != edx) return 0;
	cur+=3;
	var154 = 0;
	if (var14c <= 0) goto 100113a8;
	var15c = ebx+0x16c;

100112f4:
	edx = buf+cur;
	al = buf+cur;
	if (al & 0xc0 != 0xc0) goto 10011325;

	100020b8([var15c], [ebx+di*4+0x16c], ebp, var148);
	cur += 2;
	goto 10011382;
10011325:
	memset(xtable128, 0, 32*4); / 8x8 16bit words */
	decomp_dct(xtable128, edx, ebx);
	esi  = cur+buf+1;
	_mult_64words(&var140, xtable128, [var15c - 0x6c]);
	IDCT_AAN(&var140);
	1000123f([edi], &var140, var148);

10011382:
	eax = var154;
	edx = var15c;
	ebp++;
	edi = eax;
	eax++;
	edx += 4;
	var154 = eax;
	var15c = edx;
	if (var154 < var14c) goto 100112f4;


100113a8:
	if (cur > [ebx+0x7bc]) goto 100113c9;
	ecx = var158;
	ecx++;
	var158 = cx;
	if (cx < var150) goto 100112b4;
	goto 10011262;


100113c9:
	sprintf(&var40,"rlidx %d > pDecmpExt->CompressedDataLen %d !",
		cur,
		[ebx+0x7bc]
	);
	return 0;

100113f3:
	ebp = malloc(edi * esi);
	if (!ebp) return 0;

1001140f:
	100021e9(var150, [ebx+0x15c], esi, edi);
	100021e9(ebp, [ebx+0x164], esi ,edi);

	eax = [ebx+0x15c]
	[ebx+0x15c] = var150;
	free(eax);
	eax = [ebx+0x164]
	[ebx+0x164] = ebp;
	free(eax);
	goto 10011489;
1001145e:
	if (al != 8) goto 10011489;
	if (! [ebx+0x7c4] ) goto 10011489;
	1000202c([ebx+0x154], [ebx+0x144], [ebx+0x148]);
10011489:
	if ([ebx + 0x7a8] == 1) {
		100014ce(ebx);
	} else {
		1000150a(ebx);
	}
	eax = [ebx + 0x7a8];
	eax--;
	switch (eax) {
	case 1:
		10001889(ebx);
		return 1;
	case 2:
		10001f28(ebx);
		return 1;
	case 3:
	case 5:
		10001271(ebx);
		return 1;
	case 4:
		100012c1(ebx);
		return 1;
	case 6:
		10001712(ebx);
		return 1;
	case 7:
		100011c2(ebx);
		return 1;
	default:
		return 1;
	}
}

_10001889(this) {
	ecx = [this + 0x7a4];
	var4 = ecx;
	var70 = 3168;
	var94 = ecx + 0x835;
	var7c = ecx + 0x18b4;
	var74 = ecx + 0x1491;
	var80 = ecx + 0x106f;
	var84 = ecx + 0x0412;
	var78 = 0x0030;
10012f90:
	var90 = var84;
	var8c = var94;
	var9c = ecx - 6;
	var44 = -6;
	edx = ecx - 3;
	var3c = -3;
	ebx = ecx - 5;
	var2c = -5;
	var88 = ecx+1;
	var34 = 1;
	vara0 = ecx-2;
	var24 = -2;
	var98 = ecx - 4;
	var14 = -4;
	vara4 = ecx + 2;
	var1c = 2;
	vara8 = ecx - 1;
	var0c = -1;

	var20 = var9c - edx;
	var68 = ebx - edx;
	ecx = var88;
	var38 = var88 - edx;
	var64 = vara0 - edx;
	var54 = var98 - edx;
	var5c = vara4 - edx;
	var4c = vara8 - edx;

	var50 = var9c - ebx;
	var28 = vara0 - ebx;
	var08 = var98 - ebx;
	var48 = vara4 - ebx;
	var40 = vara8 - ebx;

	var18 = ebx - ecx
	ebp = edx;
	vara0 = 0x20;
	var10 = edx - ecx;

	var30 = var98 - ecx;
	var60 = vara4 - ecx;
	var58 = vara8 - ecx;
	ecx = ebx - edx;
	var9c = var98 - edx;
	ebp = var8c;
	vara4 = vara4 - edx;
	vara8 = vara8 - edx;
	edx = var90;

	var88 = ecx;
	goto 1001311b;
10013117:
	ecx = var88;
1001311b:
	ebx = 0;
	bl = [edx + ecx];
	ecx = 0;
	cl = [eax - 0x14a0];
	ebx += ecx;
	ecx = 0;
	ebx = ebx >> 1;
	[esi - 0x1080] = bl;
	cl = [ebp - 0x420];
	ebx = 0;
	ebp = vara4;
	bl = [edi - 0xc60];
	ecx += ebx;
	ebx = var9c;
	ecx = ecx >> 1;
	[edx] = cl;
	ecx = 0;
	cl = [edx + ebx];
	ebx = 0;
	bl = [edx + ebp];
	ebp = var8c;
	ecx += ebx;
	ebx = vara8;
	ecx = ecx >> 1;
	[ebx + edx] = cl;
	ecx = var18;
	edx = 0;
	ebx = var30;
	dl = [ecx + ebp];
	ecx = 0;
	cl = [eax - 0x1080];
	edx += ecx;
	ecx = 0;
	edx = edx >> 1;
	[esi - 0xc60] = dl;
	cl = [ebp];
	edx = 0;
	dl = [edi-0x840];
	edx += ecx;
	edx >>= 1;
	[ecx + ebp] = dl;
	ecx = var10;
1001319d ... oder so;

}

_10001f28(this) {
	int pitch;

	var0c = 0;
	arg0 = [this + 0x14c];
	var14 = [this + 0x7a4];
	var10 = [this + 0x7ac];
	pitch = [this + 0x144] * 3;
	var4 = [this + 0x150];
	var18 = 0;
	esi = 0;
	while (var0c < var4) {
		switch (var18 % 5) {
		case 0:
		case 2:
		case 4:
			var0c++;
			ebx = 0;
			edi = 0;
			ecx = var14;
			while (ebx  < arg0 )  {
				switch (edi % 25) {
				case 0: case 2: case 3: case 5: case 7: case 9:
				case 11: case 13: case 15: case 17: case 19: case 21:
				case 23: case 24:
					ebx++;
					memcpy(esi + var10, ecx, 3);
					esi+=3;
					break;
				}
				ecx += 3;
				edi++;
			}
		default: /* nothing */
			break;
		}
		var14 += pitch;
		var18++;
	}
}



_xdecomp(arg0, arg4, arg8, argc, arg10, arg14) {
	eax = argc;
	esi = arg10;
	ebx = eax - 8;
	edx = 0;
	ecx = [esi + 0x138];
	edi = 0;
	arg10 = ecx;
	eax = arg8;

	while (edx <= ebx) {
		if (!(dword)[eax] && !(byte)[eax+7]) break;
		memcpy(ecx,eax,8);
		edi+=8;
		ecx+=8;
		edx+=8;
		eax+=8;
	}
/*10010f6e:*/
	ecx = arg10;
	edi++;
	[edi + ecx - 1] ] = 0;
	[esi + 0x7bc] = edi;
	eax = _decompressor(esi);
	if (!eax) goto 1001104d;
	eax = arg14;
	if (!eax) goto 1001101f;
	eax = [esi + 0x14c];
	switch (eax) {
	case 800: 10014140(esi); break;
	case 640: 10013dd0(esi); break;
	case 352: 10013eb0(esi); break;
	case 160: 10014360(esi); break;
	case 176: 10013fd0(esi); break;
	case 320: 10014220(esi); break;
	}
}

/* shrinks image from CIF (352x288) -> VGA/4 (160x120) */
10013eb0(arg0) {
	int pitch;
	int xcnt, xpixels;
	int outoffset;
	char *outdata, *indata;

	outdata = [arg0 + 0x7b4];	/* target image data */
	pitch = 3*[arg0 + 0x14c];	/* width in real pixels */
	indata =  [arg0 + 0x7a4];	/* origin image data */
	xpixels = 0;
	xcnt = 0;
	while (xpixels < 120) {
		switch (xcnt % 12) {
		case 1: case 3: case 6: case 8: case 10: {
			char *tmpdata = indata;
			int ycnt = 0, ypixels = 0;

			xpixels++;
			while (ypixels < 160) {
				switch (ycnt % 11) {
				case 1: case 3: case 5: case 7: case 9:
					ypixels++;
					memcpy(outdata, tmpdata, 3);
					outdata += 3;
					break;
				default:
					break;
				}
				tmpdata+=3;
				ycnt++;
			}
			break;
		default /* 10013f67 */:
			break;
		}
		xcnt++;
		data += pitch;
	}
}

#endif


static int
_check_image_header(unsigned char *d, int size) {
	int width, height;

	if (d[0] != 0)
		return FALSE;
	if (d[1] > 0x3f)
		return FALSE;
	if ((d[2] & 0xc0) != 0x80)
		return FALSE;
	width	= ((d[2] & 0x3f) << 4) | ((d[3] >> 3) & 0xf);
	height	= ((d[3] & 0x07) << 7) | (d[4] & 0x7f);

	fprintf(stderr,"picture is %d x %d\n",width,height);

	return TRUE;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	unsigned char reply[8];
	int image_no, picsize;
	unsigned char *xdata, *ydata;


	if (strcmp(folder,"/"))
	    return GP_ERROR_BAD_PARAMETERS;
        image_no = gp_filesystem_number(fs, folder, filename, context);

        /* size */
	do {
		gp_port_usb_msg_read (camera->port, 1, image_no , 1, reply, 8);
	} while(reply[0]!=0);

	picsize = reply[1] | (reply[2] << 8) | ( reply[3] << 16);
	/* Setup bulk transfer */
    	do {
		gp_port_usb_msg_read (camera->port, 2, image_no , 0, reply, 6);
	} while(reply[0]!=0);

	/* bulk read */
	xdata = malloc(picsize);
	gp_port_read(camera->port, xdata, picsize);

	ydata = malloc(640*480*3);

	_check_image_header(xdata, picsize);

	if (0) { /* disabled. replace 0 by 1 to enable */
		int cur;

		cur = 6;
		while (cur < picsize)
			cur += decompressor(xdata+cur);
	}
	gp_file_append(file, xdata, picsize);
	free(xdata);
	gp_file_set_mime_type (file, GP_MIME_UNKNOWN);
	return (GP_OK);
}

static int
camera_config_get (Camera *camera, CameraWidget **window, GPContext *context)
{
	gp_widget_new (GP_WIDGET_WINDOW, "Camera Configuration", window);

	/* Append your sections and widgets here. */

	return (GP_OK);
}

static int
camera_config_set (Camera *camera, CameraWidget *window, GPContext *context)
{
	/*
	 * Check if the widgets' values have changed. If yes, tell the camera.
	 */

	return (GP_OK);
}

static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	/*
	 * Capture a preview and return the data in the given file (again,
	 * use gp_file_set_data_and_size, gp_file_set_mime_type, etc.).
	 * libgphoto2 assumes that previews are NOT stored on the camera's
	 * disk. If your camera does, please delete it from the camera.
	 */

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	char reply[6];
	int  oldimgno, newimgno, ret;

	oldimgno = _get_number_images(camera);
	if (oldimgno < GP_OK)
		return oldimgno;
	do {	/* FIXME: this will loop until a picture is taken. */
		ret = gp_port_usb_msg_read (camera->port, 4, 0, 0, reply, 6);
	} while ((ret >= GP_OK) && (reply[0]!=0));
	if (ret < GP_OK)
		return ret;
	newimgno = _get_number_images(camera);
	if (newimgno < GP_OK)
		return newimgno;
	if (newimgno == oldimgno)
		return GP_ERROR;
	strcpy(path->folder,"/");
	sprintf(path->name,"blink%03i.raw",newimgno);
	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	/*
	 * Fill out the summary with some information about the current
	 * state of the camera (like pictures taken, etc.).
	 */

	return (GP_OK);
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	/*
	 * If you would like to tell the user some information about how
	 * to use the camera or the driver, this is the place to do.
	 */

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text, _("Sipix StyleCam Blink Driver\n"
			       "Vincent Sanders <vince@kyllikki.org>\n"
			       "Marcus Meissner <marcus@jet.franken.de>.\n"
			       ));

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	CameraFileInfo  info;
	unsigned char reply[6];
	int i, numpics, ret;
	char fn[20];

	numpics = _get_number_images(camera);
	if (numpics < GP_OK)
		return numpics;
	/* gp_list_populate (list, "blink%03i.raw", numpics); */
#if 1
	for (i=0;i<numpics;i++) {
		/* we also get the fs info for free, so just set it */
		info.file.fields = GP_FILE_INFO_TYPE |
				GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT |
				GP_FILE_INFO_SIZE;
		strcpy(info.file.type,GP_MIME_UNKNOWN);
		sprintf(fn,"blink%03i.raw",i);
		ret = gp_filesystem_append(fs, "/", fn, context);
		if (ret != GP_OK)
			return ret;
		do {
			ret=gp_port_usb_msg_read(camera->port,1,i,8,reply,6);
			if (ret < GP_OK)
				return ret;
		} while (reply[0]!=0);

		switch (reply[5] >> 5) {
		case 0:	/* VGA */
			info.file.width		= 640;
			info.file.height	= 480;
			break;
		case 1:	/* CIF */
			info.file.width		= 352;
			info.file.height	= 288;
			break;
		case 2:	/* QCIF */
			info.file.width		= 176;
			info.file.height	= 144;
			break;
		case 3:	/* QVGA */
			info.file.width		= 320;
			info.file.height	= 240;
			break;
		case 4:	/* ? */
			info.file.width		= 800;
			info.file.height	= 592;
			break;
		case 5:	/* VGA/4 */
			info.file.width		= 160;
			info.file.height	= 120;
			break;
		default:
			return GP_ERROR;
		}
		info.file.size		=
			(reply[1])	|
			(reply[2] << 8)	|
			(reply[3] << 16)|
			(reply[4] << 24);
		ret = gp_filesystem_set_info_noop(fs, "/", info, context);
		if (ret != GP_OK)
			return ret;
	}
#endif
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
};

int
camera_init (Camera *camera, GPContext *context)
{
	int res;
	char reply[8];

	GPPortSettings settings;

	/* First, set up all the function pointers */
	camera->functions->exit                 = camera_exit;
	camera->functions->get_config           = camera_config_get;
	camera->functions->set_config           = camera_config_set;
	camera->functions->capture              = camera_capture;
	camera->functions->capture_preview      = camera_capture_preview;
	camera->functions->summary              = camera_summary;
	camera->functions->manual               = camera_manual;
	camera->functions->about                = camera_about;

	/* Now, tell the filesystem where to get lists, files and info */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	/*
	 * The port is already provided with camera->port (and
	 * already open). You just have to use functions like
	 * gp_port_timeout_set, gp_port_settings_get, gp_port_settings_set.
	 */

	/* Get the current settings */
	gp_port_get_settings (camera->port, &settings);

	/* Modify the default settings the core parsed */
	settings.usb.interface=1;/* we need to use interface 1 */
	settings.usb.inep=4;


	/* Set the new settings */
	res = gp_port_set_settings (camera->port, settings);
	if (res != GP_OK) {
		gp_context_error (context, _("Could not apply USB settings"));
		return res;
	}

	/*
	 * Once you have configured the port, you should check if a
	 * connection to the camera can be established.
	 */
	gp_port_usb_msg_read (camera->port, 5, 1, 0, reply, 2);
	if(reply[0]!=1)
	    return (GP_ERROR_IO);

	gp_port_usb_msg_read (camera->port, 5, 0, 0, reply, 8);
	if(reply[0]!=1)
	    return (GP_ERROR_IO);

	return (GP_OK);
}
