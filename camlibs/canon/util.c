/****************************************************************************
 *
 * File: util.c 
 *
 * Utility functions for the gPhoto Canon PowerShot A5(Z)/A50 driver.
 *
 * $Header$
 ****************************************************************************/

/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

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
 * intatpos:
 * @block: byte block we are to write the integer into
 * @pos: position in byte block where we are to write to
 * @integer: the int number we are to write into the block
 *
 * Utility function used by canon_usb_dialogue
 * FIXME: What about endianness?
 **/
void
intatpos (unsigned char *block, int pos, int integer)
{
	*(unsigned *) (block + pos) = integer;
}


unsigned int
get_int (const unsigned char *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

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

/*
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

	gp_debug_printf (GP_DEBUG_LOW, "canon", "is_movie(%s) == %i", name, res);
	return (res);
}


int
comp_dir (const void *a, const void *b)
{
	gp_debug_printf (GP_DEBUG_LOW, "canon", "comp_dir()");

	return strcmp (((const struct canon_dir *) a)->name,
		       ((const struct canon_dir *) b)->name);
}


/****************************************************************************
 *
 * End of file: util.c
 *
 ****************************************************************************/
