/*********************************************************************************************
 \file      PrefabManager.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the PrefabManager interface, which manages reusable game object
            templates (prefabs). Provides functions to load prefabs from JSON files,
            unload them from memory, and clone prefab instances on demand.

 \details   Prefabs are master GameObjectComposition (GOC) templates that can be
            efficiently cloned at runtime to spawn new entities without reparsing
            JSON definitions. This system stores prefabs in a global registry and
            exposes convenience functions for lifecycle management and instancing.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <unordered_map>
#include "Composition.h"
#include <memory>
namespace Framework {

    /*****************************************************************************************
      \var master_copies
      \brief Global registry mapping prefab names (string) to their master GOC instances.

      Each prefab is stored as a pointer to a GameObjectComposition (created via factory).
      Prefabs act as immutable templates: users should not modify them directly but instead
      use ClonePrefab() to generate instances.
    *****************************************************************************************/
    extern std::unordered_map<std::string, std::unique_ptr<GOC>> master_copies;

    /*****************************************************************************************
      \brief Loads prefabs from JSON files and registers them in the prefab map.
      \note  Typically called once during game initialization.
    *****************************************************************************************/
    void LoadPrefabs();

    // Delete all masters and clear the map
    /*****************************************************************************************
      \brief Unloads all prefabs, deleting master copies and clearing the map.
      \note  Prefabs must be reloaded by calling LoadPrefabs() again before use.
    *****************************************************************************************/
    void UnloadPrefabs();

    /*****************************************************************************************
      \brief Clones a prefab by name.
      \param name  The string key of the prefab (e.g., "Player", "Circle").
      \return Pointer to a new GOC instance if prefab exists, nullptr otherwise.

      Example usage:
      \code
        GOC* player = Framework::ClonePrefab("Player");
        if (player) {
            // safely use new player instance
        }
      \endcode
    *****************************************************************************************/
    inline GOC* ClonePrefab(const std::string& name) {
        auto it = master_copies.find(name);
        if (it == master_copies.end() || !it->second)
            return nullptr;

        GOC* clone = it->second->Clone();
        if (clone && clone->GetObjectName().empty())
            clone->SetObjectName(name);

        return clone;
    }
}
