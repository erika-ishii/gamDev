/*********************************************************************************************
 \file      MathUtils.hpp
 \par       SofaSpuds
 \author    yimo kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Minimal 4x4 matrix utilities, GLSL helpers, and a unit-quad mesh for 2D rendering.
 \details   Defines a tiny column-major 4x4 matrix (Mat4) compatible with OpenGL and provides
            constructors for common 2D transforms (identity, orthographic projection, translate,
            scale, rotateZ). Also declares lightweight GLSL compile/link helpers and a simple
            QuadGL mesh wrapper used by the renderer. All matrices follow OpenGL conventions
            (column-major; column vectors). Composition order is right-to-left, i.e. M = T * R * S.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <glad/glad.h>

/**
 * \struct Mat4
 * \brief  Column-major 4x4 matrix compatible with OpenGL.
 * \details Elements are stored column-major. Index as m[c*4 + r] where c∈[0..3], r∈[0..3].
 */
struct Mat4 {
    float m[16]{};
};

// ===== Matrix construction =====

/**
 * \brief  Construct the 4x4 identity matrix.
 * \return Identity matrix I.
 */
Mat4 Identity();

/**
 * \brief  Build an orthographic projection matrix (OpenGL NDC x,y∈[-1,1], z∈[-1,1]).
 * \param  left   World-space left plane.
 * \param  right  World-space right plane.
 * \param  bottom World-space bottom plane.
 * \param  top    World-space top plane.
 * \param  zNear  Near plane (default -1).
 * \param  zFar   Far plane  (default  1).
 * \return Ortho projection matrix.
 */
Mat4 Ortho(float left, float right, float bottom, float top,
    float zNear = -1.0f, float zFar = 1.0f);

/**
 * \brief  Matrix multiply R = A * B (column-major).
 * \param  A Left operand.
 * \param  B Right operand.
 * \return Resulting matrix.
 * \note   With column vectors v, R*v = A*(B*v). Composition is right-to-left.
 */
Mat4 Mul(const Mat4& A, const Mat4& B);

/**
 * \brief  2D translation (z = 0).
 * \param  x Translation in x.
 * \param  y Translation in y.
 * \return Translation matrix T.
 */
Mat4 Translate(float x, float y);

/**
 * \brief  2D scaling (z scale = 1).
 * \param  sx Scale along x.
 * \param  sy Scale along y.
 * \return Scaling matrix S.
 */
Mat4 Scale(float sx, float sy);

/**
 * \brief  Rotation about the Z axis (right-handed; CCW for +radians).
 * \param  radians Angle in radians.
 * \return Rotation matrix Rz.
 */
Mat4 RotateZ(float radians);

/**
 * \brief  Degrees to radians helper.
 * \param  degrees Angle in degrees.
 * \return Angle in radians.
 */
float DegToRad(float degrees);

// ===== OpenGL helpers =====

/**
 * \brief  Compile a GLSL shader.
 * \param  type GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
 * \param  src  Null-terminated GLSL source.
 * \return Shader object handle (non-zero). Logs errors to stderr on failure.
 */
GLuint Compile(GLenum type, const char* src);

/**
 * \brief  Link a shader program from a vertex and fragment shader.
 * \param  vs Compiled vertex shader handle.
 * \param  fs Compiled fragment shader handle.
 * \return Linked program handle. Deletes shaders after linking; logs errors on failure.
 */
GLuint Link(GLuint vs, GLuint fs);

// ===== Simple Quad Mesh =====

/**
 * \struct QuadGL
 * \brief  Unit quad centered at the origin (pivot at center), positions-only (vec2) in attrib 0.
 * \details Index order: (0,1,2, 2,3,0). Useful for sprites/rect passes.
 */
struct QuadGL {
    GLuint vao = 0, vbo = 0, ebo = 0;

    /**
     * \brief Create VAO/VBO/EBO and upload quad vertex/index data.
     * \note  Safe to call once per instance; call destroy() before re-creating.
     */
    void create();

    /**
     * \brief Destroy GL buffers and VAO if allocated.
     */
    void destroy();
};
