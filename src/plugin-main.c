#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("frame-hold-filter", "en-US")

extern struct obs_source_info frame_hold_filter_info;

bool obs_module_load(void)
{
	obs_register_source(&frame_hold_filter_info);
	blog(LOG_INFO, "[Frame Hold] plugin loaded");
	return true;
}

void obs_module_unload(void)
{
	blog(LOG_INFO, "[Frame Hold] plugin unloaded");
}
