/* jl2005c.c
 *
 * Copyright (C) 2006-2010 Theodore Kilgore <kilgota@auburn.edu>
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

#define _BSD_SOURCE

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include "gphoto2-endian.h"

#include "jl2005c.h"

#define GP_MODULE "jl2005c"

int
jl2005c_init (Camera *camera, GPPort *port, CameraPrivateLibrary *priv)
{
	char response;
	int model_string = 0;
	/* Needs to be big enough to hold (0xfff + 3) * 0x10 */
	unsigned char info[0x4020];
	const char camera_id[] = {0x4a, 0x4c, 0x32, 0x30, 0x30, 0x35};
	int alloc_table_size;
	int attempts = 0;
restart:
	alloc_table_size = 0;
	memset(info, 0, sizeof(info));
	GP_DEBUG("Running jl2005c_init\n");
	if (priv->init_done) {
		gp_port_close(port);
		usleep (100000);
		gp_port_open(port);
	}

	set_usb_in_endpoint	(camera, 0x84);
	gp_port_write (port, "\x08\x00", 2);
	usleep (10000);
	gp_port_write (port, "\x95\x60", 2);
	jl2005c_read_data (port, &response, 1);
	model_string = response;
	gp_port_write (port, "\x95\x61", 2);
	jl2005c_read_data (port, &response, 1);
	model_string += (response & 0xff) << 8;
	gp_port_write (port, "\x95\x62", 2);
	jl2005c_read_data (port, &response, 1);
	model_string += (response & 0xff) << 16;
	gp_port_write (port,"\x95\x63" , 2);
	jl2005c_read_data (port, &response, 1);
	model_string += (response & 0xff) << 24;
	GP_DEBUG("Model string is %08x\n", model_string);
	gp_port_write (port, "\x95\x64", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x95\x65", 2);
	jl2005c_read_data (port, &response, 1);
	/* Number of pix returned here, but not reliably reported */
	priv->nb_entries = response & 0xff;
	GP_DEBUG("%d frames in the camera (unreliable!)\n", priv->nb_entries);
	gp_port_write (port, "\x95\x66", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x95\x67", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x95\x68", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x95\x69", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x95\x6a", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x95\x6b", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x95\x6c", 2);
	jl2005c_read_data (port, &response, 1);
	priv->data_to_read = (response & 0xff) * 0x100;
	gp_port_write (port, "\x95\x6d", 2);
	jl2005c_read_data (port, &response, 1);
	priv->data_to_read += (response&0xff);
	priv->total_data_in_camera = priv->data_to_read;
	GP_DEBUG ("blocks_to_read = 0x%lx = %lu\n", priv->data_to_read,
							priv->data_to_read);
	gp_port_write (port, "\x95\x6e", 2);
	jl2005c_read_data (port, &response, 1);
	alloc_table_size = (response & 0xff) * 0x200;
	GP_DEBUG("alloc_table_size = 0x%02x * 0x200 = 0x%x\n",
				response & 0xff, (response & 0xff) * 0x200);
	gp_port_write (port, "\x95\x6f", 2);
	jl2005c_read_data (port, &response, 1);
	gp_port_write (port, "\x0a\x00", 2);
	usleep (10000);
	/* Switch the inep over to 0x82. It stays there ever after. */
	set_usb_in_endpoint	(camera, 0x82);

	/* Read the first block of the allocation table. */
	jl2005c_read_data (port, (char *)info, 0x200);
	if (strncmp(camera_id, (char*)info, 6)) {
		GP_DEBUG("Error downloading alloc table\n");
		GP_DEBUG("Init attempted %d times\n", attempts + 1);
		attempts++;
		if (attempts == 3) {
			GP_DEBUG("Third try. Giving up\n");
			gp_port_write(port, "\x07\x00", 2);
			return GP_ERROR;
		}
		goto restart;
	}

	/* Now check the number of photos. That is found in byte 13 of line 0
	 * of the allocation table.
	 */
	priv->nb_entries = (info[12] & 0xff) * 0x100 | (info[13] & 0xff);
	GP_DEBUG("Number of entries is recalculated as %d\n",
						priv->nb_entries);

	/* Just in case there was a problem, we now recalculate the total
	 * alloc_table_size. */
	alloc_table_size = priv->nb_entries * 0x10 + 0x30;
	if (alloc_table_size%0x200)
		alloc_table_size += 0x200 - (alloc_table_size%0x200);
	/* However, we have already just now downloaded 0x200 bytes, so
	 * when downloading the rest of the table we correct for that and
	 * just download whatever remains of the information block.
	 */
	if (alloc_table_size > 0x200)
		gp_port_read(port, (char *)info + 0x200,
						alloc_table_size - 0x200);
	memmove(priv->table, info + 0x30, alloc_table_size - 0x30);
	priv->model = info[6];
	GP_DEBUG("Model is %c\n", priv->model);
	switch (priv->model) {
	case 0x43:
	case 0x44:
		priv->blocksize = 0x200;
		break;
	case 0x42:
		priv->blocksize = 0x80;
		break;
	default:
		GP_DEBUG("Unknown model, unknown blocksize\n");
		return GP_ERROR_NOT_SUPPORTED;
	}
	GP_DEBUG("camera's blocksize = 0x%x = %d\n", priv->blocksize,
						     priv->blocksize);
	/* Now a more responsible calculation of the amount of data in the
	 * camera, based upon the allocation table. */
	priv->data_to_read = info[10] * 0x100 | info[11];
	priv->data_to_read -= info[8] * 0x100 | info[9];
	priv->data_to_read *= priv->blocksize;
	priv->total_data_in_camera = priv->data_to_read;
	GP_DEBUG ("data_to_read = 0x%lx = %lu\n", priv->data_to_read,
							priv->data_to_read);
	GP_DEBUG ("total_data_in_camera = 0x%lx = %lu\n", priv->data_to_read,
							  priv->data_to_read);
	priv->can_do_capture = 0;
	if (info[7] & 0x04)
		priv->can_do_capture = 1;
	priv->bytes_read_from_camera = 0;
	priv->bytes_put_away = 0;
	priv->init_done = 1;
	GP_DEBUG("Leaving jl2005c_init\n");
	return GP_OK;
}

int jl2005c_open_data_reg (Camera *camera, GPPort *port)
{
	gp_port_write (port, "\x0b\x00",2);
	usleep (10000);
	GP_DEBUG("Opening data register.\n");
	camera->pl->data_reg_opened = 1;
	return GP_OK;
}

int
jl2005c_get_pic_data_size (CameraPrivateLibrary *priv, Info *table, int n)
{
	int size;
	GP_DEBUG("table[16 * n + 7] = %02X\n", table[16 * n + 7]);
	size = table[0x10 * n + 6] * 0x100 | table[0x10 * n + 7];
	size *= priv->blocksize;
	GP_DEBUG("size = 0x%x = %d\n", size, size);
	return (size);
}

unsigned long
jl2005c_get_start_of_photo(CameraPrivateLibrary *priv, Info *table,
							unsigned int n)
{
	unsigned long start;
	start = table[0x10 * n + 0x0c] * 0x100 | table[0x10 * n + 0x0d];
	start -= table[0x0c] * 0x100 | table[0x0d];
	start *= priv->blocksize;
	return start;
}


int
set_usb_in_endpoint	(Camera *camera, int inep)
{
	GPPortSettings settings;
	gp_port_get_settings ( camera ->port, &settings);
	if(settings.usb.inep != inep)
		settings.usb.inep = inep;
	GP_DEBUG("inep reset to %02X\n", inep);
	return gp_port_set_settings ( camera->port, settings);
}

int
jl2005c_read_data (GPPort *port, char *data, int size)
{
	/* These cameras tend to be slow. */
	usleep (10000);
	gp_port_read (port, data, size);
	usleep (10000);
	return GP_OK;
}

int jl2005c_reset (Camera *camera, GPPort *port)
{
	int downloadsize = MAX_DLSIZE;
	/* If any data has been downloaded, these cameras want all data to be
	 * dumped before exit. If that is not yet done, then do it now! */
	if(camera->pl->data_reg_opened) {
		while (camera->pl->bytes_read_from_camera <
				    camera->pl->total_data_in_camera ) {
			if (!camera->pl->data_cache )
				camera->pl->data_cache = malloc (MAX_DLSIZE);
			downloadsize = MAX_DLSIZE;
			if (camera->pl->bytes_read_from_camera + MAX_DLSIZE >=
				    camera->pl->total_data_in_camera )
				downloadsize = camera->pl->total_data_in_camera -
					camera->pl->bytes_read_from_camera;
			if (downloadsize)
				jl2005c_read_data (camera->port,
					    (char *) camera->pl->data_cache,
					    downloadsize);
			camera->pl->bytes_read_from_camera += downloadsize;
		}
	}
	gp_port_write(port, "\x07\x00", 2);
	camera->pl->data_reg_opened = 0;
	return GP_OK;
}

int jl2005c_delete_all (Camera *camera, GPPort *port)
{
	gp_port_write(port, "\x09\x00", 2);
	usleep(10000);
	gp_port_write(port, "\x07\x00", 2);
	return GP_OK;
}
