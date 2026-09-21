/* eos-lv-histogram.c
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

#include "config.h"

#include "eos-lv-histogram.h"

#include <string.h>

/* Read a little-endian uint32 without depending on ptp-pack helpers. */
static uint32_t
eos_lv_u32le (const unsigned char *p)
{
	return (uint32_t)p[0]
	     | ((uint32_t)p[1] << 8)
	     | ((uint32_t)p[2] << 16)
	     | ((uint32_t)p[3] << 24);
}

static uint64_t
eos_lv_sum_channel (const unsigned char *payload, unsigned int channel)
{
	unsigned int i;
	uint64_t sum = 0;
	const unsigned char *base = payload + channel * PTP_CANON_EOS_LV_HISTOGRAM_BINS * 4;

	for (i = 0; i < PTP_CANON_EOS_LV_HISTOGRAM_BINS; i++)
		sum += eos_lv_u32le (base + i * 4);
	return sum;
}

int
ptp_canon_eos_parse_lv_histogram (const unsigned char *payload, unsigned int size,
				  unsigned char out[PTP_CANON_EOS_LV_HISTOGRAM_SIZE])
{
	uint64_t sum_y, sum_r, sum_g, sum_b;

	if (!payload || size != PTP_CANON_EOS_LV_HISTOGRAM_SIZE)
		return 0;

	sum_y = eos_lv_sum_channel (payload, 0);
	sum_r = eos_lv_sum_channel (payload, 1);
	sum_g = eos_lv_sum_channel (payload, 2);
	sum_b = eos_lv_sum_channel (payload, 3);

	if (sum_y == 0)
		return 0;
	if (sum_y != sum_r || sum_y != sum_g || sum_y != sum_b)
		return 0;

	if (out)
		memcpy (out, payload, PTP_CANON_EOS_LV_HISTOGRAM_SIZE);
	return 1;
}

int
ptp_canon_eos_extract_lv_histogram (const unsigned char *data, unsigned int size,
				    unsigned char out[PTP_CANON_EOS_LV_HISTOGRAM_SIZE])
{
	unsigned int off = 0;

	if (!data)
		return 0;

	while (off + 8 <= size) {
		uint32_t rec_len = eos_lv_u32le (data + off);
		uint32_t rec_type = eos_lv_u32le (data + off + 4);

		if (rec_len < 8)
			return 0;
		if (rec_len > size - off)
			return 0;

		if (rec_type == PTP_CANON_EOS_LV_SEG_HISTOGRAM) {
			if (ptp_canon_eos_parse_lv_histogram (data + off + 8, rec_len - 8, out))
				return 1;
			/* Bad type-17: keep looking. */
		}
		off += rec_len;
	}
	return 0;
}
