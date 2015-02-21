/*
 * Copyright 1999/2000 by Henning Zabel <henning@uni-paderborn.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef _RS232_H
#define _RS232_H

int mdc800_rs232_sendCommand(GPPort*,unsigned char* , unsigned char * , int );
int mdc800_rs232_waitForCommit(GPPort*,char commandid);
int mdc800_rs232_receive(GPPort*,unsigned char * , int );
int mdc800_rs232_download(GPPort*,unsigned char *, int);

#endif
