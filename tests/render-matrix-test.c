/*
 * Render matrix / golden-image test for obs-beauty-filter.
 *
 * Usage:
 *   beauty-render-matrix-test <plugin-binary> <module-data-dir>
 *
 * Every built-in preset is rendered at 360p, 720p, 1080p and 1440p.  The
 * test checks resolution-independent invariants (opaque alpha, stable output,
 * finite HDR values) and optionally compares a sampled golden image:
 *
 *   BEAUTY_MATRIX_WRITE_GOLDEN=path  write tests/golden/render-matrix.txt
 *   BEAUTY_MATRIX_GOLDEN=path        compare against that file
 *   BEAUTY_MATRIX_TOLERANCE=n        max per-channel difference (default 20)
 *   BEAUTY_MATRIX_4K=1               also render 2160p (slow on llvmpipe)
 *   BEAUTY_MATRIX_DUMP_DIR=path      write one PPM per preset/resolution
 *
 * The golden image is a grid of average colours rather than a byte-exact
 * frame, so it tolerates GPU/driver rounding while still catching shader or
 * preset regressions.
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "beauty-presets.h"
#include "beauty-settings.h"

#include <obs.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>
#include <util/bmem.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GRID_X 8
#define GRID_Y 6
#define GRID_SAMPLES (GRID_X * GRID_Y)

struct resolution {
	uint32_t width;
	uint32_t height;
	const char *name;
};

static int failures;

static void expect(bool condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		failures++;
	}
}

static bool source_valid(obs_source_t *source)
{
	return source && !obs_obj_invalid(source) && obs_obj_get_data(source);
}

static bool load_module(const char *binary, const char *data_path)
{
	obs_module_t *module = NULL;
	const int result = obs_open_module(&module, binary, data_path);

	if (result != MODULE_SUCCESS) {
		fprintf(stderr, "Cannot open module '%s' (error %d)\n", binary, result);
		return false;
	}
	if (!obs_init_module(module)) {
		fprintf(stderr, "Cannot initialize module '%s'\n", binary);
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------------- */
/* Synthetic test pattern                                                    */
/* ------------------------------------------------------------------------- */

struct pattern_data {
	uint32_t width;
	uint32_t height;
	enum gs_color_space space;
};

static const char *pattern_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Beauty Render Matrix Pattern";
}

static void *pattern_create(obs_data_t *settings, obs_source_t *source)
{
	struct pattern_data *pattern = bzalloc(sizeof(*pattern));
	UNUSED_PARAMETER(source);

	pattern->width = (uint32_t)obs_data_get_int(settings, "width");
	pattern->height = (uint32_t)obs_data_get_int(settings, "height");
	if (!pattern->width)
		pattern->width = 1280;
	if (!pattern->height)
		pattern->height = 720;
	pattern->space = (enum gs_color_space)obs_data_get_int(settings, "space");
	return pattern;
}

static void pattern_destroy(void *data)
{
	bfree(data);
}

static uint32_t pattern_width(void *data)
{
	return ((struct pattern_data *)data)->width;
}

static uint32_t pattern_height(void *data)
{
	return ((struct pattern_data *)data)->height;
}

static enum gs_color_space pattern_color_space(void *data, size_t count,
					       const enum gs_color_space *preferred_spaces)
{
	struct pattern_data *pattern = data;
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);
	return pattern->space;
}

static void solid_rect(gs_eparam_t *color_param, float x, float y, float w, float h, float r,
		       float g, float b)
{
	struct vec4 color;
	vec4_set(&color, r, g, b, 1.0f);
	gs_effect_set_vec4(color_param, &color);

	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_draw_sprite(NULL, 0, (uint32_t)w, (uint32_t)h);
	gs_matrix_pop();
}

static void pattern_render(void *data, gs_effect_t *effect)
{
	struct pattern_data *pattern = data;
	const float w = (float)pattern->width;
	const float h = (float)pattern->height;
	const bool hdr = pattern->space != GS_CS_SRGB;
	const bool previous_srgb = gs_framebuffer_srgb_enabled();
	int i;

	UNUSED_PARAMETER(effect);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	if (pattern->space == GS_CS_SRGB)
		gs_enable_framebuffer_srgb(false);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	if (hdr) {
		/* 4.0 is four SDR whites: a clipping filter cannot preserve it. */
		solid_rect(color, 0.0f, 0.0f, w, h, 4.0f, 0.4f, 0.4f);
	} else {
		/* Dark background with a large skin-tone face area. */
		solid_rect(color, 0.0f, 0.0f, w, h, 0.08f, 0.16f, 0.32f);
		solid_rect(color, w * 0.10f, h * 0.08f, w * 0.80f, h * 0.84f, 200.0f / 255.0f,
			   150.0f / 255.0f, 120.0f / 255.0f);

		/* Fine skin texture: alternating 1% columns across the face. */
		for (i = 0; i < 40; i += 2) {
			solid_rect(color, w * (0.14f + (float)i * 0.018f), h * 0.25f,
				   w * 0.018f, h * 0.30f, 200.0f / 255.0f, 150.0f / 255.0f,
				   120.0f / 255.0f);
			solid_rect(color, w * (0.14f + (float)(i + 1) * 0.018f),
				   h * 0.25f, w * 0.018f, h * 0.30f, 184.0f / 255.0f,
				   136.0f / 255.0f, 110.0f / 255.0f);
		}

		/* Bright highlight, dark patch, saturated patch and grey eyes. */
		solid_rect(color, w * 0.72f, h * 0.10f, w * 0.16f, h * 0.10f, 0.94f, 0.94f,
			   0.92f);
		solid_rect(color, w * 0.72f, h * 0.78f, w * 0.16f, h * 0.10f, 0.02f, 0.02f,
			   0.02f);
		solid_rect(color, w * 0.72f, h * 0.44f, w * 0.16f, h * 0.10f, 0.85f, 0.12f,
			   0.10f);
		solid_rect(color, w * 0.28f, h * 0.40f, w * 0.08f, h * 0.05f, 0.84f, 0.84f,
			   0.84f);
		solid_rect(color, w * 0.48f, h * 0.40f, w * 0.08f, h * 0.05f, 0.84f, 0.84f,
			   0.84f);
		solid_rect(color, w * 0.38f, h * 0.55f, w * 0.10f, h * 0.05f, 0.78f, 0.77f,
			   0.76f);
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_enable_framebuffer_srgb(previous_srgb);
}

static struct obs_source_info matrix_pattern_info = {
	.id = "beauty_matrix_pattern",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_SRGB,
	.get_name = pattern_get_name,
	.create = pattern_create,
	.destroy = pattern_destroy,
	.get_width = pattern_width,
	.get_height = pattern_height,
	.video_render = pattern_render,
	.video_get_color_space = pattern_color_space,
};

/* ------------------------------------------------------------------------- */
/* Readback and sampling                                                     */
/* ------------------------------------------------------------------------- */

struct frame {
	uint32_t width;
	uint32_t height;
	uint32_t linesize;
	uint8_t *data;
};

static void frame_free(struct frame *frame)
{
	if (frame && frame->data)
		bfree(frame->data);
	if (frame)
		memset(frame, 0, sizeof(*frame));
}

static bool render_rgba(obs_source_t *source, uint32_t width, uint32_t height,
			struct frame *out)
{
	bool ok = false;

	memset(out, 0, sizeof(*out));
	obs_enter_graphics();

	gs_texrender_t *render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t *stage = NULL;

	if (render && gs_texrender_begin(render, width, height)) {
		struct vec4 clear_color;
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		vec4_set(&clear_color, 0.0f, 0.0f, 1.0f, 1.0f);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

		obs_source_video_render(source);
		gs_texrender_end(render);

		gs_texture_t *texture = gs_texrender_get_texture(render);
		stage = gs_stagesurface_create(width, height, GS_RGBA);

		if (texture && stage) {
			uint8_t *pixels = NULL;
			uint32_t linesize = 0;

			gs_stage_texture(stage, texture);
			if (gs_stagesurface_map(stage, &pixels, &linesize) && linesize >= width * 4) {
				out->data = bmalloc((size_t)linesize * height);
				if (out->data) {
					memcpy(out->data, pixels, (size_t)linesize * height);
					out->width = width;
					out->height = height;
					out->linesize = linesize;
					ok = true;
				}
				gs_stagesurface_unmap(stage);
			}
		}
	}

	gs_stagesurface_destroy(stage);
	gs_texrender_destroy(render);
	obs_leave_graphics();
	return ok;
}

static float half_to_float(uint16_t h)
{
	const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
	const uint32_t exponent = (h >> 10) & 0x1fu;
	uint32_t mantissa = h & 0x3ffu;
	uint32_t bits;

	if (exponent == 0) {
		if (mantissa == 0) {
			bits = sign;
		} else {
			uint32_t e = 127 - 15 + 1;
			while (!(mantissa & 0x400u)) {
				mantissa <<= 1;
				--e;
			}
			mantissa &= 0x3ffu;
			bits = sign | (e << 23) | (mantissa << 13);
		}
	} else if (exponent == 31) {
		bits = sign | 0x7f800000u | (mantissa << 13);
	} else {
		bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
	}

	float value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

/* Rough image statistics used for the 16F pass. */
struct hdr_stats {
	bool finite;
	bool in_range;
	float max_rgb;
	float min_rgb;
	float max_alpha_error;
};

static bool render_hdr_stats(obs_source_t *source, uint32_t width, uint32_t height,
			     enum gs_color_space space, struct hdr_stats *stats)
{
	bool ok = false;

	memset(stats, 0, sizeof(*stats));
	stats->finite = true;
	stats->in_range = true;

	obs_enter_graphics();

	gs_texrender_t *render = gs_texrender_create(GS_RGBA16F, GS_ZS_NONE);
	gs_stagesurf_t *stage = NULL;

	if (render && gs_texrender_begin_with_color_space(render, width, height, space)) {
		struct vec4 clear_color;
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

		obs_source_video_render(source);
		gs_texrender_end(render);

		gs_texture_t *texture = gs_texrender_get_texture(render);
		stage = gs_stagesurface_create(width, height, GS_RGBA16F);

		if (texture && stage) {
			uint8_t *pixels = NULL;
			uint32_t linesize = 0;

			gs_stage_texture(stage, texture);
			if (gs_stagesurface_map(stage, &pixels, &linesize) &&
			    linesize >= width * 8) {
				uint32_t x, y;

				stats->max_rgb = -1.0f;
				stats->min_rgb = 1e9f;
				for (y = 0; y < height; y++) {
					const uint16_t *row =
						(const uint16_t *)(pixels + (size_t)y * linesize);
					for (x = 0; x < width; x++) {
						const uint16_t *px = row + (size_t)x * 4;
						for (int c = 0; c < 3; c++) {
							const float v = half_to_float(px[c]);
							if (!isfinite(v)) {
								stats->finite = false;
								continue;
							}
							if (v < -0.01f || v > 100.0f)
								stats->in_range = false;
							if (v > stats->max_rgb)
								stats->max_rgb = v;
							if (v < stats->min_rgb)
								stats->min_rgb = v;
						}
						const float alpha = half_to_float(px[3]);
						const float alpha_error = fabsf(alpha - 1.0f);
						if (alpha_error > stats->max_alpha_error)
							stats->max_alpha_error = alpha_error;
					}
				}
				gs_stagesurface_unmap(stage);
				ok = stats->finite;
			}
		}
	}

	gs_stagesurface_destroy(stage);
	gs_texrender_destroy(render);
	obs_leave_graphics();
	return ok;
}

/* Average colour of each cell in an 8x6 grid. */
static void sample_grid(const struct frame *frame, int out[GRID_SAMPLES][3])
{
	int cx, cy;

	for (cy = 0; cy < GRID_Y; cy++) {
		const uint32_t y0 = frame->height * (uint32_t)cy / GRID_Y;
		const uint32_t y1 = frame->height * (uint32_t)(cy + 1) / GRID_Y;

		for (cx = 0; cx < GRID_X; cx++) {
			const uint32_t x0 = frame->width * (uint32_t)cx / GRID_X;
			const uint32_t x1 = frame->width * (uint32_t)(cx + 1) / GRID_X;
			uint64_t sum[3] = {0, 0, 0};
			uint64_t count = 0;
			uint32_t x, y;
			int c;

			for (y = y0; y < y1; y++) {
				const uint8_t *row = frame->data + (size_t)y * frame->linesize;
				for (x = x0; x < x1; x++) {
					const uint8_t *px = row + (size_t)x * 4;
					sum[0] += px[0];
					sum[1] += px[1];
					sum[2] += px[2];
					count++;
				}
			}

			for (c = 0; c < 3; c++)
				out[cy * GRID_X + cx][c] =
					count ? (int)((sum[c] + count / 2) / count) : 0;
		}
	}
}

/* Every pixel must stay opaque: the filter must not punch holes in a source. */
static bool frame_is_opaque(const struct frame *frame)
{
	uint32_t x, y;

	for (y = 0; y < frame->height; y++) {
		const uint8_t *row = frame->data + (size_t)y * frame->linesize;
		for (x = 0; x < frame->width; x++) {
			if (row[(size_t)x * 4 + 3] != 255)
				return false;
		}
	}
	return true;
}

/* Mean absolute RGB difference, sampled every fourth pixel. */
static double frame_difference(const struct frame *a, const struct frame *b)
{
	uint64_t total = 0;
	uint64_t count = 0;
	uint32_t x, y;

	if (a->width != b->width || a->height != b->height)
		return 255.0;

	for (y = 0; y < a->height; y += 4) {
		const uint8_t *row_a = a->data + (size_t)y * a->linesize;
		const uint8_t *row_b = b->data + (size_t)y * b->linesize;
		for (x = 0; x < a->width; x += 4) {
			const uint8_t *px_a = row_a + (size_t)x * 4;
			const uint8_t *px_b = row_b + (size_t)x * 4;
			total += (uint64_t)abs((int)px_a[0] - (int)px_b[0]);
			total += (uint64_t)abs((int)px_a[1] - (int)px_b[1]);
			total += (uint64_t)abs((int)px_a[2] - (int)px_b[2]);
			count += 3;
		}
	}

	return count ? (double)total / (double)count : 0.0;
}

/* ------------------------------------------------------------------------- */
/* Golden image                                                              */
/* ------------------------------------------------------------------------- */

static void skip_comments(FILE *file)
{
	int ch;

	while ((ch = fgetc(file)) == '#') {
		while (ch != EOF && ch != '\n')
			ch = fgetc(file);
	}
	if (ch != EOF)
		ungetc(ch, file);
}

static bool golden_started;

static bool write_golden(const char *path, const struct resolution *res, const char *preset,
			 const int grid[GRID_SAMPLES][3])
{
	FILE *file = fopen(path, golden_started ? "a" : "w");
	int i;

	if (!file)
		return false;

	if (!golden_started) {
		fprintf(file, "# obs-beauty-filter render matrix golden image\n");
		fprintf(file, "# Each record is <width>x<height> <preset> followed by %dx%d RGB "
			      "averages.\n",
			GRID_X, GRID_Y);
	}
	golden_started = true;
	fprintf(file, "%ux%u %s", res->width, res->height, preset);
	for (i = 0; i < GRID_SAMPLES; i++)
		fprintf(file, " %d %d %d", grid[i][0], grid[i][1], grid[i][2]);
	fprintf(file, "\n");
	fclose(file);
	return true;
}

static bool compare_golden(const char *path, const struct resolution *res, const char *preset,
			   const int grid[GRID_SAMPLES][3], int tolerance)
{
	static FILE *file;
	static bool opened;
	static unsigned line;
	int expected[GRID_SAMPLES][3];
	unsigned width, height;
	char found_preset[64];
	int i, c;
	int max_diff = 0;
	double total_diff = 0.0;

	if (!opened) {
		file = fopen(path, "r");
		opened = true;
		if (!file) {
			expect(false, "cannot open golden image");
			return false;
		}
	}
	if (!file)
		return false;

	skip_comments(file);
	if (fscanf(file, "%ux%u %63s", &width, &height, found_preset) != 3) {
		expect(false, "golden image has fewer records than expected");
		return false;
	}
	line++;
	if (width != res->width || height != res->height || strcmp(found_preset, preset) != 0) {
		fprintf(stderr, "FAIL: golden record %u is %ux%u %s, expected %ux%u %s\n", line,
			width, height, found_preset, res->width, res->height, preset);
		failures++;
	}

	for (i = 0; i < GRID_SAMPLES; i++) {
		if (fscanf(file, "%d %d %d", &expected[i][0], &expected[i][1],
			   &expected[i][2]) != 3) {
			expect(false, "truncated golden image record");
			return false;
		}
	}

	for (i = 0; i < GRID_SAMPLES; i++) {
		for (c = 0; c < 3; c++) {
			const int diff = abs(expected[i][c] - grid[i][c]);
			total_diff += diff;
			if (diff > max_diff)
				max_diff = diff;
			if (diff > tolerance) {
				fprintf(stderr,
					"FAIL: %ux%u %s cell %d channel %d is %d, golden %d "
					"(tolerance %d)\n",
					res->width, res->height, preset, i, c, grid[i][c],
					expected[i][c], tolerance);
				failures++;
				return false;
			}
		}
	}

	const double mean = total_diff / (GRID_SAMPLES * 3);
	if (mean > tolerance / 3.0) {
		fprintf(stderr, "FAIL: %ux%u %s mean difference %.1f exceeds %.1f\n",
			res->width, res->height, preset, mean, tolerance / 3.0);
		failures++;
	} else if (max_diff > tolerance / 2) {
		printf("note: %ux%u %s max difference %d, mean %.1f\n", res->width,
		       res->height, preset, max_diff, mean);
	}

	return true;
}

static void dump_ppm(const char *dir, const struct resolution *res, const char *preset,
		     const struct frame *frame)
{
	char path[1024];
	FILE *file;
	uint32_t x, y;

	snprintf(path, sizeof(path), "%s/%s-%ux%u.ppm", dir, preset, res->width, res->height);
	file = fopen(path, "wb");
	if (!file)
		return;

	fprintf(file, "P6\n%u %u\n255\n", frame->width, frame->height);
	for (y = 0; y < frame->height; y++) {
		const uint8_t *row = frame->data + (size_t)y * frame->linesize;
		for (x = 0; x < frame->width; x++) {
			fwrite(row + (size_t)x * 4, 1, 3, file);
		}
	}
	fclose(file);
}

/* ------------------------------------------------------------------------- */
/* Test driver                                                               */
/* ------------------------------------------------------------------------- */

static obs_source_t *make_pattern(uint32_t width, uint32_t height, enum gs_color_space space)
{
	obs_data_t *settings = obs_data_create();
	obs_source_t *source;

	obs_data_set_int(settings, "width", width);
	obs_data_set_int(settings, "height", height);
	obs_data_set_int(settings, "space", space);
	source = obs_source_create("beauty_matrix_pattern", "beauty-matrix-pattern", settings,
				   NULL);
	obs_data_release(settings);

	return source_valid(source) ? source : NULL;
}

static obs_source_t *make_filter(const char *name, const struct beauty_preset *preset)
{
	obs_data_t *settings = obs_data_create();
	obs_source_t *filter;

	beauty_preset_apply(settings, preset);
	obs_data_set_string(settings, S_PRESET, preset->id);
	obs_data_set_bool(settings, S_MASK_PREVIEW, false);
	obs_data_set_bool(settings, S_ZONE_PREVIEW, false);
	obs_data_set_bool(settings, S_SPLIT_VIEW, false);
	obs_data_set_double(settings, S_OPACITY, 1.0);
	/* The automatic light balance depends on a GPU readback, so it is
	 * disabled for the deterministic golden comparison. */
	obs_data_set_double(settings, S_LIGHT_BALANCE, 0.0);
	obs_data_set_bool(settings, S_FACE_MANUAL, false);

	filter = obs_source_create("beauty_filter", name, settings, NULL);
	obs_data_release(settings);
	return source_valid(filter) ? filter : NULL;
}

static int run_matrix(const struct resolution *res, const char *golden_path,
		      const char *dump_dir, int tolerance, bool write_mode)
{
	obs_source_t *pattern = make_pattern(res->width, res->height, GS_CS_SRGB);
	struct frame source_frame;
	size_t i;

	if (!pattern) {
		expect(false, "pattern source creation");
		return 1;
	}
	if (!render_rgba(pattern, res->width, res->height, &source_frame)) {
		obs_source_release(pattern);
		expect(false, "render reference frame");
		return 1;
	}
	for (i = 0; i < beauty_preset_count(); i++) {
		const struct beauty_preset *preset = beauty_preset_at(i);
		struct frame frame;
		int grid[GRID_SAMPLES][3];
		char name[96];

		snprintf(name, sizeof(name), "matrix-%s-%ux%u", preset->id, res->width,
			 res->height);
		obs_source_t *filter = make_filter(name, preset);
		if (!filter) {
			expect(false, "filter creation for preset");
			continue;
		}

		obs_source_filter_add(pattern, filter);
		if (!render_rgba(pattern, res->width, res->height, &frame)) {
			expect(false, "render preset frame");
			obs_source_filter_remove(pattern, filter);
			obs_source_release(filter);
			continue;
		}
		obs_source_filter_remove(pattern, filter);
		obs_source_release(filter);

		expect(frame_is_opaque(&frame), "output alpha must stay opaque");

		sample_grid(&frame, grid);
		const double difference = frame_difference(&frame, &source_frame);
		if (difference < 0.02) {
			fprintf(stderr,
				"FAIL: %s did not change the frame (mean difference %.4f)\n",
				preset->id, difference);
			failures++;
		}

		if (golden_path && write_mode) {
			if (!write_golden(golden_path, res, preset->id, grid)) {
				expect(false, "write golden image");
				frame_free(&frame);
				break;
			}
		} else if (golden_path) {
			compare_golden(golden_path, res, preset->id, grid, tolerance);
		}

		if (dump_dir)
			dump_ppm(dump_dir, res, preset->id, &frame);

		printf("%ux%u %-10s diff %.3f grid[0]=%d,%d,%d\n", res->width, res->height,
		       preset->id, difference, grid[0][0], grid[0][1], grid[0][2]);

		frame_free(&frame);
	}

	obs_source_release(pattern);
	return 0;
}

static void run_hdr_matrix(void)
{
	obs_source_t *pattern = make_pattern(1280, 720, GS_CS_709_EXTENDED);
	size_t i;

	if (!pattern) {
		expect(false, "HDR pattern source creation");
		return;
	}

	for (i = 0; i < beauty_preset_count(); i++) {
		const struct beauty_preset *preset = beauty_preset_at(i);
		struct hdr_stats stats;
		char name[96];

		snprintf(name, sizeof(name), "matrix-hdr-%s", preset->id);
		obs_source_t *filter = make_filter(name, preset);
		if (!filter) {
			expect(false, "HDR filter creation for preset");
			continue;
		}

		obs_source_filter_add(pattern, filter);
		const bool rendered = render_hdr_stats(pattern, 1280, 720, GS_CS_709_EXTENDED,
						       &stats);
		obs_source_filter_remove(pattern, filter);
		obs_source_release(filter);

		expect(rendered, "16F render must produce finite values");
		if (rendered) {
			char message[160];
			snprintf(message, sizeof(message),
				 "16F %s must keep highlights above SDR white", preset->id);
			expect(stats.max_rgb > 1.5f, message);
			snprintf(message, sizeof(message), "16F %s values must stay in range",
				 preset->id);
			expect(stats.in_range, message);
			snprintf(message, sizeof(message),
				 "16F %s must preserve opaque alpha (max error %.3f)",
				 preset->id, stats.max_alpha_error);
			expect(stats.max_alpha_error < 0.01f, message);
			printf("16F %-10s max %.3f min %.3f alpha_err %.4f\n", preset->id,
			       stats.max_rgb, stats.min_rgb, stats.max_alpha_error);
		}
	}

	obs_source_release(pattern);
}

int main(int argc, char **argv)
{
	struct resolution resolutions[5] = {
		{640, 360, "360p"},
		{1280, 720, "720p"},
		{1920, 1080, "1080p"},
		{2560, 1440, "1440p"},
	};
	size_t resolution_count = 4;
	const char *golden_path = getenv("BEAUTY_MATRIX_GOLDEN");
	const char *write_path = getenv("BEAUTY_MATRIX_WRITE_GOLDEN");
	const char *dump_dir = getenv("BEAUTY_MATRIX_DUMP_DIR");
	const char *tolerance_env = getenv("BEAUTY_MATRIX_TOLERANCE");
	const char *fourk = getenv("BEAUTY_MATRIX_4K");
	int tolerance = tolerance_env && *tolerance_env ? atoi(tolerance_env) : 20;
	size_t i;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s <plugin-binary> <module-data-dir>\n", argv[0]);
		return 2;
	}

	if (fourk && *fourk && strcmp(fourk, "0") != 0) {
		resolutions[resolution_count].width = 3840;
		resolutions[resolution_count].height = 2160;
		resolutions[resolution_count].name = "2160p";
		resolution_count++;
	}
	if (tolerance < 1)
		tolerance = 1;

	if (write_path && *write_path)
		remove(write_path);

	if (!obs_startup("en-US", NULL, NULL))
		return 1;

	const char *extra_data_path = getenv("OBS_BEAUTY_DATA_PATH");
	if (extra_data_path && *extra_data_path)
		obs_add_data_path(extra_data_path);

	struct obs_video_info ovi = {0};
#ifdef _WIN32
	ovi.graphics_module = "libobs-d3d11";
#else
	ovi.graphics_module = "libobs-opengl";
#endif
	ovi.fps_num = 30;
	ovi.fps_den = 1;
	ovi.base_width = 1280;
	ovi.base_height = 720;
	ovi.output_width = 1280;
	ovi.output_height = 720;
	ovi.output_format = VIDEO_FORMAT_BGRA;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BICUBIC;

	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return 1;

	obs_register_source(&matrix_pattern_info);

	if (!load_module(argv[1], argv[2]))
		return 1;

	for (i = 0; i < resolution_count; i++)
		run_matrix(&resolutions[i], write_path && *write_path ? write_path : golden_path,
			   dump_dir && *dump_dir ? dump_dir : NULL, tolerance,
			   write_path && *write_path);

	run_hdr_matrix();

	obs_shutdown();

	if (failures) {
		fprintf(stderr, "%d render matrix failure(s)\n", failures);
		return 1;
	}

	printf("OK: render matrix verified %zu resolutions x %zu presets", resolution_count,
	       beauty_preset_count());
	printf(", opaque alpha, 16F range and finite output\n");
	return 0;
}
