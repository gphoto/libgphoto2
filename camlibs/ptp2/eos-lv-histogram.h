/* eos-lv-histogram.h
 *
 * Canon EOS GetViewFinderData (0x9153) live-view histogram (segment type 17).
 *
 * Copyright (C) 2026 Chris Lawson <cjlawson02@users.noreply.github.com>
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

#ifndef CAMLIBS_PTP2_EOS_LV_HISTOGRAM_H
#define CAMLIBS_PTP2_EOS_LV_HISTOGRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GetViewFinderData multi-segment envelope record type for the exposure histogram. */
#define PTP_CANON_EOS_LV_SEG_HISTOGRAM	17	/* 0x11 */

/*
 * Type-17 payload: planar Y, R, G, B x 256 bins x uint32 LE.
 * Checked on EOS 5D Mark III; FC.SDK documents the same for EOS 6D.
 * Reject unless sum(Y)==sum(R)==sum(G)==sum(B) and sum(Y)>0.
 */
#define PTP_CANON_EOS_LV_HISTOGRAM_BINS		256
#define PTP_CANON_EOS_LV_HISTOGRAM_CHANNELS	4
#ifndef PTP_CANON_EOS_LV_HISTOGRAM_SIZE
#define PTP_CANON_EOS_LV_HISTOGRAM_SIZE		\
	(PTP_CANON_EOS_LV_HISTOGRAM_CHANNELS * PTP_CANON_EOS_LV_HISTOGRAM_BINS * 4)
#endif

/**
 * ptp_canon_eos_parse_lv_histogram:
 * @payload: type-17 payload (no 8-byte segment header)
 * @size:    payload length
 * @out:     PTP_CANON_EOS_LV_HISTOGRAM_SIZE buffer, or NULL to validate only
 *
 * Returns 1 if valid planar YRGB, else 0. Copies to @out when non-NULL.
 */
int ptp_canon_eos_parse_lv_histogram (const unsigned char *payload, unsigned int size,
				      unsigned char out[PTP_CANON_EOS_LV_HISTOGRAM_SIZE]);

/**
 * ptp_canon_eos_extract_lv_histogram:
 * @data: GetViewFinderData multi-segment envelope
 * @size: envelope length
 * @out:  destination buffer, or NULL to validate only
 *
 * Walks [len:u32 LE][type:u32 LE][payload...] records; returns first valid
 * type-17 histogram (1), or 0 if none.
 */
int ptp_canon_eos_extract_lv_histogram (const unsigned char *data, unsigned int size,
					unsigned char out[PTP_CANON_EOS_LV_HISTOGRAM_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* CAMLIBS_PTP2_EOS_LV_HISTOGRAM_H */
