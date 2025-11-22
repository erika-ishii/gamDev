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
        unsigned exitBtnIdleTex = 0;  //Exit button idle texture.
        unsigned exitBtnHoverTex = 0;  //Exit button hover texture.

        // --- Latches ---------------------------------------------------------------------
        bool startLatched = false;     
        bool exitLatched = false;     

        // --- GUI system ------------------------------------------------------------------
        GUISystem gui;                 //Immediate-mode GUI owner for menu buttons.

        // --- Layout (origin: bottom-left) ------------------------------------------------
        RectF startBtn{ 100.f, 260.f, 220.f, 58.f }; //Start button rectangle.
        RectF exitBtn{ 100.f, 180.f, 220.f, 58.f }; //Exit button rectangle.
        void BuildGui();
    };

} // namespace mygame
