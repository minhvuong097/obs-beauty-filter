/*
 * Unit test for the built-in starting presets.
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "beauty-custom-presets.h"
#include "beauty-presets.h"
#include "beauty-settings.h"

#include <obs.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void expect(bool condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		failures++;
	}
}

static bool close_enough(double actual, double expected)
{
	return fabs(actual - expected) < 1e-9;
}

static void expect_double(obs_data_t *settings, const char *key, double expected, const char *message)
{
	const double actual = obs_data_get_double(settings, key);

	if (!close_enough(actual, expected)) {
		fprintf(stderr, "FAIL: %s (%s = %.6f, expected %.6f)\n", message, key, actual,
			expected);
		failures++;
	}
}

static bool quality_valid(const char *quality)
{
	return quality && (strcmp(quality, "fast") == 0 || strcmp(quality, "balanced") == 0 ||
			   strcmp(quality, "high") == 0);
}

static bool same_quality(const char *a, const char *b)
{
	return a && b && strcmp(a, b) == 0;
}

static void check_table(void)
{
	const size_t count = beauty_preset_count();
	size_t i;
	size_t j;

	expect(count == 8, "expected exactly 8 built-in presets");
	expect(beauty_preset_at(count) == NULL, "preset_at past the end must return NULL");

	for (i = 0; i < count; i++) {
		const struct beauty_preset *preset = beauty_preset_at(i);

		expect(preset != NULL, "preset_at(index) must return a preset");
		if (!preset)
			continue;

		expect(preset->id && *preset->id, "preset id must not be empty");
		expect(beauty_preset_find(preset->id) == preset,
		       "find(id) must return the same table entry");

		expect(preset->amount >= 0.0 && preset->amount <= 1.0, "amount out of range");
		expect(preset->radius >= 0.0 && preset->radius <= 24.0, "radius out of range");
		expect(preset->detail >= 0.0 && preset->detail <= 1.0, "detail out of range");
		expect(preset->skin_sensitivity >= 0.0 && preset->skin_sensitivity <= 1.0,
		       "skin sensitivity out of range");
		expect(preset->brightness >= -50.0 && preset->brightness <= 50.0,
		       "brightness out of range");
		expect(preset->warmth >= -50.0 && preset->warmth <= 50.0, "warmth out of range");
		expect(preset->saturation >= -50.0 && preset->saturation <= 50.0,
		       "saturation out of range");
		expect(preset->rosy >= 0.0 && preset->rosy <= 1.0, "rosy out of range");
		expect(preset->clarity >= 0.0 && preset->clarity <= 1.0, "clarity out of range");
		expect(preset->glow >= 0.0 && preset->glow <= 1.0, "glow out of range");
		expect(preset->temporal >= 0.0 && preset->temporal <= 1.0,
		       "temporal strength out of range");
		expect(preset->teeth >= 0.0 && preset->teeth <= 1.0, "teeth out of range");
		expect(preset->eyes >= 0.0 && preset->eyes <= 1.0, "eyes out of range");
		expect(quality_valid(preset->quality), "unknown quality value");

		for (j = i + 1; j < count; j++) {
			const struct beauty_preset *other = beauty_preset_at(j);

			expect(other && strcmp(preset->id, other->id) != 0,
			       "preset ids must be unique");
		}
	}
}

static void check_lookup(void)
{
	expect(beauty_preset_find(NULL) == NULL, "NULL id must be custom");
	expect(beauty_preset_find("") == NULL, "empty id must be custom");
	expect(beauty_preset_find("custom") == NULL, "custom must have no preset entry");
	expect(beauty_preset_find("does-not-exist") == NULL, "unknown id must be rejected");
	expect(beauty_preset_find("natural") != NULL, "natural preset must exist");
	expect(beauty_preset_find("cinema") != NULL, "cinema preset must exist");

	expect(beauty_preset_is_custom_id("custom:Look"), "custom:Look must be a custom id");
	expect(!beauty_preset_is_custom_id("custom"), "bare custom must not be a custom id");
	expect(!beauty_preset_is_custom_id("custom:"), "empty custom name must be rejected");
	expect(strcmp(beauty_preset_custom_name("custom:Look"), "Look") == 0,
	       "custom name must be extracted");
	expect(beauty_preset_custom_name("natural") == NULL,
	       "built-in id must not yield a custom name");
}

static void check_apply(void)
{
	const struct beauty_preset *cinema = beauty_preset_find("cinema");
	obs_data_t *settings = obs_data_create();

	expect(cinema != NULL, "cinema preset must exist");
	if (!cinema || !settings) {
		if (settings)
			obs_data_release(settings);
		return;
	}

	obs_data_set_string(settings, S_PRESET, "custom");
	beauty_preset_apply(settings, cinema);

	expect_double(settings, S_AMOUNT, cinema->amount, "apply amount");
	expect_double(settings, S_RADIUS, cinema->radius, "apply radius");
	expect_double(settings, S_DETAIL, cinema->detail, "apply detail");
	expect_double(settings, S_SKIN_SENSITIVITY, cinema->skin_sensitivity,
		      "apply skin sensitivity");
	expect_double(settings, S_BRIGHTNESS, cinema->brightness, "apply brightness");
	expect_double(settings, S_WARMTH, cinema->warmth, "apply warmth");
	expect_double(settings, S_SATURATION, cinema->saturation, "apply saturation");
	expect_double(settings, S_ROSY, cinema->rosy, "apply rosy");
	expect_double(settings, S_CLARITY, cinema->clarity, "apply clarity");
	expect_double(settings, S_GLOW, cinema->glow, "apply glow");
	expect(obs_data_get_bool(settings, S_FACE_AWARE) == cinema->face_aware,
	       "apply face-aware");
	expect_double(settings, S_TEMPORAL, cinema->temporal, "apply temporal");
	expect_double(settings, S_TEETH, cinema->teeth, "apply teeth");
	expect_double(settings, S_EYES, cinema->eyes, "apply eyes");
	expect(same_quality(obs_data_get_string(settings, S_QUALITY), cinema->quality),
	       "apply quality");
	expect(obs_data_get_bool(settings, S_ANTI_BANDING) == cinema->anti_banding,
	       "apply anti-banding");

	/* The preset key is owned by the property callback, not by apply(). */
	expect(strcmp(obs_data_get_string(settings, S_PRESET), "custom") == 0,
	       "apply must not overwrite the preset selection");

	obs_data_release(settings);
}

static void check_default_preset(void)
{
	const struct beauty_preset *preset = beauty_preset_find("default");
	obs_data_t *settings = obs_data_create();

	expect(preset != NULL, "default preset must exist");
	if (!preset || !settings) {
		if (settings)
			obs_data_release(settings);
		return;
	}

	beauty_preset_apply(settings, preset);

	/* Keep this in sync with beauty_defaults() in src/beauty-filter.c. */
	expect_double(settings, S_AMOUNT, 0.35, "default amount");
	expect_double(settings, S_RADIUS, 6.0, "default radius");
	expect_double(settings, S_DETAIL, 0.35, "default detail");
	expect_double(settings, S_SKIN_SENSITIVITY, 0.50, "default skin sensitivity");
	expect_double(settings, S_BRIGHTNESS, 0.0, "default brightness");
	expect_double(settings, S_WARMTH, 0.0, "default warmth");
	expect_double(settings, S_SATURATION, 0.0, "default saturation");
	expect_double(settings, S_ROSY, 0.15, "default rosy");
	expect_double(settings, S_CLARITY, 0.10, "default clarity");
	expect_double(settings, S_GLOW, 0.05, "default glow");
	expect(obs_data_get_bool(settings, S_FACE_AWARE), "default face-aware");
	expect_double(settings, S_TEMPORAL, 0.60, "default temporal");
	expect_double(settings, S_TEETH, 0.0, "default teeth");
	expect_double(settings, S_EYES, 0.0, "default eyes");
	expect(same_quality(obs_data_get_string(settings, S_QUALITY), "balanced"),
	       "default quality");
	expect(obs_data_get_bool(settings, S_ANTI_BANDING), "default anti-banding");

	obs_data_release(settings);
}

static void check_null_arguments(void)
{
	const struct beauty_preset *preset = beauty_preset_find("natural");
	obs_data_t *settings = obs_data_create();

	if (!settings)
		return;

	obs_data_set_double(settings, S_AMOUNT, 0.42);
	beauty_preset_apply(NULL, preset);
	beauty_preset_apply(settings, NULL);

	expect_double(settings, S_AMOUNT, 0.42, "NULL arguments must leave settings alone");
	obs_data_release(settings);
}

static void set_custom_path(const char *path)
{
#ifdef _WIN32
	_putenv_s("OBS_BEAUTY_CUSTOM_PRESET_PATH", path);
#else
	setenv("OBS_BEAUTY_CUSTOM_PRESET_PATH", path, 1);
#endif
}

static void check_custom_presets(void)
{
	const char *path = "beauty-custom-preset-test.json";
	const char *backup = "beauty-custom-preset-test.json.bak";
	const char *temp = "beauty-custom-preset-test.json.tmp";
	const struct beauty_preset *cinema = beauty_preset_find("cinema");
	char name[BEAUTY_CUSTOM_PRESET_MAX_NAME + 1];
	char long_name[BEAUTY_CUSTOM_PRESET_MAX_NAME + 2];
	obs_data_t *settings;
	obs_data_t *applied;

	remove(path);
	remove(backup);
	remove(temp);
	set_custom_path(path);
	beauty_custom_presets_reload();

	expect(beauty_custom_preset_count() == 0, "custom preset store must start empty");
	expect(!beauty_custom_preset_name_normalise(NULL, name, sizeof(name)),
	       "NULL custom name must be rejected");
	expect(!beauty_custom_preset_name_normalise("", name, sizeof(name)),
	       "empty custom name must be rejected");
	expect(!beauty_custom_preset_name_normalise("   ", name, sizeof(name)),
	       "whitespace-only custom name must be rejected");
	expect(!beauty_custom_preset_name_normalise("custom", name, sizeof(name)),
	       "reserved custom name must be rejected");
	expect(!beauty_custom_preset_name_normalise("CUSTOM", name, sizeof(name)),
	       "reserved custom name must be case-insensitive");
	expect(!beauty_custom_preset_name_normalise("natural", name, sizeof(name)),
	       "built-in name must be rejected");
	expect(!beauty_custom_preset_name_normalise("NATURAL", name, sizeof(name)),
	       "built-in name must be rejected case-insensitively");
	expect(!beauty_custom_preset_name_normalise("bad\nname", name, sizeof(name)),
	       "control characters must be rejected");

	memset(long_name, 'x', sizeof(long_name));
	long_name[sizeof(long_name) - 1] = '\0';
	expect(!beauty_custom_preset_name_normalise(long_name, name, sizeof(name)),
	       "over-long custom name must be rejected");

	expect(beauty_custom_preset_name_normalise("  My Look  ", name, sizeof(name)),
	       "valid custom name must be accepted");
	expect(strcmp(name, "My Look") == 0, "custom name must be trimmed");

	expect(cinema != NULL, "cinema preset must exist for custom tests");
	if (!cinema)
		return;

	settings = obs_data_create();
	if (!settings)
		return;

	beauty_preset_apply(settings, cinema);
	obs_data_set_bool(settings, S_SKIN_DETECT, false);
	obs_data_set_double(settings, S_AMOUNT, 0.42);

	expect(!beauty_custom_preset_save("natural", settings),
	       "saving with a built-in name must fail");
	expect(beauty_custom_preset_save("  My Look  ", settings),
	       "valid custom preset must save");
	expect(beauty_custom_preset_count() == 1, "one custom preset must be listed");
	expect(beauty_custom_preset_name_at(0, name, sizeof(name)) &&
		       strcmp(name, "My Look") == 0,
	       "custom preset name must round-trip");
	expect(beauty_custom_preset_exists("my look"),
	       "custom preset lookup must be case-insensitive");
	expect(!beauty_custom_preset_apply(NULL, "My Look"),
	       "custom apply must reject NULL settings");
	expect(!beauty_custom_preset_apply(settings, "missing"),
	       "custom apply must reject unknown names");

	/* Reload from disk to verify persistence rather than the cache. */
	beauty_custom_presets_reload();
	expect(beauty_custom_preset_count() == 1, "custom preset must survive a reload");

	applied = obs_data_create();
	if (applied) {
		beauty_preset_apply(applied, beauty_preset_find("default"));
		expect(beauty_custom_preset_apply(applied, "MY LOOK"),
		       "custom preset must apply case-insensitively");
		expect_double(applied, S_AMOUNT, 0.42, "custom apply amount");
		expect_double(applied, S_RADIUS, cinema->radius, "custom apply radius");
		expect(!obs_data_get_bool(applied, S_SKIN_DETECT),
		       "custom preset must restore skin detection");
		obs_data_release(applied);
	}

	/* Saving the same name must overwrite, not duplicate. */
	obs_data_set_double(settings, S_AMOUNT, 0.77);
	expect(beauty_custom_preset_save("my look", settings),
	       "overwriting a custom preset must succeed");
	expect(beauty_custom_preset_count() == 1, "overwrite must not duplicate the preset");

	beauty_custom_presets_reload();
	applied = obs_data_create();
	if (applied) {
		beauty_preset_apply(applied, beauty_preset_find("default"));
		expect(beauty_custom_preset_apply(applied, "My Look"),
		       "overwritten preset must apply");
		expect_double(applied, S_AMOUNT, 0.77, "overwritten amount must persist");
		obs_data_release(applied);
	}

	expect(beauty_custom_preset_delete("MY LOOK"), "custom preset must delete");
	expect(beauty_custom_preset_count() == 0, "custom preset store must be empty again");
	expect(!beauty_custom_preset_delete("My Look"), "deleting a missing preset must fail");

	obs_data_release(settings);
	remove(path);
	remove(backup);
	remove(temp);
	beauty_custom_presets_reload();
}

int main(void)
{
	/* obs_data_* is self-contained; no graphics/display or obs_startup(). */
	check_table();
	check_lookup();
	check_apply();
	check_default_preset();
	check_null_arguments();
	check_custom_presets();

	if (failures) {
		fprintf(stderr, "%d preset test failure(s)\n", failures);
		return 1;
	}

	printf("OK: built-in and custom presets, lookup, apply, defaults and NULL handling "
	       "verified\n");
	return 0;
}
