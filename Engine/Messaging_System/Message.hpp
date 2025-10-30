/*********************************************************************************************
 \file      Message.hpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of a simple Message system using a plain enum MessageID.
            The MessageID enum represents different key or message types, and the 
            Message class stores a single MessageID. This design is suitable for 
            small projects or simple input/event systems.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
/*********************************************************************************************
  \enum MessageID
  \brief Enumerates different message types or keys used in the system.
*********************************************************************************************/
enum MessageID
{
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_M,
    KEY_S
};
/*********************************************************************************************
  \class Message
  \brief Represents a message with a specific MessageID.

  The class stores a single MessageID type. It is intended for simple message passing
  or input handling.
*********************************************************************************************/
class Message
{
    public:
    MessageID type; ///Type of the message
    Message(MessageID t) : type(t) {}///Constructor that sets the message type
};