/*
 * Jenoptik JD350e Driver
 * Copyright (C) 2001 Michael Trawny <trawny99@yahoo.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   
 */

#ifndef LIBRARY_H
#define LIBRARY_H

int  jd350e_ping              (GPPort *);
int  jd350e_file_count        (GPPort *, int *count);
int  jd350e_get_image_raw     (GPPort *, int image_no, char **data, int *size);
int  jd350e_get_image_full    (GPPort *, int image_no, char **data, int *size);
int  jd350e_get_image_preview (GPPort *, int image_no, char **data, int *size);

#endif
