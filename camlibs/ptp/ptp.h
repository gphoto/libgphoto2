/* ptp.h
 *
 * Copyright (C) 2001 Mariusz Woloszyn <emsi@ipartners.pl>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __PTP_H__
#define __PTP_H__

typedef enum _PTPResult PTPResult;
enum _PTPResult {
	PTP_OK,
	PTP_ERROR
};

typedef PTPResult (* PTPIOReadFunc)  (unsigned char *bytes,
				      unsigned int size, void *data);
typedef PTPResult (* PTPIOWriteFunc) (unsigned char *bytes,
				      unsigned int size, void *data);

typedef struct _PTPParams PTPParams;
struct _PTPParams {
	PTPIOReadFunc io_read;
	PTPIOWriteFunc io_write;
	void *io_data;
};

PTPResult ptp_do_something (PTPParams params, unsigned char c1,
			    unsigned char *c2);

#endif /* __PTP_H__ */
