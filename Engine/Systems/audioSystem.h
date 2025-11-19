/*********************************************************************************************
 \file      AudioSystem.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of the AudioSystem class, which manages all audio-related updates
            and interfaces within the game framework.

 \details
            AudioSystem is responsible for integrating audio playback, managing audio
            components, and optionally providing a debug GUI through AudioImGui. The system
            allows developers to:
            - Initialize and shutdown audio-related resources.
            - Update audio logic each frame.
            - Render optional debug visualizations for audio assets.

            The system integrates with gfx::Window and Messaging Bus for context and event
            handling.

 \note
            This system relies on AudioImGui for runtime debugging of sound assets and
            uses the framework's messaging system for communication with other systems.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "../../Engine/Graphics/Window.hpp"
#include "Input/Input.h"
#include "Systems/InputSystem.h"
#include "Common/System.h"
#include "Messaging_System/Messager_Bus.hpp"

#include <array>
#include <memory>
#include "Debug/AudioImGui.h"

namespace Framework {
	class AudioImGui;
    /*****************************************************************************************
     \class AudioSystem
     \brief
        Manages all audio playback and debugging interfaces in the game framework.

     \details
        AudioSystem handles initialization, per-frame updates, rendering of debug GUI,
        and cleanup of audio resources. It works with AudioImGui to provide developers
        with controls to visualize and manipulate sounds during runtime.
    *****************************************************************************************/
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