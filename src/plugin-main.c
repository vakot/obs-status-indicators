#include <obs-frontend-api.h>
#include <obs-module.h>

#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void frontend_event(enum obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);

	obs_log(LOG_INFO, "frontend event received: %d", (int)event);
}

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(frontend_event, NULL);
	obs_log(LOG_INFO, "plugin loaded; frontend callback registered");
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, NULL);
	obs_log(LOG_INFO, "plugin unloaded; frontend callback removed");
}
