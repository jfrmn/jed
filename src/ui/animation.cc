#include "animation.hh"

#include <cmath>

bool needsUpdate = false;
f32 deltaTime = 0.0f;

static bool Advance(f32* inValue, f32 speed, f32 max) {
	*inValue += speed * deltaTime;
	if (*inValue > max) {
		*inValue = max;
		return true;
	} else {
		needsUpdate = true;
		return false;
	}
}

bool AnimationLinear::Advance(f32* inValue, f32 speed /* = 0.002f*/) {
	return ::Advance(inValue, speed, 1.0f);
}
bool AnimationLinear::Ended(f32 inValue) {
	return inValue == 1.0f;
}
f32 AnimationLinear::Value(f32 inValue) {
	return inValue;	
}

static constexpr f32 CYLCING_MAX = ((F32_PI * 2.0f) * 10.0f);

bool AnimationCycling::Advance(f32* inValue) {
	return ::Advance(inValue, 0.004f, CYLCING_MAX);
}
bool AnimationCycling::Ended(f32 inValue) {
	return inValue == CYLCING_MAX;
}
f32 AnimationCycling::Value(f32 inValue, f32 scale /*= 0.5f*/, f32 offset /*= 0.0f*/) {
	return std::sin(inValue + offset) * scale + 0.5f;
}

void AnimationPulse::Advance(f32* inValue) {
	::Advance(inValue, 0.006f, F32_PI);
}
bool AnimationPulse::Ended(f32 inValue) {
	return inValue == F32_PI;
}
f32 AnimationPulse::Value(f32 inValue) {
	return std::sin(inValue);
}
