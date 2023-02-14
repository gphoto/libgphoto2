/** \file
 *
 * \author Copyright 2000 Scott Fritzinger
 *
 * \note
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \note
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \note
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 * \note
 * This file contains internal functions. Use of these functions from
 * external software modules is considered <strong>deprecated</strong>.
 */
#define _POSIX_SOURCE
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE

#include "config.h"
#include <gphoto2/gphoto2-file.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-portability.h>

#include <gphoto2/gphoto2-result.h>

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

/* lengt of one path component */
#ifndef MAX_PATH
# define MAX_PATH 256
#endif

/*! The internals of the CameraFile struct are private.
 * \internal
 */
struct _CameraFile {
        char		mime_type [64];
        char		name [MAX_PATH];
        int		ref_count;
        time_t		mtime;

	CameraFileAccessType	accesstype;

	/* for GP_FILE_ACCESSTYPE_MEMORY files */
        unsigned long	size;
        unsigned char	*data;
        unsigned long	offset;	/* read pointer */

	/* for GP_FILE_ACCESSTYPE_FD files */
	int		fd;

	/* for GP_FILE_ACCESSTYPE_HANDLER files */
	CameraFileHandler*handler;
	void		*private;
};


/*! Create new #CameraFile object.
 *
 * \param file a pointer to a #CameraFile
 * \return a gphoto2 error code.
 */
int
gp_file_new (CameraFile **file)
{
	C_PARAMS (file);

	C_MEM (*file = calloc (1, sizeof (CameraFile)));

	strcpy ((*file)->mime_type, "unknown/unknown");
	(*file)->ref_count = 1;
	(*file)->accesstype = GP_FILE_ACCESSTYPE_MEMORY;
	return (GP_OK);
}


/*! Create new #CameraFile object from a UNIX filedescriptor.
 *
 * This function takes ownership of the filedescriptor and will close it when closing the CameraFile.
 *
 * \param file a pointer to a #CameraFile
 * \param fd a UNIX filedescriptor
 * \return a gphoto2 error code.
 */
int
gp_file_new_from_fd (CameraFile **file, int fd)
{
	C_PARAMS (file);

	C_MEM (*file = calloc (1, sizeof (CameraFile)));

	strcpy ((*file)->mime_type, "unknown/unknown");
	(*file)->ref_count = 1;
	(*file)->accesstype = GP_FILE_ACCESSTYPE_FD;
	(*file)->fd = fd;
	return (GP_OK);
}

/*! Create new #CameraFile object using a programmatic handler.
 *
 * \param file a pointer to a #CameraFile
 * \param handler a #CameraFileHandler
 * \param private a private pointer for frontend use
 * \return a gphoto2 error code.
 */
int
gp_file_new_from_handler (CameraFile **file, CameraFileHandler* handler, void*private)
{
	C_PARAMS (file);

	C_MEM (*file = calloc (1, sizeof (CameraFile)));

	strcpy ((*file)->mime_type, "unknown/unknown");
	(*file)->ref_count = 1;
	(*file)->accesstype = GP_FILE_ACCESSTYPE_HANDLER;
	(*file)->handler = handler;
	(*file)->private = private;
	return (GP_OK);
}



/*! \brief descruct a #CameraFile object.
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int gp_file_free (CameraFile *file)
{
	C_PARAMS (file);

	CHECK_RESULT (gp_file_clean (file));

	if (file->accesstype == GP_FILE_ACCESSTYPE_FD)
		close (file->fd);

	free (file);
	return (GP_OK);
}


/*! \brief Increase reference counter for #CameraFile object
 *
 * \param file a #CameraFile
 * \return a gphoto2 error code.
 */
int
gp_file_ref (CameraFile *file)
{
	C_PARAMS (file);

	file->ref_count += 1;

	return (GP_OK);
}


/*! \brief Decrease reference counter for #CameraFile object
 *
 * \param file a #CameraFile
 * \return a gphoto2 error code.
 *
 **/
int
gp_file_unref (CameraFile *file)
{
	C_PARAMS (file);

	file->ref_count -= 1;

	if (file->ref_count == 0)
		CHECK_RESULT (gp_file_free (file));

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param data
 * @param size
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_append (CameraFile *file, const char *data,
		unsigned long int size)
{
	C_PARAMS (file);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		C_MEM (file->data = realloc (file->data, sizeof (char) * (file->size + size)));
		memcpy (&file->data[file->size], data, size);
		file->size += size;
		break;
	case GP_FILE_ACCESSTYPE_FD: {
		unsigned long int curwritten = 0;
		while (curwritten < size) {
			ssize_t	res = write (file->fd, data+curwritten, size-curwritten);
			if (res == -1) {
				GP_LOG_E ("Encountered error %d writing to fd.", errno);
				return GP_ERROR_IO_WRITE;
			}
			if (!res) { /* no progress is bad too */
				GP_LOG_E ("Encountered 0 bytes written to fd.");
				return GP_ERROR_IO_WRITE;
			}
			curwritten += res;
		}
		break;
	}
	case GP_FILE_ACCESSTYPE_HANDLER: {
		uint64_t	xsize = size;
		/* FIXME: assume we write one blob */
		C_PARAMS (file->handler->write);
		return file->handler->write (file->private, (unsigned char*)data, &xsize);
	}
	default:
		GP_LOG_E ("Unknown file access type %d", file->accesstype);
		return GP_ERROR;
	}
        return (GP_OK);
}

/**
 * @param file a #CameraFile
 * @param data
 * @param size
 * @return a gphoto2 error code.
 *
 * Internal.
 **/
int
gp_file_slurp (CameraFile *file, char *data,
	size_t size, size_t *readlen
) {
	C_PARAMS (file);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		if (size > file->size-file->offset)
			size = file->size - file->offset;
		memcpy (data, &file->data[file->offset], size);
		file->offset += size;
		if (readlen) *readlen = size;
		break;
	case GP_FILE_ACCESSTYPE_FD: {
		unsigned long int curread = 0;
		while (curread < size) {
			ssize_t	res = read (file->fd, data+curread, size-curread);
			if (res == -1) {
				GP_LOG_E ("Encountered error %d reading from fd.", errno);
				return GP_ERROR_IO_READ;
			}
			if (!res) { /* no progress is bad too */
				GP_LOG_E ("Encountered 0 bytes reading from fd.");
				return GP_ERROR_IO_READ;
			}
			curread += res;
			if (readlen)
				*readlen = curread;
		}
		break;
	}
	case GP_FILE_ACCESSTYPE_HANDLER: {
		uint64_t	xsize = size;
		int		ret;

		C_PARAMS (file->handler->read);
		ret = file->handler->read (file->private, (unsigned char*)data, &xsize);
		*readlen = xsize;
		if (ret != GP_OK)
			GP_LOG_E ("File handler read returned %d", ret);
		return ret;
	}
	default:
		GP_LOG_E ("Unknown file access type %d", file->accesstype);
		return GP_ERROR;
	}
        return (GP_OK);
}



/**
 * @param file a #CameraFile
 * @param data
 * @param size
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_data_and_size (CameraFile *file, char *data,
			   unsigned long int size)
{
	C_PARAMS (file);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		free (file->data);
		file->data = (unsigned char*)data;
		file->size = size;
		break;
	case GP_FILE_ACCESSTYPE_FD: {
		unsigned int curwritten = 0;

		/* truncate */
		if (-1 == lseek (file->fd, 0, SEEK_SET)) {
			GP_LOG_E ("Encountered error %d lseeking to 0.", errno);
			/* might happen on pipes ... just ignore it */
		}
		if (-1 == ftruncate (file->fd, 0)) {
			GP_LOG_E ("Encountered error %d ftruncating to 0.", errno);
			/* might happen on pipes ... just ignore it */
		}
		while (curwritten < size) {
			ssize_t	res = write (file->fd, data+curwritten, size-curwritten);
			if (res == -1) {
				GP_LOG_E ("Encountered error %d writing to fd.", errno);
				return GP_ERROR_IO_WRITE;
			}
			if (!res) { /* no progress is bad too */
				GP_LOG_E ("Encountered 0 bytes written to fd.");
				return GP_ERROR_IO_WRITE;
			}
			curwritten += res;
		}
		/* This function takes over the responsibility for "data", aka
		 * it has to free it. So we do.
		 */
		free (data);
		break;
	}
	case GP_FILE_ACCESSTYPE_HANDLER: {
		uint64_t	xsize = size;
		int		ret;

		C_PARAMS (file->handler->write);
		/* FIXME: handle multiple blob writes */
		ret = file->handler->write (file->private, (unsigned char*)data, &xsize);
		if (ret != GP_OK) {
			GP_LOG_E ("Handler data() returned %d", ret);
			return ret;
		}
		/* This function takes over the responsibility for "data", aka
		 * it has to free it. So we do.
		 */
		free (data);
		return GP_OK;
	}
	default:
		GP_LOG_E ("Unknown file access type %d", file->accesstype);
		return GP_ERROR;
	}
	return (GP_OK);
}


/**
 * Get a pointer to the data and the file's size.
 *
 * @param file a #CameraFile
 * @param data
 * @param size
 * @return a gphoto2 error code.
 *
 * Both data and size can be NULL and will then be ignored.
 *
 * For regular CameraFiles, the pointer to data that is returned is
 * still owned by libgphoto2 and its lifetime is the same as the #file.
 *
 * For filedescriptor or handler based CameraFile types, the returned
 * data pointer is owned by the caller and needs to be free()d to avoid
 * memory leaks.
 *
 **/
int
gp_file_get_data_and_size (CameraFile *file, const char **data,
			   unsigned long int *size)
{
	C_PARAMS (file);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		if (data)
			*data = (char*)file->data;
		if (size)
			*size = file->size;
		break;
	case GP_FILE_ACCESSTYPE_FD: {
		off_t	offset;
		off_t	curread = 0;

		if (-1 == lseek (file->fd, 0, SEEK_END)) {
			if (errno == EBADF) return GP_ERROR_IO;
			/* Might happen for pipes or sockets. Umm. Hard. */
			/* FIXME */
		}
		if (-1 == (offset = lseek (file->fd, 0, SEEK_CUR))) {
			/* should not happen if we passed the above case */
			GP_LOG_E ("Encountered error %d lseekin to CUR.", errno);
			return GP_ERROR_IO_READ;
		}
		if (-1 == lseek (file->fd, 0, SEEK_SET)) {
			/* should not happen if we passed the above cases */
			GP_LOG_E ("Encountered error %d lseekin to CUR.", errno);
			return GP_ERROR_IO_READ;
		}
		if (size) *size = offset;
		if (!data) /* just the size... */
			return GP_OK;
		C_MEM (*data = malloc (offset));
		while (curread < offset) {
			ssize_t res = read (file->fd, (char*)((*data)+curread), offset-curread);
			if (res == -1) {
				free ((char*)*data);
				GP_LOG_E ("Encountered error %d reading.", errno);
				return GP_ERROR_IO_READ;
			}
			if (res == 0) {
				free ((char*)*data);
				GP_LOG_E ("No progress during reading.");
				return GP_ERROR_IO_READ;
			}
			curread += res;
		}
		break;
	}
	case GP_FILE_ACCESSTYPE_HANDLER: {
		uint64_t	xsize = 0;
		int		ret;

		C_PARAMS (file->handler->read);
		ret = file->handler->size (file->private, &xsize);
		if (ret != GP_OK) {
			GP_LOG_E ("Encountered error %d querying size().", ret);
			return ret;
		}
		if (size) *size = xsize;
		if (!data) /* just the size... */
			return GP_OK;
		C_MEM (*data = malloc (xsize));
		ret = file->handler->read (file->private, (unsigned char*)*data, &xsize);
		if (ret != GP_OK) {
			GP_LOG_E ("Encountered error %d getting data().", ret);
			free ((char*)*data);
			*data = NULL;
		}
		return ret;
	}
	default:
		GP_LOG_E ("Unknown file access type %d", file->accesstype);
		return GP_ERROR;
	}
	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param filename
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_save (CameraFile *file, const char *filename)
{
	FILE *fp;
	struct utimbuf u;

	C_PARAMS (file && filename);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		if (!(fp = fopen (filename, "wb")))
			return GP_ERROR;
		if (fwrite (file->data, (size_t)sizeof(char), (size_t)file->size, fp) != (size_t)file->size) {
			GP_LOG_E ("Not enough space on device in order to save '%s'.", filename);
			fclose (fp);
			unlink (filename);
			return GP_ERROR;
		}
		fclose (fp);
		break;
	case GP_FILE_ACCESSTYPE_FD: {
		char *data;
		off_t	curread = 0;
		off_t	offset;

		if (-1 == lseek (file->fd, 0, SEEK_END))
			return GP_ERROR_IO;
		if (-1 == (offset = lseek (file->fd, 0, SEEK_CUR))) {
			/* should not happen if we passed the above case */
			GP_LOG_E ("Encountered error %d lseekin to CUR.", errno);
			return GP_ERROR_IO_READ;
		}
		if (-1 == lseek (file->fd, 0, SEEK_SET)) {
			/* should not happen if we passed the above case */
			GP_LOG_E ("Encountered error %d lseekin to BEGIN.", errno);
			return GP_ERROR_IO_READ;
		}
		C_MEM (data = malloc(65536));
		if (!(fp = fopen (filename, "wb"))) {
			free (data);
			return GP_ERROR;
		}
		while (curread < offset) {
			int toread, res;

			toread = 65536;
			if (toread > (offset-curread))
				toread = offset-curread;
			res = read (file->fd, data, toread);
			if (res <= 0) {
				free (data);
				fclose (fp);
				unlink (filename);
				return GP_ERROR_IO_READ;
			}
			if ((int)fwrite (data, 1, res, fp) != res) {
				GP_LOG_E ("Not enough space on device in order to save '%s'.", filename);
				free (data);
				fclose (fp);
				unlink (filename);
				return GP_ERROR;
			}
			curread += res;
		}
		free (data);
		fclose (fp);
		break;
	}
	default:
		GP_LOG_E ("Unknown file access type %d", file->accesstype);
		return GP_ERROR;
	}

	if (file->mtime) {
		u.actime = file->mtime;
		u.modtime = file->mtime;
		utime (filename, &u);
	}

	return (GP_OK);
}


/*
 * mime types that cannot be determined by the filename
 * extension. Better hack would be to use library that examine
 * file content instead, like gnome-vfs mime handling, or
 * gnome-mime, whatever.
 * See also the GP_MIME_* definitions.
 */
static const char *mime_table[] = {
    "bmp",  GP_MIME_BMP,
    "jpg",  GP_MIME_JPEG,
    "tif",  GP_MIME_TIFF,
    "ppm",  GP_MIME_PPM,
    "pgm",  GP_MIME_PGM,
    "pnm",  GP_MIME_PNM,
    "png",  GP_MIME_PNG,
    "wav",  GP_MIME_WAV,
    "avi",  GP_MIME_AVI,
    "mp3",  GP_MIME_MP3,
    "wma",  GP_MIME_WMA,
    "asf",  GP_MIME_ASF,
    "ogg",  GP_MIME_OGG,
    "mpg",  GP_MIME_MPEG,
    "raw",  GP_MIME_RAW,
    "mts",  GP_MIME_AVCHD,
    "m2ts", GP_MIME_AVCHD,
    "arw",  GP_MIME_ARW,
    "txt",  GP_MIME_TXT,
    NULL};
/**
 * @param file a #CameraFile
 * @param filename
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_open (CameraFile *file, const char *filename)
{
        FILE *fp;
        char *name, *dot;
        long size, size_read;
        int  i;
	struct stat s;


	C_PARAMS (file && filename);

	CHECK_RESULT (gp_file_clean (file));

        fp = fopen(filename, "r");
        if (!fp)
		return (GP_ERROR);
	fseek (fp, 0, SEEK_END);
        size = ftell (fp);
        rewind (fp);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		file->data = malloc (sizeof(char)*(size + 1));
		if (!file->data) {
			fclose (fp);
			return (GP_ERROR_NO_MEMORY);
		}
		size_read = fread (file->data, (size_t)sizeof(char), (size_t)size, fp);
		if (ferror(fp)) {
			gp_file_clean (file);
			fclose (fp);
			return (GP_ERROR);
		}
		fclose(fp);
		file->size = size_read;
		file->data[size_read] = 0;
		break;
	case GP_FILE_ACCESSTYPE_FD: {
		if (file->fd == -1) {
			file->fd = dup(fileno(fp));
			fclose (fp);
			break;
		}
		GP_LOG_E ("Needs to be initialized with fd=-1 to work");
		fclose (fp);
		return GP_ERROR;
	}
	default:
		break;
	}

        name = strrchr (filename, '/');
        if (name)
                strncpy (file->name, name + 1, sizeof (file->name));
           else
                strncpy (file->name, filename, sizeof (file->name));

        /* MIME lookup */
        dot = strrchr (filename, '.');
        if (dot) {
            for (i = 0; mime_table[i] ; i+=2)
                if (!strcasecmp (mime_table[i], dot+1)) {
                    strncpy (file->mime_type, mime_table[i+1], sizeof(file->mime_type));
                    break;
                }
            if (!mime_table[i])
                /*
                 * We did not found the type in the lookup table,
                 * so we use the file suffix as mime type.
                 * Note: This should probably use GP_MIME_UNKNOWN instead
                 * of returning a non-standard type.
                 */
                sprintf(file->mime_type, "image/%s", dot + 1);
        } else
            /*
             * Damn, no filename suffix...
             */
            strncpy (file->mime_type, GP_MIME_UNKNOWN,
		     sizeof (file->mime_type));

	if (stat (filename, &s) != -1) {
		file->mtime = s.st_mtime;
	} else {
		file->mtime = time (NULL);
	}

        return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_clean (CameraFile *file)
{
        /*
	 * Frees a CameraFile's components, not the CameraFile itself.
	 * This is used to prep a CameraFile struct to be filled.
         */

	C_PARAMS (file);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		free (file->data);
		file->data = NULL;
		file->size = 0;
		break;
	case GP_FILE_ACCESSTYPE_FD:
		break;
	default:break;
	}
	strcpy (file->name, "");
        return (GP_OK);
}

/**
 * @param destination a #CameraFile
 * @param source a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_copy (CameraFile *destination, CameraFile *source)
{
	C_PARAMS (destination && source);

	GP_LOG_D ("Copying '%s' onto '%s'...", source->name, destination->name);

	/* struct members we can just copy. All generic ones, but not refcount. */
	memcpy (destination->name, source->name, sizeof (source->name));
	memcpy (destination->mime_type, source->mime_type, sizeof (source->mime_type));
	destination->mtime = source->mtime;

	if ((destination->accesstype == GP_FILE_ACCESSTYPE_MEMORY) &&
	    (source->accesstype == GP_FILE_ACCESSTYPE_MEMORY)) {
		free (destination->data);
		destination->data = NULL;
		destination->size = source->size;
		C_MEM (destination->data = malloc (sizeof (char) * source->size));
		memcpy (destination->data, source->data, source->size);
		return (GP_OK);
	}
	if (	(destination->accesstype == GP_FILE_ACCESSTYPE_MEMORY) &&
		(source->accesstype == GP_FILE_ACCESSTYPE_FD)
	) {
		off_t	offset;
		off_t	curread = 0;

		free (destination->data);
		destination->data = NULL;

		if (-1 == lseek (source->fd, 0, SEEK_END)) {
			if (errno == EBADF) return GP_ERROR_IO;
			/* Might happen for pipes or sockets. Umm. Hard. */
			/* FIXME */
		}
		if (-1 == (offset = lseek (source->fd, 0, SEEK_CUR))) {
			/* should not happen if we passed the above case */
			GP_LOG_E ("Encountered error %d lseekin to CUR.", errno);
			return GP_ERROR_IO_READ;
		}
		if (-1 == lseek (source->fd, 0, SEEK_SET)) {
			/* should not happen if we passed the above cases */
			GP_LOG_E ("Encountered error %d lseekin to CUR.", errno);
			return GP_ERROR_IO_READ;
		}
		destination->size = offset;
		C_MEM (destination->data = malloc (offset));
		while (curread < offset) {
			ssize_t res = read (source->fd, destination->data+curread, offset-curread);
			if (res == -1) {
				free (destination->data);
				GP_LOG_E ("Encountered error %d reading.", errno);
				return GP_ERROR_IO_READ;
			}
			if (res == 0) {
				free (destination->data);
				GP_LOG_E ("No progress during reading.");
				return GP_ERROR_IO_READ;
			}
			curread += res;
		}
		return GP_OK;
	}
	if (	(destination->accesstype == GP_FILE_ACCESSTYPE_FD) &&
		(source->accesstype == GP_FILE_ACCESSTYPE_FD)
	) {
		char *data;

		lseek (destination->fd, 0, SEEK_SET);
		if (-1 == ftruncate (destination->fd, 0))
			perror("ftruncate");
		lseek (source->fd, 0, SEEK_SET);
		C_MEM (data = malloc (65536));
		while (1) {
			ssize_t curwritten = 0;
			ssize_t res;

			res = read (source->fd, data, 65536);
			if (res == -1) {
				free (data);
				return GP_ERROR_IO_READ;
			}
			if (res == 0)
				break;
			while (curwritten < res) {
				ssize_t res2 = write (destination->fd, data+curwritten, res-curwritten);
				if (res2 == -1) {
					free (data);
					return GP_ERROR_IO_WRITE;
				}
				if (res2 == 0)
					break;
				curwritten += res2;
			}
			if (res < 65536) /* end of file */
				break;
		}
		free (data);
		return GP_OK;
	}
	if (	(destination->accesstype == GP_FILE_ACCESSTYPE_FD) &&
		(source->accesstype == GP_FILE_ACCESSTYPE_MEMORY)
	) {
		unsigned long curwritten = 0;
		while (curwritten < source->size) {
			int res = write (destination->fd, source->data+curwritten, source->size-curwritten);

			if (res == -1)
				return GP_ERROR_IO_WRITE;
			if (!res) /* no progress? */
				return GP_ERROR_IO_WRITE;
			curwritten += res;
		}
		return GP_OK;
	}
	if (	(destination->accesstype == GP_FILE_ACCESSTYPE_HANDLER) &&
		(source->accesstype == GP_FILE_ACCESSTYPE_MEMORY)
	) {
		uint64_t	xsize = source->size;
		unsigned long curwritten = 0;

		destination->handler->size (destination->private, &xsize);
		while (curwritten < source->size) {
			uint64_t	tmpsize = source->size - curwritten;
			int res = destination->handler->write (destination->private, source->data+curwritten, &tmpsize);

			if (res < GP_OK)
				return res;
			if (!tmpsize) /* no progress? */
				return GP_ERROR_IO_WRITE;
			curwritten += tmpsize;
		}
		return GP_OK;
	}
	GP_LOG_E ("Unhandled cases in gp_copy_file. Bad!");
	return GP_ERROR;
}

/**
 * @param file a #CameraFile
 * @param name a pointer to a name string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_get_name (CameraFile *file, const char **name)
{
	C_PARAMS (file && name);

	*name = file->name;

	return (GP_OK);
}

/**
 * @param file a #CameraFile
 * @param basename the basename of the file
 * @param type the gphoto type of the file
 * @param newname the new name generated
 * @return a gphoto2 error code.
 *
 * This function takes the basename and generates a filename out of
 * it depending on the gphoto filetype and the mime type in the file.
 * The gphoto filetype will be converted to a prefix, like
 * thumb_ or raw_, the mimetype will replace the current suffix by
 * a different one (if necessary).
 *
 * This can be used so that saving thumbnails or metadata will not
 * overwrite the normal files.
 **/
int
gp_file_get_name_by_type (CameraFile *file, const char *basename, CameraFileType type, char **newname)
{
	char *prefix = NULL, *s, *new, *slash = NULL;
	const char *suffix = NULL;
	int i;

	C_PARAMS (file && basename && newname);
	*newname = NULL;

	/* the easy case, always map 1:1, if it has a suffix already. */
	if ((type == GP_FILE_TYPE_NORMAL) && strchr(basename,'.')) {
		C_MEM (*newname = strdup (basename));
		return GP_OK;
	}

	for (i=0;mime_table[i];i+=2) {
		if (!strcmp (mime_table[i+1],file->mime_type)) {
			suffix = mime_table[i];
			break;
		}
	}
	s = strrchr(basename,'.');
	slash = strrchr(basename,'/');
	if (slash > s)	/* --filename foo.bar/foo */
		s = NULL;
	switch (type) {
	case GP_FILE_TYPE_RAW:		prefix = "raw_";break;
	case GP_FILE_TYPE_EXIF:		prefix = "exif_";break;
	case GP_FILE_TYPE_PREVIEW:	prefix = "thumb_";break;
	case GP_FILE_TYPE_METADATA:	prefix = "meta_";break;
	case GP_FILE_TYPE_AUDIO:	prefix = "audio_";break;
	default:			prefix = ""; break;
	}
	if (s) {
		int xlen;

		if (!suffix)
			suffix = s+1;

		C_MEM (new = calloc (strlen(prefix) + (s-basename+1) + strlen (suffix) + 1, 1));

		xlen = strlen (prefix);
		if (slash) {
			memcpy (new, basename, slash-basename+1);
			strcat (new, prefix);
			memcpy (new + strlen (new), slash+1, s-slash+1);
		} else {
			strcpy (new, prefix);
			memcpy (new + strlen (new), basename, s-basename+1);
		}
		new[xlen+(s-basename)+1]='\0';
		strcat (new, suffix);
	} else { /* no dot in basename? */
		if (!suffix) suffix = "";
		C_MEM (new = calloc (strlen(prefix) + strlen(basename) + 1 + strlen (suffix) + 1, 1));
		if (slash) {
			memcpy (new, basename, slash-basename+1); /* with / */
			strcat (new, prefix);
			strcat (new, slash+1);
		} else {
			strcpy (new, prefix);
			strcat (new, basename);
		}
		if (strlen(suffix)) {
			strcat (new, ".");
			strcat (new, suffix);
		}
	}
	*newname = new;
	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mime_type a pointer to a MIME type string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_get_mime_type (CameraFile *file, const char **mime_type)
{
	C_PARAMS (file && mime_type);

	*mime_type = file->mime_type;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param name a pointer to a MIME type string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_name (CameraFile *file, const char *name)
{
	C_PARAMS (file && name);

	strncpy (file->name, name, sizeof (file->name));

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mime_type a MIME type string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_mime_type (CameraFile *file, const char *mime_type)
{
	C_PARAMS (file && mime_type);

	strncpy (file->mime_type, mime_type, sizeof (file->mime_type));

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_detect_mime_type (CameraFile *file)
{
	const char TIFF_SOI_MARKER[] = {(char) 0x49, (char) 0x49, (char) 0x2A,
				        (char) 0x00, (char) 0x08, '\0' };
	const char JPEG_SOI_MARKER[] = {(char) 0xFF, (char) 0xD8, '\0' };

	C_PARAMS (file);

	switch (file->accesstype) {
	case GP_FILE_ACCESSTYPE_MEMORY:
		/* image/tiff */
		if ((file->size >= 5) && !memcmp (file->data, TIFF_SOI_MARKER, 5))
			CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_TIFF))

		/* image/jpeg */
		else if ((file->size >= 2) && !memcmp (file->data, JPEG_SOI_MARKER, 2))
			CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG))
		else
			CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_RAW));
		return GP_OK;
	case GP_FILE_ACCESSTYPE_FD: {
		char	data[5];
		off_t	offset;
		int	res;

		offset = lseek (file->fd, 0, SEEK_SET);
		res = read (file->fd, data, sizeof(data));
		if (res == -1)
			return GP_ERROR_IO_READ;
		/* image/tiff */
		if ((res >= 5) && !memcmp (data, TIFF_SOI_MARKER, 5))
			CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_TIFF))

		/* image/jpeg */
		else if ((res >= 2) && !memcmp (data, JPEG_SOI_MARKER, 2))
			CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG))
		else
			CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_RAW));
		lseek (file->fd, offset, SEEK_SET);
		break;
	}
	default:
		break;
	}
	return GP_OK;
}


/**
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_adjust_name_for_mime_type (CameraFile *file)
{
	int x;
	char *suffix;
	const char *table[] = {
		GP_MIME_RAW,  "raw",
		GP_MIME_JPEG, "jpg",
		GP_MIME_PNG,  "png",
		GP_MIME_PPM,  "ppm",
		GP_MIME_PGM,  "pgm",
		GP_MIME_PNM,  "pnm",
		GP_MIME_TIFF, "tif",
		GP_MIME_WAV,  "wav",
		GP_MIME_BMP,  "bmp",
		GP_MIME_AVI,  "avi",
		GP_MIME_CRW,  "crw",
		GP_MIME_CR2,  "cr2",
		GP_MIME_CR3,  "cr3",
		GP_MIME_ARW,  "arw",
		GP_MIME_NEF,  "nef",
		GP_MIME_TXT,  "txt",
		NULL};

	C_PARAMS (file);

	GP_LOG_D ("Adjusting file name for mime type '%s'...", file->mime_type);
	for (x = 0; table[x]; x += 2)
                if (!strcmp (file->mime_type, table[x])) {

			/* Search the current suffix and erase it */
			suffix = strrchr (file->name, '.');
			if (suffix++)
				*suffix = '\0';
			strcat (file->name, table[x + 1]);
			break;
		}
	GP_LOG_D ("Name adjusted to '%s'.", file->name);

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mtime
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_get_mtime (CameraFile *file, time_t *mtime)
{
	C_PARAMS (file && mtime);

	*mtime = file->mtime;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mtime
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_mtime (CameraFile *file, time_t mtime)
{
	C_PARAMS (file);

	file->mtime = mtime;

	return (GP_OK);
}
