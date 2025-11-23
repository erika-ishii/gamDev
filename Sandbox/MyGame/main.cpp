/*********************************************************************************************
 \file      main.cpp
 \par       SofaSpuds
 \author    yimo kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Program entry point.
            - Loads window configuration from JSON (../../Data_Files/window.json).
            - Creates the Core (window + main loop) and wires the game lifecycle
              callbacks (init/update/draw/shutdown) exposed by Game.hpp.
            - On MSVC builds, enables CRT leak checking at program exit.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "../Engine/Core/Core.hpp"
#include "../Engine/Core/PathUtils.h"
#include "Game.hpp"
#include "Config/WindowConfig.h"
#include <filesystem>

#ifdef _MSC_VER
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

int main()
{
#ifdef _MSC_VER
    // Enable MSVC CRT leak checks (prints leaks in the Output window at exit).
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    // Ensure the working directory matches the executable so relative paths resolve in builds.
    if (auto exeDir = Framework::GetExecutableDir(); !exeDir.empty())
    {
        std::error_code ec;
        std::filesystem::current_path(exeDir, ec);
    }
    // Load window config (falls back to some defaults if file is missing).
    WindowConfig cfg = LoadWindowConfig(Framework::ResolveDataPath("window.json").string());
    if (cfg.width <= 0)  cfg.width = 1280;
    if (cfg.height <= 0) cfg.height = 720;
    if (cfg.title.empty()) cfg.title = "SofaSpuds Engine";

    // Create engine core and register game callbacks.
    Core core(cfg.width, cfg.height, cfg.title.c_str(), cfg.fullscreen);
    core.SetCallbacks(mygame::init, mygame::update, mygame::draw, mygame::shutdown);
    core.SetSuspendCallback(mygame::onAppFocusChanged);
    // Run main loop.
    core.Run();
    return 0;
}
