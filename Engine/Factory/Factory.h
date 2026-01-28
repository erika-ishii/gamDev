/*********************************************************************************************
 \file      Factory.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares GameObjectFactory, the central system for creating, looking up,
            and destroying GameObjectComposition (GOC) entities. It supports loading
            prefabs from JSON, managing component creators, and ensuring **safe object
            lifetime via pooled GameObjectHandle ownership**.

 \details   Ownership & lifetime model:
            - All live GOCs are owned by the factory in a map of
              id → GameObjectHandle (GameObjectIdMap).
            - Public methods return **non-owning** GOC* for convenience; callers must
              **not delete** these pointers.
            - IdGameObject takes GameObjectHandle and transfers ownership into the map.
            - Destroy(GOC*) marks the object’s ID for deferred deletion; actual destruction
              occurs in Update()/Shutdown() when the map entry is erased (unique_ptr resets).
            - CreateTemplate builds a GOC and **releases** ownership to the caller (not ID’d
              nor tracked by the factory) so it can be stored as a prefab template elsewhere.

            Responsibilities include:
            - Creating GOCs (from JSON files or manually).
            - Assigning unique IDs to each object.
            - Registering component creators for data-driven construction.
            - Managing object lifetime (mark for destruction → sweep later).
            - Level loading utilities that construct multiple objects.

            The factory follows a data-driven approach by storing string→component-creator
            mappings, allowing JSON-defined prefabs to be built at runtime without hardcoding
            component logic.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once


#include <map>
#include <set>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include "Common/System.h"
#include "Composition/Component.h"
#include "Composition/ComponentCreator.h"
#include "Composition/Composition.h"
#include "Memory/GameObjectPool.h"
#include "Serialization/JsonSerialization.h"
#include "Core/Layer.h"
#include <optional>

// Factory responsibilities (summary)
// - Create GOCs and assign unique IDs
// - Store ownership in id→GameObjectHandle
// - Return non-owning pointers for lookups
// - Defer deletion to end-of-frame via Update()/Shutdown()
// - Load objects from data files (single and level JSON)

namespace Framework {

    /*****************************************************************************************
      \class GameObjectFactory
      \brief Central system responsible for managing all GameObjectComposition (GOC) objects.

      Ownership semantics:
      - GameObjectFactory owns all registered GOCs (id→GameObjectHandle).
      - Accessors return raw GOC* as non-owning views; do not delete them.
      - Deferred deletion is keyed by ID and executed on Update/Shutdown.

      Functional highlights:
      - Create from JSON (single object), create empty, or build from stream.
      - CreateTemplate constructs a GOC **without** assigning an ID; ownership is released
        to the caller for prefab/template storage (not tracked by factory).
      - Register component creators (string → ComponentCreator) for data-driven builds.
      - Assign unique IDs and provide ID-based lookups.
      - Load levels by iterating an array of objects in JSON.

      \see GameObjectComposition, ComponentCreator, PrefabManager
    *****************************************************************************************/
    class GameObjectFactory : public ISystem {
    public:

        GameObjectFactory();
        ~GameObjectFactory() override;

        /// Create, initialize, and assign an ID to a GOC from a JSON file.
        /// Returns a **non-owning** pointer (object is owned by the factory map).
        GOC* Create(const std::string& filename);

        /// Create an empty composition (no components), assign a new unique ID, and register it.
        /// Returns a **non-owning** pointer; ownership is stored in the id map.
        GOC* CreateEmptyComposition();

        /// Create a prefab template GOC from JSON (used by PrefabManager).
        /// Returns a raw pointer for which the **caller assumes ownership** (not ID-registered).
        GOC* CreateTemplate(const std::string& filename);

        /// Build a GOC from the current JSON object stream and register it (assign ID, take ownership).
        /// Returns a **non-owning** pointer.
        GOC* BuildFromCurrentJsonObject(ISerializer& stream);

        /// Open a JSON file and build a single GOC if root is "GameObject"; caller should call initialize().
        /// Returns a **non-owning** pointer on success, nullptr otherwise. Use CreateLevel() for arrays.
        /// \warning Function name contains a typo: BuidAndSerialize → consider BuildAndSerialize.
        GOC* BuidAndSerialize(const std::string&);

        /// Load an entire level (multiple objects) from a JSON file. Returns non-owning pointers.
        std::vector<GOC*> CreateLevel(const std::string& filename);

        /// Save the specified objects into a JSON level file compatible with CreateLevel.
        bool SaveLevel(const std::string& filename, const std::vector<GOC*>& objects,
            const std::string& levelName = "");

        /// Convenience overload that saves all currently active objects tracked by the factory.
        bool SaveLevel(const std::string& filename, const std::string& levelName = "");

        // --- Object ID & Lookup ---
        /// Assigns a unique ID (or reuses a requested one) and **transfers ownership**
        /// of the GOC into the factory’s id map. Returns a **non-owning** pointer to
        /// the registered object.
        GOC* IdGameObject(GameObjectHandle gameObject,
            std::optional<GOCId> fixedId = std::nullopt);


        /// Retrieves a GOC by its unique ID; returns a **non-owning** pointer or nullptr if not found.
        GOC* GetObjectWithId(GOCId id);

        // --- Lifetime Management ---
        /// Marks a GOC for destruction (actual erasure/deletion deferred until Update() / Shutdown()).
        void Destroy(GOC* gameObject);

        /// Performs deferred deletions by erasing map entries (which destroys unique_ptr-owned GOCs).
        void Update(float dt) override;

        /// Final sweep used during engine shutdown (mirrors Update()).
        void Shutdown() override;

        /// Name of this system.
        std::string GetName() override { return "Factory"; }

        /// Forward messages (unused in factory).
        void SendMessage(Message* m) override { (void)m; }

        LayerManager& Layers() { return LayerData; }
        const LayerManager& Layers() const { return LayerData; }

        void OnLayerChanged(GOC& object, std::string_view previousLayer);


        // --- Component Creator Registry ---
        /*************************************************************************************
          \brief Registers a component creator with the factory (factory takes ownership).
          \param name    The string identifier (e.g., "TransformComponent").
          \param creator std::unique_ptr<ComponentCreator> to transfer into the registry.
          \note Stored as std::unique_ptr in ComponentMap for exclusive ownership.
        *************************************************************************************/
        void AddComponentCreator(const std::string& name, std::unique_ptr<ComponentCreator> creator);

    private:
        unsigned LastGameObjectId = 0; ///< Counter for assigning unique GOC IDs.

        using ComponentMapType = std::map<std::string, std::unique_ptr<ComponentCreator>>;
        using GameObjectIdMapType = std::map<unsigned, GameObjectHandle>;

        ComponentMapType     ComponentMap;     ///< Map: component name → owning ComponentCreator
        GameObjectIdMapType  GameObjectIdMap;  ///< Map: GOC ID → owning handle (GameObjectHandle)
        std::set<GOCId>      ObjectsToBeDeleted; ///< Set of GOC IDs scheduled for deferred deletion
        std::vector<GOC*>     LastLevelCache;      ///< Snapshot of last saved/loaded level objects (non-owning)
        std::string           LastLevelNameCache;  ///< Cached level name (if provided)
        std::filesystem::path LastLevelPathCache;  ///< Cached level file path
        LayerManager           LayerData;

        std::string ComponentNameFromId(ComponentTypeId id) const;
        json SerializeComponentToJson(const GameComponent& component) const;
        void DeserializeComponentFromJson(GameComponent& component, const json& data) const;
        GOC* InstantiateFromSnapshotInternal(const json& data);
        bool SaveLevelInternal(const std::string& filename, const std::vector<GOC*>& objects,
            const std::string& levelName);
        /// Remove any cached last-level pointers that no longer refer to live objects.
        void PruneLastLevelCache();

    public:
        /// Read-only accessor for all objects managed by the factory (ownership retained by factory).
        const GameObjectIdMapType& Objects() const { return GameObjectIdMap; }

        /// Serialize a single live object into a JSON snapshot used by editor systems.
        json SnapshotGameObject(const GOC& object) const;

        /// Instantiate a new object from a JSON snapshot (used by editor undo).
        GOC* InstantiateFromSnapshot(const json& data);
        /// Snapshot of the most recently saved or loaded level objects.
        const std::vector<GOC*>& LastLevelObjects() const { return LastLevelCache; }
        /// Removes a pending destroy flag for the given id (used by undo).
        void CancelDestroy(GOCId id);
        /// Cached level name associated with the last save/load operation.
        const std::string& LastLevelName() const { return LastLevelNameCache; }

        /// File path used during the last save/load operation.
        const std::filesystem::path& LastLevelPath() const { return LastLevelPathCache; }

        // Example iteration usage (pseudo-code):
        // void Update(float dt) override {
        //     for (auto& [id, obj] : FACTORY->Objects()) {
        //         if (auto* r = obj->GetComponentType<Render>(CT_Render)) {
        //             if (auto* t = obj->GetComponentType<Transform>(CT_Transform)) {
        //                 Draw(*r, *t);
        //             }
        //         }
        //     }
        // }
    };

    /// Global pointer to the active factory instance (non-owning alias).
    extern GameObjectFactory* FACTORY;

}
