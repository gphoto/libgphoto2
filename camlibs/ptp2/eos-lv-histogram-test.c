/** \file camlibs/ptp2/eos-lv-histogram-test.c
 * \brief Unit test for EOS LV type-17 histogram parsing (no camera).
 *
 * From camlibs/ptp2 after ./configure:
 *   $ cc -std=c99 -Wall -Wextra -I../.. -I. -o eos-lv-histogram-test \
 *        eos-lv-histogram-test.c eos-lv-histogram.c && ./eos-lv-histogram-test
 *
 * Copyright (C) 2026 Chris Lawson <cjlawson02@users.noreply.github.com>
 *
 * \copyright GNU Lesser General Public License 2 or later
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "eos-lv-histogram.h"

static void
put_u32le (unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)(v & 0xff);
	p[1] = (unsigned char)((v >> 8) & 0xff);
	p[2] = (unsigned char)((v >> 16) & 0xff);
	p[3] = (unsigned char)((v >> 24) & 0xff);
}

static void
fill_matching_histogram (unsigned char *payload)
{
	unsigned int i, ch;

	memset (payload, 0, PTP_CANON_EOS_LV_HISTOGRAM_SIZE);
	/* Put the same non-zero counts in every channel so sums match. */
	for (ch = 0; ch < PTP_CANON_EOS_LV_HISTOGRAM_CHANNELS; ch++) {
		unsigned char *base = payload + ch * PTP_CANON_EOS_LV_HISTOGRAM_BINS * 4;
		put_u32le (base + 0 * 4, 10);
		put_u32le (base + 128 * 4, 20);
		put_u32le (base + 255 * 4, 5);
		for (i = 1; i < 128; i++)
			put_u32le (base + i * 4, 0);
	}
}

static void
append_segment (unsigned char *buf, unsigned int *off, unsigned int cap,
		uint32_t type, const unsigned char *payload, uint32_t payload_len)
{
	uint32_t rec_len = 8 + payload_len;

	assert (*off + rec_len <= cap);
	put_u32le (buf + *off, rec_len);
	put_u32le (buf + *off + 4, type);
	if (payload_len)
		memcpy (buf + *off + 8, payload, payload_len);
	*off += rec_len;
}

int
main (void)
{
	unsigned char hist[PTP_CANON_EOS_LV_HISTOGRAM_SIZE];
	unsigned char out[PTP_CANON_EOS_LV_HISTOGRAM_SIZE];
	unsigned char envelope[8 + 16 + 8 + PTP_CANON_EOS_LV_HISTOGRAM_SIZE];
	unsigned int off = 0;
	unsigned char jpeg_stub[16];
	unsigned char bad[PTP_CANON_EOS_LV_HISTOGRAM_SIZE];

	fill_matching_histogram (hist);

	/* Reject wrong size / empty / mismatched channel sums. */
	assert (!ptp_canon_eos_parse_lv_histogram (hist, 100, out));
	memset (bad, 0, sizeof (bad));
	assert (!ptp_canon_eos_parse_lv_histogram (bad, sizeof (bad), out));
	memcpy (bad, hist, sizeof (bad));
	put_u32le (bad + 0 * 4, 99); /* break Y vs R/G/B sums */
	assert (!ptp_canon_eos_parse_lv_histogram (bad, sizeof (bad), out));

	assert (ptp_canon_eos_parse_lv_histogram (hist, sizeof (hist), out));
	assert (memcmp (hist, out, sizeof (hist)) == 0);
	assert (ptp_canon_eos_parse_lv_histogram (hist, sizeof (hist), NULL));

	/* Envelope: fake JPEG (type 1) then type-17 histogram. */
	memset (jpeg_stub, 0xab, sizeof (jpeg_stub));
	append_segment (envelope, &off, sizeof (envelope), 1, jpeg_stub, sizeof (jpeg_stub));
	append_segment (envelope, &off, sizeof (envelope),
			PTP_CANON_EOS_LV_SEG_HISTOGRAM, hist, sizeof (hist));

	memset (out, 0, sizeof (out));
	assert (ptp_canon_eos_extract_lv_histogram (envelope, off, out));
	assert (memcmp (hist, out, sizeof (hist)) == 0);

	/* Histogram-only envelope. */
	off = 0;
	append_segment (envelope, &off, sizeof (envelope),
			PTP_CANON_EOS_LV_SEG_HISTOGRAM, hist, sizeof (hist));
	assert (ptp_canon_eos_extract_lv_histogram (envelope, off, out));

	/* JPEG only → no histogram. */
	off = 0;
	append_segment (envelope, &off, sizeof (envelope), 1, jpeg_stub, sizeof (jpeg_stub));
	assert (!ptp_canon_eos_extract_lv_histogram (envelope, off, out));

	printf ("eos-lv-histogram-test: OK\n");
	return 0;
}
