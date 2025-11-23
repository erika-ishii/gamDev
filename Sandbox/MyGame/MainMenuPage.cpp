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
#include <filesystem>
#include <fstream>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>
#include <initializer_list>
#include <string>
#include <vector>
#include <json.hpp>

using namespace mygame;
namespace {
    struct TextureField {
        std::string key;
        std::string path;
    };

    struct HowToRowJson {
        TextureField icon;
        TextureField label;
        int frames = 0;           // 0 = derive from strip size
        float fps = 8.0f;
        float iconAspect = 1.0f;
        float labelAspect = 1.0f;
        int cols = 0;             // 0 = derive from frames
        int rows = 0;             // 0 or less = treated as 1 in Init
    };

    struct HowToPopupJson {
        TextureField background;
        TextureField header;
        TextureField close;
        std::vector<HowToRowJson> rows;
    };

    TextureField MakeTextureField(const std::string& key, const std::string& path)
    {
        return TextureField{ key, path };
    }

    HowToPopupJson DefaultHowToPopupConfig()
    {
        HowToPopupJson config{};
        config.background = MakeTextureField("howto_note_bg", "Textures/UI/How To Play/Note.png");
        config.header = MakeTextureField("howto_header", "Textures/UI/How To Play/How To Play.png");
        config.close = MakeTextureField("menu_popup_close", "Textures/UI/How To Play/XButton.png");

        config.rows = {
            { MakeTextureField("howto_wasd_icon", "Textures/UI/How To Play/WASD_Sprite.png"),
                MakeTextureField("howto_wasd_label", "Textures/UI/How To Play/WASD to move.png"),
                0, 8.0f, 1.05f, 3.4f },
            { MakeTextureField("howto_esc_icon", "Textures/UI/How To Play/ESC_Sprite.png"),
                MakeTextureField("howto_esc_label", "Textures/UI/How To Play/Esc to pause.png"),
                0, 8.0f, 1.8f, 2.8f },
            { MakeTextureField("howto_melee_icon", "Textures/UI/How To Play/Left_Mouse_Sprite.png"),
                MakeTextureField("howto_melee_label", "Textures/UI/How To Play/For melee attack.png"),
                0, 8.0f, 0.72f, 3.1f },
            { MakeTextureField("howto_range_icon", "Textures/UI/How To Play/Right_Mouse_Sprite.png"),
                MakeTextureField("howto_range_label", "Textures/UI/How To Play/For Range attack.png"),
                0, 8.0f, 0.72f, 3.6f },
        };

        return config;
    }

    bool PopulateTextureField(const nlohmann::json& obj, TextureField& out)
    {
        if (obj.contains("key")) out.key = obj["key"].get<std::string>();
        if (obj.contains("path")) out.path = obj["path"].get<std::string>();
        return obj.contains("key") || obj.contains("path");
    }

    HowToPopupJson LoadHowToPopupConfig()
    {
        HowToPopupJson config = DefaultHowToPopupConfig();

        std::vector<std::filesystem::path> candidates;
        candidates.emplace_back(Framework::ResolveDataPath("howto_popup.json"));
        candidates.emplace_back(Framework::ResolveDataPath("HowToPopup.json"));
        candidates.emplace_back(std::filesystem::path("Data_Files") / "howto_popup.json");

        for (const auto& path : candidates)
        {
            std::ifstream stream(path);
            if (!stream.is_open())
                continue;

            nlohmann::json j;
            stream >> j;
            if (!j.contains("howToPopup"))
                return config;

            const nlohmann::json& root = j["howToPopup"];
            if (root.contains("background"))
                PopulateTextureField(root["background"], config.background);
            if (root.contains("header"))
                PopulateTextureField(root["header"], config.header);
            if (root.contains("close"))
                PopulateTextureField(root["close"], config.close);

            if (root.contains("rows") && root["rows"].is_array())
            {
                std::vector<HowToRowJson> rows;
                rows.reserve(root["rows"].size());
                for (size_t i = 0; i < root["rows"].size(); ++i)
                {
                    const auto& rowJson = root["rows"][i];
                    HowToRowJson row = (i < config.rows.size()) ? config.rows[i] : HowToRowJson{};
                    if (rowJson.contains("icon"))
                        PopulateTextureField(rowJson["icon"], row.icon);
                    if (rowJson.contains("label"))
                        PopulateTextureField(rowJson["label"], row.label);
                    if (rowJson.contains("frames"))
                        row.frames = rowJson["frames"].get<int>();
                    if (rowJson.contains("fps"))
                        row.fps = rowJson["fps"].get<float>();
                    if (rowJson.contains("iconAspect"))
                        row.iconAspect = rowJson["iconAspect"].get<float>();
                    if (rowJson.contains("labelAspect"))
                        row.labelAspect = rowJson["labelAspect"].get<float>();
                    if (rowJson.contains("cols"))
                        row.cols = rowJson["cols"].get<int>();
                    if (rowJson.contains("rows"))
                        row.rows = rowJson["rows"].get<int>();
                    rows.push_back(row);
                }
                if (!rows.empty())
                    config.rows = std::move(rows);
            }
            break;
        }

        return config;
    }
}

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

    const HowToPopupJson popupConfig = LoadHowToPopupConfig();
    const std::string closeBtnPath =
        Framework::ResolveAssetPath(popupConfig.close.path).string();
    closePopupTex = resolveTexture({ popupConfig.close.key, "menu_popup_close", "popup_close", "close_x" },
        closeBtnPath.c_str());

    const std::string noteBgPath =
        Framework::ResolveAssetPath(popupConfig.background.path).string();
    noteBackgroundTex = resolveTexture({ popupConfig.background.key, "howto_note_bg", "note_bg" }, noteBgPath.c_str());

    const std::string headerPath =
        Framework::ResolveAssetPath(popupConfig.header.path).string();
    howToHeaderTex = resolveTexture({ popupConfig.header.key, "howto_header", "howto_title" }, headerPath.c_str());

    howToRows.clear();
    auto frameCountFromStrip = [](unsigned tex) {
        int texW = 0, texH = 0;
        if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0) {
            const int frames = texW / texH;
            return std::max(1, frames);
        }
        return 1;
        };

    for (const auto& row : popupConfig.rows)
    {
        HowToRowConfig rowCfg{};
        const std::string iconPath = Framework::ResolveAssetPath(row.icon.path).string();
        rowCfg.iconTex = resolveTexture({ row.icon.key, "howto_icon" }, iconPath.c_str());
        const std::string labelPath = Framework::ResolveAssetPath(row.label.path).string();
        rowCfg.labelTex = resolveTexture({ row.label.key, "howto_label" }, labelPath.c_str());

        const int derivedFrames = (row.frames > 0) ? row.frames : frameCountFromStrip(rowCfg.iconTex);
        const int derivedCols = (row.cols > 0) ? row.cols : derivedFrames;
        const int derivedRows = (row.rows > 0) ? row.rows : 1;

        rowCfg.cols = std::max(1, derivedCols);
        rowCfg.rows = std::max(1, derivedRows);

        const int maxFrames = rowCfg.cols * rowCfg.rows;
        rowCfg.frameCount = std::clamp(std::max(1, derivedFrames), 1, maxFrames);
        rowCfg.fps = (row.fps > 0.0f) ? row.fps : 8.0f;
        rowCfg.iconAspectFallback = row.iconAspect;
        rowCfg.labelAspectFallback = row.labelAspect;
        howToRows.push_back(rowCfg);
    }

    iconAnimTime = 0.0f;
    iconTimerInitialized = false;

    SyncLayout(sw, sh);
}



/*************************************************************************************
  \brief  Update interactive state (hover/click) for menu widgets.
  \param  input Pointer to the engine input system.
*************************************************************************************/
void MainMenuPage::Update(Framework::InputSystem* input)
{
    const auto now = std::chrono::steady_clock::now();
    if (showHowToPopup) {
        if (!iconTimerInitialized) {
            lastIconTick = now;
            iconTimerInitialized = true;
        }
        else {
            const std::chrono::duration<float> delta = now - lastIconTick;
            iconAnimTime += delta.count();
            lastIconTick = now;
        }
    }
    else {
        iconTimerInitialized = false;
    }
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
    // 1.5) How To Play popup (illustrated note)
    if (showHowToPopup && render) {
        const float overlayAlpha = 0.65f;
        gfx::Graphics::renderRectangleUI(0.f, 0.f,
            static_cast<float>(sw), static_cast<float>(sh),
            0.f, 0.f, 0.f, overlayAlpha,
            sw, sh);

        auto textureAspect = [](unsigned tex, float fallback) {
            int texW = 0, texH = 0;
            if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0) {
                return static_cast<float>(texW) / static_cast<float>(texH);
            }
            return fallback;
            };

        // Draw the parchment note background.
        if (noteBackgroundTex) {
            gfx::Graphics::renderSpriteUI(noteBackgroundTex, howToPopup.x, howToPopup.y,
                howToPopup.w, howToPopup.h,
                1.f, 1.f, 1.f, 1.f,
                sw, sh);
        }
        else {
            gfx::Graphics::renderRectangleUI(howToPopup.x, howToPopup.y,
                howToPopup.w, howToPopup.h,
                0.1f, 0.08f, 0.05f, 0.95f,
                sw, sh);
        }

        const float headerPadY = howToPopup.h * 0.07f;
        const float headerHeight = howToPopup.h * 0.16f;
        const float headerAspect = textureAspect(howToHeaderTex, 2.6f);
        const float headerWidth = headerHeight * headerAspect;
        const float headerX = howToPopup.x + (howToPopup.w - headerWidth) * 0.5f;
        const float headerY = howToPopup.y + howToPopup.h - headerHeight - headerPadY;
        if (howToHeaderTex) {
            gfx::Graphics::renderSpriteUI(howToHeaderTex, headerX, headerY,
                headerWidth, headerHeight,
                1.f, 1.f, 1.f, 1.f,
                sw, sh);
        }

        const float contentTop = headerY - howToPopup.h * 0.04f;
        const float contentBottom = howToPopup.y + howToPopup.h * 0.08f;
        const float availableHeight = std::max(0.1f, contentTop - contentBottom);
        const size_t rowCount = std::max<size_t>(1, howToRows.size());
        const float rowHeight = availableHeight / static_cast<float>(rowCount);

        const float iconHeightBase = rowHeight * 0.78f;
        const float labelHeightBase = rowHeight * 0.58f;
        const float leftPad = howToPopup.w * 0.16f;
        const float rightPad = howToPopup.w * 0.14f;
        const float labelX = howToPopup.x + leftPad;
        const float iconAnchorX = howToPopup.x + howToPopup.w - rightPad;
        const glm::mat4 uiOrtho = glm::ortho(0.0f, static_cast<float>(sw), 0.0f, static_cast<float>(sh), -1.0f, 1.0f);
        gfx::Graphics::setViewProjection(glm::mat4(1.0f), uiOrtho);

        for (size_t i = 0; i < howToRows.size(); ++i) {
            const float sizeScale = (i < 2) ? 0.94f : 1.0f;
            const float iconHeight = iconHeightBase * sizeScale;
            const float labelHeight = labelHeightBase * sizeScale;
            const float rowBaseY = contentTop - rowHeight * (static_cast<float>(i) + 1.f);
            const float iconY = rowBaseY + (rowHeight - iconHeight) * 0.5f;
            const float labelY = rowBaseY + (rowHeight - labelHeight) * 0.5f;

            const int frames = std::max(1, howToRows[i].frameCount);
            const int cols = std::max(1, howToRows[i].cols);
            const int rows = std::max(1, howToRows[i].rows);
            const float iconAspect = textureAspect(howToRows[i].iconTex, howToRows[i].iconAspectFallback) *
                (static_cast<float>(rows) / static_cast<float>(cols));
            const float iconW = iconHeight * iconAspect;
            const float iconNudgeLeft = (i < 2) ? howToPopup.w * 0.01f : 0.0f;
            const float iconX = iconAnchorX - iconW - iconNudgeLeft;
            if (howToRows[i].iconTex) {
                const float fps = howToRows[i].fps > 0.0f ? howToRows[i].fps : 8.0f;
                const int frameIndex = (frames > 1)
                    ? (static_cast<int>(iconAnimTime * fps) % frames)
                    : 0;

                gfx::Graphics::renderSpriteFrame(howToRows[i].iconTex,
                    iconX + iconW * 0.5f, iconY + iconHeight * 0.5f,
                    0.f, iconW, iconHeight,
                    frameIndex, cols, rows,
                    1.f, 1.f, 1.f, 1.f);
            }
            const float labelAspect = textureAspect(howToRows[i].labelTex, howToRows[i].labelAspectFallback);
            const float labelW = labelHeight * labelAspect;
            if (howToRows[i].labelTex) {
                gfx::Graphics::renderSpriteUI(howToRows[i].labelTex, labelX, labelY,
                    labelW, labelHeight,
                    1.f, 1.f, 1.f, 1.f,
                    sw, sh);
            }
        }
        gfx::Graphics::resetViewProjection();
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

    const float defaultNoteAspect = 0.75f;
    int noteW = 0, noteH = 0;
    const bool hasNoteSize = noteBackgroundTex && gfx::Graphics::getTextureSize(noteBackgroundTex, noteW, noteH);
    const float noteAspect = (hasNoteSize && noteH > 0) ? static_cast<float>(noteW) / static_cast<float>(noteH) : defaultNoteAspect;

    float popupW = sw * 0.58f;
    float popupH = popupW / noteAspect;
    const float maxPopupH = sh * 0.82f;
    if (popupH > maxPopupH) {
        popupH = maxPopupH;
        popupW = popupH * noteAspect;
    }

    const float popupX = (sw - popupW) * 0.58f;
    const float popupY = (sh - popupH) * 0.5f;
    const float closeSize = std::min(popupW, popupH) * 0.14f;
    closeBtn = { popupX + popupW - closeSize * 0.85f,
        popupY + popupH - closeSize * 0.75f,
        closeSize,
        closeSize
    };

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
        [this]() {
            howToLatched = true;
            showHowToPopup = true;
            iconAnimTime = 0.0f;
            iconTimerInitialized = false;
            BuildGui();
        });

    gui.AddButton(exitBtn.x, exitBtn.y, exitBtn.w, exitBtn.h, "Exit",
        exitBtnIdleTex, exitBtnHoverTex,
        [this]() { exitLatched = true; });
    if (showHowToPopup && closePopupTex) {
        gui.AddButton(closeBtn.x, closeBtn.y, closeBtn.w, closeBtn.h, "",
            closePopupTex, closePopupTex,
            [this]() { showHowToPopup = false; BuildGui(); }, true);
    }
}
