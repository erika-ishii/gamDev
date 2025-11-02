#pragma once
#include "LogicSystem.h"
#include "Component/CircleRenderComponent.h"
#include "Component/RenderComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TransformComponent.h"
#include "Config/WindowConfig.h"
#include "Debug/ImGuiLayer.h"
#include "Debug/Perf.h"
#include "Debug/Spawn.h"
#include "Debug/Selection.h"
#include "Debug/HierarchyPanel.h"
#include "Debug/AssetBrowserPanel.h"
#include "Factory/Factory.h"
#include "Graphics/Graphics.hpp"
#include "Graphics/Window.hpp"
#include "Graphics/GraphicsText.hpp"
#include "Resource_Manager/Resource_Manager.h"

#include <filesystem>

struct GLFWwindow;

namespace Framework {


    class RenderSystem : public Framework::ISystem {
    public:
        RenderSystem(gfx::Window& window, LogicSystem& logic);
        void Initialize() override;
        void Update(float dt) override { (void)dt; }
        std::string GetName() override { return "RenderSystem"; }
        void Shutdown() override;
        void draw() override;

        // Menu helpers (do NOT draw the engine default background here)
        void BeginMenuFrame();
        void EndMenuFrame();

        // Text accessors
        bool IsTextReadyHint()  const { return textReadyHint; }
        bool IsTextReadyTitle() const { return textReadyTitle; }
        gfx::TextRenderer& GetTextHint() { return textHint; }
        gfx::TextRenderer& GetTextTitle() { return textTitle; }

        int ScreenWidth()  const { return screenW; }
        int ScreenHeight() const { return screenH; }

    private:
        std::filesystem::path GetExeDir() const;
        std::string           FindRoboto() const;
        std::filesystem::path FindAssetsRoot() const;
        void HandleFileDrop(int count, const char** paths);
        void ProcessImportedAssets();
        void DrawDockspace();
        void HandleShortcuts();
        void HandleViewportPicking();
        bool ScreenToWorld(double cursorX, double cursorY, float& worldX, float& worldY, bool& insideViewport) const;
        Framework::GOCId TryPickObject(float worldX, float worldY) const;
        void UpdateGameViewport();
        void RestoreFullViewport();
        void DrawViewportControls();
        static void GlfwDropCallback(GLFWwindow* window, int count, const char** paths);

        unsigned CurrentPlayerTexture() const;
        int      CurrentColumns() const;
        int      CurrentRows() const;

        gfx::Window* window = nullptr;
        LogicSystem& logic;

        static RenderSystem* sInstance;

        mygame::AssetBrowserPanel assetBrowser;
        std::filesystem::path assetsRoot;



        int screenW = 1280, screenH = 720;

        gfx::TextRenderer textTitle, textHint;
        bool textReadyTitle = false, textReadyHint = false;

        unsigned playerTex = 0, idleTex = 0, runTex = 0;

        struct ViewRect
        {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
        };

        ViewRect gameViewport{};
        bool showEditor = true;
        bool gameViewportFullWidth = false;
        bool  gameViewportFullHeight = false;
        float heightRatio = 0.8f;
        float editorSplitRatio = 0.5f;
        bool editorToggleHeld = false;
        bool fullscreenToggleHeld = false;
        bool showPhysicsHitboxes = true;

        bool leftMouseDownPrev = false;
        bool draggingSelection = false;
        float dragOffsetX = 0.0f;
        float dragOffsetY = 0.0f;
    };

} // namespace Framework
