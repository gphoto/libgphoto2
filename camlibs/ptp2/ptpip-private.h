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

/* Definitions for PTP/IP to work with WinSock and regular BSD-style sockets */

#ifdef WIN32
# define PTPSOCK_SOCKTYPE SOCKET
# define PTPSOCK_INVALID INVALID_SOCKET
# define PTPSOCK_CLOSE closesocket
# define PTPSOCK_ERR SOCKET_ERROR
# define PTPSOCK_READ(fd, buf, size) recv((fd), ((char*)(buf)), (size), 0)
# define PTPSOCK_WRITE(fd, buf, size) send((fd), ((char*)(buf)), (size), 0)
# define PTPSOCK_PROTO IPPROTO_TCP
#else
# define PTPSOCK_SOCKTYPE int
# define PTPSOCK_INVALID -1
# define PTPSOCK_CLOSE close
# define PTPSOCK_ERR -1
# define PTPSOCK_READ(fd, buf, size) read((fd), (buf), (size))
# define PTPSOCK_WRITE(fd, buf, size) write((fd), (buf), (size))
# define PTPSOCK_PROTO 0
#endif