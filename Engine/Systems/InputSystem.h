/*********************************************************************************************
 \file      InputSystem.h
 \par       SofaSpuds
 \author	Ho Jun (h.jun@digipen.edu) - Primary Author, 100%
 \brief     Handles keyboard and mouse input mapping for gameplay actions.
 \details   Wraps the engine InputManager to poll key and mouse states each frame, and
			exposes high-level movement/attack queries for gameplay logic. Operates as a
			core engine subsystem driven by SystemManager (Initialize → Update(dt) → Shutdown).
			Designed to interface with a gfx::Window for platform-specific input events.
 \copyright
			All content ©2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/
#pragma once
#include "Common/System.h"
#include "Input/Input.h"
#include "../../Engine/Graphics/Window.hpp"
namespace Framework 
{

	/*************************************************************************************
	  \class  InputSystem
	  \brief  Polls keyboard and mouse input and exposes game action flags.
	  \note   Movement/attack flags are updated each frame based on input state.
	*************************************************************************************/
	class InputSystem :public Framework::ISystem 
	{
	public:
		/*************************************************************************
		  \brief  Construct the input system using the window for input events.
		*************************************************************************/
		explicit InputSystem(gfx::Window& window);

		/*************************************************************************
		  \brief  Initialize input state tracking (no-op by default).
		*************************************************************************/
		void Initialize() override;

		/*************************************************************************
		  \brief  Poll keyboard/mouse inputs and update action flags.
		  \param  dt  Delta time in seconds (not used but kept for consistency).
		*************************************************************************/
		void Update(float dt) override;
		
		/*************************************************************************
		  \brief  Shutdown input system (no-op, reserved for cleanup).
		*************************************************************************/
		void Shutdown() override;

		/*************************************************************************
		  \brief  Subsystem name for diagnostics/profiling.
		*************************************************************************/
		std::string GetName() override{ return "InputSystem"; }

		/*************************************************************************
		  \brief  Movement and attack queries for game logic convenience.
		*************************************************************************/
		bool MoveUp() const { return m_moveUp; }
		bool MoveDown() const { return m_moveDown; }
		bool MoveLeft() const { return m_moveLeft; }
		bool MoveRight() const { return m_moveRight; }
		bool Attack() const { return m_attack; }

		/*************************************************************************
		  \brief  True during the frame the key transitioned from up → down.
		*************************************************************************/
		bool IsKeyPressed(int key) const;

		/*************************************************************************
		  \brief  True while a key is held down across frames.
		*************************************************************************/
		bool IsKeyHeld(int key) const;

		/*************************************************************************
		  \brief  True during the frame the key transitioned from down → up.
		*************************************************************************/
		bool IsKeyReleased(int key) const;

		// ---------------------------------------------------------------------
		// Mouse Button Queries
		// ---------------------------------------------------------------------
		bool IsMousePressed(int button) const;
		bool IsMouseHeld(int button) const;
		bool IsMouseReleased(int button) const;
		
		/*************************************************************************
		  \brief  Direct access to internal InputManager.
		*************************************************************************/
		InputManager& Manager() { return input; }
		const InputManager& Manager() const { return input; }

		gfx::Window* Window() const { return window; }

	private:
		gfx::Window* window; //!< Window providing platform-specific input context.
		InputManager input;  //!< Manages key/mouse states internally.

		// High-level game action flags (updated each frame)
		bool m_moveUp = false;
		bool m_moveDown = false;
		bool m_moveLeft = false;
		bool m_moveRight = false;
		bool m_attack = false;
	};
}