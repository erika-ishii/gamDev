/*********************************************************************************************
 \file      DefeatScreenPage.cpp
 \par       SofaSpuds
 \author     elvisshengjie.lim (elvisshengjie.lim@digipen.edu) -  Author, 100%
 \brief     Implements the defeat overlay and restart button for non-editor gameplay.
*********************************************************************************************/

#include "DefeatScreenPage.hpp"

#include "Core/PathUtils.h"
#include "Graphics/Graphics.hpp"
#include "Resource_Manager/Resource_Manager.h"

#include <algorithm>
#include <string>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
using namespace mygame;

namespace
{
    unsigned ResolveTexture(const std::string& key, const std::string& relativePath)
    {
        if (unsigned tex = Resource_Manager::getTexture(key))
            return tex;

        std::string path = Framework::ResolveAssetPath(relativePath).string();
        if (Resource_Manager::load(key, path))
            return Resource_Manager::getTexture(key);

        return gfx::Graphics::loadTexture(path.c_str());
    }
}

void DefeatScreenPage::Init(int screenW, int screenH)
{
    sw = screenW;
    sh = screenH;

    defeatScreenTex = ResolveTexture("defeat_screen", "Textures/UI/Defeat Menu/Defeat screen.jpg");
    tryAgainTex = ResolveTexture("defeat_try_again", "Textures/UI/Defeat Menu/Try again.png");

    SyncLayout(screenW, screenH);
    BuildGui();
}

void DefeatScreenPage::SyncLayout(int screenW, int screenH)
{
    sw = screenW;
    sh = screenH;
    layoutDirty = true;

    const float aspect = 16.f / 9.f;
    float panelW = static_cast<float>(sw) ;
    float panelH = panelW / aspect;

    const float maxH = static_cast<float>(sh);
    if (panelH > maxH)
    {
        panelH = maxH;
        panelW = panelH * aspect;
    }

    panel.w = panelW;
    panel.h = panelH;
    panel.x = (static_cast<float>(sw) - panelW) * 0.5f;
    panel.y = (static_cast<float>(sh) - panelH) * 0.5f;

    tryAgainBtn.w = panelW * 0.22f;
    tryAgainBtn.h = tryAgainBtn.w * 0.36f;
    tryAgainBtn.x = (sw - tryAgainBtn.w) * 0.5f;
    tryAgainBtn.y = sh * 0.08f;

    BuildGui();
}

void DefeatScreenPage::BuildGui()
{
    gui.Clear();
    gui.AddButton(tryAgainBtn.x, tryAgainBtn.y, tryAgainBtn.w, tryAgainBtn.h,
        "Try Again", tryAgainTex, tryAgainTex,
        [this]() { tryAgainLatched = true; }, false);
    layoutDirty = false;
}

void DefeatScreenPage::Update(Framework::InputSystem* input)
{
    if (layoutDirty)
        BuildGui();

    gui.Update(input);
}

void DefeatScreenPage::Draw(Framework::RenderSystem* render)
{
    if (!render)
        return;

    const int screenW = render->ScreenWidth();
    const int screenH = render->ScreenHeight();

    if (screenW != sw || screenH != sh)
        SyncLayout(screenW, screenH);

    gfx::Graphics::renderRectangleUI(0.f, 0.f,
        static_cast<float>(screenW), static_cast<float>(screenH),
        0.f, 0.f, 0.f, 0.55f, screenW, screenH);

    if (defeatScreenTex)
    {
        gfx::Graphics::renderSpriteUI(defeatScreenTex, panel.x, panel.y, panel.w, panel.h,
            1.f, 1.f, 1.f, 1.f, screenW, screenH);
    }

    gui.Draw(render);
}

bool DefeatScreenPage::ConsumeTryAgain()
{
    const bool triggered = tryAgainLatched;
    tryAgainLatched = false;
    return triggered;
}

void DefeatScreenPage::ResetLatches()
{
    tryAgainLatched = false;
}