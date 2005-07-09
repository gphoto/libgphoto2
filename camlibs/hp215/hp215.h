#ifndef _HP215_H
#define _HP215_H
/*****************************************************************************
 *  Copyright (C) 2002  Jason Surprise <thesurprises1@attbi.com>             *
 *                2003  Enno Bartels   <ennobartels@t-online.de>             *
 *                2005  Marcus Meissner <marcus@jet.franken.de>              *
 *****************************************************************************
 *                                                                           *
 *  This program is free software; you can redistribute it and/or            *
 *  modify it under the terms of the GNU Library General Public              *
 *  License as published by the Free Software Foundation; either             *
 *  version 2 of the License, or (at your option) any later version.         *
 *                                                                           *
 *  This program is distributed in the hope that it will be useful,          *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *  Library General Public License for more details.                         *
 *                                                                           *
 *  You should have received a copy of the GNU Library General Public        *
 *  License along with this program; see the file COPYING.LIB.  If not,      *
 *  write to the Free Software Foundation, Inc., 59 Temple Place -           *
 *  Suite 330,  Boston, MA 02111-1307, USA.                                  *
 *****************************************************************************/

#define STX 0x02
#define ETX 0x03
#define ACK 0x06

/* General commands */
#define HP_CMD_GET_PHOTO_ALBUM {0x02, 0xc6, 0x88, 0x80, 0x80, 0x83, 0x84, 0x38, 0x2f, 0x30, 0x32, 0x88, 0x84, 0x8e, 0x8b, 0x03}
#define HP_CMD_DELETE_PICS     {0x02, 0xb1, 0x84, 0x8f, 0x8f, 0x8f, 0x8f, 0x86, 0x80, 0x8a, 0x86, 0x03}

typedef struct 
{
  unsigned int hour;
  unsigned int min;
  unsigned int sec;

  unsigned int day;
  unsigned int month;
  unsigned int year;

} t_date;
#endif
