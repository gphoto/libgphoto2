/****************************************************************/
/* gsmart-registers.h - Gphoto2 library for the Mustek gSmart   */
/*                      mini 2                                  */
/*                                                              */
/* Copyright (C) 2002 Till Adam                                 */
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
/* Free Software Foundation, Inc., 59 Temple Place - Suite 330, */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#ifndef __GSMART_REGISTERS_H__
#define __GSMART_REGISTERS_H__


/* ---- Register ----  */
#define GSMART_REG_CamMode		0x2000
/* ---- CamMode Value ----  */
typedef enum
{
	GSMART_CamMode_Idle = 0,
	GSMART_CamMode_DSC = 1,
	GSMART_CamMode_VideoClip = 2,
	GSMART_CamMode_PcCamera = 3,
	GSMART_CamMode_Upload = 4
}
GsmartCamMode;

#define GSMART_REG_PbSrc		0x2301
/* ---- PbSrc Value ----  */
#define GSMART_DramUsb	                0x13

#define GSMART_REG_AutoPbSize		0x2306

#define GSMART_REG_DramType		0x2705

#define GSMART_REG_ImageType		0x270c

#define GSMART_REG_SdramSizeL		0x2710	/* Size [7:0]  */
#define GSMART_REG_SdramSizeM		0x2711	/* Size [15:8] */
#define GSMART_REG_SdramSizeH		0x2712	/* Size [21:16] */

#define GSMART_REG_VlcAddressL		0x2713
#define GSMART_REG_VlcAddressM		0x2714
#define GSMART_REG_VlcAddressH		0x2715

#define GSMART_REG_MemWidthL		0x2720
#define GSMART_REG_MemWidthH		0x2721

#define GSMART_REG_MemHeightL		0x2722
#define GSMART_REG_MemHeightH		0x2723

#define GSMART_REG_Trigger		0x27a1
/*  ---- Trigger bit Value ----  */
#define GSMART_TrigDramFifo	2

#define GSMART_REG_JFIF			0x2884

#define GSMART_REG_CompSizeL		0x2887
#define GSMART_REG_CompSizeM		0x2888
#define GSMART_REG_CompSizeH		0x2889

#define GSMART_REG_I2C_wIndex		0x2a08
#define GSMART_REG_I2C_wValue		0x2a10
#define GSMART_REG_I2C_rValue		0x2a30
#define GSMART_REG_I2C_rStatus		0x2a06


#endif /* __GSMART_REGISTERS_H__ */
