/* range.c
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#include <config.h>
#include "range.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

/* 
  str_grab_nat() - Grab positive decimal integer (natural number) from str 
  	starting with pos byte. On return pos points to the first byte after 
	grabbed integer.
*/

static int
str_grab_nat(const char *str, int *pos)
{
	int	result, old_pos = *pos;
	char 	*buf;

	if (!(buf = (char*)strdup(str))) /* we can not modify str */
		return -1;
	
	*pos += strspn(&buf[*pos], "0123456789");
	buf[*pos] = 0;
	result = atoi(&buf[old_pos]);
	free(buf);
	return result;
}

/*
  parse_range() - Intentionally, parse list of images given in range. Syntax
  	of range is:
		( m | m-n ) { , ( m | m-n ) }
	where m,n are decimal integers with 1 <= m <= n <= MAX_IMAGE_NUMBER.
	Ranges are XOR (exclusive or), so that 
		1-5,3,7
	is equivalent to
		1,2,4,5,7	
	Conversion from 1-based to 0-based numbering is performed, that is, 
	n-th image corresponds to (n-1)-th byte in index. The value of 
	index[n] is either 0 or 1. 0 means not selected, 1 selected. 
	index passed to parse_range() must be an array of MAX_IMAGE_NUMBER 
	char-s set to 0 (use memset() or bzero()).
*/
		
static int
parse_range_rec (const char *range, unsigned int start, char *index,
		 GPContext *context)
{
	int	i = 0, l = -1, r = -1, j;
	char buf[1024];

	do {
		strncpy (buf, range, sizeof (buf) - 1);
		buf[sizeof (buf) - 1] = 0;
		strncat (buf, "\n", sizeof (buf) - strlen (buf) - 1);
		buf[sizeof (buf) - 1] = 0;
		
		switch (range[start + i]) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (j = 0; j < start + i; j++) {
				strncat (buf, " ", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
			}
			l = str_grab_nat (range + start, &i);
			for (; j < start + i; j++) {
				strncat (buf, "^", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
			}
			
			if (l <= 0) {
				gp_context_error (context, _("%s\n"
					"Image IDs must be a number greater "
					"than zero."), buf);
				return (GP_ERROR_BAD_PARAMETERS);
			}
			if (l > MAX_IMAGE_NUMBER) {
				gp_context_error (context, _("%s\n"
					"Image ID %i too high."), buf, l);
				return (GP_ERROR_BAD_PARAMETERS);
			}
			break;
		
		case '-' :

			/* If we already have r, then something is wrong. */
			if (r > 0) {
				for (j = 0; j < start + i; j++) {
					strncat (buf, " ", sizeof (buf) - strlen (buf) - 1);
					buf[sizeof (buf) - 1] = 0;
				}
				strncat (buf, "^", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
				gp_context_error (context, _("%s\n"
					"Ranges must be separated by ','."),
					buf);
				return (GP_ERROR_BAD_PARAMETERS);
			}

			/* If we have not l, then something is wrong, too. */
			if (l <= 0) {
				for (j = 0; j < start + i; j++) {
					strncat (buf, " ", sizeof (buf) - strlen (buf) - 1);
					buf[sizeof (buf) - 1] = 0;
				}
				strncat (buf, "^", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
				gp_context_error (context, _("%s\n"
					"Ranges need to start with "
					"a number."), buf);
				return (GP_ERROR_BAD_PARAMETERS);
			}

			i++;
			for (j = 0; j < start + i; j++) {
				strncat (buf, " ", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
			}
			r = str_grab_nat (range + start, &i);
			for (; j < start + i; j++) {
				strncat (buf, "^", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
			}
			buf[sizeof (buf) - 1] = 0;
			if (r <= 0) {
				gp_context_error (context, _("%s\n"
					"Image IDs must be a number greater "
					"than zero."), buf);
				return (GP_ERROR_BAD_PARAMETERS);
			}
			if (r > MAX_IMAGE_NUMBER) {
				gp_context_error (context, _("%s\n"
					"Image ID %i too high."), buf, r);
				return (GP_ERROR_BAD_PARAMETERS);
			}
			break;
			
		case '\0' :
			break; 
			
		default :
			for (j = 0; j < start + i; j++) {
				strncat (buf, " ", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
			}
			strncat (buf, "^", sizeof (buf) - strlen (buf) - 1);
			buf[sizeof (buf) - 1] = 0;
			gp_context_error (context, _("%s\n"
				"Unexpected "
				"character '%c'."), buf, range[start + i]);
			return (GP_ERROR_BAD_PARAMETERS);
		}
	} while ((range[start + i] != '\0') &&
		 (range[start + i] != ','));

	if (i) {
		if (0 < r) { /* update range of bytes */ 
			if (r < l) {
				strncpy (buf, range, sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
				strncat (buf, "\n", sizeof (buf) - strlen (buf) - 1);
				buf[sizeof (buf) - 1] = 0;
				for (j = 0; j < start; j++) {
					strncat (buf, " ", sizeof (buf) - strlen (buf) - 1);
					buf[sizeof (buf) - 1] = 0;
				}
				for (; j < start + i; j++) {
					strncat (buf, "^", sizeof (buf) - strlen (buf) - 1);
					buf[sizeof (buf) - 1] = 0;
				}
				gp_context_error (context, _("%s\n"
					"Decreasing ranges "
					"are not allowed. You specified a "
					"range from %i to %i."), buf, l, r);
				return (GP_ERROR_BAD_PARAMETERS);
			}

			for (j = l; j <= r; j++)
				index[j - 1] ^= 1; /* convert to 0-based numbering */
		} else  /* update single byte */
			index[l - 1] ^= 1; /* convert to 0-based numbering */
	}

	/*
	 * If there is another range, parse it.
	 */
	if (range[start + i] == ',') 
		return parse_range_rec (range, start + i + 1, index, context);
	
	return (GP_OK);
}

int
parse_range (const char *range, char *index, GPContext *context)
{
	return (parse_range_rec (range, 0, index, context));
}
