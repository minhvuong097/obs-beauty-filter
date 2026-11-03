/*
 * OBS Beauty Filter - lightweight face region estimation
 *
 * The detector works on a small RGBA8 skin-mask grid (typically 64x36)
 * produced on the GPU.  It finds the dominant skin component and derives a
 * face rectangle from the width of its upper band.  This is intentionally a
 * heuristic: it is not a machine-learning landmark detector.
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct beauty_face_region {
	/* Normalised image coordinates, 0..1. */
	float center_x;
	float center_y;
	float half_w;
	float half_h;
	bool valid;
};

/*
 * Detect a face-like region from an RGBA8 skin-mask grid.
 *
 * data       first byte of the top-left pixel (red channel is used)
 * linesize   byte distance between rows
 * width      grid width, 1..64
 * height     grid height, 1..64
 * threshold  0..255; values >= threshold count as skin
 */
struct beauty_face_region beauty_face_region_detect(const uint8_t *data, uint32_t linesize,
						     uint32_t width, uint32_t height,
						     uint8_t threshold);
