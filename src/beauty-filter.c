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

#include "beauty-filter.h"
#include "face-region.h"

#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec4.h>

#include <math.h>
#include <string.h>

/* clang-format off */

#define S_HDR_INFO              "hdr_info"

#define S_AMOUNT                "amount"
#define S_RADIUS                "radius"
#define S_DETAIL                "detail"
#define S_SKIN_DETECT           "skin_detect"
#define S_SKIN_SENSITIVITY      "skin_sensitivity"
#define S_MASK_PREVIEW          "mask_preview"

#define S_BRIGHTNESS            "brightness"
#define S_WARMTH                "warmth"
#define S_SATURATION            "saturation"
#define S_ROSY                  "rosy"

#define S_CLARITY               "clarity"
#define S_GLOW                  "glow"

#define S_FACE_AWARE            "face_aware"
#define S_TEMPORAL              "temporal_strength"
#define S_TEETH                 "teeth_whiten"
#define S_EYES                  "eye_brighten"
#define S_ZONE_PREVIEW          "zone_preview"
#define S_FACE_MANUAL           "face_manual"
#define S_FACE_CX               "face_cx"
#define S_FACE_CY               "face_cy"
#define S_FACE_W                "face_w"
#define S_FACE_H                "face_h"

#define S_QUALITY               "quality"
#define S_SPLIT_VIEW            "split_view"
#define S_OPACITY               "opacity"

#define T_HDR_INFO              obs_module_text("HdrInfo")

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

#define T_QUALITY_FAST          obs_module_text("Quality.Fast")
#define T_QUALITY_BALANCED      obs_module_text("Quality.Balanced")
#define T_QUALITY_HIGH          obs_module_text("Quality.High")

/* clang-format on */

#define FACE_READBACK_INTERVAL 6
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
	float opacity;

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

	enum beauty_quality quality;

	/* Graphics resources, created lazily on the render thread. */
	gs_texrender_t *src_texrender;
	gs_texrender_t *mask_texrender[2];
	gs_texrender_t *analysis_texrender;
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

	bool resources_ready;
	bool history_valid;
	int mask_index;
	uint32_t frame_counter;

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
	gs_stagesurface_destroy(filter->analysis_stage);
	gs_texture_destroy(filter->white_tex);

	filter->src_texrender = NULL;
	filter->mask_texrender[0] = NULL;
	filter->mask_texrender[1] = NULL;
	filter->analysis_texrender = NULL;
	filter->analysis_stage = NULL;
	filter->white_tex = NULL;
	filter->resources_ready = false;
	filter->history_valid = false;
	filter->mask_index = 0;
	filter->frame_counter = 0;
	filter->auto_face_valid = false;
	filter->face_miss_updates = 0;
	memset(&filter->auto_face, 0, sizeof(filter->auto_face));
}

static bool beauty_ensure_resources(struct beauty_filter *filter, uint32_t width, uint32_t height,
				    enum gs_color_space space)
{
	static const uint8_t white_pixel[4] = {255, 255, 255, 255};
	const uint8_t *white_ptr = white_pixel;

	if (filter->resources_ready && filter->width == width && filter->height == height &&
	    filter->space == space)
		return true;

	beauty_release_resources(filter);

	filter->width = width;
	filter->height = height;
	filter->space = space;
	filter->src_format = gs_get_format_from_space(space);
	filter->mask_width = (width + 1) / 2;
	filter->mask_height = (height + 1) / 2;

	filter->analysis_width = FACE_GRID_WIDTH;
	filter->analysis_height =
		(uint32_t)lroundf((double)FACE_GRID_WIDTH * (double)height / (double)width);
	if (filter->analysis_height < 16)
		filter->analysis_height = 16;
	if (filter->analysis_height > FACE_GRID_MAX_HEIGHT)
		filter->analysis_height = FACE_GRID_MAX_HEIGHT;

	filter->src_texrender = gs_texrender_create(filter->src_format, GS_ZS_NONE);
	filter->mask_texrender[0] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->mask_texrender[1] = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->analysis_texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->analysis_stage =
		gs_stagesurface_create(filter->analysis_width, filter->analysis_height, GS_RGBA);
	filter->white_tex = gs_texture_create(1, 1, GS_RGBA, 1, &white_ptr, 0);

	if (!filter->src_texrender || !filter->mask_texrender[0] ||
	    !filter->mask_texrender[1] || !filter->analysis_texrender ||
	    !filter->analysis_stage || !filter->white_tex) {
		blog(LOG_ERROR, "[obs-beauty-filter] Failed to allocate GPU resources for %ux%u",
		     width, height);
		beauty_release_resources(filter);
		return false;
	}

	filter->resources_ready = true;
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

static void beauty_analyse_mask(struct beauty_filter *filter)
{
	gs_texture_t *mask_tex;
	struct vec2 analysis_texel;
	uint8_t *pixels = NULL;
	uint32_t linesize = 0;

	mask_tex = gs_texrender_get_texture(filter->mask_texrender[filter->mask_index ^ 1]);
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
		beauty_update_auto_face(filter, pixels, linesize);
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

	if (output_space == GS_CS_709_SCRGB) {
		float sdr_white = obs_get_video_sdr_white_level();
		if (sdr_white < 1.0f)
			sdr_white = 300.0f;
		hdr_scale = 80.0f / sdr_white;
		is_hdr = true;
	} else if (output_space == GS_CS_709_EXTENDED) {
		is_hdr = true;
	}

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

			/* Refresh the face box a few times per second.  The
			 * readback is tiny (<= 64x64 RGBA) and never touches the
			 * full-resolution frame. */
			filter->frame_counter++;
			if (filter->face_aware && !filter->face_manual &&
			    (filter->frame_counter % FACE_READBACK_INTERVAL) == 0) {
				beauty_analyse_mask(filter);
			}
		} else {
			mask_tex = filter->white_tex;
			use_mask_tex = false;
		}
	} else {
		mask_tex = filter->white_tex;
		filter->history_valid = false;
	}

	/* 3. Main processing pass. */
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

	beauty_update_visibility(props, skin, face, manual);
	return true;
}

static obs_properties_t *beauty_properties(void *data)
{
	struct beauty_filter *filter = data;
	obs_properties_t *props = obs_properties_create();
	obs_properties_t *group;
	obs_property_t *info;
	obs_property_t *p;

	info = obs_properties_add_text(props, S_HDR_INFO, T_HDR_INFO, OBS_TEXT_INFO);
	obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
	obs_property_text_set_info_word_wrap(info, true);

	/* --- Skin smoothing ------------------------------------------------- */
	group = obs_properties_create();
	obs_properties_add_float_slider(group, S_AMOUNT, T_AMOUNT, 0.0, 1.0, 0.001);
	obs_properties_add_float_slider(group, S_RADIUS, T_RADIUS, 0.0, 24.0, 0.5);
	obs_properties_add_float_slider(group, S_DETAIL, T_DETAIL, 0.0, 1.0, 0.01);
	p = obs_properties_add_bool(group, S_SKIN_DETECT, T_SKIN_DETECT);
	obs_property_set_modified_callback(p, skin_detect_modified);
	obs_properties_add_float_slider(group, S_SKIN_SENSITIVITY, T_SKIN_SENSITIVITY, 0.0, 1.0,
					0.01);
	obs_properties_add_bool(group, S_MASK_PREVIEW, T_MASK_PREVIEW);
	obs_properties_add_group(props, "smoothing_group", T_GROUP_SMOOTHING, OBS_GROUP_NORMAL,
				 group);

	/* --- Complexion ----------------------------------------------------- */
	group = obs_properties_create();
	obs_properties_add_float_slider(group, S_BRIGHTNESS, T_BRIGHTNESS, -50.0, 50.0, 1.0);
	obs_properties_add_float_slider(group, S_WARMTH, T_WARMTH, -50.0, 50.0, 1.0);
	obs_properties_add_float_slider(group, S_SATURATION, T_SATURATION, -50.0, 50.0, 1.0);
	obs_properties_add_float_slider(group, S_ROSY, T_ROSY, 0.0, 1.0, 0.01);
	obs_properties_add_group(props, "complexion_group", T_GROUP_COMPLEXION, OBS_GROUP_NORMAL,
				 group);

	/* --- Detail & glow -------------------------------------------------- */
	group = obs_properties_create();
	obs_properties_add_float_slider(group, S_CLARITY, T_CLARITY, 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(group, S_GLOW, T_GLOW, 0.0, 1.0, 0.01);
	obs_properties_add_group(props, "detail_group", T_GROUP_DETAIL, OBS_GROUP_NORMAL, group);

	/* --- Face & eyes ---------------------------------------------------- */
	group = obs_properties_create();
	p = obs_properties_add_bool(group, S_FACE_AWARE, T_FACE_AWARE);
	obs_property_set_modified_callback(p, face_aware_modified);
	obs_properties_add_float_slider(group, S_TEMPORAL, T_TEMPORAL, 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(group, S_TEETH, T_TEETH, 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(group, S_EYES, T_EYES, 0.0, 1.0, 0.01);
	obs_properties_add_bool(group, S_ZONE_PREVIEW, T_ZONE_PREVIEW);
	p = obs_properties_add_bool(group, S_FACE_MANUAL, T_FACE_MANUAL);
	obs_property_set_modified_callback(p, face_manual_modified);
	obs_properties_add_float_slider(group, S_FACE_CX, T_FACE_CX, 0.0, 1.0, 0.005);
	obs_properties_add_float_slider(group, S_FACE_CY, T_FACE_CY, 0.0, 1.0, 0.005);
	obs_properties_add_float_slider(group, S_FACE_W, T_FACE_W, 0.05, 1.0, 0.005);
	obs_properties_add_float_slider(group, S_FACE_H, T_FACE_H, 0.05, 1.0, 0.005);
	obs_properties_add_group(props, "face_group", T_GROUP_FACE, OBS_GROUP_NORMAL, group);

	/* --- Performance & preview ----------------------------------------- */
	group = obs_properties_create();
	p = obs_properties_add_list(group, S_QUALITY, T_QUALITY, OBS_COMBO_TYPE_LIST,
				    OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, T_QUALITY_FAST, "fast");
	obs_property_list_add_string(p, T_QUALITY_BALANCED, "balanced");
	obs_property_list_add_string(p, T_QUALITY_HIGH, "high");
	obs_properties_add_bool(group, S_SPLIT_VIEW, T_SPLIT_VIEW);
	obs_properties_add_float_slider(group, S_OPACITY, T_OPACITY, 0.0, 1.0, 0.01);
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
	if (filter->effect) {
		filter->p_image = gs_effect_get_param_by_name(filter->effect, "image");
		filter->p_mask_tex = gs_effect_get_param_by_name(filter->effect, "mask_tex");
		filter->p_history_tex =
			gs_effect_get_param_by_name(filter->effect, "history_tex");
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
