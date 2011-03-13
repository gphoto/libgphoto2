/* sierra_usbwrap.h
 *
 * Copyright © 2002 Lutz Müller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#ifndef __SIERRA_USBWRAP_H__
#define __SIERRA_USBWRAP_H__

#include <gphoto2/gphoto2-port.h>

int olympus_wrap_ptp_transaction (PTPParams *params,
        PTPContainer* ptp, uint16_t flags,
        unsigned int sendlen, unsigned char **data, unsigned int *recvlen);

uint16_t olympus_setup (PTPParams *params);

uint16_t ums_wrap_sendreq  (PTPParams* params, PTPContainer* req);
uint16_t ums_wrap_wrap_sendreq  (PTPParams* params, PTPContainer* req);
uint16_t ums_wrap_senddata (PTPParams* params, PTPContainer* ptp, unsigned long sendlen, PTPDataHandler*getter);
uint16_t ums_wrap_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *putter);
uint16_t ums_wrap_getresp(PTPParams* params, PTPContainer* resp);

uint16_t ums_wrap2_sendreq  (PTPParams* params, PTPContainer* req);
uint16_t ums_wrap2_senddata (PTPParams* params, PTPContainer* ptp, unsigned long sendlen, PTPDataHandler*getter);
uint16_t ums_wrap2_getdata (PTPParams* params, PTPContainer* ptp, PTPDataHandler *putter);
uint16_t ums_wrap2_getresp(PTPParams* params, PTPContainer* resp);
uint16_t ums_wrap2_event_check  (PTPParams* params, PTPContainer* req);
uint16_t ums_wrap2_event_wait  (PTPParams* params, PTPContainer* req);

#endif /* __SIERRA_USBWRAP_H__ */
