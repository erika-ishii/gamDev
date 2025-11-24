/*********************************************************************************************
 \file      RenderSystem.cpp
 \par       SofaSpuds
 \author    yimo.kong ( yimo.kong@digipen.edu) - Primary Author, 50%
            erika.ishii (erika.ishii@digipen.edu) - Author, 30%
            elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 10%
            h.jun (h.jun@digipen.edu) - Author, 10%

 \brief     Editor/game viewport orchestration: camera control, picking/dragging,
            split-view docking, ImGui panels, asset import plumbings, and frame submit.
 \details   The RenderSystem coordinates how the scene is viewed and interacted with:
            - Viewports: computes game viewport (split width/height) and restores full window.
            - Cameras: editor camera (pan/zoom/frame selection) and follow camera for gameplay.
            - Picking/Drag: screen→world unproject, object hit-testing, and drag with offsets.
            - Rendering: sets VP matrices, submits background and batched sprites, draws UI text.
            - Editor UI: dockspace host, viewport controls, asset browser, JSON editor, panels.
            - Imports: handles OS file drops and refreshes textures used by sprite components.
            - Lifecycle: initialize(), per-frame draw(), shutdown(), and menu-frame helpers.
            Uses Graphics.cpp for GPU work (VAOs/shaders/sprite draw) and ImGui for tools.
            Camera math relies on GLM; input comes via GLFW.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#if defined(APIENTRY)
#  undef APIENTRY
#endif
#include <Windows.h>
// Avoid Win32 macro collisions with your engine API
#ifdef SendMessage
#  undef SendMessage
#endif
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

#include "RenderSystem.h"
#include "Core/PathUtils.h"
#include <imgui.h>

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <system_error>
#include <vector>
#include <limits>
#include <unordered_set>
#include <unordered_map>
#include "Debug/AudioImGui.h"
#include "Debug/UndoStack.h"
#include "Debug/Inspector.h"
#include "Debug/EditorGizmo.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp> // for glm::inverse (used in ScreenToWorld)
#include <glm/gtc/matrix_transform.hpp>
#include "Physics/Dynamics/RigidBodyComponent.h"
#include "../../Sandbox/MyGame/Game.hpp"

namespace Framework {

    RenderSystem* RenderSystem::sInstance = nullptr;
    RenderSystem* RenderSystem::Get()
    {
        return sInstance;
    }

    namespace {
        using clock = std::chrono::high_resolution_clock;

        inline std::string ToLower(std::string value)
        {
            std::string out;
            out.resize(value.size());
            std::transform(value.begin(), value.end(), out.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        // Camera follow drag-lock state lives only in this translation unit.
        // We lock camera follow while dragging the Player so screen->world mapping stays stable.
        bool       gCameraFollowLocked = false;
        glm::vec2  gCameraLockPos = glm::vec2(0.0f, 0.0f);

        // Helper to test if an object is "Player" by name.
        inline bool IsPlayerObject(Framework::GOC* obj) {
            return obj && obj->GetObjectName() == "Player";
        }

        // Optional helper to zero rigid body velocity if such fields exist.
        inline void ZeroRigidBodyVelocityIfPresent(Framework::GOC* obj) {
            if (!obj) return;
            if (auto* rb = obj->GetComponentType<Framework::RigidBodyComponent>(
                Framework::ComponentTypeId::CT_RigidBodyComponent))
            {
                // If needed, zero velocity here:
                // rb->velX = 0.0f;
                // rb->velY = 0.0f;
            }
        }

        inline void RefreshSpriteComponentsForKey(const std::string& key)
        {
            if (key.empty() || !FACTORY)
                return;

            const unsigned handle = Resource_Manager::getTexture(key);
            if (handle == 0)
                return;

            for (auto& [id, objPtr] : FACTORY->Objects())
            {
                (void)id;
                auto* obj = objPtr.get();
                if (!obj)
                    continue;

                if (auto* sprite = obj->GetComponentType<Framework::SpriteComponent>(
                    Framework::ComponentTypeId::CT_SpriteComponent))
                {
                    if (sprite->texture_key == key)
                        sprite->texture_id = handle;
                }
            }
        }

        // After undo/redo, make sure all components that use texture_key are
        // bound to a valid GL texture handle again. This keeps sprites/rects
        // from showing the wrong texture or going "crazy" after an undo.
        inline void RebindAllComponentTextures()
        {
            if (!FACTORY)
                return;

            for (auto& [id, objPtr] : FACTORY->Objects())
            {
                (void)id;
                auto* obj = objPtr.get();
                if (!obj)
                    continue;

                if (auto* sprite = obj->GetComponentType<Framework::SpriteComponent>(
                    Framework::ComponentTypeId::CT_SpriteComponent))
                {
                    if (!sprite->texture_key.empty())
                    {
                        const unsigned handle = Resource_Manager::getTexture(sprite->texture_key);
                        if (handle != 0)
                            sprite->texture_id = handle;
                    }
                }

                if (auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                    Framework::ComponentTypeId::CT_RenderComponent))
                {
                    if (!rc->texture_key.empty())
                    {
                        const unsigned handle = Resource_Manager::getTexture(rc->texture_key);
                        if (handle != 0)
                            rc->texture_id = handle;
                    }
                }

                if (auto* anim = obj->GetComponentType<Framework::SpriteAnimationComponent>(
                    Framework::ComponentTypeId::CT_SpriteAnimationComponent))
                {
                    anim->RebindAllTextures();


                    if (auto* sprite = obj->GetComponentType<Framework::SpriteComponent>(
                        Framework::ComponentTypeId::CT_SpriteComponent))
                    {
                        auto sample = anim->CurrentSheetSample();
                        if (!sample.textureKey.empty())
                            sprite->texture_key = sample.textureKey;
                        if (sample.texture)
                            sprite->texture_id = sample.texture;
                    }
                }
            }
        }
    } // anonymous namespace

    RenderSystem::RenderSystem(gfx::Window& window, LogicSystem& logic)
        : window(&window), logic(logic) {
        sInstance = this;
        // Initialize camera with a default view height so that projection is valid early.
        camera.SetViewHeight(cameraViewHeight);
        editorCameraViewHeight = cameraViewHeight;
        editorCamera.SetViewHeight(editorCameraViewHeight);
    }

    
    /*************************************************************************************
      \brief  Probe for a Roboto font file in common asset locations.
      \return Absolute/relative path string to a usable Roboto .ttf, or empty if not found.
      \details Tries several relative paths and ascends parents to locate assets/Fonts.
    *************************************************************************************/
    std::string RenderSystem::FindRoboto() const
    {
        namespace fs = std::filesystem;

        const char* rels[] = {
            "assets/Fonts/Roboto-Black.ttf",
            "assets/Fonts/Roboto-Regular.ttf",
            "assets/Fonts/Roboto-VariableFont_wdth,wght.ttf",
            "assets/Fonts/Roboto-Italic-VariableFont_wdth,wght.ttf",
            "../assets/Fonts/Roboto-Black.ttf",
            "../assets/Fonts/Roboto-Regular.ttf",
            "../assets/Fonts/Roboto-VariableFont_wdth,wght.ttf",
            "../assets/Fonts/Roboto-Italic-VariableFont_wdth,wght.ttf",
            "../../assets/Fonts/Roboto-Black.ttf",
            "../../assets/Fonts/Roboto-Regular.ttf",
            "../../assets/Fonts/Roboto-VariableFont_wdth,wght.ttf",
            "../../assets/Fonts/Roboto-Italic-VariableFont_wdth,wght.ttf",
            "../../../assets/Fonts/Roboto-Black.ttf",
            "../../../assets/Fonts/Roboto-Regular.ttf",
            "../../../assets/Fonts/Roboto-VariableFont_wdth,wght.ttf",
            "../../../assets/Fonts/Roboto-Italic-VariableFont_wdth,wght.ttf"
        };
        // Prefer resolved asset roots when available (packaged builds).
        const auto resolvedBlack = Framework::ResolveAssetPath("Fonts/Roboto-Black.ttf");
        if (fs::exists(resolvedBlack))
            return resolvedBlack.string();
        const auto resolvedRegular = Framework::ResolveAssetPath("Fonts/Roboto-Regular.ttf");
        if (fs::exists(resolvedRegular))
            return resolvedRegular.string();

        for (auto r : rels)
            if (fs::exists(r))
                return std::string(r);

        std::vector<fs::path> roots{ fs::current_path(), Framework::GetExecutableDir() };

        auto try_pick = [&](const fs::path& fontsDir) -> std::string {
            fs::path rb = fontsDir / "Roboto-Black.ttf";
            if (fs::exists(rb)) return rb.string();
            fs::path rr = fontsDir / "Roboto-Regular.ttf";
            if (fs::exists(rr)) return rr.string();
            if (fs::exists(fontsDir))
            {
                for (auto& e : fs::directory_iterator(fontsDir))
                {
                    if (!e.is_regular_file()) continue;
                    auto name = e.path().filename().string();
                    if (name.rfind("Roboto", 0) == 0 && e.path().extension() == ".ttf")
                        return e.path().string();
                }
            }
            return {};
            };

        for (const auto& root : roots)
        {
            auto p = root;
            for (int up = 0; up < 7 && !p.empty(); ++up)
            {
                auto base = p / "assets" / "Fonts";
                if (auto picked = try_pick(base); !picked.empty())
                    return picked;
                p = p.parent_path();
            }
        }

        return {};
    }

    /*************************************************************************************
      \brief  Locate the canonical assets/ directory.
      \return Canonical path if found; otherwise an empty path.
      \details Walks up from CWD and exe directory, checking for an 'assets' folder.
    *************************************************************************************/
    std::filesystem::path RenderSystem::FindAssetsRoot() const
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> roots{ fs::current_path(), Framework::GetExecutableDir() };

        for (const auto& root : roots)
        {
            if (root.empty())
                continue;

            auto probe = root;
            for (int up = 0; up < 7 && !probe.empty(); ++up)
            {
                fs::path candidate = probe / "assets";
                std::error_code ec;
                if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec))
                    return fs::weakly_canonical(candidate, ec);
                probe = probe.parent_path();
            }
        }

        return {};
    }

    /*************************************************************************************
      \brief  Locate the canonical Data_Files/ directory.
      \return Canonical path if found; otherwise an empty path.
      \details Similar to FindAssetsRoot(), but probes for 'Data_Files' plus a few fallbacks.
    *************************************************************************************/
    std::filesystem::path RenderSystem::FindDataFilesRoot() const
    {
        namespace fs = std::filesystem;

        auto directory_exists = [](const fs::path& candidate) -> bool {
            std::error_code ec;
            return fs::exists(candidate, ec) && fs::is_directory(candidate, ec);
            };

        std::vector<fs::path> roots{ fs::current_path(), Framework::GetExecutableDir() };


        for (const auto& root : roots)
        {
            if (root.empty())
                continue;

            auto probe = root;
            for (int up = 0; up < 7 && !probe.empty(); ++up)
            {
                fs::path candidate = probe / "Data_Files";
                if (directory_exists(candidate))
                {
                    std::error_code canonicalEc;
                    auto canonical = fs::weakly_canonical(candidate, canonicalEc);
                    return canonicalEc ? candidate : canonical;
                }
                probe = probe.parent_path();
            }
        }

        static const char* rels[] = {
            "Data_Files",
            "../Data_Files",
            "../../Data_Files",
            "../../../Data_Files"
        };

        for (auto rel : rels)
        {
            fs::path candidate = rel;
            if (directory_exists(candidate))
            {
                std::error_code canonicalEc;
                auto canonical = fs::weakly_canonical(candidate, canonicalEc);
                return canonicalEc ? candidate : canonical;
            }
        }

        return {};
    }

    /*************************************************************************************
      \brief  Choose the current player sprite texture (idle vs run) based on animation state.
      \return GL texture handle of the active player sprite sheet.
    *************************************************************************************/
    unsigned RenderSystem::CurrentPlayerTexture() const
    {
        const auto& anim = logic.Animation();
        using Mode = LogicSystem::AnimationInfo::Mode;

        switch (anim.mode)
        {
        case Mode::Run:
            return runTex ? runTex : idleTex;
        case Mode::Attack1:
            return attackTex[0] ? attackTex[0] : idleTex;
        case Mode::Attack2:
            return attackTex[1] ? attackTex[1] : idleTex;
        case Mode::Attack3:
            return attackTex[2] ? attackTex[2] : idleTex;
        case Mode::Idle:
        default:
            return idleTex ? idleTex : playerTex;
        }
    }

    /*************************************************************************************
      \brief  Queue external files dropped from the OS into the Asset Browser.
      \param  count  Number of dropped paths.
      \param  paths  Array of UTF-8 file path strings.
      \details Converts to std::filesystem::path and hands over to AssetBrowser for import.
    *************************************************************************************/
    void RenderSystem::HandleFileDrop(int count, const char** paths)
    {
        if (count <= 0 || !paths || assetsRoot.empty())
            return;

        std::vector<std::filesystem::path> dropped;
        dropped.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            if (paths[i])
                dropped.emplace_back(paths[i]);
        }

        if (!dropped.empty())
            assetBrowser.QueueExternalFiles(dropped);
    }

    /*************************************************************************************
      \brief  Handle assets that were just imported (textures/audio) and refresh live sprites.
      \details For textures, refreshes SpriteComponent texture_id when keys match.
    *************************************************************************************/
    void RenderSystem::ProcessImportedAssets()
    {
        if (assetsRoot.empty())
            return;

        auto pending = assetBrowser.ConsumePendingImports();
        if (pending.empty())
            return;

        std::unordered_set<std::string> processed;
        for (const auto& relative : pending)
        {
            std::string key = relative.generic_string();
            if (key.empty() || !processed.insert(key).second)
                continue;

            std::error_code ec;
            auto absolute = std::filesystem::weakly_canonical(assetsRoot / relative, ec);
            if (ec)
                absolute = assetsRoot / relative;

            if (!std::filesystem::exists(absolute, ec) || !std::filesystem::is_regular_file(absolute, ec))
                continue;

            std::string ext = ToLower(absolute.extension().string());
            const bool isTexture = (ext == ".png" || ext == ".jpg" || ext == ".jpeg");
            const bool isAudio = (ext == ".wav" || ext == ".mp3");

            if (!isTexture && !isAudio)
                continue;

            if (Resource_Manager::load(key, absolute.string()))
            {
                if (isTexture)
                {
                    RefreshSpriteComponentsForKey(key);
                }
            }
        }
    }

    /*************************************************************************************
      \brief  Keyboard shortcuts for toggling editor/fullscreen and framing selection.
      \details F10 toggles editor panels; F11 toggles fullscreen; F frames selection
               (only in editor camera mode). Ctrl+Z triggers Undo.
    *************************************************************************************/
    void RenderSystem::HandleShortcuts()
    {
        if (!window)
            return;

        GLFWwindow* native = window->raw();
        if (!native)
            return;

        ImGuiIO& io = ImGui::GetIO();
        auto handleToggle = [&](int key, bool& held)
            {
                const bool pressed = glfwGetKey(native, key) == GLFW_PRESS;
                const bool triggered = pressed && !held;
                held = pressed;
                return triggered;
            };

        // Toggle editor panels
        if (handleToggle(GLFW_KEY_F10, editorToggleHeld))
            showEditor = !showEditor;

        // Toggle OS fullscreen always (editor or not)
        if (handleToggle(GLFW_KEY_F11, fullscreenToggleHeld))
        {
            window->ToggleFullscreen();
            screenW = window->Width();
            screenH = window->Height();
        }

        if (ShouldUseEditorCamera())
        {
            if (handleToggle(GLFW_KEY_F, editorFrameHeld))
                FrameEditorSelection();
        }
        else
        {
            // Keep state accurate so next editor activation treats F as a fresh press.
            editorFrameHeld = glfwGetKey(native, GLFW_KEY_F) == GLFW_PRESS;
        }

        if (showEditor)
        {
            if (handleToggle(GLFW_KEY_T, translateKeyHeld) && !io.WantCaptureKeyboard)
                editor::SetCurrentTransformMode(editor::EditorTransformMode::Translate);
            if (handleToggle(GLFW_KEY_R, rotateKeyHeld) && !io.WantCaptureKeyboard)
                editor::SetCurrentTransformMode(editor::EditorTransformMode::Rotate);
            if (handleToggle(GLFW_KEY_S, scaleKeyHeld) && !io.WantCaptureKeyboard)
                editor::SetCurrentTransformMode(editor::EditorTransformMode::Scale);

            if (mygame::HasSelectedObject())
            {
                if (handleToggle(GLFW_KEY_DELETE, deleteKeyHeld) && !io.WantCaptureKeyboard)
                {
                    Framework::GOCId selectedId = mygame::GetSelectedObjectId();
                    if (FACTORY)
                    {
                        if (auto* selected = FACTORY->GetObjectWithId(selectedId))
                        {
                            mygame::editor::RecordObjectDeleted(*selected);
                            FACTORY->Destroy(selected);
                        }
                    }
                    mygame::ClearSelection();
                }
            }
        }
        else
        {
            deleteKeyHeld = glfwGetKey(native, GLFW_KEY_DELETE) == GLFW_PRESS;
            translateKeyHeld = glfwGetKey(native, GLFW_KEY_T) == GLFW_PRESS;
            rotateKeyHeld = glfwGetKey(native, GLFW_KEY_R) == GLFW_PRESS;
            scaleKeyHeld = glfwGetKey(native, GLFW_KEY_S) == GLFW_PRESS;
        }
    }

    /*************************************************************************************
      \brief  Editor viewport picking/dragging and camera-follow lock while dragging Player.
      \details Converts cursor to world (ScreenToWorld), selects nearest hit, preserves drag
               offset, zeroes body velocity if present, and unlocks follow on mouse release.
    *************************************************************************************/
    void RenderSystem::HandleViewportPicking()
    {
        if (!window || !FACTORY)
        {
            leftMouseDownPrev = false;
            draggingSelection = false;
            return;
        }
        if (!showEditor)
        {
            // no picking/dragging when editor UI is hidden
            leftMouseDownPrev = false;
            draggingSelection = false;
            return;
        }
#if defined(_DEBUG) || defined(EDITOR)
        
        if (Framework::editor::IsGizmoActive())
        {
            leftMouseDownPrev = glfwGetMouseButton(window->raw(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            draggingSelection = false;
            return;
        }
#endif

        GLFWwindow* native = window->raw();
        if (!native)
        {
            leftMouseDownPrev = false;
            draggingSelection = false;
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        const bool wantCapture = io.WantCaptureMouse;

        const bool mouseDown = glfwGetMouseButton(native, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool pressed = mouseDown && !leftMouseDownPrev;
        const bool released = !mouseDown && leftMouseDownPrev;

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(native, &cursorX, &cursorY);
        UpdateEditorCameraControls(native, io, cursorX, cursorY);

        float worldX = 0.0f;
        float worldY = 0.0f;
        bool  insideViewport = false;
        if (!ScreenToWorld(cursorX, cursorY, worldX, worldY, insideViewport))
        {
            draggingSelection = false;
            leftMouseDownPrev = mouseDown;
            return;
        }

        if (mygame::HasSelectedObject())
        {
            Framework::GOCId selectedId = mygame::GetSelectedObjectId();
            if (!FACTORY->GetObjectWithId(selectedId))
            {
                mygame::ClearSelection();
                draggingSelection = false;
            }
        }
        else
        {
            draggingSelection = false;
        }

        if (pressed && !wantCapture)
        {
            Framework::GOCId pickedId = insideViewport ? TryPickObject(worldX, worldY) : 0;
            if (pickedId != 0)
            {
                mygame::SetSelectedObjectId(pickedId);
                if (auto* obj = FACTORY->GetObjectWithId(pickedId))
                {
                    if (auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent))
                    {
                        // Cache drag offset in world space so that we preserve relative grab point.
                        dragOffsetX = tr->x - worldX;
                        dragOffsetY = tr->y - worldY;
                        draggingSelection = true;

                        // If we started dragging the Player, lock camera follow at the start position.
                        if (IsPlayerObject(obj))
                            if (cameraEnabled && IsPlayerObject(obj))
                            {
                                gCameraFollowLocked = true;
                                gCameraLockPos = glm::vec2(tr->x, tr->y);
                            }

                        // Optional: Avoid physics-driven drift while dragging.
                        ZeroRigidBodyVelocityIfPresent(obj);
                    }
                }
            }
            else if (insideViewport)
            {
                mygame::ClearSelection();
                draggingSelection = false;
            }
        }

        if (draggingSelection && (!mouseDown || wantCapture))
            draggingSelection = false;

        if (draggingSelection)
        {
            Framework::GOCId selectedId = mygame::GetSelectedObjectId();
            if (selectedId != 0)
            {
                if (auto* obj = FACTORY->GetObjectWithId(selectedId))
                {
                    if (auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent))
                    {
                        tr->x = worldX + dragOffsetX;
                        tr->y = worldY + dragOffsetY;

                        // Keep physics quiet while dragging.
                        ZeroRigidBodyVelocityIfPresent(obj);
                    }
                    else
                    {
                        draggingSelection = false;
                    }
                }
                else
                {
                    mygame::ClearSelection();
                    draggingSelection = false;
                }
            }
            else
            {
                draggingSelection = false;
            }
        }

        if (released)
        {
            draggingSelection = false;
            // On release, always unlock camera follow (if it was locked due to dragging Player).
            gCameraFollowLocked = false;
        }

        leftMouseDownPrev = mouseDown;
    }

    /*************************************************************************************
      \brief  Convert a screen cursor position to world space.
      \param  cursorX,cursorY  Screen coordinates (GLFW).
      \param  worldX,worldY    Output world coordinates.
      \param  insideViewport   True if the cursor is over the game viewport.
      \return True if conversion succeeded and point lies in the viewport.
      \details Uses active camera’s inverse VP (Projection*View) to unproject.
    *************************************************************************************/
    bool RenderSystem::ScreenToWorld(double cursorX, double cursorY,
        float& worldX, float& worldY,
        bool& insideViewport) const
    {
        float ndcX = 0.0f;
        float ndcY = 0.0f;
        if (!CursorToViewportNdc(cursorX, cursorY, ndcX, ndcY, insideViewport))
            return false;

        if (!insideViewport)
            return false;

        const bool usingEditorCamera = ShouldUseEditorCamera();

        if (!usingEditorCamera && !cameraEnabled)
        {
            worldX = ndcX;
            worldY = ndcY;
            return true;
        }

        const gfx::Camera2D& activeCamera = usingEditorCamera ? editorCamera : camera;
        return UnprojectWithCamera(activeCamera, ndcX, ndcY, worldX, worldY);
    }

    /*************************************************************************************
      \brief  Map screen cursor to normalized device coords within the game viewport.
      \return True on success; sets ndcX/ndcY (\[-1,\+1\]) and insideViewport flag.
      \note   Accounts for ImGui work area and Y-up mapping.
    *************************************************************************************/
    bool RenderSystem::CursorToViewportNdc(double cursorX, double cursorY,
        float& ndcX, float& ndcY, bool& insideViewport) const
    {
        ndcX = 0.0f;
        ndcY = 0.0f;
        insideViewport = false;

        if (!window)
            return false;
        if (gameViewport.width <= 0 || gameViewport.height <= 0)
            return false;

        const double viewportLeft = static_cast<double>(gameViewport.x);
        const double viewportWidth = static_cast<double>(gameViewport.width);
        const double viewportBottom = static_cast<double>(gameViewport.y);
        const double viewportHeight = static_cast<double>(gameViewport.height);

        const int fullHeight = window->Height();
        if (fullHeight <= 0)
            return false;

        const double mouseYFromBottom = static_cast<double>(fullHeight) - cursorY;

        const double normalizedX = (cursorX - viewportLeft) / viewportWidth;
        const double normalizedY = (mouseYFromBottom - viewportBottom) / viewportHeight;

        insideViewport = (normalizedX >= 0.0 && normalizedX <= 1.0 &&
            normalizedY >= 0.0 && normalizedY <= 1.0);
        ndcX = static_cast<float>(normalizedX * 2.0 - 1.0);
        ndcY = static_cast<float>(normalizedY * 2.0 - 1.0);

        return true;
    }

    /*************************************************************************************
      \brief  Unproject an NDC point using the provided camera’s inverse VP.
      \return True if the resulting world coordinates are finite.
    *************************************************************************************/
    bool RenderSystem::UnprojectWithCamera(const gfx::Camera2D& cam,
        float ndcX, float ndcY,
        float& worldX, float& worldY) const
    {
        const glm::mat4 VP = cam.ProjectionMatrix() * cam.ViewMatrix();
        const glm::mat4 invVP = glm::inverse(VP);

        const glm::vec4 ndcPos = glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 world = invVP * ndcPos;
        if (world.w != 0.0f)
            world /= world.w;

        worldX = world.x;
        worldY = world.y;

        return std::isfinite(worldX) && std::isfinite(worldY);
    }

    /*************************************************************************************
      \brief  Decide if the editor camera should drive the view this frame.
      \details Editor must be shown, simulation must be stopped, and a valid viewport present.
    *************************************************************************************/
    bool RenderSystem::ShouldUseEditorCamera() const
    {
        if (!showEditor)
            return false;
        if (mygame::IsEditorSimulationRunning())
            return false;
        if (gameViewport.width <= 0 && gameViewport.height <= 0)
            return false;
        return true;
    }

    /*************************************************************************************
      \brief  Editor camera panning and zooming using middle-mouse and wheel.
      \param  native   GLFW window pointer.
      \param  io       ImGuiIO for wheel/mouse capture checks.
      \param  cursorX  Screen X.
      \param  cursorY  Screen Y.
      \details Pans when MMB is held inside the viewport; zooms about the cursor with wheel.
    *************************************************************************************/
    void RenderSystem::UpdateEditorCameraControls(GLFWwindow* native, const ImGuiIO& io,
        double cursorX, double cursorY)
    {
        if (!ShouldUseEditorCamera())
        {
            editorCameraPanning = false;
            return;
        }

        float ndcX = 0.0f;
        float ndcY = 0.0f;
        bool insideViewport = false;
        if (!CursorToViewportNdc(cursorX, cursorY, ndcX, ndcY, insideViewport))
        {
            editorCameraPanning = false;
            return;
        }

        float worldX = 0.0f;
        float worldY = 0.0f;
        if (insideViewport)
        {
            UnprojectWithCamera(editorCamera, ndcX, ndcY, worldX, worldY);
        }
        else
        {
            editorCameraPanning = false;
        }

        const bool wantCaptureMouse = io.WantCaptureMouse;
        const bool middleDown = glfwGetMouseButton(native, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

        if (middleDown && insideViewport && !wantCaptureMouse)
        {
            if (!editorCameraPanning)
            {
                editorCameraPanning = true;
                editorCameraPanStartWorld = glm::vec2(worldX, worldY);
                editorCameraPanStartFocus = editorCamera.Position();
            }
            else
            {
                const glm::vec2 current(worldX, worldY);
                const glm::vec2 delta = editorCameraPanStartWorld - current;
                editorCamera.SnapTo(editorCameraPanStartFocus + delta);
            }
        }
        else
        {
            editorCameraPanning = false;
        }

        const float wheel = io.MouseWheel;
        if (insideViewport && !wantCaptureMouse && std::fabs(wheel) > 0.0001f)
        {
            const float zoomFactor = std::pow(1.1f, -wheel);
            const float targetHeight = editorCameraViewHeight * zoomFactor;
            editorCamera.SetViewHeight(targetHeight);
            editorCameraViewHeight = editorCamera.ViewHeight();

            float newWorldX = worldX;
            float newWorldY = worldY;
            if (UnprojectWithCamera(editorCamera, ndcX, ndcY, newWorldX, newWorldY))
            {
                const glm::vec2 before(worldX, worldY);
                const glm::vec2 after(newWorldX, newWorldY);
                editorCamera.SnapTo(editorCamera.Position() + (before - after));
            }
        }
    }

    /*************************************************************************************
      \brief  Center the editor camera on the selected object and adjust zoom to fit it.
      \details Estimates an extent from circle/rect/sprite size and adds padding to view height.
    *************************************************************************************/
    void RenderSystem::FrameEditorSelection()
    {
        if (!ShouldUseEditorCamera())
            return;
        if (!FACTORY)
            return;
        if (!mygame::HasSelectedObject())
            return;

        Framework::GOCId selectedId = mygame::GetSelectedObjectId();
        Framework::GOC* obj = FACTORY->GetObjectWithId(selectedId);
        if (!obj)
            return;

        auto* tr = obj->GetComponentType<Framework::TransformComponent>(
            Framework::ComponentTypeId::CT_TransformComponent);
        if (!tr)
            return;

        editorCamera.SnapTo(glm::vec2(tr->x, tr->y));

        float extent = 0.5f;

        if (auto* circle = obj->GetComponentType<Framework::CircleRenderComponent>(
            Framework::ComponentTypeId::CT_CircleRenderComponent))
        {
            const float scaledRadius = circle->radius * std::max(std::fabs(tr->scaleX), std::fabs(tr->scaleY));
            extent = std::max(extent, scaledRadius);
        }

        if (auto* rect = obj->GetComponentType<Framework::RenderComponent>(
            Framework::ComponentTypeId::CT_RenderComponent))
        {
            extent = std::max(extent, std::max(std::fabs(rect->w * tr->scaleX), std::fabs(rect->h * tr->scaleY)) * 0.5f);
        }

        const float padding = 0.35f;
        const float desiredHeight = std::max(extent * 2.0f + padding, 0.4f);
        editorCamera.SetViewHeight(desiredHeight);
        editorCameraViewHeight = editorCamera.ViewHeight();
    }

    /*************************************************************************************
      \brief  Hit-test objects at (worldX,worldY) and return the nearest pickable one.
      \return GOCId of the best match or 0 if none.
      \details Tests circles and oriented rects; ignores layers filtered by game/editor rules.
    *************************************************************************************/
    Framework::GOCId RenderSystem::TryPickObject(float worldX, float worldY) const
    {
        if (!FACTORY)
            return 0;

        Framework::GOCId bestId = 0;
        float bestDistanceSq = std::numeric_limits<float>::max();

        for (const auto& [id, objPtr] : FACTORY->Objects())
        {
            (void)id;
            Framework::GOC* obj = objPtr.get();
            if (!obj)
                continue;
            if (!mygame::ShouldRenderLayer(obj->GetLayerName()))
                continue;

            auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                Framework::ComponentTypeId::CT_TransformComponent);
            if (!tr)
                continue;

            const float dx = worldX - tr->x;
            const float dy = worldY - tr->y;
            const float distanceSq = dx * dx + dy * dy;

            bool contains = false;

            if (auto* circle = obj->GetComponentType<Framework::CircleRenderComponent>(
                Framework::ComponentTypeId::CT_CircleRenderComponent))
            {
                const float radius = circle->radius * std::max(std::fabs(tr->scaleX), std::fabs(tr->scaleY));
                if (radius > 0.0f)
                    contains = (distanceSq <= radius * radius);
            }
            else
            {
                float width = std::max(1.0f, std::fabs(tr->scaleX));
                float height = std::max(1.0f, std::fabs(tr->scaleY));
                if (auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                    Framework::ComponentTypeId::CT_RenderComponent))
                {
                    width = rc->w * tr->scaleX;
                    height = rc->h * tr->scaleY;
                }
                else if (!obj->GetComponentType<Framework::SpriteComponent>(
                    Framework::ComponentTypeId::CT_SpriteComponent))
                {
                    // No render bounds information; skip.
                    continue;
                }

                if (width <= 0.0f) width = 1.0f;
                if (height <= 0.0f) height = 1.0f;

                // Transform the point into the object's local space to test oriented rectangles.
                const float cosR = std::cos(tr->rot);
                const float sinR = std::sin(tr->rot);
                const float localX = cosR * dx + sinR * dy;
                const float localY = -sinR * dx + cosR * dy;

                contains = (std::fabs(localX) <= width * 0.5f) &&
                    (std::fabs(localY) <= height * 0.5f);
            }

            if (!contains)
                continue;

            if (distanceSq < bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                bestId = obj->GetId();
            }
        }

        return bestId;
    }

    /*************************************************************************************
   \brief  Compute and apply the game viewport rectangle inside the window.
   \details Supports editor split width, optional full height, centering, and notifies
            cameras/text to update their projection/viewports. Calls glViewport accordingly.
 *************************************************************************************/
    void RenderSystem::UpdateGameViewport()
    {
        if (!window)
            return;

        const int fullWidth = window->Width();
        const int fullHeight = window->Height();
        if (fullWidth <= 0 || fullHeight <= 0)
            return;

        // When the editor is hidden (e.g., main menu or play mode), use the full window
        // regardless of fullscreen/windowed state. This keeps UI hit-tests aligned with
        // rendered buttons even when the viewport would otherwise be letterboxed.
        if (!showEditor)
        {
            gameViewport = { 0, 0, fullWidth, fullHeight };

            screenW = gameViewport.width;
            screenH = gameViewport.height;

            if (textReadyTitle) textTitle.setViewport(screenW, screenH);
            if (textReadyHint)  textHint.setViewport(screenW, screenH);

            if (gameViewport.width > 0 && gameViewport.height > 0)
            {
                camera.SetViewportSize(gameViewport.width, gameViewport.height);
                editorCamera.SetViewportSize(gameViewport.width, gameViewport.height);
            }

            camera.SetViewHeight(cameraViewHeight);
            editorCamera.SetViewHeight(editorCameraViewHeight);

            glViewport(gameViewport.x, gameViewport.y, gameViewport.width, gameViewport.height);
            return;
        }

        const float minSplit = 0.3f;
        const float maxSplit = 0.7f;
        editorSplitRatio = std::clamp(editorSplitRatio, minSplit, maxSplit);

        // --- Width ---
        int desiredWidth = fullWidth;
        if (showEditor && !gameViewportFullWidth)
        {
            desiredWidth = static_cast<int>(std::lround(fullWidth * editorSplitRatio));
            const int maxWidth = std::max(1, fullWidth - 1);
            desiredWidth = std::clamp(desiredWidth, 1, maxWidth);
        }

        // --- Height ---
        if (!gameViewportFullHeight)
        {
            heightRatio = std::clamp(heightRatio, 0.30f, 1.0f);
        }
        else
        {
            heightRatio = 1.0f;
        }

        int desiredHeight = static_cast<int>(std::lround(fullHeight * heightRatio));
        desiredHeight = std::clamp(desiredHeight, 1, fullHeight);

        // Center vertically when not using full height.
        int yOffset = (fullHeight - desiredHeight) / 2;
        if (gameViewportFullHeight)
            yOffset = 0;

        // Center horizontally when not using full width.
        int xOffset = (fullWidth - desiredWidth) / 2;
        if (gameViewportFullWidth)
            xOffset = 0;

        // Apply if changed.
        if (gameViewport.width != desiredWidth ||
            gameViewport.height != desiredHeight ||
            gameViewport.y != yOffset ||
            gameViewport.x != xOffset)
        {
            gameViewport.x = xOffset;
            gameViewport.y = yOffset;
            gameViewport.width = desiredWidth;
            gameViewport.height = desiredHeight;

            screenW = gameViewport.width;
            screenH = gameViewport.height;

            if (textReadyTitle) textTitle.setViewport(screenW, screenH);
            if (textReadyHint)  textHint.setViewport(screenW, screenH);
        }

        // Keep camera informed about viewport changes so its projection is correct.
        if (gameViewport.width > 0 && gameViewport.height > 0)
        {
            camera.SetViewportSize(gameViewport.width, gameViewport.height);
            editorCamera.SetViewportSize(gameViewport.width, gameViewport.height);
        }
        camera.SetViewHeight(cameraViewHeight);
        editorCamera.SetViewHeight(editorCameraViewHeight);

        if (gameViewport.width > 0 && gameViewport.height > 0)
            glViewport(gameViewport.x, gameViewport.y, gameViewport.width, gameViewport.height);
    }

    /*************************************************************************************
      \brief  Restore GL viewport to the full window (used before UI/menu draws).
    *************************************************************************************/
    void RenderSystem::RestoreFullViewport()
    {
        if (!window)
            return;

        glViewport(0, 0, window->Width(), window->Height());
    }

    /*************************************************************************************
      \brief  Draw the editor dockspace host window on the right side of the screen.
      \details Creates a passthrough dock node sized to the editor region; no background/chrome.
    *************************************************************************************/
    void RenderSystem::DrawDockspace()
    {
        if (!showEditor)
            return;
        ImGuiIO& io = ImGui::GetIO();
        if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable))
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        const float editorWidth = viewport->WorkSize.x - static_cast<float>(gameViewport.width);
        if (editorWidth <= 1.0f || viewport->WorkSize.y <= 1.0f)
            return;

        const ImVec2 editorPos(viewport->WorkPos.x + static_cast<float>(gameViewport.width),
            viewport->WorkPos.y);
        const ImVec2 editorSize(editorWidth, viewport->WorkSize.y);

        ImGui::SetNextWindowPos(editorPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(editorSize, ImGuiCond_Always);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;

        ImGui::Begin("EditorDockHost", nullptr, flags);
        ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode |
            ImGuiDockNodeFlags_NoDockingInCentralNode;
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);
        ImGui::End();

        ImGui::PopStyleVar(2);
    }

    /*************************************************************************************
      \brief  Small always-on-top helper for toggles and camera settings.
      \details Lets you toggle editor/full-width/full-height, play/stop sim, and camera enable/zoom.
               Also exposes Undo controls.
    *************************************************************************************/
    void RenderSystem::DrawViewportControls()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 pos = viewport->WorkPos;
        pos.x += 12.0f;
        pos.y += 12.0f;

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("Viewport Controls", nullptr, flags))
        {
            ImGui::TextUnformatted("Viewport Controls");
            ImGui::Separator();

            bool editorEnabled = showEditor;
            if (ImGui::Checkbox("Editor Enabled (F10)", &editorEnabled))
                showEditor = editorEnabled;

            if (!showEditor)
            {
                ImGui::TextDisabled("Editor panels hidden. Press F10 or re-enable above.");
                // No more controls when editor is off
                ImGui::End();
                return;
            }

            const ImGuiIO& io = ImGui::GetIO();
            bool didUndo = false;

            // Keyboard shortcut: Ctrl+Z / Cmd+Z
            if (!io.WantCaptureKeyboard && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Z))
            {
                if (mygame::editor::UndoLastAction())
                    didUndo = true;
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Undo");
            bool canUndo = mygame::editor::CanUndo();
            if (!canUndo)
                ImGui::BeginDisabled();
            if (ImGui::Button("Undo Last"))
            {
                if (mygame::editor::UndoLastAction())
                    didUndo = true;
            }
            if (!canUndo)
                ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text("%zu / %zu steps", mygame::editor::StackDepth(), mygame::editor::StackCapacity());

            // If we actually undid something, rebind textures so sprites/rects stay correct.
            if (didUndo)
            {
                RebindAllComponentTextures();
            }

            // ---- everything below this only shows when editor is ON ----

            bool fullWidth = gameViewportFullWidth;
            if (ImGui::Checkbox("Game Full Width", &fullWidth))
                gameViewportFullWidth = fullWidth;
            if (!gameViewportFullWidth)
            {
                float splitPercent = editorSplitRatio * 100.0f;
                if (ImGui::SliderFloat("Game Width", &splitPercent, 30.0f, 70.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp))
                    editorSplitRatio = splitPercent / 100.0f;
            }

            bool fullHeight = gameViewportFullHeight;
            if (ImGui::Checkbox("Game Full Height", &fullHeight))
                gameViewportFullHeight = fullHeight;

            if (!gameViewportFullHeight) {
                float hPercent = heightRatio * 100.0f;
                if (ImGui::SliderFloat("Game Height", &hPercent, 30.0f, 100.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp))
                    heightRatio = hPercent / 100.0f;
                ImGui::TextDisabled("Viewport is centered vertically");
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Simulation");
            bool isPlaying = mygame::IsEditorSimulationRunning();

            const bool wasPlaying = isPlaying;
            if (wasPlaying)
                ImGui::BeginDisabled();
            if (ImGui::Button("Play"))
            {
                mygame::EditorPlaySimulation();
                isPlaying = mygame::IsEditorSimulationRunning();
            }
            if (wasPlaying)
                ImGui::EndDisabled();

            ImGui::SameLine();

            const bool wasStopped = !isPlaying;
            if (wasStopped)
                ImGui::BeginDisabled();
            if (ImGui::Button("Stop"))
            {
                mygame::EditorStopSimulation();
                isPlaying = mygame::IsEditorSimulationRunning();
            }
            if (wasStopped)
                ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::Text("State: %s", isPlaying ? "Playing" : "Stopped");

            // ---- Camera section (editor-only) ----
            ImGui::Separator();
            ImGui::TextUnformatted("Camera");

            if (!cameraEnabled)
                ImGui::BeginDisabled();

            if (ImGui::SliderFloat("View Height (world units)", &cameraViewHeight, 0.4f, 2.5f, "%.2f"))
            {
                camera.SetViewHeight(cameraViewHeight);
            }
            if (!cameraEnabled)
                ImGui::EndDisabled();

            ImGui::TextDisabled("Smaller values zoom the camera closer to the player.");

            if (ImGui::Checkbox("Camera Enabled", &cameraEnabled))
            {
                if (!cameraEnabled)
                {
                    gCameraFollowLocked = false;
                    gfx::Graphics::resetViewProjection();
                }
                else
                {
                    camera.SetViewHeight(cameraViewHeight);
                }
            }

            if (!cameraEnabled)
            {
                ImGui::TextDisabled("Camera disabled: legacy static framing.");
            }
        }
        ImGui::End();
    }

    /*************************************************************************************
      \brief  GLFW drop-files callback trampoline into RenderSystem instance.
    *************************************************************************************/
    void RenderSystem::GlfwDropCallback(GLFWwindow*, int count, const char** paths)
    {
        if (sInstance)
            sInstance->HandleFileDrop(count, paths);
    }

    /*************************************************************************************
      \brief  Convenience accessor for current animation sheet columns.
    *************************************************************************************/
    int RenderSystem::CurrentColumns() const
    {
        return logic.Animation().columns;
    }

    /*************************************************************************************
      \brief  Convenience accessor for current animation sheet rows.
    *************************************************************************************/
    int RenderSystem::CurrentRows() const
    {
        return logic.Animation().rows;
    }

    /*************************************************************************************
      \brief  One-time GL/ImGui/asset setup and window/config discovery.
      \details Loads fonts, background/sprite textures, initializes Graphics.cpp, ImGui layer,
               discovers assets and Data_Files roots, and wires file-drop callback.
    *************************************************************************************/
    void RenderSystem::Initialize()
    {
        dataFilesRoot = FindDataFilesRoot();

        auto resolveData = [](const std::filesystem::path& rel) {
            return Framework::ResolveDataPath(rel).string();
            };
        auto resolveAsset = [](const std::filesystem::path& rel) {
            return Framework::ResolveAssetPath(rel).string();
            };

        WindowConfig cfg = LoadWindowConfig(resolveData("window.json"));
        screenW = cfg.width;
        screenH = cfg.height;
        if (window)
        {
            screenW = window->Width();
            screenH = window->Height();
        }
        gameViewport = { 0, 0, screenW, screenH };

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        gfx::Graphics::initialize();

        std::cout << "[CWD] " << std::filesystem::current_path() << "\n";
        std::cout << "[EXE] " << Framework::GetExecutableDir() << "\n";

        imguiLayoutPath = resolveData("imgui_layout.ini");

        if (auto fontPath = FindRoboto(); !fontPath.empty())
        {
            std::cout << "[Text] Using font: " << fontPath << "\n";
            textTitle.initialize(fontPath.c_str(), screenW, screenH);
            textHint.initialize(fontPath.c_str(), screenW, screenH);
            textReadyTitle = true;
            textReadyHint = true;
        }
        else
        {
            std::cout << "[Text] Roboto not found. Text will be skipped.\n";
            textReadyTitle = textReadyHint = false;
        }

        Resource_Manager::load("player_png", resolveAsset("Textures/player.png"));
        playerTex = Resource_Manager::resources_map["player_png"].handle;

        Resource_Manager::load("ming_idle", resolveAsset("Textures/Idle Sprite .png"));
        Resource_Manager::load("ming_run", resolveAsset("Textures/Running Sprite .png"));
        Resource_Manager::load("ming_attack1", resolveAsset("Textures/Character/Ming_Sprite/1st_Attack Sprite.png"));
        Resource_Manager::load("ming_attack2", resolveAsset("Textures/Character/Ming_Sprite/2nd_Attack Sprite.png"));
        Resource_Manager::load("ming_attack3", resolveAsset("Textures/Character/Ming_Sprite/3rd_Attack Sprite.png"));
        Resource_Manager::load("ming_knife", resolveAsset("Textures/Character/Ming_Sprite/Knife_Sprite.png"));

        idleTex = Resource_Manager::resources_map["ming_idle"].handle;
        runTex = Resource_Manager::resources_map["ming_run"].handle;
        attackTex[0] = Resource_Manager::resources_map["ming_attack1"].handle;
        attackTex[1] = Resource_Manager::resources_map["ming_attack2"].handle;
        attackTex[2] = Resource_Manager::resources_map["ming_attack3"].handle;
        knifeTex = Resource_Manager::resources_map["ming_knife"].handle;

        ImGuiLayerConfig config;
        config.glsl_version = "#version 330";
        config.dockspace = true;
        config.gamepad = false;
        if (window)
        {
            ImGuiLayer::Initialize(*window, config);
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = imguiLayoutPath.c_str();
        }
        else
        {
            std::cerr << "[RenderSystem] Warning: window is null, skipping ImGui initialization.\n";
        }

        assetsRoot = FindAssetsRoot();
        if (assetsRoot.empty())
        {
            std::error_code ec;
            auto cwdAssets = std::filesystem::current_path(ec) / "assets";
            if (!ec && std::filesystem::exists(cwdAssets, ec) && std::filesystem::is_directory(cwdAssets, ec))
                assetsRoot = std::filesystem::weakly_canonical(cwdAssets, ec);
        }

        if (!assetsRoot.empty())
        {
            assetBrowser.Initialize(assetsRoot);
            mygame::SetSpawnPanelAssetsRoot(assetsRoot);
            AudioImGui::SetAssetsRoot(assetsRoot);
        }


        jsonEditor.Initialize(dataFilesRoot);

        if (window && window->raw())
            glfwSetDropCallback(window->raw(), &RenderSystem::GlfwDropCallback);
    }
    /*************************************************************************************
   \brief  Handle fullscreen/editor shortcut keys when only menu UI is active.
   \note   Provides F11 support for main/pause menus that bypass RenderSystem::draw().
    *************************************************************************************/
    void RenderSystem::HandleMenuShortcuts()
    {
        HandleShortcuts();
        UpdateGameViewport();
    }
    /*************************************************************************************
      \brief  Prepare GL state for drawing the main menu pages (screen-space).
      \note   Uses full-window viewport and identity VP so UI is not camera-affected.
    *************************************************************************************/
    void Framework::RenderSystem::BeginMenuFrame()
    {
        // UI/menu renders in screen space: reset VP to identity and use full window viewport.
        RestoreFullViewport();
        gfx::Graphics::resetViewProjection();

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Intentionally DO NOT call Graphics::renderBackground() here.
        // The MainMenuPage will draw its own menu.jpg.
        glUseProgram(0);
    }

    /*************************************************************************************
      \brief  Symmetric end to BeginMenuFrame() — restores full viewport for later passes.
    *************************************************************************************/
    void Framework::RenderSystem::EndMenuFrame()
    {
        // Keep symmetry for future state restoration if needed.
        RestoreFullViewport();
    }

    /*************************************************************************************
      \brief  Static visibility query for the editor UI (used by external panels).
      \return True if the editor panels are currently shown.
    *************************************************************************************/
    bool RenderSystem::IsEditorVisible()
    {
        return sInstance ? sInstance->showEditor : false;
    }

    /*************************************************************************************
      \brief  Main per-frame render/update entry: sets VP, draws world/UI, and editor tools.
      \details Order:
               1) Shortcuts + viewport update
               2) VP reset, choose camera (editor/game follow), and set VP
               3) Picking/drag
               4) Background, batched sprites, text in screen-space
               5) Dockspace, controls, asset/json panels, debug overlays
               6) Imported assets processing and perf timing
      \note    Uses TryGuard::Run(...) to isolate and label crashes as "RenderSystem::draw".
    *************************************************************************************/
    void RenderSystem::draw()
    {
        TryGuard::Run([&] {
            HandleShortcuts();
            UpdateGameViewport();

            // === Update camera BEFORE picking and rendering ===
            gfx::Graphics::resetViewProjection();

            const bool usingEditorCamera = ShouldUseEditorCamera();
            glm::mat4 activeView(1.0f);
            glm::mat4 activeProj(1.0f);

            if (usingEditorCamera)
            {
                activeView = editorCamera.ViewMatrix();
                activeProj = editorCamera.ProjectionMatrix();
                gfx::Graphics::setViewProjection(activeView, activeProj);
            }
            else if (cameraEnabled)
            {
                float playerX = 0.0f, playerY = 0.0f;
                const bool hasPlayer = logic.GetPlayerWorldPosition(playerX, playerY);

                if (gCameraFollowLocked)
                {
                    // While locked (dragging Player), keep camera fixed.
                    camera.SnapTo(gCameraLockPos);
                }
                else if (hasPlayer)
                {
                    // Normal follow.
                    camera.SnapTo(glm::vec2(playerX, playerY));
                }

                // Submit this frame's View and Projection so picking uses the latest VP.
                activeView = camera.ViewMatrix();
                activeProj = camera.ProjectionMatrix();
                gfx::Graphics::setViewProjection(activeView, activeProj);
            }

            // Now handle picking with the correct (current) camera matrices.
            HandleViewportPicking();

            // Auto-load all textures referenced by objects
            for (auto& [id, objPtr] : FACTORY->Objects())
            {
                GOC* obj = objPtr.get();
                if (!obj) continue;
                
                // If object has SpriteComponent
                if (auto* sp = obj->GetComponentType<SpriteComponent>(ComponentTypeId::CT_SpriteComponent)) 
                {
                    if (!sp->texture_key.empty())
                    {
                        unsigned tex = Resource_Manager::getTexture(sp->texture_key);
                        if (!tex)
                        {
                            // Try load it if not already in memory
                            Resource_Manager::load(sp->texture_key, sp->texture_key);
                            tex = Resource_Manager::getTexture(sp->texture_key);
                        }
                        sp->texture_id = tex;
                    }
                }

                // If object has RenderComponent
                if (auto* rc = obj->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent)) 
                {
                    if (!rc->texture_key.empty())
                    {
                        unsigned tex = Resource_Manager::getTexture(rc->texture_key); 
                        if (!tex)
                        {
                            Resource_Manager::load(rc->texture_key, rc->texture_key); 
                            tex = Resource_Manager::getTexture(rc->texture_key);
                        }
                        rc->texture_id = tex;
                    }
                }
            }


            // Layering
            std::vector<unsigned> sortedIds;
            sortedIds.reserve(FACTORY->Objects().size());

            for (auto& [id, objPtr] : FACTORY->Objects())
                sortedIds.push_back(id);

            // Sort by RenderComponent::layer
            std::sort(sortedIds.begin(), sortedIds.end(), 
                [](unsigned a, unsigned b) 
                {
                    auto* objA = FACTORY->GetObjectWithId(a); 
                    auto* objB = FACTORY->GetObjectWithId(b); 

                    auto* rcA = objA ? objA->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent) : nullptr; 
                    auto* rcB = objB ? objB->GetComponentType<RenderComponent>(ComponentTypeId::CT_RenderComponent) : nullptr; 

                    int la = rcA ? rcA->layer : 0; 
                    int lb = rcB ? rcB->layer : 0; 

                    return la < lb; 
                });


            auto t0 = clock::now();

            static unsigned hawkerFloorTex = 0;
            static unsigned hawkerHdbTex = 0;

            auto ensureBackgroundTexture = [](unsigned& textureHandle,
                const char* key,
                const char* path)
                {
                    if (textureHandle)
                        return;

                    textureHandle = Resource_Manager::getTexture(key);
                    if (!textureHandle && path)
                    {
                        if (Resource_Manager::load(key, path))
                        {
                            textureHandle = Resource_Manager::getTexture(key);
                        }
                    }
                };

            const std::string floorPath =
                Framework::ResolveAssetPath("Textures/Environment/lvl 1_Hawker/Floor.png").string();
            const std::string hdbPath =
                Framework::ResolveAssetPath("Textures/Environment/lvl 1_Hawker/HDB.png").string();

            ensureBackgroundTexture(hawkerFloorTex, "hawker_floor_bg", floorPath.c_str());
            ensureBackgroundTexture(hawkerHdbTex, "hawker_hdb_bg", hdbPath.c_str());

            if (hawkerFloorTex && hawkerHdbTex)
            {
                gfx::Graphics::renderSprite(hawkerHdbTex, 0.0f, 0.5f, 0.0f,
                    2.0f, 1.0f,
                    1.f, 1.f, 1.f, 1.f);
                gfx::Graphics::renderSprite(hawkerFloorTex, 0.0f, -0.5f, 0.0f,
                    2.0f, 1.0f,
                    1.f, 1.f, 1.f, 1.f);
            }
            else if (unsigned bgTex = Resource_Manager::getTexture("house"))
            {
                // Big background quad in world space (uses camera VP).
                gfx::Graphics::renderSprite(bgTex, 0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 1.f, 1.f, 1.f, 1.f);
            }
            else
            {
                gfx::Graphics::renderBackground();
            }

            if (FACTORY)
            {
                std::unordered_map<unsigned, std::vector<gfx::Graphics::SpriteInstance>> spriteBatches;
                spriteBatches.reserve(64);

                //const auto& animState = logic.Animation();
                //const int animCols = std::max(1, CurrentColumns());
                //const int animRows = std::max(1, CurrentRows());

                // Pass 1: Sprites (instanced)
                for (unsigned id : sortedIds)
                {
                    auto& objPtr = FACTORY->Objects().at(id); 
                    GOC* obj = objPtr.get(); 
                    if (!obj) continue;
                    if (!mygame::ShouldRenderLayer(obj->GetLayerName())) continue;

                    auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent);
                    if (!tr) continue;

                    auto* animComp = obj->GetComponentType<Framework::SpriteAnimationComponent>(
                        Framework::ComponentTypeId::CT_SpriteAnimationComponent);

                    if (auto* sp = obj->GetComponentType<Framework::SpriteComponent>(
                        Framework::ComponentTypeId::CT_SpriteComponent))
                    {
                        float sx = 1.f, sy = 1.f;
                        float r = 1.f, g = 1.f, b = 1.f, a = 1.f;

                        // If a RenderComponent is present, use its size/tint AND visibility
                        if (auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                            Framework::ComponentTypeId::CT_RenderComponent))
                        {
                            // Skip invisible sprites
                            if (!rc->visible || rc->a <= 0.0f)
                                continue;

                            sx = rc->w;
                            sy = rc->h;
                            r = rc->r;
                            g = rc->g;
                            b = rc->b;
                            a = rc->a;
                        }

                        unsigned tex = sp->texture_id;
                        glm::vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);

                        if (animComp && animComp->HasSpriteSheets())
                        {
                            auto sample = animComp->CurrentSheetSample();
                            if (sample.texture)
                                tex = sample.texture;
                            uvRect = sample.uv;
                        }
                        
                        else if (!tex && !sp->texture_key.empty())
                        {
                            tex = Resource_Manager::getTexture(sp->texture_key);
                            sp->texture_id = tex;
                        }

                        if (!tex)
                            continue;

                        gfx::Graphics::SpriteInstance instance;
                        glm::mat4 model(1.0f);
                        model = glm::translate(model, glm::vec3(tr->x, tr->y, 0.0f));
                        model = glm::rotate(model, tr->rot, glm::vec3(0, 0, 1));
                        model = glm::scale(model, glm::vec3(sx * tr->scaleX, sy * tr->scaleY, 1.0f));
                        instance.model = model;
                        instance.tint = glm::vec4(r, g, b, a);
                        instance.uv = uvRect;

                        spriteBatches[tex].push_back(instance);
                    }
                }

                // NEW: Render knife projectiles using Knife_Sprite.png with its own anim.
                if (knifeTex && logic.hitBoxSystem)
                {
                    const auto& activeHits = logic.hitBoxSystem->GetActiveHitBoxes();
                    if (!activeHits.empty())
                    {
                        auto& knives = spriteBatches[knifeTex];
                        constexpr int knifeCols = 4;
                        constexpr int knifeRows = 1;
                        constexpr int knifeFrames = 4;
                        constexpr float knifeFps = 12.f;
                        const float invCols = 1.0f / static_cast<float>(knifeCols);
                        const float invRows = 1.0f / static_cast<float>(knifeRows);

                        for (const auto& activeHit : activeHits)
                        {
                            if (!activeHit.hitbox || !activeHit.isProjectile)
                                continue;

                            const auto* hb = activeHit.hitbox.get();

                            gfx::Graphics::SpriteInstance instance;
                            glm::mat4 model(1.0f);
                            model = glm::translate(model, glm::vec3(hb->spawnX, hb->spawnY, 0.0f));
                            const float angle = std::atan2(activeHit.velY, activeHit.velX);
                            model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
                            model = glm::scale(model, glm::vec3(hb->width, hb->height, 1.0f));
                            instance.model = model;
                            instance.tint = glm::vec4(1.0f);

                            const float duration = std::max(0.0001f, hb->duration);
                            const float elapsed = std::clamp(duration - activeHit.timer, 0.0f, duration);
                            const int frameIdx = static_cast<int>(elapsed * knifeFps) % knifeFrames;
                            const float u = static_cast<float>(frameIdx) * invCols;
                            instance.uv = glm::vec4(u, 0.0f, invCols, invRows);

                            knives.push_back(instance);
                        }
                    }
                }

                for (auto& [tex, batch] : spriteBatches)
                {
                    if (!batch.empty())
                        gfx::Graphics::renderSpriteBatchInstanced(tex, batch);
                }

                // Pass 2: Rectangles (non-sprite quads)
                for (unsigned id : sortedIds) 
                {
                    auto& objPtr = FACTORY->Objects().at(id); 
                    GOC* obj = objPtr.get();  
                    if (!obj) continue;
                    if (!mygame::ShouldRenderLayer(obj->GetLayerName())) continue;

                    auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent);
                    auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                        Framework::ComponentTypeId::CT_RenderComponent);
                    if (!tr || !rc) continue;

                    // NEW: skip invisible rect-only renderables
                    if (!rc->visible || rc->a <= 0.0f)
                        continue;

                    if (obj->GetComponentType<Framework::SpriteComponent>(
                        Framework::ComponentTypeId::CT_SpriteComponent))
                        continue;

                    unsigned rectTex = rc->texture_id;
                    if (!rectTex && !rc->texture_key.empty())
                    {
                        rectTex = Resource_Manager::getTexture(rc->texture_key);
                        rc->texture_id = rectTex;
                    }
                    const float scaledW = rc->w * tr->scaleX;
                    const float scaledH = rc->h * tr->scaleY;
                    if (rectTex)
                    {
                        gfx::Graphics::renderSprite(rectTex, tr->x, tr->y, tr->rot,
                            scaledW, scaledH,
                            rc->r, rc->g, rc->b, rc->a);
                    }
                    else
                    {
                        gfx::Graphics::renderRectangle(tr->x, tr->y, tr->rot,
                            scaledW, scaledH,
                            rc->r, rc->g, rc->b, rc->a);
                    }
                }

                // Pass 3: Circles
                for (unsigned id : sortedIds) 
                {
                    auto& objPtr = FACTORY->Objects().at(id); 
                    GOC* obj = objPtr.get(); 
                    if (!obj) continue;
                    if (!mygame::ShouldRenderLayer(obj->GetLayerName())) continue;

                    auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent);
                    auto* cc = obj->GetComponentType<Framework::CircleRenderComponent>(
                        Framework::ComponentTypeId::CT_CircleRenderComponent);
                    if (!tr || !cc) continue;

                    const float scaledRadius = cc->radius * std::max(std::fabs(tr->scaleX), std::fabs(tr->scaleY));
                    gfx::Graphics::renderCircle(tr->x, tr->y, scaledRadius, cc->r, cc->g, cc->b, cc->a);
                }

                // Pass 4: Hover/Selection highlight outlines (editor)
                // Drawn in world space, using same VP as the object passes above.
                if (showEditor) {
                    const auto hoveredId = mygame::GetHoverObjectId();
                    const auto selectedId = mygame::GetSelectedObjectId();
                    if ((hoveredId != 0) || (selectedId != 0))
                    {
                        auto drawOutline = [](float x, float y, float rot, float w, float h, bool selected)
                            {
                                // Selected: thicker cyan; Hover: thinner yellow
                                if (selected)
                                    gfx::Graphics::renderRectangleOutline(x, y, rot, w, h, 0.f, 1.f, 1.f, 1.f, 6.f);
                                else
                                    gfx::Graphics::renderRectangleOutline(x, y, rot, w, h, 1.f, 1.f, 0.f, 1.f, 2.f);
                            };

                        for (unsigned id : sortedIds) 
                        {
                            auto& objPtr = FACTORY->Objects().at(id); 
                            GOC* obj = objPtr.get(); 
                            if (!obj) continue;
                            if (!mygame::ShouldRenderLayer(obj->GetLayerName())) continue;

                            const bool isHovered = (id == hoveredId);
                            const bool isSelected = (id == selectedId);
                            if (!isHovered && !isSelected) continue;

                            auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                                Framework::ComponentTypeId::CT_TransformComponent);
                            if (!tr) continue;

                            if (auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                                Framework::ComponentTypeId::CT_RenderComponent))
                            {
                                float w = std::abs(rc->w * tr->scaleX);
                                float h = std::abs(rc->h * tr->scaleY);
                                if (w <= 0.f) w = 1.f;
                                if (h <= 0.f) h = 1.f;
                                drawOutline(tr->x, tr->y, tr->rot, w, h, isSelected);
                            }
                            else if (obj->GetComponentType<Framework::SpriteComponent>(
                                Framework::ComponentTypeId::CT_SpriteComponent))
                            {
                                // Sprites use RenderComponent for size in this engine; if missing, give a safe default box
                                const float w = std::max(0.1f, std::fabs(tr->scaleX));
                                const float h = std::max(0.1f, std::fabs(tr->scaleY));
                                drawOutline(tr->x, tr->y, tr->rot, w, h, isSelected);
                            }
                            else if (auto* cc = obj->GetComponentType<Framework::CircleRenderComponent>(
                                Framework::ComponentTypeId::CT_CircleRenderComponent))
                            {
                                const float scaledRadius = cc->radius * std::max(std::fabs(tr->scaleX), std::fabs(tr->scaleY));
                                const float d = std::max(0.1f, scaledRadius * 2.f);
                                drawOutline(tr->x, tr->y, 0.f, d, d, isSelected);
                            }
                        }
                    }

                    if (showPhysicsHitboxes && logic.hitBoxSystem)
                    {
                        for (unsigned id : sortedIds) 
                        {
                            auto& objPtr = FACTORY->Objects().at(id); 
                            GOC* obj = objPtr.get(); 
                            if (!obj) continue;

                            auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                                Framework::ComponentTypeId::CT_TransformComponent);
                            auto* rb = obj->GetComponentType<Framework::RigidBodyComponent>(
                                Framework::ComponentTypeId::CT_RigidBodyComponent);
                            if (!tr || !rb) continue;

                            gfx::Graphics::renderRectangleOutline(tr->x, tr->y, 0.0f,
                                rb->width, rb->height,
                                1.f, 0.f, 0.f, 1.f,
                                2.f);

                            // Check HitBoxSystem for active hitboxes
                            for (const auto& activeHit : logic.hitBoxSystem->GetActiveHitBoxes())
                            {
                                if (activeHit.hitbox && activeHit.hitbox->active)
                                {
                                    gfx::Graphics::renderRectangleOutline(
                                        activeHit.hitbox->spawnX,
                                        activeHit.hitbox->spawnY,
                                        0.0f,
                                        activeHit.hitbox->width,
                                        activeHit.hitbox->height,
                                        0.0f, 1.0f, 0.0f, 1.0f, // green outline for enemy attacks
                                        2.0f
                                    );
                                    std::cout << "Hit Box produced at coordinate:" << activeHit.hitbox->spawnX
                                        << "," << activeHit.hitbox->spawnY << std::endl;
                                }
                            }
                        }
                    }
                }
            }
            if (showEditor)
            {
                if (const ImGuiViewport* mainViewport = ImGui::GetMainViewport())
                {
                    editor::ViewportRect gizmoRect{};
                    gizmoRect.x = mainViewport->WorkPos.x + static_cast<float>(gameViewport.x);
                    gizmoRect.y = mainViewport->WorkPos.y + (mainViewport->WorkSize.y - static_cast<float>(gameViewport.y + gameViewport.height));
                    gizmoRect.width = static_cast<float>(gameViewport.width);
                    gizmoRect.height = static_cast<float>(gameViewport.height);

                    editor::RenderTransformGizmoForSelection(activeView, activeProj, gizmoRect);
                }
            }

            // Switch back to screen-space VP (identity) for UI text so it ignores camera.
            gfx::Graphics::resetViewProjection();

            if (textReadyTitle)
            {
                /*textTitle.RenderText(
                    "Bloody Good Curry",
                    32.0f,
                    static_cast<float>(screenH) - 64.0f,
                    1.05f,
                    glm::vec3(1.0f, 1.0f, 1.0f)
                );*/
            }
            if (textReadyHint)
            {
                /*textHint.RenderText(
                    "Press WASD to run",
                    32.0f,
                    40.0f,
                    0.75f,
                    glm::vec3(0.95f, 0.85f, 0.10f)
                );*/
            }

            const double renderMs = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
            Framework::setRender(renderMs);

            RestoreFullViewport(); // Restore full window viewport for ImGui.

            if (showEditor)
            {
                if (ImGui::BeginMainMenuBar())
                {
                    if (ImGui::BeginMenu("View"))
                    {
                        ImGui::MenuItem("Animation Editor", nullptr, &showAnimationEditor);
                        ImGui::EndMenu();
                    }
                    ImGui::EndMainMenuBar();
                }
            }

            t0 = clock::now();

            DrawDockspace();
            DrawViewportControls();
            if (showEditor)
            {
                assetBrowser.Draw();
                jsonEditor.Draw();
                mygame::DrawHierarchyPanel();
                mygame::DrawSpawnPanel();
                mygame::DrawPropertiesEditor();
                mygame::DrawInspectorWindow();
                mygame::DrawAnimationEditor(showAnimationEditor);

                if (ImGui::Begin("Crash Tests"))
                {
                    if (ImGui::Button("Crash BG shader"))     gfx::Graphics::testCrash(1);
                    if (ImGui::Button("Crash BG VAO"))        gfx::Graphics::testCrash(2);
                    if (ImGui::Button("Crash Sprite shader")) gfx::Graphics::testCrash(3);
                    if (ImGui::Button("Crash Object shader")) gfx::Graphics::testCrash(4);
                    if (ImGui::Button("Delete BG texture"))   gfx::Graphics::testCrash(5);
                }
                ImGui::End();

                if (ImGui::Begin("Debug Overlays"))
                {
                    const char* buttonLabel = showPhysicsHitboxes ? "Hide Hitboxes" : "Show Hitboxes";
                    if (ImGui::Button(buttonLabel))
                    {
                        showPhysicsHitboxes = !showPhysicsHitboxes;
                    }

                    ImGui::SameLine();
                    ImGui::Text("Hitboxes: %s", showPhysicsHitboxes ? "ON" : "OFF");
                }
                ImGui::End();

                Framework::DrawPerformanceWindow();
            }

            ProcessImportedAssets();

            const double imguiMs = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
            Framework::setImGui(imguiMs);
            }, "RenderSystem::draw");
    }

    /*************************************************************************************
      \brief  Persist ImGui layout, detach callbacks, and release any per-frame resources.
    *************************************************************************************/
    void RenderSystem::Shutdown()
    {
        ImGui::SaveIniSettingsToDisk(imguiLayoutPath.c_str());
        if (window && window->raw())
            glfwSetDropCallback(window->raw(), nullptr);

        gfx::Graphics::cleanup();
        Resource_Manager::unloadAll(Resource_Manager::Graphics);

        textTitle.cleanup();
        textHint.cleanup();
        textReadyTitle = textReadyHint = false;

        ImGuiLayer::Shutdown();
        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();

        sInstance = nullptr;
        window = nullptr;
    }

} // namespace Framework
