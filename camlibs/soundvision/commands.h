/*
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef commands_H
#define commands_H


int32_t soundvision_send_command(uint32_t command, uint32_t argument,
	                                 CameraPrivateLibrary *dev);

int32_t soundvision_send_file_command(const char *filename,
                                              CameraPrivateLibrary *dev);

int32_t soundvision_read(CameraPrivateLibrary *dev, void *buffer, int len);

#endif
