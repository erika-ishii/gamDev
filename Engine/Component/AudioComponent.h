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
        std::unordered_map<std::string, std::string> sounds;
        float volume{ 1.0f }; 
        bool loop{ false };   
        bool playing{ false }; 
        
        AudioComponent()
        {
            sounds["footsteps"] = "";
            sounds["Slash1"] = "";
            sounds["GrappleShoot1"] = "";
        };

        void initialize() override {}
    
        void Play(const std::string& action)
        {
            auto it = sounds.find(action);
            if (it != sounds.end() && SoundManager::getInstance().isSoundLoaded(it->second))
            {SoundManager::getInstance().playSound(it->second, volume, 1.0f, loop); playing = true;}
            else
            {std::cout << "[AudioComponent] ERROR: Action '" << action<< "' not found in sounds map!\n";}
        }
        
        void TriggerSound(const std::string& action)
        {if (sounds.count(action)) { SoundManager::getInstance().playSound(sounds[action], volume, 1.f, loop); }}

        void Serialize(ISerializer& s) override
        {
            if (s.EnterObject("sounds"))
            {
                for (auto& [action, id] : sounds)
                {
                    std::string val = id;
                    StreamRead(s, action, val);
                    id = val;
                }
                s.ExitObject();
            }
            if (s.HasKey("volume")) StreamRead(s, "volume", volume);
            if (s.HasKey("loop"))
            {
                int loopInt = loop ? 1 : 0;
                StreamRead(s, "loop", loopInt);
                loop = (loopInt != 0);
            }
        }

        std::unique_ptr<GameComponent> Clone() const override
        {
            auto copy = std::make_unique<AudioComponent>();
            copy->sounds = sounds;
            copy->volume = volume;
            copy->loop = loop;
            copy->playing = playing;
            return copy;
        }

        void Update(float dt) 
        {(void)dt;}
    };
}