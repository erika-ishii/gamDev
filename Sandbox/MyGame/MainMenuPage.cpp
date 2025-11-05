/*********************************************************************************************
 \file      MainMenuPage.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Simple main-menu screen with cached texture resolution and image-button GUI.
 \details   This module draws the game’s main menu:
            - Background: a fullscreen texture drawn behind the UI.
            - Buttons: Start / Exit buttons with idle/hover textures and click callbacks.
            - Latches: boolean flags (startLatched/exitLatched) consumed by the game loop.
            - Hint text: optional short instruction rendered via RenderSystem’s hint text.

            Texture loading follows a "resolveTexture" strategy:
            1) Try a list of Resource_Manager keys (cached texture handles).
            2) If missing, attempt Resource_Manager::load(primaryKey, path).
            3) As a final fallback, load directly via gfx::Graphics::loadTexture(path).

            Typical usage:
              - Call Init(sw, sh) once after creating the page.
              - Call Update(input) each frame to process hover/click.
              - Call Draw(render) after Update to render background + GUI.
              - Poll ConsumeStart()/ConsumeExit() in the outer loop to act once per click.

 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "MainMenuPage.hpp"
#include "Graphics/Graphics.hpp"
#include "Resource_Manager/Resource_Manager.h"
#include <glm/vec3.hpp>
#include <initializer_list>
#include <string>

using namespace mygame;

/*************************************************************************************
  \brief  Initialize menu textures, build GUI buttons, and set screen size.
  \param  screenW Width of the window/swapchain in pixels.
  \param  screenH Height of the window/swapchain in pixels.
  \note   Textures are resolved via Resource_Manager first, then fall back to raw load.
*************************************************************************************/
void MainMenuPage::Init(int screenW, int screenH)
{
    sw = screenW;
    sh = screenH;

    // Load the background once (prefer RM cache; fall back to raw).
    auto resolveTexture = [](const std::initializer_list<std::string>& keys,
        const char* path) -> unsigned
        {
            // 1) Try any existing cached keys
            for (const auto& key : keys) {
                if (unsigned tex = Resource_Manager::getTexture(key)) {
                    return tex;
                }
            }

            // 2) Attempt to load using the *first* key as the canonical ID
            const std::string primaryKey = *keys.begin();
            if (Resource_Manager::load(primaryKey, path)) {
                if (unsigned tex = Resource_Manager::getTexture(primaryKey)) {
                    return tex;
                }
            }

            // 3) Final fallback: raw GL texture load (no RM caching)
            return gfx::Graphics::loadTexture(path);
        };

    // --- Background ---
    menuBgTex = resolveTexture({ "menu_bg", "menu" }, "../../assets/Textures/menu.jpg");

    // --- Buttons (idle/hover share the same asset here; replace if you have distinct ones) ---
    startBtnIdleTex = resolveTexture({ "menu_start_btn", "start_btn", "start" },
        "../../assets/Textures/start_btn.png");
    startBtnHoverTex = startBtnIdleTex;

    exitBtnIdleTex = resolveTexture({ "menu_exit_btn", "exit_btn", "exit" },
        "../../assets/Textures/exit_btn.png");
    exitBtnHoverTex = exitBtnIdleTex;

    // --- Build GUI buttons with callbacks that flip latches ---
    gui.Clear();
    gui.AddButton(startBtn.x, startBtn.y, startBtn.w, startBtn.h, "Start",
        startBtnIdleTex, startBtnHoverTex,
        [this]() { startLatched = true; });

    gui.AddButton(exitBtn.x, exitBtn.y, exitBtn.w, exitBtn.h, "Exit",
        exitBtnIdleTex, exitBtnHoverTex,
        [this]() { exitLatched = true; });
}

/*************************************************************************************
  \brief  Update interactive state (hover/click) for menu widgets.
  \param  input Pointer to the engine input system.
*************************************************************************************/
void MainMenuPage::Update(Framework::InputSystem* input)
{
    gui.Update(input);
}

/*************************************************************************************
  \brief  Draw the fullscreen background, then the GUI, then optional hint text.
  \param  render Pointer to RenderSystem for text rendering (optional).
  \note   Background draw uses gfx::Graphics::renderFullscreenTexture(tex).
*************************************************************************************/
void MainMenuPage::Draw(Framework::RenderSystem* render)
{
    // 1) Background
    if (menuBgTex) {
        gfx::Graphics::renderFullscreenTexture(menuBgTex);
    }

    // 2) GUI (buttons + any labels handled by the GUI system)
    gui.Draw(render);

    // 3) Optional hint text
    if (render && render->IsTextReadyHint()) {
        render->GetTextHint().RenderText("Click a button to continue",
            58.f, 108.f, 0.55f, glm::vec3(0.9f, 0.9f, 0.9f));
    }
}

/*************************************************************************************
  \brief  Consume the Start latch (true once after the user clicks Start).
  \return true if Start was just triggered; false otherwise.
*************************************************************************************/
bool MainMenuPage::ConsumeStart()
{
    if (!startLatched) return false;
    startLatched = false;
    return true;
}

/*************************************************************************************
  \brief  Consume the Exit latch (true once after the user clicks Exit).
  \return true if Exit was just triggered; false otherwise.
*************************************************************************************/
bool MainMenuPage::ConsumeExit()
{
    if (!exitLatched)  return false;
    exitLatched = false;
    return true;
}
