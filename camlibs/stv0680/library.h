/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright (C) 2000 Adam Harrison <adam@antispin.org> 
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

int stv0680_ping(struct stv0680_s *device);
int stv0680_file_count(struct stv0680_s *device, int *count);
int stv0680_get_image(struct stv0680_s *device, int image_no, char **data,
		      int *size);
int stv0680_get_image_preview(struct stv0680_s *device, int image_no,
				char **data, int *size);

#endif
