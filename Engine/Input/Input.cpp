/*********************************************************************************************
 \file      Input.cpp
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the inputs. Captures and stores the position of the mouse,
            along with multiple states of a key, be it mouse or keyboard.

 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Input.h"
#if defined(APIENTRY)
#  undef APIENTRY
#endif
#include <Windows.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>

namespace Framework
{
    /*****************************************************************************************
        \brief Constructor for the InputManager class. This is to ensure that no
               key gets stuck in the "pressed" state and no mouse button is considered
               down.
    *****************************************************************************************/
    InputManager::InputManager(GLFWwindow* window) : m_window(window),
        m_keyHeld(GLFW_KEY_LAST + 1, false),
        m_keyPressed(GLFW_KEY_LAST + 1, false),
        m_keyReleased(GLFW_KEY_LAST + 1, false),
        m_mouseHeld(GLFW_MOUSE_BUTTON_LAST + 1, false),
        m_mousePressed(GLFW_MOUSE_BUTTON_LAST + 1, false),
        m_mouseReleased(GLFW_MOUSE_BUTTON_LAST + 1, false)
    {
    }

    /*****************************************************************************************
    \brief Updates the inputs and ensures that any key that isn't held is cleared. Also
           updates the position of the mouse cursor.
    *****************************************************************************************/
    void InputManager::Update()
    {
        std::fill(m_keyPressed.begin(), m_keyPressed.end(), false);
        std::fill(m_keyReleased.begin(), m_keyReleased.end(), false);
        std::fill(m_mousePressed.begin(), m_mousePressed.end(), false);
        std::fill(m_mouseReleased.begin(), m_mouseReleased.end(), false);

        if (!m_window)
            return;

        // Keyboard
        for (int key = 0; key <= GLFW_KEY_LAST; ++key)
        {
            if (key < GLFW_KEY_SPACE) {
                m_keyHeld[key] = false;
                continue;
            }

            int state = glfwGetKey(m_window, key);
            bool wasHeld = m_keyHeld[key];
            bool isHeld = (state == GLFW_PRESS) || (state == GLFW_REPEAT);

            if (isHeld) {
                if (!wasHeld) m_keyPressed[key] = true;
            }
            else if (wasHeld) {
                m_keyReleased[key] = true;
            }

            m_keyHeld[key] = isHeld;
        }

        // Mouse buttons
        for (int btn = 0; btn <= GLFW_MOUSE_BUTTON_LAST; ++btn)
        {
            int state = glfwGetMouseButton(m_window, btn);
            bool wasHeld = m_mouseHeld[btn];
            bool isHeld = (state == GLFW_PRESS);

            if (isHeld)
            {
                if (!wasHeld)
                    m_mousePressed[btn] = true;
            }
            else if (wasHeld)
            {
                m_mouseReleased[btn] = true;
            }

            m_mouseHeld[btn] = isHeld;
        }

        // Mouse position
        glfwGetCursorPos(m_window, &m_mouseState.x, &m_mouseState.y);

        // Validate mouse button indices at compile time
        static_assert(GLFW_MOUSE_BUTTON_LEFT >= 0 && GLFW_MOUSE_BUTTON_LEFT <= GLFW_MOUSE_BUTTON_LAST,
            "GLFW_MOUSE_BUTTON_LEFT out of range");
        static_assert(GLFW_MOUSE_BUTTON_RIGHT >= 0 && GLFW_MOUSE_BUTTON_RIGHT <= GLFW_MOUSE_BUTTON_LAST,
            "GLFW_MOUSE_BUTTON_RIGHT out of range");

        m_mouseState.leftClick = m_mouseHeld[GLFW_MOUSE_BUTTON_LEFT];
        m_mouseState.rightClick = m_mouseHeld[GLFW_MOUSE_BUTTON_RIGHT];
    }
    /*****************************************************************************************
    \brief A boolean to check if the key has been pressed
    \return True if it's pressed, false if it isn't
    *****************************************************************************************/
    bool InputManager::IsKeyPressed(int key) const
    {
        if (key < 0 || key >= static_cast<int>(m_keyPressed.size()))
            return false;
        return m_keyPressed[key];
    }
   /*****************************************************************************************
   \brief A boolean to check if the key is being held
   \return True if it's being held, false if it isn't
   *****************************************************************************************/
    bool InputManager::IsKeyHeld(int key) const
    {
        if (key < 0 || key >= static_cast<int>(m_keyHeld.size()))
            return false;
        return m_keyHeld[key];
    }
    /*****************************************************************************************
    \brief A boolean to check if the key has been released
    \return True if it's released, false if it isn't
    *****************************************************************************************/
    bool InputManager::IsKeyReleased(int key) const
    {
        if (key < 0 || key >= static_cast<int>(m_keyReleased.size()))
            return false;
        return m_keyReleased[key];
    }

    /*****************************************************************************************
    \brief Returns the state of the mouse, which includes x pos, y pos, left click and right click
    \return Mouse state
    *****************************************************************************************/

    MouseState InputManager::GetMouseState() const
    {
        return m_mouseState;
    }
    /*****************************************************************************************
    \brief A boolean to check if the mouse button has been pressed
    \return True if it's pressed, false if it isn't
    *****************************************************************************************/
    bool InputManager::IsMousePressed(int button) const
    {
        if (button < 0 || button >= static_cast<int>(m_mousePressed.size()))
            return false;
        return m_mousePressed[button];
    }
    /*****************************************************************************************
    \brief A boolean to check if the mouse button is being held
    \return True if it's being held, false if it isn't
    *****************************************************************************************/
    bool InputManager::IsMouseHeld(int button) const
    {
        if (button < 0 || button >= static_cast<int>(m_mouseHeld.size()))
            return false;
        return m_mouseHeld[button];
    }
    /*****************************************************************************************
    \brief A boolean to check if the mouse button has been released
    \return True if it's released, false if it isn't
    *****************************************************************************************/
    bool InputManager::IsMouseReleased(int button) const
    {
        if (button < 0 || button >= static_cast<int>(m_mouseReleased.size()))
            return false;
        return m_mouseReleased[button];
    }
}
