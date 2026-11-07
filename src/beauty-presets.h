#pragma once

/*
 * OBS Beauty Filter - built-in starting presets
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <obs.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct beauty_preset {
	const char *id;
	double amount;
	double radius;
	double detail;
	double skin_sensitivity;
	double brightness;
	double warmth;
	double saturation;
	double rosy;
	double clarity;
	double glow;
	bool face_aware;
	double temporal;
	double teeth;
	double eyes;
	const char *quality;
	bool anti_banding;
};

/* Custom presets are stored in the module config directory and addressed by
 * a "custom:<name>" id in the preset dropdown. */
#define BEAUTY_CUSTOM_PREFIX "custom:"

/* Returns NULL for "custom", an unknown id or a NULL id. */
const struct beauty_preset *beauty_preset_find(const char *id);

bool beauty_preset_is_custom_id(const char *id);
/* Returns the name part of a "custom:<name>" id, or NULL when id is not a
 * custom preset id.  The pointer aliases id. */
const char *beauty_preset_custom_name(const char *id);

size_t beauty_preset_count(void);
const struct beauty_preset *beauty_preset_at(size_t index);

/* Writes the tuning values into settings.  The preset key itself is not
 * touched; the property callback stores that separately. */
void beauty_preset_apply(obs_data_t *settings, const struct beauty_preset *preset);

#ifdef __cplusplus
}
#endif
