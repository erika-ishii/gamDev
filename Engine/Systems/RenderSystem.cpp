
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
#include <system_error>
#include <vector>
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
        ImGuiLayer::Initialize(*window, config);

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
    }

    void RenderSystem::draw()
    {
        TryGuard::Run([&] {
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
                    auto* tr = obj->GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent);
                    auto* cc = obj->GetComponentType<Framework::CircleRenderComponent>(
                        Framework::ComponentTypeId::CT_CircleRenderComponent);
                    if (!tr || !cc)
                        continue;

                    gfx::Graphics::renderCircle(tr->x, tr->y, cc->radius, cc->r, cc->g, cc->b, cc->a);
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

            t0 = clock::now();

            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                static bool dockspaceOpen = true;
                static bool dockspaceFullscreen = true;
                static bool dockspacePadding = false;
                ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
                ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

                if (dockspaceFullscreen)
                {
                    const ImGuiViewport* viewport = ImGui::GetMainViewport();
                    ImGui::SetNextWindowPos(viewport->Pos);
                    ImGui::SetNextWindowSize(viewport->Size);
                    ImGui::SetNextWindowViewport(viewport->ID);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                    dockspaceFlags |= ImGuiDockNodeFlags_PassthruCentralNode;
                    windowFlags |= ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                        ImGuiWindowFlags_NoNavFocus |
                        ImGuiWindowFlags_NoBackground;
                }
                else
                {
                    dockspaceFlags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
                }

                if (!dockspacePadding)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                }

                ImGui::Begin("DockSpaceRoot", dockspaceFullscreen ? nullptr : &dockspaceOpen, windowFlags);

                if (!dockspacePadding)
                {
                    ImGui::PopStyleVar();
                }
                if (dockspaceFullscreen)
                {
                    ImGui::PopStyleVar(2);
                }

                ImGuiID dockspaceID = ImGui::GetID("DockSpaceRoot");
                ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);

                ImGui::End();
            }

            assetBrowser.Draw();
            ProcessImportedAssets();

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

            Framework::DrawPerformanceWindow();

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