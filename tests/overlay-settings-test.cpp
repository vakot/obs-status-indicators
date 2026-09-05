#include <overlay-settings.hpp>

#include <cassert>

int main()
{
	const OverlaySettings defaults = default_overlay_settings();
	assert(defaults.origin == OverlayOrigin::TopLeft);
	assert(defaults.orientation == OverlayOrientation::Vertical);
	assert(defaults.offset == 4);
	assert(defaults.gap == 4);
	assert(defaults.indicator_size == 48);
	assert(defaults.background_color == 0xFF000000);
	assert(defaults.icon_color == 0xFFFFFFFF);
	assert(defaults.opacity == 65);

	OverlaySettings out_of_range = defaults;
	out_of_range.offset = 99999;
	out_of_range.gap = -10;
	out_of_range.indicator_size = 99999;
	out_of_range.opacity = 101;
	out_of_range.background_color = 0x80010203;
	out_of_range.icon_color = 0x00040506;
	const OverlaySettings normalized = normalize_overlay_settings(out_of_range);
	assert(normalized.offset == 4096);
	assert(normalized.gap == 0);
	assert(normalized.indicator_size == 256);
	assert(normalized.opacity == 100);
	assert(normalized.background_color == 0xFF010203);
	assert(normalized.icon_color == 0xFF040506);

	out_of_range.opacity = 0;
	assert(normalize_overlay_settings(out_of_range).opacity == 1);
	out_of_range.indicator_size = 0;
	assert(normalize_overlay_settings(out_of_range).indicator_size == 16);
	return 0;
}
