/* gamma.c
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gamma.h"

#include <math.h>

#include <gphoto2-result.h>

static int
gp_gamma_correct_triple (unsigned char *table_red,
			 unsigned char *table_green,
			 unsigned char *table_blue,
			 unsigned char *data, unsigned int size)
{
	int x;

	for (x = 0; x < (size * 3); x += 3) {
		data[x + 0] = table_red  [data[x + 0]];
		data[x + 1] = table_green[data[x + 1]];
		data[x + 2] = table_blue [data[x + 2]];
	}

	return (GP_OK);
}

int
gp_gamma_correct_single (unsigned char *table, unsigned char *data,
			 unsigned int size)
{
	return (gp_gamma_correct_triple (table, table, table, data, size));
}

int
gp_gamma_fill_table (unsigned char *table, double g)
{
	int x;

	for (x = 0; x < 256; x++)
		table[x] = 255 * pow ((double) x/255., g);

	return (GP_OK);
}
