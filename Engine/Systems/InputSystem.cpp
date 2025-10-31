// This one is to be removed or replaced
#include "InputSystem.h"
#include <Windows.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace Framework {

	InputSystem::InputSystem(gfx::Window& window)
		: window(&window), input(window.raw()) {}
	void InputSystem::Initialize()
	{
		if (window) {
			input = Framework::InputManager(window->raw());
		}
	}

	void InputSystem::Update(float dt) {
		(void)dt;
		if (!window)
			return;
		
		input.Update();

		if (input.IsKeyPressed(GLFW_KEY_SPACE))  std::cout << "Spacebar pressed!" << std::endl;
		if (input.IsKeyHeld(GLFW_KEY_SPACE))     std::cout << "Spacebar held!" << std::endl;
		if (input.IsKeyReleased(GLFW_KEY_SPACE)) std::cout << "Spacebar released!" << std::endl;

		if (input.IsMousePressed(GLFW_MOUSE_BUTTON_LEFT))  std::cout << "LMB pressed!" << std::endl;
		if (input.IsMouseHeld(GLFW_MOUSE_BUTTON_LEFT))     std::cout << "LMB held!" << std::endl;
		if (input.IsMouseReleased(GLFW_MOUSE_BUTTON_LEFT)) std::cout << "LMB released!" << std::endl;
	}

	void InputSystem::Shutdown()
	{
		window = nullptr;
	}
	bool InputSystem::IsKeyPressed(int key)const 
	{
		return input.IsKeyPressed(key);
	}

	bool InputSystem::IsKeyHeld(int key) const 
	{
		return input.IsKeyHeld(key);
	}

	bool InputSystem::IsKeyReleased(int key)const
	{
		return input.IsKeyReleased(key);
	}

	bool InputSystem::IsMouseHeld(int button)const 
	{
		return input.IsMouseHeld(button);
	}

	bool InputSystem::IsMousePressed(int button)const 
	{
		return input.IsMousePressed(button);
	}

	bool InputSystem::IsMouseReleased(int button)const 
	{
		return input.IsMouseReleased(button);
	}

} //namespace framework
