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
#include <gphoto2-port.h>

#define GP_ERROR_BAD_PARAMETERS      -100 /* for checking function-param. */
#define GP_ERROR_IO                  -101 /* IO problem                 */
#define GP_ERROR_CORRUPTED_DATA      -102 /* Corrupted data             */
#define GP_ERROR_FILE_EXISTS         -103 /* File exists                */
#define GP_ERROR_NO_MEMORY           -104 /* Insufficient memory        */
#define GP_ERROR_MODEL_NOT_FOUND     -105 /* Model not found            */
#define GP_ERROR_NOT_SUPPORTED       -106 /* Some op. is unsupported    */
#define GP_ERROR_DIRECTORY_NOT_FOUND -107 /* Directory not found        */
#define GP_ERROR_FILE_NOT_FOUND      -108 /* File not found             */
#define GP_ERROR_DIRECTORY_EXISTS    -109 /* Directory exists           */
#define GP_ERROR_NO_CAMERA_FOUND     -110 /* No cameras auto-detected   */
#define GP_ERROR_PATH_NOT_ABSOLUTE   -111 /* Path not absolute          */

char   *gp_result_as_string (int result);

#endif /* __GPHOTO2_RESULT_H__ */
