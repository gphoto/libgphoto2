/* gphoto2-result.h
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

#ifndef __GPHOTO2_RESULT_H__
#define __GPHOTO2_RESULT_H__

/* Additional error codes are defined here */
#include <gphoto2-port-result.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * GP_ERROR_CORRUPTED_DATA:
 *
 * Data is corrupt. This error is reported by camera drivers if corrupted
 * data has been received that can not be automatically handled. Normally,
 * drivers will do everything possible to automatically recover from this
 * error.
 **/
#define GP_ERROR_CORRUPTED_DATA      -102 /* Corrupted data             */

/**
 * GP_ERROR_FILE_EXISTS:
 *
 * An operation failed because a file existed. This error is reported for
 * example when the user tries to create a file that already exists.
 **/
#define GP_ERROR_FILE_EXISTS         -103

/**
 * GP_ERROR_MODEL_NOT_FOUND:
 *
 * The specified model could not be found. This error is reported when
 * the user specified a model that does not seem to be supported by 
 * any driver.
 **/
#define GP_ERROR_MODEL_NOT_FOUND     -105

/**
 * GP_ERROR_DIRECTORY_NOT_FOUND:
 *
 * The specified directory could not be found. This error is reported when
 * the user specified a directory that is non-existent.
 **/
#define GP_ERROR_DIRECTORY_NOT_FOUND -107

/**
 * GP_ERROR_FILE_NOT_FOUND:
 *
 * The specified file could not be found. This error is reported when
 * the user wants to access a file that is non-existent.
 **/
#define GP_ERROR_FILE_NOT_FOUND      -108

/**
 * GP_ERROR_DIRECTORY_EXISTS:
 *
 * The specified directory already exists. This error is reported for example 
 * when the user wants to create a directory that already exists.
 **/
#define GP_ERROR_DIRECTORY_EXISTS    -109

/**
 * GP_ERROR_PATH_NOT_ABSOLUTE:
 * 
 * The specified path is not absolute. This error is reported when the user
 * specifies paths that are not absolute, i.e. paths like "path/to/directory".
 * As a rule of thumb, in gphoto2, there is nothing like relative paths.
 **/
#define GP_ERROR_PATH_NOT_ABSOLUTE   -111

#define GP_ERROR_CANCEL              -112


const char *gp_result_as_string      (int result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GPHOTO2_RESULT_H__ */
