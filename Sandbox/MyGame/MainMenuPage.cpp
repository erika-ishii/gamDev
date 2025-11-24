/*********************************************************************************************
 \file      MainMenuPage.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Simple main-menu screen with cached texture resolution and image-button GUI.
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
#include <map>
#include <functional>
#include <iostream>
#include <json.hpp>

using namespace mygame;

namespace {
    // --- Helper Structures for JSON Loading ---
    struct TextureField {
        std::string key;
        std::string path;
    };

    struct MenuButtonJson {
        std::string label;
        std::string action; // Maps to functionality (Start, Exit, etc.)
        TextureField texture;
    };

    struct MainMenuJson {
        TextureField background;
        struct Layout {
            float btnW = 372.f;
            float btnH = 109.f;
            float spacing = 24.f;
            float scale = 0.60f;
            float leftAlign = 0.23f;
            float downOffset = 180.f;
        } layout;
        std::vector<MenuButtonJson> buttons;
    };

    // --- How To Popup Structs (Existing) ---
    struct HowToRowJson {
        TextureField icon;
        TextureField label;
        int frames = 0;
        float fps = 8.0f;
        float iconAspect = 1.0f;
        float labelAspect = 1.0f;
        int cols = 0;
        int rows = 0;
    };

    struct HowToPopupJson {
        TextureField background;
        TextureField header;
        TextureField close;
        std::vector<HowToRowJson> rows;
    };

    // --- JSON Parsers ---
    TextureField MakeTextureField(const std::string& key, const std::string& path) {
        return TextureField{ key, path };
    }

    bool PopulateTextureField(const nlohmann::json& obj, TextureField& out) {
        if (obj.contains("key")) out.key = obj["key"].get<std::string>();
        if (obj.contains("path")) out.path = obj["path"].get<std::string>();
        return obj.contains("key") || obj.contains("path");
    }

    MainMenuJson LoadMainMenuConfig() {
        MainMenuJson config;
        auto path = Framework::ResolveDataPath("main_menu_ui.json");

        std::ifstream stream(path);
        if (!stream.is_open()) {
            std::cerr << "[MainMenu] Warning: Could not load main_menu_ui.json, using defaults.\n";
            return config; // Return defaults
        }

        try {
            nlohmann::json j;
            stream >> j;

            if (j.contains("background")) PopulateTextureField(j["background"], config.background);

            if (j.contains("layout")) {
                auto& l = j["layout"];
                if (l.contains("button_width")) config.layout.btnW = l["button_width"];
                if (l.contains("button_height")) config.layout.btnH = l["button_height"];
                if (l.contains("vertical_spacing")) config.layout.spacing = l["vertical_spacing"];
                if (l.contains("scale_factor")) config.layout.scale = l["scale_factor"];
                if (l.contains("left_align_pct")) config.layout.leftAlign = l["left_align_pct"];
                if (l.contains("downward_offset")) config.layout.downOffset = l["downward_offset"];
            }

            if (j.contains("buttons") && j["buttons"].is_array()) {
                for (const auto& btn : j["buttons"]) {
                    MenuButtonJson b;
                    if (btn.contains("label")) b.label = btn["label"];
                    if (btn.contains("action")) b.action = btn["action"];
                    if (btn.contains("texture")) PopulateTextureField(btn["texture"], b.texture);
                    config.buttons.push_back(b);
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[MainMenu] JSON Error: " << e.what() << "\n";
        }
        return config;
    }

    HowToPopupJson DefaultHowToPopupConfig() {
        HowToPopupJson config{};
        config.background = MakeTextureField("howto_note_bg", "Textures/UI/How To Play/Note.png");
        config.header = MakeTextureField("howto_header", "Textures/UI/How To Play/How To Play.png");
        config.close = MakeTextureField("menu_popup_close", "Textures/UI/How To Play/XButton.png");

        // Add default rows here if needed, mirroring your original code
        return config;
    }

    HowToPopupJson LoadHowToPopupConfig() {
        HowToPopupJson config = DefaultHowToPopupConfig();

        // Try loading from JSON (multiple candidates)
        std::vector<std::filesystem::path> candidates;
        candidates.emplace_back(Framework::ResolveDataPath("howto_popup.json"));
        candidates.emplace_back(Framework::ResolveDataPath("HowToPopup.json"));
        candidates.emplace_back(std::filesystem::path("assets/data/howto_popup.json")); // Explicitly check assets/data
        candidates.emplace_back(std::filesystem::path("Data_Files") / "howto_popup.json");

        for (const auto& path : candidates) {
            std::ifstream stream(path);
            if (!stream.is_open()) continue;

            try {
                nlohmann::json j;
                stream >> j;
                if (!j.contains("howToPopup")) continue;

                const auto& root = j["howToPopup"];
                if (root.contains("background")) PopulateTextureField(root["background"], config.background);
                if (root.contains("header")) PopulateTextureField(root["header"], config.header);
                if (root.contains("close")) PopulateTextureField(root["close"], config.close);

                if (root.contains("rows") && root["rows"].is_array()) {
                    std::vector<HowToRowJson> rows;
                    for (const auto& rowJson : root["rows"]) {
                        HowToRowJson row;
                        if (rowJson.contains("icon")) PopulateTextureField(rowJson["icon"], row.icon);
                        if (rowJson.contains("label")) PopulateTextureField(rowJson["label"], row.label);
                        if (rowJson.contains("frames")) row.frames = rowJson["frames"];
                        if (rowJson.contains("fps")) row.fps = rowJson["fps"];
                        if (rowJson.contains("iconAspect")) row.iconAspect = rowJson["iconAspect"];
                        if (rowJson.contains("labelAspect")) row.labelAspect = rowJson["labelAspect"];
                        if (rowJson.contains("cols")) row.cols = rowJson["cols"];
                        if (rowJson.contains("rows")) row.rows = rowJson["rows"];
                        rows.push_back(row);
                    }
                    if (!rows.empty()) config.rows = rows;
                }
                std::cout << "[MainMenu] Loaded popup config from " << path << "\n";
                return config; // Successfully loaded
            }
            catch (...) {}
        }
        std::cerr << "[MainMenu] Warning: Could not load howto_popup.json, using defaults.\n";
        return config;
    }
}

// Helper to resolve textures
static unsigned ResolveTex(const TextureField& tf) {
    // 1. Try Resource Manager Cache
    if (unsigned tex = Resource_Manager::getTexture(tf.key)) return tex;

    // 2. Try Loading via Resource Manager
    std::string path = Framework::ResolveAssetPath(tf.path).string();
    if (Resource_Manager::load(tf.key, path.c_str())) {
        return Resource_Manager::getTexture(tf.key);
    }

    // 3. Fallback: Direct Graphics Load
    return gfx::Graphics::loadTexture(path.c_str());
}

// --- Global/Static Data Storage ---
static MainMenuJson g_MenuConfig;
static std::vector<std::pair<std::string, unsigned>> g_ButtonTextures;

void MainMenuPage::Init(int screenW, int screenH)
{
    sw = screenW;
    sh = screenH;

    // 1. Load Main Menu Config
    g_MenuConfig = LoadMainMenuConfig();

    // 2. Load Background
    menuBgTex = ResolveTex(g_MenuConfig.background);

    // 3. Load Button Textures
    g_ButtonTextures.clear();
    for (const auto& btn : g_MenuConfig.buttons) {
        unsigned tex = ResolveTex(btn.texture);
        g_ButtonTextures.emplace_back(btn.action, tex);
    }

    // 4. Load Popup Config & Textures
    // FIX: Ensure these member variables are populated!
    const HowToPopupJson popupConfig = LoadHowToPopupConfig();

    closePopupTex = ResolveTex(popupConfig.close);
    noteBackgroundTex = ResolveTex(popupConfig.background);
    howToHeaderTex = ResolveTex(popupConfig.header);

    // Clear old rows
    howToRows.clear();

    auto frameCountFromStrip = [](unsigned tex) {
        int texW = 0, texH = 0;
        if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0) {
            return std::max(1, texW / texH);
        }
        return 1;
        };

    for (const auto& row : popupConfig.rows)
    {
        HowToRowConfig rowCfg{};
        rowCfg.iconTex = ResolveTex(row.icon);
        rowCfg.labelTex = ResolveTex(row.label);

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

    // Force layout update
    layoutInitialized = false;
    SyncLayout(sw, sh);
}

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

void MainMenuPage::Draw(Framework::RenderSystem* render)
{
    if (render) {
        SyncLayout(render->ScreenWidth(), render->ScreenHeight());
    }

    // Background
    if (menuBgTex) {
        gfx::Graphics::renderFullscreenTexture(menuBgTex);
    }

    // Popup Overlay
    if (showHowToPopup && render) {
        const float overlayAlpha = 0.65f;
        gfx::Graphics::renderRectangleUI(0.f, 0.f, (float)sw, (float)sh, 0.f, 0.f, 0.f, overlayAlpha, sw, sh);

        // Note Background
        if (noteBackgroundTex) {
            gfx::Graphics::renderSpriteUI(noteBackgroundTex, howToPopup.x, howToPopup.y, howToPopup.w, howToPopup.h, 1.f, 1.f, 1.f, 1.f, sw, sh);
        }
        else {
            // Fallback rect if texture missing
            gfx::Graphics::renderRectangleUI(howToPopup.x, howToPopup.y, howToPopup.w, howToPopup.h, 0.1f, 0.08f, 0.05f, 0.95f, sw, sh);
        }

        // Header
        auto textureAspect = [](unsigned tex, float fallback) {
            int texW = 0, texH = 0;
            if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0) {
                return static_cast<float>(texW) / static_cast<float>(texH);
            }
            return fallback;
            };

        const float headerPadY = howToPopup.h * 0.07f;
        const float headerHeight = howToPopup.h * 0.16f;
        const float headerAspect = textureAspect(howToHeaderTex, 2.6f);
        const float headerWidth = headerHeight * headerAspect;
        const float headerX = howToPopup.x + (howToPopup.w - headerWidth) * 0.5f;
        const float headerY = howToPopup.y + howToPopup.h - headerHeight - headerPadY;

        if (howToHeaderTex) {
            gfx::Graphics::renderSpriteUI(howToHeaderTex, headerX, headerY, headerWidth, headerHeight, 1.f, 1.f, 1.f, 1.f, sw, sh);
        }

        // Content Rows
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

        // UI Projection for content
        const glm::mat4 uiOrtho = glm::ortho(0.0f, static_cast<float>(sw), 0.0f, static_cast<float>(sh), -1.0f, 1.0f);
        gfx::Graphics::setViewProjection(glm::mat4(1.0f), uiOrtho);

        for (size_t i = 0; i < howToRows.size(); ++i) {
            const float sizeScale = (i < 2) ? 0.94f : 1.0f;
            const float iconHeight = iconHeightBase * sizeScale;
            const float labelHeight = labelHeightBase * sizeScale;
            const float rowBaseY = contentTop - rowHeight * (static_cast<float>(i) + 1.f);
            const float iconY = rowBaseY + (rowHeight - iconHeight) * 0.5f;
            const float labelY = rowBaseY + (rowHeight - labelHeight) * 0.5f;

            // Icon
            if (howToRows[i].iconTex) {
                const int frames = std::max(1, howToRows[i].frameCount);
                const int cols = std::max(1, howToRows[i].cols);
                const int rows = std::max(1, howToRows[i].rows);

                const float iconAspect = textureAspect(howToRows[i].iconTex, howToRows[i].iconAspectFallback) *
                    (static_cast<float>(rows) / static_cast<float>(cols));
                const float iconW = iconHeight * iconAspect;
                const float iconX = iconAnchorX - iconW - ((i < 2) ? howToPopup.w * 0.01f : 0.0f);

                const float fps = howToRows[i].fps > 0.0f ? howToRows[i].fps : 8.0f;
                const int frameIndex = (frames > 1) ? (static_cast<int>(iconAnimTime * fps) % frames) : 0;

                gfx::Graphics::renderSpriteFrame(howToRows[i].iconTex,
                    iconX + iconW * 0.5f, iconY + iconHeight * 0.5f,
                    0.f, iconW, iconHeight,
                    frameIndex, cols, rows,
                    1.f, 1.f, 1.f, 1.f);
            }

            // Label
            if (howToRows[i].labelTex) {
                const float labelAspect = textureAspect(howToRows[i].labelTex, howToRows[i].labelAspectFallback);
                const float labelW = labelHeight * labelAspect;
                gfx::Graphics::renderSpriteUI(howToRows[i].labelTex, labelX, labelY, labelW, labelHeight, 1.f, 1.f, 1.f, 1.f, sw, sh);
            }
        }
        gfx::Graphics::resetViewProjection();
    }

    gui.Draw(render);
}

// Latch Consumers
bool MainMenuPage::ConsumeStart() { if (!startLatched) return false; startLatched = false; return true; }
bool MainMenuPage::ConsumeOptions() { if (!optionsLatched) return false; optionsLatched = false; return true; }
bool MainMenuPage::ConsumeHowToPlay() { if (!howToLatched) return false; howToLatched = false; return true; }
bool MainMenuPage::ConsumeExit() { if (!exitLatched) return false; exitLatched = false; return true; }

void MainMenuPage::SyncLayout(int screenW, int screenH)
{
    if (layoutInitialized && screenW == sw && screenH == sh) return;

    sw = screenW;
    sh = screenH;

    const float baseW = 1280.f;
    const float baseH = 720.f;
    const float scaleX = sw / baseW;
    const float scaleY = sh / baseH;

    // --- Dynamic Layout ---
    const auto& l = g_MenuConfig.layout;
    const float btnW = l.btnW * scaleX * l.scale;
    const float btnH = l.btnH * scaleY * l.scale;
    const float vSpace = l.spacing * scaleY;

    size_t count = g_MenuConfig.buttons.size();
    const float blockHeight = (btnH * count) + (vSpace * (count - 1));

    const float downwardOffset = l.downOffset * scaleY;
    // Calculate base (bottom) Y of the stack
    const float bottomY = std::max(0.f, (sh - blockHeight) * 0.5f - downwardOffset);
    const float leftAlignedX = (sw - btnW) * l.leftAlign;

    // --- Popup Layout ---
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
    howToPopup = { popupX, popupY, popupW, popupH };

    const float closeSize = std::min(popupW, popupH) * 0.14f;
    closeBtn = { popupX + popupW - closeSize * 0.85f, popupY + popupH - closeSize * 0.75f, closeSize, closeSize };

    BuildGui(leftAlignedX, bottomY, btnW, btnH, vSpace);
    layoutInitialized = true;
}

void MainMenuPage::BuildGui()
{
    // Recalculate based on current layout config
    layoutInitialized = false;
    SyncLayout(sw, sh);
}

// Helper to build GUI with calculated dimensions
void MainMenuPage::BuildGui(float x, float bottomY, float w, float h, float spacing)
{
    gui.Clear();

    size_t total = g_MenuConfig.buttons.size();
    for (size_t i = 0; i < total; ++i) {
        const auto& btnDef = g_MenuConfig.buttons[i];

        float yPos = bottomY + (total - 1 - i) * (h + spacing);

        // Find texture
        unsigned tex = 0;
        for (auto& p : g_ButtonTextures) if (p.first == btnDef.action) tex = p.second;

        // Map Action String to Lambda
        std::function<void()> callback = []() {};
        if (btnDef.action == "start")        callback = [this]() { startLatched = true; };
        else if (btnDef.action == "options") callback = [this]() { optionsLatched = true; };
        else if (btnDef.action == "exit")    callback = [this]() { exitLatched = true; };
        else if (btnDef.action == "howto")   callback = [this]() {
            howToLatched = true;
            showHowToPopup = true;
            iconAnimTime = 0.0f;
            iconTimerInitialized = false;
            // IMPORTANT: Rebuild GUI to include Close button
            BuildGui();
            };

        gui.AddButton(x, yPos, w, h, btnDef.label, tex, tex, callback);
    }

    // Add Close Button if Popup is open
    if (showHowToPopup && closePopupTex) {
   
        gui.AddButton(closeBtn.x, closeBtn.y, closeBtn.w, closeBtn.h, "",
            closePopupTex, closePopupTex,
            [this]() { showHowToPopup = false; BuildGui(); });
    }
}