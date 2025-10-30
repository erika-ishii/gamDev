/*
#pragma once
#ifndef COLLIDE_ELE_H
#define COLLIDE_ELE_H
#include "Math/Vector_2D.h"
namespace Framework {

    // Structure representing a point in 2D space
    class Point {
    private:
        Vector2D<float> Position;  // Position of the point (assuming Vector2D is template-based, using float)

    public:
        Point(const Vector2D<float>& pos): Position(pos) {}
        const Vector2D<float>& GetPosition() const { return Position;}
        void SetPosition(const Vector2D<float>& Pos) { Position = Pos; }

    };

    // Structure representing an Axis-Aligned Bounding Box (AABB)
    class AABB {
    private:
        Vector2D<float> Min;    // Minimum corner of the AABB
        Vector2D<float> Max;    // Maximum corner of the AABB
        Vector2D <float> Position;

    public:
        AABB() : Min(0.0f, 0.0f), Max(0.0f, 0.0f){}
        
        void SetMin(const Vector2D<float>& minCorner) { Min = minCorner; }
        void SetMax(const Vector2D<float>& maxCorner) { Max = maxCorner; }
        void SetPos(const Vector2D<float>& pos) { Position = pos; }
        const Vector2D<float>& getMin() const { return Min; }
        const Vector2D<float>& getMax() const { return Max; }
        const Vector2D<float>& getPosition() const { return Position; }

    };

    // Inline functions to get X and Y coordinates from Vector2D
    inline float myPointX(const Vector2D<float>& vec) {
        return vec.getX(); // Assuming Vector2D has a method getX()
    }

    inline float myPointY(const Vector2D<float>& vec) {
        return vec.getY(); // Assuming Vector2D has a method getY()
    }

    // Inline functions for maximum and minimum calculations
    inline float myMax(float a, float b) {
        return (a > b) ? a : b;
    }

    inline float myMin(float a, float b) {
        return (a < b) ? a : b;
    }


}
#endif
*/