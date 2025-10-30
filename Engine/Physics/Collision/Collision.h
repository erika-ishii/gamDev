/*********************************************************************************************
 \file      Collision.h
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%

 \brief     Implementation of Collision. Provides AABB style collision checks for 
            Rectangle to Circle and Rectangle to Rectangle, along with having structs
            for both Rectangle and Circle

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#ifndef COLLISION_H
#define COLLISION_H
#include "Math/Vector_2D.h"

namespace Framework 
{

    /*****************************************************************************************
      \brief The AABB struct for any game object that will use it.
    *****************************************************************************************/
    struct AABB
    {
        Vector2D<float> min;
        Vector2D<float> max;

        AABB(float x, float y, float width, float height) : 
            min(x - width / 2, y - height / 2), max(x + width / 2, y + height / 2) {}
    };

    /*****************************************************************************************
      \brief The Circle struct. Used exclusively for projectiles for now.
    *****************************************************************************************/
    struct Circle
    {
        Vector2D<float> center;
        float radius;

        Circle(float x, float y, float rad) : center(x, y), radius(rad) {}
    };

    /*****************************************************************************************
      \brief The Collision class that will be used to check collision.
    *****************************************************************************************/
    class Collision
    {
    public:
        // Rect-Rect collision
        static bool CheckCollisionRectToRect(const AABB& a, const AABB& b)
        {
            return (a.min.getX() < b.max.getX() && a.max.getX() > b.min.getX() &&
                    a.min.getY() < b.max.getY() && a.max.getY() > b.min.getY());
        }

        // Circle-Rect collision
        static bool CheckCollisionRectToCircle(const Circle& c, const AABB& r)
        {
            float closestX = std::max(r.min.getX(), std::min(c.center.getX(), r.max.getX()));
            float closestY = std::max(r.min.getY(), std::min(c.center.getY(), r.max.getY()));

            float dx = c.center.getX() - closestX;
            float dy = c.center.getY() - closestY;

            return (dx * dx + dy * dy) < (c.radius * c.radius);
        }
    };

} // namespace Framework
#endif //COLLISION_H
