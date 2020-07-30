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
 *  write to the 							     *
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,         *
 *  Boston, MA  02110-1301  USA						     *
 *****************************************************************************/

#define STX 0x02
#define ETX 0x03
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15

#define HP215_OK	0xe0e0

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
