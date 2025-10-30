#include "MainMenuPage.hpp"
#include "Graphics/Graphics.hpp"
#include "Resource_Manager/Resource_Manager.h"
#include <glm/vec3.hpp>

using namespace mygame;

void MainMenuPage::Init(int screenW, int screenH)
{
    sw = screenW; sh = screenH;

    // load the background once (prefer RM cache; fall back to raw)
    Resource_Manager::load("menu_bg", "../../assets/Textures/menu.jpg");
    menuBgTex = Resource_Manager::resources_map["menu_bg"].handle;
    if (!menuBgTex) {
        menuBgTex = gfx::Graphics::loadTexture("../../assets/Textures/menu.jpg");
    }

    // build GUI buttons with callbacks that flip the latches
    gui.Clear();
    gui.AddButton(startBtn.x, startBtn.y, startBtn.w, startBtn.h, "Start", [this]() {
        startLatched = true;
        });
    gui.AddButton(exitBtn.x, exitBtn.y, exitBtn.w, exitBtn.h, "Exit", [this]() {
        exitLatched = true;
        });
}

void MainMenuPage::Update(Framework::InputSystem* input)
{
    gui.Update(input);
}

void MainMenuPage::Draw(Framework::RenderSystem* render)
{
    // draw background first
    if (menuBgTex) {
        gfx::Graphics::renderFullscreenTexture(menuBgTex);
    }

    // then GUI (buttons + labels the GUISystem renders)
    gui.Draw(render);

    // optional title / hint text using your text renderers
   
    if (render && render->IsTextReadyHint()) {
        render->GetTextHint().RenderText("Click a button to continue",
            58.f, 108.f, 0.55f, glm::vec3(0.9f, 0.9f, 0.9f));
    }
}

bool MainMenuPage::ConsumeStart() { if (!startLatched) return false; startLatched = false; return true; }
bool MainMenuPage::ConsumeExit() { if (!exitLatched)  return false; exitLatched = false; return true; }
