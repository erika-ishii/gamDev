#pragma once
/*********************************************************************************************
 \file      DefeatScreenPage.hpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) -  Author, 100%
 \brief     Simple defeat overlay shown when the player dies outside of the editor.
 *********************************************************************************************/

#include "Systems/GUISystem.hpp"
#include "Systems/InputSystem.h"
#include "Systems/RenderSystem.h"

namespace mygame
{
    struct Quads { float x{}, y{}, w{}, h{}; };

    class DefeatScreenPage
    {
    public:
        void Init(int screenW, int screenH);
        void Update(Framework::InputSystem* input);
        void Draw(Framework::RenderSystem* render);

        bool ConsumeTryAgain();
        void ResetLatches();

        void SyncLayout(int screenW, int screenH);
        void BuildGui();

    private:
        int sw = 1280;
        int sh = 720;
        bool layoutDirty = true;
        bool tryAgainLatched = false;

        unsigned defeatScreenTex = 0;
        unsigned tryAgainTex = 0;

        Quads panel{ 0.f, 0.f, 0.f, 0.f };
        Quads tryAgainBtn{ 0.f, 0.f, 0.f, 0.f };

        GUISystem gui;
    };

} // namespace mygame