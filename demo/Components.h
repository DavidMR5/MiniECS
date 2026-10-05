#pragma once

#include <cmath>

namespace demo
{
    struct Vec2
    {
        float x = 0.0f;
        float y = 0.0f;

        constexpr Vec2 operator+(Vec2 o) const { return { x + o.x, y + o.y }; }
        constexpr Vec2 operator-(Vec2 o) const { return { x - o.x, y - o.y }; }
        constexpr Vec2 operator*(float s) const { return { x * s, y * s }; }
        constexpr Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
        constexpr Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }

        [[nodiscard]] constexpr float lengthSquared() const { return x * x + y * y; }
        [[nodiscard]] float length() const { return std::sqrt(lengthSquared()); }

        [[nodiscard]] Vec2 withLength(float newLength) const
        {
            const float len = length();
            return len > 1e-6f ? *this * (newLength / len) : Vec2{};
        }

        [[nodiscard]] Vec2 limited(float maxLength) const
        {
            const float sq = lengthSquared();
            return sq > maxLength * maxLength ? withLength(maxLength) : *this;
        }
    };

    struct Position     { Vec2 value; };
    struct Velocity     { Vec2 value; };
    struct Acceleration { Vec2 value; };

    struct Boid
    {
        float maxSpeed = 120.0f;
        float maxForce = 220.0f;
    };

    struct Renderable
    {
        float size = 5.0f;
        float hueShift = 0.0f;
    };
}
