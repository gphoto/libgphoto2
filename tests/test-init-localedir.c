/** \file tests/test-init-localedir.c
 * \brief Exercise the *_init_localedir() functions
 *
 * Copyright 2022 Hans Ulrich Niedermann <hun@n-dimensional.de>
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
 *
 *
 * Usage:
 *   ./test-init-localedir /foo/share/locale
 *   ./test-init-localedir /foo/share/locale NULL
 *   ./test-init-localedir NULL /foo/share/locale /usr/share/locale
 *
 * Calls the gp_init_localedir() for each and every command line
 * argument in sequence. Command line arguments are interpreted as
 * directory paths (absolute or relative to the current working
 * directory), but "NULL" is interpreted as a C NULL value.
 */

#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-abilities-list.h>


void log_func(GPLogLevel level, const char *domain,
	      const char *str, void *data)
{
	printf("%d:%s:%s\n", level, domain, str);
}


int main(const int argc, const char *const argv[])
{
	gp_log_add_func(GP_LOG_ALL, log_func, NULL);
	for (int i=1; i<argc; ++i) {
		const char *const arg = argv[i];
		const char *const localedir =
			(strcmp(arg, "NULL") == 0) ? NULL : argv[1];
		printf("main: calling gp_init_localedir(%s)\n",
		       localedir?localedir:"(null)");
		gp_init_localedir(localedir);
	}
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
