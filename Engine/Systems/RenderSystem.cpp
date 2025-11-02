
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
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
#include <imgui.h>

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <system_error>
#include <vector>
#include <limits>

#include "Physics/Dynamics/RigidBodyComponent.h"
namespace Framework {
    RenderSystem* RenderSystem::sInstance = nullptr;

    namespace {
        using clock = std::chrono::high_resolution_clock;
    }

    RenderSystem::RenderSystem(gfx::Window& window, LogicSystem& logic)
        : window(&window), logic(logic) {
        sInstance = this;
    }

    std::filesystem::path RenderSystem::GetExeDir() const
    {
#if defined(_WIN32)
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path();
#elif defined(__APPLE__)
        char buf[2048];
        uint32_t sz = sizeof(buf);
        if (_NSGetExecutablePath(buf, &sz) == 0) return std::filesystem::path(buf).parent_path();
        std::string s; s.resize(sz);
        if (_NSGetExecutablePath(s.data(), &sz) == 0) return std::filesystem::path(s).parent_path();
        return std::filesystem::current_path();
#else
        char buf[4096] = {};
        ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n > 0) { buf[n] = 0; return std::filesystem::path(buf).parent_path(); }
        return std::filesystem::current_path();
#endif
    }

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

        for (auto r : rels)
            if (fs::exists(r))
                return std::string(r);

        std::vector<fs::path> roots{ fs::current_path(), GetExeDir() };

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
    std::filesystem::path RenderSystem::FindAssetsRoot() const
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> roots{ fs::current_path(), GetExeDir() };

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

    unsigned RenderSystem::CurrentPlayerTexture() const
    {
        return logic.Animation().running ? runTex : idleTex;
    }

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

    void RenderSystem::ProcessImportedAssets()
    {
        if (assetsRoot.empty())
            return;

        auto pending = assetBrowser.ConsumePendingImports();
        if (pending.empty())
            return;

        for (const auto& relative : pending)
        {
            auto absolute = assetsRoot / relative;
            std::string key = relative.generic_string();

            if (!absolute.empty())
            {
                auto it = Resource_Manager::resources_map.find(key);
                if (it == Resource_Manager::resources_map.end())
                {
                    Resource_Manager::load(key, absolute.string());
                }

                std::string ext = std::filesystem::path(absolute).extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });

                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
                {
                    mygame::UseSpriteFromAsset(relative);
                }
            }
        }
    }

    void RenderSystem::HandleShortcuts()
    {
        if (!window)
            return;

        GLFWwindow* native = window->raw();
        if (!native)
            return;

        auto handleToggle = [&](int key, bool& held)
            {
                const bool pressed = glfwGetKey(native, key) == GLFW_PRESS;
                const bool triggered = pressed && !held;
                held = pressed;
                return triggered;
            };

        if (handleToggle(GLFW_KEY_F10, editorToggleHeld))
            showEditor = !showEditor;

        if (handleToggle(GLFW_KEY_F11, fullscreenToggleHeld))
            gameViewportFullWidth = !gameViewportFullWidth;
    }
    void RenderSystem::HandleViewportPicking()
    {
        if (!window || !FACTORY)
        {
            leftMouseDownPrev = false;
            draggingSelection = false;
            return;
        }

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

        float worldX = 0.0f;
        float worldY = 0.0f;
        bool insideViewport = false;
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
                        dragOffsetX = tr->x - worldX;
                        dragOffsetY = tr->y - worldY;
                        draggingSelection = true;
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
            draggingSelection = false;

        leftMouseDownPrev = mouseDown;
    }

    bool RenderSystem::ScreenToWorld(double cursorX, double cursorY, float& worldX, float& worldY, bool& insideViewport) const
    {
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

        worldX = static_cast<float>(normalizedX * 2.0 - 1.0);
        worldY = static_cast<float>(normalizedY * 2.0 - 1.0);

        insideViewport = (normalizedX >= 0.0 && normalizedX <= 1.0 &&
            normalizedY >= 0.0 && normalizedY <= 1.0);

        return std::isfinite(worldX) && std::isfinite(worldY);
    }

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
                const float radius = circle->radius;
                if (radius > 0.0f)
                    contains = (distanceSq <= radius * radius);
            }
            else
            {
                float width = 1.0f;
                float height = 1.0f;
                if (auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                    Framework::ComponentTypeId::CT_RenderComponent))
                {
                    width = rc->w;
                    height = rc->h;
                }
                else if (!obj->GetComponentType<Framework::SpriteComponent>(
                    Framework::ComponentTypeId::CT_SpriteComponent))
                {
                    continue;
                }

                if (width <= 0.0f)
                    width = 1.0f;
                if (height <= 0.0f)
                    height = 1.0f;

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

    void RenderSystem::UpdateGameViewport()
    {
        if (!window)
            return;

        const int fullWidth = window->Width();
        const int fullHeight = window->Height();
        if (fullWidth <= 0 || fullHeight <= 0)
            return;

        const float minSplit = 0.3f;
        const float maxSplit = 0.7f;
        editorSplitRatio = std::clamp(editorSplitRatio, minSplit, maxSplit);
        //width
        int desiredWidth = fullWidth;
        if (showEditor && !gameViewportFullWidth)
        {
            desiredWidth = static_cast<int>(std::lround(fullWidth * editorSplitRatio));
            const int maxWidth = std::max(1, fullWidth - 1);
            desiredWidth = std::clamp(desiredWidth, 1, maxWidth);
        }
        //height
        // Clamp to 30–100% of window height when not full height.
        if (!gameViewportFullHeight) {
            heightRatio = std::clamp(heightRatio, 0.30f, 1.0f);
        }
        else {
            heightRatio = 1.0f;
        }
        int desiredHeight = static_cast<int>(std::lround(fullHeight * heightRatio));
        desiredHeight = std::clamp(desiredHeight, 1, fullHeight);

        // Center vertically when not using full height
        int yOffset = (fullHeight - desiredHeight) / 2;
        if (gameViewportFullHeight) yOffset = 0;

        // Apply if changed
        if (gameViewport.width != desiredWidth ||
            gameViewport.height != desiredHeight ||
            gameViewport.y != yOffset)
        {
            gameViewport.x = 0;
            gameViewport.y = yOffset;
            gameViewport.width = desiredWidth;
            gameViewport.height = desiredHeight;

            screenW = gameViewport.width;
            screenH = gameViewport.height;

            if (textReadyTitle) textTitle.setViewport(screenW, screenH);
            if (textReadyHint)  textHint.setViewport(screenW, screenH);
        }

        if (gameViewport.width > 0 && gameViewport.height > 0)
            glViewport(gameViewport.x, gameViewport.y, gameViewport.width, gameViewport.height);
    }

    void RenderSystem::RestoreFullViewport()
    {
        if (!window)
            return;

        glViewport(0, 0, window->Width(), window->Height());
    }
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

        const ImVec2 editorPos(viewport->WorkPos.x + static_cast<float>(gameViewport.width), viewport->WorkPos.y);
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
        ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingInCentralNode;
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);
        ImGui::End();

        ImGui::PopStyleVar(2);
    }

    void RenderSystem::DrawViewportControls()
    {
        if (!showEditor)
            return;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 pos = viewport->WorkPos;
        pos.x += 12.0f;
        pos.y += 12.0f;

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;

        if (ImGui::Begin("Viewport Controls", nullptr, flags))
        {
            ImGui::TextUnformatted("Viewport Controls");
            ImGui::Separator();

            bool editorEnabled = showEditor;
            if (ImGui::Checkbox("Editor Enabled (F10)", &editorEnabled))
                showEditor = editorEnabled;

            bool fullWidth = gameViewportFullWidth;
            if (ImGui::Checkbox("Game Full Width (F11)", &fullWidth))
                gameViewportFullWidth = fullWidth;
            if (showEditor && !gameViewportFullWidth)
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
        }
        ImGui::End();
    }

    void RenderSystem::GlfwDropCallback(GLFWwindow*, int count, const char** paths)
    {
        if (sInstance)
            sInstance->HandleFileDrop(count, paths);
    }


    int RenderSystem::CurrentColumns() const
    {
        return logic.Animation().columns;
    }

    int RenderSystem::CurrentRows() const
    {
        return logic.Animation().rows;
    }

    void RenderSystem::Initialize()
    {
        WindowConfig cfg = LoadWindowConfig("../../Data_Files/window.json");
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
        std::cout << "[EXE] " << GetExeDir() << "\n";

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

        Resource_Manager::load("player_png", "../../assets/Textures/player.png");
        playerTex = Resource_Manager::resources_map["player_png"].handle;

        Resource_Manager::load("ming_idle", "../../assets/Textures/Idle Sprite .png");
        Resource_Manager::load("ming_run", "../../assets/Textures/Running Sprite .png");
        idleTex = Resource_Manager::resources_map["ming_idle"].handle;
        runTex = Resource_Manager::resources_map["ming_run"].handle;

        ImGuiLayerConfig config;
        config.glsl_version = "#version 330";
        config.dockspace = true;
        config.gamepad = false;
        if (window)
        {
            ImGuiLayer::Initialize(*window, config);
        }
        else
        {
            std::cerr << "[RenderSystem] Warning: window is null, skipping ImGui initialization.\n";
        }


        assetsRoot = FindAssetsRoot();
        if (!assetsRoot.empty())
        {
            assetBrowser.Initialize(assetsRoot);
            mygame::SetSpawnPanelAssetsRoot(assetsRoot);
        }

        if (window && window->raw())
            glfwSetDropCallback(window->raw(), &RenderSystem::GlfwDropCallback);
    }
    void Framework::RenderSystem::BeginMenuFrame()
    {
        RestoreFullViewport();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Intentionally DO NOT call Graphics::renderBackground() here.
        // The MainMenuPage will draw its own menu.jpg.
        glUseProgram(0);
    }

    void Framework::RenderSystem::EndMenuFrame()
    {
        // Nothing to restore right now; kept for symmetry/future use.
        RestoreFullViewport();
    }

    void RenderSystem::draw()
    {
        TryGuard::Run([&] {
            HandleShortcuts();
            UpdateGameViewport();
            HandleViewportPicking();
            auto t0 = clock::now();

            gfx::Graphics::renderBackground();

            if (FACTORY)
            {
                for (auto& [id, objPtr] : FACTORY->Objects())
                {
                    (void)id;
                    auto* obj = objPtr.get();
                    if (!obj)
                        continue;
                    if (!mygame::ShouldRenderLayer(obj->GetLayerName()))
                        continue;

                    auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent);
                    if (!tr)
                        continue;

                    if (auto* sp = obj->GetComponentType<Framework::SpriteComponent>(
                        Framework::ComponentTypeId::CT_SpriteComponent))
                    {
                        float sx = 1.f, sy = 1.f;
                        float r = 1.f, g = 1.f, b = 1.f, a = 1.f;

                        if (auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                            Framework::ComponentTypeId::CT_RenderComponent))
                        {
                            sx = rc->w; sy = rc->h; r = rc->r; g = rc->g; b = rc->b; a = rc->a;
                        }
                        if (obj->GetObjectName() == "Player" && idleTex && runTex)
                        {
                            gfx::Graphics::renderSpriteFrame(
                                CurrentPlayerTexture(), tr->x, tr->y, tr->rot,
                                sx, sy,
                                logic.Animation().frame, CurrentColumns(), CurrentRows(),
                                r, g, b, a
                            );
                            continue;
                        }

                        unsigned tex = sp->texture_id;
                        if (!tex && !sp->texture_key.empty())
                        {
                            tex = Resource_Manager::getTexture(sp->texture_key);
                            sp->texture_id = tex;
                        }
                        if (tex)
                        {
                            gfx::Graphics::renderSprite(tex, tr->x, tr->y, tr->rot, sx, sy, r, g, b, a);
                        }
                    }
                }

                for (auto& [id, objPtr] : FACTORY->Objects())
                {
                    (void)id;
                    auto* obj = objPtr.get();
                    if (!obj)
                        continue;
                    if (!mygame::ShouldRenderLayer(obj->GetLayerName()))
                        continue;

                    auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent);
                    auto* rc = obj->GetComponentType<Framework::RenderComponent>(
                        Framework::ComponentTypeId::CT_RenderComponent);
                    if (!tr || !rc)
                        continue;
                    if (obj->GetComponentType<Framework::SpriteComponent>(
                        Framework::ComponentTypeId::CT_SpriteComponent))
                        continue;

                    gfx::Graphics::renderRectangle(tr->x, tr->y, tr->rot, rc->w, rc->h, rc->r, rc->g, rc->b, rc->a);
                }

                for (auto& [id, objPtr] : FACTORY->Objects())
                {
                    (void)id;
                    auto* obj = objPtr.get();
                    if (!obj)
                        continue;
                    if (!mygame::ShouldRenderLayer(obj->GetLayerName()))
                        continue;
                    auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent);
                    auto* cc = obj->GetComponentType<Framework::CircleRenderComponent>(
                        Framework::ComponentTypeId::CT_CircleRenderComponent);
                    if (!tr || !cc)
                        continue;

                    gfx::Graphics::renderCircle(tr->x, tr->y, cc->radius, cc->r, cc->g, cc->b, cc->a);
                }
                if (showPhysicsHitboxes)
                {
                    for (auto& [id, objPtr] : FACTORY->Objects())
                    {
                        (void)id;
                        auto* obj = objPtr.get();
                        if (!obj)
                            continue;

                        auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                            Framework::ComponentTypeId::CT_TransformComponent);
                        auto* rb = obj->GetComponentType<Framework::RigidBodyComponent>(
                            Framework::ComponentTypeId::CT_RigidBodyComponent);
                        if (!tr || !rb)
                            continue;

                        gfx::Graphics::renderRectangleOutline(tr->x, tr->y, 0.0f,
                            rb->width, rb->height,
                            1.f, 0.f, 0.f, 1.f,
                            2.f);
                    }
                }
            }

            if (textReadyTitle)
            {
                textTitle.RenderText(
                    "Bloody Good Curry",
                    32.0f,
                    static_cast<float>(screenH) - 64.0f,
                    1.05f,
                    glm::vec3(1.0f, 1.0f, 1.0f)
                );
            }
            if (textReadyHint)
            {
                textHint.RenderText(
                    "Press WASD to run",
                    32.0f,
                    40.0f,
                    0.75f,
                    glm::vec3(0.95f, 0.85f, 0.10f)
                );
            }

            const double renderMs = std::chrono::duration<double, std::milli>(clock::now() - t0).count();
            Framework::setRender(renderMs);

            RestoreFullViewport();

            t0 = clock::now();

            DrawDockspace();
            if (showEditor)
            {
                DrawViewportControls();
                assetBrowser.Draw();
                mygame::DrawHierarchyPanel();

                mygame::DrawSpawnPanel();

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

    void RenderSystem::Shutdown()
    {

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

};