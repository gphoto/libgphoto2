/****************************************************************************
 *
 * File: util.c 
 *
 * Utility functions for the gphoto2 camlibs/canon driver.
 *
 * $Id$
 ****************************************************************************/

/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#include <config.h>

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#include <gphoto2.h>
#include <gphoto2-port-log.h>

#include "canon.h"
#include "util.h"
#include "library.h"

/**
 * is_thumbnail:
 * @name: name of file to examine
 * Test whether the given @name corresponds to a thumbnail (.THM).
 **/
int
is_thumbnail (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos)
		res = (!strcmp (pos, ".THM"));

	GP_DEBUG ("is_thumbnail(%s) == %i", name, res);
	return (res);
}

/**
 * is_image:
 * @name: name of file to examine
 *
 * Test whether the given @name corresponds to an image (.JPG or .CRW).
 **/
int
is_image (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos) {
		res = (!strcmp (pos, ".JPG"));
		if (!res)
			res = (!strcmp (pos, ".CRW"));
	}

	GP_DEBUG ("is_image(%s) == %i", name, res);
	return (res);
}

/**
 * is_jpeg:
 * @name: name of file to examine
 *
 * Test whether the given @name corresponds to a JPEG image (.JPG).
 **/
int
is_jpeg (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos)
		res = (!strcmp (pos, ".JPG"));

	GP_DEBUG ("is_jpeg(%s) == %i", name, res);
	return (res);
}

/**
 * is_crw:
 * @name: name of file to examine
 *
 * Test whether the name given corresponds to a raw CRW image (.CRW).
 **/
int
is_crw (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos)
		res = (!strcmp (pos, ".CRW"));

	GP_DEBUG ("is_crw(%s) == %i", name, res);
	return (res);
}

/**
 * is_movie:
 * @name: name of file to examine
 *
 * Test whether the name given corresponds
 * to a movie (.AVI)
 */
int
is_movie (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos)
		res = (!strcmp (pos, ".AVI"));

	GP_DEBUG ("is_movie(%s) == %i", name, res);
	return (res);
}

/**
 * filename2mimetype:
 * @filename: name of file to examine
 *
 * Deduce MIME type of file by considering the file name ending.
 *
 * @Returns: pointer to immutable string with the MIME type
 **/

const char *filename2mimetype(const char *filename)
{
	const char *pos = strchr(filename, '.');
	if (pos) {
		if      (!strcmp (pos, ".AVI")) return GP_MIME_AVI;
		else if (!strcmp (pos, ".JPG")) return GP_MIME_JPEG;
		else if (!strcmp (pos, ".THM")) return GP_MIME_JPEG;
		else if (!strcmp (pos, ".CRW")) return GP_MIME_CRW;
	}
	return GP_MIME_UNKNOWN;
}


/****************************************************************************
 *
 * End of file: util.c
 *
 ****************************************************************************/

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
