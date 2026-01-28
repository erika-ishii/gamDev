/*********************************************************************************************
 \file      EnemyComponent.h
 \par       SofaSpuds
 \author     jianwei.c (jianwei.c@digipen.edu)- Primary Author, 100%

 \brief     Declares the EnemyComponent class, a simple data-holder component used to
            mark or store enemy-related information within the game object composition.
            It currently serves as a tag component without active behavior or logic.

 \details
            The EnemyComponent acts as a lightweight data container and identifier for
            enemy entities in the engine. It can later be extended with attributes such
            as enemy type, health, AI parameters, or faction alignment. For now, it
            primarily functions as a marker for systems (e.g., AI or combat) to recognize
            enemy objects.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"

namespace Framework
{
    /*****************************************************************************************
      \class EnemyComponent
      \brief A lightweight tag component representing an enemy entity in the game world.

      This component currently holds no data or behavior and is primarily used by systems
      to identify objects as enemies. It can be extended in the future with properties
      like health, attack stats, or AI state.
    *****************************************************************************************/
    class EnemyComponent : public GameComponent
    {
    public:
        /*************************************************************************************
          \brief Initializes the component. No behavior for now.
        *************************************************************************************/
        void initialize() override {}

        /*************************************************************************************
          \brief Handles messages sent to this component.
          \param m  Reference to the message being sent.
          \note  Currently does nothing as this component has no active logic.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes this component data.
          \param s  Reference to the serializer.
          \note  No data to serialize at present.
        *************************************************************************************/
        void Serialize(ISerializer& s) override { (void)s; }

        /*************************************************************************************
          \brief Creates a deep copy of this component.
          \return A unique_ptr holding the cloned EnemyComponent instance.
        *************************************************************************************/
        ComponentHandle Clone() const override
        {
            return ComponentPool<EnemyComponent>::Create();
        }
    };
}
