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

static char *result_string[] = {
	/* GP_ERROR_BAD_PARAMETERS      -100 */ N_("Bad parameters"),
	/* GP_ERROR_IO                  -101 */ N_("I/O problem"),
	/* GP_ERROR_CORRUPTED_DATA      -102 */ N_("Corrupted data"),
	/* GP_ERROR_FILE_EXISTS         -103 */ N_("File exists"),
	/* GP_ERROR_NO_MEMORY           -104 */ N_("Insufficient memory"),
	/* GP_ERROR_MODEL_NOT_FOUND     -105 */ N_("Unknown model"),
	/* GP_ERROR_NOT_SUPPORTED       -106 */ N_("Unsupported operation"),
	/* GP_ERROR_DIRECTORY_NOT_FOUND -107 */ N_("Directory not found"),
	/* GP_ERROR_FILE_NOT_FOUND      -108 */ N_("File not found"),
	/* GP_ERROR_DIRECTORY_EXISTS    -109 */ N_("Directory exists"),
	/* GP_ERROR_NO_CAMERA_FOUND     -110 */ N_("No cameras were detected"),
	/* GP_ERROR_PATH_NOT_ABSOLUTE   -111 */ N_("Path not absolute")
};

const char *
gp_result_as_string (int result)
{
	/* Really an error? */
	if (result > 0)
		return _("Unknown error");

	/* IOlib error? Pass through. */
	if ((result <= 0) && (result >= -99))
		return gp_port_result_as_string (result);

	/* Camlib error? You should have called gp_camera_result_as_string... */
	if (result <= -1000)
		return _("Unknown camera library error"); 
	
	/* Do we have an error description? */
	if ((-result - 100) < (int) (sizeof (result_string) /
						sizeof (*result_string)))
		return _(result_string [-result - 100]);
	
	return _("Unknown error");
}
