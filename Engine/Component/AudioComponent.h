/*********************************************************************************************
 \file      AudioComponent.h
 \par       SofaSpuds
 \author    Choo Jian Wei - Primary Author

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
            All content  2025 DigiPen Institute of Technology Singapore.
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
        //Footsteps
        std::vector<std::string> footstepClips;
        std::string currentFootstep;
        bool isFootstepPlaying = false;
        //Slashes Sound
        std::vector<std::string> slashClips;//Slashing Enemy
        std::vector<std::string> punchClips;//Slashing Air
        std::vector<std::string> ineffectiveClips;//Ineffective Slashes
        //Grapple
        std::vector<std::string> grappleClips;
        //Enemy Sounds
        std::vector<std::string> attackClips;
        std::vector<std::string> hurtClips;
        std::vector<std::string> deathClips;

        float volume{ 1.0f }; 
        std::string entityType;
        
        /*************************************************************************************
          \brief Ensures that the AudioComponent is initialized.

          \details
              This function checks whether the AudioComponent has been initialized and
              whether the entityType has been set. If the component is not yet initialized
              and the entityType is valid, it calls initialize() to register the correct
              sounds for this entity. This guarantees that all sound mappings are ready
              for playback.

              In the context of prefabs, this can be called after cloning to ensure that
              the AudioComponent correctly registers sounds even if the prefab was partially
              initialized or serialized previously.

          \param force Optional boolean flag (default = false). If true, forces re-initialization
                       even if the component was already initialized. Useful for prefab cloning
                       scenarios.

          \note If entityType is empty, initialization is skipped and a warning is logged.
        *************************************************************************************/
        void ensureInitialized(bool force = false)
        {
            if (!force && initialized && !entityType.empty())
                return;
            if (entityType.empty()) {
                std::cerr << "[AudioComponent] Warning: entityType not set, cannot initialize.\n";
                return;
            }
            initialized = false; // reset just in case
            sounds.clear();
            playing.clear();
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
            footstepClips.clear();
            if (entityType == "player")
            {
                for (int i = 1; i <= 6; i++)
                {
                    std::string clip = "ConcreteFootsteps" + std::to_string(i);
                    sounds[clip] = { clip, false };
                    playing[clip] = false;
                    footstepClips.push_back(clip);
                }
                //Slashes on Enemy
                for (int i = 1; i <= 3; i++)
                {
                    sounds["Slash" + std::to_string(i)] = { "Slash" + std::to_string(i), false };
                    slashClips.push_back("Slash" + std::to_string(i));
                }
                //Slashes in Air
                for (int i = 1; i <= 4; i++)
                {
                    sounds["Punch" + std::to_string(i)] = { "Punch" + std::to_string(i), false };
                    punchClips.push_back("Punch" + std::to_string(i));
                }
                
                //Ineffective Slashes
                for (int i = 1; i <= 3; i++)
                {
                    sounds["Ineffective Boink" + std::to_string(i)] = { "Ineffective Boink" + std::to_string(i), false };
                    ineffectiveClips.push_back("Ineffective Boink" + std::to_string(i));
                }

                for (int i = 1; i <= 4; i++)
                {
                    sounds["GrappleShoot" + std::to_string(i)] = { "GrappleShoot" + std::to_string(i), false };
                    grappleClips.push_back("GrappleShoot" + std::to_string(i));
                }
                sounds["PlayerHit"] = { "PlayerHit", false };
                sounds["PlayerDead"] = { "PlayerDead", false };
            }
            else if (entityType == "enemy_fire")
            {
                // Projectile variants
                for (int i = 1; i <= 2; i++)
                {
                    std::string id = "FireGhostProjectile" + std::to_string(i);
                    sounds[id] = { id, false };
                    attackClips.push_back(id);
                }

                // Hurt variants
                for (int i = 1; i <= 8; i++)
                {
                    std::string id = "GhostHurt" + std::to_string(i);
                    sounds[id] = { id, false };
                    hurtClips.push_back(id);
                }

                sounds["FireGhostExplosion"] = { "FireGhostExplosion", false };
                deathClips.push_back("FireGhostExplosion");
            }
            else if (entityType == "enemy_water")
            {
                // Water ghost attack (1 only)
                std::string atk = "WaterGhostAttack";
                sounds[atk] = { atk, false };
                attackClips.push_back(atk);

                // Shared GhostHurt1–8
                for (int i = 1; i <= 8; i++)
                {
                    std::string id = "GhostHurt" + std::to_string(i);
                    sounds[id] = { id, false };
                    hurtClips.push_back(id);
                }

                // Water ghost death — only 1 clip
                sounds["WaterGhostExplosion"] = { "WaterGhostExplosion", false };
                deathClips.push_back("WaterGhostExplosion");
            }

            // Build playing map
            for (auto& [action, info] : sounds)
                playing[action] = false;
            std::cout << "[AudioComponent] initialize called, entityType='" << entityType << "'\n";
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
        void TriggerSound(const std::string& name)
        {
            ensureInitialized();
            std::string clipToPlay = name;

            if (name == "Slash")
                clipToPlay = GetRandomFrom(slashClips);
            else if (name == "Punch")
                clipToPlay = GetRandomFrom(punchClips);
            else if (name == "Ineffective")
                clipToPlay = GetRandomFrom(ineffectiveClips);
            else if (name == "GrappleShoot")
                clipToPlay = GetRandomFrom(grappleClips);

            // Enemy groups
            else if (name == "EnemyAttack")
                clipToPlay = GetRandomFrom(attackClips);
            else if (name == "EnemyHit")
                clipToPlay = GetRandomFrom(hurtClips);
            else if (name == "EnemyDeath")
                clipToPlay = GetRandomFrom(deathClips);

            Play(clipToPlay);
        }

        std::string GetRandomFrom(const std::vector<std::string>& list)
        {
            if (list.empty()) return "";
            int index = rand() % list.size();
            return list[index];
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
            if (!entityType.empty() && !initialized)
            {
                std::cout << "[AudioComponent] Auto-initializing after Serialize with entityType: "
                    << entityType << "\n";
                ensureInitialized(true);
            }
        }
        /*************************************************************************************
          \brief Clones this AudioComponent and its internal data.

          \return A unique_ptr containing a fully copied AudioComponent instance.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override
        {
            auto copy = std::make_unique<AudioComponent>();
            copy->entityType = entityType;
            copy->sounds = sounds;
            copy->volume = volume;
            copy->playing = playing;
            if (!copy->entityType.empty())
            {
                copy->ensureInitialized(true);  // Force init
            }
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