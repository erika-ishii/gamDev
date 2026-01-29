/*********************************************************************************************
 \file      PauseMenuPage.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     In-game pause menu: parchment overlay with stylized buttons.
 \details   the pause overlay drawn over gameplay and its popups:
            - Main note: parchment background with "Paused" header and core buttons.
            - Buttons: Resume / Options / How To Play / Main Menu plus an X close box.
            - How To Play: animated icon+label rows driven by JSON (frame count, fps, aspects).
            - Options: simple audio mute toggle popup sharing the same parchment.
            - Exit popup: "Are you sure?" confirmation when returning to main menu.
            - JSON config: howto_popup.json / exit_popup.json override default texture keys/paths.
            - Layout: computes note/popup rectangles, buttons, and header offsets on resize.
            - GUI wiring: integrates with the lightweight GUI helper to dispatch button callbacks.
*********************************************************************************************/

#include "PauseMenuPage.hpp"
#include "Graphics/Graphics.hpp"
#include "Resource_Asset_Manager/Resource_Manager.h"
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
#include <array>
#include <vector>
#include <GLFW/glfw3.h>
#include "Common/CRTDebug.h"   

#ifdef _DEBUG
#define new DBG_NEW        
#endif

using namespace mygame;

namespace {
    struct TextureField {
        std::string key;
        std::string path;
    };

    struct HowToRowJson {
        TextureField icon;
        TextureField label;
        int frames = 0;            // 0 = derive from strip size
        float fps = 8.0f;
        float iconAspect = 1.0f;
        float labelAspect = 1.0f;
        int cols = 0;              // 0 = derive from frames
        int rows = 0;              // 0 or less = treated as 1 in Init
    };

    struct HowToPopupJson {
        TextureField background;
        TextureField header;
        TextureField close;
        // Offset fields to match JSON structure
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
    TextureField MakeTextureField(const std::string& key, const std::string& path)
    {
        return TextureField{ key, path };
    }

    /*************************************************************************************
      \brief  Build default "How To Play" popup config when JSON is missing or partial.
      \details Supplies parchment background, header, X close button, and 4 default rows:
               WASD / ESC / LMB / RMB, each with sprite and text texture hints.
               Frame counts are allowed to be auto-derived from sprite strips.
    *************************************************************************************/
    HowToPopupJson DefaultHowToPopupConfig()
    {
        HowToPopupJson config{};
        config.background = MakeTextureField("howto_note_bg", "Textures/UI/How To Play/Note.png");
        config.header = MakeTextureField("howto_header", "Textures/UI/How To Play/How To Play.png");
        config.close = MakeTextureField("menu_popup_close", "Textures/UI/How To Play/XButton.png");
        config.headerOffsetX = 0.0f;
        config.headerOffsetY = 0.0f;

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

    /*************************************************************************************
      \brief  Build default exit-confirmation popup config when JSON is missing or partial.
      \details Uses the same note parchment plus Exit/Are you sure?/Yes/No/X textures.
    *************************************************************************************/
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

    /*************************************************************************************
      \brief  Read a TextureField override from a JSON object.
      \param  obj  JSON object with optional "key" and "path" fields.
      \param  out  TextureField to write into.
      \return True if either key or path was present in the JSON.
    *************************************************************************************/
    bool PopulateTextureField(const nlohmann::json& obj, TextureField& out)
    {
        if (obj.contains("key")) out.key = obj["key"].get<std::string>();
        if (obj.contains("path")) out.path = obj["path"].get<std::string>();
        return obj.contains("key") || obj.contains("path");
    }

    /*************************************************************************************
      \brief  Load how-to popup config from JSON, falling back to defaults when missing.
      \details Probes a small list of candidate paths (Data_Files, resolved data root).
               If howToPopup exists, overrides background/header/close, header offsets,
               and per-row icon/label/animation/aspect data.
    *************************************************************************************/
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

            // Parse offsets
            if (root.contains("header_offset")) {
                const auto& off = root["header_offset"];
                if (off.contains("x")) config.headerOffsetX = off["x"];
                if (off.contains("y")) config.headerOffsetY = off["y"];
            }

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

    /*************************************************************************************
      \brief  Load exit popup config from JSON, falling back to defaults when missing.
      \details Probes a small set of candidate JSON paths under data/assets/Data_Files.
               On success overrides the parchment, title, prompt, X, Yes, and No textures.
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

    /*************************************************************************************
      \brief  Resolve a texture by trying a list of cache keys and a fallback path.
      \param  keys  Preferred Resource_Manager keys to probe in order.
      \param  path  Disk path used if keys are not already loaded.
      \return GL texture handle (non-zero) or a direct load from gfx::Graphics as last resort.
      \details Attempts:
               1) Resource_Manager::getTexture for each key.
               2) Resource_Manager::load on the first key and path.
               3) gfx::Graphics::loadTexture(path) if still missing.
    *************************************************************************************/
    unsigned ResolveTexture(const std::initializer_list<std::string>& keys, const std::string& path)
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

/*************************************************************************************
  \brief  Initialize textures, layout, and popup state for the pause menu.
  \param  screenW  Initial screen width in pixels.
  \param  screenH  Initial screen height in pixels.
  \details Loads how-to/exit popup JSON, resolves all parchment/button textures, builds
           per-row animation config, resets timers, and computes the initial layout.
*************************************************************************************/
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
 
    howToTex = ResolveTexture({ "pause_howto" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/How To Play.png").string());

    // UPDATED: Load "Main Menu" PNG instead of Quit
    mainMenuTex = ResolveTexture({ "pause_mainmenu" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/Main Menu.png").string());

    closeTex = ResolveTexture({ "pause_close", "pause_x" },
        Framework::ResolveAssetPath("Textures/UI/Pause Menu/XButton.png").string());

    howToNoteTex = ResolveTexture({ popupConfig.background.key },
        Framework::ResolveAssetPath(popupConfig.background.path).string());
    howToHeaderTex = ResolveTexture({ popupConfig.header.key },
        Framework::ResolveAssetPath(popupConfig.header.path).string());
    howToCloseTex = ResolveTexture({ popupConfig.close.key },
        Framework::ResolveAssetPath(popupConfig.close.path).string());
    optionsNoteTex = ResolveTexture({ "options_note" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Note.png").string());
    optionsHeaderTex = ResolveTexture({ "options_header" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Options.png").string());
    optionsCloseTex = ResolveTexture({ "options_close" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/XButton.png").string());
    optionsSliderTrackTex = ResolveTexture({ "options_slider_track" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Slider.png").string());
    optionsSliderFillTex = ResolveTexture({ "options_slider_fill" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Slider Fill.png").string());
    optionsSliderKnobTex = ResolveTexture({ "options_slider_knob" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Slider Button.png").string());
    optionsResetTex = ResolveTexture({ "options_reset" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Reset.png").string());
    optionsMasterLabelTex = ResolveTexture({ "options_master_label" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Master Volume.png").string());
    optionsBgmLabelTex = ResolveTexture({ "options_bgm_label" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Bgm.png").string());
    optionsSfxLabelTex = ResolveTexture({ "options_sfx_label" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Sfx.png").string());
    optionsBrightnessLabelTex = ResolveTexture({ "options_brightness_label" },
        Framework::ResolveAssetPath("Textures/UI/Options Menu/Brightness.png").string());

    // Load offsets
    howToHeaderOffsetX = popupConfig.headerOffsetX;
    howToHeaderOffsetY = popupConfig.headerOffsetY;

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

/*************************************************************************************
  \brief  Advance icon animation timer when How To popup is visible and update GUI.
  \param  input  Pointer to engine input system used by the GUI helper.
*************************************************************************************/
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
    if (showOptionsPopup) {
        GLFWwindow* window = glfwGetCurrentContext();
        if (window) {
            double mxLogical = 0.0;
            double myTopLogical = 0.0;
            glfwGetCursorPos(window, &mxLogical, &myTopLogical);
            int winW = 1, winH = 1;
            glfwGetWindowSize(window, &winW, &winH);
            int fbW = winW, fbH = winH;
            glfwGetFramebufferSize(window, &fbW, &fbH);

            const double scaleX = winW > 0 ? static_cast<double>(fbW) / static_cast<double>(winW) : 1.0;
            const double scaleY = winH > 0 ? static_cast<double>(fbH) / static_cast<double>(winH) : 1.0;
            const double mx = mxLogical * scaleX;
            const double my = (static_cast<double>(winH) - myTopLogical) * scaleY;

            const bool mouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
            if (!mouseDown) {
                optionsSliderDragging = false;
                optionsSliderDragIndex = -1;
            }
            else {
                if (!optionsSliderDragging) {
                    for (size_t i = 0; i < optionsSliderRects.size(); ++i) {
                        const auto& rect = optionsSliderRects[i];
                        const auto& knob = optionsSliderKnobRects[i];
                        const bool onTrack = (mx >= rect.x && mx <= rect.x + rect.w && my >= rect.y && my <= rect.y + rect.h);
                        const bool onKnob = (mx >= knob.x && mx <= knob.x + knob.w && my >= knob.y && my <= knob.y + knob.h);
                        if (onTrack || onKnob) {
                            optionsSliderDragging = true;
                            optionsSliderDragIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }

                if (optionsSliderDragging && optionsSliderDragIndex >= 0) {
                    const auto& rect = optionsSliderRects[optionsSliderDragIndex];
                    const float newValue = std::clamp(static_cast<float>((mx - rect.x) / rect.w), 0.0f, 1.0f);
                    optionsSliderValues[optionsSliderDragIndex] = newValue;
                    if (optionsSliderDragIndex == 0) {
                        audioMuted = (newValue <= 0.001f);
                        SoundManager::getInstance().setMasterVolume(newValue);
                    }
                    layoutDirty = true;
                }
            }
        }
    }
    else {
        optionsSliderDragging = false;
        optionsSliderDragIndex = -1;
    }
    gui.Update(input);
}

/*************************************************************************************
  \brief  Render the semi-opaque overlay, active popup, and GUI buttons.
  \param  render  RenderSystem pointer used to query screen size and text readiness.
  \details Always draws a dark full-screen fade, then one of:
           - Exit popup parchment with title/prompt and buttons.
           - Options popup parchment only (header drawn inside Draw()).
           - How To Play parchment + header + animated icon/label rows.
           - Base pause parchment with header and buttons when no popup is open.
*************************************************************************************/
void PauseMenuPage::Draw(Framework::RenderSystem* render)
{
    const int screenW = render ? render->ScreenWidth() : sw;
    const int screenH = render ? render->ScreenHeight() : sh;
    SyncLayout(screenW, screenH);

    // Dark Overlay (matches MainMenu alpha)
    gfx::Graphics::renderRectangleUI(0.f, 0.f, static_cast<float>(screenW), static_cast<float>(screenH),
        0.f, 0.f, 0.f, 0.65f, screenW, screenH);

    if (showExitPopup && render)
    {
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

        if (exitPopupPromptTex) {
            gfx::Graphics::renderSpriteUI(exitPopupPromptTex, exitPrompt.x, exitPrompt.y, exitPrompt.w, exitPrompt.h,
                1.f, 1.f, 1.f, 1.f, sw, sh);
        }
    }
    else if (showOptionsPopup && render)
    {
        // OPTIONS POPUP: Stylized parchment with slider art
        if (optionsNoteTex) {
            gfx::Graphics::renderSpriteUI(optionsNoteTex, optionsPopup.x, optionsPopup.y,
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
        if (optionsHeaderTex) {
            gfx::Graphics::renderSpriteUI(optionsHeaderTex, optionsHeader.x, optionsHeader.y,
                optionsHeader.w, optionsHeader.h,
                1.f, 1.f, 1.f, 1.f,
                sw, sh);
        }

        const std::array<unsigned, 4> labelTextures = {
            optionsMasterLabelTex,
            optionsBgmLabelTex,
            optionsSfxLabelTex,
            optionsBrightnessLabelTex
        };

        for (size_t i = 0; i < optionsLabelRects.size(); ++i) {
            if (labelTextures[i]) {
                const auto& labelRect = optionsLabelRects[i];
                gfx::Graphics::renderSpriteUI(labelTextures[i], labelRect.x, labelRect.y,
                    labelRect.w, labelRect.h,
                    1.f, 1.f, 1.f, 1.f,
                    sw, sh);
            }

            if (optionsSliderTrackTex) {
                const auto& sliderRect = optionsSliderRects[i];
                gfx::Graphics::renderSpriteUI(optionsSliderTrackTex, sliderRect.x, sliderRect.y,
                    sliderRect.w, sliderRect.h,
                    1.f, 1.f, 1.f, 1.f,
                    sw, sh);
            }

            if (optionsSliderFillTex) {
                const auto& fillRect = optionsSliderFillRects[i];
                gfx::Graphics::renderSpriteUI(optionsSliderFillTex, fillRect.x, fillRect.y,
                    fillRect.w, fillRect.h,
                    1.f, 1.f, 1.f, 1.f,
                    sw, sh);
            }

            if (optionsSliderKnobTex) {
                const auto& knobRect = optionsSliderKnobRects[i];
                gfx::Graphics::renderSpriteUI(optionsSliderKnobTex, knobRect.x, knobRect.y,
                    knobRect.w, knobRect.h,
                    1.f, 1.f, 1.f, 1.f,
                    sw, sh);
            }
        }

        if (optionsResetTex) {
            gfx::Graphics::renderSpriteUI(optionsResetTex, optionsResetBtn.x, optionsResetBtn.y,
                optionsResetBtn.w, optionsResetBtn.h,
                1.f, 1.f, 1.f, 1.f,
                sw, sh);
        }
    }
    else if (showHowToPopup && render) {
        auto textureAspect = [](unsigned tex, float fallback) {
            int texW = 0, texH = 0;
            if (tex && gfx::Graphics::getTextureSize(tex, texW, texH) && texH > 0) {
                return static_cast<float>(texW) / static_cast<float>(texH);
            }
            return fallback;
            };

        // Note Background (same parchment as MainMenu)
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

        // --- HEADER (mirrors MainMenuPage) ---
        const float headerPadY = howToPopup.h * 0.07f;
        const float headerHeight = howToPopup.h * 0.16f;
        const float headerAspect = textureAspect(howToHeaderTex, 2.6f);
        const float headerWidth = headerHeight * headerAspect;

        // Offset Calculation (same scaling logic as MainMenu)
        const float offsetScale = howToPopup.w / (1280.0f * 0.58f);
        const float scaledOffsetX = howToHeaderOffsetX * offsetScale;
        const float scaledOffsetY = howToHeaderOffsetY * offsetScale;

        // Extra adjustments to move header LEFT and UP (synchronized with MainMenu)
        const float extraMoveLeft = 30.0f * offsetScale;
        const float extraMoveUp = 25.0f * offsetScale;

        const float headerX = howToPopup.x + (howToPopup.w - headerWidth) * 0.5f + scaledOffsetX - extraMoveLeft;
        const float headerY = howToPopup.y + howToPopup.h - headerHeight - headerPadY + scaledOffsetY + extraMoveUp;

        if (howToHeaderTex) {
            gfx::Graphics::renderSpriteUI(howToHeaderTex, headerX, headerY,
                headerWidth, headerHeight,
                1.f, 1.f, 1.f, 1.f,
                sw, sh);
        }

        // --- CONTENT ROWS (mirrors MainMenuPage layout) ---
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
            // Icons bigger for first two, labels slightly smaller for first two (same as MainMenu)
            const float iconScale = (i < 2) ? 1.15f : 1.0f;
            const float labelScale = (i < 2) ? 0.55f : 1.0f;

            const float iconHeight = iconHeightBase * iconScale;
            const float labelHeight = labelHeightBase * labelScale;

            const float rowBaseY = contentTop - rowHeight * (static_cast<float>(i) + 1.f);
            const float iconY = rowBaseY + (rowHeight - iconHeight) * 0.5f;
            float labelY = rowBaseY + (rowHeight - labelHeight) * 0.5f;

            // Per-row label vertical tweaks (copied from MainMenu)
            float labelOffsetY = 0.0f;
            if (i == 0)       labelOffsetY = -howToPopup.h * -0.05f;
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

                // Nudge WASD/ESC left (same as MainMenu)
                const float iconNudgeLeft = (i < 2) ? howToPopup.w * 0.12f : 0.0f;
                const float iconX = iconAnchorX - iconW - iconNudgeLeft;

                // Per-row icon vertical tweaks (copied from MainMenu)
                float iconOffsetY = 0.0f;
                if (i == 0)      iconOffsetY = howToPopup.h * 0.08f;
                else if (i == 1) iconOffsetY = howToPopup.h * 0.08f;
                else if (i == 2) iconOffsetY = howToPopup.h * 0.08f;
                else if (i == 3) iconOffsetY = howToPopup.h * 0.04f;

                float finalIconY = iconY + iconOffsetY;

                const float fps = howToRows[i].fps > 0.0f ? howToRows[i].fps : 8.0f;
                const int frameIndex = (frames > 1)
                    ? (static_cast<int>(iconAnimTime * fps) % frames)
                    : 0;

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

/*************************************************************************************
  \brief  Consume the "Resume" latch, if set.
  \return True if a resume click was recorded this frame.
*************************************************************************************/
bool PauseMenuPage::ConsumeResume()
{
    if (!resumeLatched) return false;
    resumeLatched = false;
    return true;
}

/*************************************************************************************
  \brief  Consume the "Main Menu" latch, if set.
  \return True if the Main Menu button was clicked since last check.
*************************************************************************************/
bool PauseMenuPage::ConsumeMainMenu()
{
    if (!mainMenuLatched) return false;
    mainMenuLatched = false;
    return true;
}

/*************************************************************************************
  \brief  Consume the "Options" latch, if set.
  \return True if the Options button was clicked since last check.
*************************************************************************************/
bool PauseMenuPage::ConsumeOptions()
{
    if (!optionsLatched) return false;
    optionsLatched = false;
    return true;
}

/*************************************************************************************
  \brief  Consume the "How To Play" latch, if set.
  \return True if the How To Play button was clicked since last check.
*************************************************************************************/
bool PauseMenuPage::ConsumeHowToPlay()
{
    if (!howToLatched) return false;
    howToLatched = false;
    return true;
}

/*************************************************************************************
  \brief  Consume the quit-request latch (request to show exit popup).
  \return True if a quit request was recorded since last check.
*************************************************************************************/
bool PauseMenuPage::ConsumeQuitRequest()
{
    if (!quitRequestedLatched) return false;
    quitRequestedLatched = false;
    return true;
}

/*************************************************************************************
  \brief  Consume the exit-confirmed latch.
  \return True if "Yes" was pressed in the exit popup since last check.
*************************************************************************************/
bool PauseMenuPage::ConsumeExitConfirmed()
{
    if (!exitConfirmedLatched) return false;
    exitConfirmedLatched = false;
    return true;
}

/*************************************************************************************
  \brief  Clear all button latches and popup visibility flags.
  \details Also resets animation timers and marks the layout as dirty so that the next
           SyncLayout() call recomputes rectangles for the current screen size.
*************************************************************************************/
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

/*************************************************************************************
  \brief  Activate the exit confirmation popup.
  \details Clears quit/confirm latches, hides other popups, and rebuilds the GUI so
           that only Yes/No/X buttons are active.
*************************************************************************************/
void PauseMenuPage::ShowExitPopup()
{
    quitRequestedLatched = false;
    exitConfirmedLatched = false;
    showExitPopup = true;
    showHowToPopup = false;
    showOptionsPopup = false;
    BuildGui();
}

/*************************************************************************************
  \brief  Compute rectangles for parchment, headers, popups, and buttons.
  \param  screenW  Current screen width in pixels.
  \param  screenH  Current screen height in pixels.
  \details Uses texture aspect ratios where available; clamps note/popup height to avoid
           oversizing; positions buttons in a vertical stack and centers popups. Also
           updates options/exit popup controls and triggers a GUI rebuild.
*************************************************************************************/
void PauseMenuPage::SyncLayout(int screenW, int screenH)
{
    const bool sizeChanged = (screenW != sw) || (screenH != sh);
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

    const float topPad = note.h * 0.20f;
    int headerWpx = 0, headerHpx = 0;
    const float defaultHeaderAspect = 2.7f;
    const float headerAspect = (headerTex && gfx::Graphics::getTextureSize(headerTex, headerWpx, headerHpx) && headerHpx > 0)
        ? static_cast<float>(headerWpx) / static_cast<float>(headerHpx)
        : defaultHeaderAspect;
    float headerW = note.w * 0.55f;
    float headerH = headerW / headerAspect;

    header = { note.x + (note.w - headerW) * 0.5f - (headerW * 0.23f),
               note.y + note.h - headerH * 1.0f,
               headerW, headerH };

    const float closeSize = headerH * 0.72f;
    const float closeOffsetX = closeSize * 2.6f;
    const float closeOffsetY = closeSize * 1.1f;
    closeBtn = { note.x + note.w - closeOffsetX, note.y + note.h - closeOffsetY, closeSize, closeSize };

    int buttonWpx = 0, buttonHpx = 0;
    const float defaultButtonAspect = 3.5f;
    const float buttonAspect = (resumeTex && gfx::Graphics::getTextureSize(resumeTex, buttonWpx, buttonHpx) && buttonHpx > 0)
        ? static_cast<float>(buttonWpx) / static_cast<float>(buttonHpx)
        : defaultButtonAspect;

    // Buttons smaller and centered
    const float btnW = note.w * 0.40f;
    const float btnH = btnW / buttonAspect;
    const float spacing = btnH * 0.28f;

    const float moveLeftAmount = note.w * 0.04f;
    const float moveDownAmount = note.h * 0.08f;

    const float firstBtnY = note.y + note.h - topPad - btnH - moveDownAmount;
    const float btnX = note.x + (note.w - btnW) * 0.5f - moveLeftAmount;

    // scale nudges with note height
    const float resumeNudge = note.h * 0.03f;
    const float optionsNudge = 0.0f;
    const float howToNudge = -note.h * 0.03f;
    const float quitNudge = -note.h * 0.06f;

    // Resume
    resumeBtn = {
        btnX,
        firstBtnY + resumeNudge,
        btnW, btnH
    };

    // Options
    optionsBtn = {
        btnX,
        firstBtnY - (spacing + btnH) + optionsNudge,
        btnW, btnH
    };

    // How To Play
    howToBtn = {
        btnX,
        firstBtnY - 2.f * (spacing + btnH) + howToNudge,
        btnW, btnH
    };

    // Main Menu
    quitBtn = {
        btnX,
        firstBtnY - 3.f * (spacing + btnH) + quitNudge,
        btnW, btnH
    };

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

    int optionsNoteW = 0, optionsNoteH = 0;
    const bool hasOptionsNoteSize = optionsNoteTex && gfx::Graphics::getTextureSize(optionsNoteTex, optionsNoteW, optionsNoteH);
    const float optionsAspect = (hasOptionsNoteSize && optionsNoteH > 0)
        ? static_cast<float>(optionsNoteW) / static_cast<float>(optionsNoteH)
        : defaultPopupAspect;

    float optionsPopupW = static_cast<float>(sw) * 0.58f;
    float optionsPopupH = optionsPopupW / optionsAspect;
    const float maxOptionsPopupH = static_cast<float>(sh) * 0.82f;
    if (optionsPopupH > maxOptionsPopupH) {
        optionsPopupH = maxOptionsPopupH;
        optionsPopupW = optionsPopupH * optionsAspect;
    }

    const float optionsPopupX = (static_cast<float>(sw) - optionsPopupW) * 0.58f;
    const float optionsPopupY = (static_cast<float>(sh) - optionsPopupH) * 0.5f;

    optionsPopup = { optionsPopupX, optionsPopupY, optionsPopupW, optionsPopupH };
    const float optionsCloseSize = std::min(optionsPopupW, optionsPopupH) * 0.14f;
    optionsCloseBtn = { optionsPopupX + optionsPopupW - optionsCloseSize * 0.85f,
        optionsPopupY + optionsPopupH - optionsCloseSize * 0.75f,
        optionsCloseSize,
        optionsCloseSize };

    const float optionsHeaderH = optionsPopupH * 0.18f;
    const float optionsHeaderW = optionsHeaderH * textureAspect(optionsHeaderTex, 2.7f);
    optionsHeader = { optionsPopupX + (optionsPopupW - optionsHeaderW) * 0.5f,
        optionsPopupY + optionsPopupH - optionsHeaderH - optionsPopupH * 0.08f,
        optionsHeaderW, optionsHeaderH };

    const float contentTop = optionsHeader.y - optionsPopupH * 0.05f;
    const float contentBottom = optionsPopupY + optionsPopupH * 0.2f;
    const float availableHeight = std::max(0.1f, contentTop - contentBottom);
    const float rowHeight = availableHeight / 4.0f;
    const float labelHeightBase = rowHeight * 0.42f;
    const float sliderHeight = rowHeight * 0.18f;
    const float labelX = optionsPopupX + optionsPopupW * 0.18f;
    const float sliderX = optionsPopupX + optionsPopupW * 0.18f;
    const float sliderW = optionsPopupW * 0.64f;

    const std::array<unsigned, 4> labelTextures = {
        optionsMasterLabelTex,
        optionsBgmLabelTex,
        optionsSfxLabelTex,
        optionsBrightnessLabelTex
    };

    for (size_t i = 0; i < optionsLabelRects.size(); ++i) {
        const float rowBaseY = contentTop - rowHeight * (static_cast<float>(i) + 1.f);
        const float labelH = labelHeightBase;
        const float labelAspect = textureAspect(labelTextures[i], 2.6f);
        const float labelW = labelH * labelAspect;
        const float labelY = rowBaseY + rowHeight * 0.52f;
        const float sliderY = rowBaseY + rowHeight * 0.18f;

        optionsLabelRects[i] = { labelX, labelY, labelW, labelH };
        optionsSliderRects[i] = { sliderX, sliderY, sliderW, sliderHeight };

        const float knobSize = sliderHeight * 2.1f;
        const float value = std::clamp(optionsSliderValues[i], 0.0f, 1.0f);
        const float fillW = sliderW * value;
        const float knobX = sliderX + fillW - knobSize * 0.5f;
        optionsSliderFillRects[i] = { sliderX, sliderY, fillW, sliderHeight };
        optionsSliderKnobRects[i] = { knobX, sliderY - (knobSize - sliderHeight) * 0.5f, knobSize, knobSize };
    }

    const float resetH = optionsPopupH * 0.14f;
    const float resetW = resetH * textureAspect(optionsResetTex, 2.5f);
    optionsResetBtn = { optionsPopupX + (optionsPopupW - resetW) * 0.5f,
        optionsPopupY + optionsPopupH * 0.06f,
        resetW, resetH };

    const float toggleH = optionsPopupH * 0.14f;
    const float toggleW = optionsPopupW * 0.5f;
    muteToggleBtn = { optionsPopupX + (optionsPopupW - toggleW) * 0.5f,
        optionsPopupY + optionsPopupH * 0.32f,
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
    layoutDirty = false;
}

/*************************************************************************************
  \brief  Populate the GUI helper with buttons for the current popup/mode.
  \details Only one of exit/options/how-to or the base pause menu is active at a time:
           - Exit popup: Yes/No/X buttons.
           - Options popup: mute toggle and X close.
           - How To: X close only (content is purely visual).
           - Base pause: Resume / Options / How To Play / Main Menu / X to resume.
*************************************************************************************/
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
        if (optionsCloseTex)
        {
            gui.AddButton(optionsCloseBtn.x, optionsCloseBtn.y, optionsCloseBtn.w, optionsCloseBtn.h, "",
                optionsCloseTex, optionsCloseTex,
                [this]() {
                    showOptionsPopup = false;
                    layoutDirty = true;
                    BuildGui();
                }, true);
        }

        if (optionsResetTex) {
            gui.AddButton(optionsResetBtn.x, optionsResetBtn.y, optionsResetBtn.w, optionsResetBtn.h, "",
                optionsResetTex, optionsResetTex,
                [this]() {
                    optionsSliderValues = { {0.8f, 0.65f, 0.7f, 0.75f} };
                    audioMuted = false;
                    SoundManager::getInstance().setMasterVolume(optionsSliderValues[0]);
                    layoutDirty = true;
                    BuildGui();
                });
        }
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

        gui.AddButton(quitBtn.x, quitBtn.y, quitBtn.w, quitBtn.h, "Main Menu",
            mainMenuTex, mainMenuTex,
            [this]() {
                mainMenuLatched = true;
            });

        gui.AddButton(closeBtn.x, closeBtn.y, closeBtn.w, closeBtn.h, "",
            closeTex, closeTex,
            [this]() { resumeLatched = true; });
    }
}
