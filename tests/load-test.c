/*
 * Standalone load/render test for obs-beauty-filter.
 *
 * Usage:
 *   beauty-load-test <plugin-binary> <module-data-dir> [image-source-binary] [image-source-data-dir]
 *
 * The optional image-source module is used to create sources so the filter
 * render path, mask output, face zones, teeth/eye enhancement, opacity
 * blending and HDR preservation can be checked.  Without it, the test only
 * verifies that the module loads, registers the filter and compiles the
 * effect.
 */

#include <obs.h>
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>

#include <stdio.h>
#include <string.h>

static int fail(const char *message)
{
	fprintf(stderr, "FAIL: %s\n", message);
	return 1;
}

static bool source_valid(obs_source_t *source)
{
	/* OBS keeps an invalid source object to avoid losing user settings. */
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

static bool render_pixel(obs_source_t *source, uint32_t width, uint32_t height, uint32_t x,
			 uint32_t y, uint8_t out[4])
{
	bool ok = false;

	obs_enter_graphics();

	gs_texrender_t *render = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t *stage = NULL;

	if (render && gs_texrender_begin(render, width, height)) {
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		struct vec4 clear_color;
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
			if (gs_stagesurface_map(stage, &pixels, &linesize)) {
				const uint32_t row = y;
				const uint8_t *px = pixels + row * linesize + x * 4;
				memcpy(out, px, 4);
				gs_stagesurface_unmap(stage);
				ok = true;
			}
		}
	}

	gs_stagesurface_destroy(stage);
	gs_texrender_destroy(render);
	obs_leave_graphics();

	return ok;
}

static bool render_center(obs_source_t *source, uint32_t width, uint32_t height, uint8_t out[4])
{
	return render_pixel(source, width, height, width / 2, height / 2, out);
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

/*
 * Render a single pixel from an HDR target.  Used to prove that the filter
 * processes linear HDR data instead of bypassing the source, and that both
 * Rec.709 extended and scRGB (80-nit nominal) keep their scale.
 */
static bool render_pixel_hdr(obs_source_t *source, uint32_t width, uint32_t height, uint32_t x,
			     uint32_t y, enum gs_color_space space, float out[4])
{
	bool ok = false;

	obs_enter_graphics();

	gs_texrender_t *render = gs_texrender_create(GS_RGBA16F, GS_ZS_NONE);
	gs_stagesurf_t *stage = NULL;

	if (render &&
	    gs_texrender_begin_with_color_space(render, width, height, space)) {
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		struct vec4 clear_color;
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
			if (gs_stagesurface_map(stage, &pixels, &linesize)) {
				const uint32_t row = y;
				const uint8_t *px = pixels + row * linesize + x * 8;
				const uint16_t *half = (const uint16_t *)px;
				for (int i = 0; i < 4; ++i)
					out[i] = half_to_float(half[i]);
				gs_stagesurface_unmap(stage);
				ok = true;
			}
		}
	}

	gs_stagesurface_destroy(stage);
	gs_texrender_destroy(render);
	obs_leave_graphics();

	return ok;
}

/* ------------------------------------------------------------------------- */
/* Synthetic test pattern source                                             */
/* ------------------------------------------------------------------------- */

#define PATTERN_WIDTH 1280
#define PATTERN_HEIGHT 720

struct pattern_data {
	enum gs_color_space space;
};

static const char *pattern_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Beauty Test Pattern";
}

static void *pattern_create(obs_data_t *settings, obs_source_t *source)
{
	struct pattern_data *pattern = bzalloc(sizeof(*pattern));
	UNUSED_PARAMETER(source);
	pattern->space = (enum gs_color_space)obs_data_get_int(settings, "space");
	if (pattern->space == GS_CS_SRGB && obs_data_get_bool(settings, "hdr"))
		pattern->space = GS_CS_709_EXTENDED;
	return pattern;
}

static void pattern_destroy(void *data)
{
	bfree(data);
}

static uint32_t pattern_width(void *data)
{
	UNUSED_PARAMETER(data);
	return PATTERN_WIDTH;
}

static uint32_t pattern_height(void *data)
{
	UNUSED_PARAMETER(data);
	return PATTERN_HEIGHT;
}

static enum gs_color_space pattern_color_space(void *data, size_t count,
					       const enum gs_color_space *preferred_spaces)
{
	struct pattern_data *pattern = data;
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);
	return pattern->space;
}

static void pattern_solid_rect(gs_eparam_t *color_param, float x, float y, float w, float h,
			       float r, float g, float b)
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
	UNUSED_PARAMETER(effect);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
	const bool previous_srgb = gs_framebuffer_srgb_enabled();

	/* The SDR pattern supplies display-referred values; keep the write path
	 * raw so the filter sees exactly the colours described below. */
	if (pattern->space == GS_CS_SRGB)
		gs_enable_framebuffer_srgb(false);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	if (pattern->space != GS_CS_SRGB) {
		/* 4.0 is four times SDR white.  A filter that bypasses or
		 * clamps HDR cannot preserve it. */
		pattern_solid_rect(color, 0.0f, 0.0f, PATTERN_WIDTH, PATTERN_HEIGHT, 4.0f,
				   0.4f, 0.4f);
	} else {
		pattern_solid_rect(color, 0.0f, 0.0f, PATTERN_WIDTH, PATTERN_HEIGHT, 0.08f,
				   0.16f, 0.32f);
		pattern_solid_rect(color, 400.0f, 100.0f, 480.0f, 520.0f, 200.0f / 255.0f,
				   150.0f / 255.0f, 120.0f / 255.0f);
		pattern_solid_rect(color, 520.0f, 280.0f, 60.0f, 40.0f, 215.0f / 255.0f,
				   215.0f / 255.0f, 215.0f / 255.0f);
		pattern_solid_rect(color, 700.0f, 280.0f, 60.0f, 40.0f, 215.0f / 255.0f,
				   215.0f / 255.0f, 215.0f / 255.0f);
		pattern_solid_rect(color, 600.0f, 470.0f, 80.0f, 40.0f, 200.0f / 255.0f,
				   198.0f / 255.0f, 195.0f / 255.0f);
	}

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_enable_framebuffer_srgb(previous_srgb);
}

static struct obs_source_info test_pattern_info = {
	.id = "beauty_test_pattern",
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
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static int abs_int(int value)
{
	return value < 0 ? -value : value;
}

static int luma8(const uint8_t px[4])
{
	return (px[0] * 299 + px[1] * 587 + px[2] * 114) / 1000;
}

static bool color_near(const uint8_t px[4], int r, int g, int b, int tolerance)
{
	return px[3] >= 250 && abs_int((int)px[0] - r) <= tolerance &&
	       abs_int((int)px[1] - g) <= tolerance && abs_int((int)px[2] - b) <= tolerance;
}

static void set_test_defaults(obs_data_t *settings)
{
	obs_data_set_double(settings, "amount", 0.35);
	obs_data_set_double(settings, "radius", 6.0);
	obs_data_set_double(settings, "detail", 0.35);
	obs_data_set_bool(settings, "skin_detect", true);
	obs_data_set_double(settings, "skin_sensitivity", 0.50);
	obs_data_set_bool(settings, "mask_preview", false);
	obs_data_set_double(settings, "brightness", 0.0);
	obs_data_set_double(settings, "warmth", 0.0);
	obs_data_set_double(settings, "saturation", 0.0);
	obs_data_set_double(settings, "rosy", 0.15);
	obs_data_set_double(settings, "clarity", 0.10);
	obs_data_set_double(settings, "glow", 0.05);
	obs_data_set_bool(settings, "face_aware", true);
	obs_data_set_double(settings, "temporal_strength", 0.60);
	obs_data_set_double(settings, "teeth_whiten", 0.0);
	obs_data_set_double(settings, "eye_brighten", 0.0);
	obs_data_set_bool(settings, "zone_preview", false);
	obs_data_set_bool(settings, "face_manual", false);
	obs_data_set_double(settings, "face_cx", 0.50);
	obs_data_set_double(settings, "face_cy", 0.50);
	obs_data_set_double(settings, "face_w", 0.375);
	obs_data_set_double(settings, "face_h", 0.722);
	obs_data_set_string(settings, "quality", "balanced");
	obs_data_set_bool(settings, "split_view", false);
	obs_data_set_double(settings, "opacity", 1.0);
}

static void set_neutral_processing(obs_data_t *settings)
{
	obs_data_set_double(settings, "amount", 0.0);
	obs_data_set_double(settings, "detail", 0.0);
	obs_data_set_double(settings, "brightness", 0.0);
	obs_data_set_double(settings, "warmth", 0.0);
	obs_data_set_double(settings, "saturation", 0.0);
	obs_data_set_double(settings, "rosy", 0.0);
	obs_data_set_double(settings, "clarity", 0.0);
	obs_data_set_double(settings, "glow", 0.0);
	obs_data_set_bool(settings, "split_view", false);
	obs_data_set_double(settings, "opacity", 1.0);
}

static obs_source_t *make_filter(const char *name, obs_data_t *settings)
{
	obs_source_t *filter = obs_source_create("beauty_filter", name, settings, NULL);
	if (settings)
		obs_data_release(settings);
	if (!source_valid(filter)) {
		if (filter)
			obs_source_release(filter);
		return NULL;
	}
	return filter;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <plugin-binary> <module-data-dir> "
				"[image-source-binary] [image-source-data-dir]\n",
			argv[0]);
		return 2;
	}

	if (!obs_startup("en-US", NULL, NULL))
		return fail("obs_startup");

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
		return fail("obs_reset_video");

	obs_register_source(&test_pattern_info);

	if (!load_module(argv[1], argv[2]))
		return 1;

	obs_data_t *filter_settings = obs_data_create();
	obs_source_t *filter =
		obs_source_create("beauty_filter", "beauty-filter-test", filter_settings, NULL);
	obs_data_release(filter_settings);
	if (!source_valid(filter)) {
		if (filter)
			obs_source_release(filter);
		return fail("filter creation / effect compilation");
	}

	if (argc >= 5) {
		if (!load_module(argv[3], argv[4]))
			return 1;

		obs_data_t *color_settings = obs_data_create();
		obs_data_set_int(color_settings, "width", 1280);
		obs_data_set_int(color_settings, "height", 720);
		obs_data_set_int(color_settings, "color", 0xFF7896C8); /* skin tone */
		obs_source_t *color =
			obs_source_create("color_source", "beauty-color-test", color_settings, NULL);
		obs_data_release(color_settings);
		if (!source_valid(color)) {
			if (color)
				obs_source_release(color);
			return fail("color source creation");
		}

		uint8_t original[4];
		if (!render_center(color, 1280, 720, original)) {
			obs_source_release(color);
			return fail("render original source");
		}
		printf("center pixel RGBA (original):   %u %u %u %u\n", original[0],
		       original[1], original[2], original[3]);

		obs_source_filter_add(color, filter);

		uint8_t processed[4];
		if (!render_center(color, 1280, 720, processed)) {
			obs_source_release(color);
			return fail("render filter");
		}
		printf("center pixel RGBA (processed):  %u %u %u %u\n", processed[0],
		       processed[1], processed[2], processed[3]);

		if (!color_near(processed, 200, 150, 120, 8)) {
			obs_source_release(color);
			return fail("unexpected default filter output");
		}

		/*
		 * Use fresh filter instances for the remaining checks. OBS defers
		 * obs_source_update() for video sources until the next video tick,
		 * which this headless test does not run.
		 */
		obs_source_filter_remove(color, filter);
		obs_source_release(filter);

		/* Mask preview must be fully white on a flat skin-tone frame. */
		obs_data_t *mask_settings = obs_data_create();
		set_test_defaults(mask_settings);
		obs_data_set_bool(mask_settings, "mask_preview", true);
		obs_source_t *mask_filter =
			make_filter("beauty-mask-test", mask_settings);
		if (!mask_filter) {
			obs_source_release(color);
			return fail("mask filter creation");
		}

		obs_source_filter_add(color, mask_filter);

		uint8_t mask[4];
		if (!render_center(color, 1280, 720, mask)) {
			obs_source_release(color);
			obs_source_release(mask_filter);
			return fail("render skin mask preview");
		}
		printf("center pixel RGBA (skin mask):  %u %u %u %u\n", mask[0], mask[1],
		       mask[2], mask[3]);

		if (!color_near(mask, 255, 255, 255, 2)) {
			obs_source_release(color);
			obs_source_release(mask_filter);
			return fail("skin mask did not cover the test skin tone");
		}

		obs_source_filter_remove(color, mask_filter);
		obs_source_release(mask_filter);

		/* Opacity 0 must restore the original frame exactly (within rounding). */
		obs_data_t *opacity_settings = obs_data_create();
		set_test_defaults(opacity_settings);
		obs_data_set_double(opacity_settings, "opacity", 0.0);
		obs_source_t *opacity_filter =
			make_filter("beauty-opacity-test", opacity_settings);
		if (!opacity_filter) {
			obs_source_release(color);
			return fail("opacity filter creation");
		}

		obs_source_filter_add(color, opacity_filter);

		uint8_t transparent[4];
		if (!render_center(color, 1280, 720, transparent)) {
			obs_source_release(color);
			obs_source_release(opacity_filter);
			return fail("render opacity 0");
		}
		printf("center pixel RGBA (opacity 0):  %u %u %u %u\n", transparent[0],
		       transparent[1], transparent[2], transparent[3]);

		if (!color_near(transparent, original[0], original[1], original[2], 1)) {
			obs_source_release(color);
			obs_source_release(opacity_filter);
			return fail("opacity 0 did not restore the original frame");
		}

		obs_source_filter_remove(color, opacity_filter);
		obs_source_release(opacity_filter);

		/* ------------------------------------------------------------- */
		/* Face zones: manual box + teeth/eye enhancement                 */
		/* ------------------------------------------------------------- */
		obs_data_t *pattern_sdr_settings = obs_data_create();
		obs_data_set_bool(pattern_sdr_settings, "hdr", false);
		obs_source_t *pattern =
			obs_source_create("beauty_test_pattern", "beauty-pattern", pattern_sdr_settings,
					  NULL);
		obs_data_release(pattern_sdr_settings);
		if (!source_valid(pattern)) {
			obs_source_release(color);
			return fail("pattern source creation");
		}

		uint8_t eye_before[4];
		uint8_t mouth_before[4];
		if (!render_pixel(pattern, 1280, 720, 550, 300, eye_before) ||
		    !render_pixel(pattern, 1280, 720, 640, 490, mouth_before)) {
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("render pattern reference pixels");
		}
		printf("pattern eye/mouth luma before:  %d / %d\n", luma8(eye_before),
		       luma8(mouth_before));

		obs_data_t *zone_settings = obs_data_create();
		set_test_defaults(zone_settings);
		set_neutral_processing(zone_settings);
		obs_data_set_bool(zone_settings, "face_aware", true);
		obs_data_set_bool(zone_settings, "face_manual", true);
		obs_data_set_double(zone_settings, "teeth_whiten", 1.0);
		obs_data_set_double(zone_settings, "eye_brighten", 1.0);
		obs_source_t *zone_filter =
			make_filter("beauty-face-zones", zone_settings);
		if (!zone_filter) {
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("face zone filter creation");
		}

		obs_source_filter_add(pattern, zone_filter);

		uint8_t eye_after[4];
		uint8_t mouth_after[4];
		if (!render_pixel(pattern, 1280, 720, 550, 300, eye_after) ||
		    !render_pixel(pattern, 1280, 720, 640, 490, mouth_after)) {
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(zone_filter);
			return fail("render face zone output");
		}
		printf("pattern eye/mouth luma after:   %d / %d\n", luma8(eye_after),
		       luma8(mouth_after));

		if (luma8(mouth_after) < luma8(mouth_before) + 6) {
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(zone_filter);
			return fail("teeth whitening did not brighten the mouth zone");
		}
		if (luma8(eye_after) < luma8(eye_before) + 15) {
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(zone_filter);
			return fail("eye brightening did not brighten the eye zone");
		}

		obs_source_filter_remove(pattern, zone_filter);
		obs_source_release(zone_filter);

		/* Zone preview must render the expected debug colours. */
		obs_data_t *preview_settings = obs_data_create();
		set_test_defaults(preview_settings);
		set_neutral_processing(preview_settings);
		obs_data_set_bool(preview_settings, "face_aware", true);
		obs_data_set_bool(preview_settings, "face_manual", true);
		obs_data_set_bool(preview_settings, "zone_preview", true);
		obs_source_t *preview_filter =
			make_filter("beauty-zone-preview", preview_settings);
		if (!preview_filter) {
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("zone preview filter creation");
		}

		obs_source_filter_add(pattern, preview_filter);

		uint8_t eye_preview[4];
		uint8_t mouth_preview[4];
		if (!render_pixel(pattern, 1280, 720, 550, 300, eye_preview) ||
		    !render_pixel(pattern, 1280, 720, 640, 490, mouth_preview)) {
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(preview_filter);
			return fail("render zone preview");
		}
		printf("zone preview eye RGBA:         %u %u %u %u\n", eye_preview[0],
		       eye_preview[1], eye_preview[2], eye_preview[3]);
		printf("zone preview mouth RGBA:       %u %u %u %u\n", mouth_preview[0],
		       mouth_preview[1], mouth_preview[2], mouth_preview[3]);

		if (!(eye_preview[2] > eye_preview[0] + 40)) {
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(preview_filter);
			return fail("eye zone preview is not blue");
		}
		if (!(mouth_preview[0] > 180 && mouth_preview[0] > mouth_preview[2] + 30)) {
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(preview_filter);
			return fail("mouth zone preview is not yellow");
		}

		obs_source_filter_remove(pattern, preview_filter);
		obs_source_release(preview_filter);

		/* ------------------------------------------------------------- */
		/* HDR: Rec.709 extended must be processed, not bypassed          */
		/* ------------------------------------------------------------- */
		obs_data_t *pattern_hdr_settings = obs_data_create();
		obs_data_set_bool(pattern_hdr_settings, "hdr", true);
		obs_source_t *hdr_pattern = obs_source_create("beauty_test_pattern",
							      "beauty-pattern-hdr",
							      pattern_hdr_settings, NULL);
		obs_data_release(pattern_hdr_settings);
		if (!source_valid(hdr_pattern)) {
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("HDR pattern source creation");
		}

		float hdr_before[4];
		if (!render_pixel_hdr(hdr_pattern, 1280, 720, 640, 360, GS_CS_709_EXTENDED,
				      hdr_before)) {
			obs_source_release(hdr_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("render HDR reference pixel");
		}
		printf("HDR pixel before filter:       %.3f %.3f %.3f %.3f\n", hdr_before[0],
		       hdr_before[1], hdr_before[2], hdr_before[3]);

		obs_data_t *hdr_settings = obs_data_create();
		set_test_defaults(hdr_settings);
		set_neutral_processing(hdr_settings);
		obs_data_set_bool(hdr_settings, "skin_detect", false);
		obs_data_set_bool(hdr_settings, "face_aware", false);
		obs_data_set_double(hdr_settings, "rosy", 1.0);
		obs_source_t *hdr_filter = make_filter("beauty-hdr", hdr_settings);
		if (!hdr_filter) {
			obs_source_release(hdr_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("HDR filter creation");
		}

		obs_source_filter_add(hdr_pattern, hdr_filter);

		float hdr_after[4];
		if (!render_pixel_hdr(hdr_pattern, 1280, 720, 640, 360, GS_CS_709_EXTENDED,
				      hdr_after)) {
			obs_source_release(hdr_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(hdr_filter);
			return fail("render filtered HDR pixel");
		}
		printf("HDR pixel after filter:        %.3f %.3f %.3f %.3f\n", hdr_after[0],
		       hdr_after[1], hdr_after[2], hdr_after[3]);

		if (!(hdr_after[0] > 1.5f)) {
			obs_source_release(hdr_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(hdr_filter);
			return fail("HDR highlight was clipped below SDR white");
		}
		if (!(hdr_after[0] > hdr_before[0] + 0.05f)) {
			obs_source_release(hdr_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(hdr_filter);
			return fail("HDR source was bypassed instead of filtered");
		}

		obs_source_filter_remove(hdr_pattern, hdr_filter);
		obs_source_release(hdr_filter);
		obs_source_release(hdr_pattern);

		/* ------------------------------------------------------------- */
		/* HDR: scRGB uses 80-nit units; the filter must keep that scale  */
		/* ------------------------------------------------------------- */
		obs_data_t *pattern_scrgb_settings = obs_data_create();
		obs_data_set_int(pattern_scrgb_settings, "space", GS_CS_709_SCRGB);
		obs_source_t *scrgb_pattern = obs_source_create(
			"beauty_test_pattern", "beauty-pattern-scrgb",
			pattern_scrgb_settings, NULL);
		obs_data_release(pattern_scrgb_settings);
		if (!source_valid(scrgb_pattern)) {
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("scRGB pattern source creation");
		}

		float scrgb_before[4];
		if (!render_pixel_hdr(scrgb_pattern, 1280, 720, 640, 360, GS_CS_709_SCRGB,
				      scrgb_before)) {
			obs_source_release(scrgb_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("render scRGB reference pixel");
		}
		printf("scRGB pixel before filter:     %.3f %.3f %.3f %.3f\n",
		       scrgb_before[0], scrgb_before[1], scrgb_before[2], scrgb_before[3]);

		obs_data_t *scrgb_settings = obs_data_create();
		set_test_defaults(scrgb_settings);
		set_neutral_processing(scrgb_settings);
		obs_data_set_bool(scrgb_settings, "skin_detect", false);
		obs_data_set_bool(scrgb_settings, "face_aware", false);
		obs_data_set_double(scrgb_settings, "rosy", 1.0);
		obs_source_t *scrgb_filter =
			make_filter("beauty-scrgb", scrgb_settings);
		if (!scrgb_filter) {
			obs_source_release(scrgb_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			return fail("scRGB filter creation");
		}

		obs_source_filter_add(scrgb_pattern, scrgb_filter);

		float scrgb_after[4];
		if (!render_pixel_hdr(scrgb_pattern, 1280, 720, 640, 360, GS_CS_709_SCRGB,
				      scrgb_after)) {
			obs_source_release(scrgb_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(scrgb_filter);
			return fail("render filtered scRGB pixel");
		}
		printf("scRGB pixel after filter:      %.3f %.3f %.3f %.3f\n",
		       scrgb_after[0], scrgb_after[1], scrgb_after[2], scrgb_after[3]);

		if (!(scrgb_after[0] > 1.5f)) {
			obs_source_release(scrgb_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(scrgb_filter);
			return fail("scRGB highlight was clipped below SDR white");
		}
		if (!(scrgb_after[0] > scrgb_before[0] + 0.05f)) {
			obs_source_release(scrgb_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(scrgb_filter);
			return fail("scRGB source was bypassed instead of filtered");
		}
		/* A missing hdr_scale on the working colour shows up as a large
		 * output boost (1 / hdr_scale); the upper bound catches it. */
		if (!(scrgb_after[0] < scrgb_before[0] * 1.5f)) {
			obs_source_release(scrgb_pattern);
			obs_source_release(pattern);
			obs_source_release(color);
			obs_source_release(scrgb_filter);
			return fail("scRGB output has an incorrect HDR scale");
		}

		obs_source_filter_remove(scrgb_pattern, scrgb_filter);
		obs_source_release(scrgb_filter);
		obs_source_release(scrgb_pattern);
		obs_source_release(pattern);
		obs_source_release(color);
	} else {
		obs_source_release(filter);
	}

	printf("OK: module loaded, filter created, effect compiled");
	if (argc >= 5)
		printf(", filter rendered, mask/opacity/face zones/HDR verified");
	printf("\n");

	obs_shutdown();
	return 0;
}
