/*********************************************************************************************
 \file      Input.cpp
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the inputs. Captures and stores the position of the mouse,
			along with multiple states of a key, be it mouse or keyboard.

 \copyright
			All content © 2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/
#include "Input.h"
#include <Windows.h>
#include <GLFW/glfw3.h>
#include <iostream>


namespace Framework
{
	/*****************************************************************************************
	\brief Constructor for the InputManager calss. This is to ensure that no
		   key gets stucked in the "pressed" state and no mouse button is considered
		   down
	*****************************************************************************************/
	InputManager::InputManager(GLFWwindow* window) : m_window(window)
	{
		// Initialize keys of interest
		int keys[] = {
			GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
			GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
			GLFW_KEY_SPACE
		};

		for (int key : keys)
		{
			m_keyHeld[key] = false;
		}

		// Initialize the mouse buttons
		int buttons[] = { GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT };
		for (int btn : buttons)
		{
			m_mouseHeld[btn] = false;
		}
	}

	/*****************************************************************************************
	\brief Updates the inputs and ensures that any key that isn't held is cleared. Also
		   updates the position of the mouse cursor.
	*****************************************************************************************/
	void InputManager::Update()
	{
		m_keyPressed.clear();
		m_keyReleased.clear();
		m_mousePressed.clear();
		m_mouseReleased.clear();

		// Keyboard
		for (auto& [key, held] : m_keyHeld)
		{
			int state = glfwGetKey(m_window, key);
			if (state == GLFW_PRESS)
			{
				if (!held)
					m_keyPressed[key] = true;
				held = true;
			}
			else
			{
				if (held)
					m_keyReleased[key] = true;
				held = false;
			}
		}

		// Mouse buttons
		for (auto& [btn, held] : m_mouseHeld)
		{
			int state = glfwGetMouseButton(m_window, btn);
			if (state == GLFW_PRESS)
			{
				if (!held)
					m_mousePressed[btn] = true;
				held = true;
			}
			else
			{
				if (held)
					m_mouseReleased[btn] = true;
				held = false;
			}
		}

		// Mouse position
		glfwGetCursorPos(m_window, &m_mouseState.x, &m_mouseState.y);
		m_mouseState.leftClick = m_mouseHeld[GLFW_MOUSE_BUTTON_LEFT];
		m_mouseState.rightClick = m_mouseHeld[GLFW_MOUSE_BUTTON_RIGHT];
	}

	/*****************************************************************************************
	\brief A boolean to check if the key has been pressed
	\return True if it's pressed, false if it isn't
	*****************************************************************************************/
	bool InputManager::IsKeyPressed(int key) const
	{
		auto it = m_keyPressed.find(key);
		return it != m_keyPressed.end() && it->second;
	}

	/*****************************************************************************************
	\brief A boolean to check if the key is being held
	\return True if it's being held, false if it isn't
	*****************************************************************************************/
	bool InputManager::IsKeyHeld(int key) const
	{
		auto it = m_keyHeld.find(key);
		return it != m_keyHeld.end() && it->second;
	}

	/*****************************************************************************************
	\brief A boolean to check if the key has been released
	\return True if it's released, false if it isn't
	*****************************************************************************************/
	bool InputManager::IsKeyReleased(int key) const
	{
		auto it = m_keyReleased.find(key);
		return it != m_keyReleased.end() && it->second;
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
		auto it = m_mousePressed.find(button);
		return it != m_mousePressed.end() && it->second;
	}

	/*****************************************************************************************
	\brief A boolean to check if the mouse button is being held
	\return True if it's being held, false if it isn't
	*****************************************************************************************/
	bool InputManager::IsMouseHeld(int button) const
	{
		auto it = m_mouseHeld.find(button);
		return it != m_mouseHeld.end() && it->second;
	}

	/*****************************************************************************************
	\brief A boolean to check if the mouse button has been released
	\return True if it's released, false if it isn't
	*****************************************************************************************/
	bool InputManager::IsMouseReleased(int button) const
	{
		auto it = m_mouseReleased.find(button);
		return it != m_mouseReleased.end() && it->second;
	}
}
