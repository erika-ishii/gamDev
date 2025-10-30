/*********************************************************************************************
 \file      MessageCom.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the MessageId enumeration and the base Message class, which together
            form the foundation of the engine’s messaging system. Messages are lightweight
            objects passed between components and systems to notify events such as quitting
            the application, collisions, or input.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <cstdint>

namespace Framework
{
    /*****************************************************************************************
      \enum MessageId
      \brief Enumerates all supported message/event identifiers within the engine.

      Each identifier represents a different event type that can be dispatched through
      the messaging system and handled by subscribed components or systems.
    *****************************************************************************************/
    enum class MessageId : std::uint16_t {
        None,       ///< No message (default/invalid state)
        Quit,       ///< Request to terminate the application/game
        Collide,    ///< Collision event between two objects
        MouseClick  ///< Mouse click event triggered by input
    };

    /*****************************************************************************************
      \class Message
      \brief Base class representing a generic message that can be sent through the system.

      Messages are created with a MessageId to indicate their type. Specialized messages
      may inherit from this base class to include additional payload data (e.g., collision
      details, mouse coordinates, etc.).
    *****************************************************************************************/
    class Message
    {
    public:
        /*************************************************************************************
          \brief Constructor that initializes the message with the given identifier.
          \param id  The MessageId representing the type of this message.
        **************************************************************************************/
        Message(MessageId id) : MsgId(id) {};

        /*************************************************************************************
          \brief Virtual destructor to allow polymorphic cleanup of derived message types.
        **************************************************************************************/
        virtual ~Message() = default;

        MessageId MsgId; ///< Identifier specifying the type of this message
    };
}
