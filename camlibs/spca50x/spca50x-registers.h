/****************************************************************/
/* spca50x-registers.h - Gphoto2 library for cameras with a     */
/*                       Sunplus spca50x chip                   */
/*                                                              */
/* Copyright 2002, 2003 Till Adam                               */
/*                                                              */
/* Author: Till Adam <till@adam-lilienthal.de>                  */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/

#ifndef __SPCA50X_REGISTERS_H__
#define __SPCA50X_REGISTERS_H__


/* ---- Register ---- */
#define SPCA50X_REG_CamMode		0x2000
/* ---- CamMode Value ---- */
typedef enum
{
	SPCA50X_CamMode_Idle = 0,
	SPCA50X_CamMode_DSC = 1,
	SPCA50X_CamMode_VideoClip = 2,
	SPCA50X_CamMode_PcCamera = 3,
	SPCA50X_CamMode_Upload = 4
}
SPCA50xCamMode;

#define SPCA50X_REG_PbSrc		0x2301
/* ---- PbSrc Value ---- */
#define SPCA50X_DramUsb	                0x13

#define SPCA50X_REG_AutoPbSize		0x2306

#define SPCA50X_REG_DramType		0x2705

#define SPCA50X_REG_ImageType		0x270c

#define SPCA50X_REG_SdramSizeL		0x2710	/* Size [7:0] */
#define SPCA50X_REG_SdramSizeM		0x2711	/* Size [15:8] */
#define SPCA50X_REG_SdramSizeH		0x2712	/* Size [21:16] */

#define SPCA50X_REG_VlcAddressL		0x2713
#define SPCA50X_REG_VlcAddressM		0x2714
#define SPCA50X_REG_VlcAddressH		0x2715

#define SPCA50X_REG_MemWidthL		0x2720
#define SPCA50X_REG_MemWidthH		0x2721

#define SPCA50X_REG_MemHeightL		0x2722
#define SPCA50X_REG_MemHeightH		0x2723

#define SPCA50X_REG_Trigger		0x27a1
/*  ---- Trigger bit Value ---- */
#define SPCA50X_TrigDramFifo	2

#define SPCA50X_REG_JFIF			0x2884

#define SPCA50X_REG_CompSizeL		0x2887
#define SPCA50X_REG_CompSizeM		0x2888
#define SPCA50X_REG_CompSizeH		0x2889

#define SPCA50X_REG_I2C_wIndex		0x2a08
#define SPCA50X_REG_I2C_wValue		0x2a10
#define SPCA50X_REG_I2C_rValue		0x2a30
#define SPCA50X_REG_I2C_rStatus		0x2a06


#endif /* __SPCA50X_REGISTERS_H__ */
