/* gphoto2-result.c
 *
 * Copyright (C) 2000 Scott Fritzinger
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

#include "gphoto2-result.h"

#include <config.h>
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

static struct {
	int result;
	const char *description;
} result_descriptions[] = {
	{GP_ERROR_CORRUPTED_DATA,      N_("Corrupted data")},
	{GP_ERROR_FILE_EXISTS,         N_("File exists")},
	{GP_ERROR_MODEL_NOT_FOUND,     N_("Unknown model")},
	{GP_ERROR_DIRECTORY_NOT_FOUND, N_("Directory not found")},
	{GP_ERROR_FILE_NOT_FOUND,      N_("File not found")},
	{GP_ERROR_DIRECTORY_EXISTS,    N_("Directory exists")},
	{GP_ERROR_PATH_NOT_ABSOLUTE,   N_("Path not absolute")},
	{GP_ERROR_CANCEL,              N_("Operation cancelled")},
	{0, NULL}
};

/**
 * gp_result_as_string:
 * @result: a gphoto2 error code
 *
 * Translates a gphoto2 error code into a human readable string. If the 
 * error occurred in combination with a camera,
 * #gp_camera_get_result_as_string should be used instead.
 *
 * Return value: A string representation of a gphoto2 error code
 **/
const char *
gp_result_as_string (int result)
{
	unsigned int i;

	/* IOlib error? Pass through. */
	if ((result <= 0) && (result >= -99))
		return (gp_port_result_as_string (result));

	/* Camlib error? */
	if (result <= -1000)
		return (N_("Unknown camera library error")); 

	for (i = 0; result_descriptions[i].description; i++)
		if (result_descriptions[i].result == result)
			return (result_descriptions[i].description);

	return (N_("Unknown error"));
}
