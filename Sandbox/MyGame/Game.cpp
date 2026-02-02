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
#include "Factory/Factory.h"
#include "Systems/audioSystem.h"
#include "Systems/EnemySystem.h"
#include "Systems/AiSystem.h"
#include "Systems/HealthSystem.h"
#include "Systems/ParticleSystem.h"
#include "Audio/SoundManager.h"
#include "Debug/CrashLogger.hpp"
#include "Graphics/Graphics.hpp"
#include "Debug/Perf.h"
#include "Memory/GameObjectPool.h"
#include "Memory/ObjectAllocator.h"
#include <algorithm>
#include <array>
#include <GLFW/glfw3.h>
#include <chrono>
#include <iostream>
#include <string>
#include <MainMenuPage.hpp>
#include <PauseMenuPage.hpp>
#include <DefeatScreenPage.hpp>

#include "Common/CRTDebug.h"   

#ifdef _DEBUG
#define new DBG_NEW       
#endif

namespace mygame {

    namespace {
        //Audio booleans
        //Main Menu Sounds
        bool mainMenuBGMPlaying = false;
        const char* MAIN_MENU_BGM = "MenuMusic";
        const char* START_BUTTTON = "MenuGameStart";
        const char* EXIT_BUTTTON = "Quit";
        // BGM Sounds
        bool gameplayBGMPlaying = false;
        const char* GAMEPLAY_BGM = "BGM";

        //Defeat Sounds
        const char* DEFEAT = "Defeat";
        const char* BOILING = "Boiling";
        bool defeatBGMPlaying = false;
        bool defeatSoundStarted = false;
        bool boilingStarted = false;
        //Timer
        float bgmFadeTimer = 0.0f;
        constexpr float kBGMFadeDuration = 1.5f;

        using clock = std::chrono::high_resolution_clock;
        const std::array<const char*, 2> kBgmSoundIds = {
            "MenuMusic",
            "BGM"
        };

        bool IsBgmSoundId(const std::string& name)
        {
            return std::any_of(kBgmSoundIds.begin(), kBgmSoundIds.end(),
                [&name](const char* id) { return name == id; });
        }

        void ApplyBgmVolume(float volume)
        {
            SoundManager& sm = SoundManager::getInstance();
            for (const char* id : kBgmSoundIds) {
                if (sm.isSoundLoaded(id)) {
                    sm.setSoundVolume(id, volume);
                }
            }
        }

        void ApplySfxVolume(float volume)
        {
            SoundManager& sm = SoundManager::getInstance();
            const auto sounds = sm.getLoadedSounds();
            for (const auto& name : sounds) {
                if (!IsBgmSoundId(name)) {
                    sm.setSoundVolume(name, volume);
                }
            }
        }
        Framework::SystemManager gSystems;
        Framework::InputSystem* gInputSystem = nullptr;
        Framework::LogicSystem* gLogicSystem = nullptr;
        Framework::PhysicSystem* gPhysicsSystem = nullptr;
        Framework::AudioSystem* gAudioSystem = nullptr;
        Framework::RenderSystem* gRenderSystem = nullptr;
        Framework::EnemySystem* gEnemySystem = nullptr;
        Framework::AiSystem* gAiSystem = nullptr;
        Framework::HealthSystem* gHealthSystem = nullptr;
        Framework::ParticleSystem* gParticleSystem = nullptr;

        enum class GameState { MAIN_MENU, TRANSITIONING, PLAYING, PAUSED, DEFEAT, EXIT };
        GameState currentState = GameState::MAIN_MENU;
        bool editorSimulationRunning = false;
       

        MainMenuPage mainMenu;
        PauseMenuPage pauseMenu;
        DefeatScreenPage defeatScreen;

        constexpr int START_KEY = GLFW_KEY_ENTER; // Keyboard stand-in for a controller Start button.
        constexpr int PAUSE_KEY = GLFW_KEY_ESCAPE;
        void allocatorDumpCallback(const void* block, unsigned int blockIndex)
        {
            std::cout << "[Allocator] Leak: block #" << blockIndex << " at " << block << "\n";
        }
    }

    // Transition timing helpers (file-local).
    static float transitionTimer = 0.0f;
    static constexpr float kStartTransitionDuration = 1.0f;

    void init(gfx::Window& win)
    {
        gInputSystem = gSystems.RegisterSystem<Framework::InputSystem>(win);
        gLogicSystem = gSystems.RegisterSystem<Framework::LogicSystem>(win, *gInputSystem);
        gPhysicsSystem = gSystems.RegisterSystem<Framework::PhysicSystem>(*gLogicSystem);
        gAiSystem = gSystems.RegisterSystem<Framework::AiSystem>(win, *gLogicSystem);
        gAudioSystem = gSystems.RegisterSystem<Framework::AudioSystem>(win);
        gRenderSystem = gSystems.RegisterSystem<Framework::RenderSystem>(win, *gLogicSystem);
        gHealthSystem = gSystems.RegisterSystem<Framework::HealthSystem>(win);
        gParticleSystem = gSystems.RegisterSystem<Framework::ParticleSystem>();

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
            SoundManager::getInstance().update(dt);
            const bool editorMode = Framework::RenderSystem::IsEditorVisible();
            const bool systemsUpdating = (currentState == GameState::PLAYING && editorSimulationRunning);
            if (!systemsUpdating && gInputSystem) {
                gInputSystem->Update(dt);
            }
            float bgmVolume = pauseMenu.GetBgmVolume();
            float sfxVolume = pauseMenu.GetSfxVolume();
            const auto& pauseOptions = pauseMenu.GetOptionsValues();
            const auto& mainOptions = mainMenu.GetOptionsValues();
            if (currentState == GameState::MAIN_MENU || currentState == GameState::TRANSITIONING) {
                bgmVolume = mainMenu.GetBgmVolume();
                sfxVolume = mainMenu.GetSfxVolume();
                pauseMenu.SetOptionsValues(mainOptions);
            }
            else {
                mainMenu.SetOptionsValues(pauseOptions);
            }
            ApplyBgmVolume(bgmVolume);
            ApplySfxVolume(sfxVolume);
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
                if (!mainMenuBGMPlaying && SoundManager::getInstance().isSoundLoaded(MAIN_MENU_BGM)) {
                    SoundManager::getInstance().playSound(MAIN_MENU_BGM, 1.0f, 1.0f, true);
                    SoundManager::getInstance().setSoundVolume(MAIN_MENU_BGM, 0.0f);
                    SoundManager::getInstance().fadeInMusic(MAIN_MENU_BGM, kBGMFadeDuration, 0.3f);
                    mainMenuBGMPlaying = true;
                    gameplayBGMPlaying = false;
                }
                if (mainMenu.ConsumeStart())
                {
                    if (SoundManager::getInstance().isSoundLoaded(START_BUTTTON))
                        SoundManager::getInstance().playSound(START_BUTTTON);
                    SoundManager::getInstance().isSoundLoaded(MAIN_MENU_BGM);
                    SoundManager::getInstance().fadeOutMusic(MAIN_MENU_BGM, kBGMFadeDuration);
                    currentState = GameState::TRANSITIONING;
                    transitionTimer = kStartTransitionDuration;
                    editorSimulationRunning = false;
                    pauseMenu.ResetLatches();
                    if (gHealthSystem)
                        gHealthSystem->ClearPlayerDeathFlag();
                }
                if (mainMenu.ConsumeExit())
                {
                    currentState = GameState::EXIT;
                }
                break;

            case GameState::TRANSITIONING:
                transitionTimer -= dt;
                if (transitionTimer <= 0.0f)
                {
                    currentState = GameState::PLAYING;
                    editorSimulationRunning = true;
                }
                break;

            case GameState::PLAYING:
                if (editorSimulationRunning)
                {
                    gSystems.UpdateAll(dt);
                }
                // When simulation is not running we already refreshed input above.
                handlePerfToggle();

                if (!gameplayBGMPlaying && SoundManager::getInstance().isSoundLoaded(GAMEPLAY_BGM))
                {
                    SoundManager::getInstance().playSound(GAMEPLAY_BGM, 1.0f, 1.0f, true);
                    SoundManager::getInstance().setSoundVolume(GAMEPLAY_BGM, 0.0f); // start silent
                    SoundManager::getInstance().fadeInMusic(GAMEPLAY_BGM, kBGMFadeDuration, 0.4f); // fade to 0.4
                    gameplayBGMPlaying = true;
                }
     
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
                    if (SoundManager::getInstance().isSoundLoaded(GAMEPLAY_BGM))
                    {
                        SoundManager::getInstance().fadeOutMusic(GAMEPLAY_BGM, kBGMFadeDuration);
                        gameplayBGMPlaying = false;
                    }
                    if (SoundManager::getInstance().isSoundLoaded(MAIN_MENU_BGM))
                    {
                        SoundManager::getInstance().playSound(MAIN_MENU_BGM, true); // loop
                        SoundManager::getInstance().setSoundVolume(MAIN_MENU_BGM, 0.0f);
                        SoundManager::getInstance().fadeInMusic(MAIN_MENU_BGM, kBGMFadeDuration, 0.4f);
                        mainMenuBGMPlaying = true;
                    }
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

                if (!defeatSoundStarted && SoundManager::getInstance().isSoundLoaded(DEFEAT))
                {
                    SoundManager::getInstance().playSound(DEFEAT, false); // one-shot
                    SoundManager::getInstance().setSoundVolume(DEFEAT, 0.5f);
                    defeatSoundStarted = true;
                }
                if (!boilingStarted && SoundManager::getInstance().isSoundLoaded(BOILING))
                {
                    SoundManager::getInstance().playSound(BOILING, false, 1.0f); // start silent
                    SoundManager::getInstance().fadeInMusic(BOILING, 0.7f, 1.0f); // fade in to 0.5 volume over 2 seconds
                    boilingStarted = true;
                }
                if (gameplayBGMPlaying && SoundManager::getInstance().isSoundLoaded(GAMEPLAY_BGM))
                {
                    SoundManager::getInstance().fadeOutMusic(GAMEPLAY_BGM, kBGMFadeDuration);
                    gameplayBGMPlaying = false;
                }

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
                    if (SoundManager::getInstance().isSoundLoaded(DEFEAT)&& SoundManager::getInstance().isSoundLoaded(BOILING))
                    {
                        SoundManager::getInstance().stopSound(DEFEAT);
                        SoundManager::getInstance().stopSound(BOILING);
                    }
                    if (SoundManager::getInstance().isSoundLoaded(GAMEPLAY_BGM))
                    {
                        SoundManager::getInstance().playSound(GAMEPLAY_BGM, true);
                        SoundManager::getInstance().setSoundVolume(GAMEPLAY_BGM, 0.0f);
                        SoundManager::getInstance().fadeInMusic(GAMEPLAY_BGM, kBGMFadeDuration, 0.4f);
                        gameplayBGMPlaying = true;
                    }

                    defeatSoundStarted = false;
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
                    gRenderSystem->RenderBrightnessOverlay();
                }
                break;

            case GameState::PLAYING:
                gSystems.DrawAll();
                if (gRenderSystem) {
                    gRenderSystem->RenderBrightnessOverlay();
                }
                break;

            case GameState::TRANSITIONING:
                if (gRenderSystem)
                {
                    gRenderSystem->BeginMenuFrame();
                    mainMenu.Draw(gRenderSystem);
                    const float normalized = (kStartTransitionDuration > 0.0f)
                        ? std::clamp(1.0f - (transitionTimer / kStartTransitionDuration), 0.0f, 1.0f)
                        : 1.0f;
                    gfx::Graphics::renderRectangleUI(0.0f, 0.0f,
                        static_cast<float>(gRenderSystem->ScreenWidth()),
                        static_cast<float>(gRenderSystem->ScreenHeight()),
                        0.0f, 0.0f, 0.0f, normalized,
                        gRenderSystem->ScreenWidth(), gRenderSystem->ScreenHeight());
                    gRenderSystem->EndMenuFrame();
                    gRenderSystem->RenderBrightnessOverlay();
                }
                break;

            case GameState::PAUSED:
                gSystems.DrawAll();
                if (gRenderSystem) {
                    gRenderSystem->BeginMenuFrame();
                    pauseMenu.Draw(gRenderSystem);
                    gRenderSystem->EndMenuFrame();
                    gRenderSystem->RenderBrightnessOverlay();
                }
                break;

            case GameState::DEFEAT:
                gSystems.DrawAll();
                if (gRenderSystem)
                {
                    gRenderSystem->BeginMenuFrame();
                    defeatScreen.Draw(gRenderSystem);
                    gRenderSystem->EndMenuFrame();
                    gRenderSystem->RenderBrightnessOverlay();
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

        const unsigned leaks = Framework::GameObjectPool::Storage().Allocator().DumpMemoryInUse(&allocatorDumpCallback);
        std::cout << "[Allocator] DumpMemoryInUse found " << leaks << " live blocks at shutdown.\n";
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
        if (Framework::FACTORY)
            Framework::FACTORY->Layers().LogVisibilitySummary("EditorPlaySimulation");
    }

    void EditorStopSimulation()
    {

        editorSimulationRunning = false;
        if (Framework::FACTORY)
            Framework::FACTORY->Layers().LogVisibilitySummary("EditorStopSimulation");
    }

} // namespace mygame