/*********************************************************************************************
 \file      Window.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief    setup window

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Graphics/Window.hpp"

// Keep GL/GLFW only in the .cpp to avoid polluting headers.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>

namespace {
    
    // Prefer constexpr over macros (resolves your VCR101 suggestion)
    constexpr int kGlMajor = 4;
    constexpr int kGlMinor = 5;

} // anonymous namespace

// Define the static window pointer
GLFWwindow* gfx::Window::s_window = nullptr;

namespace gfx {

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

        // Non-resizable for now (you can change this to GLFW_TRUE if desired)
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;

        if (m_fullscreen && monitor && mode)
        {
            m_width = mode->width;
            m_height = mode->height;
            s_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), monitor, nullptr);

            // Center the future windowed position if we later toggle back.
            m_windowedX = (mode->width - m_windowedWidth) / 2;
            m_windowedY = (mode->height - m_windowedHeight) / 2;
        }
        else
        {
            m_fullscreen = false;
            s_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
            glfwGetWindowPos(s_window, &m_windowedX, &m_windowedY);
        }

        if (!s_window) {
            std::cerr << "Failed to create GLFW window.\n";
            glfwTerminate();
            throw std::runtime_error("GLFW window creation failed");
        }

        // Make the context current
        glfwMakeContextCurrent(s_window);

        // Load GL function pointers with GLAD
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwDestroyWindow(s_window);
            s_window = nullptr;
            glfwTerminate();
            throw std::runtime_error("Failed to initialize GLAD");
        }
        const GLubyte* renderer = glGetString(GL_RENDERER);
        const GLubyte* version = glGetString(GL_VERSION);

        std::cout << "Renderer: " << renderer << "\n";
        std::cout << "OpenGL version supported: " << version << "\n";
        // Set viewport and vsync
        glViewport(0, 0, m_width, m_height);
        glfwSwapInterval(1); // vsync on
    }

    Window::~Window() {
        if (s_window) {
            glfwDestroyWindow(s_window);
            s_window = nullptr;
        }
        glfwTerminate();
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(s_window) != 0;
    }

    void Window::pollEvents() {
        glfwPollEvents();

        
        if (glfwGetKey(s_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(s_window, GLFW_TRUE);
        }
    }

    void Window::beginFrame() {
        // Clear the color buffer (set your preferred clear color)
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void Window::endFrame() {
       
    }

    void Window::swapBuffers() {
        glfwSwapBuffers(s_window);
    }
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

        // Keep GL viewport in sync
        glViewport(0, 0, m_width, m_height);
    }

    void Window::run() {
        while (!glfwWindowShouldClose(Window::s_window)) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glfwSwapBuffers(Window::s_window);
            glfwPollEvents();
            if (glfwGetKey(Window::s_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(Window::s_window, 1);
            }
        }
    }


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

            if (glfwGetKey(Window::s_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(Window::s_window, 1);
            }
        }
    }


    bool Window::isKeyPressed(int key) const {
        return glfwGetKey(Window::s_window, key) == GLFW_PRESS;
    }

    bool Window::isOpen() const {
        return !glfwWindowShouldClose(Window::s_window);
    }

    void Window::close() {
        glfwSetWindowShouldClose(Window::s_window, 1);
    }

    void Window::error_cb(int /*error*/, const char* description) {
        std::cerr << "GLFW error: " << description << std::endl;
    }

} // namespace gfx
