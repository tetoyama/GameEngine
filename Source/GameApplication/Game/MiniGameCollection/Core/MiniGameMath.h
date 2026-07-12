#pragma once

#include <algorithm>
#include <cmath>

namespace MiniGameCollection {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2 operator+(const Vec2& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y};
    }

    constexpr Vec2 operator-(const Vec2& rhs) const noexcept {
        return {x - rhs.x, y - rhs.y};
    }

    constexpr Vec2 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar};
    }

    constexpr Vec2 operator/(float scalar) const noexcept {
        return scalar != 0.0f ? Vec2{x / scalar, y / scalar} : Vec2{};
    }

    Vec2& operator+=(const Vec2& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
};

inline constexpr float Dot(const Vec2& lhs, const Vec2& rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline constexpr float LengthSquared(const Vec2& value) noexcept {
    return Dot(value, value);
}

inline float Length(const Vec2& value) noexcept {
    return std::sqrt(LengthSquared(value));
}

inline Vec2 NormalizeOrZero(const Vec2& value) noexcept {
    const float length = Length(value);
    return length > 0.00001f ? value / length : Vec2{};
}

inline Vec2 ClampLength(const Vec2& value, float maximumLength) noexcept {
    const float safeMaximum = std::max(0.0f, maximumLength);
    const float lengthSquared = LengthSquared(value);
    if (lengthSquared <= safeMaximum * safeMaximum) {
        return value;
    }
    return NormalizeOrZero(value) * safeMaximum;
}

inline Vec2 Lerp(const Vec2& from, const Vec2& to, float amount) noexcept {
    const float t = std::clamp(amount, 0.0f, 1.0f);
    return from + (to - from) * t;
}

inline float DistanceSquared(const Vec2& lhs, const Vec2& rhs) noexcept {
    return LengthSquared(lhs - rhs);
}

inline float Distance(const Vec2& lhs, const Vec2& rhs) noexcept {
    return std::sqrt(DistanceSquared(lhs, rhs));
}

} // namespace MiniGameCollection
