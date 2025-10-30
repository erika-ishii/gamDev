/*********************************************************************************************
 \file      Factory.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the GameObjectFactory system responsible for creating, identifying,
            serializing, and destroying GameObjectComposition (GOC) instances. Supports
            JSON-driven construction (single object and levels), prefab template creation,
            component-creator registry management, and deferred deletion.

 \details   Ownership model (important):
            - The factory maintains ownership of all live GOCs via a map of
              id → std::unique_ptr<GOC> (GameObjectIdMap).
            - Public methods that return a GOC* return a **non-owning** raw pointer
              for convenience. Callers must **not** delete these pointers.
            - Prefab templates built by CreateTemplate are allocated with std::unique_ptr
              and then **released** (ownership transferred) to the caller.
            - Deferred deletion is implemented by recording GOC IDs in a set
              (ObjectsToBeDeleted) and erasing the corresponding unique_ptrs in Update/Shutdown.
              Erasing from the map automatically destroys the GOC.

            Key behaviors:
            - Enforces a single global factory instance (FACTORY).
            - Assigns unique IDs to GOCs and maintains an id→unique_ptr<GOC> map.
            - Supports BuildFromCurrentJsonObject for data-driven construction from an
              already-positioned JSON serializer.
            - Defers destruction via an ObjectsToBeDeleted set to avoid mid-frame invalidation.
            - Exposes level loading by iterating a "GameObjects" JSON array.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Factory.h"
#include <stdexcept>

// Summary of responsibilities:
// - Create empty game objects (GOCs)
// - Assign each a unique integer ID and keep an ownership table (id -> std::unique_ptr<GOC>)
// - Provide lookups that return non-owning GOC* for read/modify access
// - Let you mark GOCs for deferred deletion; the actual destruction happens in Update/Shutdown
// - Hold a registry of component creators (string name -> unique_ptr<ComponentCreator>) so
//   data-driven loader can build GOCs from JSON files

namespace Framework {

    /// Global singleton pointer to the active factory instance. Non-owning.
    GameObjectFactory* FACTORY = nullptr;

    /*************************************************************************************
      \brief Constructs the factory and sets the global FACTORY pointer.
      \throws std::runtime_error if a factory already exists.
      \note   The factory is intended to be a unique runtime service.
    *************************************************************************************/
    GameObjectFactory::GameObjectFactory()
    {
        if (FACTORY) throw std::runtime_error("Factory already created");
        FACTORY = this; // FACTORY is a non-owning alias to the sole instance
    }

    /*************************************************************************************
      \brief Destroys the factory, cleaning up all remaining objects and creators.
      \details
        - Clearing GameObjectIdMap destroys all GOCs via std::unique_ptr.
        - Clearing ObjectsToBeDeleted simply drops IDs (no live objects remain).
        - Clearing ComponentMap destroys all registered ComponentCreators via std::unique_ptr.
        - Resets global FACTORY pointer to nullptr.
    *************************************************************************************/
    GameObjectFactory::~GameObjectFactory() {
        GameObjectIdMap.clear();
        ObjectsToBeDeleted.clear();
        ComponentMap.clear();
        FACTORY = nullptr;
    }

    /*************************************************************************************
      \brief Creates a GOC from a JSON file, initializes it, and returns it.
      \param filename Path to JSON describing a single GameObject.
      \return Non-owning pointer to the created GOC (owned by the factory), or nullptr on failure.
      \note   Ownership remains with the factory (stored in GameObjectIdMap).
    *************************************************************************************/
    GOC* GameObjectFactory::Create(const std::string& filename)
    {
        GOC* goc = BuidAndSerialize(filename);
        if (goc) goc->initialize();
        return goc; // non-owning
    }

    /*************************************************************************************
      \brief Creates an empty GOC (no components), assigns a unique ID, and registers it.
      \return Non-owning raw pointer to the new GOC (owned by the factory’s id map).
      \note   Ownership is transferred into the id map as a std::unique_ptr.
    *************************************************************************************/
    GOC* GameObjectFactory::CreateEmptyComposition() {
        auto goc = std::make_unique<GOC>();
        return IdGameObject(std::move(goc));
    }

    /*************************************************************************************
      \brief Creates a prefab template GOC from a JSON file **without** assigning an ID.
      \param filename Path to JSON describing a single GameObject.
      \return Raw pointer to the newly built GOC template; **caller takes ownership**.
      \note   Intended for PrefabManager. The template is created with unique_ptr and then
              released (goc.release()), so it is **not** tracked in the factory’s id map.
    *************************************************************************************/
    GOC* GameObjectFactory::CreateTemplate(const std::string& filename)
    {
        JsonSerializer s;
        if (!s.Open(filename) || !s.IsGood()) return nullptr;
        if (!s.EnterObject("GameObject")) return nullptr;

        auto goc = std::make_unique<GOC>();

        if (s.HasKey("name")) {
            std::string name; s.ReadString("name", name);
            goc->SetObjectName(name);
        }
        if (s.EnterObject("Components")) {
            for (auto& kv : ComponentMap) {
                const std::string& compName = kv.first;
                ComponentCreator* creator = kv.second.get();
                if (!s.HasKey(compName)) continue;
                if (!s.EnterObject(compName)) continue;

                std::unique_ptr<GameComponent> comp(creator->Create());
                if (comp) {
                    StreamRead(s, *comp);
                    goc->AddComponent(creator->TypeId, std::move(comp));
                }
                s.ExitObject();
            }
            s.ExitObject();
        }

        s.ExitObject();

        // IMPORTANT: no IdGameObject(); no registration in GameObjectIdMap.
        // Ownership is intentionally released to the caller.
        return goc.release();
    }

    /*************************************************************************************
      \brief Builds a GOC from the serializer’s current object (expects "Components").
      \param stream An opened serializer, positioned at a GameObject JSON object.
      \return Non-owning pointer to the created and ID-assigned GOC.
      \details
        - Reads "name" if present.
        - Iterates all registered ComponentCreators and builds any present components.
        - Assigns a unique ID and registers the object in the id map as unique_ptr.
    *************************************************************************************/
    GOC* GameObjectFactory::BuildFromCurrentJsonObject(ISerializer& stream)
    {
        auto goc = std::make_unique<GOC>();
        if (stream.HasKey("name")) {
            std::string name;
            stream.ReadString("name", name);
            goc->SetObjectName(name);
        }

        // Enter the Components object if present
        if (stream.EnterObject("Components")) {
            for (auto& kv : ComponentMap) {
                const std::string& compName = kv.first;
                ComponentCreator* creator = kv.second.get();

                if (!stream.HasKey(compName))
                    continue;

                // First, try to enter the component's JSON object; only then create the component
                if (!stream.EnterObject(compName))
                    continue;

                // Create as unique_ptr so any failure auto-cleans
                std::unique_ptr<GameComponent> comp(creator->Create());
                if (!comp) {
                    // couldn't create; leave JSON object and continue
                    stream.ExitObject();
                    continue;
                }

                // Let the component load itself
                StreamRead(stream, *comp);

                // Leave the component scope
                stream.ExitObject();

                goc->AddComponent(creator->TypeId, std::move(comp));
            }
            stream.ExitObject();
        }
        return IdGameObject(std::move(goc));
    }

    /*************************************************************************************
      \brief Opens a JSON file and builds a single GameObject if the root is "GameObject".
      \param filename Path to JSON file.
      \return Non-owning pointer to GOC on success (caller should call initialize()),
              nullptr if not a single-object file.
      \note   Use CreateLevel() for level files with arrays of objects.
      \warning The function name contains a typo (BuidAndSerialize). Consider renaming.
    *************************************************************************************/
    GOC* GameObjectFactory::BuidAndSerialize(const std::string& filename)
    {
        JsonSerializer stream;
        if (!stream.Open(filename) || !stream.IsGood()) return nullptr;

        // Old single-object shape
        if (stream.EnterObject("GameObject")) {
            GOC* g = BuildFromCurrentJsonObject(stream);
            stream.ExitObject();
            return g; // non-owning; owned by GameObjectIdMap
        }

        // If the file is actually a level, just return nullptr
        // (call CreateLevel() for that file)
        return nullptr;
    }

    /*************************************************************************************
      \brief Loads a level file containing an array "GameObjects" and builds each GOC.
      \param filename Path to a JSON level file.
      \return Vector of non-owning pointers to created GOCs (each ID-assigned and owned by the factory).
    *************************************************************************************/
    std::vector<GOC*> GameObjectFactory::CreateLevel(const std::string& filename)
    {
        JsonSerializer s;
        std::vector<GOC*> out;
        if (!s.Open(filename) || !s.IsGood()) return out;

        if (!s.EnterObject("Level")) return out;

        if (s.EnterArray("GameObjects")) {
            size_t n = s.ArraySize();
            out.reserve(n);
            for (size_t i = 0; i < n; i++) {
                if (!s.EnterIndex(i)) continue; // now at GameObjects[i]

                GOC* g = BuildFromCurrentJsonObject(s);
                if (g) { out.push_back(g); }
                s.ExitObject();                     // leave GameObject[i]
            }
            s.ExitArray();
        }
        s.ExitObject();
        return out;
    }

    /*************************************************************************************
      \brief Assigns a unique ID to the GOC and registers it in the id→object map.
      \param gameObject Newly constructed GOC to identify and take ownership of.
      \return Non-owning pointer to the now-registered GOC.
      \details
        - Increments the running ID counter.
        - Moves the unique_ptr into GameObjectIdMap, which now owns the object.
    *************************************************************************************/
    GOC* GameObjectFactory::IdGameObject(std::unique_ptr<GOC> gameObject) {
        if (!gameObject)
            return nullptr;

        ++LastGameObjectId;   // assign next unique sequential ID
        gameObject->ObjectId = LastGameObjectId; // friend access grants direct write
        GOC* raw = gameObject.get();             // non-owning view
        GameObjectIdMap.emplace(LastGameObjectId, std::move(gameObject)); // take ownership
        return raw;
    }

    /*************************************************************************************
      \brief Looks up a GOC by its unique ID.
      \param id The identifier to look for.
      \return Non-owning pointer to the GOC if found, nullptr otherwise.
      \note   The returned pointer is owned by the factory; do not delete.
    *************************************************************************************/
    GOC* GameObjectFactory::GetObjectWithId(GOCId id)
    {
        // Lookup by id: return GOC* (non-owning) or nullptr if not found
        auto it = GameObjectIdMap.find(id);
        return it == GameObjectIdMap.end() ? nullptr : it->second.get();
    }

    /*************************************************************************************
      \brief Marks a GOC for deferred destruction.
      \param gameObject Pointer to the object to destroy (non-owning).
      \details Inserts the object's ID into a set to avoid duplicate entries and to defer
               destruction until Update/Shutdown, preventing mid-frame invalidation.
    *************************************************************************************/
    void GameObjectFactory::Destroy(GOC* gameObject)
    {
        if (!gameObject)
            return;

        ObjectsToBeDeleted.insert(gameObject->ObjectId); // mark by ID; actual erase later
    }

    /*************************************************************************************
      \brief Performs the end-of-frame sweep to delete marked objects safely.
      \param dt Delta time (unused here).
      \details
        - Iterates the deletion set, finds each ID in the map, and erases it.
        - Erasing the map entry releases and destroys the owned GOC (unique_ptr resets).
        - Clears the deletion set after processing.
        - Prevents iterator invalidation / crashes during update loops.
    *************************************************************************************/
    void GameObjectFactory::Update(float dt)
    {
        (void)dt;
        for (auto id : ObjectsToBeDeleted) {
            auto it = GameObjectIdMap.find(id);
            if (it != GameObjectIdMap.end()) {
                GameObjectIdMap.erase(it); // unique_ptr destruction happens here
            }
        }
        ObjectsToBeDeleted.clear();
    }

    /*************************************************************************************
      \brief Final sweep used during engine shutdown.
      \details Mirrors Update(): erases any still-marked IDs from the map to destroy
               their objects, then clears the deletion set.
    *************************************************************************************/
    void GameObjectFactory::Shutdown()
    {
        for (auto id : ObjectsToBeDeleted) {
            auto it = GameObjectIdMap.find(id);
            if (it != GameObjectIdMap.end()) {
                GameObjectIdMap.erase(it); // unique_ptr destruction happens here
            }
        }
        ObjectsToBeDeleted.clear();
    }

    /*************************************************************************************
      \brief Registers a ComponentCreator under a string name for data-driven builds.
      \param name    Component name as it appears in JSON (e.g., "TransformComponent").
      \param creator Unique pointer to the ComponentCreator; factory assumes ownership.
      \note  The creator is stored as std::unique_ptr in ComponentMap.
    *************************************************************************************/
    void GameObjectFactory::AddComponentCreator(const std::string& name, std::unique_ptr<ComponentCreator> creator)
    {
        ComponentMap[name] = std::move(creator);
    }
}
