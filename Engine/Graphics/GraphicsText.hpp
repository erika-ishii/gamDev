/*********************************************************************************************
 \file      GraphicsText.hpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     Public interface for FreeType-backed OpenGL text rendering (shader/projection,
            ASCII glyph cache, and draw API).
 \details   Provides a small API to:
            - initialize() : compile a text shader, set a pixel-space orthographic projection,
              load ASCII glyphs (0–127) into GL_RED textures, and build VAO/VBO.
            - setViewport(): refresh the projection on window resize.
            - RenderText() : draw a string at a pixel position with color and scale.
            - cleanup()    : release glyph textures and GL objects.
            Call initialize() once after a valid GL context is current; call cleanup() at shutdown.
            If FreeType/font loading fails, the renderer no-ops gracefully (no hard crash).
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <map>
#include <string>

namespace gfx {

    struct Character {
        unsigned int TextureID;
        glm::ivec2   Size;
        glm::ivec2   Bearing;
        unsigned int Advance;
    };

    class TextRenderer {
    public:
        void initialize(const char* fontPath, unsigned int width, unsigned int height);
        void setViewport(unsigned int width, unsigned int height);
        void RenderText(std::string text, float x, float y, float scale, glm::vec3 color);
        void cleanup();

    private:
        unsigned int shaderID = 0;
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        std::map<char, Character> Characters;
    };

} // namespace gfx
