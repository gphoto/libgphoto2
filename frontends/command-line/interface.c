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

	if (*status) {
		fprintf (stderr, "Status: %s\n", status);
		fflush(stdout);
	} else {
		/*
		 * This is still not very clever - 
		 * if you know any better, step forward.
		 */
		fprintf (stderr, "Status: OK (i.e. \"\")\n");
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
