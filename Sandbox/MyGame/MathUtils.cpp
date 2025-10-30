/*********************************************************************************************
 \file      MathUtils.cpp
 \par       SofaSpuds
 \author    yimo kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Implementation for tiny 4x4 matrices, GLSL compile/link, and QuadGL.
 \details   Implements identity/ortho/translate/scale/rotateZ, matrix multiply (column-major),
            shader compile/link utilities with error logging, and a unit-quad helper that
            uploads positions-only geometry for basic 2D rendering paths.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "MathUtils.hpp"

#include <cmath>
#include <iostream>
#include <string>

// =============================== Mat4 constructors ========================================

Mat4 Identity() {
    Mat4 r{};
    r.m[0] = 1.0f; r.m[5] = 1.0f; r.m[10] = 1.0f; r.m[15] = 1.0f;
    return r;
}

Mat4 Ortho(float l, float r, float b, float t, float zn, float zf) {
    Mat4 M{};
    // OpenGL-style ortho projecting to NDC [-1,1]
    M.m[0] = 2.0f / (r - l);
    M.m[5] = 2.0f / (t - b);
    M.m[10] = -2.0f / (zf - zn);
    M.m[12] = -(r + l) / (r - l);
    M.m[13] = -(t + b) / (t - b);
    M.m[14] = -(zf + zn) / (zf - zn);
    M.m[15] = 1.0f;
    return M;
}

Mat4 Mul(const Mat4& A, const Mat4& B) {
    Mat4 R{};
    // Column-major multiply: R = A * B
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            R.m[c * 4 + r] =
                A.m[0 * 4 + r] * B.m[c * 4 + 0]
                + A.m[1 * 4 + r] * B.m[c * 4 + 1]
                + A.m[2 * 4 + r] * B.m[c * 4 + 2]
                + A.m[3 * 4 + r] * B.m[c * 4 + 3];
        }
    }
    return R;
}

Mat4 Translate(float x, float y) {
    Mat4 T = Identity();
    T.m[12] = x;
    T.m[13] = y;
    return T;
}

Mat4 Scale(float sx, float sy) {
    Mat4 S{};
    S.m[0] = sx;
    S.m[5] = sy;
    S.m[10] = 1.0f;
    S.m[15] = 1.0f;
    return S;
}

Mat4 RotateZ(float rad) {
    Mat4 R = Identity();
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    // Standard 2D rotation in the XY plane (right-handed)
    R.m[0] = c;  R.m[4] = -s;
    R.m[1] = s;  R.m[5] = c;
    return R;
}

float DegToRad(float degrees) {
    return degrees * 3.14159265358979323846f / 180.0f;
}

// =============================== OpenGL helpers ===========================================

GLuint Compile(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(sh, len, nullptr, log.data());
        std::cerr << "[Shader compile error]\n" << log << std::endl;
    }
    return sh;
}

GLuint Link(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::cerr << "[Program link error]\n" << log << std::endl;
    }

    // shaders are no longer needed after linking
    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

// =============================== QuadGL ===================================================

void QuadGL::create() {
    // Unit quad centered at origin (pivot at center)
    const float verts[8] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };
    const unsigned short idx[6] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);

    glBindVertexArray(0);
}

void QuadGL::destroy() {
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
    vao = vbo = ebo = 0;
}
