
#include "audioSystem.h"
#include "Core/PathUtils.h"
#include "RenderSystem.h"
#include <iostream>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
/*********************************************************************************************
 \file      AudioSystem.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the AudioSystem class, responsible for managing audio playback,
            resource loading, and debug GUI integration.

 \details
            AudioSystem integrates the SoundManager and Resource_Manager to:
            - Initialize and shutdown audio resources.
            - Load audio assets at startup.
            - Provide per-frame updates (currently empty, reserved for future logic).
            - Render an ImGui-based debug panel for runtime audio management via AudioImGui.
*********************************************************************************************/
namespace Framework {
    /*****************************************************************************************
     \brief
        Constructs the AudioSystem with a reference to the main window.

     \param window
        Reference to the graphics window for ImGui context.
    *****************************************************************************************/
    AudioSystem::AudioSystem(gfx::Window& window) :window(&window) {}
    /*****************************************************************************************
     \brief
        Initializes the audio system and debug GUI.

     \details
        - Initializes the SoundManager engine.
        - Loads all audio assets from the assets directory.
        - Sets the default master volume.
        - Initializes the ImGui audio debug panel via AudioImGui.
    *****************************************************************************************/
    void AudioSystem::Initialize()
    {
        // 1. Start audio engine
        if (!SoundManager::getInstance().initialize())
        {
            std::cerr << "[AudioSystem] Failed to initialize SoundManager!\n";
            return;
        }

        // 2. Load all audio files under /Assets/Audio
        const std::string audioPath = Framework::ResolveAssetPath("Audio").string();
        Resource_Manager::loadAll(audioPath);

        // 3. Set global master volume
        SoundManager::getInstance().setMasterVolume(0.7f);

        if (SoundManager::getInstance().isSoundLoaded("SoundTrackloop"))
            SoundManager::getInstance().playSound("SoundTrackloop", true);

        // 6. Debug UI
        AudioImGui::Initialize(*window);

        std::cout << "[AudioSystem] Initialized successfully.\n";
    }

    /*****************************************************************************************
     \brief
        Updates the audio system per frame.

     \param dt
        Delta time since the last frame (currently unused, reserved for future logic).
    *****************************************************************************************/
    void AudioSystem::Update(float dt)
    {
        (void)dt;
    // In editor-only builds or during shutdown the LogicSystem may not have
    // initialized the global factory yet. Guard against that scenario so we
    // do not dereference a null FACTORY pointer (was causing access
    // violations when the audio system continued updating after the factory
    // was torn down).
        if (!FACTORY)
            return;
        // Iterate all game objects in the factory
        for (auto& [id, gocPtr] : FACTORY->Objects())
        {
            if (!gocPtr) continue;
            GOC* goc = gocPtr.get();
            if (auto* ph = goc->GetComponentType<PlayerHealthComponent>(ComponentTypeId::CT_PlayerHealthComponent))
            {
                if (ph->playerHealth <= 0)
                {
                    if (auto* audio = goc->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent))
                        audio->Stop("footsteps");
                }
            }
            // Get Rigidbody and Audio components
            auto* rb = goc->GetComponentType<RigidBodyComponent>(ComponentTypeId::CT_RigidBodyComponent);
            auto* audio = goc->GetComponentType<AudioComponent>(ComponentTypeId::CT_AudioComponent);
            if (!rb || !audio) continue;
            // Footsteps audio
            const float moveThreshold = 0.01f; // tweak as needed
            bool isMoving = (std::fabs(rb->velX) > moveThreshold ||
                std::fabs(rb->velY) > moveThreshold);
            if (isMoving)
            {if (!audio->playing["footsteps"])audio->Play("footsteps");}
            else
            {if (audio->playing["footsteps"])  audio->Stop("footsteps");}
        }
    }

    /*****************************************************************************************
     \brief
        Draws the ImGui-based audio debug panel.
    *****************************************************************************************/
    void AudioSystem::draw() {
        if (!RenderSystem::IsEditorVisible())
            return;

        AudioImGui::Render();
    };
    /*****************************************************************************************
     \brief
        Shuts down the audio system and releases resources.

     \details
        - Unloads all loaded sounds from SoundManager.
        - Shuts down the AudioImGui debug interface.
        - Prints a confirmation message to the console.
    *****************************************************************************************/
    void AudioSystem::Shutdown()
    {
        // Unload all sounds
        SoundManager::getInstance().unloadAllSounds();
        // Fully tear down the audio backend to release FMOD allocations
        SoundManager::getInstance().shutdown();
        // Shutdown the audio ImGui UI
        AudioImGui::Shutdown();
        std::cout << "[AudioSystem] Audio system shutdown completed.\n";
    }
};