/*********************************************************************************************
 \file      PlayerComponent.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu)- Primary Author, 100%

 \brief     Declares the PlayerComponent class, a simple data container used to identify
            player-controlled entities within the game. It currently functions as a tag
            component without behavior or data, serving as a marker for input and gameplay
            systems.

 \details
            The PlayerComponent marks a GameObjectComposition as player-controlled. While
            it currently has no logic or serialized data, it can be extended in the future
            to include player-specific attributes such as ID, input bindings, or statistics.

            Responsibilities:
            - Acts as a tag for player entities.
            - Enables systems (e.g., input, camera, UI) to locate the player object.
            - Supports cloning for prefab or respawn logic.
            - Logs initialization messages for debugging.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include <iostream>

namespace Framework
{
    /*****************************************************************************************
      \class PlayerComponent
      \brief Tag component identifying an entity as a player-controlled object.

      Currently a minimal component used to mark GameObjects as player entities. It can
      be extended with player-specific state or metadata in future implementations.
    *****************************************************************************************/
    class PlayerComponent : public GameComponent
    {
    public:
        /*************************************************************************************
          \brief Called when the component is initialized.
          \details Prints a debug message confirming this GameObject has a PlayerComponent.
        *************************************************************************************/
        void initialize() override { std::cout << "This object has a PlayerComponent!\n"; }

        /*************************************************************************************
          \brief Handles incoming messages sent to this component.
          \param m  Reference to the message.
          \note  Currently unused; placeholder for future logic.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes the component�s data.
          \param s  Reference to the serializer.
          \note  No data is serialized at present.
        *************************************************************************************/
        void Serialize(ISerializer& s) override { (void)s; }

        /*************************************************************************************
          \brief Creates a deep copy of this component.
          \return A unique_ptr holding a cloned PlayerComponent instance.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override
        {
            return std::make_unique<PlayerComponent>();
        }
    };
}
