/** \file
 * \brief internal header to help with locking e.g. MT-unsafe libltdl
 *
 * \author Copyright 2017 Kris Adler <spurfan15@gmail.com>
 * \author Copyright 2022 Marcus Meissner
 * \author Copyright 2022 Hans Ulrich Niedermann
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef LIBGPHOTO2_GPHOTO2_PORT_LOCKING_H
#define LIBGPHOTO2_GPHOTO2_PORT_LOCKING_H

#ifdef _GPHOTO2_INTERNAL_CODE

/** lock libltdl before calling lt_*() (libltdl is not thread safe) */
extern void gpi_libltdl_lock(void);

/** unlock libltdl after calling lt_*() (libltdl is not thread safe) */
extern void gpi_libltdl_unlock(void);

#endif /* defined(_GPHOTO2_INTERNAL_CODE) */

#endif /* defined(LIBGPHOTO2_GPHOTO2_PORT_LOCKING_H) */
