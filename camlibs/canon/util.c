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

#include "config.h"

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
 *
 * Test whether the given @name corresponds to a thumbnail (.THM).
 *
 * Returns:
 *   1 if filename is the name of a thumbnail (i.e. ends with ".THM" )
 *   0 if not
 *
 */
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
 * is_audio:
 * @name: name of file to examine
 *
 * Test whether the given @name corresponds to an audio file (.WAV).
 *
 * Returns:
 *   1 if filename is the name of an audio file (i.e. ends with ".WAV" )
 *   0 if not
 *
 */
int
is_audio (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos)
		res = (!strcmp (pos, ".WAV"));

	GP_DEBUG ("is_audio(%s) == %i", name, res);
	return (res);
}

/**
 * is_image:
 * @name: name of file to examine
 *
 * Tests whether the given @name corresponds to an image (.JPG, .CR2, or .CRW).
 *
 * Returns:
 *   1 if filename is the name of an image
 *   0 if not
 *
 */
int
is_image (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos) {
		res |= !strcmp (pos, ".JPG");
		res |= !strcmp (pos, ".CRW");
		res |= !strcmp (pos, ".CR2");
	}

	GP_DEBUG ("is_image(%s) == %i", name, res);
	return (res);
}

/**
 * is_jpeg:
 * @name: name of file to examine
 *
 * Test whether the given @name corresponds to a JPEG image (.JPG).
 * Returns:
 *   1 if filename is the name of a JPEG image (i.e. ends with ".JPG" )
 *   0 if not
 *
 */
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
 * Tests whether the name given corresponds to a raw CRW image (.CRW or .CR2).
 *
 * Returns:
 *   1 if filename is the name of a raw image (i.e. ends with .CRW or .CR2)
 *   0 if not
 *
 */
int
is_crw (const char *name)
{
	const char *pos;
	int res = 0;

	pos = strchr (name, '.');
	if (pos)
		res = (!strcmp (pos, ".CRW")) || (!strcmp ( pos, ".CR2" ) );

	GP_DEBUG ("is_crw(%s) == %i", name, res);
	return (res);
}

/**
 * is_movie:
 * @name: name of file to examine
 *
 * Test whether the name given corresponds
 * to a movie (.AVI)
 *
 * Returns:
 *   1 if filename is the name of a movie (i.e. ends with ".AVI" )
 *   0 if not
 *
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
 * @filename: name of file to examine
 *
 * Deduces MIME type of file by considering the file name ending.
 *
 * Returns: pointer to immutable string with the MIME type
 */

const char *
filename2mimetype (const char *filename)
{
	const char *pos = strchr (filename, '.');

	if (pos) {
		if (!strcmp (pos, ".AVI"))
			return GP_MIME_AVI;
		else if (!strcmp (pos, ".JPG"))
			return GP_MIME_JPEG;
		else if (!strcmp (pos, ".WAV"))
			return GP_MIME_WAV;
		else if (!strcmp (pos, ".THM"))
			return GP_MIME_JPEG;
		else if (!strcmp (pos, ".CRW"))
			return GP_MIME_CRW;
		else if (!strcmp (pos, ".CR2"))
			return GP_MIME_CRW;
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
