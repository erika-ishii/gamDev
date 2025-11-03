/*********************************************************************************************
 \file      AiSystem.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of the AiSystem class, which manages all AI-related updates for
            enemy entities within the game world.

 \details
            The AiSystem serves as the central controller for enemy artificial intelligence.
            It coordinates with the Factory and component systems to:
            - Initialize and manage AI components.
            - Update enemy behavior each frame using Decision Trees.
            - Handle AI-related rendering or debugging logic.
            - Shutdown and clean up AI-related resources.

            This system is integrated into the core engine loop and is executed alongside
            other systems like Physics, Rendering, and Audio.

 \note
            The AiSystem interacts directly with:
            - EnemyComponent for AI-specific data.
            - EnemyDecisionTreeComponent for decision-making logic.
            - gfx::Window for optional debug visualization or overlays.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Common/System.h"
#include "Factory/Factory.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "../../Engine/Graphics/Window.hpp"
namespace Framework {
    /*****************************************************************************************
    \class AiSystem
    \brief
    Manages enemy AI logic and updates within the game framework.

    \details
    The AiSystem is responsible for updating all AI entities each frame by
    evaluating their decision trees, handling transitions between behaviors
    such as patrol, attack, and flee, and optionally visualizing AI state
    during debugging sessions.
    *****************************************************************************************/
	class AiSystem :public Framework::ISystem {
	public:
		explicit AiSystem(gfx::Window& window);
		void Initialize() override;
		void Update(float dt) override;
		void draw() override;
		void Shutdown() override;
		std::string GetName() override{ return "AiSystem"; }
	private:
		gfx::Window* window;
	};

}