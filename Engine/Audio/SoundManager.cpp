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
    if (!m_audioManager)
    {
        m_audioManager = std::make_unique<AudioManager>();
    }
    
    bool success = m_audioManager->initialize();
    if (success)
    {
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
    if (m_audioManager)
    {
        m_audioManager->shutdown();
        m_audioManager.reset();
        std::cout << "SoundManager shutdown complete" << std::endl;
    }
}

/*****************************************************************************************
 \brief Update the AudioManager. This is called once per frame.
*****************************************************************************************/
void SoundManager::update()
{
    if (m_audioManager)
    {
        m_audioManager->update();
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
    if (!m_audioManager)
    {
        std::cerr << "SoundManager not initialized" << std::endl;
        return false;
    }
    
    return m_audioManager->loadSound(name, filePath, loop);
}

/*****************************************************************************************
 \brief Unload a specific sound by it's identifier.
 \param name To identifier the sound to unload.
*****************************************************************************************/
void SoundManager::unloadSound(const std::string& name)
{
    if (m_audioManager)
    {
        m_audioManager->unloadSound(name);
    }
}

/*****************************************************************************************
 \brief Unloads all the sound that has been loaded.
*****************************************************************************************/
void SoundManager::unloadAllSounds()
{
    if (m_audioManager)
    {
        m_audioManager->unloadAllSounds();
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
    if (!m_audioManager)
    {
        std::cerr << "SoundManager not initialized" << std::endl;
        return false;
    }

    return m_audioManager->playSound(name, volume, pitch, loop);
}

/*****************************************************************************************
 \brief Stops the playback of a specific sound.
 \param name The identifies the sound to stop.
*****************************************************************************************/
void SoundManager::stopSound(const std::string& name)
{
    if (m_audioManager)
    {
        m_audioManager->stopSound(name);
    }
}

/*****************************************************************************************
 \brief Stops all currently playing sounds.
*****************************************************************************************/
void SoundManager::stopAllSounds()
{
    if (m_audioManager)
    {
        m_audioManager->stopAllSounds();
    }
}

/*****************************************************************************************
 \brief Pauses or resumes a specific sound.
 \param name The identifies the sound.
 \param pause True to pause, false to resume.
*****************************************************************************************/
void SoundManager::pauseSound(const std::string& name, bool pause)
{
    if (m_audioManager)
    {
        m_audioManager->pauseSound(name, pause);
    }
}

/*****************************************************************************************
 \brief Pauses or resumes all currently playing sounds.
 \param pause True to pause, false to resume.
*****************************************************************************************/
void SoundManager::pauseAllSounds(bool pause)
{
    if (m_audioManager)
    {
        m_audioManager->pauseAllSounds(pause);
    }
}

/*****************************************************************************************
 \brief Sets the global master volume.
 \param volume The new master volume level.
*****************************************************************************************/
void SoundManager::setMasterVolume(float volume)
{
    if (m_audioManager)
    {
        m_audioManager->setMasterVolume(volume);
    }
}

/*****************************************************************************************
 \brief Sets the volume for all active instances of a specific sound.
 \param name The identifier for the sound.
 \param volume The new volume level.
*****************************************************************************************/
void SoundManager::setSoundVolume(const std::string& name, float volume)
{
    if (m_audioManager)
    {
        m_audioManager->setSoundVolume(name, volume);
    }
}

/*****************************************************************************************
 \brief Sets the pitch for all active instances of a specific sound.
 \param name The identifies the sound.
 \param pitch The new pitch level.
*****************************************************************************************/
void SoundManager::setSoundPitch(const std::string& name, float pitch)
{
    if (m_audioManager)
    {
        m_audioManager->setSoundPitch(name, pitch);
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
    if (m_audioManager)
        m_audioManager->setSoundLoop(name, loop);
}
/*****************************************************************************************
 \brief Checks whether a sound is currently loaded.
 \param name The identifier for the sound.
 \return True if the sound is loaded, false otherwise.
*****************************************************************************************/
bool SoundManager::isSoundLoaded(const std::string& name) const
{
    if (!m_audioManager)
    {
        return false;
    }
    
    return m_audioManager->isSoundLoaded(name);
}

/*****************************************************************************************
 \brief Checks whether a sound is currently playing.
 \param name The identifier for the sound.
 \return True if the sound is playing, false otherwise.
*****************************************************************************************/
bool SoundManager::isSoundPlaying(const std::string& name) const
{
    if (!m_audioManager)
    {
        return false;
    }
    
    return m_audioManager->isSoundPlaying(name);
}

/*****************************************************************************************
 \brief Retrieves a list of all loaded sounds.
 \return A vector containing the identifiers of loaded sounds.
*****************************************************************************************/
std::vector<std::string> SoundManager::getLoadedSounds() const
{
    if (!m_audioManager)
    {
        return {};
    }
    
    return m_audioManager->getLoadedSounds();
}
