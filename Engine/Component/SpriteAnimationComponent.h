#pragma once

#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Resource_Manager/Resource_Manager.h"

#include <string>
#include <vector>

namespace Framework {

    struct SpriteAnimationFrame {
        std::string texture_key;
        std::string path;
    };

    class SpriteAnimationComponent : public GameComponent {
    public:
        float fps{ 6.0f };
        bool loop{ true };
        bool play{ true };

        std::vector<SpriteAnimationFrame> frames{};

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
        }

        std::unique_ptr<GameComponent> Clone() const override {
            auto copy = std::make_unique<SpriteAnimationComponent>();
            copy->fps = fps;
            copy->loop = loop;
            copy->play = play;
            copy->frames = frames;
            copy->currentFrame = currentFrame;
            copy->accumulator = accumulator;
            return copy;
        }

        void SendMessage(Message&) override {}

        bool HasFrames() const { return !frames.empty(); }

        size_t FrameCount() const { return frames.size(); }

        size_t CurrentFrameIndex() const { return currentFrame; }

        unsigned ResolveFrameTexture(size_t index) {
            if (index >= frames.size())
                return 0;

            const auto& frame = frames[index];
            unsigned tex = Resource_Manager::getTexture(frame.texture_key);
            if (!tex && !frame.path.empty()) {
                if (Resource_Manager::load(frame.texture_key, frame.path))
                    tex = Resource_Manager::getTexture(frame.texture_key);
            }
            return tex;
        }

        void SetFrame(size_t index) {
            if (index < frames.size())
                currentFrame = index;
        }

        void Advance(float dt) {
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

    private:
        void PreloadFrames() {
            for (size_t i = 0; i < frames.size(); ++i) {
                ResolveFrameTexture(i);
            }
        }

        size_t currentFrame{ 0 };
        float accumulator{ 0.0f };
    };
}