/*********************************************************************************************
 \file      Vector2D.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration and inline implementation of a templated 2D vector class supporting arithmetic operations, 
            vector math functions, and utility methods for game development or physics 
            calculations. Supports generic numeric types via template parameter T (default float).

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
**********************************************************************************************/
#pragma once
#include <cmath>
/*********************************************************************************************
 \class Vector2D
 \brief Represents a 2D vector with arithmetic and vector operations.

 The Vector2D class supports standard arithmetic operators, normalization, length, distance, 
 dot product, cross product magnitude, and other utility functions similar to Unity's vector 
 math. It is templated to work with any numeric type (float, double, int, etc.).
*********************************************************************************************/
template <typename T = float>
class Vector2D
{
    private:
        T x, y; ///Components of the vector
    public:
    /*****************************************************************************************
      \brief Default constructor. Initializes the vector to (0, 0).
    *****************************************************************************************/
    Vector2D() : x(0), y(0) {}
    /*****************************************************************************************
      \brief Parameterized constructor.
      \param _x  X component of the vector.
      \param _y  Y component of the vector.
    *****************************************************************************************/
    Vector2D(T _x, T _y) : x(_x), y(_y) {}
    /*****************************************************************************************
      \brief Assignment operator.
      \param rhs  Vector to assign from.
      \return Reference to this vector.
    *****************************************************************************************/
    Vector2D& operator=(const Vector2D& rhs)
    {
        if (this != &rhs) {
            x = rhs.x;
            y = rhs.y;
        }
        return *this;
    }
     /*****************************************************************************************
      \brief Adds another vector to this vector.
      \param rhs  Vector to add.
      \return Reference to this vector.
    *****************************************************************************************/
    Vector2D& operator += (const Vector2D& rhs) { x += rhs.x; y += rhs.y; return *this; }
    /*****************************************************************************************
      \brief Subtracts another vector from this vector.
      \param rhs  Vector to subtract.
      \return Reference to this vector.
    *****************************************************************************************/
    Vector2D& operator -= (const Vector2D& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    /*****************************************************************************************
      \brief Subtract operator.
      \param rhs  Vector to subtract.
      \return Resulting vector after subtraction.
    *****************************************************************************************/
    Vector2D operator-(const Vector2D& rhs) const { return Vector2D(x - rhs.x, y - rhs.y); }
    /*****************************************************************************************
      \brief Multiply vector by a scalar.
      \param rhs  Scalar value.
      \return Reference to this vector.
    *****************************************************************************************/
    Vector2D& operator *= (T rhs) { x *= rhs; y *= rhs; return *this; }
    /*****************************************************************************************
      \brief Divide vector by a scalar.
      \param rhs  Scalar value.
      \return Reference to this vector.
    *****************************************************************************************/
    Vector2D& operator /= (T rhs) { x /= rhs; y /= rhs; return *this; }
    /*****************************************************************************************
      \brief Unary negation operator.
      \return Negated vector.
    *****************************************************************************************/
    Vector2D operator -() const { return Vector2D(-x, -y); }
    /*****************************************************************************************
      \brief Getter for X component.
      \return X component.
    *****************************************************************************************/
    T getX() const { return x; }
    /*****************************************************************************************
      \brief Getter for Y component.
      \return Y component.
    *****************************************************************************************/
    T getY() const { return y; }
    /*****************************************************************************************
      \brief Setter for X component.
      \param _x  New X value.
    *****************************************************************************************/
    void setX(T _x) { x = _x; }
    /*****************************************************************************************
      \brief Setter for Y component.
      \param _y  New Y value.
    *****************************************************************************************/
    void setY(T _y) { y = _y; }
    /*****************************************************************************************
      \brief Adds two vectors.
      \param lhs  Left-hand side vector.
      \param rhs  Right-hand side vector.
      \return Resulting vector.
    *****************************************************************************************/
    static Vector2D Add(const Vector2D& lhs, const Vector2D& rhs) { return Vector2D(lhs.x + rhs.x, lhs.y + rhs.y); }
    /*****************************************************************************************
      \brief Subtracts two vectors.
      \param lhs  Left-hand side vector.
      \param rhs  Right-hand side vector.
      \return Resulting vector.
    *****************************************************************************************/
    static Vector2D Subtract(const Vector2D& lhs, const Vector2D& rhs) { return Vector2D(lhs.x - rhs.x, lhs.y - rhs.y); }
    /*****************************************************************************************
      \brief Multiplies a vector by a scalar.
      \param lhs  Vector to multiply.
      \param rhs  Scalar value.
      \return Resulting vector.
    *****************************************************************************************/
    static Vector2D Multiply(const Vector2D& lhs, T rhs) { return Vector2D(lhs.x * rhs, lhs.y * rhs); }
    /*****************************************************************************************
      \brief Divides a vector by a scalar.
      \param lhs  Vector to divide.
      \param rhs  Scalar value.
      \return Resulting vector.
    *****************************************************************************************/
    static Vector2D Divide(const Vector2D& lhs, T rhs) { return Vector2D(lhs.x / rhs, lhs.y / rhs); }
    /*****************************************************************************************
      \brief Normalizes a vector.
      \param pVec  Vector to normalize.
      \return Normalized vector.
    *****************************************************************************************/
    static Vector2D Normalize(const Vector2D& pVec)
    {
        T len = Length(pVec);
        return (len != 0) ? Vector2D(pVec.x / len, pVec.y / len) : Vector2D(0, 0);
    }
    /*****************************************************************************************
      \brief Computes the length (magnitude) of a vector.
      \param pVec  Vector.
      \return Length of the vector.
    *****************************************************************************************/
    static T Length(const Vector2D& pVec) { return std::sqrt(pVec.x * pVec.x + pVec.y * pVec.y); }
    /*****************************************************************************************
      \brief Computes the squared length of a vector.
      \param pVec  Vector.
      \return Squared length of the vector.
    *****************************************************************************************/
    static T SquareLength(const Vector2D& pVec) { return pVec.x * pVec.x + pVec.y * pVec.y; }
    /*****************************************************************************************
      \brief Computes the distance between two vectors.
      \param pVec0  First vector.
      \param pVec1  Second vector.
      \return Distance between vectors.
    *****************************************************************************************/
    static T Distance(const Vector2D& pVec0, const Vector2D& pVec1) { return length(subtract(pVec0, pVec1)); }
    /*****************************************************************************************
      \brief Computes squared distance between two vectors.
      \param pVec0  First vector.
      \param pVec1  Second vector.
      \return Squared distance.
    *****************************************************************************************/
    static T SquareDistance(const Vector2D& pVec0, const Vector2D& pVec1) { return squareLength(subtract(pVec0, pVec1)); }
     /*****************************************************************************************
      \brief Computes dot product between two vectors.
      \param pVec0  First vector.
      \param pVec1  Second vector.
      \return Dot product.
    *****************************************************************************************/
    static T DotProduct(const Vector2D& pVec0, const Vector2D& pVec1) { return pVec0.x * pVec1.x + pVec0.y * pVec1.y; }
    /*****************************************************************************************
      \brief Scales a vector by a scalar.
      \param scalar  Scalar value.
      \param vec  Vector to scale.
      \return Scaled vector.
    *****************************************************************************************/
    static Vector2D Scale(T scalar, const Vector2D& vec) { return Vector2D(vec.x * scalar, vec.y * scalar); }
    /*****************************************************************************************
      \brief Computes the magnitude of the cross product between two vectors.
      \param pVec0  First vector.
      \param pVec1  Second vector.
      \return Magnitude of cross product.
    *****************************************************************************************/
    static T CrossProductMag(const Vector2D& pVec0, const Vector2D& pVec1) { return pVec0.x * pVec1.y - pVec0.y * pVec1.x; }
    /*****************************************************************************************
      \brief Moves a vector towards a target vector by a maximum distance.
      \param current  Current vector.
      \param target  Target vector.
      \param maxDistanceDelta  Maximum distance to move.
      \return New vector after moving towards target.
    *****************************************************************************************/
    static Vector2D MoveTowards(const Vector2D& current, const Vector2D& target, T maxDistanceDelta)
    {
        Vector2D direction = subtract(target, current);
        T dist = length(direction);
        if (dist <= maxDistanceDelta || dist == 0.0f)
            return target;
        return add(current, scale(maxDistanceDelta / dist, direction));
    }
    /*****************************************************************************************
      \brief Computes the clockwise angle from one vector to another.
      \param pVec0  First vector.
      \param pVec1  Second vector.
      \return Clockwise angle in radians.
    *****************************************************************************************/
    static T AngleClockwise(const Vector2D& pVec0, const Vector2D& pVec1)
    {
        T dot = dotProduct(pVec0, pVec1);
        T det = crossProductMag(pVec0, pVec1);
        return std::atan2(det, dot);
    }
};






