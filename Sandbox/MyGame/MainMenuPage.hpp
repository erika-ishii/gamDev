/*********************************************************************************************
 \file      MainMenuPage.hpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Declaration of a simple main-menu screen with cached texture resolution and GUI.
 \details   Renders a simple main menu with a fullscreen background texture, Start/Exit image
            buttons (hover + click callbacks), one-shot latches (startLatched/exitLatched)
            consumed by the game loop, and optional hint text via RenderSystem; textures are
            resolved by first checking Resource_Manager keys, then attempting
            Resource_Manager::load(primaryKey, path), and finally falling back to
            gfx::Graphics::loadTexture(path); typical flow is Init(sw,sh) → per-frame
            Update(input) → Draw(render), while the outer loop polls ConsumeStart()/ConsumeExit()
            to act once per click.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include "Systems/GUISystem.hpp"
#include "Systems/RenderSystem.h"
#include "Systems/InputSystem.h"
#include "Audio/SoundManager.h"
#include <chrono>
#include <vector>
namespace mygame {

    /*************************************************************************************
      \struct RectF
      \brief  Lightweight rectangle for positioning UI elements (origin: bottom-left).
    *************************************************************************************/
    struct RectF { float x{}, y{}, w{}, h{}; };

    /*************************************************************************************
      \class  MainMenuPage
      \brief  Simple main-menu page owning background + two image buttons (Start/Exit).
      \note   Latch-style events are exposed via ConsumeStart()/ConsumeExit().
    *************************************************************************************/
    class MainMenuPage {
    public:
        /*************************************************************************
          \brief  Initialize screen size, resolve textures, and build GUI buttons.
          \param  screenW  Swapchain/window width in pixels.
          \param  screenH  Swapchain/window height in pixels.
        *************************************************************************/
        void Init(int screenW, int screenH);

        /*************************************************************************
          \brief  Update interactive state (hover/click) for menu widgets.
          \param  input  Pointer to engine input system.
        *************************************************************************/
        void Update(Framework::InputSystem* input);

        /*************************************************************************
          \brief  Draw background, GUI, and optional hint text.
          \param  render Pointer to RenderSystem for text rendering (optional).
        *************************************************************************/
        void Draw(Framework::RenderSystem* render);

        /*************************************************************************
          \brief  Consume the Start latch (true once after a Start click).
          \return true if Start was triggered this frame; false otherwise.
        *************************************************************************/
        bool ConsumeStart();

        /*************************************************************************
        \brief  Consume the Options latch (true once after an Options click).
        \return true if Options was triggered this frame; false otherwise.
      *************************************************************************/
        bool ConsumeOptions();

        /*************************************************************************
          \brief  Consume the How To Play latch (true once after a How To Play click).
          \return true if How To Play was triggered this frame; false otherwise.
        *************************************************************************/
        bool ConsumeHowToPlay();

        /*************************************************************************
          \brief  Consume the Exit latch (true once after an Exit click).
          \return true if Exit was triggered this frame; false otherwise.
        *************************************************************************/
        bool ConsumeExit();


        // Adjust cached layout if the screen size changes (keeps buttons under the cursor).
        void SyncLayout(int screenW, int screenH);

    private:
        // --- Screen ----------------------------------------------------------------------
        int sw = 1280;                 //Screen width in pixels.
        int sh = 720;                  //Screen height in pixels.

        // --- Background ------------------------------------------------------------------
        unsigned menuBgTex = 0;       

        // --- Button textures --------------------------------------------------------------
        unsigned startBtnIdleTex = 0;  //Start button idle texture.
        unsigned startBtnHoverTex = 0; //Start button hover texture.
        unsigned optionsBtnIdleTex = 0; //Options button idle texture.
        unsigned optionsBtnHoverTex = 0; //Options button hover texture.
        unsigned howToBtnIdleTex = 0;   //How To Play button idle texture.
        unsigned howToBtnHoverTex = 0;  //How To Play button hover texture.
        unsigned exitBtnIdleTex = 0;  //Exit button idle texture.
        unsigned exitBtnHoverTex = 0;  //Exit button hover texture.
        unsigned closePopupTex = 0;    //Popup close button texture.
        unsigned optionsHeaderTex = 0;
        unsigned exitPopupNoteTex = 0;     //Exit confirmation note texture.
        unsigned exitPopupTitleTex = 0;    //Exit popup title texture.
        unsigned exitPopupPromptTex = 0;   //Exit popup prompt texture.
        unsigned exitPopupCloseTex = 0;    //Exit popup close texture.
        unsigned exitPopupYesTex = 0;      //Exit popup YES texture.
        unsigned exitPopupNoTex = 0;       //Exit popup NO texture.
        // --- How To Play textures ---------------------------------------------------------
        unsigned noteBackgroundTex = 0;    //Note-style popup background.
        unsigned howToHeaderTex = 0;       //"How To Play" title art.
        float howToHeaderOffsetX = 0.0f;   //Configurable header offset X.
        float howToHeaderOffsetY = 0.0f;   //Configurable header offset Y.
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

        // --- Animated UI sprites -----------------------------------------------------------
        std::vector<HowToRowConfig> howToRows;                 //Config-driven how-to rows.
        float iconAnimTime = 0.0f;                             //Accumulated seconds for animation.
        std::chrono::steady_clock::time_point lastIconTick{};  //Monotonic timer snapshot.
        bool iconTimerInitialized = false;                     //Guard to seed the timer once.

        // --- Latches ---------------------------------------------------------------------
        bool startLatched = false;
        bool exitLatched = false;
        bool optionsLatched = false;
        bool howToLatched = false;
        bool showHowToPopup = false;
        bool showOptionsPopup = false;
        bool showExitPopup = false;
        bool audioMuted = false;
        float masterVolumeDefault = 0.7f;
   

        // --- GUI system ------------------------------------------------------------------
        GUISystem gui;                 //Immediate-mode GUI owner for menu buttons.

        // --- Layout (origin: bottom-left) ------------------------------------------------
        RectF startBtn{ 100.f, 260.f, 220.f, 58.f }; //Start button rectangle.
        RectF optionsBtn{ 100.f, 260.f, 220.f, 58.f }; //options button rectangle.
        RectF howToBtn{ 100.f, 180.f, 220.f, 58.f }; //how to play button rectangle.
        RectF exitBtn{ 100.f, 180.f, 220.f, 58.f }; //Exit button rectangle.
        RectF howToPopup{ 72.f, 420.f, 520.f, 320.f }; //Popup background rectangle.
        RectF closeBtn{ 0.f, 0.f, 56.f, 56.f };
        RectF optionsPopup{ 0.f, 0.f, 520.f, 320.f };   //Options popup rectangle.
        RectF optionsHeader{ 0.f, 0.f, 220.f, 80.f };   //Options title art rectangle.
        RectF optionsCloseBtn{ 0.f, 0.f, 56.f, 56.f };  //Options popup close button rectangle.
        RectF muteToggleBtn{ 0.f, 0.f, 220.f, 58.f };   //Mute checkbox button rectangle.
        RectF exitPopup{ 0.f, 0.f, 520.f, 320.f };     //Exit confirmation popup rectangle.
        RectF exitCloseBtn{ 0.f, 0.f, 56.f, 56.f };    //Exit popup close button rectangle.
        RectF exitYesBtn{ 0.f, 0.f, 160.f, 64.f };     //Exit popup YES button.
        RectF exitNoBtn{ 0.f, 0.f, 160.f, 64.f };      //Exit popup NO button.
        RectF exitTitle{ 0.f, 0.f, 220.f, 80.f };      //Exit popup title texture rect.
        RectF exitPrompt{ 0.f, 0.f, 340.f, 96.f };     //Exit popup prompt texture rect.//Close button rectangle.
        bool layoutInitialized = false;
        void BuildGui();
        void BuildGui(float x, float bottomY, float w, float h, float spacing);
    };

} // namespace mygame
