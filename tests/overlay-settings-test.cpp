#include <overlay-settings.hpp>

#include <cassert>

int main()
{
	const OverlaySettings defaults = default_overlay_settings();
	assert(defaults.origin == OverlayOrigin::TopLeft);
	assert(defaults.orientation == OverlayOrientation::Vertical);
	assert(defaults.offset == 0);
	assert(defaults.gap == 8);
	assert(defaults.background_color == 0x80000000);
	assert(defaults.icon_color == 0x80FFFFFF);

	OverlaySettings out_of_range = defaults;
	out_of_range.offset = 99999;
	out_of_range.gap = -10;
	const OverlaySettings normalized = normalize_overlay_settings(out_of_range);
	assert(normalized.offset == 4096);
	assert(normalized.gap == 0);
	return 0;
}
