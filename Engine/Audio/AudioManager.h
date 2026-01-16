/*********************************************************************************************
 \file      AudioManager.h
 \par       SofaSpuds
 \author    Ho Jun(h.jun@digipen.edu) - Primary Author, 50%
            jianwei.c (jianwei.c@digipen.edu) - Secondary Author, 50%

 \brief     Declaration of the AudioManager class, which manages audio playback using the FMOD 
            audio library. This includes loading, playing, pausing, stopping, and unloading 
            sounds, as well as volume and pitch control.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore. 
            All rights reserved.
**********************************************************************************************/
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "fmod.h"

struct FadeData
{
    FMOD_CHANNEL* channel;
    float startVolume;
    float endVolume;
    float duration;
    float elapsed;
};
/*********************************************************************************************
  \class AudioManager
  \brief Manages the initialization, loading, playback, and cleanup of audio using FMOD.
  
  The AudioManager provides functions to control sounds including volume, pitch, pausing,
  and stopping playback. It also tracks currently loaded sounds and manages channels 
  associated with them.
**********************************************************************************************/
class AudioManager 
{
    public:
    AudioManager();
    ~AudioManager();
    bool initialize();
    void shutdown();
    void update(float dt);
    bool loadSound(const std::string& name, const std::string& filePath, bool loop = false);
    void unloadSound(const std::string& name);
    void unloadAllSounds();
    bool playSound(const std::string& name, float volume = 1.0f, float pitch = 1.0f, bool loop= false);
    void stopSound(const std::string& name);
    void stopAllSounds();
    void pauseSound(const std::string& name, bool pause = true);
    void pauseAllSounds(bool pause = true);
    void setMasterVolume(float volume);
    void setSoundVolume(const std::string& name, float volume);
    void setSoundPitch(const std::string& name, float pitch);
    void setSoundLoop(const std::string& name, bool loop);
    bool isSoundLoaded(const std::string& name) const;
    bool isSoundPlaying(const std::string& name) const;
    void fadeInSound(const std::string& name, float duration, float targetVolume = 1.0f);
    void fadeOutSound(const std::string& name, float duration);
    std::vector<std::string> getLoadedSounds() const;
    private:
    FMOD_SYSTEM* m_system;///Pointer to the FMOD system instance.
    std::unordered_map<std::string, FMOD_SOUND*> m_sounds;///Map of loaded sounds by name.
    std::unordered_map<std::string, std::vector<FMOD_CHANNEL*>> m_channels;///Map of channels for each sound.
    std::vector<FadeData> m_fades;
    std::string getFullPath(const std::string& fileName) const;
    void updateFades(float deltaTime);
    void checkFMODError(FMOD_RESULT result, const std::string& operation) const;
    void pruneStoppedChannels();
};