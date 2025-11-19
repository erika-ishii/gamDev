/*********************************************************************************************
 \file      Window.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief    setup window

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <string>
#include <functional>

// Forward-declare GLFWwindow to avoid leaking GLFW headers in the header file
struct GLFWwindow;

namespace gfx {

    // Minimal window wrapper that exposes per-frame controls,
    // so the main Game/Engine can own the game loop.
    class Window {
    public:
        // Create an OpenGL context and a window with the given size and title.
        Window(int width, int height, const char* title, bool startFullscreen = true);

        // Destroy the window and terminate GLFW (if needed).
        ~Window();

        //From Parminder
        void run();               // open window and loop until closed
        void runWithCallback(std::function<void()> updateCallback); // run with custom update callback
        bool isKeyPressed(int key) const; // check if key is pressed
        bool isOpen() const;      // check if window is still open
        void close();             // close the window

        // ---- Per-frame controls (call these from your Engine/Game loop) ----

        // Returns true if the OS asked to close the window.
        bool shouldClose() const;

        // Poll OS / input events (keyboard, mouse, etc.).
        void pollEvents();

        // Start a new frame (clear color/depth, begin ImGui frame if you use it).
        void beginFrame();

        // End the frame (end ImGui frame if you use it).
        void endFrame();

        // Present the back buffer.
        void swapBuffers();

        // Static GLFW error callback.
        static void error_cb(int error, const char* description);

        // Toggle fullscreen/windowed at runtime (non-resizable windowed mode).
        void ToggleFullscreen();
        bool IsFullscreen() const { return m_fullscreen; }

        GLFWwindow* raw() const { return s_window; }

        // Current window size (logical pixels)
        int Width()  const { return m_width; }
        int Height() const { return m_height; }


    private:
        // Global raw pointer to the GLFW window. (kept static to match your original design)
        static GLFWwindow* s_window;

        int m_width;
        int m_height;
        std::string m_title;
        bool m_fullscreen = true;
        int  m_windowedX = 100;
        int  m_windowedY = 100;
        int  m_windowedWidth = 0;
        int  m_windowedHeight = 0;
    };

} // namespace gfx
