/* command.h
 *
 * Copyright (C) M. Adam Kendall <joker@penguinpub.com>
 * Copyright (C) 2002 Bart van Leeuwen <bart@netage.nl>
 *
 * Based on the chotplay CLI interface from Ken-ichi Hayashi 1996,1997
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

#ifndef __COMMAND_H__
#define __COMMAND_H__

int	F1ok (GPPort*);
long	F1getdata (GPPort*,char *, unsigned char *);
int	F1status (GPPort*);
char	F1newstatus (GPPort *port, int, char *);
int	F1howmany (GPPort *);
int	F1fopen (GPPort *,char *);
long	F1fread(GPPort *,unsigned char *data, long len);
long	F1fwrite(GPPort *,unsigned char *data, long len, unsigned char b);
long	F1fseek (GPPort *,long, int);
unsigned long	F1finfo (GPPort *,char *);
int	F1fclose (GPPort *);
int	F1deletepicture (GPPort*,int);
#endif /* __COMMAND_H__ */
