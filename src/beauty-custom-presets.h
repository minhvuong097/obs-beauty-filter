#pragma once

/*
 * OBS Beauty Filter - user-defined preset persistence
 *
 * Custom presets are stored as a small JSON array in the module config
 * directory, so they survive scene-collection changes and OBS restarts.
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <obs.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BEAUTY_CUSTOM_PRESET_MAX_NAME 64

/* Trims surrounding whitespace, rejects empty/reserved/control-character
 * names and writes the cleaned name to out.  Returns false on error. */
bool beauty_custom_preset_name_normalise(const char *name, char *out, size_t out_size);

size_t beauty_custom_preset_count(void);
bool beauty_custom_preset_name_at(size_t index, char *out, size_t out_size);
bool beauty_custom_preset_exists(const char *name);

/* Saves the creative controls from settings under name.  Overwrites an
 * existing preset with the same name (case-insensitive). */
bool beauty_custom_preset_save(const char *name, obs_data_t *settings);

/* Applies a saved preset.  Missing keys fall back to the built-in Default
 * preset, so hand-edited or older JSON files stay usable. */
bool beauty_custom_preset_apply(obs_data_t *settings, const char *name);

bool beauty_custom_preset_delete(const char *name);

/* Drops the in-memory cache.  The next call reloads from disk.  Useful for
 * tests and for picking up external edits. */
void beauty_custom_presets_reload(void);

#ifdef __cplusplus
}
#endif
