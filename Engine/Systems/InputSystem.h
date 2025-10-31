// This one is to be removed or replaced
#pragma once
#include "Common/System.h"
#include "Input/Input.h"
#include "../../Engine/Graphics/Window.hpp"
namespace Framework {
	class InputSystem :public Framework::ISystem 
	{
	public:
		explicit InputSystem(gfx::Window& window);

		void Initialize() override;

		void Update(float dt) override;
		
		void Shutdown() override;

		std::string GetName() override{ return "InputSystem"; }

		// Movement queries
		bool MoveUp() const { return m_moveUp; }
		bool MoveDown() const { return m_moveDown; }
		bool MoveLeft() const { return m_moveLeft; }
		bool MoveRight() const { return m_moveRight; }
		bool Attack() const { return m_attack; }

		bool IsKeyPressed(int key) const;
		bool IsKeyHeld(int key) const;
		bool IsKeyReleased(int key) const;

		bool IsMousePressed(int button) const;
		bool IsMouseHeld(int button) const;
		bool IsMouseReleased(int button) const;
		

		InputManager& Manager() { return input; }
		const InputManager& Manager() const { return input; }

		gfx::Window* Window() const { return window; }

	private:
		gfx::Window* window;
		InputManager input;
		bool m_moveUp = false;
		bool m_moveDown = false;
		bool m_moveLeft = false;
		bool m_moveRight = false;
		bool m_attack = false;
	};
}