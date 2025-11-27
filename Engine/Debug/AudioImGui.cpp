/*********************************************************************************************
 \file      AudioImGui.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the AudioImGui class, providing an ImGui-based interface for
            audio management, including master volume control, playback control, and
            per-sound volume and loop settings.

 \details
            AudioImGui integrates with the framework's SoundManager and Resource_Manager
            to allow developers to visualize and control audio assets during runtime.
            Features include:
            - Master volume slider for all sounds.
            - Play, pause, stop, and resume controls for all sounds.
            - Individual sound volume sliders and loop toggles.
            - Dynamic listing of all loaded sound resources.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "AudioImGui.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework
{
    //Static Memeber Initialization
    bool AudioImGui::s_audioReady = false;
    std::vector<std::string> AudioImGui::soundNames;
    float AudioImGui::masterVolume = 0.7f;
    bool AudioImGui::s_showUnsupportedPopup = false;
    std::string AudioImGui::s_unsupportedFile;
    std::filesystem::path AudioImGui::s_assetsRoot;
    std::string AudioImGui::s_importStatus;
    /*****************************************************************************************
    \brief
    Initializes the AudioImGui interface and sets up ImGui for audio control.

    \param win
    Reference to the graphics window where the ImGui interface will be attached.

    \details
    This function must be called before rendering the audio interface. It ensures
    that the system is ready and prints a confirmation message to the console.
    *****************************************************************************************/
    void AudioImGui::Initialize(gfx::Window& win)
    {
        (void)win;
        if (s_audioReady) return;
        s_audioReady = true;
        std::cout << "[AudioImGui] Audio initialized successfully.\n";
    }
    void AudioImGui::SetAssetsRoot(const std::filesystem::path& root)
    {
        std::error_code ec;
        s_assetsRoot = std::filesystem::weakly_canonical(root, ec);
        if (ec)
            s_assetsRoot = root;
    }
    /*****************************************************************************************
    \brief
    Renders the ImGui audio interface with controls for all loaded sounds.

    \details
    Provides a GUI window that allows the user to:
    - Adjust master volume for all sounds.
    - Play, pause, resume, or stop all sounds.
    - View and control individual sounds with sliders for volume and checkboxes for looping.
    - Dynamically detects and lists all sound resources from the Resource_Manager.
    *****************************************************************************************/
    void AudioImGui::Render()
    {
        if (s_showUnsupportedPopup) { ImGui::OpenPopup("Unsupported Audio File"); }
        if (ImGui::BeginPopupModal("Unsupported Audio File", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Failed to load audio file:\n%s", s_unsupportedFile.c_str());
            ImGui::Separator();
            if (ImGui::Button("OK", ImVec2(120, 0))) 
            {s_showUnsupportedPopup = false;ImGui::CloseCurrentPopup();}
            ImGui::EndPopup();
        }
        ImVec2 windowSize(400, 300);
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGuiIO& io = ImGui::GetIO();

        // Allow ImGui to restore position normally
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);

        // Only set a default pos IF NOT DOCKING
        if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable))
        {
            // Use FirstUseEver so it applies only on the first launch 
            ImGui::SetNextWindowPos(ImVec2(30, 70), ImGuiCond_FirstUseEver);
        }
        else
        {
            // When docking is enabled, don't force position at all
            // ImGui will dock it automatically based on saved layout
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
            ImGui::SeparatorText("Import");
            ImGui::TextDisabled("Drag .wav or .mp3 from the Content Browser to load them.");
            ImVec2 dropSize(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 2.0f);
            if (ImGui::Button("Drop Audio Here", dropSize))
            {
                // Button only provides the target; no click action needed.
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_AUDIO_PATH"))
                {
                    if (payload->Data && payload->DataSize > 0)
                    {
                        std::string relative(static_cast<const char*>(payload->Data), payload->DataSize - 1);
                        std::filesystem::path absolute = relative;
                        if (!relative.empty())
                        {
                            if (!s_assetsRoot.empty())
                            {
                                std::error_code ec;
                                absolute = std::filesystem::weakly_canonical(s_assetsRoot / relative, ec);
                                if (ec)
                                    absolute = s_assetsRoot / relative;
                            }
                        }

                        bool loaded = false;
                        if (!absolute.empty())
                            loaded = Resource_Manager::load(relative, absolute.string());

                        s_importStatus = loaded
                            ? "Loaded audio: " + relative
                            : "Failed to load audio: " + relative;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (!s_importStatus.empty())
            {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", s_importStatus.c_str());
            }
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

    /*****************************************************************************************
    \brief
    Shuts down the AudioImGui interface and releases associated resources.

    \details
    Marks the interface as no longer ready and prints a shutdown confirmation message.
    This should be called before application exit or when audio GUI is no longer needed.
    *****************************************************************************************/
    void AudioImGui::Shutdown()
    {
        if (!s_audioReady) return;
        s_audioReady = false;
        std::cout << "[AudioImGui] Audio shutdown completed.\n";
    }
    
    void AudioImGui::ShowUnsupportedAudioPopup(const std::string& file)
    {
        s_unsupportedFile = file;
        s_showUnsupportedPopup = true;
    }
}
