/*********************************************************************************************
 \file      RenderSystem.h
 \par       SofaSpuds
 \author    yimo.kong ( yimo.kong@digipen.edu) - Primary Author, 50%
            erika.ishii (erika.ishii@digipen.edu) - Author, 30%
            elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Author, 10%
            h.jun (h.jun@digipen.edu) - Author, 10%
 \brief     Editor-aware 2D render system: game viewport, UI dockspace, text, and picking.
 \details   Drives all frame-time drawing for the sandbox/editor:
            - Game viewport: manages full/partial splits, camera control, screen→world unproject.
            - Editor UI: ImGui dockspace, asset browser, JSON editor, hierarchy/selection.
            - Text: title + hint text renderers with lazy font/asset resolution.
            - Picking: cursor-to-world, object hit tests, and selection framing.
            Provides Begin/EndMenuFrame helpers (without drawing the engine default background).
            Exposes editor visibility for other systems to react to UI state.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include "LogicSystem.h"
#include "Component/CircleRenderComponent.h"
#include "Component/RenderComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/TransformComponent.h"
#include "Component/SpriteAnimationComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/PlayerAttackComponent.h"

#include "Config/WindowConfig.h"

#include "Debug/ImGuiLayer.h"
#include "Debug/Perf.h"
#include "Debug/Spawn.h"
#include "Debug/Selection.h"
#include "Debug/HierarchyPanel.h"
#include "Debug/InspectorPanel.h"
#include "Debug/AssetBrowserPanel.h"
#include "Debug/AnimationEditorPanel.h"
#include "Debug/JsonEditorPanel.h"

#include "Factory/Factory.h"
#include "Graphics/Graphics.hpp"
#include "Graphics/Camera2D.hpp"
#include "Graphics/Window.hpp"
#include "Graphics/GraphicsText.hpp"
#include "Resource_Manager/Resource_Manager.h"

#include <array>
#include <filesystem>
#include <string>

#include <imgui.h>
#include <glm/vec2.hpp>

struct GLFWwindow;

namespace Framework {

    /*************************************************************************************
      \class  RenderSystem
      \brief  Editor-aware renderer for the 2D sandbox: draws game + tools UI each frame.
      \note   Offers text renderers, picking helpers, and viewport/camera management.
    *************************************************************************************/
    class RenderSystem : public Framework::ISystem {
    public:
        /*************************************************************************
          \brief  Construct with a target window and logic system reference.
        *************************************************************************/
        RenderSystem(gfx::Window& window, LogicSystem& logic);

        /*************************************************************************
          \brief  Initialize render assets, fonts, cameras, and editor panels.
        *************************************************************************/
        void Initialize() override;

        /*************************************************************************
          \brief  No-op for now (rendering is driven in draw()).
          \param  dt  Delta time in seconds.
        *************************************************************************/
        void Update(float dt) override { (void)dt; }

        /*************************************************************************
          \brief  System name for profiling / diagnostics.
        *************************************************************************/
        std::string GetName() override { return "RenderSystem"; }

        /*************************************************************************
          \brief  Release graphics/editor resources.
        *************************************************************************/
        void Shutdown() override;

        /*************************************************************************
          \brief  Render the current frame: game viewport + editor UI.
        *************************************************************************/
        void draw() override;

        // Menu helpers (do NOT draw the engine default background here)
        /// \brief Begin drawing a simple menu bar frame (no default background).
        void BeginMenuFrame();
        /// \brief End drawing the menu bar frame.
        void EndMenuFrame();
        /// \brief Handle fullscreen/editor shortcuts when only menu UI is rendering.
        void HandleMenuShortcuts();
        /// \brief Returns true if the editor UI is visible (for other systems to adapt).
        static bool IsEditorVisible();

        /// \brief Global accessor to the current RenderSystem instance.
        static RenderSystem* Get();

        // Text accessors
        /// \brief  True if the hint text renderer is ready (font/atlas loaded).
        bool IsTextReadyHint()  const { return textReadyHint; }
        /// \brief  True if the title text renderer is ready.
        bool IsTextReadyTitle() const { return textReadyTitle; }
        /// \brief  Access the hint text renderer.
        gfx::TextRenderer& GetTextHint() { return textHint; }
        /// \brief  Access the title text renderer.
        gfx::TextRenderer& GetTextTitle() { return textTitle; }

        /// \brief  Back-buffer width in pixels.
        int ScreenWidth()  const { return screenW; }
        /// \brief  Back-buffer height in pixels.
        int ScreenHeight() const { return screenH; }

        /// \brief  Convert a screen cursor position to world space using the active camera.
        bool ScreenToWorld(double cursorX, double cursorY,
            float& worldX, float& worldY,
            bool& insideViewport) const;

    private:
        // --- Filesystem / asset resolution ------------------------------------------------
        std::string           FindRoboto() const;
        std::filesystem::path FindAssetsRoot() const;
        std::filesystem::path FindDataFilesRoot() const;

        // --- Asset import / file-drop -----------------------------------------------------
        void HandleFileDrop(int count, const char** paths);
        void ProcessImportedAssets();

        // --- Editor frame scaffolding -----------------------------------------------------
        void DrawDockspace();
        void HandleShortcuts();
        void HandleViewportPicking();

        // --- Camera & picking helpers -----------------------------------------------------
        void  UpdateEditorCameraControls(GLFWwindow* native, const ImGuiIO& io,
            double cursorX, double cursorY);

        bool  CursorToViewportNdc(double cursorX, double cursorY,
            float& ndcX, float& ndcY,
            bool& insideViewport) const;

        bool  UnprojectWithCamera(const gfx::Camera2D& cam,
            float ndcX, float ndcY,
            float& worldX, float& worldY) const;

        bool  ShouldUseEditorCamera() const;
        void  FrameEditorSelection();
        Framework::GOCId TryPickObject(float worldX, float worldY) const;

        // --- Viewport layout --------------------------------------------------------------
        void UpdateGameViewport();
        void RestoreFullViewport();
        void DrawViewportControls();

        // --- GLFW static callback ---------------------------------------------------------
        static void GlfwDropCallback(GLFWwindow* window, int count, const char** paths);

        // --- Temporary sprite-sheet helpers ----------------------------------------------
        unsigned CurrentPlayerTexture() const;
        int      CurrentColumns() const;
        int      CurrentRows() const;

        // --- Core references --------------------------------------------------------------
        gfx::Window* window = nullptr;   //!< Target graphics window (native + GL context).
        LogicSystem& logic;              //!< Game/scene access.

        // --- Singleton bridge (for callbacks) --------------------------------------------
        static RenderSystem* sInstance;

        // --- Editor panels & roots --------------------------------------------------------
        mygame::AssetBrowserPanel assetBrowser;
        std::filesystem::path     assetsRoot;
        mygame::JsonEditorPanel   jsonEditor;
        std::filesystem::path     dataFilesRoot;

        // --- Frame/viewport state ---------------------------------------------------------
        int  screenW = 1280;             //!< Back-buffer width.
        int  screenH = 720;             //!< Back-buffer height.

        // --- Text ------------------------------------------------------------------------
        gfx::TextRenderer textTitle;     //!< Title text (e.g., big header).
        gfx::TextRenderer textHint;      //!< Hint text (e.g., input/help).
        bool textReadyTitle = false;     //!< True once title font is ready.
        bool textReadyHint = false;      //!< True once hint font is ready.

        // --- Demo textures (player / animation) ------------------------------------------
        unsigned playerTex = 0;               //!< Legacy fallback player texture.
        unsigned idleTex = 0;               //!< Idle animation sheet.
        unsigned runTex = 0;               //!< Run animation sheet.
        std::array<unsigned, 3> attackTex{};  //!< Combo attack sheets (1st / 2nd / 3rd).
        unsigned knifeTex = 0;              //!< Animated knife projectile sheet.

        // --- Game viewport rectangle (pixels) --------------------------------------------
        struct ViewRect {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
        };

        ViewRect gameViewport{};              //!< Active game viewport in pixels.

        // --- Editor layout flags ---------------------------------------------------------
        bool  showEditor = false;  //!< Toggle editor UI visibility.
        bool  showAnimationEditor = false; //!< Toggle Animation Editor visibility.
        bool  gameViewportFullWidth = false; //!< Maximize viewport width.
        bool  gameViewportFullHeight = false; //!< Maximize viewport height.
        float heightRatio = 0.8f;   //!< Viewport height vs window height.
        float editorSplitRatio = 0.5f;   //!< Editor/game split ratio.

        bool  editorToggleHeld = false;  //!< Debounce toggle key (F10).
        bool  fullscreenToggleHeld = false;  //!< Debounce fullscreen toggle (F11).
        bool  deleteKeyHeld = false;  //!< Debounce Delete shortcut for removing objects.
        bool  translateKeyHeld = false; //!< Debounce translate gizmo hotkey (T).
        bool  rotateKeyHeld = false;    //!< Debounce rotate gizmo hotkey (R).
        bool  scaleKeyHeld = false;     //!< Debounce scale gizmo hotkey (S).
        bool  showPhysicsHitboxes = true;   //!< Debug: draw physics hitboxes.

        // --- Mouse drag selection --------------------------------------------------------
        bool  leftMouseDownPrev = false;
        bool  draggingSelection = false;
        float dragOffsetX = 0.0f;
        float dragOffsetY = 0.0f;

        // --- Game camera -----------------------------------------------------------------
        gfx::Camera2D camera;                 //!< In-game camera.
        float         cameraViewHeight = 1.0f; //!< Ortho view height (world units).
        bool          cameraEnabled = true; //!< Toggle in-game camera control.

        // --- Editor camera ---------------------------------------------------------------
        gfx::Camera2D editorCamera;                //!< Editor camera.
        float         editorCameraViewHeight = 1.0f;
        bool          editorCameraPanning = false;
        glm::vec2     editorCameraPanStartWorld{ 0.0f, 0.0f };
        glm::vec2     editorCameraPanStartFocus{ 0.0f, 0.0f };
        bool          editorFrameHeld = false;

        // --- Layout persistence -----------------------------------------------------------
        std::string imguiLayoutPath{};       //!< Optional saved ImGui layout path.
    };

} // namespace Framework
