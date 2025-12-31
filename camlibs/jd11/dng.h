/*
 * Jenopt JD11 Camera Driver
 * Copyright 2025 Nicholas Sherlock <n.sherlock@gmail.com>
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

#ifndef CAMLIBS_JD11_DNG_H
#define CAMLIBS_JD11_DNG_H

#include <stdint.h>
#include <stddef.h>

#define CFA_R 0
#define CFA_G 1
#define CFA_B 2

int write_dng_cfa8_to_memory(
    const uint8_t *bayer,
    int width, int height,
    const uint8_t cfa_pattern[4],
    uint8_t black_level, uint8_t white_level,
    const char *make, const char *model, const char *unique_model,
    int thumb_shift,
    uint8_t **out_buf, size_t *out_size);

#endif /* !defined(CAMLIBS_JD11_DNG_H) */
