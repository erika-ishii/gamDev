#include "MainMenuPage.hpp"
#include "Graphics/Graphics.hpp"
#include "Resource_Manager/Resource_Manager.h"
#include <glm/vec3.hpp>
#include <initializer_list>

using namespace mygame;

void MainMenuPage::Init(int screenW, int screenH)
{
    sw = screenW; sh = screenH;

    // load the background once (prefer RM cache; fall back to raw)
    
    auto resolveTexture = [](const std::initializer_list<std::string>& keys,
        const char* path) -> unsigned
        {
            for (const auto& key : keys) {
                if (unsigned tex = Resource_Manager::getTexture(key)) {
                    return tex;
                }
            }

            const std::string primaryKey = *keys.begin();
            if (Resource_Manager::load(primaryKey, path)) {
                if (unsigned tex = Resource_Manager::getTexture(primaryKey)) {
                    return tex;
                }
            }

            return gfx::Graphics::loadTexture(path);
        };

    menuBgTex = resolveTexture({ "menu_bg", "menu" }, "../../assets/Textures/menu.jpg");

    // load button textures (try cached keys first, then explicit load, finally raw GL load)
    startBtnIdleTex = resolveTexture({ "menu_start_btn", "start_btn", "start" },
        "../../assets/Textures/start_btn.png");
    startBtnHoverTex = startBtnIdleTex;

    exitBtnIdleTex = resolveTexture({ "menu_exit_btn", "exit_btn", "exit" },
        "../../assets/Textures/exit_btn.png");
    exitBtnHoverTex = exitBtnIdleTex;
   

    // build GUI buttons with callbacks that flip the latches
    gui.Clear();
    gui.AddButton(startBtn.x, startBtn.y, startBtn.w, startBtn.h, "Start",
        startBtnIdleTex, startBtnHoverTex,
        [this]() {
        startLatched = true;
        });
    gui.AddButton(exitBtn.x, exitBtn.y, exitBtn.w, exitBtn.h, "Exit",
        exitBtnIdleTex, exitBtnHoverTex,
        [this]() {
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
