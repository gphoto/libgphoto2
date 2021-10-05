/* ptpip-private.h
 *
 * Copyright (C) 2020 Daniel Schulte <trilader@schroedingers-bit.net>
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

#ifndef CAMLIBS_PTP2_PTPIP_PRIVATE_H
#define CAMLIBS_PTP2_PTPIP_PRIVATE_H

/* Definitions for PTP/IP to work with WinSock and regular BSD-style sockets */
#ifdef WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# define PTPSOCK_SOCKTYPE SOCKET
# define PTPSOCK_INVALID INVALID_SOCKET
# define PTPSOCK_CLOSE closesocket
# define PTPSOCK_ERR SOCKET_ERROR
# define PTPSOCK_READ(fd, buf, size) recv((fd), ((char*)(buf)), (size), 0)
# define PTPSOCK_WRITE(fd, buf, size) send((fd), ((char*)(buf)), (size), 0)
# define PTPSOCK_PROTO IPPROTO_TCP
#else
# include <sys/socket.h>
# define PTPSOCK_SOCKTYPE int
# define PTPSOCK_INVALID -1
# define PTPSOCK_CLOSE close
# define PTPSOCK_ERR -1
# define PTPSOCK_READ(fd, buf, size) read((fd), (buf), (size))
# define PTPSOCK_WRITE(fd, buf, size) write((fd), (buf), (size))
# define PTPSOCK_PROTO 0
#endif

#include <sys/types.h> /* for ssize_t, size_t */

#define PTPIP_DEFAULT_TIMEOUT_S 2
#define PTPIP_DEFAULT_TIMEOUT_MS 500

int ptpip_connect_with_timeout(int fd, const struct sockaddr *address, socklen_t address_len, int seconds, int milliseconds);
ssize_t ptpip_read_with_timeout(int fd, void *buf, size_t nbytes, int seconds, int milliseconds);
ssize_t ptpip_write_with_timeout(int fd, void *buf, size_t nbytes, int seconds, int milliseconds);
int ptpip_set_nonblock(int fd);
void ptpip_perror(const char *what);
int ptpip_get_socket_error(void);
void ptpip_set_socket_error(int err);

#endif /* !defined(CAMLIBS_PTP2_PTPIP_PRIVATE_H) */
