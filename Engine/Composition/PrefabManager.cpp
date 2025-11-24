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
#include "Core/PathUtils.h"
#include <filesystem>

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
        auto resolveData = [](const std::string& name) {
            return Framework::ResolveDataPath(std::filesystem::path(name)).string();
            };

        if (auto* c = FACTORY->CreateTemplate(resolveData("circle.json")))
            master_copies["Circle"].reset(c);

        if (auto* r = FACTORY->CreateTemplate(resolveData("rect.json")))
            master_copies["Rect"].reset(r);
        
        if (auto* e = FACTORY->CreateTemplate(resolveData("enemy.json")))
            master_copies["Enemy"].reset(e);

        if (auto * re = FACTORY->CreateTemplate(resolveData("enemyranged.json")))
            master_copies["Enemyranged"].reset(re);

        if (auto* p = FACTORY->CreateTemplate(resolveData("player.json"))) {
            master_copies["Player"].reset(p);
        }
        if (auto* b = FACTORY->CreateTemplate(resolveData("boss.json"))) {
            master_copies["Boss"].reset(b);
        }
        else {
            std::cerr << "[Prefab] Failed to create 'boss' from boss.json\n";
        }
        if (auto* h1 = FACTORY->CreateTemplate(resolveData("hang_clothes_01.json"))) {
            master_copies["Hang_Clothes_01"].reset(h1);
        }

        if (auto* h2 = FACTORY->CreateTemplate(resolveData("hang_clothes_02.json"))) {
            master_copies["Hang_Clothes_02"].reset(h2);
        }

        if (auto* flag = FACTORY->CreateTemplate(resolveData("hawker_flag.json"))) {
            master_copies["Hawker_Flag"].reset(flag);
        }
        if (auto* table = FACTORY->CreateTemplate(resolveData("hawker_table.json"))) {
            master_copies["Hawker_Table"].reset(table);
        }

        if (auto* store = FACTORY->CreateTemplate(resolveData("hawker_store.json"))) {
            master_copies["Hawker_Store"].reset(store);
        }

        if (auto* floor = FACTORY->CreateTemplate(resolveData("hawker_floor_background.json"))) {
            master_copies["Hawker_Floor"].reset(floor);
        }

        if (auto* hdb = FACTORY->CreateTemplate(resolveData("hawker_hdb_background.json"))) {
            master_copies["Hawker_HDB"].reset(hdb);
        }

        if (auto* bin = FACTORY->CreateTemplate(resolveData("hawker_bin.json"))) {
            master_copies["Hawker_Bin"].reset(bin);
        }

        if (auto* bucket = FACTORY->CreateTemplate(resolveData("hawker_bucket.json"))) {
            master_copies["Hawker_Bucket"].reset(bucket);
        }

        if (auto* cart = FACTORY->CreateTemplate(resolveData("hawker_cleaning_cart.json"))) {
            master_copies["Cleaning_Cart"].reset(cart);
        }

        if (auto* cs1 = FACTORY->CreateTemplate(resolveData("hawker_cleaning_sign_01.json"))) {
            master_copies["Cleaning_Sign_01"].reset(cs1);
        }

        if (auto* cs2 = FACTORY->CreateTemplate(resolveData("hawker_cleaning_sign_02.json"))) {
            master_copies["Cleaning_Sign_02"].reset(cs2);
        }

        if (auto* tray = FACTORY->CreateTemplate(resolveData("hawker_tray_return.json"))) {
            master_copies["Tray_Return"].reset(tray);
        }
        if (auto* storeAnimated = FACTORY->CreateTemplate(resolveData("hawker_store_animated.json"))) {
            master_copies["Hawker_Store_Animated"].reset(storeAnimated);
        }


        if (auto* gate = FACTORY->CreateTemplate(resolveData("hawker_gate.json"))) {
            master_copies["Hawker_Gate"].reset(gate);
        }

        if (auto* artasset = FACTORY->CreateTemplate(resolveData("artassets.json"))) {
            master_copies["Art_Asset"].reset(artasset); 
        }

        if (auto* artassetForward = FACTORY->CreateTemplate(resolveData("artassetsForward.json"))) {
            master_copies["Art_Asset_Forward"].reset(artassetForward);
        }
       
        if (auto* artassetFrontOfPlayer = FACTORY->CreateTemplate(resolveData("artassetsFrontOfPlayer.json"))) {
            master_copies["Art_Asset_Front_Of_Player"].reset(artassetFrontOfPlayer);
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
