#pragma once
/*********************************************************************************************
 \file      PauseMenuPage.hpp
 \par       SofaSpuds
 \author     erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Declaration of an in-game pause menu with overlay, buttons, and latch events.
*********************************************************************************************/

#include "Systems/GUISystem.hpp"
#include "Systems/InputSystem.h"
#include "Systems/RenderSystem.h"
#include <chrono>
#include <vector>
namespace mygame {

    struct Quad { float x{}, y{}, w{}, h{}; };

    class PauseMenuPage {
    public:
        void Init(int screenW, int screenH);
        void Update(Framework::InputSystem* input);
        void Draw(Framework::RenderSystem* render);

        bool ConsumeResume();
        bool ConsumeMainMenu();
        bool ConsumeOptions();
        bool ConsumeHowToPlay();
        bool ConsumeQuitRequest();
        bool ConsumeExitConfirmed();

        bool IsExitPopupVisible() const { return showExitPopup; }
        void ShowExitPopup();
        void ResetLatches();
        void SyncLayout(int screenW, int screenH);
        void BuildGui();

    private:
        int sw = 1280;
        int sh = 720;
        bool layoutDirty = true;

        unsigned noteTex = 0;
        unsigned headerTex = 0;
        unsigned resumeTex = 0;
        unsigned optionsTex = 0;
        unsigned howToTex = 0;
        unsigned mainMenuTex = 0; // RENAMED from quitTex
        unsigned closeTex = 0;

        unsigned howToNoteTex = 0;
        unsigned howToHeaderTex = 0;
        unsigned howToCloseTex = 0;
        unsigned optionsHeaderTex = 0;
        unsigned exitPopupNoteTex = 0;
        unsigned exitPopupTitleTex = 0;
        unsigned exitPopupPromptTex = 0;
        unsigned exitPopupCloseTex = 0;
        unsigned exitPopupYesTex = 0;
        unsigned exitPopupNoTex = 0;

        float howToHeaderOffsetX = 0.0f;
        float howToHeaderOffsetY = 0.0f;

        struct HowToRowConfig {
            unsigned iconTex = 0;
            unsigned labelTex = 0;
            int frameCount = 1;
            int cols = 1;
            int rows = 1;
            float fps = 8.0f;
            float iconAspectFallback = 1.0f;
            float labelAspectFallback = 1.0f;
        };

        std::vector<HowToRowConfig> howToRows;
        float iconAnimTime = 0.0f;
        std::chrono::steady_clock::time_point lastIconTick{};
        bool iconTimerInitialized = false;
        bool showHowToPopup = false;
        bool showOptionsPopup = false;
        bool showExitPopup = false;
        bool audioMuted = false;
        float masterVolumeDefault = 0.7f;

        bool resumeLatched = false;
        bool mainMenuLatched = false;
        bool optionsLatched = false;
        bool howToLatched = false;
        bool quitRequestedLatched = false;
        bool exitConfirmedLatched = false;

        GUISystem gui;

        Quad panel{ 0.f, 0.f, 0.f, 0.f };
        Quad note{ 0.f, 0.f, 0.f, 0.f };
        Quad header{ 0.f, 0.f, 0.f, 0.f };
        Quad closeBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad resumeBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad optionsBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad howToBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad quitBtn{ 0.f, 0.f, 0.f, 0.f }; // This quad is reused for Main Menu button
        Quad howToPopup{ 0.f, 0.f, 0.f, 0.f };
        Quad howToCloseBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad optionsPopup{ 0.f, 0.f, 0.f, 0.f };
        Quad optionsHeader{ 0.f, 0.f, 0.f, 0.f };
        Quad optionsCloseBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad muteToggleBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad exitPopup{ 0.f, 0.f, 0.f, 0.f };
        Quad exitCloseBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad exitYesBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad exitNoBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad exitTitle{ 0.f, 0.f, 0.f, 0.f };
        Quad exitPrompt{ 0.f, 0.f, 0.f, 0.f };
    };

} // namespace mygame