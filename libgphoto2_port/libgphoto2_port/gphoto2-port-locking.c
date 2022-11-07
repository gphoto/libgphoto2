/** \file
 * \brief internal header to help with locking e.g. MT-unsafe libltdl
 *
 * \author Copyright 2017 kadler15 <spurfan15@gmail.com>
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

#include <pthread.h>

#include <gphoto2/gphoto2-port-locking.h>

static
pthread_mutex_t gpi_libltdl_mutex = PTHREAD_MUTEX_INITIALIZER;

void gpi_libltdl_lock(void)
{
  pthread_mutex_lock(&gpi_libltdl_mutex);
}

void gpi_libltdl_unlock(void)
{
  pthread_mutex_unlock(&gpi_libltdl_mutex);
}
