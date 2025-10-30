/*********************************************************************************************
 \file      Input.h
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the inputs. Captures and stores the position of the mouse,
			along with multiple states of a key, be it mouse or keyboard.

 \copyright
			All content © 2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/
#pragma once

#include <unordered_map>
#include "Composition/Component.h"
struct GLFWwindow;

namespace Framework
{
	/*****************************************************************************************
	  \brief The state of the mouse that can be used if needed.
	*****************************************************************************************/
	struct MouseState
	{
		double x = 0;
		double y = 0;
		bool leftClick = false;
		bool rightClick = false;
	};

	/*****************************************************************************************
	  \brief The Input Manager handles the input and also provides means to see 
	         if the key is pressed, held or released.
	*****************************************************************************************/
	class InputManager
	{
	public:
		InputManager(GLFWwindow* window);
		~InputManager() = default;

		void Update();

		// Keyboard queries
		bool IsKeyPressed(int key) const;
		bool IsKeyHeld(int key) const;
		bool IsKeyReleased(int key) const;

		// Mouse Queries
		MouseState GetMouseState() const;
		bool IsMousePressed(int button) const;
		bool IsMouseHeld(int button) const;
		bool IsMouseReleased(int button) const;

	private:
		GLFWwindow* m_window;

		std::unordered_map<int, bool> m_keyHeld;
		std::unordered_map<int, bool> m_keyPressed;
		std::unordered_map<int, bool> m_keyReleased;

		std::unordered_map<int, bool> m_mouseHeld;
		std::unordered_map<int, bool> m_mousePressed;
		std::unordered_map<int, bool> m_mouseReleased;

		MouseState m_mouseState;
	};
}
