/*********************************************************************************************
 \file      PauseMenuPage.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     In-game pause menu: parchment overlay with stylized buttons.
 \details   Provides a themed pause UI that dims the active scene, displays a textured
            note card for legibility, and exposes Resume/Options/How To Play/Quit plus a
            close button. Textures are resolved via Resource_Manager first, then fall back
            to raw gfx::Graphics loads. Layout is responsive to the current screen size
            passed to Init().
*********************************************************************************************/

#include "PauseMenuPage.hpp"
#include "Graphics/Graphics.hpp"
#include "Resource_Manager/Resource_Manager.h"
#include "Audio/SoundManager.h"
#include "Core/PathUtils.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <json.hpp>
#include <initializer_list>
#include <string>
#include <vector>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
using namespace mygame;

namespace{
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
    struct ExitPopupJson {
        TextureField background;
        TextureField title;
        TextureField prompt;
        TextureField close;
        TextureField yes;
        TextureField no;
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
                 0, 8.0f, 0.9f, 2.6f },
            { MakeTextureField("howto_esc_icon", "Textures/UI/How To Play/ESC_Sprite.png"),
                MakeTextureField("howto_esc_label", "Textures/UI/How To Play/Esc to pause.png"),
                 0, 8.0f, 1.05f, 3.1f },
            { MakeTextureField("howto_melee_icon", "Textures/UI/How To Play/Left_Mouse_Sprite.png"),
                MakeTextureField("howto_melee_label", "Textures/UI/How To Play/For melee attack.png"),
                0, 8.0f, 0.72f, 3.1f },
            { MakeTextureField("howto_range_icon", "Textures/UI/How To Play/Right_Mouse_Sprite.png"),
                MakeTextureField("howto_range_label", "Textures/UI/How To Play/For Range attack.png"),
                0, 8.0f, 0.72f, 3.1f },
        };

        return config;
    }

    ExitPopupJson DefaultExitPopupConfig()
    {
        ExitPopupJson config{};
        config.background = MakeTextureField("exit_popup_note", "Textures/UI/How To Play/Note.png");
        config.title = MakeTextureField("exit_popup_title", "Textures/UI/How To Play/Exit.png");
        config.prompt = MakeTextureField("exit_popup_prompt", "Textures/UI/How To Play/Are you sure.png");
        config.close = MakeTextureField("exit_popup_close", "Textures/UI/How To Play/XButton.png");
        config.yes = MakeTextureField("exit_popup_yes", "Textures/UI/How To Play/Yes.png");
        config.no = MakeTextureField("exit_popup_no", "Textures/UI/How To Play/No.png");
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
                    config.rows = rows;
            }
            return config;
        }

        return config;
    }
    ExitPopupJson LoadExitPopupConfig()
    {
        ExitPopupJson config = DefaultExitPopupConfig();

        std::vector<std::filesystem::path> candidates;
        candidates.emplace_back(Framework::ResolveDataPath("exit_popup.json"));
        candidates.emplace_back(std::filesystem::path("assets/data/exit_popup.json"));
        candidates.emplace_back(std::filesystem::path("Data_Files") / "exit_popup.json");

        for (const auto& path : candidates)
        {
            std::ifstream stream(path);
            if (!stream.is_open())
                continue;

            try
            {
                nlohmann::json j;
                stream >> j;
                if (!j.contains("exitPopup"))
                    continue;

                const auto& root = j["exitPopup"];
                if (root.contains("background"))
                    PopulateTextureField(root["background"], config.background);
                if (root.contains("title"))
                    PopulateTextureField(root["title"], config.title);
                if (root.contains("prompt"))
                    PopulateTextureField(root["prompt"], config.prompt);
                if (root.contains("close"))
                    PopulateTextureField(root["close"], config.close);
                if (root.contains("yes"))
                    PopulateTextureField(root["yes"], config.yes);
                if (root.contains("no"))
                    PopulateTextureField(root["no"], config.no);

                return config;
            }
            catch (...)
            {
            }
        }

        return config;
    }

    // Resolve a texture by trying cached keys, loading via Resource_Manager, then falling back.
    unsigned ResolveTexture(const std::initializer_list<std::string>&keys, const std::string & path)
    {
        for (const auto& key : keys) {
            if (unsigned tex = Resource_Manager::getTexture(key)) {
                return tex;
            }
        }

        const std::string primaryKey = *keys.begin();
        if (Resource_Manager::load(primaryKey, path.c_str())) {
            if (unsigned tex = Resource_Manager::getTexture(primaryKey)) {
                return tex;
            }
        }

        return gfx::Graphics::loadTexture(path.c_str());
    }
}

void PauseMenuPage::Init(int screenW, int screenH)
{
    const HowToPopupJson popupConfig = LoadHowToPopupConfig();
    const ExitPopupJson exitConfig = LoadExitPopupConfig();
    noteTex = ResolveTexture({ "pause_note", "howto_note_bg" },
        Framework::ResolveAssetPath("Textures/UI/How To Play/Note.png").string());
    headerTex = ResolveTexture({ "pause_header", "paused_header" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/Paused.png").string());
    resumeTex = ResolveTexture({ "pause_resume" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/Resume.png").string());
    optionsTex = ResolveTexture({ "pause_options" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/Options.png").string());
    optionsHeaderTex = optionsTex;
    howToTex = ResolveTexture({ "pause_howto" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/How To Play.png").string());
    quitTex = ResolveTexture({ "pause_quit" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/Quit.png").string());
    closeTex = ResolveTexture({ "pause_close", "pause_x" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/XButton.png").string());

    howToNoteTex = ResolveTexture({ popupConfig.background.key },
        Framework::ResolveAssetPath(popupConfig.background.path).string());
    howToHeaderTex = ResolveTexture({ popupConfig.header.key },
        Framework::ResolveAssetPath(popupConfig.header.path).string());
    howToCloseTex = ResolveTexture({ popupConfig.close.key },
       
        Framework::ResolveAssetPath(popupConfig.close.path).string());
    exitPopupNoteTex = ResolveTexture({ exitConfig.background.key },
        Framework::ResolveAssetPath(exitConfig.background.path).string());
    exitPopupTitleTex = ResolveTexture({ exitConfig.title.key },
        Framework::ResolveAssetPath(exitConfig.title.path).string());
    exitPopupPromptTex = ResolveTexture({ exitConfig.prompt.key },
        Framework::ResolveAssetPath(exitConfig.prompt.path).string());
    exitPopupCloseTex = ResolveTexture({ exitConfig.close.key },
        Framework::ResolveAssetPath(exitConfig.close.path).string());
    exitPopupYesTex = ResolveTexture({ exitConfig.yes.key },
        Framework::ResolveAssetPath(exitConfig.yes.path).string());
    exitPopupNoTex = ResolveTexture({ exitConfig.no.key },
        Framework::ResolveAssetPath(exitConfig.no.path).string());

    howToRows.clear();
    howToRows.reserve(popupConfig.rows.size());
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
        rowCfg.iconTex = ResolveTexture({ row.icon.key, "howto_icon" }, iconPath.c_str());
        const std::string labelPath = Framework::ResolveAssetPath(row.label.path).string();
        rowCfg.labelTex = ResolveTexture({ row.label.key, "howto_label" }, labelPath.c_str());

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
    showHowToPopup = false;
    layoutDirty = true;

    SyncLayout(screenW, screenH);
    ResetLatches();
}

void PauseMenuPage::Update(Framework::InputSystem* input)
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

void PauseMenuPage::Draw(Framework::RenderSystem* render)
{
    const int screenW = render ? render->ScreenWidth() : sw;
    const int screenH = render ? render->ScreenHeight() : sh;
    SyncLayout(screenW, screenH);


    gfx::Graphics::renderRectangleUI(0.f, 0.f, static_cast<float>(screenW), static_cast<float>(screenH),
        0.f, 0.f, 0.f, 0.65f, screenW, screenH);
    if (showExitPopup && render)
    {
        auto textureAspect = [](unsigned tex, float fallback) {
            int texW = 0, texH = 0;
            if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0) {
                return static_cast<float>(texW) / static_cast<float>(texH);
            }
            return fallback;
            };

        if (exitPopupNoteTex) {
            gfx::Graphics::renderSpriteUI(exitPopupNoteTex, exitPopup.x, exitPopup.y, exitPopup.w, exitPopup.h,
                1.f, 1.f, 1.f, 1.f, sw, sh);
        }
        else {
            gfx::Graphics::renderRectangleUI(exitPopup.x, exitPopup.y, exitPopup.w, exitPopup.h,
                0.1f, 0.08f, 0.05f, 0.95f, sw, sh);
        }

        if (exitPopupTitleTex) {
            gfx::Graphics::renderSpriteUI(exitPopupTitleTex, exitTitle.x, exitTitle.y, exitTitle.w, exitTitle.h,
                1.f, 1.f, 1.f, 1.f, sw, sh);
        }
        else {
            gfx::Graphics::renderRectangleUI(exitTitle.x, exitTitle.y, exitTitle.w, exitTitle.h,
                0.2f, 0.18f, 0.12f, 0.6f, sw, sh);
        }

        if (exitPopupPromptTex) {
            gfx::Graphics::renderSpriteUI(exitPopupPromptTex, exitPrompt.x, exitPrompt.y, exitPrompt.w, exitPrompt.h,
                1.f, 1.f, 1.f, 1.f, sw, sh);
        }
        else {
            gfx::Graphics::renderRectangleUI(exitPrompt.x, exitPrompt.y, exitPrompt.w, exitPrompt.h,
                0.2f, 0.18f, 0.12f, 0.5f, sw, sh);
        }
    }
    else if (showOptionsPopup && render)
    {
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

        if (howToNoteTex) {
            gfx::Graphics::renderSpriteUI(howToNoteTex, optionsPopup.x, optionsPopup.y,
                optionsPopup.w, optionsPopup.h,
                1.f, 1.f, 1.f, 1.f,
                sw, sh);
        }
        else {
            gfx::Graphics::renderRectangleUI(optionsPopup.x, optionsPopup.y,
                optionsPopup.w, optionsPopup.h,
                0.1f, 0.08f, 0.05f, 0.95f,
                sw, sh);
        }

        const float headerAspect = textureAspect(optionsHeaderTex, 2.7f);
        const float headerH = optionsPopup.h * 0.2f;
        const float headerW = headerH * headerAspect;
        const float headerX = optionsPopup.x + (optionsPopup.w - headerW) * 0.5f;
        const float headerY = optionsPopup.y + optionsPopup.h - headerH - optionsPopup.h * 0.08f;
        if (optionsHeaderTex) {
            gfx::Graphics::renderSpriteUI(optionsHeaderTex, headerX, headerY, headerW, headerH,
                1.f, 1.f, 1.f, 1.f,
                sw, sh);
        }
        else if (render && render->IsTextReadyTitle()) {
            render->GetTextTitle().RenderText("Options", headerX + headerW * 0.15f, headerY + headerH * 0.24f,
                1.0f, glm::vec3(0.80f, 0.62f, 0.28f));
        }

        
    }
    else if (showHowToPopup && render) {
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

        if (howToNoteTex) {
            gfx::Graphics::renderSpriteUI(howToNoteTex, howToPopup.x, howToPopup.y,
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
    else {
        if (noteTex) {
            gfx::Graphics::renderSpriteUI(noteTex, note.x, note.y, note.w, note.h,
                1.f, 1.f, 1.f, 1.f, screenW, screenH);
        }
        else {
            gfx::Graphics::renderRectangleUI(note.x, note.y, note.w, note.h,
                0.89f, 0.85f, 0.74f, 0.95f, screenW, screenH);
        }

        if (headerTex) {
            gfx::Graphics::renderSpriteUI(headerTex, header.x, header.y, header.w, header.h,
                1.f, 1.f, 1.f, 1.f, screenW, screenH);
        }
        else if (render && render->IsTextReadyTitle()) {
            render->GetTextTitle().RenderText("Paused", header.x + header.w * 0.2f, header.y + header.h * 0.25f,
                1.0f, glm::vec3(0.80f, 0.62f, 0.28f));
        }
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

bool PauseMenuPage::ConsumeOptions()
{
    if (!optionsLatched) return false;
    optionsLatched = false;
    return true;
}

bool PauseMenuPage::ConsumeHowToPlay()
{
    if (!howToLatched) return false;
    howToLatched = false;
    return true;
}

bool PauseMenuPage::ConsumeQuitRequest()
{
    if (!quitRequestedLatched) return false;
    quitRequestedLatched = false;
    return true;
}

bool PauseMenuPage::ConsumeExitConfirmed()
{
    if (!exitConfirmedLatched) return false;
    exitConfirmedLatched = false;
    return true;
}

void PauseMenuPage::ResetLatches()
{
    resumeLatched = false;
    mainMenuLatched = false;
    optionsLatched = false;
    howToLatched = false;
    quitRequestedLatched = false;
    exitConfirmedLatched = false;
    showHowToPopup = false;
    showExitPopup = false;
    iconAnimTime = 0.0f;
    iconTimerInitialized = false;
    layoutDirty = true;
}
void PauseMenuPage::ShowExitPopup()
{
    quitRequestedLatched = false;
    exitConfirmedLatched = false;
    showExitPopup = true;
    showHowToPopup = false;
    showOptionsPopup = false;
    BuildGui();
}
void PauseMenuPage::SyncLayout(int screenW, int screenH)
{
    const bool sizeChanged = screenW != sw || screenH != sh;
    if (!layoutDirty && !sizeChanged)
        return;

    sw = screenW;
    sh = screenH;

    int noteWpx = 0, noteHpx = 0;
    const float defaultNoteAspect = 0.7f;
    const float noteAspect = (noteTex && gfx::Graphics::getTextureSize(noteTex, noteWpx, noteHpx) && noteHpx > 0)
        ? static_cast<float>(noteWpx) / static_cast<float>(noteHpx)
        : defaultNoteAspect;

    float noteW = static_cast<float>(sw) * 0.58f;
    float noteH = noteW / noteAspect;
    const float maxNoteH = static_cast<float>(sh) * 0.82f;
    if (noteH > maxNoteH) {
        noteH = maxNoteH;
        noteW = noteH * noteAspect;
    }

    note = { (static_cast<float>(sw) - noteW) * 0.5f, (static_cast<float>(sh) - noteH) * 0.5f, noteW, noteH };

    const float paddingX = note.w * 0.18f;
    const float topPad = note.h * 0.20f;
    int headerWpx = 0, headerHpx = 0;
    const float defaultHeaderAspect = 2.7f;
    const float headerAspect = (headerTex && gfx::Graphics::getTextureSize(headerTex, headerWpx, headerHpx) && headerHpx > 0)
        ? static_cast<float>(headerWpx) / static_cast<float>(headerHpx)
        : defaultHeaderAspect;
    float headerW = note.w * 0.46f;
    float headerH = headerW / headerAspect;
    header = { note.x + (note.w - headerW) * 0.5f, note.y + note.h - headerH * 1.1f, headerW, headerH };

    const float closeSize = headerH * 0.72f;
    closeBtn = { note.x + note.w - closeSize * 0.75f, note.y + note.h - closeSize * 0.72f, closeSize, closeSize };

    int buttonWpx = 0, buttonHpx = 0;
    const float defaultButtonAspect = 3.5f;
    const float buttonAspect = (resumeTex && gfx::Graphics::getTextureSize(resumeTex, buttonWpx, buttonHpx) && buttonHpx > 0)
        ? static_cast<float>(buttonWpx) / static_cast<float>(buttonHpx)
        : defaultButtonAspect;

    const float btnW = note.w * 0.54f;
    const float btnH = btnW / buttonAspect;
    const float spacing = btnH * 0.28f;
    const float firstBtnY = note.y + note.h - topPad - btnH;

    resumeBtn = { note.x + paddingX, firstBtnY, btnW, btnH };
    optionsBtn = { note.x + paddingX, resumeBtn.y - spacing - btnH, btnW, btnH };
    howToBtn = { note.x + paddingX, optionsBtn.y - spacing - btnH, btnW, btnH };
    quitBtn = { note.x + paddingX, howToBtn.y - spacing - btnH, btnW, btnH };

    const float defaultPopupAspect = 0.75f;
    int popupW = 0, popupH = 0;
    const bool hasPopupSize = howToNoteTex && gfx::Graphics::getTextureSize(howToNoteTex, popupW, popupH);
    const float popupAspect = (hasPopupSize && popupH > 0) ? static_cast<float>(popupW) / static_cast<float>(popupH) : defaultPopupAspect;

    float popupWf = static_cast<float>(sw) * 0.58f;
    float popupHf = popupWf / popupAspect;
    const float maxPopupH = static_cast<float>(sh) * 0.82f;
    if (popupHf > maxPopupH) {
        popupHf = maxPopupH;
        popupWf = popupHf * popupAspect;
    }

    const float popupX = (static_cast<float>(sw) - popupWf) * 0.58f;
    const float popupY = (static_cast<float>(sh) - popupHf) * 0.5f;
    const float popupCloseSize = std::min(popupWf, popupHf) * 0.14f;
    howToCloseBtn = { popupX + popupWf - popupCloseSize * 0.85f,
        popupY + popupHf - popupCloseSize * 0.75f,
        popupCloseSize,
        popupCloseSize };
    howToPopup = { popupX, popupY, popupWf, popupHf };

    auto textureAspect = [](unsigned tex, float fallback)
        {
            int texW = 0, texH = 0;
            if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0)
                return static_cast<float>(texW) / static_cast<float>(texH);
            return fallback;
        };

    optionsPopup = { popupX, popupY, popupWf, popupHf };
    optionsCloseBtn = howToCloseBtn;
    const float optionsHeaderH = popupHf * 0.18f;
    const float optionsHeaderW = optionsHeaderH * textureAspect(optionsHeaderTex, 2.7f);
    optionsHeader = { popupX + (popupWf - optionsHeaderW) * 0.5f,
        popupY + popupHf - optionsHeaderH - popupHf * 0.08f,
        optionsHeaderW, optionsHeaderH };

    const float toggleH = popupHf * 0.14f;
    const float toggleW = popupWf * 0.5f;
    muteToggleBtn = { popupX + (popupWf - toggleW) * 0.5f,
        popupY + popupHf * 0.32f,
        toggleW, toggleH };

    int exitNoteW = 0, exitNoteH = 0;
    const float defaultExitAspect = 0.72f;
    const float exitNoteAspect = (exitPopupNoteTex && gfx::Graphics::getTextureSize(exitPopupNoteTex, exitNoteW, exitNoteH) && exitNoteH > 0)
        ? static_cast<float>(exitNoteW) / static_cast<float>(exitNoteH)
        : defaultExitAspect;

    float exitPopupW = sw * 0.6f;
    float exitPopupH = exitPopupW / exitNoteAspect;
    const float maxExitPopupH = sh * 0.8f;
    if (exitPopupH > maxExitPopupH)
    {
        exitPopupH = maxExitPopupH;
        exitPopupW = exitPopupH * exitNoteAspect;
    }

    const float exitPopupX = (sw - exitPopupW) * 0.5f;
    const float exitPopupY = (sh - exitPopupH) * 0.5f;
    exitPopup = { exitPopupX, exitPopupY, exitPopupW, exitPopupH };

    const float exitTitleH = exitPopupH * 0.22f;
    const float exitTitleW = exitTitleH * textureAspect(exitPopupTitleTex, 2.7f);
    exitTitle = { exitPopupX + (exitPopupW - exitTitleW) * 0.5f,
        exitPopupY + exitPopupH - exitTitleH - exitPopupH * 0.08f,
        exitTitleW, exitTitleH };

    const float exitPromptH = exitPopupH * 0.15f;
    const float exitPromptW = exitPromptH * textureAspect(exitPopupPromptTex, 2.3f);
    const float exitPromptY = exitPopupY + exitPopupH * 0.52f - exitPromptH * 0.5f;
    exitPrompt = { exitPopupX + (exitPopupW - exitPromptW) * 0.5f,
        exitPromptY,
        exitPromptW, exitPromptH };

    const float exitCloseSize = std::min(exitPopupW, exitPopupH) * 0.13f;
    exitCloseBtn = { exitPopupX + exitPopupW - exitCloseSize * 0.82f,
        exitPopupY + exitPopupH - exitCloseSize * 0.78f,
        exitCloseSize,
        exitCloseSize };

    const float exitBtnH = exitPopupH * 0.18f;
    const float exitYesW = exitBtnH * textureAspect(exitPopupYesTex, 1.7f);
    const float exitNoW = exitBtnH * textureAspect(exitPopupNoTex, 1.7f);
    const float exitBtnSpacing = exitPopupW * 0.06f;
    const float exitBtnCenter = exitPopupX + exitPopupW * 0.5f;
    const float exitBtnY = exitPopupY + exitPopupH * 0.18f;
    exitYesBtn = { exitBtnCenter - exitBtnSpacing * 0.5f - exitYesW,
        exitBtnY,
        exitYesW, exitBtnH };
    exitNoBtn = { exitBtnCenter + exitBtnSpacing * 0.5f,
        exitBtnY,
        exitNoW, exitBtnH };

    BuildGui();
}

void PauseMenuPage::BuildGui()
{
    gui.Clear();
    if (showExitPopup)
    {
        gui.AddButton(exitYesBtn.x, exitYesBtn.y, exitYesBtn.w, exitYesBtn.h, "YES",
            exitPopupYesTex, exitPopupYesTex,
            [this]() {
                exitConfirmedLatched = true;
                
            });

        gui.AddButton(exitNoBtn.x, exitNoBtn.y, exitNoBtn.w, exitNoBtn.h, "NO",
            exitPopupNoTex, exitPopupNoTex,
            [this]() {
                showExitPopup = false;
                BuildGui();
            });

        gui.AddButton(exitCloseBtn.x, exitCloseBtn.y, exitCloseBtn.w, exitCloseBtn.h, "",
            exitPopupCloseTex, exitPopupCloseTex,
            [this]() {
                showExitPopup = false;
                BuildGui();
            }, true);
    }
    else if (showOptionsPopup)
    {
        if (howToCloseTex)
        {
            gui.AddButton(optionsCloseBtn.x, optionsCloseBtn.y, optionsCloseBtn.w, optionsCloseBtn.h, "",
                howToCloseTex, howToCloseTex,
                [this]() {
                    showOptionsPopup = false;
                    layoutDirty = true;
                    BuildGui();
                }, true);
        }

        const std::string muteLabel = (audioMuted ? "[X] " : "[ ] ") + std::string("Mute Audio");
        gui.AddButton(muteToggleBtn.x, muteToggleBtn.y, muteToggleBtn.w, muteToggleBtn.h,
            muteLabel,
            [this]() {
                audioMuted = !audioMuted;
                SoundManager::getInstance().setMasterVolume(audioMuted ? 0.0f : masterVolumeDefault);
                BuildGui();
            });
    }
    else if (showHowToPopup) {
        if (howToCloseTex) {
            gui.AddButton(howToCloseBtn.x, howToCloseBtn.y, howToCloseBtn.w, howToCloseBtn.h, "",
                howToCloseTex, howToCloseTex,
                [this]() {
                    showHowToPopup = false;
                    layoutDirty = true;
                    BuildGui();
                }, true);
        }
    }
    else {
        gui.AddButton(resumeBtn.x, resumeBtn.y, resumeBtn.w, resumeBtn.h, "Resume",
            resumeTex, resumeTex,
            [this]() { resumeLatched = true; });
        gui.AddButton(optionsBtn.x, optionsBtn.y, optionsBtn.w, optionsBtn.h, "Options",
            optionsTex, optionsTex,
            [this]() {
                optionsLatched = true;
                showOptionsPopup = true;
                showHowToPopup = false;
                showExitPopup = false;
                layoutDirty = true;
                BuildGui();
            });
        gui.AddButton(howToBtn.x, howToBtn.y, howToBtn.w, howToBtn.h, "How To Play",
            howToTex, howToTex,
            [this]() {
                howToLatched = true;
                showHowToPopup = true;
                iconAnimTime = 0.0f;
                iconTimerInitialized = false;
                layoutDirty = true;
                BuildGui();
            });
        gui.AddButton(quitBtn.x, quitBtn.y, quitBtn.w, quitBtn.h, "Quit",
            quitTex, quitTex,
            [this]() {
                quitRequestedLatched = true;
            });
        gui.AddButton(closeBtn.x, closeBtn.y, closeBtn.w, closeBtn.h, "",
            closeTex, closeTex,
            [this]() { resumeLatched = true; });
    }
}
