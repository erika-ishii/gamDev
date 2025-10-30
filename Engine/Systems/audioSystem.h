#pragma once
#include "../../Engine/Graphics/Window.hpp"
#include "Common/System.h"
#include "Messaging_System/Messager_Bus.hpp"

#include <array>
#include <memory>
#include "Debug/AudioImGui.h"

namespace Framework {
	class AudioImGui;

	class AudioSystem :public Framework::ISystem {
	public:
		explicit AudioSystem(gfx::Window& window);

		void Initialize() override;

		void Update(float dt) override;

		void draw();

		void Shutdown() override;

		std::string GetName() override{ return "AudioSystem"; }



	private:
		gfx::Window* window;

	};
}