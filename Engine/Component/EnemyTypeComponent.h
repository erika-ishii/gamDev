/*********************************************************************************************
 \file      EnemyTypeComponent.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu)- Primary Author, 100%

 \brief     Declares the EnemyTypeComponent class, which defines the behavioral classification
            of an enemy entity (e.g., physical or ranged). This component allows game systems
            such as AI and combat to adjust logic depending on enemy type.

 \details
            The EnemyTypeComponent serves as a lightweight identifier that determines how
            an enemy behaves or interacts within the game world. It is primarily used by
            combat, AI, or spawning systems to differentiate enemies and trigger type-specific
            logic.

            Responsibilities:
            - Define the enemy's classification (physical or ranged).
            - Provide serialization support for prefab and level data.
            - Support deep-copy functionality for prefab instancing or runtime duplication.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"
#include <string>

namespace Framework
{
    /*****************************************************************************************
      \class EnemyTypeComponent
      \brief Component that stores the type of an enemy (physical or ranged).

      This component acts as a data container to distinguish between enemy behavior types.
      Systems like AI, combat, or attack logic can query this to determine how the enemy
      should act or what attacks it can perform.
    *****************************************************************************************/
    class EnemyTypeComponent : public GameComponent
    {
    public:
        /*************************************************************************************
          \enum EnemyType
          \brief Defines the type of enemy.
        *************************************************************************************/
        enum class EnemyType { physical, ranged };

        EnemyType Etype{ EnemyType::physical };  ///< Current type of the enemy.

        /*************************************************************************************
          \brief Default constructor.
          \details Initializes the enemy type to physical by default.
        *************************************************************************************/
        EnemyTypeComponent() = default;

        /*************************************************************************************
          \brief Parameterized constructor.
          \param t Type to assign to the enemy.
        *************************************************************************************/
        EnemyTypeComponent(EnemyType t) : Etype(t) {}

        /*************************************************************************************
          \brief Initializes the component.
          \note Currently does nothing; included for interface consistency.
        *************************************************************************************/
        void initialize() override {}

        /*************************************************************************************
          \brief Handles messages sent to this component.
          \param m Reference to a message object.
          \note Currently unused; included for interface consistency.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes the enemy type from a given serializer.
          \param s Reference to the serializer.
          \details Reads the "type" key. If the value is "ranged" (case-insensitive), sets
                   Etype to ranged; otherwise defaults to physical.
        *************************************************************************************/
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("type"))
            {
                std::string typeStr;
                StreamRead(s, "type", typeStr);
                if (typeStr == "ranged" || typeStr == "Ranged") { Etype = EnemyType::ranged; }
                else { Etype = EnemyType::physical; }
            }
        }

        /*************************************************************************************
          \brief Creates a deep copy of this component.
          \return A unique_ptr holding the cloned EnemyTypeComponent instance.
          \details Copies the current EnemyType value to the new instance.
        *************************************************************************************/
        ComponentHandle Clone() const override
        {
            auto copy = ComponentPool<EnemyTypeComponent>::CreateTyped();
            copy->Etype = Etype;
            return copy;
        }
    };
}
