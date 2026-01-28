/*********************************************************************************************
 \file      ComponentCreator.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the ComponentCreator system, which provides an abstract interface
            and templated implementation for dynamically creating game components at
            runtime. Components are constructed by creators and then **immediately
            wrapped in ComponentHandle by the caller (the factory)** to enforce
            single‑owner semantics.

 \details   Ownership & lifetime model:
            - The factory stores creators as std::unique_ptr<ComponentCreator> in its
              registry (string → creator).
            - ComponentCreator::Create() returns a **ComponentHandle** that owns the
              newly created component. The handle's custom deleter returns memory
              to the component pool instead of using delete.
            - This design allows the factory to manage component lifetimes uniformly
              without exposing ownership to clients of the component system.

            Registration is simplified via the RegisterComponent macro, which creates a
            ComponentCreatorType<T> and transfers ownership to the factory using
            std::make_unique.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <memory>
#include <string>
#include "Component.h"
#include "Memory/ComponentPool.h"

// Responsibility: declare a uniform interface for creating components while letting
// the factory own both creators (unique_ptr) and the components they produce (wrapped
// into ComponentHandle immediately after creation).

namespace Framework {

    /*****************************************************************************************
      \class ComponentCreator
      \brief Abstract base class for component creators.

      Provides a uniform interface for dynamically creating game components without
      knowing their concrete type. Each creator is associated with a ComponentTypeId
      for lookup and registration in the factory.
    *****************************************************************************************/
    class ComponentCreator {
    public:
        // explicit to prevent unintended implicit conversions
        /*************************************************************************************
          \brief Constructs a ComponentCreator with the given type ID.
          \param typeId  The ComponentTypeId that this creator is responsible for.
        *************************************************************************************/
        explicit ComponentCreator(ComponentTypeId typeId) : TypeId(typeId) {}

        /*************************************************************************************
          \brief Virtual destructor for safe polymorphic deletion.
        *************************************************************************************/
        virtual ~ComponentCreator() = default;

        ComponentTypeId TypeId; ///< Identifier for the component type produced by this creator

        /*************************************************************************************
          \brief Creates a new instance of the component.
          \return A ComponentHandle owning the newly created GameComponent.
          \note   The creator returns a handle with a custom deleter so the object
                  returns to the pool instead of using delete.
        *************************************************************************************/
        virtual ComponentHandle Create() = 0; // pure virtual
    };

    /*****************************************************************************************
      \class ComponentCreatorType
      \brief Templated concrete creator for a specific component type.

      Implements Create() by instantiating objects of type T. Intended to be used with
      the RegisterComponent macro for simple registration.
    *****************************************************************************************/
    template<typename T>
    class ComponentCreatorType : public ComponentCreator {
    public:
        /*************************************************************************************
          \brief Constructs a ComponentCreatorType for the given component type ID.
          \param typeId  The ComponentTypeId associated with the component type T.
        *************************************************************************************/
        explicit ComponentCreatorType(ComponentTypeId typeId)
            : ComponentCreator(typeId) {
        }

        /*************************************************************************************
          \brief Creates a new instance of the component of type T.
          \return ComponentHandle owning a new T instance.
        *************************************************************************************/
        ComponentHandle Create() override
        {
            // Allocate raw storage from the pool and placement-new the component.
            return ComponentPool<T>::Create();
        }
    };
}

// Register component macro
// Factory API: void AddComponentCreator(const std::string&, std::unique_ptr<ComponentCreator>);
/*************************************************************************************
  \def RegisterComponent
  \brief Registers a component type with the global factory.

  Creates a ComponentCreatorType for the given type and associates it with its
  ComponentTypeId. Ownership of the creator is transferred to the factory via
  std::make_unique and stored as std::unique_ptr in the registry.

  Example:
  \code
      RegisterComponent(TransformComponent);
  \endcode
*************************************************************************************/
#define RegisterComponent(type) \
    FACTORY->AddComponentCreator( \
        #type, std::make_unique<Framework::ComponentCreatorType<type>>( \
            Framework::ComponentTypeId::CT_##type))
