#pragma once
#include "Common/System.h"
#include "Input/Input.h"
#include "../../Engine/Graphics/Window.hpp"
namespace Framework {
	class InputSystem :public Framework::ISystem {
	public:
		explicit InputSystem(gfx::Window& window);

		void Initialize() override;

		void Update(float dt) override;
		

		
		void Shutdown() override;

		std::string GetName() override{ return "InputSystem"; }

		bool IsKeyPressed(int key) const;
		bool IsKeyHeld(int key) const;
		bool IsKeyReleased(int key) const;

		bool IsMousePressed(int button) const;
		bool IsMouseHeld(int button) const;
		bool IsMouseReleased(int button) const;

		bool IsWindowKeyPressed(int key) const;

		InputManager& Manager() { return input; }
		const InputManager& Manager() const { return input; }

		gfx::Window* Window() const { return window; }

	private:
		gfx::Window* window;
		InputManager input;
	};
}