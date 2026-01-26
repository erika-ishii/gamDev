/*********************************************************************************************
 \file      Graphics.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Main Author, 70%
            yimo kong (yimo.kong@digipen.edu)      - Author, 30%
 \brief     OpenGL-based 2D rendering utilities: geometry setup, shader utils, textures,
            background, shapes, and sprite/sprite-sheet rendering with sub-UV animation.
 \details   This module encapsulates lightweight graphics helpers used by the sandbox/game:
            - Geometry: unit rect, circle (procedural), fullscreen background, sprite quad.
            - Shaders: minimal compile/link/validate with error logging.
            - Textures: stb_image loading with GL setup.
            - Transforms: GLM-based model builds (translate/rotate/scale), pivot-aware rect.
            - Sprites: whole-texture draw and sprite-sheet framed draw via uUVOffset/uUVScale.
            - Diagnostics: GL error guard and crash-test toggles for robustness testing.
            Data is kept in static members (per-process), initialized via initialize() and
            released in cleanup(). Sprite-sheet animation is driven externally by callers
            (e.g., Game.cpp) using renderSpriteFrame(...) with frame/cols/rows.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Graphics.hpp"
#include "Core/PathUtils.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstddef>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdexcept>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace gfx {

    /// \brief PI constant used for circle tessellation.
    constexpr float PI = 3.14159265359f;

    // ===== Static GL objects / program handles =====
    unsigned int Graphics::VAO_rect = 0;
    unsigned int Graphics::VBO_rect = 0;
    unsigned int Graphics::VAO_circle = 0;
    unsigned int Graphics::VBO_circle = 0;
    int          Graphics::circleVertexCount = 0;
    unsigned int Graphics::VAO_bg = 0;
    unsigned int Graphics::VBO_bg = 0;
    unsigned int Graphics::bgTexture = 0;
    unsigned int Graphics::bgShader = 0;
    unsigned int Graphics::objectShader = 0;
    unsigned int Graphics::VAO_sprite = 0;
    unsigned int Graphics::VBO_sprite = 0;
    unsigned int Graphics::EBO_sprite = 0;
    unsigned int Graphics::spriteShader = 0;
    unsigned int Graphics::spriteInstanceVBO = 0;
    unsigned int Graphics::spriteInstanceShader = 0;
    unsigned int Graphics::glowShader = 0;

    /// \brief Circle tessellation segments (triangle fan).
    static int segments = 50;

    // Cache rectangle local-space geometric center (pivot).
    // Computed in initialize() from current rect vertex data.
    static float sRectPivotX = 0.0f;
    static float sRectPivotY = 0.0f;

    /// \brief Cached camera/state matrices for world rendering.
    static glm::mat4 sViewMatrix(1.0f);
    static glm::mat4 sProjectionMatrix(1.0f);
    static glm::mat4 sViewProjectionMatrix(1.0f);

    /*****************************************************************************************
     \brief  Throws std::runtime_error if a GL error is present (post-call guard).
     \param  where  Call site identifier for error context (function or step name).
     \throws std::runtime_error if glGetError() != GL_NO_ERROR.
    ******************************************************************************************/
    static inline void GL_THROW_IF_ERROR(const char* where) {
        GLenum e = glGetError();
        if (e != GL_NO_ERROR)
            throw std::runtime_error(std::string(where) + "|gl_error=" + std::to_string((int)e));
    }

    /*****************************************************************************************
     \brief  Compile a GLSL shader from source.
     \param  source Null-terminated GLSL source.
     \param  type   GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
     \return Shader handle.
     \throws std::runtime_error on compilation failure.
    ******************************************************************************************/
    static unsigned int compileShader(const char* source, GLenum type) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success;
        char infoLog[512];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation failed:\n" << infoLog << std::endl;
            throw std::runtime_error(std::string(type == GL_VERTEX_SHADER ? "compile_vs" : "compile_fs") + "|" + infoLog);
        }
        GL_THROW_IF_ERROR("compileShader");
        return shader;
    }

    /*****************************************************************************************
     \brief  Link a program from compiled vertex/fragment shaders and validate.
     \param  vSource Vertex shader source.
     \param  fSource Fragment shader source.
     \return Program handle. (Shaders are detached & deleted after link.)
     \throws std::runtime_error on link/validate failure.
    ******************************************************************************************/
    static unsigned int createShaderProgram(const char* vSource, const char* fSource) {
        unsigned int vertex = compileShader(vSource, GL_VERTEX_SHADER);
        unsigned int fragment = compileShader(fSource, GL_FRAGMENT_SHADER);

        unsigned int program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);

        int success;
        char infoLog[512];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Shader linking failed:\n" << infoLog << std::endl;
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            glDeleteProgram(program);
            throw std::runtime_error(std::string("link_program|") + infoLog);
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        glValidateProgram(program);
        int validated = 0;
        glGetProgramiv(program, GL_VALIDATE_STATUS, &validated);
        if (!validated) {
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            glDeleteProgram(program);
            throw std::runtime_error(std::string("validate_program|") + infoLog);
        }

        GL_THROW_IF_ERROR("createShaderProgram");
        return program;
    }

    /*****************************************************************************************
     \brief  Load a 2D texture via stb_image and configure basic filtering/wrap.
     \param  path Filesystem path to the image.
     \return GL texture handle.
     \throws std::runtime_error if the file cannot be loaded.
     \note   MIN_FILTER is GL_LINEAR; mipmaps are generated for future flexibility.
    ******************************************************************************************/
    unsigned int Graphics::loadTexture(const char* path) {
        unsigned int textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

        if (data) {
            GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        else {
            std::cerr << "Failed to load texture: " << path << std::endl;
            glBindTexture(GL_TEXTURE_2D, 0);
            if (textureID) glDeleteTextures(1, &textureID);
            throw std::runtime_error(std::string("texture_load|failed|") + path);
        }

        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);
        GL_THROW_IF_ERROR("loadTexture");
        return textureID;
    }

    /*****************************************************************************************
     \brief  Destroy a GL texture if non-zero.
     \param  tex GL texture handle.
    ******************************************************************************************/
    void Graphics::destroyTexture(unsigned int tex) {
        if (tex != 0)
            glDeleteTextures(1, &tex);
    }

    /*****************************************************************************************
     \brief  Create geometry (rect, circle, background, sprite), load background texture,
             build object/background/sprite shader programs, and compute rect pivot.
     \note   Must be called after a valid GL context is current.
    ******************************************************************************************/
    void Graphics::initialize() {
        // ----- Rect (positions+colors; indexed) -----
        float rectVertices[] = {
          -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
           0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
           0.5f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f,
          -0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 0.0f
        };
        unsigned int rectIndices[] = { 0,1,2, 2,3,0 };

        unsigned int EBO_rect;
        glGenVertexArrays(1, &VAO_rect);
        glGenBuffers(1, &VBO_rect);
        glGenBuffers(1, &EBO_rect);

        glBindVertexArray(VAO_rect);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_rect);
        glBufferData(GL_ARRAY_BUFFER, sizeof(rectVertices), rectVertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_rect);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(rectIndices), rectIndices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Compute local-space geometric center (pivot)
        sRectPivotX = (rectVertices[0] + rectVertices[6] + rectVertices[12] + rectVertices[18]) * 0.25f;
        sRectPivotY = (rectVertices[1] + rectVertices[7] + rectVertices[13] + rectVertices[19]) * 0.25f;

        // ----- Circle (triangle fan; positions+colors) -----
        std::vector<float> circleVertices;
        circleVertices.reserve((segments + 2) * 6);
        circleVertices.insert(circleVertices.end(), { 0.f, 0.f, 0.f, 0.f, 0.f, 1.f }); // center
        for (int i = 0; i <= segments; ++i) {
            float angle = (2.0f * PI * i) / segments;
            float x = std::cos(angle);
            float y = std::sin(angle);
            circleVertices.insert(circleVertices.end(), { x, y, 0.f, 0.f, 0.f, 1.f });
        }
        circleVertexCount = segments + 2;

        glGenVertexArrays(1, &VAO_circle);
        glGenBuffers(1, &VBO_circle);
        glBindVertexArray(VAO_circle);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_circle);
        glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float), circleVertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // ----- Fullscreen background (positions+uv) -----
        float bgVertices[] = {
          -1.0f,  1.0f,  0.0f, 1.0f,
          -1.0f, -1.0f,  0.0f, 0.0f,
           1.0f, -1.0f,  1.0f, 0.0f,
          -1.0f,  1.0f,  0.0f, 1.0f,
           1.0f, -1.0f,  1.0f, 0.0f,
           1.0f,  1.0f,  1.0f, 1.0f
        };
        glGenVertexArrays(1, &VAO_bg);
        glGenBuffers(1, &VBO_bg);
        glBindVertexArray(VAO_bg);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_bg);
        glBufferData(GL_ARRAY_BUFFER, sizeof(bgVertices), bgVertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // --- Load textures (via Resource_Manager) ---
        Resource_Manager::loadAll(Framework::ResolveAssetPath("Textures").string());

        // Use the string ID directly
        //bgTexture = Resource_Manager::resources_map["house"].handle;
        //if (!bgTexture) throw std::runtime_error("bg_texture|missing|house");

        // ----- Background shader -----
        const char* bgVertexSrc =
            "#version 330 core\n"
            "layout (location = 0) in vec2 aPos;\n"
            "layout (location = 1) in vec2 aTexCoord;\n"
            "out vec2 TexCoord;\n"
            "void main(){gl_Position=vec4(aPos,0.0,1.0);TexCoord=aTexCoord;}\n";
        const char* bgFragmentSrc =
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "in vec2 TexCoord;\n"
            "uniform sampler2D backgroundTex;\n"
            "void main(){FragColor=texture(backgroundTex,TexCoord);} \n";
        bgShader = createShaderProgram(bgVertexSrc, bgFragmentSrc);

        // ----- Object (rect/circle) shader -----
        const char* objVertexSrc =
            "#version 330 core\n"
            "layout (location = 0) in vec3 aPos;\n"
            "uniform mat4 uMVP;\n"
            "void main(){ gl_Position = uMVP * vec4(aPos, 1.0); }\n";
        const char* objFragmentSrc =
            "#version 330 core\n"
            "out vec4 FragColor;\n"
            "uniform vec4 uColor;\n"
            "void main(){ FragColor = uColor; }\n";
        objectShader = createShaderProgram(objVertexSrc, objFragmentSrc);

        // ----- Glow shader (circle with radial falloff) -----
        const char* glowVertexSrc =
            "#version 330 core\n"
            "layout (location = 0) in vec3 aPos;\n"
            "uniform mat4 uMVP;\n"
            "out vec2 vLocal;\n"
            "void main(){\n"
            "  vLocal = aPos.xy;\n"
            "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
            "}\n";
        const char* glowFragmentSrc =
            "#version 330 core\n"
            "in vec2 vLocal;\n"
            "out vec4 FragColor;\n"
            "uniform vec4 uColor;\n"
            "uniform float uInnerRadius;\n"
            "uniform float uOuterRadius;\n"
            "uniform float uBrightness;\n"
            "uniform float uFalloffExp;\n"
            "void main(){\n"
            "  float dist = length(vLocal);\n"
            "  float inner = max(uInnerRadius, 0.0001);\n"
            "  float outer = max(uOuterRadius, inner + 0.0001);\n"
            "  float t = clamp((dist - inner) / (outer - inner), 0.0, 1.0);\n"
            "  float falloff = pow(1.0 - t, max(uFalloffExp, 0.01));\n"
            "  float alpha = uColor.a * uBrightness * falloff;\n"
            "  FragColor = vec4(uColor.rgb * uBrightness, alpha);\n"
            "}\n";
        glowShader = createShaderProgram(glowVertexSrc, glowFragmentSrc);

        // ----- Sprite pipeline (quad VAO + shader with sub-UV) -----
        initSpritePipeline();

        glBindVertexArray(0);
        glUseProgram(0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        resetViewProjection();
        GL_THROW_IF_ERROR("initialize_end");
    }

    /*****************************************************************************************
     \brief  Draw the fullscreen background (textured triangle list).
    ******************************************************************************************/
    void Graphics::renderBackground() {
        static bool warnedMissingShader = false;
        static bool warnedMissingUniform = false;

        if (bgShader == 0 || bgTexture == 0)
        {
            if (!warnedMissingShader)
            {
                std::cerr << "[Graphics] Background shader/texture not initialized; skipping background draw.\n";
                warnedMissingShader = true;
            }
            return;
        }
        glUseProgram(bgShader);
        glActiveTexture(GL_TEXTURE0);
        int loc = glGetUniformLocation(bgShader, "backgroundTex");
        if (loc < 0)
        {
            if (!warnedMissingUniform)
            {
                std::cerr << "[Graphics] Background shader missing 'backgroundTex' uniform; skipping background draw.\n";
                warnedMissingUniform = true;
            }
            glUseProgram(0);
            return;
        }

        glUniform1i(loc, 0);
        glBindTexture(GL_TEXTURE_2D, bgTexture);
        glBindVertexArray(VAO_bg);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderBackground");
    }

    /*****************************************************************************************
     \brief  Draw a colored rectangle at (posX,posY) with rotation & scale; pivot-aware.
     \details Pivot is computed from the rectangle geometry. Because scale is applied last
              (rightmost in GLM chain), rotation is around the *scaled* pivot.
     \param  posX,posY  World position of the rectangle center.
     \param  rot        Rotation in radians about Z.
     \param  scaleX,Y   Non-uniform scale factors.
     \param  r,g,b,a    Color (linear) and alpha.
    ******************************************************************************************/
    void Graphics::renderRectangle(float posX, float posY, float rot,
        float scaleX, float scaleY,
        float r, float g, float b, float a) {
        glUseProgram(objectShader);

        // Rotate around the rectangle's geometric center (pivot).
        // Because scale happens first (rightmost), rotate around the "scaled pivot".
        const float pivot_sx = sRectPivotX * scaleX;
        const float pivot_sy = sRectPivotY * scaleY;

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::translate(model, glm::vec3(pivot_sx, pivot_sy, 0.0f));
        model = glm::rotate(model, rot, glm::vec3(0, 0, 1));
        model = glm::translate(model, glm::vec3(-pivot_sx, -pivot_sy, 0.0f));
        model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

        const glm::mat4 mvp = sViewProjectionMatrix * model;
        glUniformMatrix4fv(glGetUniformLocation(objectShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(objectShader, "uColor"), r, g, b, a);

        glBindVertexArray(VAO_rect);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderRectangle");
    }

    /*****************************************************************************************
     \brief  Draw a rectangle outline (wireframe) using the shared quad geometry.
     \param  lineWidth Thickness of the rendered outline in pixels.
    ******************************************************************************************/
    void Graphics::renderRectangleOutline(float posX, float posY, float rot, float scaleX, float scaleY,
        float r, float g, float b, float a, float lineWidth) {
        glUseProgram(objectShader);

        const float pivot_sx = sRectPivotX * scaleX;
        const float pivot_sy = sRectPivotY * scaleY;

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::translate(model, glm::vec3(pivot_sx, pivot_sy, 0.0f));
        model = glm::rotate(model, rot, glm::vec3(0, 0, 1));
        model = glm::translate(model, glm::vec3(-pivot_sx, -pivot_sy, 0.0f));
        model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

        const glm::mat4 mvp = sViewProjectionMatrix * model;
        glUniformMatrix4fv(glGetUniformLocation(objectShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(objectShader, "uColor"), r, g, b, a);

        const float width = (lineWidth <= 0.f) ? 1.f : lineWidth;

        glBindVertexArray(VAO_rect);
        glLineWidth(width);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        glLineWidth(1.0f);
        glBindVertexArray(0);

        glUseProgram(0);
        GL_THROW_IF_ERROR("renderRectangleOutline");
    }

    /*****************************************************************************************
     \brief  Convenience overload: uniform scaling and white color.
    ******************************************************************************************/
    void Graphics::renderRectangle(float posX, float posY, float rot, float scale) {
        renderRectangle(posX, posY, rot, scale, scale, 1.f, 1.f, 1.f, 1.f);
    }

    /*****************************************************************************************
     \brief  Draw a colored filled circle at (posX,posY).
     \param  radius Model scale for the procedurally-built unit circle.
    ******************************************************************************************/
    void Graphics::renderCircle(float posX, float posY, float radius,
        float r, float g, float b, float a) {
        glUseProgram(objectShader);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::scale(model, glm::vec3(radius, radius, 1.0f));

        const glm::mat4 mvp = sViewProjectionMatrix * model;
        glUniformMatrix4fv(glGetUniformLocation(objectShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(objectShader, "uColor"), r, g, b, a);

        glBindVertexArray(VAO_circle);
        glDrawArrays(GL_TRIANGLE_FAN, 0, circleVertexCount);
        glBindVertexArray(0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderCircle");
    }

    /*****************************************************************************************
     \brief  Draw a glow circle with radial falloff at (posX,posY).
    ******************************************************************************************/
    void Graphics::renderGlow(float posX, float posY, float innerRadius, float outerRadius,
        float brightness, float falloffExponent,
        float r, float g, float b, float a) {
        if (!glowShader)
            return;

        glUseProgram(glowShader);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::scale(model, glm::vec3(outerRadius, outerRadius, 1.0f));

        const glm::mat4 mvp = sViewProjectionMatrix * model;
        const float safeOuter = std::max(outerRadius, 0.0001f);
        float normalizedInner = innerRadius / safeOuter;
        normalizedInner = std::max(0.0f, std::min(normalizedInner, 0.999f));

        glUniformMatrix4fv(glGetUniformLocation(glowShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(glowShader, "uColor"), r, g, b, a);
        glUniform1f(glGetUniformLocation(glowShader, "uInnerRadius"), normalizedInner);
        glUniform1f(glGetUniformLocation(glowShader, "uOuterRadius"), 1.0f);
        glUniform1f(glGetUniformLocation(glowShader, "uBrightness"), brightness);
        glUniform1f(glGetUniformLocation(glowShader, "uFalloffExp"), falloffExponent);

        glBindVertexArray(VAO_circle);
        glDrawArrays(GL_TRIANGLE_FAN, 0, circleVertexCount);
        glBindVertexArray(0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderGlow");
    }

    /*****************************************************************************************
     \brief  Draw a textured sprite (entire texture) with tint.
     \details The sprite quad is centered at origin (pivot at center).
              Use renderSpriteFrame for sprite-sheet sub-rects.
    ******************************************************************************************/
    void Graphics::renderSprite(unsigned int tex, float posX, float posY, float rot,
        float scaleX, float scaleY,
        float r, float g, float b, float a) {
        glUseProgram(spriteShader);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::rotate(model, rot, glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

        const glm::mat4 mvp = sViewProjectionMatrix * model;
        glUniformMatrix4fv(glGetUniformLocation(spriteShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(spriteShader, "uTint"), r, g, b, a);

        // Whole-texture UVs
        glUniform2f(glGetUniformLocation(spriteShader, "uUVOffset"), 0.0f, 0.0f);
        glUniform2f(glGetUniformLocation(spriteShader, "uUVScale"), 1.0f, 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(spriteShader, "uTex"), 0);

        glBindVertexArray(VAO_sprite);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderSprite");
    }

    /*****************************************************************************************
     \brief  Draw a single frame from a sprite sheet laid out in (cols x rows).
     \param  tex         GL texture handle of the sheet.
     \param  frameIndex  Zero-based index. Frame (c,rIdx)=(frame%cols, frame/cols).
     \param  cols,rows   Sheet layout (>=1).
     \param  r,g,b,a     Tint color (multiplied with sampled texel).
     \note   UV origin is bottom-left (stb_image flipped); adjust if your assets differ.
    ******************************************************************************************/
    void Graphics::renderSpriteFrame(unsigned int tex,
        float posX, float posY, float rot, float scaleX, float scaleY,
        int frameIndex, int cols, int rows,
        float r, float g, float b, float a) {
        glUseProgram(spriteShader);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(posX, posY, 0.0f));
        model = glm::rotate(model, rot, glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));

        const glm::mat4 mvp = sViewProjectionMatrix * model;
        glUniformMatrix4fv(glGetUniformLocation(spriteShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(spriteShader, "uTint"), r, g, b, a);

        if (cols <= 0) cols = 1;
        if (rows <= 0) rows = 1;

        const float sx = 1.0f / static_cast<float>(cols);
        const float sy = 1.0f / static_cast<float>(rows);
        const int   c = frameIndex % cols;
        const int   rIdx = frameIndex / cols;

        // stbi flips Y => (0,0) bottom-left; single-row sheets: offY=rIdx*sy
        const float offX = c * sx;
        const float offY = rIdx * sy;

        glUniform2f(glGetUniformLocation(spriteShader, "uUVOffset"), offX, offY);
        glUniform2f(glGetUniformLocation(spriteShader, "uUVScale"), sx, sy);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(spriteShader, "uTex"), 0);

        glBindVertexArray(VAO_sprite);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderSpriteFrame");
    }

    /*****************************************************************************************
     \brief  Draw a batch of sprites using instanced rendering (raw pointer input).
     \param  tex        GL texture handle shared by all instances.
     \param  instances  Pointer to array of SpriteInstance.
     \param  count      Number of instances.
    ******************************************************************************************/
    void Graphics::renderSpriteBatchInstanced(unsigned int tex,
        const SpriteInstance* instances,
        size_t count) {
        if (!tex || !instances || count == 0)
            return;

        glUseProgram(spriteInstanceShader);
        glUniformMatrix4fv(glGetUniformLocation(spriteInstanceShader, "uVP"), 1, GL_FALSE, glm::value_ptr(sViewProjectionMatrix));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(spriteInstanceShader, "uTex"), 0);

        glBindVertexArray(VAO_sprite);

        glBindBuffer(GL_ARRAY_BUFFER, spriteInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(SpriteInstance) * count), instances, GL_DYNAMIC_DRAW);

        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, static_cast<GLsizei>(count));

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        GL_THROW_IF_ERROR("renderSpriteBatchInstanced");
    }

    /*****************************************************************************************
     \brief  Draw a batch of sprites using instanced rendering (vector input).
    ******************************************************************************************/
    void Graphics::renderSpriteBatchInstanced(unsigned int tex,
        const std::vector<SpriteInstance>& instances) {
        renderSpriteBatchInstanced(tex, instances.data(), instances.size());
    }

    /*****************************************************************************************
     \brief  Destroy GL resources created by initialize()/initSpritePipeline().
    ******************************************************************************************/
    void Graphics::cleanup() {
        glDeleteVertexArrays(1, &VAO_rect);
        glDeleteBuffers(1, &VBO_rect);

        glDeleteVertexArrays(1, &VAO_circle);
        glDeleteBuffers(1, &VBO_circle);

        glDeleteVertexArrays(1, &VAO_bg);
        glDeleteBuffers(1, &VBO_bg);
        glDeleteTextures(1, &bgTexture);
        glDeleteProgram(bgShader);

        glDeleteProgram(objectShader);

        glDeleteVertexArrays(1, &VAO_sprite);
        glDeleteBuffers(1, &VBO_sprite);
        glDeleteBuffers(1, &EBO_sprite);
        glDeleteProgram(spriteShader);

        glDeleteBuffers(1, &spriteInstanceVBO);
        glDeleteProgram(spriteInstanceShader);
        glDeleteProgram(glowShader);
    }

    /*****************************************************************************************
     \brief  Create sprite quad VAO/VBO/EBO and build the sprite shaders.
     \details Vertex layout: location 0 = vec3 position, location 1 = vec2 UV.
              The shader exposes uUVOffset/uUVScale to support sub-rect drawing.
              Also prepares per-instance attributes for instanced sprites.
    ******************************************************************************************/
    void Graphics::initSpritePipeline() {
        float spriteVerts[] = {
          -0.5f, -0.5f, 0.0f, 0.f, 0.f,
           0.5f, -0.5f, 0.0f, 1.f, 0.f,
           0.5f,  0.5f, 0.0f, 1.f, 1.f,
          -0.5f,  0.5f, 0.0f, 0.f, 1.f
        };
        unsigned int idx[] = { 0,1,2, 2,3,0 };

        glGenVertexArrays(1, &VAO_sprite);
        glGenBuffers(1, &VBO_sprite);
        glGenBuffers(1, &EBO_sprite);

        glBindVertexArray(VAO_sprite);

        glBindBuffer(GL_ARRAY_BUFFER, VBO_sprite);
        glBufferData(GL_ARRAY_BUFFER, sizeof(spriteVerts), spriteVerts, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_sprite);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Sprite (single) shader with sub-UV
        const char* vs =
            "#version 330 core\n"
            "layout(location=0) in vec3 aPos;\n"
            "layout(location=1) in vec2 aUV;\n"
            "uniform mat4 uMVP;\n"
            "uniform vec2 uUVOffset;\n"
            "uniform vec2 uUVScale;\n"
            "out vec2 vUV;\n"
            "void main(){\n"
            "  gl_Position = uMVP * vec4(aPos,1.0);\n"
            "  vUV = aUV * uUVScale + uUVOffset;\n"
            "}\n";
        const char* fs =
            "#version 330 core\n"
            "in vec2 vUV;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D uTex;\n"
            "uniform vec4 uTint;\n"
            "void main(){ FragColor = texture(uTex, vUV) * uTint; }\n";
        spriteShader = createShaderProgram(vs, fs);

        // Per-instance buffer (mat4 + tint + uv)
        if (!spriteInstanceVBO)
            glGenBuffers(1, &spriteInstanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, spriteInstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

        const GLsizei    stride = static_cast<GLsizei>(sizeof(SpriteInstance));
        const std::size_t modelOffset = offsetof(SpriteInstance, model);
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(2 + i);
            glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const void*>(modelOffset + sizeof(glm::vec4) * static_cast<std::size_t>(i)));
            glVertexAttribDivisor(2 + i, 1);
        }

        const std::size_t tintOffset = offsetof(SpriteInstance, tint);
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(tintOffset));
        glVertexAttribDivisor(6, 1);

        const std::size_t uvOffset = offsetof(SpriteInstance, uv);
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, stride,
            reinterpret_cast<const void*>(uvOffset));
        glVertexAttribDivisor(7, 1);

        // Instanced sprite shader
        const char* instVs =
            "#version 330 core\n"
            "layout(location=0) in vec3 aPos;\n"
            "layout(location=1) in vec2 aUV;\n"
            "layout(location=2) in mat4 iModel;\n"
            "layout(location=6) in vec4 iTint;\n"
            "layout(location=7) in vec4 iUV;\n"
            "uniform mat4 uVP;\n"
            "out vec2 vUV;\n"
            "out vec4 vTint;\n"
            "void main(){\n"
            "  gl_Position = uVP * iModel * vec4(aPos,1.0);\n"
            "  vUV  = aUV * iUV.zw + iUV.xy;\n"
            "  vTint= iTint;\n"
            "}\n";
        const char* instFs =
            "#version 330 core\n"
            "in vec2 vUV;\n"
            "in vec4 vTint;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D uTex;\n"
            "void main(){ FragColor = texture(uTex, vUV) * vTint; }\n";
        spriteInstanceShader = createShaderProgram(instVs, instFs);

        glBindVertexArray(0);
        GL_THROW_IF_ERROR("initSpritePipeline");
    }

    /*****************************************************************************************
     \brief  Draw a solid UI rectangle in pixel-space (origin bottom-left).
     \param  x,y,w,h  Pixel-space rectangle.
     \param  r,g,b,a  Fill color.
     \param  screenW,screenH  Current framebuffer size.
    ******************************************************************************************/
    void Graphics::renderRectangleUI(float x, float y, float w, float h,
        float r, float g, float b, float a,
        int screenW, int screenH) {
        glUseProgram(objectShader);

        glm::mat4 proj = glm::ortho(0.0f, float(screenW), 0.0f, float(screenH), -1.0f, 1.0f);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(x + w * 0.5f, y + h * 0.5f, 0.0f));
        model = glm::scale(model, glm::vec3(w, h, 1.0f));

        glm::mat4 mvp = proj * model;

        glUniformMatrix4fv(glGetUniformLocation(objectShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(objectShader, "uColor"), r, g, b, a);

        glBindVertexArray(VAO_rect);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderRectangleUI");
    }

    /*****************************************************************************************
     \brief  Draw a UI sprite in pixel-space (origin bottom-left).
     \param  tex      GL texture handle.
     \param  x,y,w,h  Pixel-space rect where the sprite is drawn.
     \param  r,g,b,a  Tint.
     \param  screenW,screenH  Current framebuffer size.
    ******************************************************************************************/
    void Graphics::renderSpriteUI(unsigned int tex, float x, float y, float w, float h,
        float r, float g, float b, float a,
        int screenW, int screenH) {
        if (!tex || !spriteShader || !VAO_sprite)
            return;

        glUseProgram(spriteShader);

        glm::mat4 proj = glm::ortho(0.0f, float(screenW), 0.0f, float(screenH), -1.0f, 1.0f);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(x + w * 0.5f, y + h * 0.5f, 0.0f));
        model = glm::scale(model, glm::vec3(w, h, 1.0f));

        glm::mat4 mvp = proj * model;

        glUniformMatrix4fv(glGetUniformLocation(spriteShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform4f(glGetUniformLocation(spriteShader, "uTint"), r, g, b, a);
        glUniform2f(glGetUniformLocation(spriteShader, "uUVOffset"), 0.0f, 0.0f);
        glUniform2f(glGetUniformLocation(spriteShader, "uUVScale"), 1.0f, 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(spriteShader, "uTex"), 0);

        glBindVertexArray(VAO_sprite);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderSpriteUI");
    }

    /*****************************************************************************************
     \brief  Query base-level dimensions of a GL texture.
     \param  tex   GL texture handle.
     \param  outW  Width (output).
     \param  outH  Height (output).
     \return true if valid and dimensions > 0, else false.
    ******************************************************************************************/
    bool Graphics::getTextureSize(unsigned int tex, int& outW, int& outH) {
        outW = 0;
        outH = 0;

        if (!tex)
            return false;

        glBindTexture(GL_TEXTURE_2D, tex);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &outW);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &outH);
        glBindTexture(GL_TEXTURE_2D, 0);

        GL_THROW_IF_ERROR("getTextureSize");
        return outW > 0 && outH > 0;
    }

    /*****************************************************************************************
     \brief  Draw a texture to the entire screen using the background shader/VAO.
     \param  tex GL texture handle.
    ******************************************************************************************/
    void Graphics::renderFullscreenTexture(unsigned tex) {
        if (!tex || !bgShader || !VAO_bg) return;

        glUseProgram(bgShader);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        int loc = glGetUniformLocation(bgShader, "backgroundTex");
        glUniform1i(loc, 0);

        glBindVertexArray(VAO_bg);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        GL_THROW_IF_ERROR("renderFullscreenTexture");
    }

    /*****************************************************************************************
     \brief  Set the view/projection matrices used by world-space rendering.
     \param  view View matrix.
     \param  proj Projection matrix.
    ******************************************************************************************/
    void Graphics::setViewProjection(const glm::mat4& view, const glm::mat4& proj) {
        sViewMatrix = view;
        sProjectionMatrix = proj;
        sViewProjectionMatrix = proj * view;
    }

    /*****************************************************************************************
     \brief  Reset view/projection to identity (NDC space).
    ******************************************************************************************/
    void Graphics::resetViewProjection() {
        setViewProjection(glm::mat4(1.0f), glm::mat4(1.0f));
    }

    /*****************************************************************************************
     \brief  Intentionally perturb GL state for crash/robustness testing.
     \param  which 1: bg shader=0, 2: bg VAO=0, 3: sprite shader=0,
                    4: object shader=0, 5: delete bg texture.
    ******************************************************************************************/
    void Graphics::testCrash(int which) {
        if (which == 1) { bgShader = 0; }
        else if (which == 2) { VAO_bg = 0; }
        else if (which == 3) { spriteShader = 0; }
        else if (which == 4) { objectShader = 0; }
        else if (which == 5) {
            if (bgTexture) {
                glDeleteTextures(1, &bgTexture);
                bgTexture = 0;
            }
        }
    }

} // namespace gfx
