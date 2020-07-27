
/* $Id: usb_io.h,v 1.14 2005/08/26 16:25:34 purbanec Exp $ */

/*

  Copyright (C) 2004 Peter Urbanec <toppy at urbanec.net>

  This file is part of puppy.

  puppy is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  puppy is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with puppy; if not, write to the
  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301  USA

*/

#ifndef _USB_IO_H
#define _USB_IO_H 1

#include <gphoto2/gphoto2-camera.h>
#include <sys/types.h>

#define BITS_PER_LONG sizeof(long)*8

#include "mjd.h"
#include "tf_bytes.h"

/* Topfield command codes */
#define FAIL                      (0x0001L)
#define SUCCESS                   (0x0002L)
#define CANCEL                    (0x0003L)

#define CMD_READY                 (0x0100L)
#define CMD_RESET                 (0x0101L)
#define CMD_TURBO                 (0x0102L)

#define CMD_HDD_SIZE              (0x1000L)
#define DATA_HDD_SIZE             (0x1001L)

#define CMD_HDD_DIR               (0x1002L)
#define DATA_HDD_DIR              (0x1003L)
#define DATA_HDD_DIR_END          (0x1004L)

#define CMD_HDD_DEL               (0x1005L)
#define CMD_HDD_RENAME            (0x1006L)
#define CMD_HDD_CREATE_DIR        (0x1007L)

#define CMD_HDD_FILE_SEND         (0x1008L)
#define DATA_HDD_FILE_START       (0x1009L)
#define DATA_HDD_FILE_DATA        (0x100aL)
#define DATA_HDD_FILE_END         (0x100bL)

/* Number of milliseconds to wait for a packet transfer to complete. */

/* This is intentionally large enough to allow for a HDD spin up. */
#define TF_PROTOCOL_TIMEOUT 11000

/* The maximum packet size used by the Toppy.  */
#define MAXIMUM_PACKET_SIZE 0xFFFFL

/* The size of every packet header. */
#define PACKET_HEAD_SIZE 8

/* Format of a Topfield protocol packet */
#pragma pack(1)
struct tf_packet
{
    uint16_t length;
    uint16_t crc;
    uint32_t cmd;
    uint8_t data[MAXIMUM_PACKET_SIZE - PACKET_HEAD_SIZE];
};
#pragma pack()

/* Topfield file descriptor data structure. */
#pragma pack(1)
struct typefile
{
    struct tf_datetime stamp;
    uint8_t filetype;
    uint64_t size;
    uint8_t name[95];
    uint8_t unused;
    uint32_t attrib;
};
#pragma pack()


ssize_t send_success(Camera *camera, GPContext *context);
ssize_t send_cancel(Camera *camera, GPContext *context);
ssize_t send_cmd_ready(Camera *camera, GPContext *context);
ssize_t send_cmd_reset(Camera *camera, GPContext *context);
ssize_t send_cmd_turbo(Camera *camera, int turbo_on, GPContext *context);
ssize_t send_cmd_hdd_size(Camera *camera, GPContext *context);
ssize_t send_cmd_hdd_dir(Camera *camera, char *path, GPContext *context);
ssize_t send_cmd_hdd_file_send(Camera *camera, uint8_t dir, char *path, GPContext *context);
ssize_t send_cmd_hdd_del(Camera *camera, char *path, GPContext *context);
ssize_t send_cmd_hdd_rename(Camera *camera, char *src, char *dst, GPContext *context);
ssize_t send_cmd_hdd_create_dir(Camera *camera, char *path, GPContext *context);

ssize_t get_tf_packet(Camera *camera, struct tf_packet *packet, GPContext *context);
ssize_t send_tf_packet(Camera *camera, struct tf_packet *packet, GPContext *context);

#endif /* _USB_IO_H */
