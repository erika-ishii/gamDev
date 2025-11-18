/*********************************************************************************************
 \file      Core.cpp
 \par       SofaSpuds
 \author    yimo.kong (yimo.kong@digipen.edu) - Primary Author, 80%
            elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) 20%
 \brief     Minimal game/application core driving the main loop, timing, window events,
            and ImGui frame lifecycle.
 \details   Responsibilities:
            - Owns a gfx::Window via RAII and exposes user-supplied callbacks: init/update/render/shutdown.
            - Runs the main loop: poll events → compute/clamp dt → update → render.
            - Orders rendering with ImGui: beginFrame → ImGui BeginFrame → user render → ImGui EndFrame
              → endFrame → swapBuffers.
            - Provides Quit() to request a graceful exit on the next loop iteration.
            Notes:
            * dt is clamped to <= 0.1s to avoid simulation explosions after stalls.
            * Callbacks are checked for null before use, keeping Core lightweight and embeddable.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Core.hpp"
#include "Debug/Perf.h" 

Core::Core(int width, int height, const char* title)
    : m_Running(false),
    // create window immediately (unique_ptr ensures RAII cleanup)
    m_Window(std::make_unique<gfx::Window>(width, height, title)) {}

/*************************************************************************************
 \brief  Enter the main loop and drive the app until the window closes or Quit() is called.
         Calls optional user hooks: init/update/render/shutdown.
*************************************************************************************/
void Core::Run() {
    m_Running = true;

    // Call user-defined init if provided (setup resources, GL state, etc.)
    if (init) init(*m_Window);

    auto t_prev = Clock::now();
    //target simulation step (60 hz)
    const SecondsF fixedStep{ 1.0f / 60.0f };
    // carry over leftover frame time
    SecondsF accumulator = SecondsF::zero();
    // safety cap to avoid a spiral of death
    constexpr int kMaxSubSteps = 5;

    while (m_Running && !m_Window->shouldClose()) {
        // Process input/events (keyboard, mouse, OS signals)
        m_Window->pollEvents();

        // Measure frame delta time (in seconds, as float)
        const auto t_now = Clock::now();
  

        float frameDt = std::chrono::duration_cast<SecondsF>(t_now - t_prev).count();
        t_prev = t_now;
       

        // Clamp delta to avoid simulation explosion after stalls (>100 ms)
        if (frameDt > 0.1f) frameDt = 0.1f;

        Framework::PerfFrameStart(frameDt, false);

        // Accumulate elapsed time and step the simulation with a fixed timestep
        accumulator += SecondsF{ frameDt };
        int subSteps = 0;
        while (accumulator >= fixedStep && subSteps < kMaxSubSteps) {
            if (update) update(fixedStep.count());
            accumulator -= fixedStep;
            ++subSteps;
        }

        // Game/logic update
        // Drop excess time if we hit the cap to prevent spiral of death
        if (subSteps == kMaxSubSteps && accumulator > fixedStep) {
            accumulator = SecondsF::zero();
        }

        m_CurrentNumSteps = subSteps;

        // Rendering stage
        m_Window->beginFrame();    // clear buffers, prepare GL state
        ImGuiLayer::BeginFrame();  // start ImGui frame AFTER pollEvents and BEFORE user render
        if (render) render();      // user drawing code
        ImGuiLayer::EndFrame();    // draw ImGui last into the same framebuffer
        m_Window->endFrame();      // flush GL commands
        m_Window->swapBuffers();   // present frame to screen
    }

    // Cleanup stage
    if (shutdown) shutdown();
}

/*************************************************************************************
 \brief  Request a graceful exit; the loop will stop on the next iteration.
*************************************************************************************/
void Core::Quit() {
    // Allows external code to exit gracefully on next loop iteration
    m_Running = false;
}
