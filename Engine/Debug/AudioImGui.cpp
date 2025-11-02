#include "AudioImGui.h"
#include <iostream>

namespace Framework
{
    bool AudioImGui::s_audioReady = false;
    std::vector<std::string> AudioImGui::soundNames;
    float AudioImGui::masterVolume = 0.7f;

    void AudioImGui::Initialize(gfx::Window& win)
    {
        (void)win;
        if (s_audioReady) return;
        s_audioReady = true;
        std::cout << "[AudioImGui] Audio initialized successfully.\n";
    }
    void AudioImGui::Render()
    {
        ImVec2 windowSize(400, 300);
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(displaySize.x - windowSize.x, 70), ImGuiCond_FirstUseEver);
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGui::SetNextWindowDockID(ImGui::GetMainViewport()->ID, ImGuiCond_FirstUseEver);
        }
        if (ImGui::Begin("Audio Panel"))
        {
            auto& soundManager = SoundManager::getInstance();
            //Master Controller
            if (ImGui::SliderFloat("##Master Volume", &masterVolume, 0.0f, 3.0f))
            {
                soundManager.setMasterVolume(masterVolume);
            }

            //ImGui General Controller
            if (ImGui::Button("Resume All")) { soundManager.pauseAllSounds(false); }
            if (ImGui::Button("Pause All")) { soundManager.pauseAllSounds(true); }
            if (ImGui::Button("Stop All")) { soundManager.stopAllSounds(); }
            //Sort sounds 
            std::vector<std::string> soundIDs;
            for (auto& [id, res] : Resource_Manager::resources_map)
            {
                if (res.type == Resource_Manager::Sound) { soundIDs.push_back(id); }
            }
            ImGui::Separator();

            //Map to hold different states of imgui
            static std::unordered_map < std::string, float> soundVolumes;
            static std::unordered_map < std::string, bool> soundLoops;
            std::sort(soundIDs.begin(), soundIDs.end());

            for (auto& id : soundIDs)
            {
                ImGui::Text("%s", id.c_str());
                ImGui::SameLine();
                if (ImGui::Button(("Play##" + id).c_str())) soundManager.playSound(id, soundVolumes[id], 1.0f, soundLoops[id]);
                ImGui::SameLine();
                if (ImGui::Button(("Pause##" + id).c_str())) soundManager.pauseSound(id);
                if (soundVolumes.find(id) == soundVolumes.end()) { soundVolumes[id] = 1.0f; }
                if (ImGui::SliderFloat(("Volume##" + id).c_str(), &soundVolumes[id], 0.0f, 3.0f))
                {
                    soundManager.setSoundVolume(id, soundVolumes[id]);
                }
                if (soundLoops.find(id) == soundLoops.end()) { soundLoops[id] = false; }
                if (ImGui::Checkbox(("Loop##" + id).c_str(), &soundLoops[id]))
                {
                    if (!soundManager.isSoundPlaying(id)) soundManager.setSoundLoop(id, soundLoops[id]);
                }
                ImGui::Separator();
            }
        }
        ImGui::End();
    }


    void AudioImGui::Shutdown()
    {
        if (!s_audioReady) return;
        s_audioReady = false;
        std::cout << "[AudioImGui] Audio shutdown completed.\n";
    }

}
