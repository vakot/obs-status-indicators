#include "microphone-activity.hpp"

#include <cassert>

int main()
{
	MicrophoneActivity activity;
	assert(activity.active());
	assert(!activity.tick(9.9f));
	assert(activity.active());
	assert(activity.tick(0.1f));
	assert(!activity.active());
	assert(!activity.tick(1.0f));

	activity.observe(-60.0f);
	assert(!activity.tick(0.01f));
	assert(!activity.active());

	activity.observe(-59.9f);
	assert(activity.tick(0.01f));
	assert(activity.active());

	assert(!activity.tick(-1.0f));
	assert(activity.active());

	return 0;
}
