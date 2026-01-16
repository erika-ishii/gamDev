/*********************************************************************************************
 \file      MainMenuPage.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Simple main-menu screen with cached texture resolution and image-button GUI.
 \details   main menu and its popups:
            - Background: fullscreen parchment/background texture.
            - Buttons: Start / How To / Options / Exit (labels and callbacks via JSON).
            - Layout: button size/spacing/position derived from main_menu_ui.json.
            - How To popup: note-style parchment with animated icon/label rows.
            - Options popup: simple mute toggle using SoundManager master volume.
            - Exit popup: confirmation dialog before quitting the game.
            - JSON config: main_menu_ui.json, howto_popup.json, exit_popup.json override defaults.
            - GUI wiring: uses a GUI helper to register clickable buttons and invoke lambdas.
*********************************************************************************************/

#include "MainMenuPage.hpp"
#include "Core/PathUtils.h"
#include "Graphics/Graphics.hpp"
#include "Resource_Asset_Manager/Resource_Manager.h"
#include <algorithm>
#include "Audio/SoundManager.h"
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
        float headerOffsetX = 0.0f;
        float headerOffsetY = 0.0f;
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

    /*************************************************************************************
      \brief  Helper to construct a TextureField from key/path.
      \param  key   Resource_Manager lookup key.
      \param  path  Relative texture path under assets/.
    *************************************************************************************/
    TextureField MakeTextureField(const std::string& key, const std::string& path) {
        return TextureField{ key, path };
    }

    /*************************************************************************************
      \brief  Populate a TextureField from a JSON object, if fields exist.
      \param  obj  JSON object that may contain "key" and/or "path".
      \param  out  TextureField to update.
      \return True if either key or path was present.
    *************************************************************************************/
    bool PopulateTextureField(const nlohmann::json& obj, TextureField& out) {
        if (obj.contains("key")) out.key = obj["key"].get<std::string>();
        if (obj.contains("path")) out.path = obj["path"].get<std::string>();
        return obj.contains("key") || obj.contains("path");
    }

    /*************************************************************************************
      \brief  Load main menu layout and button configuration from JSON.
      \details Reads Data_Files/main_menu_ui.json via ResolveDataPath. On success:
               - background: texture key/path.
               - layout: button sizes, spacing, scaling, left alignment, downward offset.
               - buttons: label/action/texture for each menu entry.
               Falls back to default-initialized config and logs a warning on failure.
      \return Parsed MainMenuJson structure (defaults if JSON missing/invalid).
    *************************************************************************************/
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

    /*************************************************************************************
      \brief  Build default "How To Play" popup config when JSON is missing or partial.
      \details Provides parchment background, header, X button, and default WASD/ESC/LMB/RMB
               rows with sprite and label texture hints. Frames/FPS/aspect can be overridden
               by howto_popup.json if present.
    *************************************************************************************/
    HowToPopupJson DefaultHowToPopupConfig() {
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
                0, 8.0f, 1.8f, 3.1f },
            { MakeTextureField("howto_melee_icon", "Textures/UI/How To Play/Left_Mouse_Sprite.png"),
                MakeTextureField("howto_melee_label", "Textures/UI/How To Play/For melee attack.png"),
                0, 8.0f, 0.72f, 3.1f },
            { MakeTextureField("howto_range_icon", "Textures/UI/How To Play/Right_Mouse_Sprite.png"),
                MakeTextureField("howto_range_label", "Textures/UI/How To Play/For Range attack.png"),
                0, 8.0f, 0.72f, 3.1f },
        };
        config.headerOffsetX = 0.0f;
        config.headerOffsetY = 0.0f;

        return config;
    }

    /*************************************************************************************
      \brief  Build default exit popup config when JSON is missing or partial.
      \details Uses Exit-specific parchment and text textures under Textures/UI/Exit.
    *************************************************************************************/
    ExitPopupJson DefaultExitPopupConfig()
    {
        ExitPopupJson config{};
        config.background = MakeTextureField("exit_popup_note", "Textures/UI/Exit/Note.png");
        config.title = MakeTextureField("exit_popup_title", "Textures/UI/Exit/Exit.png");
        config.prompt = MakeTextureField("exit_popup_prompt", "Textures/UI/Exit/Exit Anot_.png");
        config.close = MakeTextureField("exit_popup_close", "Textures/UI/Exit/XButton.png");
        config.yes = MakeTextureField("exit_popup_yes", "Textures/UI/Exit/Yes.png");
        config.no = MakeTextureField("exit_popup_no", "Textures/UI/Exit/No.png");
        return config;
    }

    /*************************************************************************************
      \brief  Load How To popup config from JSON (if present), otherwise use defaults.
      \details Checks a small list of candidate JSON paths in data/assets/Data_Files.
               On success, overrides background/header/close, header offset, and row list.
               Logs a message on success and a warning when falling back to defaults.
    *************************************************************************************/
    HowToPopupJson LoadHowToPopupConfig() {
        HowToPopupJson config = DefaultHowToPopupConfig();

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
                if (root.contains("header_offset")) {
                    const auto& off = root["header_offset"];
                    if (off.contains("x")) config.headerOffsetX = off["x"];
                    if (off.contains("y")) config.headerOffsetY = off["y"];
                }
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
} // anonymous namespace

/*************************************************************************************
  \brief  Load exit popup config from JSON (if present), otherwise use defaults.
  \details Checks exit_popup.json under data/assets/Data_Files. On success, overrides
           the parchment, title, prompt, X, Yes, and No textures, and logs the source.
  \return Parsed ExitPopupJson structure (defaults if JSON missing/invalid).
*************************************************************************************/
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
        if (!stream.is_open()) continue;

        try
        {
            nlohmann::json j;
            stream >> j;
            if (!j.contains("exitPopup")) continue;

            const auto& root = j["exitPopup"];
            if (root.contains("background")) PopulateTextureField(root["background"], config.background);
            if (root.contains("title")) PopulateTextureField(root["title"], config.title);
            if (root.contains("prompt")) PopulateTextureField(root["prompt"], config.prompt);
            if (root.contains("close")) PopulateTextureField(root["close"], config.close);
            if (root.contains("yes")) PopulateTextureField(root["yes"], config.yes);
            if (root.contains("no")) PopulateTextureField(root["no"], config.no);

            std::cout << "[MainMenu] Loaded exit popup config from " << path << "\n";
            return config;
        }
        catch (...)
        {
        }
    }

    std::cerr << "[MainMenu] Warning: Could not load exit_popup.json, using defaults.\n";
    return config;
}

/*************************************************************************************
  \brief  Resolve a texture using cached Resource_Manager keys and a fallback path.
  \param  tf  TextureField containing a cache key and relative asset path.
  \return OpenGL texture handle, or a direct load via gfx::Graphics as fallback.
  \details Attempts:
           1) Resource_Manager::getTexture(tf.key).
           2) Resource_Manager::load(tf.key, resolved path).
           3) gfx::Graphics::loadTexture(path) if still missing.
*************************************************************************************/
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

/*************************************************************************************
  \brief  Initialize main menu textures, button definitions, and popup configuration.
  \param  screenW  Initial screen width in pixels.
  \param  screenH  Initial screen height in pixels.
  \details Loads main_menu_ui.json (background, button layout and labels), howto popup
           JSON, and exit popup JSON. Resolves all textures, builds How To row animation
           state, resets timers, and forces a layout recompute.
*************************************************************************************/
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
        if (btn.action == "options") {
            optionsHeaderTex = tex;
        }
    }

    // 4. Load Popup Config & Textures
    const HowToPopupJson popupConfig = LoadHowToPopupConfig();
    const ExitPopupJson exitPopupConfig = LoadExitPopupConfig();

    closePopupTex = ResolveTex(popupConfig.close);
    noteBackgroundTex = ResolveTex(popupConfig.background);
    howToHeaderTex = ResolveTex(popupConfig.header);
    howToHeaderOffsetX = popupConfig.headerOffsetX;
    howToHeaderOffsetY = popupConfig.headerOffsetY;

    exitPopupNoteTex = ResolveTex(exitPopupConfig.background);
    exitPopupTitleTex = ResolveTex(exitPopupConfig.title);
    exitPopupPromptTex = ResolveTex(exitPopupConfig.prompt);
    exitPopupCloseTex = ResolveTex(exitPopupConfig.close);
    exitPopupYesTex = ResolveTex(exitPopupConfig.yes);
    exitPopupNoTex = ResolveTex(exitPopupConfig.no);

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

/*************************************************************************************
  \brief  Update animation timers and forward input to the GUI system.
  \param  input  Pointer to the InputSystem used by the GUI helper.
  \details When the How To popup is visible, advances iconAnimTime based on a steady
           clock. Regardless of popup state, forwards input into gui.Update().
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
  \brief  Draw the main menu background, active popup (if any), and GUI widgets.
  \param  render  RenderSystem pointer used for screen size and projection setup.
  \details Renders:
           - Fullscreen background.
           - A dark overlay + Exit popup, Options popup, or How To popup when active.
           - Or just the base menu buttons when no popup is visible.
           The GUI helper then renders interactive elements on top.
*************************************************************************************/
void MainMenuPage::Draw(Framework::RenderSystem* render)
{
    if (render) {
        SyncLayout(render->ScreenWidth(), render->ScreenHeight());
    }

    // Background
    if (menuBgTex) {
        gfx::Graphics::renderFullscreenTexture(menuBgTex);
    }
    auto textureAspect = [](unsigned tex, float fallback) {
        int texW = 0, texH = 0;
        if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0) {
            return static_cast<float>(texW) / static_cast<float>(texH);
        }
        return fallback;
        };

    // Popup Overlay
    if (showExitPopup && render)
    {
        const float overlayAlpha = 0.65f;
        gfx::Graphics::renderRectangleUI(0.f, 0.f, (float)sw, (float)sh, 0.f, 0.f, 0.f, overlayAlpha, sw, sh);

        if (exitPopupNoteTex)
        {
            gfx::Graphics::renderSpriteUI(exitPopupNoteTex, exitPopup.x, exitPopup.y, exitPopup.w, exitPopup.h, 1.f, 1.f, 1.f, 1.f, sw, sh);
        }
        else
        {
            gfx::Graphics::renderRectangleUI(exitPopup.x, exitPopup.y, exitPopup.w, exitPopup.h, 0.1f, 0.08f, 0.05f, 0.95f, sw, sh);
        }

        if (exitPopupTitleTex)
        {
            // EXIT header already has its left/up nudge applied in SyncLayout via exitTitle
            gfx::Graphics::renderSpriteUI(exitPopupTitleTex,
                exitTitle.x,
                exitTitle.y,
                exitTitle.w, exitTitle.h,
                1.f, 1.f, 1.f, 1.f, sw, sh);
        }

        if (exitPopupPromptTex)
        {
            gfx::Graphics::renderSpriteUI(exitPopupPromptTex, exitPrompt.x, exitPrompt.y, exitPrompt.w, exitPrompt.h, 1.f, 1.f, 1.f, 1.f, sw, sh);
        }
    }
    else if (showOptionsPopup && render)
    {
        // ---------------------------------------------------------
        // OPTIONS POPUP: Background Only (No Header)
        // ---------------------------------------------------------
        const float overlayAlpha = 0.65f;
        gfx::Graphics::renderRectangleUI(0.f, 0.f, static_cast<float>(sw), static_cast<float>(sh), 0.f, 0.f, 0.f, overlayAlpha, sw, sh);

        if (noteBackgroundTex) {
            gfx::Graphics::renderSpriteUI(noteBackgroundTex, optionsPopup.x, optionsPopup.y, optionsPopup.w, optionsPopup.h, 1.f, 1.f, 1.f, 1.f, sw, sh);
        }
        else {
            gfx::Graphics::renderRectangleUI(optionsPopup.x, optionsPopup.y, optionsPopup.w, optionsPopup.h, 0.1f, 0.08f, 0.05f, 0.95f, sw, sh);
        }
    }
    else if (showHowToPopup && render) {
        const float overlayAlpha = 0.65f;
        gfx::Graphics::renderRectangleUI(0.f, 0.f, (float)sw, (float)sh, 0.f, 0.f, 0.f, overlayAlpha, sw, sh);

        // Note Background
        if (noteBackgroundTex) {
            gfx::Graphics::renderSpriteUI(noteBackgroundTex, howToPopup.x, howToPopup.y, howToPopup.w, howToPopup.h, 1.f, 1.f, 1.f, 1.f, sw, sh);
        }
        else {
            gfx::Graphics::renderRectangleUI(howToPopup.x, howToPopup.y, howToPopup.w, howToPopup.h, 0.1f, 0.08f, 0.05f, 0.95f, sw, sh);
        }

        // --- HEADER ---
        const float headerPadY = howToPopup.h * 0.07f;
        const float headerHeight = howToPopup.h * 0.16f;
        const float headerAspect = textureAspect(howToHeaderTex, 2.6f);
        const float headerWidth = headerHeight * headerAspect;

        // Offset Calculation
        const float offsetScale = howToPopup.w / (1280.0f * 0.58f);
        const float scaledOffsetX = howToHeaderOffsetX * offsetScale;
        const float scaledOffsetY = howToHeaderOffsetY * offsetScale;

        // Apply extra adjustments to move Header LEFT (-X) and UP (+Y)
        const float extraMoveLeft = 30.0f * offsetScale;
        const float extraMoveUp = 25.0f * offsetScale;

        const float headerX = howToPopup.x + (howToPopup.w - headerWidth) * 0.5f + scaledOffsetX - extraMoveLeft;
        const float headerY = howToPopup.y + howToPopup.h - headerHeight - headerPadY + scaledOffsetY + extraMoveUp;

        if (howToHeaderTex) {
            gfx::Graphics::renderSpriteUI(howToHeaderTex, headerX, headerY, headerWidth, headerHeight, 1.f, 1.f, 1.f, 1.f, sw, sh);
        }

        // --- CONTENT ROWS ---
        const float contentTop = headerY - howToPopup.h * 0.04f;
        const float contentBottom = howToPopup.y + howToPopup.h * 0.08f;
        const float availableHeight = std::max(0.1f, contentTop - contentBottom);
        const size_t rowCount = std::max<size_t>(1, howToRows.size());
        const float rowHeight = availableHeight / static_cast<float>(rowCount);

        const float iconHeightBase = rowHeight * 0.78f;
        const float labelHeightBase = rowHeight * 0.58f;
        const float baseLeftPad = howToPopup.w * 0.20f;   // base position for labels
        const float rightPad = howToPopup.w * 0.14f;
        const float iconAnchorX = howToPopup.x + howToPopup.w - rightPad;


        const glm::mat4 uiOrtho = glm::ortho(0.0f, static_cast<float>(sw), 0.0f, static_cast<float>(sh), -1.0f, 1.0f);
        gfx::Graphics::setViewProjection(glm::mat4(1.0f), uiOrtho);

        for (size_t i = 0; i < howToRows.size(); ++i) {
            // Icons bigger for first two, labels smaller for first two to match visual style
            const float iconScale = (i < 2) ? 1.15f : 1.0f;
            const float labelScale = (i < 2) ? 0.55f : 1.0f;

            const float iconHeight = iconHeightBase * iconScale;
            const float labelHeight = labelHeightBase * labelScale;

            const float rowBaseY = contentTop - rowHeight * (static_cast<float>(i) + 1.f);
            const float iconY = rowBaseY + (rowHeight - iconHeight) * 0.5f;
            float labelY = rowBaseY + (rowHeight - labelHeight) * 0.5f;

            // --- Per-row vertical adjustments ---
            float labelOffsetY = 0.0f;

            if (i == 0)       labelOffsetY = -howToPopup.h * -0.05f;   // move DOWN slightly
            else if (i == 1)  labelOffsetY = -howToPopup.h * -0.075f;
            else if (i == 2)  labelOffsetY = -howToPopup.h * -0.08f;
            else if (i == 3)  labelOffsetY = -howToPopup.h * -0.04f;

            labelY += labelOffsetY;


            // Icon
            if (howToRows[i].iconTex) {
                const int frames = std::max(1, howToRows[i].frameCount);
                const int cols = std::max(1, howToRows[i].cols);
                const int rows = std::max(1, howToRows[i].rows);

                const float iconAspectVal = textureAspect(howToRows[i].iconTex, howToRows[i].iconAspectFallback) *
                    (static_cast<float>(rows) / static_cast<float>(cols));
                const float iconW = iconHeight * iconAspectVal;

                const float iconNudgeLeft = (i < 2) ? howToPopup.w * 0.12f : 0.0f;
                const float iconX = iconAnchorX - iconW - iconNudgeLeft;

                // --- Per-row vertical icon offset ---
                float iconOffsetY = 0.0f;

                if (i == 0)      iconOffsetY = howToPopup.h * 0.08f;
                else if (i == 1) iconOffsetY = howToPopup.h * 0.08f;
                else if (i == 2) iconOffsetY = howToPopup.h * 0.08f;
                else if (i == 3) iconOffsetY = howToPopup.h * 0.04f;

                float finalIconY = iconY + iconOffsetY;


                const float fps = howToRows[i].fps > 0.0f ? howToRows[i].fps : 8.0f;
                const int frameIndex = (frames > 1) ? (static_cast<int>(iconAnimTime * fps) % frames) : 0;

                gfx::Graphics::renderSpriteFrame(howToRows[i].iconTex,
                    iconX + iconW * 0.5f, finalIconY + iconHeight * 0.5f,
                    0.f, iconW, iconHeight,
                    frameIndex, cols, rows,
                    1.f, 1.f, 1.f, 1.f);
            }

            // Label
            if (howToRows[i].labelTex) {
                float labelOffsetX = 0.0f;

                if (i == 0)       labelOffsetX = howToPopup.w * 0.00f;
                else if (i == 1)  labelOffsetX = howToPopup.w * 0.02f;
                else if (i == 2)  labelOffsetX = howToPopup.w * 0.04f;
                else if (i == 3)  labelOffsetX = howToPopup.w * 0.04f;

                const float labelX = howToPopup.x + baseLeftPad + labelOffsetX;

                const float labelAspectVal = textureAspect(howToRows[i].labelTex, howToRows[i].labelAspectFallback);
                const float labelW = labelHeight * labelAspectVal;

                gfx::Graphics::renderSpriteUI(
                    howToRows[i].labelTex,
                    labelX, labelY,
                    labelW, labelHeight,
                    1.f, 1.f, 1.f, 1.f,
                    sw, sh
                );
            }

        }
        gfx::Graphics::resetViewProjection();
    }

    gui.Draw(render);
}

// Latch Consumers
/*************************************************************************************
  \brief  Consume the Start latch, if set.
  \return True if the Start button was clicked since last check.
*************************************************************************************/
bool MainMenuPage::ConsumeStart() { if (!startLatched) return false; startLatched = false; return true; }

/*************************************************************************************
  \brief  Consume the Options latch, if set.
  \return True if the Options button was clicked since last check.
*************************************************************************************/
bool MainMenuPage::ConsumeOptions() { if (!optionsLatched) return false; optionsLatched = false; return true; }

/*************************************************************************************
  \brief  Consume the How To Play latch, if set.
  \return True if the How To button was clicked since last check.
*************************************************************************************/
bool MainMenuPage::ConsumeHowToPlay() { if (!howToLatched) return false; howToLatched = false; return true; }

/*************************************************************************************
  \brief  Consume the Exit latch, if set.
  \return True if a confirmed Exit (Yes in popup) was latched.
*************************************************************************************/
bool MainMenuPage::ConsumeExit() { if (!exitLatched) return false; exitLatched = false; return true; }

/*************************************************************************************
  \brief  Compute button and popup rectangles based on current screen size.
  \param  screenW  Current screen width in pixels.
  \param  screenH  Current screen height in pixels.
  \details Uses layout parameters from main_menu_ui.json with a uniform scale factor
           to avoid stretching. Also computes:
           - How To popup note rect and close button.
           - Options popup rect, header, and mute toggle button.
           - Exit popup parchment, title, prompt, close box, and Yes/No buttons.
           Finally calls BuildGui() to rebuild GUI button hit regions.
*************************************************************************************/
void MainMenuPage::SyncLayout(int screenW, int screenH)
{
    if (layoutInitialized && screenW == sw && screenH == sh) return;

    sw = screenW;
    sh = screenH;

    const float baseW = 1280.f;
    const float baseH = 720.f;
    const float scaleX = static_cast<float>(sw) / baseW;
    const float scaleY = static_cast<float>(sh) / baseH;

    // Use uniform scale for buttons to prevent stretching in fullscreen
    const float uniformScale = std::min(scaleX, scaleY);

    // --- Dynamic Layout ---
    const auto& l = g_MenuConfig.layout;

    // Apply uniform scaling to buttons
    const float btnW = l.btnW * uniformScale * l.scale;
    const float btnH = l.btnH * uniformScale * l.scale;
    const float vSpace = l.spacing * uniformScale;

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
    auto textureAspect = [](unsigned tex, float fallback)
        {
            int texW = 0, texH = 0;
            if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0)
            {
                return static_cast<float>(texW) / static_cast<float>(texH);
            }
            return fallback;
        };

    optionsPopup = howToPopup;
    optionsCloseBtn = closeBtn;
    const float optionsHeaderH = popupH * 0.18f;
    const float optionsHeaderW = optionsHeaderH * textureAspect(optionsHeaderTex, 2.7f);
    optionsHeader = { popupX + (popupW - optionsHeaderW) * 0.5f,
        popupY + popupH - optionsHeaderH - popupH * 0.08f,
        optionsHeaderW, optionsHeaderH };

    const float toggleH = popupH * 0.14f;
    const float toggleW = popupW * 0.5f;
    muteToggleBtn = { popupX + (popupW - toggleW) * 0.5f,
        popupY + popupH * 0.32f,
        toggleW, toggleH };

    // --- Exit Popup Layout ---
    const float defaultExitAspect = 0.78f;
    int exitNoteW = 0, exitNoteH = 0;
    const bool hasExitNoteSize = exitPopupNoteTex && gfx::Graphics::getTextureSize(exitPopupNoteTex, exitNoteW, exitNoteH);
    const float exitNoteAspect = (hasExitNoteSize && exitNoteH > 0) ? static_cast<float>(exitNoteW) / static_cast<float>(exitNoteH) : defaultExitAspect;

    float exitPopupW = sw * 0.62f;
    float exitPopupH = exitPopupW / exitNoteAspect;
    const float maxExitPopupH = sh * 0.76f;
    if (exitPopupH > maxExitPopupH)
    {
        exitPopupH = maxExitPopupH;
        exitPopupW = exitPopupH * exitNoteAspect;
    }

    const float exitPopupX = (sw - exitPopupW) * 0.5f;
    const float exitPopupY = (sh - exitPopupH) * 0.5f;
    exitPopup = { exitPopupX, exitPopupY, exitPopupW, exitPopupH };

    const float exitTitleHeight = exitPopupH * 0.22f;
    const float exitTitleWidth = exitTitleHeight * textureAspect(exitPopupTitleTex, 2.7f);

    // Scaled nudge so it behaves nicely when F11 / resolution changes
    const float exitTitleNudgeLeft = exitPopupW * 0.10f;
    const float exitTitleNudgeUp = exitPopupH * 0.1f;

    exitTitle = {
        exitPopupX + (exitPopupW - exitTitleWidth) * 0.5f - exitTitleNudgeLeft,
        exitPopupY + exitPopupH - exitTitleHeight - exitPopupH * 0.08f + exitTitleNudgeUp,
        exitTitleWidth,
        exitTitleHeight
    };

    const float exitPromptHeight = exitPopupH * 0.20f;
    const float exitPromptWidth = exitPromptHeight * textureAspect(exitPopupPromptTex, 2.3f);


    const float exitPromptNudgeUp = exitPopupH * 0.056f;
    const float exitPromptY = exitPopupY
        + exitPopupH * 0.52f
        + exitPromptNudgeUp
        - exitPromptHeight * 0.5f;

    exitPrompt = {
        exitPopupX + (exitPopupW - exitPromptWidth) * 0.5f,
        exitPromptY,
        exitPromptWidth,
        exitPromptHeight
    };


    const float exitCloseSize = std::min(exitPopupW, exitPopupH) * 0.13f;
    exitCloseBtn = { exitPopupX + exitPopupW - exitCloseSize * 0.82f,
        exitPopupY + exitPopupH - exitCloseSize * 0.78f,
        exitCloseSize, exitCloseSize };

    const float exitBtnHeight = exitPopupH * 0.18f;
    const float exitYesAspect = textureAspect(exitPopupYesTex, 1.7f);
    const float exitNoAspect = textureAspect(exitPopupNoTex, 1.7f);
    const float exitYesWidth = exitBtnHeight * exitYesAspect;
    const float exitNoWidth = exitBtnHeight * exitNoAspect;
    const float exitBtnSpacing = exitPopupW * 0.06f;
    const float exitBtnCenter = exitPopupX + exitPopupW * 0.5f;
    const float exitBtnY = exitPopupY + exitPopupH * 0.18f;
    exitYesBtn = { exitBtnCenter - exitBtnSpacing * 0.5f - exitYesWidth + 20.0f, exitBtnY, exitYesWidth, exitBtnHeight };
    exitNoBtn = { exitBtnCenter + exitBtnSpacing * 0.5f, exitBtnY, exitNoWidth, exitBtnHeight };

    BuildGui(leftAlignedX, bottomY, btnW, btnH, vSpace);
    layoutInitialized = true;
}

/*************************************************************************************
  \brief  Rebuild the GUI after a layout change by forcing SyncLayout().
  \details Marks layout as uninitialized, then calls SyncLayout with the stored width
           and height so all button rectangles and popups are recomputed before
           recreating GUI buttons.
*************************************************************************************/
void MainMenuPage::BuildGui()
{
    layoutInitialized = false;
    SyncLayout(sw, sh);
}

/*************************************************************************************
  \brief  Populate GUI buttons for either active popups or the base main menu.
  \param  x        Left X coordinate for the button stack.
  \param  bottomY  Bottom Y coordinate of the button stack.
  \param  w        Button width in pixels.
  \param  h        Button height in pixels.
  \param  spacing  Vertical spacing between buttons in pixels.
  \details Modes:
           - Exit popup: show Yes/No/X only.
           - Options popup: show mute toggle and close X.
           - How To popup: show close X on parchment.
           - Base menu: Start / Options / Exit / How To mapped from JSON actions.
*************************************************************************************/
void MainMenuPage::BuildGui(float x, float bottomY, float w, float h, float spacing)
{
    gui.Clear();

    if (showExitPopup)
    {
        gui.AddButton(exitYesBtn.x, exitYesBtn.y, exitYesBtn.w, exitYesBtn.h, "YES",
            exitPopupYesTex, exitPopupYesTex,
            [this]()
            {
                exitLatched = true;
                showExitPopup = false;
                BuildGui();
            });

        gui.AddButton(exitNoBtn.x, exitNoBtn.y, exitNoBtn.w, exitNoBtn.h, "NO",
            exitPopupNoTex, exitPopupNoTex,
            [this]()
            {
                showExitPopup = false;
                BuildGui();
            });


        gui.AddButton(exitCloseBtn.x, exitCloseBtn.y, exitCloseBtn.w, exitCloseBtn.h, "",
            exitPopupCloseTex, exitPopupCloseTex,
            [this]()
            {
                showExitPopup = false;
                BuildGui();
            });

        return;
    }

    if (showOptionsPopup) {
        if (closePopupTex) {
            gui.AddButton(optionsCloseBtn.x, optionsCloseBtn.y, optionsCloseBtn.w, optionsCloseBtn.h, "",
                closePopupTex, closePopupTex,
                [this]() { showOptionsPopup = false; BuildGui(); });
        }

        const std::string muteLabel = (audioMuted ? "[X] " : "[ ] ") + std::string("Mute Audio");
        gui.AddButton(muteToggleBtn.x, muteToggleBtn.y, muteToggleBtn.w, muteToggleBtn.h, muteLabel,
            [this]() {
                audioMuted = !audioMuted;
                SoundManager::getInstance().setMasterVolume(audioMuted ? 0.0f : masterVolumeDefault);
                BuildGui();
            });
        return;
    }

    if (showHowToPopup) {
        if (closePopupTex) {
            gui.AddButton(closeBtn.x, closeBtn.y, closeBtn.w, closeBtn.h, "",
                closePopupTex, closePopupTex,
                [this]() { showHowToPopup = false; BuildGui(); });
        }
        return;
    }

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
        else if (btnDef.action == "options") callback = [this]() {
            optionsLatched = true;
            showOptionsPopup = true;
            showHowToPopup = false;
            showExitPopup = false;
            BuildGui();
            };
        else if (btnDef.action == "exit")    callback = [this]() {
            showExitPopup = true;
            showHowToPopup = false;
            showOptionsPopup = false;
            BuildGui();
            };
        else if (btnDef.action == "howto")   callback = [this]() {
            howToLatched = true;
            showHowToPopup = true;
            showOptionsPopup = false;
            iconAnimTime = 0.0f;
            iconTimerInitialized = false;
            BuildGui();
            };

        gui.AddButton(x, yPos, w, h, btnDef.label, tex, tex, callback);
    }
}
