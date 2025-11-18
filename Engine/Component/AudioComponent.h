#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Audio/SoundManager.h"
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
namespace Framework
{
    class AudioComponent : public GameComponent
    {
        public:
        std::string soundID; 
        float volume{ 1.0f }; 
        bool loop{ false };   
        bool playing{ false }; 
        
        AudioComponent() = default;
        AudioComponent(const std::string& id, float vol, bool lp)
        : soundID(id), volume(vol), loop(lp) {}
        void initialize() override {}
    
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("soundID")) StreamRead(s, "soundID", soundID);
            if (s.HasKey("volume"))  StreamRead(s, "volume", volume);
            if (s.HasKey("loop"))
            {
                int loopInt = loop ? 1 : 0;
                StreamRead(s, "loop", loopInt);
                loop = (loopInt != 0);
            }

            if (s.HasKey("playing"))
            {
                int playingInt = playing ? 1 : 0;
                StreamRead(s, "playing", playingInt);
                playing = (playingInt != 0);
            }
        }

        std::unique_ptr<GameComponent> Clone() const override
        {
            auto copy = std::make_unique<AudioComponent>();
            copy->soundID = soundID;
            copy->volume = volume;
            copy->loop = loop;
            copy->playing = playing;
            return copy;
        }

        void Update(float dt) 
        {
            (void)dt;
            if (playing && SoundManager::getInstance().isSoundLoaded(soundID))
            { SoundManager::getInstance().playSound(soundID, volume, 1.0f, loop); playing = false;}
        }
    };
}