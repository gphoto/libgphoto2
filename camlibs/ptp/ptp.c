/* ptp.c
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

#include <stdlib.h>
#include "ptp.h"

PTPResult
ptp_sendreq(PTPParams* params, ptp_req* databuf, short code)
{
	PTPResult ret;
	ptp_req* req=(databuf==NULL)?
		malloc(sizeof(ptp_req)):databuf;
	
	req->len = PTP_REQ_LEN;
	req->type = PTP_TYPE_REQ;
	req->code = code;
	req->trans_id = params->id;
	params->id++;

	ret=params->io_write (req, PTP_REQ_LEN, params->io_data);
	if (databuf==NULL) free (req);
	return ret;
}

