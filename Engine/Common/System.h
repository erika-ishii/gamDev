/*********************************************************************************************
 \file      System.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the ISystem interface which represents a generic engine subsystem.
            All engine systems (e.g., graphics, audio, physics, input) derive from this
            interface and must implement their own Update and GetName functionality.
            Provides lifecycle hooks such as initialization and message handling.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <string>
#include "MessageCom.h"

namespace Framework {
    /*****************************************************************************************
      \class ISystem
      \brief Abstract base class for all engine systems.

      The ISystem interface enforces a standard API for subsystems in the engine,
      including initialization, message handling, updating, and identifying the system
      by name. This design ensures consistent management of subsystems by the core engine.
    *****************************************************************************************/
    class ISystem {
    public:
        /*************************************************************************************
          \brief Virtual destructor to allow safe cleanup of derived systems.
        *************************************************************************************/
        virtual ~ISystem() = default;

        /*************************************************************************************
          \brief Initializes the system.
                 Default implementation does nothing, but may be overridden by derived systems.
        *************************************************************************************/
        virtual void Initialize() {}

        /*************************************************************************************
          \brief Handles incoming messages sent to the system.
          \param m  Pointer to the Message object.
          \note  Default implementation ignores the parameter using `(void)m` to avoid warnings.
        *************************************************************************************/
        virtual void SendMessage(Message* m) { (void)m; } // tell complier i know parameter is unused dont warn me

       /*************************************************************************************
          \brief Pure virtual update function that all derived systems must implement.
          \param dt  Delta time in seconds since the last update call.
        *************************************************************************************/
        virtual void Update(float dt) = 0; // pure virtual function all derived class must implement it

        /*************************************************************************************
         \brief optional Draw
         *************************************************************************************/
        virtual void draw() {};

        /*************************************************************************************
         \brief optional shutdown hook for derived system
        *************************************************************************************/
        virtual void Shutdown() {}

        /*************************************************************************************
          \brief Retrieves the name of the system.
          \return A string representing the system's name.
        *************************************************************************************/
        virtual std::string GetName() = 0;


    };
}