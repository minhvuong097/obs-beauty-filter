/*
 * OBS Beauty Filter
 *
 * A GPU beauty filter for livestreams: edge-preserving skin smoothing,
 * temporal mask stabilisation, heuristic face-zone processing (teeth and
 * eyes), skin-weighted complexion adjustments and HDR-safe output.
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "beauty-custom-presets.h"
#include "beauty-filter.h"
#include "beauty-presets.h"
#include "beauty-settings.h"
#include "face-region.h"

#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */

#define T_HDR_INFO              obs_module_text("HdrInfo")

#define T_PRESET                obs_module_text("Preset")
#define T_PRESET_CUSTOM         obs_module_text("Preset.custom")
#define T_PRESET_NAME           obs_module_text("Preset.Name")
#define T_PRESET_SAVE           obs_module_text("Preset.Save")
#define T_PRESET_DELETE         obs_module_text("Preset.Delete")
#define T_PRESET_RESET          obs_module_text("Preset.Reset")
#define T_PRESET_CUSTOM_ENTRY   obs_module_text("Preset.CustomEntry")
#define T_PRESET_STATUS_SAVED   obs_module_text("Preset.StatusSaved")
#define T_PRESET_STATUS_DELETED obs_module_text("Preset.StatusDeleted")
#define T_PRESET_STATUS_BAD     obs_module_text("Preset.StatusBadName")
#define T_PRESET_STATUS_WRITE   obs_module_text("Preset.StatusWriteFailed")
#define T_PRESET_STATUS_MISSING obs_module_text("Preset.StatusNotFound")
#define T_PRESET_STATUS_RESET   obs_module_text("Preset.StatusReset")
#define T_ERROR_INFO            obs_module_text("ErrorInfo")

#define T_GROUP_SMOOTHING       obs_module_text("Group.Smoothing")
#define T_AMOUNT                obs_module_text("Smoothing.Amount")
#define T_RADIUS                obs_module_text("Smoothing.Radius")
#define T_DETAIL                obs_module_text("Smoothing.Detail")
#define T_SKIN_DETECT           obs_module_text("Smoothing.SkinDetect")
#define T_SKIN_SENSITIVITY      obs_module_text("Smoothing.SkinSensitivity")
#define T_MASK_PREVIEW          obs_module_text("Smoothing.MaskPreview")

#define T_GROUP_COMPLEXION      obs_module_text("Group.Complexion")
#define T_BRIGHTNESS            obs_module_text("Complexion.Brightness")
#define T_WARMTH                obs_module_text("Complexion.Warmth")
#define T_SATURATION            obs_module_text("Complexion.Saturation")
#define T_ROSY                  obs_module_text("Complexion.Rosy")

#define T_GROUP_DETAIL          obs_module_text("Group.Detail")
#define T_CLARITY               obs_module_text("Detail.Clarity")
#define T_GLOW                  obs_module_text("Detail.Glow")
#define T_LIGHT_BALANCE         obs_module_text("Detail.LightBalance")

#define T_GROUP_FACE            obs_module_text("Group.Face")
#define T_FACE_AWARE            obs_module_text("Face.Enable")
#define T_TEMPORAL              obs_module_text("Face.Temporal")
#define T_TEETH                 obs_module_text("Face.Teeth")
#define T_EYES                  obs_module_text("Face.Eyes")
#define T_ZONE_PREVIEW          obs_module_text("Face.ZonePreview")
#define T_FACE_MANUAL           obs_module_text("Face.Manual")
#define T_FACE_CX               obs_module_text("Face.CenterX")
#define T_FACE_CY               obs_module_text("Face.CenterY")
#define T_FACE_W                obs_module_text("Face.Width")
#define T_FACE_H                obs_module_text("Face.Height")

#define T_GROUP_ADVANCED        obs_module_text("Group.Advanced")
#define T_QUALITY               obs_module_text("Advanced.Quality")
#define T_SPLIT_VIEW            obs_module_text("Advanced.SplitView")
#define T_OPACITY               obs_module_text("Advanced.Opacity")
#define T_ANTI_BANDING          obs_module_text("Advanced.AntiBanding")

#define T_QUALITY_FAST          obs_module_text("Quality.Fast")
#define T_QUALITY_BALANCED      obs_module_text("Quality.Balanced")
#define T_QUALITY_HIGH          obs_module_text("Quality.High")

/* clang-format on */

#define FACE_READBACK_INTERVAL 6
#define LIGHT_READBACK_INTERVAL 6
#define FACE_HOLD_UPDATES 5
#define FACE_GRID_WIDTH 64
#define FACE_GRID_MAX_HEIGHT 64

enum beauty_quality {
	BEAUTY_QUALITY_FAST,
	BEAUTY_QUALITY_BALANCED,
	BEAUTY_QUALITY_HIGH,
};

static const char *beauty_technique(enum beauty_quality quality);

struct beauty_filter {
	obs_source_t *context;
	gs_effect_t *effect;

	gs_eparam_t *p_image;
	gs_eparam_t *p_mask_tex;
	gs_eparam_t *p_history_tex;
	gs_eparam_t *p_base_tex;

	gs_eparam_t *p_texel_size;
	gs_eparam_t *p_analysis_texel;
	gs_eparam_t *p_radius;
	gs_eparam_t *p_amount;
	gs_eparam_t *p_detail;
	gs_eparam_t *p_skin_detect;
	gs_eparam_t *p_skin_sensitivity;
	gs_eparam_t *p_brightness;
	gs_eparam_t *p_warmth;
	gs_eparam_t *p_saturation;
	gs_eparam_t *p_rosy;
	gs_eparam_t *p_clarity;
	gs_eparam_t *p_glow;
	gs_eparam_t *p_light_balance;
	gs_eparam_t *p_auto_gain;
	gs_eparam_t *p_opacity;
	gs_eparam_t *p_mask_preview;
	gs_eparam_t *p_split_view;

	gs_eparam_t *p_temporal_strength;
	gs_eparam_t *p_history_valid;
	gs_eparam_t *p_use_mask_tex;
	gs_eparam_t *p_face_aware;
	gs_eparam_t *p_face_valid;
	gs_eparam_t *p_face_center;
	gs_eparam_t *p_face_size;
	gs_eparam_t *p_teeth_whiten;
	gs_eparam_t *p_eye_brighten;
	gs_eparam_t *p_zone_preview;
	gs_eparam_t *p_hdr_scale;
	gs_eparam_t *p_is_hdr;
	gs_eparam_t *p_base_valid;
	gs_eparam_t *p_dither;

	float amount;
	float radius;
	float detail;
	float skin_sensitivity;
	float brightness;
	float warmth;
	float saturation;
	float rosy;
	float clarity;
	float glow;
	float light_balance;
	float opacity;

	/* Measured exposure gain for the automatic light balance.  Smoothed
	 * between analysis updates so the image follows the subject. */
	float auto_gain;
	bool auto_gain_valid;

	float temporal_strength;
	float teeth_whiten;
	float eye_brighten;

	float face_cx;
	float face_cy;
	float face_w;
	float face_h;

	bool skin_detect;
	bool mask_preview;
	bool split_view;
	bool face_aware;
	bool zone_preview;
	bool face_manual;
	bool anti_banding;

	enum beauty_quality quality;
	enum beauty_quality quality_at_alloc;

	/* Graphics resources, created lazily on the render thread. */
	gs_texrender_t *src_texrender;
	gs_texrender_t *mask_texrender[2];
	gs_texrender_t *analysis_texrender;
	gs_texrender_t *base_texrender;
	gs_stagesurf_t *analysis_stage;
	gs_texture_t *white_tex;

	enum gs_color_space space;
	enum gs_color_format src_format;
	uint32_t width;
	uint32_t height;
	uint32_t mask_width;
	uint32_t mask_height;
	uint32_t analysis_width;
	uint32_t analysis_height;
	uint32_t base_width;
	uint32_t base_height;
	uint32_t base_divisor;

	float dither_strength;

	bool resources_ready;
	bool effect_failed;
	bool resource_failed;
	bool history_valid;
	int mask_index;
	uint32_t frame_counter;
	char preset_status[192];

	struct beauty_face_region auto_face;
	bool auto_face_valid;
	int face_miss_updates;
};

static const char *beauty_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("BeautyFilter");
}

static float clamp_float(float value, float lo, float hi)
{
	return value < lo ? lo : (value > hi ? hi : value);
}

static void beauty_update(void *data, obs_data_t *settings)
{
	struct beauty_filter *filter = data;
	const char *quality;

	filter->amount = (float)obs_data_get_double(settings, S_AMOUNT);
	filter->radius = (float)obs_data_get_double(settings, S_RADIUS);
	filter->detail = (float)obs_data_get_double(settings, S_DETAIL);
	filter->skin_sensitivity = (float)obs_data_get_double(settings, S_SKIN_SENSITIVITY);

	filter->brightness = (float)obs_data_get_double(settings, S_BRIGHTNESS) / 100.0f;
	filter->warmth = (float)obs_data_get_double(settings, S_WARMTH) / 100.0f;
	filter->saturation = (float)obs_data_get_double(settings, S_SATURATION) / 100.0f;
	filter->rosy = (float)obs_data_get_double(settings, S_ROSY);

	filter->clarity = (float)obs_data_get_double(settings, S_CLARITY);
	filter->glow = (float)obs_data_get_double(settings, S_GLOW);
	filter->light_balance =
		clamp_float((float)obs_data_get_double(settings, S_LIGHT_BALANCE), 0.0f, 1.0f);
	filter->opacity = (float)obs_data_get_double(settings, S_OPACITY);

	filter->temporal_strength =
		clamp_float((float)obs_data_get_double(settings, S_TEMPORAL), 0.0f, 1.0f);
	filter->teeth_whiten =
		clamp_float((float)obs_data_get_double(settings, S_TEETH), 0.0f, 1.0f);
	filter->eye_brighten =
		clamp_float((float)obs_data_get_double(settings, S_EYES), 0.0f, 1.0f);

	filter->face_cx = clamp_float((float)obs_data_get_double(settings, S_FACE_CX), 0.0f, 1.0f);
	filter->face_cy = clamp_float((float)obs_data_get_double(settings, S_FACE_CY), 0.0f, 1.0f);
	filter->face_w = clamp_float((float)obs_data_get_double(settings, S_FACE_W), 0.02f, 1.0f);
	filter->face_h = clamp_float((float)obs_data_get_double(settings, S_FACE_H), 0.02f, 1.0f);

	filter->skin_detect = obs_data_get_bool(settings, S_SKIN_DETECT);
	filter->mask_preview = obs_data_get_bool(settings, S_MASK_PREVIEW);
	filter->split_view = obs_data_get_bool(settings, S_SPLIT_VIEW);
	filter->face_aware = obs_data_get_bool(settings, S_FACE_AWARE);
	filter->zone_preview = obs_data_get_bool(settings, S_ZONE_PREVIEW);
	filter->face_manual = obs_data_get_bool(settings, S_FACE_MANUAL);
	filter->anti_banding = obs_data_get_bool(settings, S_ANTI_BANDING);

	quality = obs_data_get_string(settings, S_QUALITY);
	if (quality && strcmp(quality, "fast") == 0)
		filter->quality = BEAUTY_QUALITY_FAST;
	else if (quality && strcmp(quality, "high") == 0)
		filter->quality = BEAUTY_QUALITY_HIGH;
	else
		filter->quality = BEAUTY_QUALITY_BALANCED;
}

static void beauty_release_resources(struct beauty_filter *filter)
{
	gs_texrender_destroy(filter->src_texrender);
	gs_texrender_destroy(filter->mask_texrender[0]);
	gs_texrender_destroy(filter->mask_texrender[1]);
	gs_texrender_destroy(filter->analysis_texrender);
	gs_texrender_destroy(filter->base_texrender);
	gs_stagesurface_destroy(filter->analysis_stage);
	gs_texture_destroy(filter->white_tex);

	filter->src_texrender = NULL;
	filter->mask_texrender[0] = NULL;
	filter->mask_texrender[1] = NULL;
	filter->analysis_texrender = NULL;
	filter->base_texrender = NULL;
	filter->analysis_stage = NULL;
	filter->white_tex = NULL;
	filter->base_width = 0;
	filter->base_height = 0;
	filter->base_divisor = 0;
	filter->dither_strength = 0.0f;
	filter->resources_ready = false;
	filter->history_valid = false;
	filter->mask_index = 0;
	filter->frame_counter = 0;
	filter->auto_face_valid = false;
	filter->face_miss_updates = 0;
	memset(&filter->auto_face, 0, sizeof(filter->auto_face));
}

static bool beauty_space_is_extended(enum gs_color_space space)
{
	return space == GS_CS_SRGB_16F || space == GS_CS_709_EXTENDED ||
	       space == GS_CS_709_SCRGB;
}

static bool beauty_ensure_resources(struct beauty_filter *filter, uint32_t width, uint32_t height,
				    enum gs_color_space space)
{
	static const uint8_t white_pixel[4] = {255, 255, 255, 255};
	const uint8_t *white_ptr = white_pixel;
	/* The base holds linear light.  Keep it in half float even for SDR:
	 * quantising a blurred linear image to 8 bits creates visible steps in
	 * skin shadows before the final sRGB encode. */
	const enum gs_color_format base_format = GS_RGBA16F;

	if (filter->resources_ready && filter->width == width && filter->height == height &&
	    filter->space == space && filter->quality_at_alloc == filter->quality)
		return true;

	beauty_release_resources(filter);

	filter->width = width;
	filter->height = height;
	filter->space = space;
	filter->src_format = gs_get_format_from_space(space);
	filter->mask_width = (width + 1) / 2;
	filter->mask_height = (height + 1) / 2;
	filter->quality_at_alloc = filter->quality;

	/* The bilateral base is rendered at a fraction of the output
	 * resolution.  Smaller bases are cheaper and allow a wider blur with a
	 * fixed 25-tap kernel; the full-resolution detail is reconstructed in
	 * the final pass. */
	switch (filter->quality) {
	case BEAUTY_QUALITY_FAST:
		filter->base_divisor = 4;
		break;
	case BEAUTY_QUALITY_HIGH:
		filter->base_divisor = 2;
		break;
	case BEAUTY_QUALITY_BALANCED:
	default:
		filter->base_divisor = 3;
		break;
	}

	{
		uint32_t max_divisor = width / 32;
		if (height / 32 < max_divisor)
			max_divisor = height / 32;
		if (max_divisor < 1)
			max_divisor = 1;
		if (filter->base_divisor > max_divisor)
			filter->base_divisor = max_divisor;
		if (filter->base_divisor > 8)
			filter->base_divisor = 8;
		if (filter->base_divisor < 1)
			filter->base_divisor = 1;
	}

	filter->base_width = (width + filter->base_divisor - 1) / filter->base_divisor;
	filter->base_height = (height + filter->base_divisor - 1) / filter->base_divisor;
	if (filter->base_width < 1)
		filter->base_width = 1;
	if (filter->base_height < 1)
		filter->base_height = 1;

	filter->analysis_width = FACE_GRID_WIDTH;
	filter->analysis_height =
		(uint32_t)lround((double)FACE_GRID_WIDTH * (double)height / (double)width);
	if (filter->analysis_height < 16)
		filter->analysis_height = 16;
	if (filter->analysis_height > FACE_GRID_MAX_HEIGHT)
		filter->analysis_height = FACE_GRID_MAX_HEIGHT;

	filter->src_texrender = gs_texrender_create(filter->src_format, GS_ZS_NONE);
	filter->mask_texrender[0] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->mask_texrender[1] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->analysis_texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->base_texrender = gs_texrender_create(base_format, GS_ZS_NONE);
	filter->analysis_stage =
		gs_stagesurface_create(filter->analysis_width, filter->analysis_height, GS_RGBA);
	filter->white_tex = gs_texture_create(1, 1, GS_RGBA, 1, &white_ptr, 0);

	if (!filter->src_texrender || !filter->mask_texrender[0] ||
	    !filter->mask_texrender[1] || !filter->analysis_texrender ||
	    !filter->base_texrender || !filter->analysis_stage || !filter->white_tex) {
		if (!filter->resource_failed) {
			blog(LOG_ERROR,
			     "[obs-beauty-filter] Failed to allocate GPU resources for %ux%u; "
			     "passing the source through",
			     width, height);
			filter->resource_failed = true;
		}
		beauty_release_resources(filter);
		return false;
	}

	filter->resources_ready = true;
	filter->resource_failed = false;
	return true;
}

static void beauty_draw_technique(struct beauty_filter *filter, const char *name,
				  uint32_t width, uint32_t height, gs_texture_t *draw_tex)
{
	gs_technique_t *tech = gs_effect_get_technique(filter->effect, name);
	size_t passes;
	size_t i;

	if (!tech)
		return;

	passes = gs_technique_begin(tech);
	for (i = 0; i < passes; ++i) {
		gs_technique_begin_pass(tech, i);
		gs_draw_sprite(draw_tex, 0, width, height);
		gs_technique_end_pass(tech);
	}
	gs_technique_end(tech);
}

static void beauty_update_auto_face(struct beauty_filter *filter, const uint8_t *data,
				    uint32_t linesize)
{
	struct beauty_face_region region =
		beauty_face_region_detect(data, linesize, filter->analysis_width,
					  filter->analysis_height, 64);

	if (region.valid) {
		if (filter->auto_face_valid) {
			const float blend = 0.35f;
			filter->auto_face.center_x +=
				(region.center_x - filter->auto_face.center_x) * blend;
			filter->auto_face.center_y +=
				(region.center_y - filter->auto_face.center_y) * blend;
			filter->auto_face.half_w +=
				(region.half_w - filter->auto_face.half_w) * blend;
			filter->auto_face.half_h +=
				(region.half_h - filter->auto_face.half_h) * blend;
			filter->auto_face.valid = true;
		} else {
			filter->auto_face = region;
		}
		filter->auto_face_valid = true;
		filter->face_miss_updates = 0;
	} else if (filter->auto_face_valid) {
		if (++filter->face_miss_updates > FACE_HOLD_UPDATES) {
			filter->auto_face_valid = false;
			filter->face_miss_updates = 0;
		}
	}
}

/*
 * Derive the exposure gain from the tiny analysis readback.  b = linear luma,
 * r = raw skin mask.  When skin covers a sensible part of the frame the face
 * dominates the measurement, so a bright window behind the subject does not
 * darken the skin.
 */
static void beauty_update_light_balance(struct beauty_filter *filter, const uint8_t *pixels,
					uint32_t linesize)
{
	const float target = 0.20f;
	const float rate = 0.25f;
	double sum = 0.0;
	double skin_sum = 0.0;
	double skin_weight = 0.0;
	double mean;
	double gain;
	uint32_t x, y;

	for (y = 0; y < filter->analysis_height; y++) {
		const uint8_t *row = pixels + (size_t)y * linesize;

		for (x = 0; x < filter->analysis_width; x++) {
			const uint8_t *px = row + x * 4;
			const double luma = (double)px[2] / 255.0;
			const double skin = (double)px[0] / 255.0;

			sum += luma;
			skin_sum += luma * skin;
			skin_weight += skin;
		}
	}

	const double count = (double)filter->analysis_width * (double)filter->analysis_height;
	if (count < 1.0)
		return;

	mean = sum / count;
	if (skin_weight > count * 0.01 && skin_weight < count * 0.90)
		mean = mean * 0.35 + (skin_sum / skin_weight) * 0.65;

	gain = (double)target / (mean > 0.004 ? mean : 0.004);
	if (gain < 0.50)
		gain = 0.50;
	if (gain > 2.50)
		gain = 2.50;

	if (!filter->auto_gain_valid) {
		filter->auto_gain = (float)gain;
		filter->auto_gain_valid = true;
	} else {
		filter->auto_gain += (float)(gain - filter->auto_gain) * rate;
	}
}

static void beauty_analyse_mask(struct beauty_filter *filter, gs_texture_t *mask_tex)
{
	struct vec2 analysis_texel;
	uint8_t *pixels = NULL;
	uint32_t linesize = 0;

	if (!mask_tex || !filter->analysis_texrender || !filter->analysis_stage)
		return;

	vec2_set(&analysis_texel, 1.0f / (float)filter->analysis_width,
		 1.0f / (float)filter->analysis_height);

	if (filter->p_mask_tex)
		gs_effect_set_texture(filter->p_mask_tex, mask_tex);
	if (filter->p_analysis_texel)
		gs_effect_set_vec2(filter->p_analysis_texel, &analysis_texel);

	const bool previous_fb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);

	gs_texrender_reset(filter->analysis_texrender);
	if (!gs_texrender_begin(filter->analysis_texrender, filter->analysis_width,
				filter->analysis_height)) {
		gs_enable_framebuffer_srgb(previous_fb);
		return;
	}

	gs_ortho(0.0f, (float)filter->analysis_width, 0.0f,
		 (float)filter->analysis_height, -100.0f, 100.0f);
	beauty_draw_technique(filter, "MaskAnalysis", filter->analysis_width,
			      filter->analysis_height, mask_tex);
	gs_texrender_end(filter->analysis_texrender);

	gs_stage_texture(filter->analysis_stage,
			 gs_texrender_get_texture(filter->analysis_texrender));
	if (gs_stagesurface_map(filter->analysis_stage, &pixels, &linesize)) {
		if (filter->face_aware && !filter->face_manual)
			beauty_update_auto_face(filter, pixels, linesize);
		if (filter->light_balance > 0.0001f)
			beauty_update_light_balance(filter, pixels, linesize);
		gs_stagesurface_unmap(filter->analysis_stage);
	}

	gs_enable_framebuffer_srgb(previous_fb);
}

static void beauty_render(void *data, gs_effect_t *unused)
{
	struct beauty_filter *filter = data;
	obs_source_t *target;
	obs_source_t *parent;
	uint32_t width;
	uint32_t height;
	uint32_t target_flags;
	struct vec2 texel;
	struct vec2 face_center;
	struct vec2 face_size;
	gs_texture_t *src_tex;
	gs_texture_t *mask_tex;
	float radius_scale;
	float radius;
	float hdr_scale = 1.0f;
	bool is_hdr = false;
	bool need_mask;
	bool use_mask_tex = false;
	bool mask_rendered = false;
	bool face_valid = false;
	float face_cx = 0.0f;
	float face_cy = 0.0f;
	float face_hw = 0.0f;
	float face_hh = 0.0f;
	bool split_linear;

	UNUSED_PARAMETER(unused);

	vec2_zero(&face_center);
	vec2_zero(&face_size);

	if (!filter->effect) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	target = obs_filter_get_target(filter->context);
	parent = obs_filter_get_parent(filter->context);
	if (!target || !parent) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	width = obs_source_get_base_width(target);
	height = obs_source_get_base_height(target);
	if (!width || !height) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
		GS_CS_709_SCRGB,
	};
	const enum gs_color_space source_space = obs_source_get_color_space(
		target, OBS_COUNTOF(preferred_spaces), preferred_spaces);

	if (source_space != GS_CS_SRGB && source_space != GS_CS_SRGB_16F &&
	    source_space != GS_CS_709_EXTENDED && source_space != GS_CS_709_SCRGB) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	/* Capture directly into the current output space.  OBS converts the
	 * source (including HDR tonemapping/scaling) while rendering it into
	 * this texrender, so the main pass always writes values in the same
	 * space as the target. */
	const enum gs_color_space output_space = gs_get_color_space();

	if (!beauty_ensure_resources(filter, width, height, output_space)) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	/* 1. Capture the source into a texture we control. */
	gs_texrender_reset(filter->src_texrender);
	gs_blend_state_push();
	gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA,
				   GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	if (gs_texrender_begin_with_color_space(filter->src_texrender, width, height,
						output_space)) {
		struct vec4 clear_color;
		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);

		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		target_flags = obs_source_get_output_flags(target);
		if (target == parent && !(target_flags & OBS_SOURCE_CUSTOM_DRAW) &&
		    !(target_flags & OBS_SOURCE_ASYNC))
			obs_source_default_render(target);
		else
			obs_source_video_render(target);

		gs_texrender_end(filter->src_texrender);
	}
	gs_blend_state_pop();

	src_tex = gs_texrender_get_texture(filter->src_texrender);
	if (!src_tex) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	const bool previous_linear = gs_set_linear_srgb(true);
	if (filter->p_image) {
		if (gs_is_srgb_format(filter->src_format))
			gs_effect_set_texture_srgb(filter->p_image, src_tex);
		else
			gs_effect_set_texture(filter->p_image, src_tex);
	}

	vec2_set(&texel, 1.0f / (float)width, 1.0f / (float)height);
	radius_scale = (float)height / 1080.0f;
	if (radius_scale < 0.25f)
		radius_scale = 0.25f;
	radius = filter->radius * radius_scale;

	is_hdr = beauty_space_is_extended(output_space);
	if (output_space == GS_CS_709_SCRGB) {
		float sdr_white = obs_get_video_sdr_white_level();
		if (sdr_white < 1.0f)
			sdr_white = 300.0f;
		hdr_scale = 80.0f / sdr_white;
	}

	/* hdr_scale is needed by the base pass even when the mask is disabled. */
	if (filter->p_hdr_scale)
		gs_effect_set_float(filter->p_hdr_scale, hdr_scale);

	need_mask = filter->skin_detect || filter->face_aware ||
		    filter->teeth_whiten > 0.0001f || filter->eye_brighten > 0.0001f;

	/* 2. Skin mask + temporal stabilisation (half resolution). */
	if (need_mask) {
		const bool previous_fb = gs_framebuffer_srgb_enabled();
		gs_texture_t *history_tex = filter->white_tex;
		const int write_index = filter->mask_index;

		if (filter->history_valid)
			history_tex = gs_texrender_get_texture(
				filter->mask_texrender[write_index ^ 1]);
		if (!history_tex)
			history_tex = filter->white_tex;

		if (filter->p_history_tex)
			gs_effect_set_texture(filter->p_history_tex, history_tex);
		if (filter->p_texel_size)
			gs_effect_set_vec2(filter->p_texel_size, &texel);
		if (filter->p_skin_sensitivity)
			gs_effect_set_float(filter->p_skin_sensitivity,
					    filter->skin_sensitivity);
		if (filter->p_temporal_strength)
			gs_effect_set_float(filter->p_temporal_strength,
					    filter->temporal_strength);
		if (filter->p_history_valid)
			gs_effect_set_float(filter->p_history_valid,
					    filter->history_valid ? 1.0f : 0.0f);
		if (filter->p_hdr_scale)
			gs_effect_set_float(filter->p_hdr_scale, hdr_scale);

		face_valid = false;
		if (filter->face_aware) {
			if (filter->face_manual) {
				face_valid = true;
				face_cx = filter->face_cx;
				face_cy = filter->face_cy;
				face_hw = filter->face_w * 0.5f;
				face_hh = filter->face_h * 0.5f;
			} else if (filter->auto_face_valid) {
				face_valid = true;
				face_cx = filter->auto_face.center_x;
				face_cy = filter->auto_face.center_y;
				face_hw = filter->auto_face.half_w;
				face_hh = filter->auto_face.half_h;
			}
		}
		vec2_set(&face_center, face_cx, face_cy);
		vec2_set(&face_size, face_hw, face_hh);
		if (filter->p_face_valid)
			gs_effect_set_float(filter->p_face_valid,
					    face_valid ? 1.0f : 0.0f);
		if (filter->p_face_aware)
			gs_effect_set_float(filter->p_face_aware,
					    filter->face_aware ? 1.0f : 0.0f);
		if (filter->p_face_center)
			gs_effect_set_vec2(filter->p_face_center, &face_center);
		if (filter->p_face_size)
			gs_effect_set_vec2(filter->p_face_size, &face_size);

		gs_enable_framebuffer_srgb(false);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		gs_texrender_reset(filter->mask_texrender[write_index]);
		if (gs_texrender_begin(filter->mask_texrender[write_index],
				       filter->mask_width, filter->mask_height)) {
			gs_ortho(0.0f, (float)filter->mask_width, 0.0f,
				 (float)filter->mask_height, -100.0f, 100.0f);
			beauty_draw_technique(filter, "MaskPass", filter->mask_width,
					      filter->mask_height, src_tex);
			gs_texrender_end(filter->mask_texrender[write_index]);
			mask_rendered = true;
		}

		gs_blend_state_pop();
		gs_enable_framebuffer_srgb(previous_fb);

		mask_tex = gs_texrender_get_texture(filter->mask_texrender[write_index]);
		if (mask_rendered && mask_tex) {
			use_mask_tex = true;
			filter->history_valid = true;
			filter->mask_index ^= 1;
		} else {
			mask_tex = filter->white_tex;
			use_mask_tex = false;
		}
	} else {
		mask_tex = filter->white_tex;
		filter->history_valid = false;
	}

	/* The tiny readback drives the face box and the automatic light
	 * balance.  It is <= 64x64 RGBA and never touches a full-resolution
	 * frame, so it can run a few times per second on either path. */
	filter->frame_counter++;
	{
		const bool face_tick =
			use_mask_tex && filter->face_aware && !filter->face_manual &&
			(filter->frame_counter % FACE_READBACK_INTERVAL) == 0;
		const bool light_tick = filter->light_balance > 0.0001f &&
					(filter->frame_counter % LIGHT_READBACK_INTERVAL) == 0;

		if (face_tick || light_tick)
			beauty_analyse_mask(filter, mask_tex);
	}

	/* 3. Low-resolution bilateral base for frequency separation.  This is
	 * skipped when smoothing is off or when the GPU refused the extra
	 * render target; the main shader then falls back to a full-resolution
	 * bilateral kernel. */
	bool base_rendered = false;
	gs_texture_t *base_tex = NULL;
	if (filter->base_texrender && filter->amount > 0.0001f && radius > 0.01f) {
		struct vec2 base_texel;
		const float base_radius = radius / (float)filter->base_divisor;
		const bool previous_fb = gs_framebuffer_srgb_enabled();

		vec2_set(&base_texel, 1.0f / (float)filter->base_width,
			 1.0f / (float)filter->base_height);

		if (filter->p_texel_size)
			gs_effect_set_vec2(filter->p_texel_size, &base_texel);
		if (filter->p_radius)
			gs_effect_set_float(filter->p_radius, base_radius);
		if (filter->p_detail)
			gs_effect_set_float(filter->p_detail, filter->detail);
		if (filter->p_hdr_scale)
			gs_effect_set_float(filter->p_hdr_scale, hdr_scale);

		if (filter->p_image) {
			if (gs_is_srgb_format(filter->src_format))
				gs_effect_set_texture_srgb(filter->p_image, src_tex);
			else
				gs_effect_set_texture(filter->p_image, src_tex);
		}

		gs_enable_framebuffer_srgb(false);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		gs_texrender_reset(filter->base_texrender);
		if (gs_texrender_begin(filter->base_texrender, filter->base_width,
				       filter->base_height)) {
			gs_ortho(0.0f, (float)filter->base_width, 0.0f,
				 (float)filter->base_height, -100.0f, 100.0f);
			beauty_draw_technique(filter, "BlurBase", filter->base_width,
					      filter->base_height, src_tex);
			gs_texrender_end(filter->base_texrender);

			base_tex = gs_texrender_get_texture(filter->base_texrender);
			base_rendered = base_tex != NULL;
		}

		gs_blend_state_pop();
		gs_enable_framebuffer_srgb(previous_fb);
	}

	/* 4. Main processing pass. */
	if (filter->p_base_tex)
		gs_effect_set_texture(filter->p_base_tex,
				      base_rendered ? base_tex : filter->white_tex);
	if (filter->p_base_valid)
		gs_effect_set_float(filter->p_base_valid,
				    base_rendered ? 1.0f : 0.0f);

	const bool is_8bit = filter->src_format == GS_RGBA ||
			     filter->src_format == GS_BGRA ||
			     filter->src_format == GS_BGRX;
	filter->dither_strength =
		(filter->anti_banding && is_8bit) ? (1.0f / 255.0f) : 0.0f;
	if (filter->p_dither)
		gs_effect_set_float(filter->p_dither, filter->dither_strength);

	if (filter->p_mask_tex)
		gs_effect_set_texture(filter->p_mask_tex, mask_tex);
	if (filter->p_texel_size)
		gs_effect_set_vec2(filter->p_texel_size, &texel);
	if (filter->p_radius)
		gs_effect_set_float(filter->p_radius, radius);
	if (filter->p_amount)
		gs_effect_set_float(filter->p_amount, filter->amount);
	if (filter->p_detail)
		gs_effect_set_float(filter->p_detail, filter->detail);
	if (filter->p_skin_detect)
		gs_effect_set_float(filter->p_skin_detect, filter->skin_detect ? 1.0f : 0.0f);
	if (filter->p_skin_sensitivity)
		gs_effect_set_float(filter->p_skin_sensitivity, filter->skin_sensitivity);
	if (filter->p_brightness)
		gs_effect_set_float(filter->p_brightness, filter->brightness);
	if (filter->p_warmth)
		gs_effect_set_float(filter->p_warmth, filter->warmth);
	if (filter->p_saturation)
		gs_effect_set_float(filter->p_saturation, filter->saturation);
	if (filter->p_rosy)
		gs_effect_set_float(filter->p_rosy, filter->rosy);
	if (filter->p_clarity)
		gs_effect_set_float(filter->p_clarity, filter->clarity);
	if (filter->p_glow)
		gs_effect_set_float(filter->p_glow, filter->glow);
	if (filter->p_light_balance)
		gs_effect_set_float(filter->p_light_balance, filter->light_balance);
	if (filter->p_auto_gain)
		gs_effect_set_float(filter->p_auto_gain,
				    filter->auto_gain_valid ? filter->auto_gain : 1.0f);
	if (filter->p_opacity)
		gs_effect_set_float(filter->p_opacity, filter->opacity);
	if (filter->p_mask_preview)
		gs_effect_set_float(filter->p_mask_preview,
				    filter->mask_preview ? 1.0f : 0.0f);
	if (filter->p_split_view)
		gs_effect_set_float(filter->p_split_view, filter->split_view ? 1.0f : 0.0f);
	if (filter->p_use_mask_tex)
		gs_effect_set_float(filter->p_use_mask_tex, use_mask_tex ? 1.0f : 0.0f);
	if (filter->p_face_valid)
		gs_effect_set_float(filter->p_face_valid, face_valid ? 1.0f : 0.0f);
	if (filter->p_face_aware)
		gs_effect_set_float(filter->p_face_aware, filter->face_aware ? 1.0f : 0.0f);
	if (filter->p_face_center)
		gs_effect_set_vec2(filter->p_face_center, &face_center);
	if (filter->p_face_size)
		gs_effect_set_vec2(filter->p_face_size, &face_size);
	if (filter->p_teeth_whiten)
		gs_effect_set_float(filter->p_teeth_whiten, filter->teeth_whiten);
	if (filter->p_eye_brighten)
		gs_effect_set_float(filter->p_eye_brighten, filter->eye_brighten);
	if (filter->p_zone_preview)
		gs_effect_set_float(filter->p_zone_preview,
				    filter->zone_preview ? 1.0f : 0.0f);
	if (filter->p_hdr_scale)
		gs_effect_set_float(filter->p_hdr_scale, hdr_scale);
	if (filter->p_is_hdr)
		gs_effect_set_float(filter->p_is_hdr, is_hdr ? 1.0f : 0.0f);

	/* Rebind the captured frame right before the main pass: switching
	 * techniques resets the texture unit bindings in some backends. */
	if (filter->p_image) {
		if (gs_is_srgb_format(filter->src_format))
			gs_effect_set_texture_srgb(filter->p_image, src_tex);
		else
			gs_effect_set_texture(filter->p_image, src_tex);
	}

	split_linear = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	beauty_draw_technique(filter, beauty_technique(filter->quality), 0, 0, src_tex);
	gs_blend_state_pop();

	gs_enable_framebuffer_srgb(split_linear);
	gs_set_linear_srgb(previous_linear);
}

static bool beauty_is_visible(obs_properties_t *props, const char *name, bool visible)
{
	obs_property_t *property = obs_properties_get(props, name);
	if (property)
		obs_property_set_visible(property, visible);
	return true;
}

static void beauty_update_visibility(obs_properties_t *props, bool skin_detect, bool face_aware,
				     bool face_manual);

static void beauty_set_status(struct beauty_filter *filter, const char *text)
{
	if (!filter)
		return;

	if (!text) {
		filter->preset_status[0] = '\0';
		return;
	}

	snprintf(filter->preset_status, sizeof(filter->preset_status), "%s", text);
}

static void beauty_set_defaults_as_values(obs_data_t *settings)
{
	const struct beauty_preset *preset = beauty_preset_find("default");

	if (preset)
		beauty_preset_apply(settings, preset);

	obs_data_set_bool(settings, S_SKIN_DETECT, true);
	obs_data_set_bool(settings, S_MASK_PREVIEW, false);
	obs_data_set_bool(settings, S_ZONE_PREVIEW, false);
	obs_data_set_bool(settings, S_FACE_MANUAL, false);
	obs_data_set_double(settings, S_FACE_CX, 0.50);
	obs_data_set_double(settings, S_FACE_CY, 0.45);
	obs_data_set_double(settings, S_FACE_W, 0.32);
	obs_data_set_double(settings, S_FACE_H, 0.42);
	obs_data_set_double(settings, S_LIGHT_BALANCE, 0.35);
	obs_data_set_bool(settings, S_SPLIT_VIEW, false);
	obs_data_set_double(settings, S_OPACITY, 1.0);
	obs_data_set_string(settings, S_PRESET, "default");
	obs_data_set_string(settings, S_PRESET_NAME, "");
}

static struct beauty_filter *properties_filter(obs_properties_t *props, void *data)
{
	struct beauty_filter *filter = obs_properties_get_param(props);

	return filter ? filter : data;
}

static bool reset_settings_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	struct beauty_filter *filter = properties_filter(props, data);
	obs_data_t *settings;

	UNUSED_PARAMETER(property);

	if (!filter || !filter->context)
		return false;

	settings = obs_source_get_settings(filter->context);
	if (!settings)
		return false;

	beauty_set_defaults_as_values(settings);
	beauty_set_status(filter, T_PRESET_STATUS_RESET);
	obs_source_update(filter->context, settings);

	obs_data_release(settings);
	return true;
}

static bool save_preset_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	struct beauty_filter *filter = properties_filter(props, data);
	char name[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];
	obs_data_t *settings;

	UNUSED_PARAMETER(property);

	if (!filter || !filter->context)
		return false;

	settings = obs_source_get_settings(filter->context);
	if (!settings)
		return false;

	if (!beauty_custom_preset_name_normalise(
		    obs_data_get_string(settings, S_PRESET_NAME), name, sizeof(name))) {
		beauty_set_status(filter, T_PRESET_STATUS_BAD);
	} else if (!beauty_custom_preset_save(name, settings)) {
		beauty_set_status(filter, T_PRESET_STATUS_WRITE);
	} else {
		char id[sizeof(BEAUTY_CUSTOM_PREFIX) + BEAUTY_CUSTOM_PRESET_MAX_NAME];

		snprintf(id, sizeof(id), "%s%s", BEAUTY_CUSTOM_PREFIX, name);
		obs_data_set_string(settings, S_PRESET_NAME, name);
		obs_data_set_string(settings, S_PRESET, id);
		snprintf(filter->preset_status, sizeof(filter->preset_status),
			 T_PRESET_STATUS_SAVED, name);
		obs_source_update(filter->context, settings);
	}

	obs_data_release(settings);
	return true;
}

static bool delete_preset_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	struct beauty_filter *filter = properties_filter(props, data);
	char name[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];
	const char *current;
	obs_data_t *settings;
	bool have_name;

	UNUSED_PARAMETER(property);

	if (!filter || !filter->context)
		return false;

	settings = obs_source_get_settings(filter->context);
	if (!settings)
		return false;

	current = obs_data_get_string(settings, S_PRESET);
	have_name = beauty_preset_is_custom_id(current) &&
		    beauty_custom_preset_name_normalise(beauty_preset_custom_name(current),
							name, sizeof(name));
	if (!have_name)
		have_name = beauty_custom_preset_name_normalise(
			obs_data_get_string(settings, S_PRESET_NAME), name, sizeof(name));

	if (!have_name) {
		beauty_set_status(filter, T_PRESET_STATUS_BAD);
	} else if (!beauty_custom_preset_delete(name)) {
		beauty_set_status(filter, T_PRESET_STATUS_MISSING);
	} else {
		if (beauty_preset_is_custom_id(current) &&
		    !beauty_custom_preset_exists(beauty_preset_custom_name(current)))
			obs_data_set_string(settings, S_PRESET, "custom");
		snprintf(filter->preset_status, sizeof(filter->preset_status),
			 T_PRESET_STATUS_DELETED, name);
		obs_source_update(filter->context, settings);
	}

	obs_data_release(settings);
	return true;
}

static bool beauty_mark_custom(obs_data_t *settings)
{
	const char *preset = obs_data_get_string(settings, S_PRESET);

	if (preset && *preset && strcmp(preset, "custom") != 0) {
		obs_data_set_string(settings, S_PRESET, "custom");
		/* Only the first manual edit needs a property refresh.  Returning
		 * true on every slider tick would rebuild the UI mid-drag. */
		return true;
	}

	return false;
}

static bool setting_modified(obs_properties_t *props, obs_property_t *property,
			     obs_data_t *settings)
{
	beauty_set_status(obs_properties_get_param(props), NULL);
	UNUSED_PARAMETER(property);

	/* OBS rebuilds the whole property page when this returns true.  Doing
	 * that while a slider is being dragged makes the handle jump back and
	 * look like the value cannot be changed, so only the stored preset id
	 * is updated here. */
	beauty_mark_custom(settings);
	return false;
}

static obs_property_t *add_float_setting(obs_properties_t *group, const char *name,
					 const char *text, double min, double max,
					 double step)
{
	obs_property_t *property =
		obs_properties_add_float_slider(group, name, text, min, max, step);
	obs_property_set_modified_callback(property, setting_modified);
	return property;
}

static obs_property_t *add_bool_setting(obs_properties_t *group, const char *name,
					const char *text)
{
	obs_property_t *property = obs_properties_add_bool(group, name, text);
	obs_property_set_modified_callback(property, setting_modified);
	return property;
}

static bool preset_modified(obs_properties_t *props, obs_property_t *property,
			    obs_data_t *settings)
{
	struct beauty_filter *filter = obs_properties_get_param(props);
	const char *id = obs_data_get_string(settings, S_PRESET);

	UNUSED_PARAMETER(property);

	if (beauty_preset_is_custom_id(id)) {
		const char *name = beauty_preset_custom_name(id);

		if (beauty_custom_preset_apply(settings, name)) {
			obs_data_set_string(settings, S_PRESET_NAME, name);
			beauty_set_status(filter, NULL);
		} else {
			obs_data_set_string(settings, S_PRESET, "custom");
			beauty_set_status(filter, T_PRESET_STATUS_MISSING);
		}
	} else {
		const struct beauty_preset *preset = beauty_preset_find(id);

		if (preset) {
			beauty_preset_apply(settings, preset);
			obs_data_set_string(settings, S_PRESET, preset->id);
			obs_data_set_string(settings, S_PRESET_NAME, "");
			beauty_set_status(filter, NULL);
		} else if (strcmp(id, "custom") == 0) {
			beauty_set_status(filter, NULL);
		}
	}

	beauty_update_visibility(props, obs_data_get_bool(settings, S_SKIN_DETECT),
				 obs_data_get_bool(settings, S_FACE_AWARE),
				 obs_data_get_bool(settings, S_FACE_MANUAL));

	/* Refresh every control so the applied preset is visible immediately. */
	return true;
}

static void beauty_update_visibility(obs_properties_t *props, bool skin_detect, bool face_aware,
				     bool face_manual)
{
	const bool skin_or_face = skin_detect || face_aware;

	beauty_is_visible(props, S_SKIN_SENSITIVITY, skin_or_face);
	beauty_is_visible(props, S_MASK_PREVIEW, skin_or_face);

	beauty_is_visible(props, S_TEMPORAL, face_aware);
	beauty_is_visible(props, S_TEETH, face_aware);
	beauty_is_visible(props, S_EYES, face_aware);
	beauty_is_visible(props, S_ZONE_PREVIEW, face_aware);
	beauty_is_visible(props, S_FACE_MANUAL, face_aware);

	beauty_is_visible(props, S_FACE_CX, face_aware && face_manual);
	beauty_is_visible(props, S_FACE_CY, face_aware && face_manual);
	beauty_is_visible(props, S_FACE_W, face_aware && face_manual);
	beauty_is_visible(props, S_FACE_H, face_aware && face_manual);
}

static bool skin_detect_modified(obs_properties_t *props, obs_property_t *property,
				 obs_data_t *settings)
{
	const bool skin = obs_data_get_bool(settings, S_SKIN_DETECT);
	const bool face = obs_data_get_bool(settings, S_FACE_AWARE);
	const bool manual = obs_data_get_bool(settings, S_FACE_MANUAL);

	UNUSED_PARAMETER(property);

	beauty_set_status(obs_properties_get_param(props), NULL);
	beauty_mark_custom(settings);
	beauty_update_visibility(props, skin, face, manual);
	return true;
}

static bool face_aware_modified(obs_properties_t *props, obs_property_t *property,
				obs_data_t *settings)
{
	const bool skin = obs_data_get_bool(settings, S_SKIN_DETECT);
	const bool face = obs_data_get_bool(settings, S_FACE_AWARE);
	const bool manual = obs_data_get_bool(settings, S_FACE_MANUAL);

	UNUSED_PARAMETER(property);

	beauty_set_status(obs_properties_get_param(props), NULL);
	beauty_mark_custom(settings);
	beauty_update_visibility(props, skin, face, manual);
	return true;
}

static bool face_manual_modified(obs_properties_t *props, obs_property_t *property,
				 obs_data_t *settings)
{
	const bool skin = obs_data_get_bool(settings, S_SKIN_DETECT);
	const bool face = obs_data_get_bool(settings, S_FACE_AWARE);
	const bool manual = obs_data_get_bool(settings, S_FACE_MANUAL);

	UNUSED_PARAMETER(property);

	beauty_set_status(obs_properties_get_param(props), NULL);
	beauty_update_visibility(props, skin, face, manual);
	return true;
}

static obs_properties_t *beauty_properties(void *data)
{
	struct beauty_filter *filter = data;
	obs_properties_t *props = obs_properties_create_param(filter, NULL);
	obs_properties_t *group;
	obs_property_t *info;
	obs_property_t *p;

	if (filter && (filter->effect_failed || filter->resource_failed)) {
		info = obs_properties_add_text(props, S_ERROR_INFO, T_ERROR_INFO,
					       OBS_TEXT_INFO);
		obs_property_text_set_info_type(info, OBS_TEXT_INFO_ERROR);
		obs_property_text_set_info_word_wrap(info, true);
	}

	info = obs_properties_add_text(props, S_HDR_INFO, T_HDR_INFO, OBS_TEXT_INFO);
	obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
	obs_property_text_set_info_word_wrap(info, true);

	/* --- Starting preset ------------------------------------------------- */
	p = obs_properties_add_list(props, S_PRESET, T_PRESET, OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_PRESET_CUSTOM, "custom");
	for (size_t i = 0; i < beauty_preset_count(); i++) {
		const struct beauty_preset *preset = beauty_preset_at(i);
		char locale_key[64];

		snprintf(locale_key, sizeof(locale_key), "Preset.%s", preset->id);
		obs_property_list_add_string(p, obs_module_text(locale_key), preset->id);
	}
	for (size_t i = 0; i < beauty_custom_preset_count(); i++) {
		char name[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];
		char label[160];
		char value[sizeof(BEAUTY_CUSTOM_PREFIX) + BEAUTY_CUSTOM_PRESET_MAX_NAME];

		if (!beauty_custom_preset_name_at(i, name, sizeof(name)))
			continue;

		snprintf(label, sizeof(label), T_PRESET_CUSTOM_ENTRY, name);
		snprintf(value, sizeof(value), "%s%s", BEAUTY_CUSTOM_PREFIX, name);
		obs_property_list_add_string(p, label, value);
	}
	obs_property_set_modified_callback(p, preset_modified);

	obs_properties_add_text(props, S_PRESET_NAME, T_PRESET_NAME, OBS_TEXT_DEFAULT);
	obs_properties_add_button(props, "preset_save", T_PRESET_SAVE, save_preset_clicked);
	obs_properties_add_button(props, "preset_delete", T_PRESET_DELETE,
				  delete_preset_clicked);
	obs_properties_add_button(props, "preset_reset", T_PRESET_RESET, reset_settings_clicked);

	if (filter && filter->preset_status[0]) {
		info = obs_properties_add_text(props, "preset_status_info",
					       filter->preset_status, OBS_TEXT_INFO);
		obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
		obs_property_text_set_info_word_wrap(info, true);
	}

	/* --- Skin smoothing ------------------------------------------------- */
	group = obs_properties_create();
	add_float_setting(group, S_AMOUNT, T_AMOUNT, 0.0, 1.0, 0.001);
	add_float_setting(group, S_RADIUS, T_RADIUS, 0.0, 24.0, 0.5);
	add_float_setting(group, S_DETAIL, T_DETAIL, 0.0, 1.0, 0.01);
	p = obs_properties_add_bool(group, S_SKIN_DETECT, T_SKIN_DETECT);
	obs_property_set_modified_callback(p, skin_detect_modified);
	add_float_setting(group, S_SKIN_SENSITIVITY, T_SKIN_SENSITIVITY, 0.0, 1.0, 0.01);
	add_bool_setting(group, S_MASK_PREVIEW, T_MASK_PREVIEW);
	obs_properties_add_group(props, "smoothing_group", T_GROUP_SMOOTHING, OBS_GROUP_NORMAL,
				 group);

	/* --- Complexion ----------------------------------------------------- */
	group = obs_properties_create();
	add_float_setting(group, S_BRIGHTNESS, T_BRIGHTNESS, -50.0, 50.0, 1.0);
	add_float_setting(group, S_WARMTH, T_WARMTH, -50.0, 50.0, 1.0);
	add_float_setting(group, S_SATURATION, T_SATURATION, -50.0, 50.0, 1.0);
	add_float_setting(group, S_ROSY, T_ROSY, 0.0, 1.0, 0.01);
	obs_properties_add_group(props, "complexion_group", T_GROUP_COMPLEXION, OBS_GROUP_NORMAL,
				 group);

	/* --- Detail & light ------------------------------------------------- */
	group = obs_properties_create();
	add_float_setting(group, S_CLARITY, T_CLARITY, 0.0, 1.0, 0.01);
	add_float_setting(group, S_GLOW, T_GLOW, 0.0, 1.0, 0.01);
	add_float_setting(group, S_LIGHT_BALANCE, T_LIGHT_BALANCE, 0.0, 1.0, 0.01);
	obs_properties_add_group(props, "detail_group", T_GROUP_DETAIL, OBS_GROUP_NORMAL, group);

	/* --- Face & eyes ---------------------------------------------------- */
	group = obs_properties_create();
	p = obs_properties_add_bool(group, S_FACE_AWARE, T_FACE_AWARE);
	obs_property_set_modified_callback(p, face_aware_modified);
	add_float_setting(group, S_TEMPORAL, T_TEMPORAL, 0.0, 1.0, 0.01);
	add_float_setting(group, S_TEETH, T_TEETH, 0.0, 1.0, 0.01);
	add_float_setting(group, S_EYES, T_EYES, 0.0, 1.0, 0.01);
	add_bool_setting(group, S_ZONE_PREVIEW, T_ZONE_PREVIEW);
	p = obs_properties_add_bool(group, S_FACE_MANUAL, T_FACE_MANUAL);
	obs_property_set_modified_callback(p, face_manual_modified);
	add_float_setting(group, S_FACE_CX, T_FACE_CX, 0.0, 1.0, 0.005);
	add_float_setting(group, S_FACE_CY, T_FACE_CY, 0.0, 1.0, 0.005);
	add_float_setting(group, S_FACE_W, T_FACE_W, 0.05, 1.0, 0.005);
	add_float_setting(group, S_FACE_H, T_FACE_H, 0.05, 1.0, 0.005);
	obs_properties_add_group(props, "face_group", T_GROUP_FACE, OBS_GROUP_NORMAL, group);

	/* --- Performance & preview ----------------------------------------- */
	group = obs_properties_create();
	p = obs_properties_add_list(group, S_QUALITY, T_QUALITY, OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_QUALITY_FAST, "fast");
	obs_property_list_add_string(p, T_QUALITY_BALANCED, "balanced");
	obs_property_list_add_string(p, T_QUALITY_HIGH, "high");
	obs_property_set_modified_callback(p, setting_modified);
	add_bool_setting(group, S_SPLIT_VIEW, T_SPLIT_VIEW);
	add_float_setting(group, S_OPACITY, T_OPACITY, 0.0, 1.0, 0.01);
	add_bool_setting(group, S_ANTI_BANDING, T_ANTI_BANDING);
	obs_properties_add_group(props, "advanced_group", T_GROUP_ADVANCED, OBS_GROUP_NORMAL,
				 group);

	/* Keep dependent controls consistent with the stored setting. */
	beauty_update_visibility(props, filter ? filter->skin_detect : true,
				 filter ? filter->face_aware : true,
				 filter ? filter->face_manual : false);

	return props;
}

static void beauty_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, S_PRESET, "custom");
	obs_data_set_default_string(settings, S_PRESET_NAME, "");
	obs_data_set_default_double(settings, S_AMOUNT, 0.35);
	obs_data_set_default_double(settings, S_RADIUS, 6.0);
	obs_data_set_default_double(settings, S_DETAIL, 0.35);
	obs_data_set_default_bool(settings, S_SKIN_DETECT, true);
	obs_data_set_default_double(settings, S_SKIN_SENSITIVITY, 0.50);
	obs_data_set_default_bool(settings, S_MASK_PREVIEW, false);

	obs_data_set_default_double(settings, S_BRIGHTNESS, 0.0);
	obs_data_set_default_double(settings, S_WARMTH, 0.0);
	obs_data_set_default_double(settings, S_SATURATION, 0.0);
	obs_data_set_default_double(settings, S_ROSY, 0.15);

	obs_data_set_default_double(settings, S_CLARITY, 0.10);
	obs_data_set_default_double(settings, S_GLOW, 0.05);
	obs_data_set_default_double(settings, S_LIGHT_BALANCE, 0.35);

	obs_data_set_default_bool(settings, S_FACE_AWARE, true);
	obs_data_set_default_double(settings, S_TEMPORAL, 0.60);
	obs_data_set_default_double(settings, S_TEETH, 0.0);
	obs_data_set_default_double(settings, S_EYES, 0.0);
	obs_data_set_default_bool(settings, S_ZONE_PREVIEW, false);
	obs_data_set_default_bool(settings, S_FACE_MANUAL, false);
	obs_data_set_default_double(settings, S_FACE_CX, 0.50);
	obs_data_set_default_double(settings, S_FACE_CY, 0.45);
	obs_data_set_default_double(settings, S_FACE_W, 0.32);
	obs_data_set_default_double(settings, S_FACE_H, 0.42);

	obs_data_set_default_string(settings, S_QUALITY, "balanced");
	obs_data_set_default_bool(settings, S_SPLIT_VIEW, false);
	obs_data_set_default_double(settings, S_OPACITY, 1.0);
	obs_data_set_default_bool(settings, S_ANTI_BANDING, true);
}

static void *beauty_create(obs_data_t *settings, obs_source_t *context)
{
	struct beauty_filter *filter = bzalloc(sizeof(*filter));
	char *effect_path = obs_module_file("beauty_filter.effect");

	filter->context = context;

	if (!effect_path) {
		blog(LOG_ERROR,
		     "[obs-beauty-filter] Could not find 'beauty_filter.effect'. "
		     "Check that the module data directory is installed correctly.");
		bfree(filter);
		return NULL;
	}

	obs_enter_graphics();
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	filter->effect_failed = !filter->effect;
	if (filter->effect) {
		filter->p_image = gs_effect_get_param_by_name(filter->effect, "image");
		filter->p_mask_tex = gs_effect_get_param_by_name(filter->effect, "mask_tex");
		filter->p_history_tex =
			gs_effect_get_param_by_name(filter->effect, "history_tex");
		filter->p_base_tex = gs_effect_get_param_by_name(filter->effect, "base_tex");
		filter->p_base_valid =
			gs_effect_get_param_by_name(filter->effect, "base_valid");
		filter->p_dither = gs_effect_get_param_by_name(filter->effect, "dither_strength");
		filter->p_texel_size = gs_effect_get_param_by_name(filter->effect, "texel_size");
		filter->p_analysis_texel =
			gs_effect_get_param_by_name(filter->effect, "analysis_texel");
		filter->p_radius = gs_effect_get_param_by_name(filter->effect, "radius");
		filter->p_amount = gs_effect_get_param_by_name(filter->effect, "amount");
		filter->p_detail = gs_effect_get_param_by_name(filter->effect, "detail");
		filter->p_skin_detect =
			gs_effect_get_param_by_name(filter->effect, "skin_detect");
		filter->p_skin_sensitivity =
			gs_effect_get_param_by_name(filter->effect, "skin_sensitivity");
		filter->p_brightness =
			gs_effect_get_param_by_name(filter->effect, "brightness");
		filter->p_warmth = gs_effect_get_param_by_name(filter->effect, "warmth");
		filter->p_saturation =
			gs_effect_get_param_by_name(filter->effect, "saturation");
		filter->p_rosy = gs_effect_get_param_by_name(filter->effect, "rosy");
		filter->p_clarity = gs_effect_get_param_by_name(filter->effect, "clarity");
		filter->p_glow = gs_effect_get_param_by_name(filter->effect, "glow");
		filter->p_light_balance =
			gs_effect_get_param_by_name(filter->effect, "light_balance");
		filter->p_auto_gain =
			gs_effect_get_param_by_name(filter->effect, "auto_gain");
		filter->p_opacity = gs_effect_get_param_by_name(filter->effect, "opacity");
		filter->p_mask_preview =
			gs_effect_get_param_by_name(filter->effect, "mask_preview");
		filter->p_split_view =
			gs_effect_get_param_by_name(filter->effect, "split_view");
		filter->p_temporal_strength =
			gs_effect_get_param_by_name(filter->effect, "temporal_strength");
		filter->p_history_valid =
			gs_effect_get_param_by_name(filter->effect, "history_valid");
		filter->p_use_mask_tex =
			gs_effect_get_param_by_name(filter->effect, "use_mask_tex");
		filter->p_face_aware =
			gs_effect_get_param_by_name(filter->effect, "face_aware");
		filter->p_face_valid =
			gs_effect_get_param_by_name(filter->effect, "face_valid");
		filter->p_face_center =
			gs_effect_get_param_by_name(filter->effect, "face_center");
		filter->p_face_size = gs_effect_get_param_by_name(filter->effect, "face_size");
		filter->p_teeth_whiten =
			gs_effect_get_param_by_name(filter->effect, "teeth_whiten");
		filter->p_eye_brighten =
			gs_effect_get_param_by_name(filter->effect, "eye_brighten");
		filter->p_zone_preview =
			gs_effect_get_param_by_name(filter->effect, "zone_preview");
		filter->p_hdr_scale =
			gs_effect_get_param_by_name(filter->effect, "hdr_scale");
		filter->p_is_hdr = gs_effect_get_param_by_name(filter->effect, "is_hdr");
	}
	obs_leave_graphics();

	if (!filter->effect) {
		blog(LOG_WARNING, "[obs-beauty-filter] Failed to load '%s'", effect_path);
		bfree(effect_path);
		bfree(filter);
		return NULL;
	}

	bfree(effect_path);
	beauty_update(filter, settings);
	return filter;
}

static void beauty_destroy(void *data)
{
	struct beauty_filter *filter = data;

	obs_enter_graphics();
	beauty_release_resources(filter);
	gs_effect_destroy(filter->effect);
	obs_leave_graphics();

	bfree(filter);
}

static const char *beauty_technique(enum beauty_quality quality)
{
	switch (quality) {
	case BEAUTY_QUALITY_FAST:
		return "DrawFast";
	case BEAUTY_QUALITY_HIGH:
		return "DrawHigh";
	case BEAUTY_QUALITY_BALANCED:
	default:
		return "DrawBalanced";
	}
}

static enum gs_color_space beauty_get_color_space(void *data, size_t count,
						  const enum gs_color_space *preferred_spaces)
{
	struct beauty_filter *filter = data;
	const enum gs_color_space potential_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
		GS_CS_709_SCRGB,
	};
	const enum gs_color_space source_space = obs_source_get_color_space(
		obs_filter_get_target(filter->context), OBS_COUNTOF(potential_spaces),
		potential_spaces);

	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);

	return source_space;
}

struct obs_source_info beauty_filter_info = {
	.id = "beauty_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = beauty_get_name,
	.create = beauty_create,
	.destroy = beauty_destroy,
	.update = beauty_update,
	.video_render = beauty_render,
	.get_properties = beauty_properties,
	.get_defaults = beauty_defaults,
	.video_get_color_space = beauty_get_color_space,
};
