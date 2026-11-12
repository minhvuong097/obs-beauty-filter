#pragma once

/*
 * Shared obs_data setting names for obs-beauty-filter.
 *
 * Keep these in one place so the UI, preset table and tests cannot drift
 * apart.  The T_* locale keys live in beauty-filter.c because they are
 * resolved at runtime through obs_module_text().
 */

/* clang-format off */

#define S_PRESET                "preset"
#define S_PRESET_NAME           "preset_name"
#define S_HDR_INFO              "hdr_info"
#define S_ERROR_INFO            "error_info"

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
#define S_LIGHT_BALANCE         "light_balance"

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
#define S_ANTI_BANDING          "anti_banding"

/* clang-format on */
