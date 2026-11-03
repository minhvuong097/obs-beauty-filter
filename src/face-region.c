/*
 * OBS Beauty Filter - lightweight face region estimation
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "face-region.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define FACE_GRID_MAX 64
#define FACE_GRID_CELLS (FACE_GRID_MAX * FACE_GRID_MAX)

struct skin_component {
	int min_x;
	int max_x;
	int min_y;
	int max_y;
	int area;
	int sum_x;
	int sum_y;
};

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static float smoothstepf(float edge0, float edge1, float x)
{
	float t;

	if (edge1 <= edge0)
		return x >= edge1 ? 1.0f : 0.0f;

	t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

static uint8_t mask_at(const uint8_t *data, uint32_t linesize, uint32_t x, uint32_t y)
{
	return data[(size_t)y * linesize + (size_t)x * 4u];
}

/*
 * Estimate a face box from one connected skin component.  The upper band of
 * the component is a decent proxy for forehead/cheek width.  The returned
 * score combines size, skin density in that band and proximity to the upper
 * centre of the frame.
 */
static bool component_to_face(const uint8_t *data, uint32_t linesize, uint32_t width,
			      uint32_t height, const struct skin_component *c, uint8_t threshold,
			      struct beauty_face_region *out, float *out_score)
{
	const int comp_w = c->max_x - c->min_x + 1;
	const int comp_h = c->max_y - c->min_y + 1;
	int band_y0;
	int band_y1;
	int band_min_x = width;
	int band_max_x = -1;
	int band_count = 0;
	int x;
	int y;

	if (comp_w < 4 || comp_h < 4)
		return false;

	/* 6%..34% of the component height: forehead and upper cheeks. */
	band_y0 = c->min_y + (int)((float)comp_h * 0.06f);
	band_y1 = c->min_y + (int)((float)comp_h * 0.34f);
	if (band_y1 <= band_y0)
		band_y1 = band_y0 + 1;
	if (band_y1 >= (int)height)
		band_y1 = (int)height - 1;

	for (y = band_y0; y <= band_y1; ++y) {
		for (x = c->min_x; x <= c->max_x; ++x) {
			if (mask_at(data, linesize, (uint32_t)x, (uint32_t)y) < threshold)
				continue;
			band_count++;
			if (x < band_min_x)
				band_min_x = x;
			if (x > band_max_x)
				band_max_x = x;
		}
	}

	if (band_max_x < band_min_x || band_count < 6)
		return false;

	const int band_w = band_max_x - band_min_x + 1;
	const int band_h = band_y1 - band_y0 + 1;

	/* A band this narrow cannot be a face; one this wide is a wall/body. */
	if (band_w < (int)((float)width * 0.04f))
		return false;
	if (band_w > (int)((float)width * 0.80f))
		return false;

	const float density = (float)band_count / (float)(band_w * band_h);
	if (density < 0.22f)
		return false;

	float face_w_px = (float)band_w * 1.10f;
	float face_h_px = face_w_px * 1.35f;
	float center_x_px = ((float)band_min_x + (float)band_max_x + 1.0f) * 0.5f;
	float face_cx = center_x_px / (float)width;

	if (face_cx < 0.08f || face_cx > 0.92f)
		return false;

	if (face_h_px > (float)height * 0.98f)
		face_h_px = (float)height * 0.98f;
	if (face_h_px < (float)height * 0.10f)
		return false;

	const float face_top = (float)c->min_y / (float)height;
	const float face_cy = face_top + face_h_px * 0.5f / (float)height;

	const float density_conf = smoothstepf(0.22f, 0.55f, density);
	const float aspect = (float)band_w / (float)comp_h;
	const float aspect_conf = 0.55f + 0.45f * smoothstepf(0.12f, 0.45f, aspect);
	const float center_conf = 1.0f - 0.70f * fabsf(face_cx - 0.5f);
	const float upper_conf = 1.0f - 0.35f * clampf(face_cy, 0.0f, 1.0f);
	const float score = (float)c->area * density_conf * aspect_conf * center_conf *
			    upper_conf;

	if (score <= 0.0f)
		return false;

	out->center_x = face_cx;
	out->center_y = face_cy;
	out->half_w = face_w_px * 0.5f * 1.12f / (float)width;
	out->half_h = face_h_px * 0.5f * 1.12f / (float)height;
	out->valid = true;

	*out_score = score;
	return true;
}

struct beauty_face_region beauty_face_region_detect(const uint8_t *data, uint32_t linesize,
						     uint32_t width, uint32_t height,
						     uint8_t threshold)
{
	struct beauty_face_region best = {0};
	struct beauty_face_region candidate;
	float best_score = 0.0f;
	uint8_t visited[FACE_GRID_CELLS];
	int stack[FACE_GRID_CELLS];
	uint32_t x;
	uint32_t y;

	if (!data || width < 8 || height < 8 || width > FACE_GRID_MAX ||
	    height > FACE_GRID_MAX)
		return best;
	if (width * height > FACE_GRID_CELLS)
		return best;

	memset(visited, 0, sizeof(visited));

	for (y = 0; y < height; ++y) {
		for (x = 0; x < width; ++x) {
			const uint32_t start = y * width + x;
			struct skin_component comp;
			int stack_size = 0;
			float score;

			if (visited[start] || mask_at(data, linesize, x, y) < threshold)
				continue;

			memset(&comp, 0, sizeof(comp));
			comp.min_x = (int)x;
			comp.max_x = (int)x;
			comp.min_y = (int)y;
			comp.max_y = (int)y;

			visited[start] = 1;
			stack[stack_size++] = (int)start;

			while (stack_size > 0) {
				const int index = stack[--stack_size];
				const int cx = index % (int)width;
				const int cy = index / (int)width;
				static const int dx[4] = {1, -1, 0, 0};
				static const int dy[4] = {0, 0, 1, -1};
				int n;

				comp.area++;
				comp.sum_x += cx;
				comp.sum_y += cy;
				if (cx < comp.min_x)
					comp.min_x = cx;
				if (cx > comp.max_x)
					comp.max_x = cx;
				if (cy < comp.min_y)
					comp.min_y = cy;
				if (cy > comp.max_y)
					comp.max_y = cy;

				for (n = 0; n < 4; ++n) {
					const int nx = cx + dx[n];
					const int ny = cy + dy[n];
					uint32_t ni;

					if (nx < 0 || ny < 0 || nx >= (int)width ||
					    ny >= (int)height)
						continue;
					ni = (uint32_t)ny * width + (uint32_t)nx;
					if (visited[ni])
						continue;
					if (mask_at(data, linesize, (uint32_t)nx,
						    (uint32_t)ny) < threshold)
						continue;
					visited[ni] = 1;
					stack[stack_size++] = (int)ni;
				}
			}

			/* Ignore speckles and tiny patches. */
			if (comp.area < 8)
				continue;

			if (component_to_face(data, linesize, width, height, &comp,
					      threshold, &candidate, &score) &&
			    score > best_score) {
				best = candidate;
				best_score = score;
			}
		}
	}

	return best;
}
