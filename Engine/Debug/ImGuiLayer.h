/*********************************************************************************************
 \file      ImGuiLayer.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the ImGuiLayer interface, which integrates Dear ImGui into the engine.
            Provides lifecycle functions for initialization, per-frame updates, and
            shutdown. Also defines ImGuiLayerConfig for runtime configuration.

 \details   ImGuiLayer acts as a thin wrapper around Dear ImGui’s GLFW and OpenGL3 backends.
            It allows the engine to easily enable debug UIs, in-game overlays, and
            editor-like interfaces using ImGui. Configuration options such as docking
            and gamepad navigation are exposed via ImGuiLayerConfig.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Graphics/Window.hpp"

/*****************************************************************************************
  \struct ImGuiLayerConfig
  \brief  Configuration options for initializing ImGuiLayer.

  - glsl_version : The GLSL version string compatible with the active OpenGL context.
  - dockspace    : Enables ImGui docking support if true.
  - gamepad      : Enables gamepad navigation if true.

  Example usage:
  \code
    ImGuiLayerConfig cfg;
    cfg.glsl_version = "#version 450";
    cfg.dockspace = true;
    cfg.gamepad = true;
    ImGuiLayer::Initialize(window, cfg);
  \endcode
*****************************************************************************************/
struct ImGuiLayerConfig {
    const char* glsl_version = "#version 330"; ///< GLSL version string (default: 330 core)
    bool dockspace = true;                     ///< Enable ImGui docking
    bool gamepad = false;                      ///< Enable gamepad navigation
};

/*****************************************************************************************
  \class ImGuiLayer
  \brief Static interface for managing the lifecycle of ImGui within the engine.

  Responsibilities:
  - Initialize ImGui with the correct context, backends, and configuration.
  - Manage the frame lifecycle (BeginFrame / EndFrame).
  - Safely shut down ImGui and release resources.

  This abstraction simplifies integration with the engine’s render loop.
*****************************************************************************************/
class ImGuiLayer {
public:
    /*************************************************************************************
      \brief Initializes ImGui with the specified window and configuration.
      \param win Reference to the engine’s gfx::Window wrapper (provides GLFW handle).
      \param cfg Configuration struct (defaults to ImGuiLayerConfig with docking enabled).
    *************************************************************************************/
    static void Initialize(gfx::Window& win, const ImGuiLayerConfig& cfg = {});

    /*************************************************************************************
      \brief Begins a new ImGui frame.
      \note Call once per frame, after pollEvents() but before any UI rendering.
    *************************************************************************************/
    static void BeginFrame();   // call each frame (after pollEvents)

    /*************************************************************************************
      \brief Ends the ImGui frame and renders the draw data.
      \note Call once per frame, before swapBuffers().
    *************************************************************************************/
    static void EndFrame();     // call each frame (before swapBuffers)

    /*************************************************************************************
      \brief Shuts down ImGui and cleans up resources.
      \note Should be called during engine shutdown to release ImGui context/backends.
    *************************************************************************************/
    static void Shutdown();
};
