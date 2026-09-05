#include "overlay-settings.hpp"

#include <obs-data.h>
#include <obs-module.h>
#include <util/platform.h>

#include <cstdint>
#include <cstring>
#include <limits>

namespace {
constexpr char kOrigin[] = "origin";
constexpr char kOrientation[] = "orientation";
constexpr char kOffset[] = "offset";
constexpr char kGap[] = "gap";
constexpr char kBackgroundColor[] = "background_color";
constexpr char kIconColor[] = "icon_color";

constexpr char kTopLeft[] = "top-left";
constexpr char kTopRight[] = "top-right";
constexpr char kBottomLeft[] = "bottom-left";
constexpr char kBottomRight[] = "bottom-right";
constexpr char kVertical[] = "vertical";
constexpr char kHorizontal[] = "horizontal";

const char *origin_name(OverlayOrigin origin)
{
	switch (origin) {
	case OverlayOrigin::TopLeft:
		return kTopLeft;
	case OverlayOrigin::TopRight:
		return kTopRight;
	case OverlayOrigin::BottomLeft:
		return kBottomLeft;
	case OverlayOrigin::BottomRight:
		return kBottomRight;
	}
	return kTopLeft;
}

OverlayOrigin origin_from_name(const char *name)
{
	if (!name)
		return OverlayOrigin::TopLeft;
	if (strcmp(name, kTopRight) == 0)
		return OverlayOrigin::TopRight;
	if (strcmp(name, kBottomLeft) == 0)
		return OverlayOrigin::BottomLeft;
	if (strcmp(name, kBottomRight) == 0)
		return OverlayOrigin::BottomRight;
	return OverlayOrigin::TopLeft;
}

const char *orientation_name(OverlayOrientation orientation)
{
	return orientation == OverlayOrientation::Horizontal ? kHorizontal : kVertical;
}

OverlayOrientation orientation_from_name(const char *name)
{
	if (name && strcmp(name, kHorizontal) == 0)
		return OverlayOrientation::Horizontal;
	return OverlayOrientation::Vertical;
}

void read_color(obs_data_t *data, const char *name, std::uint32_t fallback, std::uint32_t &value)
{
	if (!obs_data_has_user_value(data, name)) {
		value = fallback;
		return;
	}

	const long long stored = obs_data_get_int(data, name);
	if (stored < 0 || static_cast<unsigned long long>(stored) > std::numeric_limits<std::uint32_t>::max()) {
		value = fallback;
		return;
	}

	value = static_cast<std::uint32_t>(stored);
}
}

OverlaySettings load_overlay_settings()
{
	OverlaySettings settings = default_overlay_settings();
	char *path = obs_module_config_path("settings.json");
	if (!path)
		return settings;

	obs_data_t *data = obs_data_create_from_json_file_safe(path, ".bak");
	bfree(path);
	if (!data)
		return settings;

	settings.origin = origin_from_name(obs_data_get_string(data, kOrigin));
	settings.orientation = orientation_from_name(obs_data_get_string(data, kOrientation));
	settings.offset = static_cast<int>(obs_data_get_int(data, kOffset));
	settings.gap = static_cast<int>(obs_data_get_int(data, kGap));
	read_color(data, kBackgroundColor, settings.background_color, settings.background_color);
	read_color(data, kIconColor, settings.icon_color, settings.icon_color);
	obs_data_release(data);
	return normalize_overlay_settings(settings);
}

bool save_overlay_settings(const OverlaySettings &input)
{
	const OverlaySettings settings = normalize_overlay_settings(input);
	char *directory = obs_module_config_path("");
	if (!directory)
		return false;
	const int directory_result = os_mkdirs(directory);
	bfree(directory);
	if (directory_result != 0 && directory_result != MKDIR_SUCCESS)
		return false;

	char *path = obs_module_config_path("settings.json");
	if (!path)
		return false;
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, kOrigin, origin_name(settings.origin));
	obs_data_set_string(data, kOrientation, orientation_name(settings.orientation));
	obs_data_set_int(data, kOffset, settings.offset);
	obs_data_set_int(data, kGap, settings.gap);
	obs_data_set_int(data, kBackgroundColor, settings.background_color);
	obs_data_set_int(data, kIconColor, settings.icon_color);
	const bool saved = obs_data_save_json_safe(data, path, ".tmp", ".bak");
	obs_data_release(data);
	bfree(path);
	return saved;
}
