/*********************************************************************************************
 \file      Component.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the GameComponent base class, the core building block of the
            component system (ECS-like). Each GameComponent represents an independent
            behavior (e.g., Transform, Renderer, Physics) that can be attached to a
            GameObjectComposition. Provides lifecycle hooks, messaging, serialization,
            ownership access, and deep-copy functionality for prefab instancing.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "memory"
#include <cstdint>
#include <string>
#include "Common/ComponentTypeID.h"
#include "Common/MessageCom.h"
#include "Serialization/Serialization.h"



//GameObjectComposition = collection of GameComponents.
//
//Each GameComponent = independent piece of behavior(Transform, Renderer, etc.).
//
//Components communicate via SendMessage and can query siblings via GetOwner()

namespace Framework
{
    class GameObjectComposition;

    /*****************************************************************************************
      \class GameComponent
      \brief Abstract base class for all components in the component based architecture.

      Each component encapsulates one specific behavior (e.g., Transform, Renderer,
      Sprite, Physics). Components are attached to GameObjectComposition instances
      and can communicate with each other via messages or access their owner.

      Responsibilities:
      - Lifecycle management (initialize, destruction).
      - Inter-component communication via SendMessage().
      - Serialization for data-driven initialization.
      - Deep-copy support for prefab instancing.
      - Ownership access (back-pointer to parent GameObjectComposition).
    *****************************************************************************************/
    class GameComponent {
    public:
        /*************************************************************************************
          \brief Virtual destructor to ensure derived components are destroyed safely.
        *************************************************************************************/
        virtual ~GameComponent() = default;

        //Lifecycle
        //called once when component is attached, Derived component override this to set themselves up
        /*************************************************************************************
          \brief Called when the component is attached to its owner.
          \note  Default implementation does nothing. Derived components override this.
        *************************************************************************************/
        virtual void initialize() {}

        //Used for communication between components and system
        /*************************************************************************************
          \brief Handles messages sent to this component.
          \param m  Reference to the Message object.
          \note  Default implementation ignores the message (no-op).
        *************************************************************************************/
        virtual void SendMessage(Message& m) { (void)m; } // optional to override

        // polymorphic deep-copy
        /*************************************************************************************
          \brief Creates a polymorphic deep copy of this component.
          \return A unique_ptr holding the cloned component instance.
          \note  Pure virtual — must be implemented by derived components.
        *************************************************************************************/
        virtual std::unique_ptr<GameComponent> Clone() const = 0;

        //Ownership access
        //Lets component get their owning GameObject( aka composition)
        // Example: PhysicsComponent might call GetOwner()->GetComponent<Transform>() to move it object
        /*************************************************************************************
          \brief Returns a pointer to the owning GameObjectComposition.
          \return A non-const pointer to the owner.
        *************************************************************************************/
        GameObjectComposition* GetOwner() { return owner; }

        //read only
        /*************************************************************************************
          \brief Returns a const pointer to the owning GameObjectComposition.
          \return A const-qualified pointer to the owner.
        *************************************************************************************/
        GameObjectComposition const* GetOwner() const { return owner; }

        //return typeid
        // example :if (comp->GetTypeId() == ComponentTypeId::CT_Transform) { ... }
        /*************************************************************************************
          \brief Returns the type identifier of this component.
          \return ComponentTypeId enum representing the component type.
        *************************************************************************************/
        ComponentTypeId GetTypeId() const { return type_id; }

        ///Component Serialization Interface see Serialization.h for details.
        /*************************************************************************************
          \brief Serializes component data from the provided serializer.
          \param str  Serializer reference.
          \note  Default implementation does nothing; override as needed in derived components.
        *************************************************************************************/
        virtual void Serialize(ISerializer& ) {};

    protected:
        //this are call when you add componenent to the game object
        //you dont want random code changing owner/type so it is protected
        // example to how add T*comp = new T();
        //                      comp->set_owner(this);
        //                      comp_>set_type(ComponentTypeId::CT_Transform);
        /*************************************************************************************
          \brief Sets the owner of this component. Called internally when attaching to a GOC.
          \param goc  Pointer to the owning GameObjectComposition.
        *************************************************************************************/
        void set_owner(GameObjectComposition* goc) { owner = goc; }

        /*************************************************************************************
          \brief Sets the type of this component.
          \param id  ComponentTypeId representing the type.
        *************************************************************************************/
        void set_type(ComponentTypeId id) { type_id = id; }

        friend class GameObjectComposition;

    private:
        //back-pointer to the game object 
        GameObjectComposition* owner = nullptr; ///< Pointer to the owning GameObjectComposition
        //store component type
        ComponentTypeId       type_id = ComponentTypeId::CT_None; ///< Enum identifying the component type
    };
}
