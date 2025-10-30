/*********************************************************************************************
 \file      PrefabManager.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the PrefabManager system, which loads and unloads reusable
            game object templates (prefabs) from JSON files. Prefabs serve as
            master copies that can be cloned to spawn multiple instances of
            game objects at runtime.

 \details   The PrefabManager maintains an internal map of prefab names to their
            corresponding GameObjectComposition (GOC) master copies. Prefabs are
            created via the GameObjectFactory and can be reused to efficiently
            spawn new objects without reparsing JSON each time.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "PrefabManager.h"
#include <iostream>
#include "Factory/Factory.h"


namespace Framework {

    /// Stores the master prefab copies (name → GOC pointer).
    std::unordered_map<std::string, std::unique_ptr<Framework::GOC>> master_copies;

    /*************************************************************************************
      \brief Loads prefabs from disk and registers them into the master_copies map.
      \details
        - Each prefab is created by the GameObjectFactory from a JSON file.
        - Prefabs currently include Circle, Rect, and Player.
        - The Player prefab logs an error if creation fails.

      Example JSON paths:
        - "../../Data_Files/circle.json"
        - "../../Data_Files/rect.json"
        - "../../Data_Files/player.json"
    *************************************************************************************/
    void LoadPrefabs()
    {
        if (auto* c = FACTORY->CreateTemplate("../../Data_Files/circle.json"))
            master_copies["Circle"].reset(c);

        if (auto* r = FACTORY->CreateTemplate("../../Data_Files/rect.json"))
            master_copies["Rect"].reset(r);
        
        if (auto* e = FACTORY->CreateTemplate("../../Data_Files/basic_enemy.json"))
            master_copies["BasicEnemy"].reset(e);

        if (auto* p = FACTORY->CreateTemplate("../../Data_Files/player.json")) {
            master_copies["Player"].reset(p);
        }
        if (auto* b = FACTORY->CreateTemplate("../../Data_Files/boss.json")) {
            master_copies["Boss"].reset(b);
        }
        else {
            std::cerr << "[Prefab] Failed to create 'boss' from boss.json\n";
        }
    }

    /*************************************************************************************
      \brief Unloads all prefabs and clears the master_copies map.
      \details
     - master_copies now stores std::unique_ptr<GOC>, so clearing the map automatically
          destroys the prefab masters.
      \note Prefabs must be reloaded via LoadPrefabs() if needed again.
    *************************************************************************************/
    void UnloadPrefabs() {
        master_copies.clear();
    }

}
