/*********************************************************************************************
 \file      Game.hpp
 \par       SofaSpuds
 \author    All TEAM MEMBERS

 \brief     Public interface of the sandbox game layer. It exposes the lifecycle
            entry points used by the engine:
              • init(gfx::Window&): Hook up the Window, create the GameObjectFactory,
                load prefabs/level data, initialize Graphics, ImGui, text, and audio.
              • update(float dt): Handle input, advance animation/physics, process the
                factory sweep, and record per-stage CPU timings for the profiler.
              • draw(): Render background, sprites/rects/circles and on-screen text,
                build debug UIs (Spawn, Crash Tests, Performance), and submit ImGui.
              • shutdown(): Tear down audio/graphics resources, unload prefabs, and
                destroy ImGui/context objects safely.

            Audio helpers (initializeAudio / cleanupAudio) wrap SoundManager start/stop.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include "Graphics/Window.hpp"

namespace mygame {

    // Game lifecycle
    void init(gfx::Window& win);
    void update(float dt);
    void draw();
    void shutdown();
    void onAppFocusChanged(bool suspended);
    // Editor simulation controls
    bool IsEditorSimulationRunning();
    void EditorPlaySimulation();
    void EditorStopSimulation();

}
