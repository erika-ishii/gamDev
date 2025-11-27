/*********************************************************************************************
 \file      AudioComponent.h
 \par       SofaSpuds
 \author    Choo Jian Wei – Primary Author

 \brief     Declares the AudioComponent class, which manages sound playback for game entities
            such as players, enemies, or interactable objects.

 \details
            The AudioComponent is responsible for registering, initializing, and controlling
            audio cues associated with a specific entity type. This component automatically
            configures different sound sets depending on whether the entity represents a player,
            enemy, or other object type.

            Responsibilities:
            - Store metadata about each sound (ID and loop state).
            - Track whether individual sounds are currently playing.
            - Provide methods to play one-shot, looping, or triggered sounds.
            - Interface with the SoundManager to execute audio playback logic.
            - Support serialization for prefab and level loading.
            - Support deep-copy functionality for object instancing.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
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
    /*****************************************************************************************
      \struct SoundInfo
      \brief Lightweight structure storing metadata for a specific sound.

      Contains an audio identifier and a flag indicating whether the sound should loop
      when played (e.g., footsteps).
    *****************************************************************************************/
    struct SoundInfo
    {
        std::string id;
        bool loop{ false };
    };
    /*****************************************************************************************
      \class AudioComponent
      \brief Component that manages audio playback for entities.

      This component provides a flexible audio system for any GameObject. Depending on its
      assigned entityType, it automatically loads the correct sound set on initialization.
      It also tracks playback state, supports serialization, and ensures proper cleanup when
      sounds are no longer needed.
*****************************************************************************************/
    class AudioComponent : public GameComponent
    {
        public:
        bool initialized = false;
        std::unordered_map<std::string, SoundInfo> sounds;
        std::unordered_map<std::string, bool> playing;
        float volume{ 1.0f }; 
        std::string entityType;
        
        void ensureInitialized()
        {
            if (initialized || entityType.empty())
                return;

            initialize();
            initialized = true;
        }

        /*************************************************************************************
          \brief Default constructor.
        *************************************************************************************/
        AudioComponent() = default;
        /*************************************************************************************
         \brief Initializes the component by registering sounds based on entity type.

         \details
             Clears existing maps, assigns default sounds depending on whether this object
             represents a player or enemy, and initializes the playback tracking map.
       *************************************************************************************/
        void initialize() override
        {
            sounds.clear();
            playing.clear();


            if (entityType == "player")
            {
                sounds["footsteps"] = { "footsteps", true };
                sounds["Slash1"] = { "Slash1", false };
                sounds["GrappleShoot1"] = { "GrappleShoot1", false };
                sounds["PlayerHit"] = { "PlayerHit", false };
                sounds["PlayerDead"] = { "PlayerDead", false };
            }
            else if (entityType == "enemy")
            {
                sounds["GhostSounds"] = { "GhostSounds", false };
            }

            // Build playing map
            for (auto& [action, info] : sounds)
                playing[action] = false;
        }
        /*************************************************************************************
          \brief Plays a sound associated with the given action key.

          \param action The identifier for the desired sound (e.g., "footsteps").
          \details
              Begins playback only if the sound exists and is loaded by the SoundManager.
        *************************************************************************************/
        void Play(const std::string& action)
        {
            ensureInitialized();
            auto it = sounds.find(action);
            if (it != sounds.end() && SoundManager::getInstance().isSoundLoaded(it->second.id))
            {SoundManager::getInstance().playSound(it->second.id, volume, 1.0f, it->second.loop); playing[action] = true;}
        }
        /*************************************************************************************
          \brief Stops a currently looping or active sound.

          \param action The identifier for the sound to stop.
        *************************************************************************************/
        void Stop(const std::string& action)
        {
            ensureInitialized();
            auto it = sounds.find(action);
            if (it != sounds.end())
            {
                SoundManager::getInstance().stopSound(it->second.id);
                playing[action] = false;
            }
        }
        /*************************************************************************************
          \brief Triggers a sound without modifying playback state tracking.

          \param action Name of the sound.
          \details
              Useful for one-shot events such as effects, hits, UI sounds, or ambient cues.
        *************************************************************************************/
        void TriggerSound(const std::string& action)
        {
            ensureInitialized();
            auto it = sounds.find(action);
            if (it != sounds.end()) { SoundManager::getInstance().playSound(it->second.id, volume, 1.f, it->second.loop);}
        }
        /*************************************************************************************
          \brief Serializes sound configuration and volume settings.

          \param s Reference to the serializer.
          \details
              Reads entityType, sound metadata, and volume if present in serialized data.
        *************************************************************************************/
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("entityType"))
                StreamRead(s, "entityType", entityType);

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

            if (s.HasKey("volume"))
                StreamRead(s, "volume", volume);
        }
        /*************************************************************************************
          \brief Clones this AudioComponent and its internal data.

          \return A unique_ptr containing a fully copied AudioComponent instance.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override
        {
            auto copy = std::make_unique<AudioComponent>();
            copy->sounds = sounds;
            copy->volume = volume;
            copy->playing = playing;
            return copy;
        }
        /*************************************************************************************
          \brief Frame update function for extended audio behavior.

          \details Currently unused but reserved for future audio logic.
        *************************************************************************************/
        void Update(float dt) 
        {(void)dt;}
        /*************************************************************************************
          \brief Destructor ensures all active sounds are stopped on component removal.
        *************************************************************************************/
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