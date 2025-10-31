
#include "[To be deleted] InputManager.h"
#include "Input/Input.h"
#include <GLFW/glfw3.h>
#include <iostream>

namespace Framework
{
	void PlayerInputManager::Update(float dt)
	{
		(void)dt; // Temporary until we use it

		m_moveUp = m_input->IsKeyHeld(GLFW_KEY_W) || m_input->IsKeyHeld(GLFW_KEY_UP);
		m_moveDown = m_input->IsKeyHeld(GLFW_KEY_S) || m_input->IsKeyHeld(GLFW_KEY_DOWN); 
		m_moveLeft = m_input->IsKeyHeld(GLFW_KEY_A) || m_input->IsKeyHeld(GLFW_KEY_LEFT); 
		m_moveRight = m_input->IsKeyHeld(GLFW_KEY_D) || m_input->IsKeyHeld(GLFW_KEY_RIGHT); 

		m_attack = m_input->IsKeyPressed(GLFW_MOUSE_BUTTON_LEFT);

		if (m_moveUp) std::cout << "Move up\n";
		if (m_moveDown) std::cout << "Move down\n";
		if (m_moveLeft) std::cout << "Move left\n";
		if (m_moveRight) std::cout << "Move right\n";
		if (m_attack) std::cout << "Attack\n";
	}
}
