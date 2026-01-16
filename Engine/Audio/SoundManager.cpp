/*********************************************************************************************
 \file      SoundManager.cpp
 \par       SofaSpuds
 \author    Ho Jun(h.jun@digipen.edu) - Primary Author, 50%
            jianwei.c (jianwei.c@digipen.edu) - Secondary Author, 50%

 \brief     Implementation of the SoundManager singleton class. This class provides a global
            access point for managing audio in the game by delegating operations to the
            AudioManager. It handles initialization, updates, cleanup, as well as loading,
            playing, pausing, stopping, and unloading sounds.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "SoundManager.h"
#include <iostream>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW        // <- redefine new AFTER all includes
#endif

/*****************************************************************************************
 \brief Get the instance of the SoundManager.
 \return Reference to the single SoundManager instance.
*****************************************************************************************/
SoundManager& SoundManager::getInstance()
{
    static SoundManager instance;
    return instance;
}

/*****************************************************************************************
 \brief Initializes the underlying AudioManager system.
 \return True if initialization succeeded, false otherwise.
*****************************************************************************************/
bool SoundManager::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create and initialize AudioManager
    auto audio = std::make_shared<AudioManager>();

    bool success = audio->initialize();
    if (success)
    {
        m_audioManager = std::move(audio);
        std::cout << "SoundManager initialized successfully" << std::endl;
    }
    else
    {
        std::cerr << "Failed to initialize SoundManager" << std::endl;
    }

    return success;
}

/*****************************************************************************************
 \brief Shuts down the AudioManager and releases all resources.
*****************************************************************************************/
void SoundManager::shutdown()
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = std::move(m_audioManager);
        // m_audioManager is now nullptr for other threads
    }

    if (local)
    {
        local->shutdown();
        // local will go out of scope and be destroyed here if no other copies exist
        std::cout << "SoundManager shutdown complete" << std::endl;
    }
}

/*****************************************************************************************
 \brief Update the AudioManager. This is called once per frame.
*****************************************************************************************/
void SoundManager::update(float dt)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->update(dt);
    }
}

/*****************************************************************************************
 \brief Load a sound into memory.
 \param name The identifies the sound.
 \param filePath Path to the audio file.
 \param loop Whether the sound should loop when played.
 \return True if the sound was successfully loaded.
*****************************************************************************************/
bool SoundManager::loadSound(const std::string& name, const std::string& filePath, bool loop)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (!local)
    {
        std::cerr << "SoundManager not initialized" << std::endl;
        return false;
    }

    return local->loadSound(name, filePath, loop);
}

/*****************************************************************************************
 \brief Unload a specific sound by it's identifier.
 \param name To identifier the sound to unload.
*****************************************************************************************/
void SoundManager::unloadSound(const std::string& name)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->unloadSound(name);
    }
}

/*****************************************************************************************
 \brief Unloads all the sound that has been loaded.
*****************************************************************************************/
void SoundManager::unloadAllSounds()
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->unloadAllSounds();
    }
}

/*****************************************************************************************
 \brief Plays a loaded sound.
 \param name The identifies the sound.
 \param volume Playback volume (default is 1.0f).
 \param pitch Playback pitch (default is 1.0f).
 \return True if the sound was successfully played.
*****************************************************************************************/
bool SoundManager::playSound(const std::string& name, float volume, float pitch, bool loop)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (!local)
    {
        std::cerr << "SoundManager not initialized" << std::endl;
        return false;
    }

    return local->playSound(name, volume, pitch, loop);
}

/*****************************************************************************************
 \brief Stops the playback of a specific sound.
 \param name The identifies the sound to stop.
*****************************************************************************************/
void SoundManager::stopSound(const std::string& name)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->stopSound(name);
    }
}

/*****************************************************************************************
 \brief Stops all currently playing sounds.
*****************************************************************************************/
void SoundManager::stopAllSounds()
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->stopAllSounds();
    }
}

/*****************************************************************************************
 \brief Pauses or resumes a specific sound.
 \param name The identifies the sound.
 \param pause True to pause, false to resume.
*****************************************************************************************/
void SoundManager::pauseSound(const std::string& name, bool pause)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->pauseSound(name, pause);
    }
}

/*****************************************************************************************
 \brief Pauses or resumes all currently playing sounds.
 \param pause True to pause, false to resume.
*****************************************************************************************/
void SoundManager::pauseAllSounds(bool pause)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->pauseAllSounds(pause);
    }
}

/*****************************************************************************************
 \brief Sets the global master volume.
 \param volume The new master volume level.
*****************************************************************************************/
void SoundManager::setMasterVolume(float volume)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->setMasterVolume(volume);
    }
}

/*****************************************************************************************
 \brief Sets the volume for all active instances of a specific sound.
 \param name The identifier for the sound.
 \param volume The new volume level.
*****************************************************************************************/
void SoundManager::setSoundVolume(const std::string& name, float volume)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->setSoundVolume(name, volume);
    }
}

/*****************************************************************************************
 \brief Sets the pitch for all active instances of a specific sound.
 \param name The identifies the sound.
 \param pitch The new pitch level.
*****************************************************************************************/
void SoundManager::setSoundPitch(const std::string& name, float pitch)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->setSoundPitch(name, pitch);
    }
}

/*****************************************************************************************
    \brief Sets the looping state of a loaded sound.
    \param name  The unique identifier of the sound.
    \param loop  True to enable looping, false to disable looping.
    \note If the sound is not loaded, the function does nothing.
*****************************************************************************************/
void SoundManager::setSoundLoop(const std::string& name, bool loop)
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (local)
    {
        local->setSoundLoop(name, loop);
    }
}

/*****************************************************************************************
 \brief Checks whether a sound is currently loaded.
 \param name The identifier for the sound.
 \return True if the sound is loaded, false otherwise.
*****************************************************************************************/
bool SoundManager::isSoundLoaded(const std::string& name) const
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (!local)
    {
        return false;
    }

    return local->isSoundLoaded(name);
}

/*****************************************************************************************
 \brief Checks whether a sound is currently playing.
 \param name The identifier for the sound.
 \return True if the sound is playing, false otherwise.
*****************************************************************************************/
bool SoundManager::isSoundPlaying(const std::string& name) const
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (!local)
    {
        return false;
    }

    return local->isSoundPlaying(name);
}
/*****************************************************************************************
 \brief Fades in a currently playing sound over a specified duration.
 \param name         The identifier of the sound to fade in.
 \param duration     Duration of the fade in seconds.
 \param targetVolume The final volume level after the fade (default is 1.0f).
*****************************************************************************************/
void SoundManager::fadeInMusic(const std::string& name, float duration, float targetVolume)
{
    if (m_audioManager)
        m_audioManager->fadeInSound(name, duration, targetVolume); // call AudioManager
}
/*****************************************************************************************
 \brief Fades out a currently playing sound over a specified duration and stops it at the end.
 \param name     The identifier of the sound to fade out.
 \param duration Duration of the fade in seconds.
*****************************************************************************************/
void SoundManager::fadeOutMusic(const std::string& name, float duration)
{
    if (m_audioManager)
        m_audioManager->fadeOutSound(name, duration); // call AudioManager
}
/*****************************************************************************************
 \brief Retrieves a list of all loaded sounds.
 \return A vector containing the identifiers of loaded sounds.
*****************************************************************************************/
std::vector<std::string> SoundManager::getLoadedSounds() const
{
    std::shared_ptr<AudioManager> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local = m_audioManager;
    }

    if (!local)
    {
        return {};
    }

    return local->getLoadedSounds();
}