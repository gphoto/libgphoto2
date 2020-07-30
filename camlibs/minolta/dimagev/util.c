/**********************************************************************
*       Minolta Dimage V digital camera communication library         *
*               Copyright 2000,2001 Gus Hartmann                      *
*                                                                     *
*    This program is free software; you can redistribute it and/or    *
*    modify it under the terms of the GNU General Public License as   *
*    published by the Free Software Foundation; either version 2 of   *
*    the License, or (at your option) any later version.              *
*                                                                     *
*    This program is distributed in the hope that it will be useful,  *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of   *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
*    GNU General Public License for more details.                     *
*                                                                     *
*    You should have received a copy of the GNU General Public        *
*    License along with this program; if not, write to the *
*    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
*    Boston, MA  02110-1301  USA
*                                                                     *
**********************************************************************/

/* $Id$ */

#include "config.h"

#include "dimagev.h"

#define GP_MODULE "dimagev"

/* This function handles the ugliness of converting Y:Cb:Cr data to RGB. The
   return value is a pointer to an array of unsigned chars that has 14413)
   members. The additional thirteen bytes are the PPM "rawbits" header.

   The Dimage V returns 9600 bytes of data for a thumbnail, in what is referred
   to in the documentation as a "Y:Cb:Cr (4:2:2)" format. This means that the
   data is in a Y:Cr:Cb solor space, and is horizontally compressed so that
   the Cb and Cr values are used for two adjacent bytes. The data essentially
   looks like this:

   Y(0):Y(1):Cb(0):Cr(0):Y(2):Y(3) ... Y(9599):Y(9600):Cb(9599):Cr(9599)

	Each of the above is a byte. The image is defined as having 80 pixels of
	width and 60 of height, and this information is not included in the 9600
	bytes of data. Since Y:Cb:Cr data is essentially useless, this function
	converts it into RGB data, with a PPM "rawbits" header containing the
	height and width. The formulae to convert Y:Cb:Cr to RGB are below:

	B(0,0) = (Cb - 128) * (2 - ( 2 * Cr_Coeff )) + Y(0,0)
	R(0,0) = (Cr - 128) * (2 - ( 2 * Y_Coeff  )) + Y(0,0)
	G(0,0) = (Y(0,0) - ( Cr_Coeff * B(0,0) + Y_Coeff * R(0,0)) ) / Cb_Coeff

	Still with me? Good. The values for the three coefficients all add up to
	one; the values used here are defined in dimagev.h . The code below is
	more complex than this, however; Minolta has not been forthcoming with
	the permitted ranges for the values of Y, Cb, and Cr, so I have had to
	do some guesswork based on the math. I don't know much about colorspaces,
	and I'm not looking to know more. It looks like Cb and Cr have a range
	of (0,128) and Y has a range of (0,255). The Dimage V sometimes returns
	values for Cb and Cr slightly over this range; I crop these to 128. Also,
	sometimes the values returned by the conversion formulae are greater
	than 255, which would overflow the unsigned char used to store them, so
	I crop these to 0. I used to just drop them to 255, but this lead to
	image corruption in dark areas. An afternoon of guesswork later, I tried
	cutting them to 0, and it works well enough for me.

	I could not have possibly figured this out without the help of these sites:
	http://www.butaman.ne.jp/~tsuruzoh/Computer/Digicams/exif-e.html
	http://www.efg2.com/lab/Graphics/Colors/YUV.htm#411
	http://www.dcs.ed.ac.uk/home/mxr/gfx/2d-hi.html
*/
unsigned char *dimagev_ycbcr_to_ppm(unsigned char *ycbcr) {
	unsigned char *rgb_data, *ycrcb_current, *rgb_current;
	int count=0;
	unsigned int magic_r, magic_g, magic_b;

	if ( ( rgb_data = malloc( 14413 ) ) == NULL ) {
		GP_DEBUG( "dimagev_ycbcr_to_ppm::unable to allocate buffer for Y:Cb:Cr conversion");
		return NULL;
	}

	ycrcb_current = ycbcr;
	rgb_current = &(rgb_data[13]);

	/* This is the header for a PPM "rawbits" bitmap of size 80x60. */
	strncpy((char *)rgb_data, "P6\n80 60\n255\n", 13);

	for ( count = 0 ; count < 9600 ; count+=4, ycrcb_current+=4, rgb_current+=6 ) {
		magic_b = ( ( ycrcb_current[2] > (unsigned char) 128 ? 128 : ycrcb_current[2] ) - 128 ) * ( 2 - ( 2 * CR_COEFF ) ) + ycrcb_current[0];
		rgb_current[2] = (unsigned char) ( magic_b > 255 ? 0 : magic_b );
		magic_r = ( ( ycrcb_current[3] > (unsigned char) 128 ? 128 : ycrcb_current[3] ) - 128 ) * ( 2 - ( 2 * Y_COEFF ) ) + ycrcb_current[0];
		rgb_current[0] = (unsigned char) ( magic_r > 255 ? 0 : magic_r );
		magic_g = (( ycrcb_current[0] - ( CR_COEFF * rgb_current[2] ) ) - ( Y_COEFF * rgb_current[0])) / CB_COEFF ;
		rgb_current[1] = (unsigned char) ( magic_g > 255 ? 0 : magic_g );

		/* Wipe everything clean. */
		magic_b = magic_r = magic_g = 0;

		magic_b = ( ( ycrcb_current[2] > (unsigned char) 128 ? 128 : ycrcb_current[2] ) - 128 ) * ( 2 - ( 2 * CR_COEFF ) ) + ycrcb_current[1];
		rgb_current[5] = (unsigned char) ( magic_b > 255 ? 0 : magic_b );
		magic_r = ( ( ycrcb_current[3] > (unsigned char) 128 ? 128 : ycrcb_current[3] ) - 128 ) * ( 2 - ( 2 * Y_COEFF ) ) + ycrcb_current[1];
		rgb_current[3] = (unsigned char) ( magic_r > 255 ? 0 : magic_r );
		magic_g = (( ycrcb_current[1] - ( CR_COEFF * rgb_current[5] ) ) - ( Y_COEFF * rgb_current[3])) / CB_COEFF ;
		rgb_current[4] = (unsigned char) ( magic_g > 255 ? 0 : magic_g );

		/* Wipe everything clean. */
		magic_b = magic_r = magic_g = 0;
	}

	return rgb_data;
}
