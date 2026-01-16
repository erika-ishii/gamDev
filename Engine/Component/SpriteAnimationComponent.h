/*********************************************************************************************
 \file      SpriteAnimationComponent.h
 \par       SofaSpuds
 \author     Ho Jun (h.jun@digipen.edu) - Primary Author, 100% - Primary Author, 60%
            yimo.kong ( yimo.kong@digipen.edu) - Author, 40%

 \brief     Declares the SpriteAnimationComponent responsible for 2D sprite-based animation.
            Supports both legacy frame-array animations and modern sprite-sheet animations
            (grid-based UV sampling), with per-animation FPS, looping, and active animation
            selection. Integrates with the Resource_Manager to resolve and (re)bind textures.

 \details   Responsibilities:
            - Supports legacy frame-array animations and grid-based sprite-sheet animations.
            - Stores per-animation playback parameters (FPS, looping, frame range).
            - Integrates with Resource_Manager to resolve and (re)bind textures at runtime.
            - Advances animation state over time and exposes sampling info (texture + UV)
              for the RenderSystem and editor tools such as the Animation Editor panel.
            - Handles JSON-driven serialization for both legacy and sprite-sheet formats,
              including restoration of the active animation index.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Resource_Asset_Manager/Resource_Manager.h"
#include "Core/PathUtils.h"
#include <glm/vec4.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Framework {

    /*************************************************************************************
      \struct SpriteAnimationFrame
      \brief  Legacy frame representation: each frame points to a single texture.

      Used by the older frame-array animation path, where each animation frame is an
      independent texture (or can be lazily loaded from a path).
    *************************************************************************************/
    struct SpriteAnimationFrame {
        std::string texture_key; ///< Resource_Manager key for the frame texture.
        std::string path;        ///< Optional relative/absolute path to load the texture.
    };

    /*****************************************************************************************
      \class SpriteAnimationComponent
      \brief Component that manages sprite animations for a GameObject.

      Supports two animation styles:
      - Legacy frame-array animation (frames[] + fps/loop/play flags).
      - Modern sprite-sheet animations (animations[], each with a grid of frames).

      The component is responsible for:
      - Advancing animation time and updating current frame indices.
      - Serializing/deserializing animation data from JSON.
      - Resolving and binding textures via the Resource_Manager.
      - Providing sampling information (texture + UV rect) for rendering.
    *****************************************************************************************/
    class SpriteAnimationComponent : public GameComponent {
    public:
        // --- Sprite-sheet based animation config -----------------------------
        /*************************************************************************************
          \struct AnimConfig
          \brief  Configuration describing how to interpret a sprite sheet as frames.

          Defines grid layout and playback behavior (fps, looping, frame range).
        *************************************************************************************/
        struct AnimConfig {
            int   totalFrames{ 1 };  ///< Total number of frames in the sheet.
            int   rows{ 1 };         ///< Number of rows in the spritesheet grid.
            int   columns{ 1 };      ///< Number of columns in the spritesheet grid.
            int   startFrame{ 0 };   ///< First frame index to play (inclusive).
            int   endFrame{ -1 };    ///< Last frame index (inclusive); -1 = totalFrames - 1.
            float fps{ 6.0f };       ///< Playback speed in frames per second.
            bool  loop{ true };      ///< Whether this animation loops when it reaches the end.
        };

        /*************************************************************************************
          \struct SpriteSheetAnimation
          \brief  Represents a single named animation backed by a sprite sheet.

          Contains:
          - Metadata (name, spriteSheetPath, textureKey).
          - Layout and playback configuration (AnimConfig).
          - Runtime state (currentFrame, accumulator, textureId).
        *************************************************************************************/
        struct SpriteSheetAnimation {
            std::string name{ "idle" };       ///< Logical name of the animation (e.g., "run").
            std::string spriteSheetPath{};    ///< Original path to the sprite sheet asset.
            std::string textureKey{};         ///< Resource_Manager key used to fetch the texture.
            AnimConfig  config{};             ///< Grid and playback configuration.

            int   currentFrame{ 0 };          ///< Current frame index within [startFrame, endFrame].
            float accumulator{ 0.0f };        ///< Accumulated time since last frame switch.
            unsigned textureId{ 0 };          ///< Cached GL texture ID (loaded lazily).
        };

        // --- Old frame-array style animation (still supported) --------------
        float fps{ 6.0f };   ///< Legacy animation fps for frames[].
        bool  loop{ true };  ///< Whether frames[] animation loops.
        bool  play{ true };  ///< Whether frames[] animation is currently playing.

        std::vector<SpriteAnimationFrame> frames{}; ///< Legacy list of frame textures.

        // --- sprite-sheet animation set ----------------------------------
        std::vector<SpriteSheetAnimation> animations{}; ///< Set of named sprite-sheet animations.
        int activeAnimation{ 0 };                       ///< Index into animations[] for active clip.

        // ---------------------------------------------------------------------
        // Engine lifecycle
        // ---------------------------------------------------------------------
        /*************************************************************************************
          \brief Called when the component is attached to its owner.

          Preloads textures for legacy frame-array animations so that all frames are
          ready when the game starts running.
        *************************************************************************************/
        void initialize() override {
            PreloadFrames();
        }

        /*************************************************************************************
          \brief Deserialize animation data from the given serializer.

          Expected JSON fields:
          - "fps" / "loop" / "play" for legacy frame-array animation.
          - "frames"[] with "texture_key" and optional "path".
          - "animations"[] each with:
            * "name", "textureKey", "spriteSheetPath"
            * "config" object containing layout/fps/loop fields
            * optional "currentFrame"
          - "activeAnimation" index to restore current sheet animation selection.
        *************************************************************************************/
        void Serialize(ISerializer& s) override {
            if (s.HasKey("fps")) StreamRead(s, "fps", fps);

            if (s.HasKey("loop")) {
                int loopInt = static_cast<int>(loop);
                StreamRead(s, "loop", loopInt);
                loop = loopInt != 0;
            }

            if (s.HasKey("play")) {
                int playInt = static_cast<int>(play);
                StreamRead(s, "play", playInt);
                play = playInt != 0;
            }

            // Legacy frames[] array
            if (s.EnterArray("frames")) {
                frames.clear();
                frames.reserve(s.ArraySize());
                for (size_t i = 0; i < s.ArraySize(); ++i) {
                    if (!s.EnterIndex(i))
                        continue;

                    SpriteAnimationFrame frame;
                    if (s.HasKey("texture_key")) StreamRead(s, "texture_key", frame.texture_key);
                    if (s.HasKey("path"))        StreamRead(s, "path", frame.path);
                    frames.push_back(std::move(frame));
                    s.ExitObject();
                }
                s.ExitArray();
            }

            // --- NEW: sprite-sheet animations[] -----------------------------------
            if (s.EnterArray("animations")) {
                animations.clear();
                animations.reserve(s.ArraySize());

                for (size_t i = 0; i < s.ArraySize(); ++i) {
                    if (!s.EnterIndex(i))
                        continue;

                    SpriteSheetAnimation sheet{};

                    if (s.HasKey("name"))            StreamRead(s, "name", sheet.name);
                    if (s.HasKey("textureKey"))      StreamRead(s, "textureKey", sheet.textureKey);
                    if (s.HasKey("spriteSheetPath")) StreamRead(s, "spriteSheetPath", sheet.spriteSheetPath);

                    // config object
                    if (s.EnterObject("config")) {
                        StreamRead(s, "totalFrames", sheet.config.totalFrames);
                        StreamRead(s, "rows", sheet.config.rows);
                        StreamRead(s, "columns", sheet.config.columns);
                        StreamRead(s, "startFrame", sheet.config.startFrame);
                        StreamRead(s, "endFrame", sheet.config.endFrame);
                        StreamRead(s, "fps", sheet.config.fps);
                        int loopInt = sheet.config.loop ? 1 : 0;
                        StreamRead(s, "loop", loopInt);
                        sheet.config.loop = (loopInt != 0);
                        s.ExitObject();
                    }

                    if (s.HasKey("currentFrame"))
                        StreamRead(s, "currentFrame", sheet.currentFrame);

                    // runtime fields are reset on load
                    sheet.accumulator = 0.0f;
                    sheet.textureId = 0;  // lazily loaded later

                    animations.push_back(std::move(sheet));

                    s.ExitObject(); // leave animations[i]
                }

                s.ExitArray();
            }

            // active animation index
            if (s.HasKey("activeAnimation")) {
                StreamRead(s, "activeAnimation", activeAnimation);
            }
        }

        /*************************************************************************************
          \brief Create a polymorphic deep copy of this component.

          Copies both legacy frame-array animation data and sprite-sheet animations, as
          well as current playback state (currentFrame/accumulator/activeAnimation).
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override {
            auto copy = std::make_unique<SpriteAnimationComponent>();
            copy->fps = fps;
            copy->loop = loop;
            copy->play = play;
            copy->frames = frames;
            copy->currentFrame = currentFrame;
            copy->accumulator = accumulator;
            copy->animations = animations;
            copy->activeAnimation = activeAnimation;
            return copy;
        }

        /*************************************************************************************
          \brief Handle incoming messages sent to this component.

          \note Currently unused (no-op). Can be extended later for event-driven animation
                changes (e.g., "OnAttackStart" → switch to attack animation).
        *************************************************************************************/
        void SendMessage(Message&) override {}

        // ---------------------------------------------------------------------
        // Basic info helpers
        // ---------------------------------------------------------------------
        /*************************************************************************************
          \brief Check if the legacy frame-array animation has any frames.
          \return True if frames[] is non-empty.
        *************************************************************************************/
        bool HasFrames() const { return !frames.empty(); }

        /*************************************************************************************
          \brief Check if any sprite-sheet animations are defined.
          \return True if animations[] is non-empty.
        *************************************************************************************/
        bool HasSpriteSheets() const { return !animations.empty(); }

        /*************************************************************************************
          \brief Get the number of legacy frames available.
          \return Size of frames[].
        *************************************************************************************/
        size_t FrameCount() const { return frames.size(); }

        /*************************************************************************************
          \brief Get the number of sprite-sheet animations.
          \return Size of animations[].
        *************************************************************************************/
        std::size_t AnimationCount() const { return animations.size(); }

        /*************************************************************************************
          \brief Get the current legacy frame index.
          \return Index into frames[].
        *************************************************************************************/
        size_t CurrentFrameIndex() const { return currentFrame; }

        /*************************************************************************************
          \brief Get the currently active sprite-sheet animation index (clamped).
          \return -1 if there are no animations, otherwise a valid index in [0, size-1].
        *************************************************************************************/
        int ActiveAnimationIndex() const {
            if (animations.empty())
                return -1;
            const int clamped = std::clamp(
                activeAnimation,
                0,
                static_cast<int>(animations.size()) - 1
            );
            return clamped;
        }

        /*************************************************************************************
          \brief Get a pointer to the active sprite-sheet animation.
          \return Pointer to the active SpriteSheetAnimation, or nullptr if none exist.
        *************************************************************************************/
        SpriteSheetAnimation* ActiveAnimation() {
            const int idx = ActiveAnimationIndex();
            if (idx < 0)
                return nullptr;
            return &animations[static_cast<std::size_t>(idx)];
        }

        /*************************************************************************************
          \brief Get a const pointer to the active sprite-sheet animation.
          \return Const pointer to active SpriteSheetAnimation, or nullptr if none exist.
        *************************************************************************************/
        const SpriteSheetAnimation* ActiveAnimation() const {
            const int idx = ActiveAnimationIndex();
            if (idx < 0)
                return nullptr;
            return &animations[static_cast<std::size_t>(idx)];
        }

        /*************************************************************************************
          \brief Set the active sprite-sheet animation by index.

          Clamps the index to a valid range and resets the animation's runtime state
          (currentFrame = 0, accumulator = 0.0f) if successful.
        *************************************************************************************/
        void SetActiveAnimation(int index) {
            if (index < 0 || animations.empty())
                return;

            const int maxIndex = static_cast<int>(animations.size()) - 1;
            activeAnimation = std::clamp(index, 0, maxIndex);

            if (auto* anim = ActiveAnimation()) {
                anim->currentFrame = 0;
                anim->accumulator = 0.0f;
            }
        }

        // ---------------------------------------------------------------------
        // Texture maintenance helpers (used after undo/redo)
        // ---------------------------------------------------------------------
        /*************************************************************************************
          \brief Rebind textures for all sprite-sheet animations.

          Useful after undo/redo or hot-reload when texture IDs may no longer be valid.
          Attempts to:
          - Reuse existing textureKey, or
          - Reload from spriteSheetPath if needed.
        *************************************************************************************/
        void RebindAllTextures() {
            for (auto& anim : animations) {
                // Try existing key first
                if (!anim.textureKey.empty()) {
                    anim.textureId = Resource_Manager::getTexture(anim.textureKey);
                }

                // If still missing, attempt to reload from spriteSheetPath
                if (!anim.textureId && !anim.spriteSheetPath.empty()) {
                    ReloadAnimationTexture(anim);
                }
            }
        }

        // ---------------------------------------------------------------------
        // Old frame-array texture resolving
        // ---------------------------------------------------------------------
        /*************************************************************************************
          \brief Resolve and load (if necessary) the texture for the given legacy frame.
          \param  index Index into frames[].
          \return GL texture ID, or 0 if loading failed.
        *************************************************************************************/
        unsigned ResolveFrameTexture(size_t index) {
            if (index >= frames.size())
                return 0;

            const auto& frame = frames[index];
            unsigned tex = Resource_Manager::getTexture(frame.texture_key);
            if (!tex && !frame.path.empty()) {
                const auto resolvedPath = ResolveAnimationPath(frame.path);
                if (Resource_Manager::load(frame.texture_key, resolvedPath))
                    tex = Resource_Manager::getTexture(frame.texture_key);
            }
            return tex;
        }

        /*************************************************************************************
          \brief Set the current legacy frame index.
          \param  index New index into frames[].
        *************************************************************************************/
        void SetFrame(size_t index) {
            if (index < frames.size())
                currentFrame = index;
        }

        // ---------------------------------------------------------------------
        // Update
        // ---------------------------------------------------------------------
        /*************************************************************************************
          \brief Advance both legacy frame-array and sprite-sheet animations.
          \param dt Delta time in seconds since last update.
        *************************************************************************************/
        void Advance(float dt) {
            AdvanceFrameArray(dt);
            AdvanceSpriteSheets(dt);
        }

        /*************************************************************************************
          \brief Advance the legacy frame-array animation.

          Uses fps / loop / play flags and dt to step the frame index forward,
          wrapping or stopping based on the loop state.
        *************************************************************************************/
        void AdvanceFrameArray(float dt) {
            if (!play || frames.empty() || fps <= 0.f)
                return;

            const float frameDuration = 1.0f / fps;
            accumulator += dt;
            while (accumulator >= frameDuration) {
                accumulator -= frameDuration;
                if (currentFrame + 1 < frames.size()) {
                    ++currentFrame;
                }
                else if (loop) {
                    currentFrame = 0;
                }
                else {
                    play = false;
                    break;
                }
            }
        }

        /*************************************************************************************
          \brief Advance the active sprite-sheet animation according to its AnimConfig.

          Uses per-animation fps, startFrame, endFrame, and loop to move through frames
          as time (dt) accumulates.
        *************************************************************************************/
        void AdvanceSpriteSheets(float dt) {
            auto* anim = ActiveAnimation();
            if (!anim || anim->config.fps <= 0.f)
                return;

            const int totalFrames = std::max(1, anim->config.totalFrames);
            const int startFrame = std::clamp(anim->config.startFrame, 0, totalFrames - 1);
            const int endFrame = anim->config.endFrame >= 0
                ? std::clamp(anim->config.endFrame, startFrame, totalFrames - 1)
                : totalFrames - 1;

            const float frameDuration = 1.0f / anim->config.fps;
            anim->accumulator += dt;

            while (anim->accumulator >= frameDuration) {
                anim->accumulator -= frameDuration;
                if (anim->currentFrame < endFrame) {
                    ++anim->currentFrame;
                }
                else if (anim->config.loop) {
                    anim->currentFrame = startFrame;
                }
            }
        }

        // ---------------------------------------------------------------------
        // Sampling current sprite-sheet frame (for RenderSystem / Animation Editor)
        // ---------------------------------------------------------------------
        /*************************************************************************************
          \struct SheetSample
          \brief  Return value for sampling a sprite-sheet frame.

          Packs:
          - texture: GL texture ID to use.
          - uv:      UV rectangle (x, y, width, height) inside the sheet.
          - textureKey: resource key (useful for editor UI/inspection).
        *************************************************************************************/
        struct SheetSample {
            unsigned    texture{ 0 };          ///< GL texture ID of the spritesheet.
            glm::vec4   uv{ 0.f, 0.f, 1.f, 1.f }; ///< UV rectangle of the current frame.
            std::string textureKey;           ///< Resource_Manager key used for the texture.
        };

        /*************************************************************************************
          \brief Sample the current frame of the active sprite-sheet animation.

          Computes the UV sub-rectangle based on rows/columns and the current frame index,
          and ensures the spritesheet texture is loaded.

          \return A SheetSample with texture ID and UV rect; texture == 0 if invalid.
        *************************************************************************************/
        SheetSample CurrentSheetSample() {
            SheetSample sample{};
            auto* anim = ActiveAnimation();
            if (!anim)
                return sample;

            if (!anim->textureKey.empty())
                sample.textureKey = anim->textureKey;

            EnsureTexture(*anim);
            sample.texture = anim->textureId;

            const int columns = std::max(1, anim->config.columns);
            const int rows = std::max(1, anim->config.rows);
            const int total = std::max(1, anim->config.totalFrames);
            int frameIndex = std::clamp(anim->currentFrame, 0, total - 1);

            const int col = frameIndex % columns;
            const int row = frameIndex / columns;

            const float invCols = 1.0f / static_cast<float>(columns);
            const float invRows = 1.0f / static_cast<float>(rows);

            sample.uv = glm::vec4(
                static_cast<float>(col) * invCols,
                static_cast<float>(row) * invRows,
                invCols,
                invRows
            );

            return sample;
        }

        /*************************************************************************************
          \brief Ensure a default set of named animations exists.

          If animations[] is empty, populates a basic set:
          - "idle", "run", "attack1", "attack2", "attack3"
          with corresponding default textureKey "<name>_sheet".
        *************************************************************************************/
        void EnsureDefaultAnimations() {
            if (!animations.empty())
                return;

            static constexpr const char* kDefaultNames[] = {
                "idle", "run", "attack1", "attack2", "attack3"
            };

            animations.clear();
            for (const char* name : kDefaultNames) {
                SpriteSheetAnimation anim{};
                anim.name = name;
                anim.textureKey = std::string(name) + "_sheet";
                animations.push_back(std::move(anim));
            }
            activeAnimation = 0;
        }

        /*************************************************************************************
          \brief Reload the texture for a given sprite-sheet animation from spriteSheetPath.

          If textureKey is empty, uses the animation's name as key. The resolved asset
          path is computed via ResolveAnimationPath(), then passed to Resource_Manager.
        *************************************************************************************/
        void ReloadAnimationTexture(SpriteSheetAnimation& anim) {
            if (anim.textureKey.empty())
                anim.textureKey = anim.name;
            if (anim.spriteSheetPath.empty())
                return;

            const auto resolvedPath = ResolveAnimationPath(anim.spriteSheetPath);
            if (Resource_Manager::load(anim.textureKey, resolvedPath))
                anim.textureId = Resource_Manager::getTexture(anim.textureKey);
            else
                anim.textureId = Resource_Manager::getTexture(anim.textureKey);
        }

        /*************************************************************************************
          \brief Ensure that anim.textureId is valid, loading the texture if needed.

          Tries, in order:
          - Existing textureKey (or uses anim.name if key is empty).
          - If that fails and spriteSheetPath is set, calls ReloadAnimationTexture().
        *************************************************************************************/
        void EnsureTexture(SpriteSheetAnimation& anim) {
            if (anim.textureId)
                return;

            if (anim.textureKey.empty() && !anim.name.empty())
                anim.textureKey = anim.name;

            if (!anim.textureKey.empty()) {
                anim.textureId = Resource_Manager::getTexture(anim.textureKey);
            }
            if (!anim.textureId && !anim.spriteSheetPath.empty()) {
                ReloadAnimationTexture(anim);
            }
        }

    private:
        /*************************************************************************************
          \brief Preload all legacy frame textures.

          Iterates over frames[] and resolves textures via ResolveFrameTexture(), so that
          legacy animations do not incur first-use load hitches.
        *************************************************************************************/
        void PreloadFrames() {
            for (size_t i = 0; i < frames.size(); ++i) {
                ResolveFrameTexture(i);
            }
        }

        /*************************************************************************************
          \brief Normalize animation paths to the packaged assets directory.

          Rules:
          - If rawPath is empty, return as-is.
          - Normalize backslashes to forward slashes.
          - If the resulting path is absolute, return it directly.
          - If it contains the "assets/" prefix, strip it and resolve relative to
            the engine's asset root via Framework::ResolveAssetPath().
        *************************************************************************************/
        static std::string ResolveAnimationPath(const std::string& rawPath) {
            if (rawPath.empty())
                return rawPath;

            std::string normalized = rawPath;
            std::replace(normalized.begin(), normalized.end(), '\\', '/');

            std::filesystem::path asPath{ normalized };
            if (asPath.is_absolute())
                return asPath.string();

            constexpr std::string_view kPrefix = "assets/";
            const auto pos = normalized.find(kPrefix);
            if (pos != std::string::npos)
                asPath = normalized.substr(pos + kPrefix.size());

            return Framework::ResolveAssetPath(asPath).string();
        }

        size_t currentFrame{ 0 }; ///< Current index in the legacy frames[] animation.
        float  accumulator{ 0.0f }; ///< Time accumulator for legacy frame stepping.
    };

} // namespace Framework
