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
#include <filesystem>
#include <system_error>
#include <fstream>

#include "Factory/Factory.h"
#include "Core/PathUtils.h"
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW



#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif

namespace Framework
{
    /// Stores the master prefab copies (name → GOC master copy).
    std::unordered_map<std::string, std::unique_ptr<Framework::GOC>> master_copies;

    namespace
    {
        // Only treat files shaped like:
        // { "GameObject": { "name": "...", "Components": { ... } } }
        static bool IsPrefabJson(const std::filesystem::path& path)
        {
            if (path.extension() != ".json")
                return false;

            std::ifstream in(path);
            if (!in.is_open())
                return false;

            try
            {
                nlohmann::json j;
                in >> j;

                if (!j.contains("GameObject") || !j["GameObject"].is_object())
                    return false;

                auto& go = j["GameObject"];
;

                if (!go.contains("Components") || !go["Components"].is_object())
                    return false;

                return true;
            }
            catch (...)
            {
                // malformed json / not parseable / not expected shape
                return false;
            }
        }

        static void RegisterPrefabFromFile(const std::filesystem::path& path)
        {
            if (!IsPrefabJson(path))
                return;

            GOC* templateGoc = FACTORY->CreateTemplate(path.string());
            if (!templateGoc)
                return;

            std::unique_ptr<GOC> owned(templateGoc);

            // Prefer the "name" inside JSON (Factory likely sets it on the GOC),
            // fallback to filename stem.
            std::string prefabKey = owned->GetObjectName();
            if (prefabKey.empty())
                prefabKey = path.stem().string();
            if (prefabKey.empty())
                return;

            // IMPORTANT: overwrite existing key so updated files replace old ones.
            master_copies[prefabKey] = std::move(owned);
        }

        static void LoadPrefabsFromDirectory(const std::filesystem::path& directory)
        {
            std::error_code ec;
            if (!std::filesystem::exists(directory, ec))
                return;

            for (auto const& entry : std::filesystem::recursive_directory_iterator(directory, ec))
            {
                if (ec) break;
                if (!entry.is_regular_file()) continue;
                RegisterPrefabFromFile(entry.path());
            }
        }
    } // anonymous namespace

    /*************************************************************************************
      \brief Loads prefabs from disk and registers them into the master_copies map.
      \details
        - Loads ALL prefab json files from Data_Files/Prefabs (and subfolders).
        - Skips non-prefab json using schema validation.
        - Overwrites duplicates by prefab name so updated files take effect.

      Folder convention:
        Data_Files/Prefabs/*.json
    *************************************************************************************/
    void LoadPrefabs()
    {
        // Clear first to avoid stale prefabs when reloading.
        master_copies.clear();

        // Scan ONLY the Prefabs folder.
        // Example: <Project>/Data_Files/Prefabs/
        const std::filesystem::path prefabDir = Framework::ResolveDataPath("Prefabs");
        LoadPrefabsFromDirectory(prefabDir);


    }

    /*************************************************************************************
      \brief Unloads all prefabs and clears the master_copies map.
      \details
        - master_copies stores std::unique_ptr<GOC>, so clearing destroys the masters.
    *************************************************************************************/
    void UnloadPrefabs()
    {
        master_copies.clear();
    }

} // namespace Framework
