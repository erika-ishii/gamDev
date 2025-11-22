#pragma once

#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Resource_Manager/Resource_Manager.h"
#include "Core/PathUtils.h"
#include <glm/vec4.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Framework {

    struct SpriteAnimationFrame {
        std::string texture_key;
        std::string path;
    };

    class SpriteAnimationComponent : public GameComponent {
    public:
        // --- Sprite-sheet based animation config -----------------------------
        struct AnimConfig {
            int   totalFrames{ 1 };
            int   rows{ 1 };
            int   columns{ 1 };
            int   startFrame{ 0 };
            int   endFrame{ -1 };   // -1 == use totalFrames - 1
            float fps{ 6.0f };
            bool  loop{ true };
        };

        struct SpriteSheetAnimation {
            std::string name{ "idle" };
            std::string spriteSheetPath{};
            std::string textureKey{};
            AnimConfig  config{};

            int   currentFrame{ 0 };
            float accumulator{ 0.0f };
            unsigned textureId{ 0 };
        };

        // --- Old frame-array style animation (still supported) --------------
        float fps{ 6.0f };
        bool  loop{ true };
        bool  play{ true };

        std::vector<SpriteAnimationFrame> frames{};

        // --- New sprite-sheet animation set ----------------------------------
        std::vector<SpriteSheetAnimation> animations{};
        int activeAnimation{ 0 };

        // ---------------------------------------------------------------------
        // Engine lifecycle
        // ---------------------------------------------------------------------
        void initialize() override {
            PreloadFrames();
        }

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

        void SendMessage(Message&) override {}

        // ---------------------------------------------------------------------
        // Basic info helpers
        // ---------------------------------------------------------------------
        bool HasFrames() const { return !frames.empty(); }
        bool HasSpriteSheets() const { return !animations.empty(); }

        size_t FrameCount() const { return frames.size(); }
        std::size_t AnimationCount() const { return animations.size(); }

        size_t CurrentFrameIndex() const { return currentFrame; }

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

        SpriteSheetAnimation* ActiveAnimation() {
            const int idx = ActiveAnimationIndex();
            if (idx < 0)
                return nullptr;
            return &animations[static_cast<std::size_t>(idx)];
        }

        const SpriteSheetAnimation* ActiveAnimation() const {
            const int idx = ActiveAnimationIndex();
            if (idx < 0)
                return nullptr;
            return &animations[static_cast<std::size_t>(idx)];
        }

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
        // Old frame-array texture resolving
        // ---------------------------------------------------------------------
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

        void SetFrame(size_t index) {
            if (index < frames.size())
                currentFrame = index;
        }

        // ---------------------------------------------------------------------
        // Update
        // ---------------------------------------------------------------------
        void Advance(float dt) {
            AdvanceFrameArray(dt);
            AdvanceSpriteSheets(dt);
        }

        // Advance legacy frame array animation
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

        // Advance sprite-sheet animations
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
        struct SheetSample {
            unsigned    texture{ 0 };
            glm::vec4   uv{ 0.f, 0.f, 1.f, 1.f };
            std::string textureKey;
        };

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

        // Create default animation entries (idle/run/attack1/2/3)
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

        // Reload texture from spriteSheetPath into textureKey
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

        // Make sure anim.textureId is valid (load if needed)
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
        void PreloadFrames() {
            for (size_t i = 0; i < frames.size(); ++i) {
                ResolveFrameTexture(i);
            }
        }
        // Normalize animation paths to the packaged assets directory.
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

        size_t currentFrame{ 0 };
        float  accumulator{ 0.0f };
    };

} // namespace Framework
