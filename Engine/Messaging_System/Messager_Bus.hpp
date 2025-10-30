/*********************************************************************************************
 \file      MessageBus.hpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration and inline implementation of the MessageBus class. Provides a 
            simple publish-subscribe message system where clients can subscribe callbacks 
            to specific MessageID types and have them invoked when a message is published.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Message.hpp"
#include <functional>
#include <unordered_map>
#include <vector>
/*********************************************************************************************
  \class MessageBus
  \brief Implements a simple publish-subscribe message system.

  The MessageBus allows clients to subscribe callback functions to specific MessageID types.
  When a message is published, all callbacks registered for that message type are executed.
*********************************************************************************************/
class MessageBus
{
    public:
        using Callback = std::function<void()>;/// Type alias for subscriber callback functions
        /*****************************************************************************************
        \brief Subscribe a callback function to a specific MessageID.
        \param id  The MessageID to subscribe to.
        \param cb  The callback function to invoke when the message is published.

        Adds the callback to the internal subscriber map under the given MessageID. 
        Multiple callbacks can be registered for the same MessageID.
        *****************************************************************************************/
        void subscribe(MessageID id, Callback cb){subscribers[id].push_back(cb);}
        /*****************************************************************************************
        \brief Publish a message to all subscribers of its MessageID.
        \param msg  The message to publish.

        Executes all callback functions that were previously subscribed to the message's type.
        If no subscribers exist for the given MessageID, the function does nothing.
        *****************************************************************************************/
        void publish(const Message& msg)
        {  auto it = subscribers.find(msg.type);
            if (it != subscribers.end())
            { for (auto& cb : it->second) cb();}// Call each subscriber callback
        }
    private:
        std::unordered_map<MessageID, std::vector<Callback>> subscribers;/// Map of subscribers
};