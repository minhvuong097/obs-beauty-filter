/*
 * Unit test for the dependency-free face region estimator.
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "face-region.h"

#include <stdio.h>
#include <string.h>

#define GRID_W 64
#define GRID_H 36

static int failures;

static void fill_rect(uint8_t *grid, int x0, int y0, int x1, int y1, uint8_t value)
{
	int x;
	int y;

	for (y = y0; y <= y1; ++y) {
		for (x = x0; x <= x1; ++x) {
			if (x < 0 || y < 0 || x >= GRID_W || y >= GRID_H)
				continue;
			grid[(size_t)y * GRID_W * 4 + (size_t)x * 4] = value;
		}
	}
}

static void expect(bool condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		failures++;
	}
}

int main(void)
{
	uint8_t grid[GRID_W * GRID_H * 4];
	struct beauty_face_region face;
	struct beauty_face_region full;

	memset(grid, 0, sizeof(grid));
	face = beauty_face_region_detect(grid, GRID_W * 4, GRID_W, GRID_H, 64);
	expect(!face.valid, "empty grid must not produce a face");

	memset(grid, 0, sizeof(grid));
	/* A face is taller than it is wide.  A 14x18 grid cell face at
	 * 64x36 is roughly 22% of the frame width and 50% of its height. */
	fill_rect(grid, 24, 7, 37, 24, 255);  /* face */
	fill_rect(grid, 27, 11, 29, 13, 0);   /* left eye hole */
	fill_rect(grid, 32, 11, 34, 13, 0);   /* right eye hole */
	fill_rect(grid, 28, 19, 33, 20, 0);   /* mouth hole */
	fill_rect(grid, 55, 2, 62, 7, 255);   /* skin-coloured wall patch */

	face = beauty_face_region_detect(grid, GRID_W * 4, GRID_W, GRID_H, 64);
	expect(face.valid, "synthetic face must be detected");
	expect(face.center_x > 0.42f && face.center_x < 0.58f,
	       "face centre X must follow the dominant component");
	expect(face.center_y > 0.20f && face.center_y < 0.70f,
	       "face centre Y must stay in the upper body region");
	expect(face.half_w > 0.10f && face.half_w < 0.35f, "face half width out of range");
	expect(face.half_h > 0.15f && face.half_h < 0.50f, "face half height out of range");

	memset(grid, 0, sizeof(grid));
	fill_rect(grid, 4, 4, 6, 6, 255);
	fill_rect(grid, 40, 20, 42, 22, 255);
	face = beauty_face_region_detect(grid, GRID_W * 4, GRID_W, GRID_H, 64);
	expect(!face.valid, "small isolated specks must not produce a face");

	memset(grid, 0, sizeof(grid));
	fill_rect(grid, 0, 0, GRID_W - 1, GRID_H - 1, 255);
	full = beauty_face_region_detect(grid, GRID_W * 4, GRID_W, GRID_H, 64);
	expect(!full.valid, "a full-frame skin wall must not produce a face box");

	if (failures == 0) {
		printf("OK: face region detection tests passed\n");
		return 0;
	}

	printf("FAILED: %d face region checks\n", failures);
	return 1;
}
