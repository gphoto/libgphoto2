#include <stdio.h>
#include <stdlib.h>

#include <gphoto2.h>

#include "interface.h"

extern int glob_stdout;
extern int glob_quiet;

void
status_func (Camera *camera, const char *status, void *data)
{
	if (glob_quiet)
		return;

	if (status && *status) {
		fprintf (stderr, "Status: %s\n", status);
		fflush (stderr);
	} else {

		/*
		 * gphoto2 tells us to clear the status. But it seems
		 * people really want '\n' above (not '\r'), therefore
		 * we can't clear the status. Hence, we don't do anything
		 * here.
		 *
		 * If we would use '\r' above, we could overwrite an existing
		 * status message by writing an empty line. But we don't, as 
		 * when you're using the command line, stdout and stderr usually
		 * get mixed up on the same terminal. And overwriting the last
		 * program message with status message isn't really clever.
		 */
	}
}

void
message_func (Camera *camera, const char *message, void *data)
{
	if (glob_quiet)
		return;

	printf ("Message: %s\n", message);
}

void
progress_func (Camera *camera, float percentage, void *data)
{
	if (glob_quiet)
		return;

	if (percentage > 0.) {
		printf ("Percent completed: %02.01f\r", percentage * 100.);
		fflush(stdout);
	}
}
