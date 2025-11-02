/*********************************************************************************************
 \file      Composition.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the GameObjectComposition class, which represents a collection of
            components that together define the behavior and state of a game object.
            Provides lifecycle management, component querying, deep-copy cloning,
            and integration with the GameObjectFactory for creation and destruction.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Composition.h"
#include "Factory/Factory.h"
#include <utility>
namespace Framework {
    //Default destructor: vector<unique_ptr<...>> automatically release own component
    /*************************************************************************************
      \brief Default destructor. Relies on std::vector<std::unique_ptr<GameComponent>>
             to automatically clean up owned components.
    *************************************************************************************/
    GameObjectComposition::~GameObjectComposition() = default;

    /*************************************************************************************
    \brief Updates the logical layer assignment for this game object.
    \param layer Desired layer name. Empty strings fall back to "Default".
    \note  Notifies the factory so that layer membership lists stay in sync.
  *************************************************************************************/
    void GameObjectComposition::SetLayerName(const std::string& layer)
    {
        std::string newLayer = layer.empty() ? std::string("Default") : layer;
        if (LayerName == newLayer)
            return;

        std::string previous = LayerName;
        LayerName = std::move(newLayer);

        if (FACTORY && ObjectId != 0)
        {
            FACTORY->OnLayerChanged(*this, previous);
        }
    }

    /*************************************************************************************
      \brief Sends a message to all components attached to this GameObjectComposition.
      \param message Reference to the message being dispatched.
    *************************************************************************************/
    void GameObjectComposition::SendMessage(Message& message) {
        for (auto& up : Components) {
            if (up) up->SendMessage(message);
        }
    }

    //GetComponent is member function of GameObjectComposition 
    //Its Job is to find a component attached to a game object by its ComponentTypeId and return it
    /*************************************************************************************
      \brief Retrieves a component by its ComponentTypeId.
      \param typeId The type identifier of the component to find.
      \return Raw pointer to the component if found, otherwise nullptr.
    *************************************************************************************/
    GameComponent* GameObjectComposition::GetComponent(ComponentTypeId typeId)
    {
        for (auto& up : Components) { // scan through component
            if (up && up->GetTypeId() == typeId) {
                return up.get();    // Return raw pointer(non-owner)
            }
        }
        return nullptr; // not found
    }

    /*************************************************************************************
      \brief Const-qualified version of GetComponent.
      \param typeId The type identifier of the component to find.
      \return Const raw pointer to the component if found, otherwise nullptr.
    *************************************************************************************/
    GameComponent const* GameObjectComposition::GetComponent(ComponentTypeId typeId) const
    {
        for (auto const& up : Components) {
            if (up && up->GetTypeId() == typeId) {
                return up.get();
            }
        }
        return nullptr;
    }

    /*************************************************************************************
      \brief Calls initialize() on all attached components.
    *************************************************************************************/
    void GameObjectComposition::initialize() {
        for (auto& up : Components) {   // call Initialize all component
            if (up) up->initialize();
        }
    }

    /*************************************************************************************
      \brief Marks this GameObjectComposition for destruction by the factory.
    *************************************************************************************/
    void GameObjectComposition::Destroy()
    {
        FACTORY->Destroy(this);
    }

    /*************************************************************************************
      \brief Creates a deep copy of this GameObjectComposition, including all components.
      \return Pointer to the newly cloned GameObjectComposition.
      \note   The clone is registered with the factory and assigned a new unique ID.
    *************************************************************************************/
    GameObjectComposition* GameObjectComposition::Clone() const {
        // Fresh object with a new ID and registered in the factory map
        GOC* clone = FACTORY->CreateEmptyComposition();

        // Copy name (optional but useful)
        clone->ObjectName = ObjectName;

        // Avoid reallocations
        clone->Components.reserve(Components.size());

        // Deep-copy components
        for (auto const& up : Components) {
            if (!up) continue;

            auto newComp = up->Clone();           // unique_ptr<GameComponent>
            newComp->set_owner(clone);            // rewire owner
            newComp->set_type(up->GetTypeId());   // preserve type id
            clone->Components.emplace_back(std::move(newComp));
        }
        clone->SetLayerName(LayerName);
        clone->initialize();
        return clone;
    }

    // it take a new GameCompomnent(wrapped in std::unqiue_ptr for ownwership) set it up
    // and adds it to the GameObjectComposition list of components
    /*************************************************************************************
      \brief Adds a component to this GameObjectComposition.
      \param typeId The type identifier of the component.
      \param comp   Unique pointer to the component to add. Ownership is transferred.
      \details
        - Sets the component’s owner back-pointer to this GameObjectComposition.
        - Stores the ComponentTypeId in the component.
        - Transfers ownership into the Components vector.
    *************************************************************************************/
    void GameObjectComposition::AddComponent(ComponentTypeId typeId, std::unique_ptr<GameComponent> comp)
    {
        if (!comp) return;   //ignore null
        // call the protected setter inside GameComponent to store a back-poingter to owning GameObjectComposition
        // This allow component to call GetOwner()->GetComponent<Transform>() 
        comp->set_owner(this);
        //Store the componentTypeId (e.g CT_Transform) 
        // if (comp->GetTypeId() == CT_Transform)
        comp->set_type(typeId);
        //Component is a std::vector<std::unique_ptr<GameComponent>>
        //std::move transfer ownership from caller into the vector so GameObjectComposition own it component exclusively
        //emplace_back effienctly construct or move object at the end of the vector
        Components.emplace_back(std::move(comp));
    }
}
