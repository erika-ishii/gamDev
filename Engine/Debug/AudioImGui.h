/*********************************************************************************************
 \file      AudioImGui.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of the AudioImGui class, which provides an ImGui-based interface
            for managing and visualizing audio playback within the framework.

 \details
            AudioImGui allows developers to:
            - Initialize a GUI overlay for audio management.
            - Render audio controls, including a master volume slider and sound list.
            - Shutdown the interface and clean up associated resources.

            This class relies on ImGui and integrates with the framework's SoundManager
            and Resource_Manager for accessing and controlling audio assets.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "../Audio/SoundManager.h"
#include "../Resource_Manager/Resource_Manager.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Graphics/Window.hpp"
#include <vector>
#include <string>
namespace Framework 
{
    /*********************************************************************************************
     \class AudioImGui
     \brief
        Provides an ImGui-based interface for managing and controlling audio assets in the framework.

     \details
        AudioImGui allows developers to visualize and manipulate audio in real time. The interface
        provides:
          - A master volume slider to control global audio levels.
          - Controls to play, pause, resume, or stop all sounds.
          - Per-sound controls including volume sliders and loop toggles.
          - Dynamic listing of all loaded sound assets from the Resource_Manager.

        The class uses static members to maintain state across frames and to ensure a single
        interface instance per application. Initialization and shutdown must be explicitly
        called to manage ImGui resources properly.

     \note
        - Requires a gfx::Window reference during initialization to attach the ImGui context.
        - Designed for runtime debugging and visualization; not intended for production audio
          gameplay logic.
    *********************************************************************************************/

    class AudioImGui
    {
    public:
        static void Initialize(gfx::Window& win);
        static void Render();
        static void Shutdown();
        AudioImGui() = default;
        ~AudioImGui() = default;

    private:

        static bool s_audioReady;
        static std::vector<std::string> soundNames;
        static float masterVolume;




    };
}
