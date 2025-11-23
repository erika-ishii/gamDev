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
#include "Core/PathUtils.h"
#include "Graphics/Graphics.hpp"
#include "Resource_Manager/Resource_Manager.h"
#include <algorithm>
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
    const std::string menuBgPath =
        Framework::ResolveAssetPath("Textures/UI/Start Menu/Start Menu Screen.jpg").string();
    menuBgTex = resolveTexture({ "menu_bg", "menu" }, menuBgPath.c_str());

    // --- Buttons ---
    const std::string startBtnPath =
        Framework::ResolveAssetPath("Textures/UI/Start Menu/Button_Start.PNG").string();
    startBtnIdleTex = resolveTexture({ "menu_start_btn_startmenu", "menu_start_btn", "start_btn", "start" },
        startBtnPath.c_str());
    startBtnHoverTex = startBtnIdleTex;

    const std::string optionsBtnPath =
        Framework::ResolveAssetPath("Textures/UI/Start Menu/Button_Options.PNG").string();
    optionsBtnIdleTex = resolveTexture({ "menu_options_btn_startmenu", "menu_options_btn", "options_btn", "options" },
        optionsBtnPath.c_str());
    optionsBtnHoverTex = optionsBtnIdleTex;

    const std::string howToBtnPath =
        Framework::ResolveAssetPath("Textures/UI/Start Menu/Button_How To Play.png").string();
    howToBtnIdleTex = resolveTexture({ "menu_howto_btn_startmenu", "menu_howto_btn", "howto_btn", "how_to_play" },
        howToBtnPath.c_str());
    howToBtnHoverTex = howToBtnIdleTex;

    const std::string exitBtnPath =
        Framework::ResolveAssetPath("Textures/UI/Start Menu/Button_Quit.PNG").string();
    exitBtnIdleTex = resolveTexture({ "menu_exit_btn_startmenu", "menu_exit_btn", "exit_btn", "exit" },
        exitBtnPath.c_str());
    exitBtnHoverTex = exitBtnIdleTex;

    const std::string closeBtnPath =
    Framework::ResolveAssetPath("Textures/UI/Pause Menu/XButton.png").string();
    closePopupTex = resolveTexture({ "menu_popup_close", "popup_close", "close_x" },
        closeBtnPath.c_str());

    SyncLayout(sw, sh);
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
    if (render) {
        SyncLayout(render->ScreenWidth(), render->ScreenHeight());
    }
    // 1) Background
    if (menuBgTex) {
        gfx::Graphics::renderFullscreenTexture(menuBgTex);
    }
    // 1.5) How To Play popup (background + text)
    if (showHowToPopup && render) {
        const float overlayAlpha = 0.65f;
        gfx::Graphics::renderRectangleUI(howToPopup.x, howToPopup.y,
            howToPopup.w, howToPopup.h,
            0.f, 0.f, 0.f, overlayAlpha,
            sw, sh);

        const float inset = 28.f;
        const float textX = howToPopup.x + inset;
        float textY = howToPopup.y + howToPopup.h - inset;
        const float lineHeight = 34.f;
        const float textScale = 0.6f;
        const glm::vec3 textColor(0.93f, 0.93f, 0.93f);
        if (render->IsTextReadyHint()) {
            render->GetTextHint().RenderText("HOW TO PLAY", textX, textY, textScale, textColor);
            textY -= lineHeight;
            render->GetTextHint().RenderText("- Move with WASD Keys", textX, textY, textScale, textColor);
            textY -= lineHeight;
            render->GetTextHint().RenderText("- Left Click for melee attack", textX, textY, textScale, textColor);
            textY -= lineHeight;
            render->GetTextHint().RenderText("- Right Click for range attack", textX, textY, textScale, textColor);
            textY -= lineHeight;
            render->GetTextHint().RenderText("- Pause anytime to adjust settings", textX, textY, textScale, textColor);
        }
    }

    // 2) GUI (buttons + any labels handled by the GUI system)
    gui.Draw(render);

    //// 3) Optional hint text
    //if (render && render->IsTextReadyHint()) {
    //    render->GetTextHint().RenderText("Click a button to continue",
    //        58.f, 108.f, 0.55f, glm::vec3(0.9f, 0.9f, 0.9f));
//}
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

bool MainMenuPage::ConsumeOptions()
{
    if (!optionsLatched) return false;
    optionsLatched = false;
    return true;
}

bool MainMenuPage::ConsumeHowToPlay()
{
    if (!howToLatched) return false;
    howToLatched = false;
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

void MainMenuPage::SyncLayout(int screenW, int screenH)
{
    if (layoutInitialized && screenW == sw && screenH == sh)
        return;


    sw = screenW;
    sh = screenH;

    // Maintain relative positions when the window/fullscreen size changes.
    const float baseW = 1280.f;
    const float baseH = 720.f;
    const float scaleX = sw / baseW;
    const float scaleY = sh / baseH;

    const float sizeScale = 0.60f; // Slightly shrink the buttons for breathing room
    const float btnW = 372.f * scaleX * sizeScale;   // Texture-native width
    const float btnH = 109.f * scaleY * sizeScale;   // Texture-native height
  
    const float verticalSpacing = 24.f * scaleY;
    const float blockHeight = btnH * 4.f + verticalSpacing * 3.f;
   
    const float downwardOffset = 180.f * scaleY; // Nudge stack downward a bit
    const float baseY = std::max(0.f, (sh - blockHeight) * 0.5f - downwardOffset);
    const float leftAlignedX = (sw - btnW) * 0.23f; // Lean the stack toward the left

    const float popupW = std::min(sw * 0.55f, 560.f * scaleX);
    const float popupH = std::min(sh * 0.6f, 360.f * scaleY);
    const float popupX = std::max(32.f * scaleX, leftAlignedX - 18.f * scaleX);
    const float popupY = std::min(sh - popupH - 32.f * scaleY, baseY + blockHeight + 32.f * scaleY);

    const float closeSize = 56.f * scaleX;
    closeBtn = { popupX + popupW - closeSize - 10.f * scaleX,
        popupY + popupH - closeSize - 10.f * scaleY,
        closeSize,
        56.f * scaleY };

    howToPopup = { popupX, popupY, popupW, popupH };

    startBtn = { leftAlignedX, baseY + (btnH + verticalSpacing) * 3.f, btnW, btnH };
    optionsBtn = { leftAlignedX, baseY + (btnH + verticalSpacing) * 2.f, btnW, btnH };
    howToBtn = { leftAlignedX, baseY + (btnH + verticalSpacing), btnW, btnH };
    exitBtn = { leftAlignedX, baseY, btnW, btnH };

    BuildGui();
    layoutInitialized = true;
}

void MainMenuPage::BuildGui()
{
    gui.Clear();
    gui.AddButton(startBtn.x, startBtn.y, startBtn.w, startBtn.h, "Start",
        startBtnIdleTex, startBtnHoverTex,
        [this]() { startLatched = true; });
    gui.AddButton(optionsBtn.x, optionsBtn.y, optionsBtn.w, optionsBtn.h, "Options",
        optionsBtnIdleTex, optionsBtnHoverTex,
        [this]() { optionsLatched = true; });

    gui.AddButton(howToBtn.x, howToBtn.y, howToBtn.w, howToBtn.h, "How To Play",
        howToBtnIdleTex, howToBtnHoverTex,
        [this]() { howToLatched = true; showHowToPopup = true; BuildGui(); });

    gui.AddButton(exitBtn.x, exitBtn.y, exitBtn.w, exitBtn.h, "Exit",
        exitBtnIdleTex, exitBtnHoverTex,
        [this]() { exitLatched = true; });
    if (showHowToPopup && closePopupTex) {
        gui.AddButton(closeBtn.x, closeBtn.y, closeBtn.w, closeBtn.h, "",
            closePopupTex, closePopupTex,
            [this]() { showHowToPopup = false; BuildGui(); }, true);
    }
}
