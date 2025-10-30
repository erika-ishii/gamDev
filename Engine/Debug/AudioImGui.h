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
