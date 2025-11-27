/*********************************************************************************************
 \file      AudioManager.cpp
 \par       SofaSpuds
 \author    Ho Jun(h.jun@digipen.edu) - Primary Author, 50%
            jianwei.c (jianwei.c@digipen.edu) - Secondary Author, 50%

 \brief     Implementation of the AudioManager class. It provides sound loading, playback, 
            pausing, stopping, and cleanup using the FMOD audio library. This file contains 
            all function definitions declared in AudioManager.h.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
**********************************************************************************************/
#include "AudioManager.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include "fmod.h"
#include "fmod_errors.h"
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
/*****************************************************************************************
 \brief Constructor for the AudioManager.
*****************************************************************************************/
AudioManager::AudioManager() : m_system(nullptr) {}

/*****************************************************************************************
 \brief Destructor for the AudioManager. It cleans up any FMOD resources.
*****************************************************************************************/
AudioManager::~AudioManager() {shutdown();}

/*****************************************************************************************
 \brief Initializes the FMOD system for audio playback.
 \return True if initialization succeeds, false otherwise.
*****************************************************************************************/
bool AudioManager::initialize()
{
    // Creates the FMOD System
    FMOD_RESULT result = FMOD_System_Create(&m_system, FMOD_VERSION);
    if (result != FMOD_OK) 
    {
    std::cerr << "Failed to create FMOD system: " << FMOD_ErrorString(result) <<
    std::endl;
    return false;
    }

    // Initializes the FMOD system
    result = FMOD_System_Init(m_system, 32, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK) 
    {
    std::cerr << "Failed to initialize FMOD system: " << FMOD_ErrorString(result) << std::endl;
    return false;
    }

    std::cout << "AudioManager initialized successfully" << std::endl;
    return true;
}

/*****************************************************************************************
 \brief Shuts down the FMOD system and releases all associated resources.
*****************************************************************************************/
void AudioManager::shutdown() 
{
    if (m_system) 
    {
        pruneStoppedChannels();
        stopAllSounds();       // stop active channels
        unloadAllSounds();     // release FMOD sounds
        // Close and release FMOD system
        FMOD_System_Close(m_system);
        FMOD_System_Release(m_system);

        m_system = nullptr;
        std::cout << "AudioManager shutdown complete" << std::endl;
    }
}

/*****************************************************************************************
 \brief Updates the FMOD system. 
        This is called once per frame in the main game loop.
*****************************************************************************************/
void AudioManager::update() 
{
    if (m_system) 
    {
        pruneStoppedChannels();
        FMOD_System_Update(m_system);
    }
}

/*****************************************************************************************
 \brief Loads a sound from a given file path.
 \param name      The name in order to identify the sound.
 \param filePath  The path to the audio file.
 \param loop      Checks whether the sound should loop during playback.
 \return True if the sound is loaded successfully, false otherwise.
*****************************************************************************************/
bool AudioManager::loadSound(const std::string& name, const std::string& filePath, bool loop) 
{
    if (!m_system) 
    {
        std::cerr << "AudioManager not initialized" << std::endl; return false;
    }

    // Check if sound is already loaded
    if (m_sounds.find(name) != m_sounds.end()) 
    { 
        std::cout << "Sound '" << name << "' is already loaded" << std::endl; return true;
    }

    std::string fullPath = getFullPath(filePath);
    // Check if file exists
    if (!std::filesystem::exists(fullPath)) 
    {
        std::cerr << "Audio file not found: " << fullPath << std::endl; return false;
    }
    FMOD_SOUND* sound = nullptr;
    FMOD_MODE mode = FMOD_DEFAULT;
    if (loop) 
    {
        mode |= FMOD_LOOP_NORMAL;
    }

    FMOD_RESULT result = FMOD_System_CreateSound(m_system, fullPath.c_str(), mode, nullptr,&sound);
    if (result != FMOD_OK) 
    {
        std::cerr << "Failed to load sound '" << name << "': " << FMOD_ErrorString(result)<< std::endl;
        return false;
    }
    
    m_sounds[name] = sound;
    
    std::cout << "Loaded sound: " << name << " from " << fullPath << std::endl;
    return true;
}

/*****************************************************************************************
 \brief Unloads a specific sound by name.
 \param name  To identify the sound to unload.
*****************************************************************************************/
void AudioManager::unloadSound(const std::string& name)
{
    auto it = m_sounds.find(name);
    if (it!= m_sounds.end()) 
    { 
        FMOD_Sound_Release(it->second);
        m_sounds.erase(it);
        std::cout << "Unloaded sound: " << name << std::endl;
    }
    else 
    {
        std::cerr << "Sound:"<<name << "is not found in Audio Manager\n";
    }
}
/*****************************************************************************************
 \brief Unloads all currently loaded sounds.
*****************************************************************************************/
void AudioManager::unloadAllSounds()
{
    for (auto& [name,sound]: m_sounds)
    {
        FMOD_Sound_Release(sound);
        std::cout << "Unloaded sound: "<< name<< std::endl;
    }
    m_sounds.clear();
}

/*****************************************************************************************
 \brief Plays a sound by name.
 \param name      To identify the sound to play.
 \param volume    Playback volume (default by 1.0f).
 \param pitch     Playback pitch (default by 1.0f).
 \return True if the sound is played successfully, false otherwise.
*****************************************************************************************/
bool AudioManager::playSound(const std::string& name, float volume, float pitch, bool loop)
{
    if (!m_system)
    {return false;}
    pruneStoppedChannels();
        
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) 
    {
        std::cerr << "Sound '" << name << "' not loaded" << std::endl;
        return false;
    }

    FMOD_CHANNEL* channel = nullptr;
    FMOD_RESULT result = FMOD_System_PlaySound(m_system, it->second, nullptr, false, &channel);
    if (result != FMOD_OK) 
    {
        std::cerr << "Failed to play sound '" << name << "': " << FMOD_ErrorString(result) << std::endl;
        return false;
    }
    // Set looping mode dynamically per channel
    if (loop) FMOD_Channel_SetMode(channel, FMOD_LOOP_NORMAL);
    else FMOD_Channel_SetMode(channel, FMOD_LOOP_OFF);

    FMOD_Channel_SetVolume(channel, volume);
    FMOD_Channel_SetPitch(channel, pitch);

    // store this channel
    m_channels[name].push_back(channel);

    std::cout << "Playing sound: " << name << (loop ? " [looping]" : "") << std::endl;
    return true;
}

/*****************************************************************************************
 \brief Stops playback of a specific sound.
 \param name  Identifies the sound to stop.
*****************************************************************************************/
void AudioManager::stopSound(const std::string& name)
{
    pruneStoppedChannels();
    auto it = m_channels.find(name);
    if (it == m_channels.end()) 
        return;

    for (auto* ch : it->second) 
    {
        if (ch) FMOD_Channel_Stop(ch);
    }

    m_channels.erase(it);
    std::cout << "Stopped all instances of: " << name << std::endl;
}

/*****************************************************************************************
 \brief Stops the playback of all sounds currently being played.
*****************************************************************************************/
void AudioManager::stopAllSounds()
{
    pruneStoppedChannels();
    for (auto& [name, channels] : m_channels) 
    {
        for (auto* ch : channels) 
        {
            if (ch) FMOD_Channel_Stop(ch);
        }
    }
    m_channels.clear();
    std::cout << "Stopped all sounds" << std::endl;
}

/*****************************************************************************************
    \brief Pauses or unpauses a specific sound.
    \param name   Identifies the sound to pause.
    \param pause  True to pause, false to resume.
*****************************************************************************************/
void AudioManager::pauseSound(const std::string& name, bool pause)
{
    pruneStoppedChannels();
    auto it = m_channels.find(name);
    if (it == m_channels.end()) 
    {
        std::cerr << "No active channels for sound '" << name << "'" << std::endl;
        return;
    }

    for (FMOD_CHANNEL* channel : it->second) 
    {
        FMOD_Channel_SetPaused(channel, pause);
    }

    std::cout << (pause ? "Paused" : "Resumed") << " all instances of sound: " << name << std::endl;
}

/*****************************************************************************************
 \brief Pauses or unpauses all sounds.
 \param pause  True to pause, false to resume.
*****************************************************************************************/
void AudioManager::pauseAllSounds(bool pause)
{
    pruneStoppedChannels();
    for (auto& [name, channels] : m_channels) 
    {
        for (FMOD_CHANNEL* channel : channels) 
        {
            FMOD_Channel_SetPaused(channel, pause);
        }
        std::cout << (pause ? "Paused" : "Resumed") << " all instances of sound: " << name << std::endl;
    }
}

/*****************************************************************************************
    \brief Sets the master volume for all sounds.
    \param volume  New master volume level.
*****************************************************************************************/
void AudioManager::setMasterVolume(float volume) 
{
    if (m_system) 
    {
        FMOD_CHANNELGROUP* masterGroup = nullptr;
        FMOD_RESULT result = FMOD_System_GetMasterChannelGroup(m_system, &masterGroup);
        if (result == FMOD_OK && masterGroup) 
        {
            FMOD_ChannelGroup_SetVolume(masterGroup, volume);std::cout << "Set master volume to: " << volume << std::endl;
        } 
        else 
        {
            std::cerr << "Failed to get master channel group: " << FMOD_ErrorString(result) << std::endl;
        }
    }
}

/*****************************************************************************************
 \brief Sets the volume of a specific sound.
 \param name    Identifies the sound.
 \param volume  New volume level.
*****************************************************************************************/
void AudioManager::setSoundVolume(const std::string& name, float volume)
{
    pruneStoppedChannels();
    auto it = m_channels.find(name);
    if (it == m_channels.end()) 
    {
        std::cerr << "No active channels for sound '" << name << "'" << std::endl;
        return;
    }

    for (FMOD_CHANNEL* channel : it->second) 
    {
        FMOD_Channel_SetVolume(channel, volume);
    }

    std::cout << "Set volume of all instances of '" << name << "' to " << volume << std::endl;
}

/*****************************************************************************************
    \brief Sets the pitch of a specific sound.
    \param name   Identifies the sound.
    \param pitch  New pitch value.
*****************************************************************************************/
void AudioManager::setSoundPitch(const std::string& name, float pitch)
{
    pruneStoppedChannels();
    auto it = m_channels.find(name);
    if (it == m_channels.end()) 
    {
        std::cerr << "No active channels for sound '" << name << "'" << std::endl;
        return;
    }

    for (FMOD_CHANNEL* channel : it->second) 
    {
        FMOD_Channel_SetPitch(channel, pitch);
    }

    std::cout << "Set pitch of all instances of '" << name << "' to " << pitch << std::endl;
}
/*****************************************************************************************
    \brief Sets the looping state of a loaded sound.
    \param name  The unique identifier of the sound.
    \param loop  True to enable looping, false to disable looping.
    \note If the sound is not loaded, the function does nothing.
*****************************************************************************************/
void AudioManager::setSoundLoop(const std::string& name, bool loop)
{
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) { return;}
    FMOD_MODE mode;
    FMOD_Sound_GetMode(it->second, &mode);
    if (loop) mode |= FMOD_LOOP_NORMAL;
    else mode &= ~FMOD_LOOP_NORMAL;
    FMOD_Sound_SetMode(it->second, mode);
}
/*****************************************************************************************
    \brief Checks if a sound is loaded.
    \param name  Identifies the sound.
    \return True if the sound is being played, false otherwise.
*****************************************************************************************/
bool AudioManager::isSoundLoaded(const std::string& name) const
{
    return m_sounds.find(name) != m_sounds.end();
}

/*****************************************************************************************
    \brief Checks if a sound is currently playing.
    \param name  Identifies the sound.
    \return True if the sound is playing, false otherwise.
*****************************************************************************************/
bool AudioManager::isSoundPlaying(const std::string& name) const
{
    auto it = m_channels.find(name);
    if (it == m_channels.end()) 
        return false;

    for (FMOD_CHANNEL* channel : it->second) 
    {
        if (!channel) 
            continue;
        FMOD_BOOL playing = false;
        FMOD_Channel_IsPlaying(channel, &playing);
        if (playing) 
            return true;
    }

    return false;
}
/*****************************************************************************************
 \brief Remove any channels that have stopped playing or became invalid.
*****************************************************************************************/
void AudioManager::pruneStoppedChannels()
{
    for (auto it = m_channels.begin(); it != m_channels.end(); )
    {
        auto& channels = it->second;

        channels.erase(std::remove_if(channels.begin(), channels.end(), [](FMOD_CHANNEL* ch)
            {
                if (!ch)
                    return true;

                FMOD_BOOL isPlaying = 0;
                const FMOD_RESULT res = FMOD_Channel_IsPlaying(ch, &isPlaying);
                return res != FMOD_OK || isPlaying == 0;
            }), channels.end());

        if (channels.empty())
            it = m_channels.erase(it);
        else
            ++it;
    }
}
/*****************************************************************************************
 \brief Retrieves a list of all the loaded sounds.
 \return Vector containing identifiers of the loaded sounds.
*****************************************************************************************/
std::vector<std::string> AudioManager::getLoadedSounds() const
{
    std::vector<std::string> sounds;
    for (const auto& [name, sound] : m_sounds) 
    {
        sounds.push_back(name);
    }
    return sounds;
}

/*****************************************************************************************
 \brief Constructs the full file path for a given file.
 \param fileName  Name of the file.
 \return Full path string.
*****************************************************************************************/
std::string AudioManager::getFullPath(const std::string& fileName) const 
{
    // Try to find the audio file in the game-assets directory
    std::filesystem::path currentPath = std::filesystem::current_path();

    // Try different possible paths
    std::vector<std::filesystem::path> possiblePaths = {
    currentPath / "game-assests" / "audio" / "sfx" / fileName,
    currentPath / ".." / "game-assests" / "audio" / "sfx" / fileName,
    currentPath / ".." / ".." / "game-assests" / "audio" / "sfx" / fileName,
    currentPath / ".." / ".." / ".." / "game-assests" / "audio" / "sfx" / fileName
    };

    for (const auto& path: possiblePaths)
    {
        if (std::filesystem::exists(path))
        {
            return path.string();
        }
    }
    
    // If no path found, return the original filename
    return fileName;
}

/*****************************************************************************************
 \brief Checks the result of an FMOD operation and logs errors if any.
 \param result     FMOD operation result.
 \param operation  Description of the operation attempted.
*****************************************************************************************/
void AudioManager::checkFMODError(FMOD_RESULT result, const std::string& operation) const
{
    if (result != FMOD_OK) 
    {
        std::cerr << "FMOD Error during '" << operation << "': " << FMOD_ErrorString(result) << " (code " << result << ")" << std::endl;
    }
}

