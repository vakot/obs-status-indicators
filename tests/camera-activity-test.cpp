#include "camera-activity.hpp"

#include <cassert>

int main()
{
	CameraActivity activity;
	assert(!activity.active());
	assert(!activity.tick(0.99f));
	assert(!activity.active());
	assert(!activity.tick(0.01f));
	assert(!activity.active());

	activity.observe();
	assert(activity.tick(0.01f));
	assert(activity.active());

	assert(!activity.tick(0.99f));
	assert(activity.tick(0.01f));
	assert(!activity.active());

	activity.observe();
	activity.observe();
	assert(activity.tick(-1.0f));
	assert(activity.active());

	return 0;
}
