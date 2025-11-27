/*********************************************************************************************
 \file      AiSystem.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the AiSystem class, which manages AI behavior for enemy entities.

 \details
            AiSystem is responsible for updating enemy AI each frame by evaluating their
            decision trees. It interacts with the Factory to access all game objects,
            and with EnemyDecisionTreeComponent to run per-enemy AI logic.

            The system also provides initialization, optional debug drawing, and shutdown
            functionality.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "AiSystem.h"
#include <iostream>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework 
{
    /*****************************************************************************************
     \brief
        Constructs the AiSystem with a reference to the main window.

     \param window
        Reference to the game's graphics window, used for optional debug rendering.
    *****************************************************************************************/
    AiSystem::AiSystem(gfx::Window& window,
        LogicSystem& logicSystem)
        : window(&window),
        logic(&logicSystem)
    {}

   /*****************************************************************************************
     \brief
        Initializes the AI system.

     \details
        Prepares any required resources or state before AI updates begin. Currently
        logs initialization status to the console.
   *****************************************************************************************/
    void AiSystem::Initialize()
    {
        std::cout << "[AiSystem] Initialized.\n";
    }
    /*****************************************************************************************
     \brief
        Updates all AI-controlled entities.

     \param dt
        Delta time (time elapsed since the last frame), used for time-based updates.

     \details
        Iterates through all game objects retrieved from the Factory. For each object,
        it attempts to run the default enemy decision tree safely using the provided delta time.
    *****************************************************************************************/
void AiSystem::Update(float dt)
{
    for (auto& [id, gocPtr] : FACTORY->Objects())
    {
        if (!gocPtr) continue;
        GOC* goc = gocPtr.get();
        auto* ai = goc->GetComponentType<EnemyDecisionTreeComponent>(
            ComponentTypeId::CT_EnemyDecisionTreeComponent);
        if (!ai) continue;
        // Lazy initialize decision tree
        if (!ai->tree)
        {
            ai->tree = CreateDefaultEnemyTree(goc, logic); // Pass LogicSystem pointer from AiSystem
            std::cout << "[AiSystem] Initialized decision tree for enemy ID: " << id << "\n";
        }
       
        UpdateDefaultEnemyTree(goc, dt, logic);
    }
}

    /*****************************************************************************************
     \brief
        Optional debug drawing for AI visualization.

     \details
        Currently empty. Can be extended to render AI debug information, such as
        decision tree states or enemy paths.
    *****************************************************************************************/
    void AiSystem::draw()
    {
    }
    /*****************************************************************************************
     \brief
        Shuts down the AI system.

     \details
        Cleans up AI-related resources and logs shutdown status to the console.
    *****************************************************************************************/
    void AiSystem::Shutdown()
    {
        std::cout << "[AiSystem] Shutdown.\n";
    }
}