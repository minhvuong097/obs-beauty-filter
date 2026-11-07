/*
 * OBS Beauty Filter - user-defined preset persistence
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "beauty-custom-presets.h"
#include "beauty-presets.h"
#include "beauty-settings.h"

#include <obs-module.h>

#include <util/bmem.h>
#include <util/platform.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The file is intentionally small and human-editable:
 *
 * {
 *   "version": 1,
 *   "presets": [
 *     { "name": "My Look", "amount": 0.35, ... }
 *   ]
 * }
 */
static obs_data_array_t *custom_array;
static bool custom_loaded;

static bool names_equal_ci(const char *a, const char *b)
{
	if (!a || !b)
		return false;

	while (*a && *b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return false;
		a++;
		b++;
	}

	return *a == *b;
}

static char *custom_path(void)
{
	const char *override = getenv("OBS_BEAUTY_CUSTOM_PRESET_PATH");

	if (override && *override)
		return bstrdup(override);

#ifdef BEAUTY_CUSTOM_PRESETS_NO_MODULE_PATH
	/* Unit tests set the environment override above.  Do not reference
	 * obs_current_module() here: it is supplied by OBS_DECLARE_MODULE(),
	 * which standalone test binaries do not have. */
	return NULL;
#else
	char *path;
	char *slash;
	char *backslash;

	path = obs_module_config_path("custom-presets.json");
	if (!path)
		return NULL;

	/* Make sure <module-config>/ exists before the first save. */
	slash = strrchr(path, '/');
	backslash = strrchr(path, '\\');
	if (!slash || (backslash && backslash > slash))
		slash = backslash;
	if (slash) {
		*slash = '\0';
		os_mkdirs(path);
		*slash = '/';
	}

	return path;
#endif
}

void beauty_custom_presets_reload(void)
{
	if (custom_array)
		obs_data_array_release(custom_array);
	custom_array = NULL;
	custom_loaded = false;
}

static obs_data_array_t *custom_array_get(void)
{
	char *path;
	obs_data_t *root;

	if (custom_loaded)
		return custom_array;

	custom_loaded = true;
	custom_array = obs_data_array_create();

	path = custom_path();
	if (!path)
		return custom_array;

	const bool existed = os_file_exists(path);

	root = obs_data_create_from_json_file_safe(path, "bak");
	if (!root && existed)
		blog(LOG_WARNING,
		     "[obs-beauty-filter] Could not read custom presets from '%s'; "
		     "using an empty list",
		     path);
	bfree(path);
	if (!root)
		return custom_array;

	obs_data_array_t *loaded = obs_data_get_array(root, "presets");
	if (loaded) {
		obs_data_array_release(custom_array);
		custom_array = loaded;
	}
	obs_data_release(root);

	return custom_array;
}

static bool custom_write(void)
{
	char *path = custom_path();
	obs_data_t *root;
	bool ok;

	if (!path)
		return false;

	root = obs_data_create();
	obs_data_set_int(root, "version", 1);
	obs_data_set_array(root, "presets", custom_array_get());
	ok = obs_data_save_json_safe(root, path, "tmp", "bak");
	if (!ok)
		blog(LOG_WARNING, "[obs-beauty-filter] Could not write custom presets to '%s'",
		     path);
	obs_data_release(root);
	bfree(path);

	return ok;
}

bool beauty_custom_preset_name_normalise(const char *name, char *out, size_t out_size)
{
	const char *start;
	const char *end;
	size_t length;
	const char *p;

	if (!name || !out || out_size < 2)
		return false;

	start = name;
	while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
		start++;

	end = start + strlen(start);
	while (end > start) {
		const char last = end[-1];
		if (last != ' ' && last != '\t' && last != '\r' && last != '\n')
			break;
		end--;
	}

	length = (size_t)(end - start);
	if (length == 0 || length >= out_size || length > BEAUTY_CUSTOM_PRESET_MAX_NAME)
		return false;

	for (p = start; p < end; p++) {
		if ((unsigned char)*p < 0x20)
			return false;
	}

	memcpy(out, start, length);
	out[length] = '\0';

	if (names_equal_ci(out, "custom"))
		return false;

	for (size_t i = 0; i < beauty_preset_count(); i++) {
		const struct beauty_preset *preset = beauty_preset_at(i);

		if (preset && names_equal_ci(out, preset->id))
			return false;
	}

	return true;
}

static bool custom_find_index(const char *name, size_t *index)
{
	obs_data_array_t *array = custom_array_get();
	const size_t count = obs_data_array_count(array);
	size_t i;

	for (i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const bool match = item && names_equal_ci(obs_data_get_string(item, "name"), name);
		if (item)
			obs_data_release(item);
		if (match) {
			if (index)
				*index = i;
			return true;
		}
	}

	return false;
}

size_t beauty_custom_preset_count(void)
{
	return obs_data_array_count(custom_array_get());
}

bool beauty_custom_preset_name_at(size_t index, char *out, size_t out_size)
{
	obs_data_array_t *array = custom_array_get();
	obs_data_t *item;
	const char *name;

	if (!out || out_size == 0 || index >= obs_data_array_count(array))
		return false;

	item = obs_data_array_item(array, index);
	if (!item)
		return false;

	name = obs_data_get_string(item, "name");
	if (!name || !*name || strlen(name) >= out_size) {
		obs_data_release(item);
		return false;
	}

	strcpy(out, name);
	obs_data_release(item);
	return true;
}

bool beauty_custom_preset_exists(const char *name)
{
	char clean[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];

	if (!beauty_custom_preset_name_normalise(name, clean, sizeof(clean)))
		return false;

	return custom_find_index(clean, NULL);
}

static obs_data_t *settings_to_item(const char *name, obs_data_t *settings)
{
	obs_data_t *item = obs_data_create();

	obs_data_set_string(item, "name", name);

	obs_data_set_double(item, S_AMOUNT, obs_data_get_double(settings, S_AMOUNT));
	obs_data_set_double(item, S_RADIUS, obs_data_get_double(settings, S_RADIUS));
	obs_data_set_double(item, S_DETAIL, obs_data_get_double(settings, S_DETAIL));
	obs_data_set_bool(item, S_SKIN_DETECT, obs_data_get_bool(settings, S_SKIN_DETECT));
	obs_data_set_double(item, S_SKIN_SENSITIVITY,
			    obs_data_get_double(settings, S_SKIN_SENSITIVITY));
	obs_data_set_double(item, S_BRIGHTNESS, obs_data_get_double(settings, S_BRIGHTNESS));
	obs_data_set_double(item, S_WARMTH, obs_data_get_double(settings, S_WARMTH));
	obs_data_set_double(item, S_SATURATION, obs_data_get_double(settings, S_SATURATION));
	obs_data_set_double(item, S_ROSY, obs_data_get_double(settings, S_ROSY));
	obs_data_set_double(item, S_CLARITY, obs_data_get_double(settings, S_CLARITY));
	obs_data_set_double(item, S_GLOW, obs_data_get_double(settings, S_GLOW));
	obs_data_set_bool(item, S_FACE_AWARE, obs_data_get_bool(settings, S_FACE_AWARE));
	obs_data_set_double(item, S_TEMPORAL, obs_data_get_double(settings, S_TEMPORAL));
	obs_data_set_double(item, S_TEETH, obs_data_get_double(settings, S_TEETH));
	obs_data_set_double(item, S_EYES, obs_data_get_double(settings, S_EYES));
	obs_data_set_string(item, S_QUALITY, obs_data_get_string(settings, S_QUALITY));
	obs_data_set_bool(item, S_ANTI_BANDING, obs_data_get_bool(settings, S_ANTI_BANDING));

	return item;
}

bool beauty_custom_preset_save(const char *name, obs_data_t *settings)
{
	char clean[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];
	obs_data_array_t *array;
	obs_data_t *item;
	size_t index;

	if (!settings || !beauty_custom_preset_name_normalise(name, clean, sizeof(clean)))
		return false;

	array = custom_array_get();
	item = settings_to_item(clean, settings);

	if (custom_find_index(clean, &index))
		obs_data_array_erase(array, index);
	obs_data_array_push_back(array, item);
	obs_data_release(item);

	if (!custom_write()) {
		/* The in-memory list no longer matches the file. */
		beauty_custom_presets_reload();
		return false;
	}

	return true;
}

static double item_double(obs_data_t *item, const char *key, double fallback)
{
	return obs_data_has_user_value(item, key) ? obs_data_get_double(item, key) : fallback;
}

static bool item_bool(obs_data_t *item, const char *key, bool fallback)
{
	return obs_data_has_user_value(item, key) ? obs_data_get_bool(item, key) : fallback;
}

static const char *item_string(obs_data_t *item, const char *key, const char *fallback)
{
	const char *value;

	if (!obs_data_has_user_value(item, key))
		return fallback;

	value = obs_data_get_string(item, key);
	return value && *value ? value : fallback;
}

bool beauty_custom_preset_apply(obs_data_t *settings, const char *name)
{
	char clean[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];
	const struct beauty_preset *fallback = beauty_preset_find("default");
	obs_data_array_t *array;
	obs_data_t *item;
	size_t index;

	if (!settings || !fallback ||
	    !beauty_custom_preset_name_normalise(name, clean, sizeof(clean)))
		return false;

	array = custom_array_get();
	if (!custom_find_index(clean, &index))
		return false;

	item = obs_data_array_item(array, index);
	if (!item)
		return false;

	obs_data_set_double(settings, S_AMOUNT,
			    item_double(item, S_AMOUNT, fallback->amount));
	obs_data_set_double(settings, S_RADIUS,
			    item_double(item, S_RADIUS, fallback->radius));
	obs_data_set_double(settings, S_DETAIL,
			    item_double(item, S_DETAIL, fallback->detail));
	obs_data_set_bool(settings, S_SKIN_DETECT, item_bool(item, S_SKIN_DETECT, true));
	obs_data_set_double(settings, S_SKIN_SENSITIVITY,
			    item_double(item, S_SKIN_SENSITIVITY,
					fallback->skin_sensitivity));
	obs_data_set_double(settings, S_BRIGHTNESS,
			    item_double(item, S_BRIGHTNESS, fallback->brightness));
	obs_data_set_double(settings, S_WARMTH, item_double(item, S_WARMTH, fallback->warmth));
	obs_data_set_double(settings, S_SATURATION,
			    item_double(item, S_SATURATION, fallback->saturation));
	obs_data_set_double(settings, S_ROSY, item_double(item, S_ROSY, fallback->rosy));
	obs_data_set_double(settings, S_CLARITY, item_double(item, S_CLARITY, fallback->clarity));
	obs_data_set_double(settings, S_GLOW, item_double(item, S_GLOW, fallback->glow));
	obs_data_set_bool(settings, S_FACE_AWARE, item_bool(item, S_FACE_AWARE, true));
	obs_data_set_double(settings, S_TEMPORAL,
			    item_double(item, S_TEMPORAL, fallback->temporal));
	obs_data_set_double(settings, S_TEETH, item_double(item, S_TEETH, fallback->teeth));
	obs_data_set_double(settings, S_EYES, item_double(item, S_EYES, fallback->eyes));
	obs_data_set_string(settings, S_QUALITY,
			    item_string(item, S_QUALITY, fallback->quality));
	obs_data_set_bool(settings, S_ANTI_BANDING,
			  item_bool(item, S_ANTI_BANDING, fallback->anti_banding));

	obs_data_release(item);
	return true;
}

bool beauty_custom_preset_delete(const char *name)
{
	char clean[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];
	size_t index;

	if (!beauty_custom_preset_name_normalise(name, clean, sizeof(clean)))
		return false;

	if (!custom_find_index(clean, &index))
		return false;

	obs_data_array_erase(custom_array_get(), index);
	if (!custom_write()) {
		beauty_custom_presets_reload();
		return false;
	}

	return true;
}
