#pragma once
/*********************************************************************************************
 \file      PauseMenuPage.hpp
 \par       SofaSpuds
 \author     erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Declaration of an in-game pause menu with overlay, buttons, and latch events.
 \details   The pause page draws a dimmed overlay on top of the active scene, displays a
             stylized note card, and surfaces actions for resuming, options/main menu,
            how-to reference, and quit. Button clicks set one-shot latches consumed by the
            game loop. Layout uses absolute coordinates (bottom-left origin) and the
            GUISystem for interaction.
*********************************************************************************************/

#include "Systems/GUISystem.hpp"
#include "Systems/InputSystem.h"
#include "Systems/RenderSystem.h"
#include <chrono>
#include <vector>
namespace mygame {

    /*************************************************************************************
      \struct Quad
      \brief  Minimal rectangle helper for positioning UI elements.
    *************************************************************************************/
    struct Quad { float x{}, y{}, w{}, h{}; };

    /*************************************************************************************
      \class  PauseMenuPage
      \brief  Overlay pause menu with stylized parchment buttons (resume/options/how-to/quit).
    *************************************************************************************/
    class PauseMenuPage {
    public:
        // Initialize layout, load textures, and build GUI widgets.
        void Init(int screenW, int screenH);

        // Update hover/click state for buttons.
        void Update(Framework::InputSystem* input);

        // Draw overlay, panel, labels, and buttons.
        void Draw(Framework::RenderSystem* render);

        // Consume latches (true once per click).
        bool ConsumeResume();
        bool ConsumeMainMenu();
        bool ConsumeOptions();
        bool ConsumeHowToPlay();
        bool ConsumeQuitRequest();
        bool ConsumeExitConfirmed();

        // Exit confirmation helpers.
        bool IsExitPopupVisible() const { return showExitPopup; }
        void ShowExitPopup();
        // Reset internal one-shot latches (used when reopening the pause menu).
        void ResetLatches();

        // Rebuild layout if the screen size changes (keeps buttons aligned with cursor).
        void SyncLayout(int screenW, int screenH);

        // Rebuild GUI widgets for the current layout/state.
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
        unsigned quitTex = 0;
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
        Quad quitBtn{ 0.f, 0.f, 0.f, 0.f };
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
