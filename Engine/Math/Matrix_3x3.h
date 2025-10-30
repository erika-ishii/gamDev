/*********************************************************************************************
 \file       Matrix_3x3.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration and some inline immeplementations of the Matrix_3x3 class, 
            which provides 3x3 matrix operations for 2D transformations. 
            Includes constructors, operator overloads, and static 
            utility functions for identity, translation, scaling, rotation, 
            and transposition.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
**********************************************************************************************/
#include "Vector_2D.h"
#ifndef MATRIX_3X3_H
#define MATRIX_3X3_H
/*****************************************************************************************
  \class Matrix_3x3
  \brief Represents a 3x3 matrix and provides basic linear algebra operations.

  The Matrix_3x3 class supports initialization, copying, assignment, multiplication,
  and utility functions for creating transformation matrices commonly used in 2D graphics
  (translation, scaling, rotation) as well as transposition.
*****************************************************************************************/
class Matrix_3x3 {
private:
    float m2[3][3]; /// Internal representation of the matrix
public:
    Matrix_3x3();
    Matrix_3x3(const float* pArr);
    Matrix_3x3(float a, float b, float c, float d, float e, float f, float g, float h, float i);
    /*****************************************************************************************
      \brief Copy constructor (defaulted).
      \param rhs Matrix to copy from.
    *****************************************************************************************/
    Matrix_3x3(const Matrix_3x3& rhs) = default;
    /*****************************************************************************************
      \brief Returns a const reference to the internal 3x3 array.
      \return Const reference to the matrix array.
    *****************************************************************************************/
    const float(&getMatrix() const)[3][3]{ return m2; }
    /*****************************************************************************************
      \brief Returns a pointer to the first element of the internal matrix array.
      \return Pointer to the first element of the matrix.
    *****************************************************************************************/
    float* getData() { return &m2[0][0]; }
    Matrix_3x3& operator=(const Matrix_3x3& rhs);
    Matrix_3x3 operator*(const Matrix_3x3& rhs) const;
    Matrix_3x3& operator*=(const Matrix_3x3& rhs);
    static void Mtx33Identity(Matrix_3x3& pResult);
    static void Mtx33Translate(Matrix_3x3& pResult, float x, float y);
    static void Mtx33Scale(Matrix_3x3& pResult, float x, float y);
    static void Mtx33RotRad(Matrix_3x3& pResult, float angle);
    static void Mtx33Transpose(Matrix_3x3& pResult, const Matrix_3x3& pMtx);
};

#endif
