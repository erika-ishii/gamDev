/*********************************************************************************************
 \file      PauseMenuPage.cpp
 \par       SofaSpuds
 \author    
 \brief     In-game pause menu: overlay card with Resume/Main Menu/Quit buttons.
 \details   Provides a lightweight pause UI that dims the active scene, displays a textured
            card for legibility, and exposes three latch-based actions. Textures are resolved
            via Resource_Manager first, then fall back to raw gfx::Graphics loads. Layout is
            responsive to the current screen size passed to Init().
*********************************************************************************************/

#include "PauseMenuPage.hpp"
#include "Graphics/Graphics.hpp"
#include "Resource_Manager/Resource_Manager.h"
#include <glm/vec3.hpp>
#include <initializer_list>
#include <string>

using namespace mygame;

//namespace {
//    // Resolve a texture by trying cached keys, loading via Resource_Manager, then falling back.
//    unsigned ResolveTexture(const std::initializer_list<std::string>& keys, const char* path)
//    {
//        for (const auto& key : keys) {
//            if (unsigned tex = Resource_Manager::getTexture(key)) {
//                return tex;
//            }
//        }
//
//        const std::string primaryKey = *keys.begin();
//        if (Resource_Manager::load(primaryKey, path)) {
//            if (unsigned tex = Resource_Manager::getTexture(primaryKey)) {
//                return tex;
//            }
//        }
//
//        return gfx::Graphics::loadTexture(path);
//    }
//}

void PauseMenuPage::Init(int screenW, int screenH)
{
   
    SyncLayout(screenW, screenH);
    ResetLatches();
}

void PauseMenuPage::Update(Framework::InputSystem* input)
{
    gui.Update(input);
}

void PauseMenuPage::Draw(Framework::RenderSystem* render)
{
    const int screenW = render ? render->ScreenWidth() : sw;
    const int screenH = render ? render->ScreenHeight() : sh;
    SyncLayout(screenW, screenH);


    gfx::Graphics::renderRectangleUI(0.f, 0.f, static_cast<float>(screenW), static_cast<float>(screenH),
        0.f, 0.f, 0.f, 0.55f, screenW, screenH);

    /*const float glowExpand = 14.f;
    gfx::Graphics::renderRectangleUI(panel.x - glowExpand, panel.y - glowExpand,
        panel.w + glowExpand * 2.f, panel.h + glowExpand * 2.f,
        0.05f, 0.08f, 0.12f, 0.35f, screenW, screenH);

    if (panelTex) {
        gfx::Graphics::renderSpriteUI(panelTex, panel.x, panel.y, panel.w, panel.h,
            1.f, 1.f, 1.f, 0.9f, screenW, screenH);
    }
    else {
        gfx::Graphics::renderRectangleUI(panel.x, panel.y, panel.w, panel.h,
            0.12f, 0.14f, 0.18f, 0.92f, screenW, screenH);
    }

    gfx::Graphics::renderRectangleUI(panel.x, panel.y, panel.w, 3.f,
        0.62f, 0.78f, 0.92f, 1.f, screenW, screenH);*/

        // Draw a simple flat panel so the pause view stays minimal and distinct from the main menu.
    gfx::Graphics::renderRectangleUI(panel.x, panel.y, panel.w, panel.h,
        0.12f, 0.14f, 0.18f, 0.90f, screenW, screenH);

    if (render && render->IsTextReadyTitle()) {
        render->GetTextTitle().RenderText("Paused", panel.x + 32.f, panel.y + panel.h - 56.f,
            1.0f, glm::vec3(0.95f, 0.97f, 0.99f));
    }

    if (render && render->IsTextReadyHint()) {
        render->GetTextHint().RenderText("Press ESC or Start to resume",
            panel.x + 36.f, panel.y + panel.h - 86.f,
            0.65f, glm::vec3(0.86f, 0.88f, 0.92f));

        render->GetTextHint().RenderText("Use the buttons to navigate",
            panel.x + 36.f, panel.y + 22.f,
            0.58f, glm::vec3(0.78f, 0.82f, 0.88f));
    }

    gui.Draw(render);
}

bool PauseMenuPage::ConsumeResume()
{
    if (!resumeLatched) return false;
    resumeLatched = false;
    return true;
}

bool PauseMenuPage::ConsumeMainMenu()
{
    if (!mainMenuLatched) return false;
    mainMenuLatched = false;
    return true;
}

bool PauseMenuPage::ConsumeQuit()
{
    if (!quitLatched) return false;
    quitLatched = false;
    return true;
}

void PauseMenuPage::ResetLatches()
{
    resumeLatched = false;
    mainMenuLatched = false;
    quitLatched = false;
}
void PauseMenuPage::SyncLayout(int screenW, int screenH)
{
    if (screenW == sw && screenH == sh)
        return;

    sw = screenW;
    sh = screenH;

    const float panelW = static_cast<float>(sw) * 0.42f;
    const float panelH = static_cast<float>(sh) * 0.46f;
    panel = { (static_cast<float>(sw) - panelW) * 0.5f, (static_cast<float>(sh) - panelH) * 0.55f, panelW, panelH };

    resumeBtn = { panel.x + 40.f, panel.y + panel.h - 120.f, panel.w - 80.f, 68.f };
    quitBtn = { panel.x + 40.f, panel.y + 40.f, panel.w - 80.f, 64.f };

    gui.Clear();
    gui.AddButton(resumeBtn.x, resumeBtn.y, resumeBtn.w, resumeBtn.h, "Resume",
        [this]() { resumeLatched = true; });

    gui.AddButton(quitBtn.x, quitBtn.y, quitBtn.w, quitBtn.h, "Quit",
        [this]() { quitLatched = true; });
}
