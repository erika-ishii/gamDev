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
    struct SoundInfo
    {
        std::string id;
        bool loop{ false };
    };

    class AudioComponent : public GameComponent
    {
        public:
        std::unordered_map<std::string, SoundInfo> sounds;
        std::unordered_map<std::string, bool> playing;
        float volume{ 1.0f }; 
        
        
        AudioComponent()
        {
            sounds["footsteps"] = { "", false };
            sounds["Slash1"] = { "", false };
            sounds["GrappleShoot1"] = { "", false };
            playing["footsteps"] = false;
            playing["Slash1"] = false;
            playing["GrappleShoot1"] = false;
        };

        void initialize() override {}
    
        void Play(const std::string& action)
        {
            auto it = sounds.find(action);
            if (it != sounds.end() && SoundManager::getInstance().isSoundLoaded(it->second.id))
            {SoundManager::getInstance().playSound(it->second.id, volume, 1.0f, it->second.loop); playing[action] = true;}
            else
            {std::cout << "[AudioComponent] ERROR: Action '" << action<< "' not found in sounds map!\n";}
        }
        
        void Stop(const std::string& action)
        {
            auto it = sounds.find(action);
            if (it != sounds.end())
            {
                SoundManager::getInstance().stopSound(it->second.id);
                playing[action] = false;
            }
        }

        void TriggerSound(const std::string& action)
        {
            auto it = sounds.find(action);
            if (it != sounds.end()) { SoundManager::getInstance().playSound(it->second.id, volume, 1.f, it->second.loop);}
        }

        void Serialize(ISerializer& s) override
        {
            if (s.EnterObject("sounds"))
            {
                for (auto& [action, info] : sounds)
                {
                    if (s.EnterObject(action))
                    {
                        StreamRead(s, "id", info.id);
                        int loopInt = info.loop ? 1 : 0;
                        StreamRead(s, "loop", loopInt);
                        info.loop = (loopInt != 0);
                        s.ExitObject();
                    }
                }
                s.ExitObject();
            }
            if (s.HasKey("volume")) StreamRead(s, "volume", volume);
        }

        std::unique_ptr<GameComponent> Clone() const override
        {
            auto copy = std::make_unique<AudioComponent>();
            copy->sounds = sounds;
            copy->volume = volume;
            copy->playing = playing;
            return copy;
        }

        void Update(float dt) 
        {(void)dt;}
        ~AudioComponent() override 
        {
            for (auto& [action, isPlaying] : playing)
            {
                if (isPlaying)
                {
                    auto it = sounds.find(action);
                    if (it != sounds.end())
                    {
                        SoundManager::getInstance().stopSound(it->second.id);
                    }
                }
            }
        }
    };
}