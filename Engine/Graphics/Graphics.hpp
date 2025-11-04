/*********************************************************************************************
 \file      Graphics.hpp
 \par       SofaSpuds
 \author    SofaSpuds Team
 \brief     Public 2D rendering API: init/teardown, background, shapes, sprites, and
            sprite-sheet frame drawing. Also exposes simple crash/robustness test hooks.
 \details   Static-only helper used by the sandbox/game. Call initialize() once after a valid
            OpenGL context is current; call cleanup() at shutdown. Textures are loaded through
            stb_image (via loadTexture) or the engine Resource_Manager. Sprite-sheet animation
            is driven by callers with renderSpriteFrame(...).
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <glad/glad.h>
#include "../Resource_Manager/Resource_Manager.h"
#include <glm/mat4x4.hpp>
#include <vector>
namespace gfx {

    class Graphics {
    public:
        struct SpriteInstance {
            glm::mat4 model{ 1.0f };
            glm::vec4 tint{ 1.0f, 1.0f, 1.0f, 1.0f };
            glm::vec4 uv{ 0.0f, 0.0f, 1.0f, 1.0f };
        };
        /**
         * \brief Load a 2D texture from disk (stb_image) and set basic filtering/wrap.
         * \param path Filesystem path.
         * \return GL texture handle.
         */
        static unsigned int loadTexture(const char* path);

        /**
         * \brief Destroy a previously created texture handle.
         */
        static void destroyTexture(unsigned int tex);

        /**
         * \brief Create geometry VAOs/VBOs, build shaders, and prepare background resources.
         * \note  Must be called after a valid GL context is current.
         */
        static void initialize();

        /**
         * \brief Draw fullscreen textured background.
         */
        static void renderBackground();

        static void renderFullscreenTexture(unsigned tex);

        /**
         * \brief Configure the active camera view/projection used for world-space rendering.
         * \param view  View matrix (usually from camera transform).
         * \param proj  Projection matrix (orthographic or perspective).
         */
        static void setViewProjection(const glm::mat4& view, const glm::mat4& proj);

        /**
         * \brief Reset camera transforms to identity (useful for UI/menus).
         */
        static void resetViewProjection();

        /**
         * \brief Draw a colored rectangle at (posX,posY) with rotation and non-uniform scale.
         * \param r,g,b,a Multiplicative tint.
         */
        static void renderRectangle(float posX, float posY, float rot,
            float scaleX, float scaleY,
            float r, float g, float b, float a);


        /**
           * \brief Draw an outline-only rectangle using the shared quad geometry.
           * \param lineWidth Thickness of the outline in pixels (clamped to >= 1).
           */
        static void renderRectangleOutline(float posX, float posY, float rot,
            float scaleX, float scaleY,
            float r, float g, float b, float a,
            float lineWidth = 1.f);


        /**
         * \brief Convenience rectangle draw with uniform scale and white tint.
         */
        static void renderRectangle(float posX, float posY, float rot, float scale);

        /**
         * \brief Draw a colored filled circle at (posX,posY) with given radius.
         */
        static void renderCircle(float posX, float posY, float radius,
            float r, float g, float b, float a);


        // Draw a UI rect in pixel coordinates (origin bottom-left)
        static void renderRectangleUI(float x, float y, float w, float h,
            float r, float g, float b, float a,
            int screenW, int screenH);

        // Draw a sprite in UI pixel coordinates (origin bottom-left)
        static void renderSpriteUI(unsigned int tex, float x, float y, float w, float h,
            float r, float g, float b, float a,
            int screenW, int screenH);

        /// Query the width/height (level 0) of a 2D texture. Returns false if unavailable.
        static bool getTextureSize(unsigned int tex, int& outW, int& outH);

        /**
         * \brief Draw a whole-texture sprite with tint.
         * \param tex GL texture handle.
         */
        static void renderSprite(unsigned int tex, float posX, float posY, float rot,
            float scaleX, float scaleY, float r, float g, float b, float a);

        /**
         * \brief Draw a single frame from a sprite sheet (laid out as cols x rows).
         * \param tex         GL texture handle of the sheet.
         * \param frameIndex  Zero-based frame index (c = frame % cols, r = frame / cols).
         * \param cols,rows   Sheet layout.
         * \param r,g,b,a     Multiplicative tint (defaults to white).
         */
        static void renderSpriteFrame(unsigned int tex,
            float posX, float posY, float rot,
            float scaleX, float scaleY,
            int frameIndex, int cols, int rows,
            float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f);
        /**
         * \brief Draw multiple sprites sharing the same texture using instanced rendering.
         * \param tex        GL texture handle shared by all instances.
         * \param instances  Array of per-instance transforms/tints/UVs.
         * \param count      Number of instances in the array.
         */
        static void renderSpriteBatchInstanced(unsigned int tex, const SpriteInstance* instances, size_t count);

        /**
         * \brief Convenience overload accepting an std::vector of SpriteInstance data.
         */
        static void renderSpriteBatchInstanced(unsigned int tex, const std::vector<SpriteInstance>& instances);
        /**
         * \brief Release GL resources created by initialize().
         */
        static void cleanup();

        /**
         * \brief Intentionally perturb GL state for crash/robustness testing.
         * \param which 1: bg shader=0, 2: bg VAO=0, 3: sprite shader=0,
         *              4: object shader=0, 5: delete bg texture.
         */
        static void testCrash(int which);

    private:
        /// Build sprite quad VAO/EBO/VBO and sprite shader (supports sub-UV).
        static void initSpritePipeline();

        // --- GL objects & state (created in initialize/initSpritePipeline) ---
        static unsigned int VAO_rect, VBO_rect;
        static unsigned int VAO_circle, VBO_circle;
        static int          circleVertexCount;
        static unsigned int VAO_bg, VBO_bg, bgTexture;

        // Programs
        static unsigned int bgShader;
        static unsigned int objectShader;
        static unsigned int VAO_sprite, VBO_sprite, EBO_sprite;
        static unsigned int spriteShader;
        static unsigned int spriteInstanceVBO;
        static unsigned int spriteInstanceShader;
    };

} // namespace gfx

#endif // GRAPHICS_HPP
