#include "audioSystem.h"
#include <iostream>


namespace Framework {
    AudioSystem::AudioSystem(gfx::Window& window) :window(&window) {}

    void AudioSystem::Initialize()
    {
        // Initialize audio engine
        if (!SoundManager::getInstance().initialize())
        {
            std::cerr << "[AudioSystem] Failed to initialize SoundManager!" << std::endl; return;
        }
        
        // Load all sounds (previously in AudioImGui)
        Resource_Manager::loadAll("../../assets/Audio");
        // Set default master volume
        SoundManager::getInstance().setMasterVolume(0.7f);
        std::cout << "[AudioSystem] Audio system initialized successfully.\n";
        // Initialize ImGui for audio panel
        AudioImGui::Initialize(*window);
    }

    void AudioSystem::Update(float dt){ (void)dt; }
    void AudioSystem::draw() { AudioImGui::Render(); };

    void AudioSystem::Shutdown()
    {
        // Unload all sounds
        SoundManager::getInstance().unloadAllSounds();
        // Shutdown the audio ImGui UI
        AudioImGui::Shutdown();
        std::cout << "[AudioSystem] Audio system shutdown completed.\n";
    }
};