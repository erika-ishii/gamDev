/*********************************************************************************************
 \file      Matrix_3x3.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the Matrix_3x3 class and its major functions, 
            which provides 3x3 matrix operations for 2D transformations. 
            Includes constructors, operator overloads, and static utility 
            functions for identity, translation, scaling, rotation, 
            and transposition.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
**********************************************************************************************/

#include "Matrix_3x3.h"
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
/*****************************************************************************************
     \brief Default constructor. Initializes the matrix to the identity matrix.
*****************************************************************************************/
Matrix_3x3::Matrix_3x3() {
    Mtx33Identity(*this);
}
/*****************************************************************************************
     \brief Constructor from a flat array of 9 floats.
    \param pArr Pointer to an array of 9 floats representing the matrix elements.
*****************************************************************************************/
Matrix_3x3::Matrix_3x3(const float* pArr) {
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            m2[row][col] = pArr[col * 3 + row]; // Column-major order
        }
    }
}
/*****************************************************************************************
     \brief Parameterized constructor.
    \param a,b,c,d,e,f,g,h,i Elements of the matrix in row-major order.
*****************************************************************************************/
Matrix_3x3::Matrix_3x3(float a, float b, float c, float d, float e, float f, float g, float h, float i) {
    m2[0][0] = a; m2[1][0] = b; m2[2][0] = c;
    m2[0][1] = d; m2[1][1] = e; m2[2][1] = f;
    m2[0][2] = g; m2[1][2] = h; m2[2][2] = i;
}
/*****************************************************************************************
     \brief Assignment operator.
    \param rhs Matrix to assign from.
    \return Reference to this matrix.
*****************************************************************************************/
Matrix_3x3& Matrix_3x3::operator=(const Matrix_3x3& rhs) {
    if (this != &rhs) {
        for (int col = 0; col < 3; ++col) {
            for (int row = 0; row < 3; ++row) {
                m2[row][col] = rhs.m2[row][col];
            }
        }
    }
    return *this;
}
/*****************************************************************************************
     \brief Multiplies this matrix with another matrix.
    \param rhs Matrix to multiply with.
    \return Resulting matrix after multiplication.
*****************************************************************************************/
Matrix_3x3 Matrix_3x3::operator*(const Matrix_3x3& rhs) const {
    Matrix_3x3 result;
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            result.m2[row][col] =
                m2[row][0] * rhs.m2[0][col] +
                m2[row][1] * rhs.m2[1][col] +
                m2[row][2] * rhs.m2[2][col];
        }
    }
    return result;
}
/*****************************************************************************************
     \brief Multiplies this matrix with another matrix in-place.
    \param rhs Matrix to multiply with.
    \return Reference to this matrix after multiplication.
*****************************************************************************************/
Matrix_3x3& Matrix_3x3::operator*=(const Matrix_3x3& rhs) {
    *this = *this * rhs;
    return *this;
}
/*****************************************************************************************
     \brief Sets a matrix to the identity matrix.
    \param pResult Matrix to store the result.
*****************************************************************************************/
void Matrix_3x3::Mtx33Identity(Matrix_3x3& pResult) {
    pResult = Matrix_3x3(
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    );
}
/*****************************************************************************************
     \brief Sets a matrix to a translation matrix.
    \param pResult Matrix to store the result.
    \param x Translation along the X-axis.
    \param y Translation along the Y-axis.
*****************************************************************************************/
void Matrix_3x3::Mtx33Translate(Matrix_3x3& pResult, float x, float y) {
    pResult = Matrix_3x3(
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        x, y, 1.0f
    );
}
/*****************************************************************************************
     \brief Sets a matrix to a scale matrix.
    \param pResult Matrix to store the result.
    \param x Scale factor along the X-axis.
    \param y Scale factor along the Y-axis.
*****************************************************************************************/
void Matrix_3x3::Mtx33Scale(Matrix_3x3& pResult, float x, float y) {
    pResult = Matrix_3x3(
        x, 0.0f, 0.0f,
        0.0f, y, 0.0f,
        0.0f, 0.0f, 1.0f
    );
}
/*****************************************************************************************
     \brief Sets a matrix to a rotation matrix using radians.
    \param pResult Matrix to store the result.
    \param angle Rotation angle in radians.
*****************************************************************************************/
void Matrix_3x3::Mtx33RotRad(Matrix_3x3& pResult, float angle) {
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);
    pResult = Matrix_3x3(
        cosA, -sinA, 0.0f,
        sinA, cosA, 0.0f,
        0.0f, 0.0f, 1.0f
    );
}
/*****************************************************************************************
     \brief Computes the transpose of a matrix.
    \param pResult Matrix to store the result.
    \param pMtx Matrix to transpose.
*****************************************************************************************/
void Matrix_3x3::Mtx33Transpose(Matrix_3x3& pResult, const Matrix_3x3& pMtx) {
    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 3; ++row) {
            pResult.m2[row][col] = pMtx.m2[col][row];
        }
    }
}
