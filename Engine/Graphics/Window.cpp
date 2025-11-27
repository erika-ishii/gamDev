/*********************************************************************************************
 \file      Window.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Implements the gfx::Window wrapper around GLFW + OpenGL context creation.
            Handles window lifecycle, fullscreen toggling, focus/iconify callbacks, and
            basic frame management (clear, swap, event polling).
 \details   Responsibilities:
            - Initialize and terminate GLFW.
            - Create an OpenGL 4.5 core-profile context via GLFW.
            - Track fullscreen/windowed size and position and allow toggling at runtime.
            - Maintain focus/minimize (iconify) state via GLFW callbacks.
            - Provide helper loop functions (run / runWithCallback) for simple main loops.
            - Expose basic query functions (isKeyPressed, isOpen, shouldClose).
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Graphics/Window.hpp"

// Keep GL/GLFW only in the .cpp to avoid polluting headers.
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../Sandbox/MyGame/Game.hpp"
#include <iostream>
#include <stdexcept>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace {

    // Prefer constexpr over macros (resolves your VCR101 suggestion)
    constexpr int kGlMajor = 4; ///< Requested OpenGL major version.
    constexpr int kGlMinor = 5; ///< Requested OpenGL minor version.

} // anonymous namespace

// Define the static window pointer
GLFWwindow* gfx::Window::s_window = nullptr;

namespace gfx {

    /*************************************************************************************
      \brief Construct a Window, initialize GLFW, and create an OpenGL context.

      \param width           Initial window width (in windowed mode).
      \param height          Initial window height (in windowed mode).
      \param title           Window title string.
      \param startFullscreen If true, start in fullscreen using the primary monitor.

      Steps:
      - Initialize GLFW and set a global error callback.
      - Request an OpenGL 4.5 core-profile context.
      - Create either a fullscreen or windowed GLFWwindow.
      - Store windowed position/size for future fullscreen toggles.
      - Hook up iconify/focus callbacks and sync state.
      - Initialize GLAD and print renderer/version info.
      - Set initial viewport and enable VSync.
    *************************************************************************************/
    Window::Window(int width, int height, const char* title, bool startFullscreen)
        : m_width(width), m_height(height), m_title(title),
        m_fullscreen(startFullscreen),
        m_windowedWidth(width), m_windowedHeight(height)
    {
        if (!glfwInit()) {
            std::cerr << "GLFW initialization failed.\n";
            throw std::runtime_error("GLFW init failed");
        }

        // Set error callback first so we catch any GLFW errors
        glfwSetErrorCallback(Window::error_cb);

        // Request a modern OpenGL context (4.5 core)
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, kGlMajor);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, kGlMinor);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        // Double buffered (default)
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

        // Allow resizing (editor/game may rely on this)
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;

        if (m_fullscreen && monitor && mode)
        {
            // Start in fullscreen mode on primary monitor
            m_width = mode->width;
            m_height = mode->height;
            s_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), monitor, nullptr);

            // Center the future windowed position if we later toggle back.
            m_windowedX = (mode->width - m_windowedWidth) / 2;
            m_windowedY = (mode->height - m_windowedHeight) / 2;
        }
        else
        {
            // Fallback to windowed mode if fullscreen cannot be used
            m_fullscreen = false;
            s_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
            glfwGetWindowPos(s_window, &m_windowedX, &m_windowedY);
        }

        if (!s_window) {
            std::cerr << "Failed to create GLFW window.\n";
            glfwTerminate();
            throw std::runtime_error("GLFW window creation failed");
        }

        // Store this in the GLFW user pointer so static callbacks can retrieve the Window instance.
        // Register:
        // OnIconify(GLFWwindow* win, int iconified)
        // OnFocus(GLFWwindow* win, int focused)
        // Initialize the flags from the current GLFW attributes via SyncFocusFromAttribs
        glfwSetWindowUserPointer(s_window, this);
        glfwSetWindowIconifyCallback(s_window, &Window::OnIconify);
        glfwSetWindowFocusCallback(s_window, &Window::OnFocus);
        SyncFocusFromAttribs(this, s_window);

        // Make the context current
        glfwMakeContextCurrent(s_window);

        // Load GL function pointers with GLAD
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwDestroyWindow(s_window);
            s_window = nullptr;
            glfwTerminate();
            throw std::runtime_error("Failed to initialize GLAD");
        }

        // Print renderer and version info for debugging
        const GLubyte* renderer = glGetString(GL_RENDERER);
        const GLubyte* version = glGetString(GL_VERSION);

        std::cout << "Renderer: " << renderer << "\n";
        std::cout << "OpenGL version supported: " << version << "\n";

        // Set initial viewport and vsync
        glViewport(0, 0, m_width, m_height);
        glfwSwapInterval(1); // vsync on
    }

    /*************************************************************************************
      \brief Destructor for Window.

      Destroys the GLFW window and terminates GLFW. Assumes this is the only active
      Window instance managing GLFW.
    *************************************************************************************/
    Window::~Window() {
        if (s_window) {
            glfwDestroyWindow(s_window);
            s_window = nullptr;
        }
        glfwTerminate();
    }

    /*************************************************************************************
      \brief Check if the underlying GLFW window has requested to close.

      \return True if the user or system requested window close.
    *************************************************************************************/
    bool Window::shouldClose() const {
        return glfwWindowShouldClose(s_window) != 0;
    }

    /*************************************************************************************
      \brief Poll window events (keyboard, mouse, window messages).

      Wraps glfwPollEvents(). Should be called once per frame in the main loop
      so input and window messages stay responsive.
    *************************************************************************************/
    void Window::pollEvents() {
        glfwPollEvents();
    }

    /*************************************************************************************
      \brief Begin a new frame by clearing the color buffer.

      Sets a dark gray-ish clear color and clears the color buffer. Additional buffers
      (depth, stencil) can be added later if needed.
    *************************************************************************************/
    void Window::beginFrame() {
        // Clear the color buffer (set your preferred clear color)
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    /*************************************************************************************
      \brief End the frame.

      Currently a placeholder; you can add additional end-of-frame operations here
      (e.g., ImGui::Render/Draw, debug overlays, etc.) before swapBuffers().
    *************************************************************************************/
    void Window::endFrame() {
        // Intentionally empty; reserved for end-of-frame operations.
    }

    /*************************************************************************************
      \brief Swap the front and back buffers for the window.

      Wraps glfwSwapBuffers(). Should be called once per frame after rendering is done.
    *************************************************************************************/
    void Window::swapBuffers() {
        glfwSwapBuffers(s_window);
    }

    /*************************************************************************************
      \brief Sync internal focus/iconify flags from GLFW window attributes.

      \param self Pointer to the Window instance.
      \param win  Pointer to the GLFWwindow.

      Reads:
      - GLFW_ICONIFIED → m_iconified
      - GLFW_FOCUSED   → m_focused
    *************************************************************************************/
    void Window::SyncFocusFromAttribs(Window* self, GLFWwindow* win)
    {
        if (!self || !win) return;
        self->m_iconified = glfwGetWindowAttrib(win, GLFW_ICONIFIED) == GLFW_TRUE;
        self->m_focused = glfwGetWindowAttrib(win, GLFW_FOCUSED) == GLFW_TRUE;
    }

    /*************************************************************************************
      \brief GLFW iconify callback.

      \param win       GLFW window.
      \param iconified Non-zero if the window was minimized (iconified), zero otherwise.

      When the window is minimized, m_iconified = true and m_focused is also cleared.
    *************************************************************************************/
    void Window::OnIconify(GLFWwindow* win, int iconified)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (!self) return;
        self->m_iconified = (iconified == GLFW_TRUE);
        if (iconified == GLFW_TRUE)
        {
            self->m_focused = false;
        }
    }

    /*************************************************************************************
      \brief GLFW focus callback.

      \param win     GLFW window.
      \param focused Non-zero if the window gained focus, zero if it lost focus.

      When focus changes, you:
      - Update m_focused.
      - Call SyncFocusFromAttribs to ensure both flags match GLFW’s view.
      So any ALT-TAB / CTRL-ALT-DEL transition will result in these booleans
      being set correctly.
    *************************************************************************************/
    void Window::OnFocus(GLFWwindow* win, int focused)
    {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (!self) return;
        self->m_focused = (focused == GLFW_TRUE);
        SyncFocusFromAttribs(self, win);
    }

    /*************************************************************************************
      \brief Toggle between fullscreen and windowed modes.

      Behavior:
      - If currently fullscreen:
        * Switch to windowed mode at stored m_windowedX/Y and m_windowedWidth/Height.
        * Restore decorations and disable resizing.
      - If currently windowed:
        * Save current position/size as "windowed" values.
        * Switch to fullscreen on primary monitor using its current resolution.
        * Optionally hide decorations.

      After changing size, glViewport() is updated to match the new dimensions.
    *************************************************************************************/
    void Window::ToggleFullscreen()
    {
        if (!s_window)
            return;

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;

        if (m_fullscreen)
        {
            // Going back to WINDOWED mode
            glfwSetWindowMonitor(
                s_window,
                nullptr,
                m_windowedX,
                m_windowedY,
                m_windowedWidth,
                m_windowedHeight,
                0
            );

            m_width = m_windowedWidth;
            m_height = m_windowedHeight;
            m_fullscreen = false;

            // Windowed mode: fixed size + border + title bar
            glfwSetWindowAttrib(s_window, GLFW_DECORATED, GLFW_TRUE);
            glfwSetWindowAttrib(s_window, GLFW_RESIZABLE, GLFW_FALSE);
        }
        else if (monitor && mode)
        {
            // Going to FULLSCREEN
            glfwGetWindowPos(s_window, &m_windowedX, &m_windowedY);
            glfwGetWindowSize(s_window, &m_windowedWidth, &m_windowedHeight);

            // Optional, but nice: hide decorations in fullscreen
            glfwSetWindowAttrib(s_window, GLFW_DECORATED, GLFW_FALSE);

            glfwSetWindowMonitor(
                s_window,
                monitor,
                0,
                0,
                mode->width,
                mode->height,
                mode->refreshRate
            );

            m_width = mode->width;
            m_height = mode->height;
            m_fullscreen = true;
        }

        // Keep GL viewport in sync with logical width/height.
        glViewport(0, 0, m_width, m_height);
    }

    /*************************************************************************************
      \brief Simple main loop helper that clears the screen and polls events.

      Runs until the GLFW window should close. Intended for quick tests or tools.
      For the actual game, use runWithCallback() so systems can update per frame.
    *************************************************************************************/
    void Window::run() {
        while (!glfwWindowShouldClose(Window::s_window)) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glfwSwapBuffers(Window::s_window);
            glfwPollEvents();
        }
    }

    /*************************************************************************************
      \brief Main loop helper that accepts a per-frame callback.

      \param updateCallback Callback invoked once per frame before swapping buffers.
                            Used by the engine to update systems (audio, input, game).

      The loop runs until the window requests close, clearing the screen, calling
      the user callback, swapping buffers, then polling events each iteration.
    *************************************************************************************/
    void Window::runWithCallback(std::function<void()> updateCallback) {
        while (!glfwWindowShouldClose(Window::s_window)) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Call the custom update callback (for audio updates, input handling, etc.)
            if (updateCallback) {
                updateCallback();
            }

            glfwSwapBuffers(Window::s_window);
            glfwPollEvents();
        }
    }

    /*************************************************************************************
      \brief Query whether a given key is currently pressed.

      \param key GLFW key code (e.g., GLFW_KEY_ESCAPE).
      \return True if the key is reported as pressed by GLFW.
    *************************************************************************************/
    bool Window::isKeyPressed(int key) const {
        return glfwGetKey(Window::s_window, key) == GLFW_PRESS;
    }

    /*************************************************************************************
      \brief Check if the window is still considered "open".

      \return True if the window has not requested to close.
    *************************************************************************************/
    bool Window::isOpen() const {
        return !glfwWindowShouldClose(Window::s_window);
    }

    /*************************************************************************************
      \brief Request that the window should close.

      Sets the GLFW "should close" flag, so subsequent shouldClose()/isOpen() calls will
      reflect the shutdown request and loops can exit cleanly.
    *************************************************************************************/
    void Window::close() {
        glfwSetWindowShouldClose(Window::s_window, 1);
    }

    /*************************************************************************************
      \brief GLFW error callback.

      \param error       Error code (unused).
      \param description Human-readable description of the error.

      Logs the error string to std::cerr for debugging.
    *************************************************************************************/
    void Window::error_cb(int /*error*/, const char* description) {
        std::cerr << "GLFW error: " << description << std::endl;
    }

} // namespace gfx
