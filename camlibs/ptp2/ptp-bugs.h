/* ptp-bugs.h
 *
 * Copyright (C) 2005-2006 Hubert Figuiere <hfiguiere@teaser.fr>
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


#ifndef __PTP_BUGS_H_
#define __PTP_BUGS_H_

#include "device-flags.h"

#define PTPBUG_DELETE_SENDS_EVENT	DEVICE_FLAG_DELETE_SENDS_EVENT
#define PTP_CAP				DEVICE_FLAG_CAPTURE
#define PTP_CAP_PREVIEW			DEVICE_FLAG_CAPTURE_PREVIEW
#define PTP_NIKON_BROKEN_CAP		DEVICE_FLAG_NIKON_BROKEN_CAPTURE
#define PTP_MTP_ZEN_BROKEN_HEADER	DEVICE_FLAG_IGNORE_HEADER_ERRORS
#define PTP_NO_CAPTURE_COMPLETE		DEVICE_FLAG_NO_CAPTURE_COMPLETE
#define PTP_OLYMPUS_XML			DEVICE_FLAG_OLYMPUS_XML_WRAPPED
#define PTP_NIKON_1			DEVICE_FLAG_NIKON_1

#define DELETE_SENDS_EVENT(x) \
  ((x)->device_flags & (DEVICE_FLAG_DELETE_SENDS_EVENT))
#define NIKON_BROKEN_CAP(x) \
  ((x)->device_flags & (DEVICE_FLAG_NIKON_BROKEN_CAPTURE))
#define NIKON_1(x) \
  ((x)->device_flags & (DEVICE_FLAG_NIKON_1))
#define NO_CAPTURE_COMPLETE(x) \
  ((x)->device_flags & (DEVICE_FLAG_NO_CAPTURE_COMPLETE))
#define MTP_ZEN_BROKEN_HEADER(x) \
  ((x)->device_flags & (DEVICE_FLAG_IGNORE_HEADER_ERRORS))

#endif
