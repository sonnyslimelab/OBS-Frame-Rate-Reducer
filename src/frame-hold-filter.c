#include "frame-hold-filter.h"

#define SETTING_ENABLED "enabled"
#define SETTING_FPS "fps"

#define TEXT_ENABLED obs_module_text("Enabled")
#define TEXT_FPS obs_module_text("FPS")

/* ------------------------------------------------------------------- */

static const char *frame_hold_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("FrameHold");
}

static void frame_hold_update(void *data, obs_data_t *settings)
{
	struct frame_hold_filter *filter = data;

	bool was_enabled = filter->enabled;

	filter->enabled = obs_data_get_bool(settings, SETTING_ENABLED);
	filter->fps = obs_data_get_double(settings, SETTING_FPS);

	if (filter->fps < 1.0)
		filter->fps = 1.0;
	if (filter->fps > 60.0)
		filter->fps = 60.0;

	filter->interval = 1.0f / (float)filter->fps;

	/* Re-enabling (or first update) should force an immediate
	 * fresh capture rather than showing a stale/blank texture. */
	if (filter->enabled && !was_enabled) {
		filter->elapsed = 0.0f;
		filter->need_capture = true;
	}
}

static void frame_hold_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, SETTING_ENABLED, true);
	obs_data_set_default_double(settings, SETTING_FPS, 10.0);
}

static obs_properties_t *frame_hold_get_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_bool(props, SETTING_ENABLED, TEXT_ENABLED);

	obs_property_t *fps_slider = obs_properties_add_float_slider(
		props, SETTING_FPS, TEXT_FPS, 1.0, 60.0, 1.0);
	obs_property_float_set_suffix(fps_slider, " fps");

	return props;
}

/* ------------------------------------------------------------------- */

static void *frame_hold_create(obs_data_t *settings, obs_source_t *source)
{
	struct frame_hold_filter *filter = bzalloc(sizeof(struct frame_hold_filter));

	filter->source = source;
	filter->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	filter->need_capture = true;
	filter->has_texture = false;
	filter->elapsed = 0.0f;
	filter->width = 0;
	filter->height = 0;

	frame_hold_update(filter, settings);

	return filter;
}

static void frame_hold_destroy(void *data)
{
	struct frame_hold_filter *filter = data;

	obs_enter_graphics();
	gs_texrender_destroy(filter->texrender);
	obs_leave_graphics();

	bfree(filter);
}

/* ------------------------------------------------------------------- */

static void frame_hold_video_tick(void *data, float seconds)
{
	struct frame_hold_filter *filter = data;

	if (!filter->enabled)
		return;

	filter->elapsed += seconds;

	if (filter->elapsed >= filter->interval) {
		/* Subtract rather than reset to zero so the capture
		 * cadence doesn't drift when frame time is uneven. */
		filter->elapsed -= filter->interval;
		if (filter->elapsed < 0.0f || filter->elapsed > filter->interval)
			filter->elapsed = 0.0f;

		filter->need_capture = true;
	}
}

static void frame_hold_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	struct frame_hold_filter *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->source);
	obs_source_t *parent = obs_filter_get_parent(filter->source);

	if (!target || !parent) {
		obs_source_skip_video_filter(filter->source);
		return;
	}

	/* Pass-through mode: filter disabled, just forward live video. */
	if (!filter->enabled) {
		obs_source_skip_video_filter(filter->source);
		return;
	}

	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);

	if (width == 0 || height == 0) {
		obs_source_skip_video_filter(filter->source);
		return;
	}

	bool size_changed = (width != filter->width || height != filter->height);

	if (filter->need_capture || size_changed || !filter->has_texture) {
		gs_texrender_reset(filter->texrender);

		if (gs_texrender_begin(filter->texrender, width, height)) {
			struct vec4 clear_color;
			vec4_zero(&clear_color);

			gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
			gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

			if (obs_source_process_filter_begin(filter->source, GS_RGBA,
							     OBS_ALLOW_DIRECT_RENDERING)) {
				obs_source_process_filter_end(filter->source,
							       obs_get_base_effect(OBS_EFFECT_DEFAULT),
							       width, height);
			}

			gs_texrender_end(filter->texrender);

			filter->width = width;
			filter->height = height;
			filter->has_texture = true;
			filter->need_capture = false;
		}
	}

	if (!filter->has_texture) {
		obs_source_skip_video_filter(filter->source);
		return;
	}

	gs_texture_t *tex = gs_texrender_get_texture(filter->texrender);
	if (!tex) {
		obs_source_skip_video_filter(filter->source);
		return;
	}

	gs_effect_t *default_effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(default_effect, "image");
	gs_effect_set_texture(image, tex);

	while (gs_effect_loop(default_effect, "Draw")) {
		gs_draw_sprite(tex, 0, filter->width, filter->height);
	}
}

static uint32_t frame_hold_get_width(void *data)
{
	struct frame_hold_filter *filter = data;
	if (filter->width)
		return filter->width;

	obs_source_t *target = obs_filter_get_target(filter->source);
	return target ? obs_source_get_base_width(target) : 0;
}

static uint32_t frame_hold_get_height(void *data)
{
	struct frame_hold_filter *filter = data;
	if (filter->height)
		return filter->height;

	obs_source_t *target = obs_filter_get_target(filter->source);
	return target ? obs_source_get_base_height(target) : 0;
}

/* ------------------------------------------------------------------- */

struct obs_source_info frame_hold_filter_info = {
	.id = "frame_hold_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = frame_hold_get_name,
	.create = frame_hold_create,
	.destroy = frame_hold_destroy,
	.update = frame_hold_update,
	.get_defaults = frame_hold_get_defaults,
	.get_properties = frame_hold_get_properties,
	.video_tick = frame_hold_video_tick,
	.video_render = frame_hold_video_render,
	.get_width = frame_hold_get_width,
	.get_height = frame_hold_get_height,
};
