#pragma once
#include <math.h>
#include <DirectXMath.h>
struct Vector2 {
	float x = 0.0f;
	float y = 0.0f;
	Vector2() = default;
	Vector2(float _x, float _y): x(_x), y(_y){}
};