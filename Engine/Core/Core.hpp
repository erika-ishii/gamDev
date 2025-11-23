/*********************************************************************************************
 \file      Core.hpp
 \par       SofaSpuds
 \author    yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Minimal application core that owns the window and drives the main loop.
 \details   Provides lightweight lifecycle callbacks (init/update/render/shutdown) and an
            orderly frame flow:
             pollEvents → accumulate frame dt → fixed-step update → beginFrame →
             ImGui::BeginFrame → user render → ImGui::EndFrame → endFrame → swapBuffers.
            The simulation advances using a 60 Hz fixed timestep (with a safety cap on
            sub-steps) while the measured frame delta is still clamped (≤ 0.1s) to avoid
            runaway accumulation after stalls.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <memory>
#include <chrono>
#include "Graphics/Window.hpp"
#include "Debug/ImGuiLayer.h"

class Core {
public:
    // Callback types (simple C-style function pointers). Each is optional (nullptr by default).
    using InitFn = void(*)(gfx::Window&); // Called once at startup
    using UpdateFn = void(*)(float);        // Called every frame with delta time (seconds)
    using RenderFn = void(*)();             // Called every frame to draw
    using ShutdownFn = void(*)();             // Called once at shutdown
    using SuspendFn = void(*)(bool);         // Called when the app is suspended/resumed

    /// \brief Create a windowed application core.
    Core(int width, int height, const char* title, bool fullscreen);
    ~Core() = default; // unique_ptr handles window cleanup (RAII)

    /// \brief Run the main loop until Quit() is called or the window closes.
    void Run();

    /// \brief Request loop termination; exits gracefully on the next iteration.
    void Quit();

    /// \brief Set all lifecycle callbacks at once (init/update/render/shutdown).
    void SetCallbacks(InitFn i, UpdateFn u, RenderFn r, ShutdownFn s) {
        init = i; update = u; render = r; shutdown = s;
    }
    void SetSuspendCallback(SuspendFn s) { onSuspend = s; }
    int GetCurrentNumSteps() const noexcept { return m_CurrentNumSteps; }
    float GetFixedDeltaSeconds() const noexcept { return m_FixedStep.count(); }

private:
    using Clock = std::chrono::steady_clock;       // monotonic clock for dt
    using SecondsF = std::chrono::duration<float>;    // time duration in float seconds

    bool m_Running{ false };                         ///< Main loop flag
    std::unique_ptr<gfx::Window> m_Window;           ///< Owned window (RAII)
    int m_CurrentNumSteps = 0;
    SecondsF  m_FixedStep{ 1.0f / 60.0f };
    // Callback storage (may be null)
    InitFn     init{ nullptr };
    UpdateFn   update{ nullptr };
    RenderFn   render{ nullptr };
    ShutdownFn shutdown{ nullptr };
    SuspendFn  onSuspend{ nullptr };
};
