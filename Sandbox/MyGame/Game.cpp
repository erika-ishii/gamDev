/*********************************************************************************************
 \file      Game.cpp
 \par       SofaSpuds
 \author    All TEAM MEMBERS
 \brief     Game lifecycle management + Main Menu transition (GUISystem-backed)
*********************************************************************************************/
#include "Graphics/Window.hpp"
#include "Systems/SystemManager.h"
#include "Systems/InputSystem.h"
#include "Systems/LogicSystem.h"
#include "Systems/PhysicSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/audioSystem.h"
#include "Systems/EnemySystem.h"
#include "Systems/AiSystem.h"
#include "Systems/HealthSystem.h"
#include "Debug/CrashLogger.hpp"
#include "Debug/Perf.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <MainMenuPage.hpp>

namespace mygame {

    namespace {
        using clock = std::chrono::high_resolution_clock;

        Framework::SystemManager gSystems;
        Framework::InputSystem* gInputSystem = nullptr;
        Framework::LogicSystem* gLogicSystem = nullptr;
        Framework::PhysicSystem* gPhysicsSystem = nullptr;
        Framework::AudioSystem* gAudioSystem = nullptr;
        Framework::RenderSystem* gRenderSystem = nullptr;
        Framework::EnemySystem* gEnemySystem = nullptr;
        Framework::AiSystem* gAiSystem = nullptr;
        Framework::HealthSystem* gHealthSystem = nullptr;

        enum class GameState { MAIN_MENU, PLAYING, EXIT };
        GameState currentState = GameState::MAIN_MENU;
        bool editorSimulationRunning = false;

        MainMenuPage mainMenu;
    }

    void init(gfx::Window& win)
    {
        gInputSystem = gSystems.RegisterSystem<Framework::InputSystem>(win);
        gLogicSystem = gSystems.RegisterSystem<Framework::LogicSystem>(win, *gInputSystem);
        gPhysicsSystem = gSystems.RegisterSystem<Framework::PhysicSystem>(*gLogicSystem);
        gAiSystem = gSystems.RegisterSystem<Framework::AiSystem>(win,*gLogicSystem);
        gAudioSystem = gSystems.RegisterSystem<Framework::AudioSystem>(win);
        gRenderSystem = gSystems.RegisterSystem<Framework::RenderSystem>(win, *gLogicSystem);
        gHealthSystem = gSystems.RegisterSystem<Framework::HealthSystem>(win);
     
      
        //(void)gPhysicsSystem;
        //(void)gAudioSystem;
        //(void)gRenderSystem;

        gSystems.IntializeAll();
        

        mainMenu.Init(gRenderSystem->ScreenWidth(), gRenderSystem->ScreenHeight());
        currentState = GameState::MAIN_MENU;

        editorSimulationRunning = false;
    }

    void update(float dt)
    {
        TryGuard::Run([&] {
            // F1 toggle for performance window (edge-triggered)
            static bool prevTogglePerf = false;
            const bool togglePerf = gInputSystem && gInputSystem->IsKeyPressed(GLFW_KEY_F1);
            if (togglePerf && !prevTogglePerf) {
                Framework::ToggleVisible();
            }
            prevTogglePerf = togglePerf;
            switch (currentState)
            {
            case GameState::MAIN_MENU:
                mainMenu.Update(gInputSystem);
                if (mainMenu.ConsumeStart())
                {
                    currentState = GameState::PLAYING;
                    editorSimulationRunning = true;
                }
                if (mainMenu.ConsumeExit())  currentState = GameState::EXIT;
                break;

            case GameState::PLAYING:
                if (editorSimulationRunning)
                {
                    gSystems.UpdateAll(dt);
                }
                else if (gInputSystem)
                {
                    gInputSystem->Update(dt);
                }
                break;

            case GameState::EXIT:
                if (gInputSystem) {
                    if (auto* w = gInputSystem->Window()) w->close();
                }
                break;
            }

            Framework::setUpdate(0.0);
            }, "mygame::update");
    }

    void draw()
    {
        TryGuard::Run([&] {
            switch (currentState)
            {
            case GameState::MAIN_MENU:
                if (gRenderSystem) {
                    gRenderSystem->BeginMenuFrame();
                    mainMenu.Draw(gRenderSystem);   // bg + GUI buttons
                    gRenderSystem->EndMenuFrame();
                }
                break;

            case GameState::PLAYING:
                gSystems.DrawAll();
                break;

            case GameState::EXIT:
                break;
            }
            }, "mygame::draw");
    }

    void shutdown()
    {
        std::cout << "[Game] Shutting down systems...\n";

        // Only call ShutdownAll(), do NOT manually delete gEnemySystem etc.
        gSystems.ShutdownAll();

        // Null out global pointers so you don’t accidentally access them later
        gEnemySystem = nullptr;
        gAiSystem = nullptr;
        gRenderSystem = nullptr;
        gAudioSystem = nullptr;
        gPhysicsSystem = nullptr;
        gLogicSystem = nullptr;
        gInputSystem = nullptr;

        std::cout << "[Game] Shutdown complete.\n";
    }

    bool IsEditorSimulationRunning()
    {
        return editorSimulationRunning;
    }

    void EditorPlaySimulation()
    {
        editorSimulationRunning = true;
        if (currentState != GameState::PLAYING)
            currentState = GameState::PLAYING;
    }

    void EditorStopSimulation()
    {
        if (!gLogicSystem)
        {
            editorSimulationRunning = false;
            return;
        }

        gLogicSystem->ReloadLevel();
        editorSimulationRunning = false;
    }

} // namespace mygame
