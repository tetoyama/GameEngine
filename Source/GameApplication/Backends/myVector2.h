#pragma once
#include <math.h>
#include <DirectXMath.h>
struct Vector2 {


	union {
		float v[2];
		struct {
			float x, y;
		};
	};

	Vector2() = default;
	Vector2(float _x, float _y): x(_x), y(_y){}
};
