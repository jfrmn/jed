#pragma once
#include "basic.hh"

extern bool needsUpdate;
extern f32 deltaTime;
extern u64 ticksPerMs;

// x=y; from 0 to 1
namespace AnimationLinear {
	bool Advance(f32* inValue, f32 speed = 0.002f);
	bool Ended(f32 inValue);
	f32  Value(f32 inValue);
};

// sin(x + offset) * scale + 0.5f; 10 full cylces	
namespace AnimationCycling {
	bool Advance(f32* inValue);
	bool Ended(f32 inValue);
	f32  Value(f32 inValue, f32 scale = 0.5f, f32 offset = 0.0f);
};

// sin(x); from 0 to pi
namespace AnimationPulse {
	void Advance(f32* inValue);
	bool Ended(f32 inValue);
	f32  Value(f32 inValue);
};

bool InitPerformanceCounter();
u64  GetPerformanceTimestamp();