/*********************************************************************************************
 \file      ImGuiLayer.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the ImGuiLayer class, which integrates the Dear ImGui library
            into the engine. Provides initialization, per-frame begin/end calls,
            and shutdown management for the ImGui context and backends.

 \details   This module sets up Dear ImGui with GLFW and OpenGL3 backends, enables
            input navigation options, and manages the frame lifecycle. The ImGuiLayer
            serves as a lightweight abstraction for enabling debug UIs, editors,
            and in-game overlays using ImGui.

 \note      Uses a static internal flag (s_imguiReady) to prevent redundant calls
            when ImGui is not initialized or has already been shut down.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

// Engine/Graphics/ImGuiLayer.cpp
#include "ImGuiLayer.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

/// Internal state flag to track whether ImGui has been initialized.
static bool s_imguiReady = false;

/*************************************************************************************
  \brief Initializes ImGui with GLFW and OpenGL3 backends.
  \param win Reference to the gfx::Window wrapper (provides native GLFWwindow).
  \param cfg Reference to the ImGuiLayerConfig struct specifying options
             (e.g., gamepad navigation, docking, GLSL version).
  \note
    - Creates an ImGui context.
    - Configures navigation and docking based on cfg flags.
    - Initializes GLFW + OpenGL3 bindings.
    - Marks ImGui as ready for use.
*************************************************************************************/
void ImGuiLayer::Initialize(gfx::Window& win, const ImGuiLayerConfig& cfg) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Options
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (cfg.gamepad)  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    if (cfg.dockspace) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    // Backends
    GLFWwindow* native = win.raw();
    // true = install callbacks and chain to existing ones
    ImGui_ImplGlfw_InitForOpenGL(native, /*install_callbacks*/ true);
    ImGui_ImplOpenGL3_Init(cfg.glsl_version);

    s_imguiReady = true;
}

/*************************************************************************************
  \brief Begins a new ImGui frame.
  \note Returns immediately if ImGui has not been initialized.
*************************************************************************************/
void ImGuiLayer::BeginFrame() {
    if (!s_imguiReady) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

/*************************************************************************************
  \brief Ends the current ImGui frame and renders the UI.
  \note Returns immediately if ImGui has not been initialized.
*************************************************************************************/
void ImGuiLayer::EndFrame() {
    if (!s_imguiReady) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

/*************************************************************************************
  \brief Shuts down ImGui and releases its resources.
  \note
    - Safely cleans up OpenGL3 + GLFW backends.
    - Destroys the ImGui context.
    - Marks ImGui as no longer ready.
*************************************************************************************/
void ImGuiLayer::Shutdown() {
    if (!s_imguiReady) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    s_imguiReady = false;
}
