#pragma once

#include <obs-module.h>
#include <graphics/graphics.h>

/*
 * Frame Hold filter
 *
 * Behaviour:
 *  - Every 1/FPS seconds, captures a fresh copy of the parent source's
 *    rendered texture into an internal render target.
 *  - On every other frame, instead of forwarding the live video, it
 *    redraws the last captured texture, effectively "holding" (freezing)
 *    the image between captures.
 *  - When disabled, the filter becomes a pass-through (live video).
 */
struct frame_hold_filter {
	obs_source_t *source;

	/* settings */
	bool enabled;
	double fps;
	float interval; /* seconds between captures = 1.0f / fps */

	/* runtime state */
	float elapsed;
	bool need_capture;
	bool has_texture;
	uint32_t width;
	uint32_t height;

	gs_texrender_t *texrender;
};

extern struct obs_source_info frame_hold_filter_info;
