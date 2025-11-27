/*********************************************************************************************
 \file      GraphicsText.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     FreeType-backed OpenGL text rendering: shader setup, glyph caching, and draw calls.
 \details   Builds a small shader pair (vertex/fragment) with a pixel-space orthographic
            projection, loads ASCII glyphs (0–127) via FreeType into single-channel (GL_RED)
            textures with metrics (size, bearing, advance), and renders strings as textured
            quads updated through a dynamic VBO. Provides viewport updates and resource cleanup.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "GraphicsText.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <stdexcept>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace gfx {

    /*************************************************************************************
      \brief  Compile a GLSL shader from source and return its handle.
      \param  source Null-terminated GLSL source string.
      \param  type   GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
      \return Shader handle.
      \throws std::runtime_error on compilation failure (stage-tag + info log).
    *************************************************************************************/
    static unsigned int compileShader(const char* source, GLenum type) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader compilation failed:\n" << infoLog << std::endl;
            throw std::runtime_error(std::string(type == GL_VERTEX_SHADER ? "text_vs" : "text_fs") + "|" + infoLog);
        }
        return shader;
    }

    /*************************************************************************************
      \brief  Link a program from vertex/fragment source strings.
      \param  vSource Vertex shader source.
      \param  fSource Fragment shader source.
      \return Program handle. Stages are deleted after a successful link.
      \throws std::runtime_error on link failure (includes info log).
    *************************************************************************************/
    static unsigned int createShaderProgram(const char* vSource, const char* fSource) {
        unsigned int vertex = compileShader(vSource, GL_VERTEX_SHADER);
        unsigned int fragment = compileShader(fSource, GL_FRAGMENT_SHADER);

        unsigned int program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);

        int success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Shader linking failed:\n" << infoLog << std::endl;
            throw std::runtime_error(std::string("text_link|") + infoLog);
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return program;
    }

    /*************************************************************************************
      \brief  Initialize text rendering: build shaders, set pixel-space projection,
              load ASCII glyphs with FreeType, and create VAO/VBO for quads.
      \param  fontPath Filesystem path to a TTF/OTF font.
      \param  width    Viewport width in pixels (for ortho).
      \param  height   Viewport height in pixels (for ortho).
      \details On FreeType/font failure, logs and returns early so the app can continue
               (renderer becomes a no-op).
    *************************************************************************************/
    void TextRenderer::initialize(const char* fontPath, unsigned int width, unsigned int height) {
        // text shaders
        const char* vShader =
            "#version 330 core\n"
            "layout (location = 0) in vec4 vertex;\n"
            "out vec2 TexCoords;\n"
            "uniform mat4 projection;\n"
            "void main() {\n"
            "    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);\n"
            "    TexCoords = vertex.zw;\n"
            "}\n";

        const char* fShader =
            "#version 330 core\n"
            "in vec2 TexCoords;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D text;\n"
            "uniform vec3 textColor;\n"
            "void main() {\n"
            "    float alpha = texture(text, TexCoords).r;\n"
            "    FragColor = vec4(textColor, alpha);\n"
            "}\n";

        shaderID = createShaderProgram(vShader, fShader);

        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(width),
            0.0f, static_cast<float>(height));
        glUseProgram(shaderID);
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUniform1i(glGetUniformLocation(shaderID, "text"), 0);
        glUseProgram(0);

        // FreeType init (graceful: do NOT crash log on failure; just skip text)
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
            return;
        }

        FT_Face face;
        if (FT_New_Face(ft, fontPath, 0, &face)) {
            std::cerr << "ERROR::FREETYPE: Failed to load font" << std::endl;
            FT_Done_FreeType(ft);
            return;
        }
        FT_Set_Pixel_Sizes(face, 0, 48);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cerr << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
                continue;
            }
            unsigned int texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RED,
                face->glyph->bitmap.width, face->glyph->bitmap.rows,
                0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Character character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<unsigned int>(face->glyph->advance.x)
            };
            Characters.insert(std::pair<char, Character>(c, character));
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    /*************************************************************************************
      \brief  Update the orthographic projection after a viewport resize.
      \param  width   New viewport width in pixels.
      \param  height  New viewport height in pixels.
    *************************************************************************************/
    void TextRenderer::setViewport(unsigned int width, unsigned int height) {
        if (!shaderID) return;
        glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(width),
            0.0f, static_cast<float>(height));
        glUseProgram(shaderID);
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, &projection[0][0]);
        glUseProgram(0);
    }

    /*************************************************************************************
      \brief  Render a string at a pixel position using cached ASCII glyphs.
      \param  text   ASCII string (0–127 cached).
      \param  x,y    Baseline origin in pixels.
      \param  scale  Uniform scale factor for glyph quads.
      \param  color  RGB tint; alpha comes from glyph texture (red channel).
      \details Advances the pen using FreeType’s 26.6 fixed-point advance (Advance >> 6).
    *************************************************************************************/
    void TextRenderer::RenderText(std::string text, float x, float y, float scale, glm::vec3 color) {
        if (!shaderID || Characters.empty()) return; // nothing to render
        glUseProgram(shaderID);
        glUniform3f(glGetUniformLocation(shaderID, "textColor"), color.x, color.y, color.z);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        for (char c : text) {
            Character ch = Characters[c];

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // advance cursor for the next character (FreeType uses 1/64 pixels)
            x += (ch.Advance >> 6) * scale;
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

    /*************************************************************************************
      \brief  Release GL resources created by initialize().
      \details Deletes glyph textures, VAO/VBO, and shader program; resets handles to 0.
    *************************************************************************************/
    void TextRenderer::cleanup() {
        for (auto& pair : Characters) {
            glDeleteTextures(1, &pair.second.TextureID);
        }
        Characters.clear();
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (shaderID) glDeleteProgram(shaderID);
        VAO = 0; VBO = 0; shaderID = 0;
    }

} // namespace gfx
