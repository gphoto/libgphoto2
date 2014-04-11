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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "usb_io.h"
#include "tf_bytes.h"
#include "crc16.h"

/* The Topfield packet handling is a bit unusual. All data is stored in
 * memory in big endian order, however, just prior to transmission all
 * data is byte swapped.
 *
 * Functions to read and write the memory version of packets are provided
 * in tf_bytes.c
 *
 * Routines here take care of CRC generation, byte swapping and packet
 * transmission.
 */

/* Swap the odd and even bytes in the buffer, up to count bytes.
 * If count is odd, the last byte remains unafected.
 */
static void byte_swap(unsigned char * d, int count)
{
    int i;

    for(i = 0; i < (count & ~1); i += 2)
    {
        unsigned char t = d[i];

        d[i] = d[i + 1];
        d[i + 1] = t;
    }
}

/* Byte swap an incoming packet. */
static void swap_in_packet(struct tf_packet *packet)
{
    int size = (get_u16_raw(packet) + 1) & ~1;

    if(size > MAXIMUM_PACKET_SIZE)
    {
        size = MAXIMUM_PACKET_SIZE;
    };

    byte_swap((unsigned char *) packet, size);
}

/* Byte swap an outgoing packet. */
static void swap_out_packet(struct tf_packet *packet)
{
    int size = (get_u16(packet) + 1) & ~1;

    byte_swap((unsigned char *) packet, size);
}

static const unsigned char cancel_packet[8] = {
    0x08, 0x00, 0x40, 0x01, 0x00, 0x00, 0x03, 0x00
};

static const unsigned char success_packet[8] = {
    0x08, 0x00, 0x81, 0xc1, 0x00, 0x00, 0x02, 0x00
};

/* Optimised packet handling to reduce overhead during bulk file transfers. */
ssize_t send_cancel(Camera *camera,GPContext *context)
{
    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    return gp_port_write (camera->port, (char *)cancel_packet, 8);
}

ssize_t send_success(Camera *camera, GPContext *context)
{
    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    return gp_port_write (camera->port, (char *)success_packet, 8);
}

ssize_t send_cmd_ready(Camera *camera, GPContext *context)
{
    struct tf_packet req;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    put_u16(&req.length, 8);
    put_u32(&req.cmd, CMD_READY);
    return send_tf_packet(camera, &req, context);
}

ssize_t send_cmd_reset(Camera *camera, GPContext *context)
{
    struct tf_packet req;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    put_u16(&req.length, 8);
    put_u32(&req.cmd, CMD_RESET);
    return send_tf_packet(camera, &req, context);
}

ssize_t send_cmd_turbo(Camera *camera, int turbo_on, GPContext *context)
{
    struct tf_packet req;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    put_u16(&req.length, 12);
    put_u32(&req.cmd, CMD_TURBO);
    put_u32(&req.data, turbo_on);
    return send_tf_packet(camera, &req, context);
}

ssize_t send_cmd_hdd_size(Camera *camera, GPContext *context)
{
    struct tf_packet req;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    put_u16(&req.length, 8);
    put_u32(&req.cmd, CMD_HDD_SIZE);
    return send_tf_packet(camera, &req, context);
}

static unsigned short get_crc(struct tf_packet * packet)
{
    return crc16_ansi(&(packet->cmd), get_u16(&packet->length) - 4);
}

ssize_t send_cmd_hdd_dir(Camera *camera, char *path, GPContext *context)
{
    struct tf_packet req;
    unsigned short packetSize;
    int pathLen = strlen(path) + 1;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);

    if((PACKET_HEAD_SIZE + pathLen) >= MAXIMUM_PACKET_SIZE)
    {
        fprintf(stderr, "ERROR: Path is too long.\n");
        return -1;
    }

    packetSize = PACKET_HEAD_SIZE + pathLen;
    packetSize = (packetSize + 1) & ~1;
    put_u16(&req.length, packetSize);
    put_u32(&req.cmd, CMD_HDD_DIR);
    strcpy((char *) req.data, path);
    return send_tf_packet(camera, &req,context);
}

ssize_t send_cmd_hdd_file_send(Camera *camera, unsigned char dir, char *path, GPContext *context)
{
    struct tf_packet req;
    unsigned short packetSize;
    int pathLen = strlen(path) + 1;

    gp_log (GP_LOG_DEBUG, "topfield", "send_cmd_hdd_file_send(dir = %d, path = %s)", dir, path);
    if((PACKET_HEAD_SIZE + 1 + 2 + pathLen) >= MAXIMUM_PACKET_SIZE) {
        fprintf(stderr, "ERROR: Path is too long.\n");
        return -1;
    }

    packetSize = PACKET_HEAD_SIZE + 1 + 2 + pathLen;
    packetSize = (packetSize + 1) & ~1;
    put_u16(&req.length, packetSize);
    put_u32(&req.cmd, CMD_HDD_FILE_SEND);
    req.data[0] = dir;
    put_u16(&req.data[1], pathLen);
    strcpy((char *) &req.data[3], path);
    return send_tf_packet(camera, &req, context);
}

ssize_t send_cmd_hdd_del(Camera *camera, char *path, GPContext *context)
{
    struct tf_packet req;
    unsigned short packetSize;
    int pathLen = strlen(path) + 1;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    if((PACKET_HEAD_SIZE + pathLen) >= MAXIMUM_PACKET_SIZE)
    {
        fprintf(stderr, "ERROR: Path is too long.\n");
        return -1;
    }

    packetSize = PACKET_HEAD_SIZE + pathLen;
    packetSize = (packetSize + 1) & ~1;
    put_u16(&req.length, packetSize);
    put_u32(&req.cmd, CMD_HDD_DEL);
    strcpy((char *) req.data, path);
    return send_tf_packet(camera, &req, context);
}

ssize_t send_cmd_hdd_rename(Camera *camera, char *src, char *dst, GPContext *context)
{
    struct tf_packet req;
    unsigned short packetSize;
    unsigned short srcLen = strlen(src) + 1;
    unsigned short dstLen = strlen(dst) + 1;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    if((PACKET_HEAD_SIZE + 2 + srcLen + 2 + dstLen) >= MAXIMUM_PACKET_SIZE)
    {
        fprintf(stderr,
                "ERROR: Combination of source and destination paths is too long.\n");
        return -1;
    }

    packetSize = PACKET_HEAD_SIZE + 2 + srcLen + 2 + dstLen;
    packetSize = (packetSize + 1) & ~1;
    put_u16(&req.length, packetSize);
    put_u32(&req.cmd, CMD_HDD_RENAME);
    put_u16(&req.data[0], srcLen);
    strcpy((char *) &req.data[2], src);
    put_u16(&req.data[2 + srcLen], dstLen);
    strcpy((char *) &req.data[2 + srcLen + 2], dst);
    return send_tf_packet(camera, &req, context);
}

ssize_t send_cmd_hdd_create_dir(Camera *camera, char *path, GPContext *context)
{
    struct tf_packet req;
    unsigned short packetSize;
    unsigned short pathLen = strlen(path) + 1;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    if((PACKET_HEAD_SIZE + 2 + pathLen) >= MAXIMUM_PACKET_SIZE)
    {
        fprintf(stderr, "ERROR: Path is too long.\n");
        return -1;
    }

    packetSize = PACKET_HEAD_SIZE + 2 + pathLen;
    packetSize = (packetSize + 1) & ~1;
    put_u16(&req.length, packetSize);
    put_u32(&req.cmd, CMD_HDD_CREATE_DIR);
    put_u16(&req.data[0], pathLen);
    strcpy((char *) &req.data[2], path);
    return send_tf_packet(camera, &req, context);
}

/* Given a Topfield protocol packet, this function will calculate the required
 * CRC and send the packet out over a bulk pipe. */
ssize_t send_tf_packet(Camera *camera, struct tf_packet *packet, GPContext *context)
{
    unsigned int pl = get_u16(&packet->length);
    ssize_t byte_count = (pl + 1) & ~1;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    put_u16(&packet->crc, get_crc(packet));
    swap_out_packet(packet);
    return gp_port_write (camera->port, (char *) packet, byte_count);
}

/* Receive a Topfield protocol packet.
 * Returns a negative number if the packet read failed for some reason.
 */
ssize_t get_tf_packet(Camera *camera, struct tf_packet * packet, GPContext *context)
{
    unsigned char *buf = (unsigned char *) packet;
    int r;

    gp_log (GP_LOG_DEBUG, "topfield", __func__);
    r = gp_port_read (camera->port, (char *)buf, MAXIMUM_PACKET_SIZE);
    if(r < 0)
        return r;

    if(r < PACKET_HEAD_SIZE) {
        gp_log (GP_LOG_DEBUG, "topfield", "Short read. %d bytes\n", r);
        return -1;
    }

    /* Send SUCCESS as soon as we see a data transfer packet */
    if(DATA_HDD_FILE_DATA == get_u32_raw(&packet->cmd))
        send_success(camera,context);

    swap_in_packet(packet);

    {
        unsigned short crc;
        unsigned short calc_crc;
        unsigned short len = get_u16(&packet->length);

        if(len < PACKET_HEAD_SIZE) {
            gp_log (GP_LOG_DEBUG, "topfield", "Invalid packet length %04x\n", len);
            return -1;
        }

        crc = get_u16(&packet->crc);
        calc_crc = get_crc(packet);

        /* Complain about CRC mismatch */
        if(crc != calc_crc)
            gp_log (GP_LOG_ERROR, "topfield", "WARNING: Packet CRC %04x, expected %04x\n", crc, calc_crc);
    }
    return r;
}
