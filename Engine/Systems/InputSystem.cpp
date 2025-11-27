/*********************************************************************************************
 \file      InputSystem.cpp
 \par       SofaSpuds
 \author	Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief     Polls keyboard and mouse states and updates gameplay-relevant input flags.
 \details   Wraps the platform-level input from gfx::Window and InputManager. Each frame,
			InputSystem updates its internal action states (movement + attack) and provides
			convenience queries for both high-level actions and raw key/mouse state checks.
			Designed to run once per frame via SystemManager Update(dt).
			Debug printing is available but commented out by default.
 ©2025 DigiPen Institute of Technology Singapore. All rights reserved.
*********************************************************************************************/

#include "InputSystem.h"
#if defined(APIENTRY)
#  undef APIENTRY
#endif
#include <Windows.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework 
{
	/*************************************************************************************
	  \brief  Construct the input system and bind it to a rendering window.
	  \param  window  Reference to a gfx::Window that provides raw input handles.
	*************************************************************************************/
	InputSystem::InputSystem(gfx::Window& window)
		: window(&window), input(window.raw()) {}

	/*************************************************************************************
	  \brief  Initialize the input manager using the window's raw handle.
	  \note   Re-initializes InputManager in case the window/context changed.
	*************************************************************************************/
	void InputSystem::Initialize()
	{
		if (window) {
			input = Framework::InputManager(window->raw());
		}
	}

	/*************************************************************************************
	  \brief  Poll mouse/keyboard states and update action flags.
	  \param  dt  Delta time (unused; included for consistency).
	  \note   Developer debugging examples are included but commented out.
	*************************************************************************************/
	void InputSystem::Update(float dt) {
		(void)dt;
		if (!window)
			return;
		
		input.Update();

		// For debugging
		/*
		if (input.IsKeyPressed(GLFW_KEY_W))  std::cout << "W Key pressed!" << std::endl;
		if (input.IsKeyHeld(GLFW_KEY_W))     std::cout << "W Key held!" << std::endl;
		if (input.IsKeyReleased(GLFW_KEY_W)) std::cout << "W Key released!" << std::endl;

		if (input.IsKeyPressed(GLFW_KEY_A))  std::cout << "A Key pressed!" << std::endl;
		if (input.IsKeyHeld(GLFW_KEY_A))     std::cout << "A Key held!" << std::endl;
		if (input.IsKeyReleased(GLFW_KEY_A)) std::cout << "A Key released!" << std::endl;

		if (input.IsKeyPressed(GLFW_KEY_S))  std::cout << "S Key pressed!" << std::endl;
		if (input.IsKeyHeld(GLFW_KEY_S))     std::cout << "S Key held!" << std::endl;
		if (input.IsKeyReleased(GLFW_KEY_S)) std::cout << "S Key released!" << std::endl;

		if (input.IsKeyPressed(GLFW_KEY_D))  std::cout << "D Key pressed!" << std::endl;
		if (input.IsKeyHeld(GLFW_KEY_D))     std::cout << "D Key held!" << std::endl;
		if (input.IsKeyReleased(GLFW_KEY_D)) std::cout << "D Key released!" << std::endl;

		if (input.IsMousePressed(GLFW_MOUSE_BUTTON_LEFT))  std::cout << "LMB pressed!" << std::endl;
		if (input.IsMouseHeld(GLFW_MOUSE_BUTTON_LEFT))     std::cout << "LMB held!" << std::endl;
		if (input.IsMouseReleased(GLFW_MOUSE_BUTTON_LEFT)) std::cout << "LMB released!" << std::endl;
		*/
		
	}

	/*************************************************************************************
	  \brief  Shutdown input system by clearing window reference.
	  \note   Does not destroy input state; simply prevents further polling.
	*************************************************************************************/
	void InputSystem::Shutdown()
	{
		window = nullptr;
	}

	// -------------------------------------------------------------------------
	// Key State Queries (Thin wrappers around InputManager)
	// -------------------------------------------------------------------------
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

	// -------------------------------------------------------------------------
	// Mouse Button Queries
	// -------------------------------------------------------------------------
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
