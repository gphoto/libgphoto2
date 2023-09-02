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

/*
	Defines some PrintMacros. They are need to disable much
	outputs they were used during development.
*/

#ifndef CAMLIBS_MUSTEK_PRINT_H
#define CAMLIBS_MUSTEK_PRINT_H

#include <stdio.h>

/* Message or Errors from the gphoto API */
#define printAPINote		if (1) printf
#define printAPIError		if (1) printf

/* only for debugging : The FunktionCall */
#define printFnkCall		if (0) printf

/* Message or Errors from the Core Layer */
#define printCoreNote 		if (1) printf
#define printCoreError		if (1) printf

/* CError are Error Messages below */
/* the Core of this driver */
#define printCError			if (1) printf


#endif /* !defined(CAMLIBS_MUSTEK_PRINT_H) */

