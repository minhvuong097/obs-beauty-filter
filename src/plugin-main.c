/*
 * OBS Beauty Filter - module entry point
 *
 * Copyright (C) 2026 OBS Beauty Filter contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "beauty-filter.h"

#ifndef BEAUTY_VERSION
#define BEAUTY_VERSION "dev"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-beauty-filter", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_author(void)
{
	return "OBS Beauty Filter contributors";
}

bool obs_module_load(void)
{
	/* One line in the OBS log makes bug reports actionable: the release
	 * archives carry no separate version file, and OBS does not print a
	 * plugin version on its own. */
	blog(LOG_INFO, "[obs-beauty-filter] version %s loaded (GPL-2.0-or-later)",
	     BEAUTY_VERSION);
	obs_register_source(&beauty_filter_info);
	return true;
}
