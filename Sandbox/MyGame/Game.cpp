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
#include "Audio/SoundManager.h"
#include "Debug/CrashLogger.hpp"
#include "Debug/Perf.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <MainMenuPage.hpp>
#include <PauseMenuPage.hpp>
#include <DefeatScreenPage.hpp>

#include "Common/CRTDebug.h"   

#ifdef _DEBUG
#define new DBG_NEW       
#endif

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

        enum class GameState { MAIN_MENU, PLAYING, PAUSED, DEFEAT, EXIT };
        GameState currentState = GameState::MAIN_MENU;
        bool editorSimulationRunning = false;

        MainMenuPage mainMenu;
        PauseMenuPage pauseMenu;
        DefeatScreenPage defeatScreen;

        constexpr int START_KEY = GLFW_KEY_ENTER; // Keyboard stand-in for a controller Start button.
        constexpr int PAUSE_KEY = GLFW_KEY_ESCAPE;
    }

    void init(gfx::Window& win)
    {
        gInputSystem = gSystems.RegisterSystem<Framework::InputSystem>(win);
        gLogicSystem = gSystems.RegisterSystem<Framework::LogicSystem>(win, *gInputSystem);
        gPhysicsSystem = gSystems.RegisterSystem<Framework::PhysicSystem>(*gLogicSystem);
        gAiSystem = gSystems.RegisterSystem<Framework::AiSystem>(win, *gLogicSystem);
        gAudioSystem = gSystems.RegisterSystem<Framework::AudioSystem>(win);
        gRenderSystem = gSystems.RegisterSystem<Framework::RenderSystem>(win, *gLogicSystem);
        gHealthSystem = gSystems.RegisterSystem<Framework::HealthSystem>(win);


        //(void)gPhysicsSystem;
        //(void)gAudioSystem;
        //(void)gRenderSystem;

        gSystems.IntializeAll();


        mainMenu.Init(gRenderSystem->ScreenWidth(), gRenderSystem->ScreenHeight());
        pauseMenu.Init(gRenderSystem->ScreenWidth(), gRenderSystem->ScreenHeight());
        defeatScreen.Init(gRenderSystem->ScreenWidth(), gRenderSystem->ScreenHeight());
        currentState = GameState::MAIN_MENU;

        editorSimulationRunning = false;
    }

    void update(float dt)
    {
        TryGuard::Run([&] {
            const bool editorMode = Framework::RenderSystem::IsEditorVisible();
            const bool systemsUpdating = (currentState == GameState::PLAYING && editorSimulationRunning);
            if (!systemsUpdating && gInputSystem) {
                gInputSystem->Update(dt);
            }
            auto handlePerfToggle = []() {
                static bool prevTogglePerf = false;
                const bool togglePerf = gInputSystem && gInputSystem->IsKeyPressed(GLFW_KEY_F1);
                if (togglePerf && !prevTogglePerf) {
                    Framework::ToggleVisible();
                }
                prevTogglePerf = togglePerf;
                };
            switch (currentState)
            {
            case GameState::MAIN_MENU:
                mainMenu.Update(gInputSystem);
                handlePerfToggle();
                if (mainMenu.ConsumeStart())
                {
                    currentState = GameState::PLAYING;
                    editorSimulationRunning = true;
                    pauseMenu.ResetLatches();
                    if (gHealthSystem)
                        gHealthSystem->ClearPlayerDeathFlag();
                }
                if (mainMenu.ConsumeExit())
                {
                    currentState = GameState::EXIT;
                }
                break;

            case GameState::PLAYING:
                if (editorSimulationRunning)
                {
                    gSystems.UpdateAll(dt);
                }
                // When simulation is not running we already refreshed input above.
                handlePerfToggle();
                if (!editorMode && gHealthSystem && gHealthSystem->HasPlayerDied())
                {
                    defeatScreen.ResetLatches();
                    if (gRenderSystem)
                        defeatScreen.SyncLayout(gRenderSystem->ScreenWidth(), gRenderSystem->ScreenHeight());
                    editorSimulationRunning = false;
                    currentState = GameState::DEFEAT;
                    break;
                }
                if (gInputSystem && !editorMode &&
                    (gInputSystem->IsKeyPressed(PAUSE_KEY) || gInputSystem->IsKeyPressed(START_KEY)))
                {
                    pauseMenu.ResetLatches();
                    currentState = GameState::PAUSED;
                }
                break;

            case GameState::PAUSED:
                if (editorMode)
                {
                    currentState = GameState::PLAYING;
                    break;
                }
                pauseMenu.Update(gInputSystem);
                handlePerfToggle();
                if (pauseMenu.ConsumeResume() ||
                    (gInputSystem && (gInputSystem->IsKeyPressed(PAUSE_KEY) || gInputSystem->IsKeyPressed(START_KEY))))
                {
                    // [UPDATED LOGIC] Resume based on previous state if possible, 
                    // or default to PLAYING. If we came from DEFEAT, going back to PLAYING
                    // might be weird if the player is still dead, but typically "Resume" means "Back to Game".
                    // If the player is dead, the next frame's check in PLAYING will send them back to DEFEAT screen.
                    currentState = GameState::PLAYING;
                    break;
                }

                if (pauseMenu.ConsumeMainMenu())
                {
                    if (gLogicSystem)
                    {
                        gLogicSystem->ReloadLevel();
                    }
                    if (gHealthSystem)
                        gHealthSystem->ClearPlayerDeathFlag();

                    editorSimulationRunning = false;
                    currentState = GameState::MAIN_MENU;
                    break;
                }

                if (pauseMenu.ConsumeExitConfirmed())
                {
                    currentState = GameState::EXIT;
                    break;
                }

                if (pauseMenu.ConsumeQuitRequest())
                {
                    pauseMenu.ShowExitPopup();
                }
                break;

            case GameState::DEFEAT:
                defeatScreen.Update(gInputSystem);
                handlePerfToggle();

                // [ADDED] Check for Pause input to go to Pause Menu
                if (gInputSystem && !editorMode &&
                    (gInputSystem->IsKeyPressed(PAUSE_KEY) || gInputSystem->IsKeyPressed(START_KEY)))
                {
                    pauseMenu.ResetLatches();
                    currentState = GameState::PAUSED;
                    break;
                }

                if (defeatScreen.ConsumeTryAgain())
                {
                    if (gLogicSystem)
                    {
                        gLogicSystem->ReloadLevel();
                    }
                    if (gHealthSystem)
                        gHealthSystem->ClearPlayerDeathFlag();

                    editorSimulationRunning = true;
                    currentState = GameState::PLAYING;
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
                    gRenderSystem->HandleMenuShortcuts();
                    gRenderSystem->BeginMenuFrame();
                    mainMenu.Draw(gRenderSystem);   // bg + GUI buttons
                    gRenderSystem->EndMenuFrame();
                }
                break;

            case GameState::PLAYING:
                gSystems.DrawAll();
                break;

            case GameState::PAUSED:
                gSystems.DrawAll();
                if (gRenderSystem) {
                    gRenderSystem->BeginMenuFrame();
                    pauseMenu.Draw(gRenderSystem);
                    gRenderSystem->EndMenuFrame();
                }
                break;

            case GameState::DEFEAT:
                gSystems.DrawAll();
                if (gRenderSystem)
                {
                    gRenderSystem->BeginMenuFrame();
                    defeatScreen.Draw(gRenderSystem);
                    gRenderSystem->EndMenuFrame();
                }
                break;


            case GameState::EXIT:
                break;
            }
            }, "mygame::draw");
    }

    void onAppFocusChanged(bool suspended)
    {
        // Halt/resume audio cleanly and flush transient input so keys do not stick.
        SoundManager::getInstance().pauseAllSounds(suspended);
        if (gInputSystem)
        {
            gInputSystem->Manager().ClearState();
        }
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

        editorSimulationRunning = false;
    }

} // namespace mygame