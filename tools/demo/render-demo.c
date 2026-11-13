/*
 * Offline demo renderer for obs-beauty-filter.
 *
 * Renders a still image through the real plugin on a real libobs graphics
 * context so the README before/after and slider GIF are genuine filter output
 * rather than a mock-up.  Frames are written as binary PPM (P6) files that
 * `tools/demo/build-assets.sh` composites with ImageMagick and ffmpeg.
 *
 * Usage:
 *   beauty-demo-render <plugin.so> <plugin-data-dir> <image-source.so> \
 *       <image-source-data-dir> <image.ppm> <out-dir> [options]
 *
 * Options:
 *   --width N / --height N   Render size (must match the input image).
 *   --amount F               Smoothing amount (0..1).
 *   --radius F               Radius in pixels at 1080p (0..24).
 *   --detail F               Detail preservation (0..1).
 *   --light F                Automatic light balance (0..1).
 *   --rosy F                 Rosy tint (0..1).
 *   --clarity F              Clarity on non-skin areas (0..1).
 *   --teeth F / --eyes F     Teeth whitening / eye brightening (0..1).
 *   --settle N               Frames rendered before each capture (default 10).
 *   --sweep NAME=VALUES      Capture one frame per value, e.g.
 *                            --sweep sweep=0,0.15,0.3,0.45,0.6,0.75
 *                            (the value is applied to --sweep-property,
 *                            default "amount").
 *   --sweep-property NAME    Property swept by --sweep (default "amount").
 *   --flip-rows              Reverse row order while writing (OpenGL readback
 *                            is bottom-up; D3D11 is top-down).
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <obs.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
	fprintf(stderr, "FAIL: %s\n", message);
	return 1;
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
/* Frame capture                                                             */
/* ------------------------------------------------------------------------- */

static bool capture(obs_source_t *source, uint32_t width, uint32_t height, const char *path,
		    bool flip_rows)
{
	bool ok = false;
	uint8_t *frame = NULL;
	uint32_t linesize = 0;

	obs_enter_graphics();

	gs_texrender_t *render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t *stage = NULL;

	if (render && gs_texrender_begin(render, width, height)) {
		struct vec4 clear_color;
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 1.0f);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

		obs_source_video_render(source);
		gs_texrender_end(render);

		gs_texture_t *texture = gs_texrender_get_texture(render);
		stage = gs_stagesurface_create(width, height, GS_RGBA);

		if (texture && stage) {
			uint8_t *pixels = NULL;

			gs_stage_texture(stage, texture);
			if (gs_stagesurface_map(stage, &pixels, &linesize) &&
			    linesize >= width * 4) {
				frame = malloc((size_t)linesize * height);
				if (frame) {
					memcpy(frame, pixels, (size_t)linesize * height);
					ok = true;
				}
				gs_stagesurface_unmap(stage);
			}
		}
	}

	gs_stagesurface_destroy(stage);
	gs_texrender_destroy(render);
	obs_leave_graphics();

	if (ok) {
		FILE *file = fopen(path, "wb");
		if (!file) {
			free(frame);
			return false;
		}
		fprintf(file, "P6\n%u %u\n255\n", width, height);
		for (uint32_t row = 0; row < height; row++) {
			const uint32_t y = flip_rows ? height - 1 - row : row;
			const uint8_t *line = frame + (size_t)y * linesize;
			for (uint32_t x = 0; x < width; x++)
				fwrite(line + (size_t)x * 4, 1, 3, file);
		}
		fclose(file);
	}

	free(frame);
	return ok;
}

/* ------------------------------------------------------------------------- */
/* Arguments                                                                 */
/* ------------------------------------------------------------------------- */

struct options {
	double amount;
	double radius;
	double detail;
	double light;
	double rosy;
	double clarity;
	double teeth;
	double eyes;
	int settle;
	bool flip_rows;
	const char *sweep_name;
	const char *sweep_values;
};

static void set_defaults(obs_data_t *settings, const struct options *opt)
{
	obs_data_set_string(settings, "preset", "custom");
	obs_data_set_double(settings, "amount", opt->amount);
	obs_data_set_double(settings, "radius", opt->radius);
	obs_data_set_double(settings, "detail", opt->detail);
	obs_data_set_bool(settings, "skin_detect", true);
	obs_data_set_double(settings, "skin_sensitivity", 0.5);
	obs_data_set_bool(settings, "mask_preview", false);
	obs_data_set_double(settings, "brightness", 0.0);
	obs_data_set_double(settings, "warmth", 0.0);
	obs_data_set_double(settings, "saturation", 0.0);
	obs_data_set_double(settings, "rosy", opt->rosy);
	obs_data_set_double(settings, "clarity", opt->clarity);
	obs_data_set_double(settings, "glow", 0.0);
	obs_data_set_double(settings, "light_balance", opt->light);
	obs_data_set_bool(settings, "face_aware", true);
	obs_data_set_double(settings, "temporal_strength", 0.6);
	obs_data_set_double(settings, "teeth_whiten", opt->teeth);
	obs_data_set_double(settings, "eye_brighten", opt->eyes);
	obs_data_set_bool(settings, "zone_preview", false);
	obs_data_set_bool(settings, "face_manual", false);
	obs_data_set_double(settings, "face_cx", 0.5);
	obs_data_set_double(settings, "face_cy", 0.45);
	obs_data_set_double(settings, "face_w", 0.45);
	obs_data_set_double(settings, "face_h", 0.6);
	obs_data_set_string(settings, "quality", "balanced");
	obs_data_set_bool(settings, "split_view", false);
	obs_data_set_double(settings, "opacity", 1.0);
	obs_data_set_bool(settings, "anti_banding", true);
}

int main(int argc, char **argv)
{
	if (argc < 7) {
		fprintf(stderr,
			"Usage: %s <plugin.so> <plugin-data> <image-source.so> "
			"<image-source-data> <image> <out-dir> [options]\n",
			argv[0]);
		return 2;
	}

	struct options opt = {
		.amount = 0.55,
		.radius = 8.0,
		.detail = 0.35,
		.light = 0.35,
		.rosy = 0.12,
		.clarity = 0.08,
		.teeth = 0.0,
		.eyes = 0.0,
		.settle = 10,
		.flip_rows = false,
		.sweep_name = "amount",
		.sweep_values = NULL,
	};
	uint32_t width = 1024;
	uint32_t height = 683;

	for (int i = 7; i < argc; i++) {
		const char *arg = argv[i];
		const char *value = i + 1 < argc ? argv[i + 1] : NULL;

#define NEED_VALUE()                                                                     \
	do {                                                                             \
		if (!value) {                                                             \
			fprintf(stderr, "Missing value for %s\n", arg);                   \
			return 2;                                                         \
		}                                                                         \
		i++;                                                                      \
	} while (0)

		if (strcmp(arg, "--width") == 0) {
			NEED_VALUE();
			width = (uint32_t)atoi(value);
		} else if (strcmp(arg, "--height") == 0) {
			NEED_VALUE();
			height = (uint32_t)atoi(value);
		} else if (strcmp(arg, "--amount") == 0) {
			NEED_VALUE();
			opt.amount = atof(value);
		} else if (strcmp(arg, "--radius") == 0) {
			NEED_VALUE();
			opt.radius = atof(value);
		} else if (strcmp(arg, "--detail") == 0) {
			NEED_VALUE();
			opt.detail = atof(value);
		} else if (strcmp(arg, "--light") == 0) {
			NEED_VALUE();
			opt.light = atof(value);
		} else if (strcmp(arg, "--rosy") == 0) {
			NEED_VALUE();
			opt.rosy = atof(value);
		} else if (strcmp(arg, "--clarity") == 0) {
			NEED_VALUE();
			opt.clarity = atof(value);
		} else if (strcmp(arg, "--teeth") == 0) {
			NEED_VALUE();
			opt.teeth = atof(value);
		} else if (strcmp(arg, "--eyes") == 0) {
			NEED_VALUE();
			opt.eyes = atof(value);
		} else if (strcmp(arg, "--settle") == 0) {
			NEED_VALUE();
			opt.settle = atoi(value);
		} else if (strcmp(arg, "--sweep") == 0) {
			NEED_VALUE();
			opt.sweep_values = value;
		} else if (strcmp(arg, "--sweep-property") == 0) {
			NEED_VALUE();
			opt.sweep_name = value;
		} else if (strcmp(arg, "--flip-rows") == 0) {
			opt.flip_rows = true;
		} else {
			fprintf(stderr, "Unknown option %s\n", arg);
			return 2;
		}
#undef NEED_VALUE
	}

	if (!obs_startup("en-US", NULL, NULL))
		return fail("obs_startup");

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
	ovi.base_width = width;
	ovi.base_height = height;
	ovi.output_width = width;
	ovi.output_height = height;
	ovi.output_format = VIDEO_FORMAT_BGRA;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BICUBIC;

	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return fail("obs_reset_video");

	if (!load_module(argv[3], argv[4]))
		return fail("image-source module");
	if (!load_module(argv[1], argv[2]))
		return fail("beauty-filter module");

	obs_data_t *image_settings = obs_data_create();
	obs_data_set_string(image_settings, "file", argv[5]);
	obs_source_t *image =
		obs_source_create("image_source", "demo-photo", image_settings, NULL);
	obs_data_release(image_settings);
	if (!source_valid(image))
		return fail("image_source creation (is the file a supported image?)");

	char path[1024];

	snprintf(path, sizeof(path), "%s/before.ppm", argv[6]);
	if (!capture(image, width, height, path, opt.flip_rows))
		return fail("render before frame");

	obs_data_t *settings = obs_data_create();
	set_defaults(settings, &opt);
	obs_source_t *filter = obs_source_create("beauty_filter", "demo-filter", settings, NULL);
	obs_data_release(settings);
	if (!source_valid(filter))
		return fail("beauty filter creation");

	obs_source_filter_add(image, filter);

	/* Let the temporal mask and the light balance measurement settle. */
	for (int i = 0; i < opt.settle; i++)
		(void)capture(image, width, height, "/dev/null", opt.flip_rows);

	snprintf(path, sizeof(path), "%s/after.ppm", argv[6]);
	if (!capture(image, width, height, path, opt.flip_rows))
		return fail("render after frame");

	if (opt.sweep_values) {
		char *copy = strdup(opt.sweep_values);
		char *save = NULL;
		int index = 0;

		for (char *token = strtok_r(copy, ",", &save); token;
		     token = strtok_r(NULL, ",", &save)) {
			obs_data_t *update = obs_data_create();
			obs_data_set_double(update, opt.sweep_name, atof(token));
			obs_source_update(filter, update);
			obs_data_release(update);

			for (int i = 0; i < opt.settle; i++)
				(void)capture(image, width, height, "/dev/null", opt.flip_rows);

			snprintf(path, sizeof(path), "%s/sweep-%03d.ppm", argv[6], index++);
			if (!capture(image, width, height, path, opt.flip_rows))
				return fail("render sweep frame");
		}

		free(copy);
		printf("rendered %d sweep frames of %s\n", index, opt.sweep_name);
	}

	printf("demo frames at %ux%u: before=%s/after.ppm\n", width, height, argv[6]);

	obs_source_filter_remove(image, filter);
	obs_source_release(filter);
	obs_source_release(image);
	obs_shutdown();
	return 0;
}