/*
 * OBS Beauty Filter - built-in starting presets
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "beauty-presets.h"
#include "beauty-settings.h"

#include <stddef.h>
#include <string.h>

/*
 * These mirror the tables in docs/TUNING*.md.  A preset is only a starting
 * point: the property UI marks the selection as "custom" as soon as the user
 * changes a creative control.
 */
static const struct beauty_preset presets[] = {
	/* id          amount radius detail sens  bright warm sat   rosy  clar  glow
	 * face  temporal teeth eyes  quality    anti */
	{"default",    0.35,  6.0,   0.35,  0.50, 0.0,   0.0, 0.0,  0.15, 0.10, 0.05,
	 true, 0.60,    0.0,   0.0,   "balanced", true},
	{"natural",    0.35,  6.0,   0.40,  0.50, 0.0,   3.0, 0.0,  0.12, 0.10, 0.05,
	 true, 0.60,    0.0,   0.0,   "balanced", true},
	{"cinema",     0.28,  8.0,   0.55,  0.50, 0.0,   2.0, 0.0,  0.10, 0.08, 0.04,
	 true, 0.65,    0.20,  0.12,  "high",     true},
	{"studio",     0.60,  10.0,  0.28,  0.50, 4.0,   2.0, 2.0,  0.18, 0.12, 0.10,
	 true, 0.55,    0.35,  0.20,  "high",     true},
	{"soft",       0.45,  8.0,   0.30,  0.55, 3.0,   4.0, 4.0,  0.20, 0.08, 0.18,
	 true, 0.65,    0.25,  0.15,  "balanced", true},
	{"beard",      0.22,  5.0,   0.60,  0.45, 0.0,   0.0, 0.0,  0.05, 0.12, 0.0,
	 true, 0.50,    0.20,  0.10,  "balanced", true},
	{"dark_skin",  0.30,  6.0,   0.45,  0.70, 4.0,   2.0, 3.0,  0.08, 0.10, 0.05,
	 true, 0.60,    0.30,  0.15,  "balanced", true},
	{"hdr",        0.30,  6.0,   0.48,  0.50, 0.0,   0.0, 0.0,  0.08, 0.10, 0.03,
	 true, 0.60,    0.15,  0.10,  "high",     true},
};

bool beauty_preset_is_custom_id(const char *id)
{
	return id && strncmp(id, BEAUTY_CUSTOM_PREFIX, strlen(BEAUTY_CUSTOM_PREFIX)) == 0 &&
	       id[strlen(BEAUTY_CUSTOM_PREFIX)] != '\0';
}

const char *beauty_preset_custom_name(const char *id)
{
	if (!beauty_preset_is_custom_id(id))
		return NULL;

	return id + strlen(BEAUTY_CUSTOM_PREFIX);
}

const struct beauty_preset *beauty_preset_find(const char *id)
{
	size_t i;

	if (!id || !*id || strcmp(id, "custom") == 0 || beauty_preset_is_custom_id(id))
		return NULL;

	for (i = 0; i < beauty_preset_count(); i++) {
		if (strcmp(presets[i].id, id) == 0)
			return presets + i;
	}

	return NULL;
}

size_t beauty_preset_count(void)
{
	return sizeof(presets) / sizeof(presets[0]);
}

const struct beauty_preset *beauty_preset_at(size_t index)
{
	return index < beauty_preset_count() ? presets + index : NULL;
}

void beauty_preset_apply(obs_data_t *settings, const struct beauty_preset *preset)
{
	if (!settings || !preset)
		return;

	obs_data_set_double(settings, S_AMOUNT, preset->amount);
	obs_data_set_double(settings, S_RADIUS, preset->radius);
	obs_data_set_double(settings, S_DETAIL, preset->detail);
	obs_data_set_double(settings, S_SKIN_SENSITIVITY, preset->skin_sensitivity);
	obs_data_set_double(settings, S_BRIGHTNESS, preset->brightness);
	obs_data_set_double(settings, S_WARMTH, preset->warmth);
	obs_data_set_double(settings, S_SATURATION, preset->saturation);
	obs_data_set_double(settings, S_ROSY, preset->rosy);
	obs_data_set_double(settings, S_CLARITY, preset->clarity);
	obs_data_set_double(settings, S_GLOW, preset->glow);
	obs_data_set_bool(settings, S_FACE_AWARE, preset->face_aware);
	obs_data_set_double(settings, S_TEMPORAL, preset->temporal);
	obs_data_set_double(settings, S_TEETH, preset->teeth);
	obs_data_set_double(settings, S_EYES, preset->eyes);
	obs_data_set_string(settings, S_QUALITY, preset->quality);
	obs_data_set_bool(settings, S_ANTI_BANDING, preset->anti_banding);
}
