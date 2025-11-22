#pragma once
/*********************************************************************************************
 \file      PauseMenuPage.hpp
 \par       SofaSpuds
 \author    ChatGPT - Added pause overlay and navigation menu
 \brief     Declaration of an in-game pause menu with overlay, buttons, and latch events.
 \details   The pause page draws a dimmed overlay on top of the active scene, displays a
            stylized card for clarity, and surfaces three actions: Resume, Main Menu, and
            Quit. Button clicks set one-shot latches consumed by the game loop. Layout uses
            absolute coordinates (bottom-left origin) and the GUISystem for interaction.
*********************************************************************************************/

#include "Systems/GUISystem.hpp"
#include "Systems/InputSystem.h"
#include "Systems/RenderSystem.h"

namespace mygame {

    /*************************************************************************************
      \struct Quad
      \brief  Minimal rectangle helper for positioning UI elements.
    *************************************************************************************/
    struct Quad { float x{}, y{}, w{}, h{}; };

    /*************************************************************************************
      \class  PauseMenuPage
      \brief  Overlay pause menu with Resume/Main Menu/Quit actions.
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
        bool ConsumeQuit();

        // Reset internal one-shot latches (used when reopening the pause menu).
        void ResetLatches();

        // Rebuild layout if the screen size changes (keeps buttons aligned with cursor).
        void SyncLayout(int screenW, int screenH);
    private:
        int sw = 1280;
        int sh = 720;

   /*     unsigned panelTex = 0;
        unsigned resumeTex = 0;
        unsigned quitTex = 0;*/

        bool resumeLatched = false;
        bool mainMenuLatched = false;
        bool quitLatched = false;

        GUISystem gui;

        Quad panel{ 0.f, 0.f, 0.f, 0.f };
        Quad resumeBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad mainMenuBtn{ 0.f, 0.f, 0.f, 0.f };
        Quad quitBtn{ 0.f, 0.f, 0.f, 0.f };
    };

} // namespace mygame
