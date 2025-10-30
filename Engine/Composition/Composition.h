/*********************************************************************************************
 \file      Composition.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the GameObjectComposition class, which acts as the container (entity)
            for a collection of components in the ECS system. Provides methods to add,
            query, clone, and manage components, as well as lifecycle management
            and integration with the factory system.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <vector>
#include <memory>
#include "Component.h"
#include "Common/MessageCom.h"
#include <string>

namespace Framework {
    using GOCId = unsigned int; //Alias for a game ID

    /*****************************************************************************************
      \class GameObjectComposition
      \brief Represents an entity in the ECS system.

      A GameObjectComposition (GOC) is a collection of components that together define
      an object’s behavior and data. Provides functions for:
        - Naming and identification
        - Adding and retrieving components
        - Broadcasting messages to components
        - Cloning entities and their components
        - Lifecycle management (initialize/destroy)

      GOCs are created and managed by the GameObjectFactory, which ensures unique IDs and
      safe destruction.
    *****************************************************************************************/
    class GameObjectComposition {   //Entity/"composition" that own component
    public:
        friend class GameObjectFactory; //Grant factory access

        //Set and get name
        /*************************************************************************************
          \brief Sets the name of this game object.
          \param name  String name to assign.
        *************************************************************************************/
        void SetObjectName(const std::string& name) { ObjectName = name; }

        /*************************************************************************************
          \brief Retrieves the name of this game object.
          \return A const reference to the object name string.
        *************************************************************************************/
        const std::string& GetObjectName() const { return ObjectName; }

        // Broadcast a message to all component
        /*************************************************************************************
          \brief Broadcasts a message to all attached components.
          \param message Reference to the message being sent.
        *************************************************************************************/
        void SendMessage(Message& message);

        // Get first component matching type id (nullptr if none)
        /*************************************************************************************
          \brief Retrieves the first component with the given type ID.
          \param typeId  The ComponentTypeId to search for.
          \return Pointer to the component if found, nullptr otherwise.
        *************************************************************************************/
        GameComponent* GetComponent(ComponentTypeId typeId);

        //read only overload preserve when GOC itself is const
        /*************************************************************************************
          \brief Const-qualified overload of GetComponent.
          \param typeId  The ComponentTypeId to search for.
          \return Const pointer to the component if found, nullptr otherwise.
        *************************************************************************************/
        GameComponent const* GetComponent(ComponentTypeId typeId) const;

        //Clone GameObject
        /*************************************************************************************
          \brief Creates a deep clone of this game object and all its components.
          \return Pointer to the newly cloned GameObjectComposition.
        *************************************************************************************/
        GameObjectComposition* Clone() const;

        //Find the first component with the give type ID and return it
        /*************************************************************************************
          \brief Retrieves a component casted to a specific type.
          \tparam T       The expected component type.
          \param typeId   The ComponentTypeId to search for.
          \return Pointer to the component as type T, nullptr if not found.
        *************************************************************************************/
        template <typename T>
        T* GetComponentAs(ComponentTypeId typeId) {
            return static_cast<T*>(GetComponent(typeId));
        }

        template<typename T>
        T const* GetComponentAs(ComponentTypeId typeId) const {
            return static_cast<T const*>(GetComponent(typeId));
        }

        ///Type safe way of accessing components.
        /*************************************************************************************
          \brief Type-safe way of retrieving components using templated access.
          \tparam T       The expected component type.
          \param typeId   The ComponentTypeId to search for.
          \return Pointer to the component as type T, nullptr if not found.
        *************************************************************************************/
        template<typename T>
        T* GetComponentType(ComponentTypeId typeId);

        // const overload
        template <typename T>
        T const* GetComponentType(ComponentTypeId typeId) const;

        //Lifecycle
        /*************************************************************************************
          \brief Initializes all components attached to this GOC.
        *************************************************************************************/
        void initialize(); //call initialize() on all component

        /*************************************************************************************
          \brief Marks this GOC for destruction. Actual deletion is handled by the factory.
        *************************************************************************************/
        void Destroy(); // mark for removal but actual deleted in factory

        //Add existing component
        /*************************************************************************************
          \brief Adds an existing component to this GOC.
          \param typeId  ComponentTypeId of the component.
          \param comp    Unique pointer to the component (ownership transferred).
        *************************************************************************************/
        void AddComponent(ComponentTypeId typeId, std::unique_ptr<GameComponent> comp);

        //construct add component of type T
        //A function parameter pack is a function parameter that accepts zero or more function arguments ...
        //build the component
        //Wires its owner back-pointer and type id
        //transfer ownership to the composition
        //returns a handy non-owning raw pointer to the new component
        // example : auto* t = goc.EmplaceComponent<TransformComponent>(ComponentTypeId::CT_Transform, 0.0f, 0.0f);
        //         later   t->SetPosition(15.0f,25.0f);
        /*************************************************************************************
          \brief Constructs and adds a new component of type T.
          \tparam T    The component type to construct.
          \tparam Args Constructor argument pack for T.
          \param typeId  The ComponentTypeId to assign.
          \return Raw pointer to the newly constructed component (non-owning).
        *************************************************************************************/
        template <typename T, typename... Args>
        T* EmplaceComponent(ComponentTypeId typeId, Args&&... args) {
            auto up = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw = up.get();
            raw->set_owner(this);
            raw->set_type(typeId);
            Components.emplace_back(std::move(up));
            return raw;
        }

        /*************************************************************************************
          \brief Retrieves the unique ID of this GOC.
          \return Unsigned int representing the object ID.
        *************************************************************************************/
        GOCId GetId() const { return ObjectId; }

        GameObjectComposition() = default;
        /*************************************************************************************
          \brief Destructor. Cleans up all owned components.
        *************************************************************************************/
        ~GameObjectComposition() noexcept;

    private:
        // use unique pointer as The composition exclusively owns its components when the GameObjectComposition
        //is destroy every unique_ptr is delete its component
        //unique pointer are move-only so a component instance cant be owned by 2 game object
        using UptrComp = std::unique_ptr<GameComponent>;
        std::vector<UptrComp> Components; //owned 
        GOCId ObjectId = 0;
        std::string ObjectName;

  
    };

    using GOC = GameObjectComposition;

    template<typename T>
    inline T* GameObjectComposition::GetComponentType(ComponentTypeId typeId) {
        return static_cast<T*>(GetComponent(typeId));
    }

    template<typename T>
    inline T const* GameObjectComposition::GetComponentType(ComponentTypeId typeId) const {
        return static_cast<T const*>(GetComponent(typeId));
    }

#define HAS(obj, Type) ((obj)->GetComponentType<Type>(Framework::ComponentTypeId::CT_##Type))
    // how to use it
    // usage:
    // auto* t = HAS(obj, Transform);
}
