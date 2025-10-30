#pragma once
#include "Systems/GUISystem.hpp"
#include "Systems/RenderSystem.h"
#include "Systems/InputSystem.h"

namespace mygame {

    struct RectF { float x{}, y{}, w{}, h{}; };

    class MainMenuPage {
    public:
        void Init(int screenW, int screenH);
        void Update(Framework::InputSystem* input);
        void Draw(Framework::RenderSystem* render);

        bool ConsumeStart();
        bool ConsumeExit();

    private:
        // screen
        int sw = 1280, sh = 720;

        // bg
        unsigned menuBgTex = 0;

        // simple latches for transitions
        bool startLatched = false;
        bool exitLatched = false;

        // gui
        GUISystem gui;

        // layout for buttons (in your render/world space, origin bottom-left)
        RectF startBtn{ 100.f, 260.f, 220.f, 58.f };
        RectF exitBtn{ 100.f, 180.f, 220.f, 58.f };
    };

} // namespace mygame
