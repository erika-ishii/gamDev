#pragma once
#include "Input/Input.h"
#include "Common/System.h"
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <string>

namespace Framework
{
	class PlayerInputManager : public ISystem
	{
	public:
		PlayerInputManager(InputManager* input)
			: m_input(input) {}

		void Update(float dt) override;

		// Movement queries
		bool MoveUp() const { return m_moveUp; }
		bool MoveDown() const { return m_moveDown; }
		bool MoveLeft() const { return m_moveLeft; }
		bool MoveRight() const { return m_moveRight; }
		bool Attack() const { return m_attack; }

	private:
		Framework::InputManager* m_input;
		bool m_moveUp = false;
		bool m_moveDown = false;
		bool m_moveLeft = false;
		bool m_moveRight = false;
		bool m_attack = false;
	};
}
