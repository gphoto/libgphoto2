/*
 * commands.c
 *
 *  Command set for the digita cameras
 *
 * Copyright 1999-2001 Johannes Erdfelt
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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#ifdef OS2
#include <db.h>
#endif

#include "digita.h"
#include "gphoto2-endian.h"

#define GP_MODULE "digita"

#include <gphoto2/gphoto2-port.h>

static void build_command(struct digita_command *cmd, int length, short command)
{
	memset(cmd, 0, sizeof(*cmd));

	/* Length is the sizeof the digita_command minus the length */
	/*  parameter, plus whatever other data we send */
	cmd->length = htobe32(sizeof(struct digita_command) -
		sizeof(unsigned int) + length);
	cmd->command = htobe16(command);
}

struct storage_status {
	struct digita_command cmd;

	unsigned int takencount;
	unsigned int availablecount;
	int rawcount;
};

int digita_get_storage_status(CameraPrivateLibrary *dev, int *taken,
	int *available, int *rawcount)
{
	struct digita_command cmd;
	struct storage_status ss;
	int ret;

	build_command(&cmd, 0, DIGITA_GET_STORAGE_STATUS);

	ret = dev->send(dev, &cmd, sizeof(cmd));
	if (ret < 0) {
		GP_DEBUG("digita_get_storage_status: error sending command (ret = %d)", ret);
		return -1;
	}

	ret = dev->read(dev, (unsigned char *)&ss, sizeof(ss));
	if (ret < 0) {
		GP_DEBUG("digita_get_storage_status: error getting count (ret = %d)", ret);
		return -1;
	}

	if (taken)
		*taken = be32toh(ss.takencount);
	if (available)
		*available = be32toh(ss.availablecount);
	if (rawcount)
		*rawcount = be32toh(ss.rawcount);

	return 0;
}

int digita_get_file_list(CameraPrivateLibrary *dev)
{
	struct digita_command cmd;
	struct get_file_list gfl;
	char *buffer;
	int ret, taken, buflen;
	int err;

	if (digita_get_storage_status(dev, &taken, NULL, NULL) < 0)
		return -1;

	dev->num_pictures = taken;

	buflen = (taken * sizeof(struct file_item)) + sizeof(cmd) + 4;
	buffer = malloc(buflen);
	if (!buffer) {
		GP_DEBUG("digita_get_file_list: error allocating %d bytes", buflen);
		return -1;
	}

	build_command(&gfl.cmd, sizeof(gfl) - sizeof(gfl.cmd), DIGITA_GET_FILE_LIST);
	gfl.listorder = htobe32(1);

	ret = dev->send(dev, (void *)&gfl, sizeof(gfl));
	if (ret < 0) {
		GP_DEBUG("digita_get_file_list: error sending command (ret = %d)", ret);
		err = -1;
		goto end;
	}

	ret = dev->read(dev, (void *)buffer, buflen);
	if (ret < 0) {
		GP_DEBUG("digita_get_file_list: error receiving data (ret = %d)", ret);
		err = -1;
		goto end;
	}

	if (dev->file_list)
		free(dev->file_list);

	dev->file_list = malloc(taken * sizeof(struct file_item));
	if (!dev->file_list) {
		GP_DEBUG("digita_get_file_list: error allocating file_list memory (ret = %d)", ret);
		err = -1;
		goto end;
	}

	memcpy(dev->file_list, buffer + sizeof(cmd) + 4, taken * sizeof(struct file_item));

	err = 0;
 end:
	free(buffer);

	return err;
}

#define GFD_BUFSIZE 19432
int digita_get_file_data(CameraPrivateLibrary *dev, int thumbnail,
	struct filename *filename, struct partial_tag *tag, void *buffer)
{
	struct get_file_data_send gfds;
	struct get_file_data_receive *gfdr;
	int ret;
	char *tbuf;

	build_command(&gfds.cmd, sizeof(gfds) - sizeof(gfds.cmd), DIGITA_GET_FILE_DATA);

	memcpy(&gfds.fn, filename, sizeof(gfds.fn));
	memcpy(&gfds.tag, tag, sizeof(gfds.tag));
	gfds.dataselector = htobe32(thumbnail);

	tbuf = malloc(GFD_BUFSIZE + sizeof(*gfdr));
	if (!tbuf) {
		GP_DEBUG("digita_get_file_data: unable to allocate %ud bytes",
			(unsigned int)(GFD_BUFSIZE + sizeof(*gfdr)));

		return -1;
	}
	gfdr = (struct get_file_data_receive *)tbuf;

	ret = dev->send(dev, &gfds, sizeof(gfds));
	if (ret < 0) {
		GP_DEBUG("digita_get_file_data: error sending command (ret = %d)", ret);
		free(tbuf);
		return -1;
	}

	ret = dev->read(dev, gfdr, GFD_BUFSIZE + sizeof(*gfdr));
	if (ret < 0) {
		GP_DEBUG("digita_get_file_data: error reading data (ret = %d)", ret);
		return -1;
	}

	if (gfdr->cmd.result) {
		GP_DEBUG("digita_get_file_data: bad result (%d)", gfdr->cmd.result);
		return gfdr->cmd.result;
	}

	memcpy(buffer, tbuf + sizeof(*gfdr), be32toh(gfdr->tag.length) + (thumbnail ? 16 : 0));
	memcpy(tag, &gfdr->tag, sizeof(*tag));

	free(tbuf);

	return 0;
}

int digita_delete_picture(CameraPrivateLibrary *dev, struct filename *filename)
{
	struct erase_file ef;
	struct digita_command response;
	int ret;

	build_command(&ef.cmd, sizeof(ef.fn), DIGITA_ERASE_FILE);

	memcpy(&ef.fn, filename, sizeof(ef.fn));
	ef.zero = 0;

	ret = dev->send(dev, (unsigned char *)&ef, sizeof(ef));
	if (ret < 0) {
		GP_DEBUG("error sending command (ret = %d)", ret);
		return -1;
	}

	ret = dev->read(dev, (unsigned char *)&response, sizeof(response));
	if (ret < 0) {
		GP_DEBUG("error reading reply (ret = %d)", ret);
		return -1;
	}

	return 0;
}

