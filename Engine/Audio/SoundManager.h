/*********************************************************************************************
 \file      SoundManager.h
 \par       SofaSpuds
 \author    Ho Jun(h.jun@digipen.edu) - Primary Author, 50%
            jianwei.c (jianwei.c@digipen.edu) - Secondary Author, 50%


 \brief     Declaration of the SoundManager class, which acts as a wrapper around
            the AudioManager to provide global sound control for the application. The
            SoundManager ensures only one instance of the AudioManager is used and simplifies
            access to sound loading, playback, pausing, stopping, and cleanup.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
**********************************************************************************************/

#pragma once
#include "AudioManager.h"
#include <memory>
#include <mutex>
#include <vector>
#include <string>

/*********************************************************************************************
 \class SoundManager
 \brief A wrapper around AudioManager for centralized sound management.

 The SoundManager ensures only one instance of AudioManager exists. It provides an easy-to-
 access global interface for loading, playing, pausing, stopping, and unloading sounds, as
 well as controlling volume and pitch. It also forwards utility functions to check sound
 states and retrieve loaded sounds.
**********************************************************************************************/
class SoundManager
{
public:
    static SoundManager& getInstance();
    bool initialize();
    void shutdown();
    void update();
    bool loadSound(const std::string& name, const std::string& filePath, bool loop = false);
    void unloadSound(const std::string& name);
    void unloadAllSounds();
    bool playSound(const std::string& name, float volume = 1.0f, float pitch = 1.0f, bool loop = false);
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
    std::vector<std::string> getLoadedSounds() const;

private:
    // Private constructor
    SoundManager() = default;

    // Private destructor
    ~SoundManager() = default;

    // Deletion of copy operations
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    /// Underlying AudioManager instance.
    /// Changed to shared_ptr to allow local thread-safe copying.
    std::shared_ptr<AudioManager> m_audioManager;

    /// Mutex to protect access to m_audioManager during init/shutdown.
    mutable std::mutex m_mutex;
};