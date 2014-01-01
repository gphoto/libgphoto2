/* Tenx tp6801 picframe memory dump tool
 *
 *   Copyright (c) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the 
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#define READ 0
#define WRITE 1

void tp6801_send_cmd(int fd, int rw, int cmd, int offset, unsigned char *data, int data_size)
{
    int i;
    sg_io_hdr_t io_hdr;
    unsigned char sense_buffer[32];
    unsigned char cmd_buffer[16];

    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    cmd_buffer[0] = cmd; 
    cmd_buffer[1] = 0x11;
    cmd_buffer[2] = 0x31;
    cmd_buffer[3] = 0x0f;
    cmd_buffer[4] = 0x30;
    cmd_buffer[5] = 0x01; /* cmd 0xca hour not bcd! */
    cmd_buffer[6] = data_size >> 8; /* cmd 0xca min */
    cmd_buffer[7] = data_size & 0xff; /* cmd 0xca sec */
    cmd_buffer[8] = (offset >> 16) & 0xff;
    cmd_buffer[9] = (offset >> 8) & 0xff;
    cmd_buffer[10] = offset & 0xff;

    memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
    if (rw == READ)
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    else
        io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.interface_id = 'S';
    io_hdr.dxfer_len = data_size;
    io_hdr.dxferp = (unsigned char *)data;
    io_hdr.cmdp = cmd_buffer;
    io_hdr.cmd_len = sizeof(cmd_buffer);
    io_hdr.sbp = sense_buffer;
    io_hdr.mx_sb_len = sizeof(sense_buffer);
    io_hdr.timeout = 1000;

    if(ioctl(fd, SG_IO, &io_hdr) < 0)
    {
        printf("error status: %d, errno: %s\n", io_hdr.status, strerror(errno));
        exit (1);
    }
}

void eeprom_read(int fd, int offset, unsigned char *data, int data_size)
{
    tp6801_send_cmd(fd, READ, 0xc1, offset, data, data_size);
}

void eeprom_erase_block(int fd, int offset)
{
    tp6801_send_cmd(fd, READ, 0xc6, offset, NULL, 0);
}

void eeprom_program_page(int fd, int offset, unsigned char *data)
{
    tp6801_send_cmd(fd, WRITE, 0xcb, offset, data, 256);
}

int main(int argc, char* argv[])
{
    int i, j, fd;
    FILE* f;
    unsigned char data[0x10000]; /* 64k */

    fd = open(argv[1], O_RDWR);
    if(fd < 0)
    {
        printf("Cannot open %s: %s\n", argv[1], strerror(errno));
        exit(-1);
    }

#if 1
    f = fopen("dump.bin", "w");
    /* We can read max 32k at a time */
    for(i = 0; i < (4160*1024); i += 0x8000)
    {
        eeprom_read(fd, i, data, 0x8000);
        fwrite(data, 1, 0x8000, f);
    }
#else
    f = fopen("dump.bin", "r");
    /* programming is done in blocks of 64k */
    /* Note this is only restores the 64k block as that often gets
       mangled during development */
    for(i = 0; i < 0x10000; i += 0x10000)
    {
        fread(data, 1, 0x10000, f);
        eeprom_erase_block(fd, i);
        for (j = 0; j < 0x10000; j += 256)
            eeprom_program_page(fd, i + j, data + j);
    }
#endif
    fclose(f);
    close(fd);
    
    return 0;
}
