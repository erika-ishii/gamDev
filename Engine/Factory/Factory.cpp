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
#include <filesystem>
#include <fstream>
#include <iomanip>

#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"
#include "Component/SpriteAnimationComponent.h"

#include "Component/PlayerComponent.h"
#include "Component/PlayerHealthComponent.h"
#include "Component/PlayerAttackComponent.h"
#include "Component/HitBoxComponent.h"
#include "Component/EnemyComponent.h"
#include "Component/EnemyAttackComponent.h"
#include "Component/EnemyDecisionTreeComponent.h"
#include "Component/EnemyHealthComponent.h"
#include "Component/EnemyTypeComponent.h"

#include "Physics/Dynamics/RigidBodyComponent.h"

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
        - Clears all game objects and component creators via std::unique_ptr.
        - Resets global FACTORY pointer to nullptr.
    *************************************************************************************/
    GameObjectFactory::~GameObjectFactory() {
        Shutdown();
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
        if (s.HasKey("layer")) {
            std::string layer; s.ReadString("layer", layer);
            goc->SetLayerName(layer);
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
        if (stream.HasKey("layer")) {
            std::string layer;
            stream.ReadString("layer", layer);
            goc->SetLayerName(layer);
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
        LastLevelCache.clear();
        LastLevelNameCache.clear();
        LastLevelPathCache = filename;

        if (!s.Open(filename) || !s.IsGood()) return out;

        if (!s.EnterObject("Level")) return out;

        if (s.HasKey("name")) {
            std::string levelName;
            s.ReadString("name", levelName);
            LastLevelNameCache = levelName;
        }

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
        LastLevelCache = out;
        return out;
    }

    /*************************************************************************************
    \brief Look up the JSON component name string from a ComponentTypeId.
    \param id The concrete ComponentTypeId to resolve.
    \return The registered JSON key (e.g., "TransformComponent") if found; empty string otherwise.
    \details
      - Iterates the ComponentMap registry (name → ComponentCreator).
      - Matches by comparing the stored creator->TypeId against the given id.
      - Safe on missing/unknown ids and null creators.
   *************************************************************************************/
    std::string GameObjectFactory::ComponentNameFromId(ComponentTypeId id) const
    {
        for (auto const& [name, creator] : ComponentMap) {
            if (creator && creator->TypeId == id)
                return name;
        }
        return {};
    }

    /*************************************************************************************
     \brief Serialize a single attached component into a JSON object.
     \param component The component to serialize (polymorphic GameComponent&).
     \return A json::object() containing only the relevant fields for that component type.
     \details
       - Switches on component.GetTypeId() and static_casts to the concrete type.
       - Emits a compact JSON blob with stable keys per component (e.g., Transform: x,y,rot).
       - Unknown/unhandled types default to an empty JSON object.
       - Pure tag/marker components (e.g., PlayerComponent) serialize to an empty object.
    *************************************************************************************/
    json GameObjectFactory::SerializeComponentToJson(const GameComponent& component) const
    {
        switch (component.GetTypeId()) {
        case ComponentTypeId::CT_TransformComponent: {
            auto const& tr = static_cast<TransformComponent const&>(component);
            return json{ {"x", tr.x}, {"y", tr.y}, {"rot", tr.rot} };
        }
        case ComponentTypeId::CT_RenderComponent: {
            auto const& rc = static_cast<RenderComponent const&>(component);
            json out = {
               {"w", rc.w},
               {"h", rc.h},
               {"r", rc.r},
               {"g", rc.g},
               {"b", rc.b},
               {"a", rc.a},
               {"visible", rc.visible},
               {"layer", rc.layer}
            };
            if (!rc.texture_key.empty())
                out["texture_key"] = rc.texture_key;
            if (!rc.texture_path.empty())
                out["texture_path"] = rc.texture_path;
            return out;
        }
        case ComponentTypeId::CT_CircleRenderComponent: {
            auto const& cc = static_cast<CircleRenderComponent const&>(component);
            return json{ {"radius", cc.radius}, {"r", cc.r}, {"g", cc.g}, {"b", cc.b}, {"a", cc.a} };
        }
        case ComponentTypeId::CT_SpriteComponent: {
            auto const& sp = static_cast<SpriteComponent const&>(component);
            json out = json::object();
            if (!sp.texture_key.empty()) out["texture_key"] = sp.texture_key;
            if (!sp.path.empty()) out["path"] = sp.path;
            return out;
        }
        case ComponentTypeId::CT_SpriteAnimationComponent: {
            auto const& anim = static_cast<const SpriteAnimationComponent&>(component);

            json animations = json::array();
            for (auto const& a : anim.animations) {
                json cfg = {
                    {"totalFrames", a.config.totalFrames},
                    {"rows",        a.config.rows},
                    {"columns",     a.config.columns},
                    {"startFrame",  a.config.startFrame},
                    {"endFrame",    a.config.endFrame},
                    {"fps",         a.config.fps},
                    {"loop",        a.config.loop}
                };

                json entry = {
                    {"name",            a.name},
                    {"textureKey",      a.textureKey},
                    {"spriteSheetPath", a.spriteSheetPath},
                    {"config",          cfg},
                    {"currentFrame",    a.currentFrame}
                };

                animations.push_back(entry);
            }

            return json{
                {"fps", anim.fps},
                {"loop", anim.loop},
                {"play", anim.play},
                {"frames", /* legacy frames array */ json::array()},
                {"animations", animations},
                {"activeAnimation", anim.ActiveAnimationIndex()}
            };
        }

        case ComponentTypeId::CT_RigidBodyComponent: {
            auto const& rb = static_cast<RigidBodyComponent const&>(component);
            return json{
                {"velocity_x", rb.velX},
                {"velocity_y", rb.velY},
                {"width", rb.width},
                {"height", rb.height}
            };
        }
        case ComponentTypeId::CT_InputComponents:
            return json::object();
        case ComponentTypeId::CT_PlayerComponent:
            return json::object();
        case ComponentTypeId::CT_PlayerHealthComponent:
        {
            auto const& hp = static_cast<PlayerHealthComponent const&>(component);
            return json{ {"playerHealth", hp.playerHealth}, {"playerMaxhealth", hp.playerMaxhealth} };
        }
        case ComponentTypeId::CT_PlayerAttackComponent:
        {
            auto const& atk = static_cast<PlayerAttackComponent const&>(component);
            return json{ {"damage", atk.damage}, {"attack_speed", atk.attack_speed} };
        }
        case ComponentTypeId::CT_EnemyComponent:
        case ComponentTypeId::CT_EnemyDecisionTreeComponent:
            return json::object();
        case ComponentTypeId::CT_EnemyAttackComponent: {
            auto const& atk = static_cast<EnemyAttackComponent const&>(component);
            return json{ {"damage", atk.damage}, {"attack_speed", atk.attack_speed} };
        }
        case ComponentTypeId::CT_EnemyHealthComponent: {
            auto const& hp = static_cast<EnemyHealthComponent const&>(component);
            return json{ {"enemyHealth", hp.enemyHealth}, {"enemyMaxhealth", hp.enemyMaxhealth} };
        }
        case ComponentTypeId::CT_EnemyTypeComponent: {
            auto const& type = static_cast<EnemyTypeComponent const&>(component);
            std::string typeStr = "physical";
            if (type.Etype == EnemyTypeComponent::EnemyType::ranged)
                typeStr = "ranged";
            return json{ {"type", typeStr} };
        }
        case ComponentTypeId::CT_HitBoxComponent: {
            auto const& hit = static_cast<HitBoxComponent const&>(component);
            return json{
                {"width",   hit.width},
                {"height",  hit.height},
                {"duration",hit.duration}
            };
        }
        default:
            break;
        }
        return json::object();
    }

    /*************************************************************************************
     \brief Save a set of GOCs to a level JSON file.
     \param filename  Target JSON path to write to (folders are created if missing).
     \param objects   Non-owning pointers to GOCs to serialize (must still be owned by the factory).
     \param levelName Optional level name; if empty, the filename stem is used.
     \return true on successful write; false if file I/O fails.
     \details
       - Builds the JSON structure:
           { "Level": { "name": "<levelName>", "GameObjects": [ { ... }, ... ] } }
       - Skips objects not owned by the factory or those pending deletion.
       - For each object:
           - Writes "name" (if non-empty) and "layer".
           - Serializes each component via SerializeComponentToJson().
       - Pretty-prints with 2-space indentation.
       - Caches last-saved level metadata (path/name/object list) for editor convenience.
    *************************************************************************************/
    bool GameObjectFactory::SaveLevelInternal(const std::string& filename, const std::vector<GOC*>& objects,
        const std::string& levelName)
    {
        json root = json::object();                 // Create the root JSON object: { }
        auto& level = root["Level"];                // Create/access child "Level" node (creates it if missing)
        level = json::object();                     // Ensure "Level" itself is an object: { "Level": { } }

        std::string finalName = levelName;          // Copy the provided level name
        if (finalName.empty()) {
            std::filesystem::path p(filename);
            finalName = p.stem().string();          //  default to its stem (filename without extension)
        }
        if (!finalName.empty())                     // If we have a non-empty name by now…
            level["name"] = finalName;              //   …write it into JSON: "Level": { "name": "<finalName>" }

        auto& array = level["GameObjects"];         // Create/access the "GameObjects" array node
        array = json::array();                      // Make sure it's an array: "GameObjects": [ ]

        // Iterate every object the caller asked to save
        for (GOC* obj : objects) {
            if (!obj) continue;                     // Skip null entries defensively

            // Only save objects still tracked by the factory and not pending deletion
            auto it = GameObjectIdMap.find(obj->ObjectId);
            if (it == GameObjectIdMap.end() || it->second.get() != obj)
                continue;                           // Skip if not owned by factory 
            if (ObjectsToBeDeleted.count(obj->ObjectId))
                continue;                           // Skip if flagged for deletion (deferred)

            json objJson = json::object();          // Build: { } for this one object
            if (!obj->ObjectName.empty())           // If it has a non-empty name…
                objJson["name"] = obj->ObjectName;  //  write "name": "<GOC name>"
            objJson["layer"] = obj->GetLayerName(); // Always write the object's layer (string)

            json comps = json::object();            // Container for this object's components

            // Serialize each component attached to the object
            for (auto const& up : obj->Components) {
                if (!up) continue;                  // Skip empty component slots
                const GameComponent& comp = *up;    // Reference to concrete component
                std::string compName = ComponentNameFromId(comp.GetTypeId());
                if (compName.empty())
                    continue;                       // Skip if we don't know the component's JSON name
                comps[compName] = SerializeComponentToJson(comp); // Write its JSON blob
            }

            objJson["Components"] = std::move(comps); // Attach "Components": { ... } to this object
            array.push_back(std::move(objJson));      // Append object to "GameObjects" array
        }

        std::filesystem::path outputPath(filename);   // Normalize/hold output path
        std::error_code ec;                           // Non-throwing error code holder
        if (outputPath.has_parent_path())
            std::filesystem::create_directories(outputPath.parent_path(), ec); // Ensure folders exist

        std::ofstream out(outputPath);                // Open the output file for write
        if (!out.is_open())
            return false;                             // Fail if we couldn't open it

        out << std::setw(2) << root;                  // Pretty-print JSON with 2-space indent
        if (!out.good())
            return false;                             // Fail if stream went bad during write

        LastLevelCache = objects;                     // Cache snapshot of the objects we just saved (non-owning)
        LastLevelNameCache = finalName;               // Cache the level name
        LastLevelPathCache = outputPath;              // Cache the level path
        return true;                                  // Success
    }

    /*************************************************************************************
     \brief Save a specific subset of game objects into a level file.
     \param filename  Target JSON file path to write to.
     \param objects   List of non-owning GOC* pointers to serialize (must belong to the factory).
     \param levelName Optional level name; overrides the default filename stem.
     \return true if the level was successfully saved; false on failure.
     \details
       - Acts as a convenience wrapper for SaveLevelInternal().
       - The caller explicitly specifies which objects to write (e.g., a selection or layer subset).
       - SaveLevelInternal handles JSON formatting, component serialization, and directory creation.
    *************************************************************************************/
    bool GameObjectFactory::SaveLevel(const std::string& filename, const std::vector<GOC*>& objects,
        const std::string& levelName)
    {
        return SaveLevelInternal(filename, objects, levelName);
    }

    /*************************************************************************************
     \brief Save all active (non-deleted) game objects currently owned by the factory.
     \param filename  Target JSON file path to write to.
     \param levelName Optional level name; defaults to filename stem if empty.
     \return true if save succeeded; false if file I/O failed.
     \details
       - Iterates through GameObjectIdMap and collects all valid, non-pending-deletion GOCs.
       - Builds a temporary vector of pointers to pass to SaveLevelInternal().
       - Useful for autosaving or full-level export operations from the editor/runtime.
    *************************************************************************************/
    bool GameObjectFactory::SaveLevel(const std::string& filename, const std::string& levelName)
    {
        std::vector<GOC*> active;
        active.reserve(GameObjectIdMap.size());
        for (auto const& [id, ptr] : GameObjectIdMap) {
            (void)id;
            if (!ptr) continue;
            if (ObjectsToBeDeleted.count(ptr->ObjectId)) continue;
            active.push_back(ptr.get());
        }
        return SaveLevelInternal(filename, active, levelName);
    }

    /*************************************************************************************
      \brief Assigns a unique ID to the GOC and registers it in the id→object map.
      \param gameObject Newly constructed GOC to identify and take ownership of.
      \param fixedId    Optional explicit id to reuse (used by some loaders/undo systems).
      \return Non-owning pointer to the now-registered GOC.
      \details
        - Increments the running ID counter when no fixedId is used.
        - Moves the unique_ptr into GameObjectIdMap, which now owns the object.
    *************************************************************************************/
    GOC* GameObjectFactory::IdGameObject(std::unique_ptr<GOC> gameObject,
        std::optional<GOCId> fixedId) {
        if (!gameObject)
            return nullptr;

        bool reuseRequested = fixedId.has_value() && fixedId.value() != 0;
        GOCId assignedId = 0;

        if (reuseRequested)
        {
            assignedId = fixedId.value();
            auto existing = GameObjectIdMap.find(assignedId);
            if (existing != GameObjectIdMap.end())
            {
                // If the previous object is still pending deletion, finish removing it so
                // the ID can be reused. Otherwise, fall back to issuing a new ID.
                if (ObjectsToBeDeleted.count(assignedId))
                {
                    LayerData.RemoveObject(assignedId);
                    GameObjectIdMap.erase(existing);
                }
                else
                {
                    reuseRequested = false;
                }
            }

            if (reuseRequested)
            {
                ObjectsToBeDeleted.erase(assignedId);
                if (assignedId > LastGameObjectId)
                    LastGameObjectId = assignedId;
            }
        }

        if (!reuseRequested)
        {
            assignedId = ++LastGameObjectId;
        }

        gameObject->ObjectId = assignedId;
        GOC* raw = gameObject.get();
        LayerData.AssignToLayer(raw->ObjectId, raw->GetLayerName());
        GameObjectIdMap.emplace(assignedId, std::move(gameObject));
        return raw;
    }

    void GameObjectFactory::OnLayerChanged(GOC& object, std::string_view previousLayer)
    {
        (void)previousLayer;
        LayerData.AssignToLayer(object.ObjectId, object.GetLayerName());
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
        LayerData.RemoveObject(gameObject->ObjectId);
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
                LayerData.RemoveObject(id);
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
                LayerData.RemoveObject(id);
                GameObjectIdMap.erase(it); // unique_ptr destruction happens here
            }
        }
        ObjectsToBeDeleted.clear();
        // Destroy any remaining tracked game objects and release their components
        for (auto const& [id, _] : GameObjectIdMap) {
            (void)_;
            LayerData.RemoveObject(id);
        }
        // Destroy any remaining tracked game objects and release their components
        GameObjectIdMap.clear();

        LayerData.Clear();

        // Component creators are owned by the factory; release them to avoid leak reports
        ComponentMap.clear();

        LastLevelCache.clear();
        LastLevelNameCache.clear();
        LastLevelPathCache.clear();

        LastGameObjectId = 0;

        if (FACTORY == this)
            FACTORY = nullptr;
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

    /*************************************************************************************
      \brief Deserialize a single component from a JSON object and apply it to an instance.
      \param component Target component instance (already created and attached).
      \param data      JSON blob produced by SerializeComponentToJson().
      \note  Missing keys are ignored and leave fields unchanged.
    *************************************************************************************/
    void GameObjectFactory::DeserializeComponentFromJson(GameComponent& component,
        const json& data) const
    {
        if (!data.is_object())
            return;

        auto readFloat = [&](const char* key, float& out)
            {
                auto it = data.find(key);
                if (it != data.end() && it->is_number())
                    out = static_cast<float>(it->get<double>());
            };

        auto readInt = [&](const char* key, int& out)
            {
                auto it = data.find(key);
                if (it != data.end() && it->is_number_integer())
                    out = it->get<int>();
            };

        auto readBool = [&](const char* key, bool& out)
            {
                auto it = data.find(key);
                if (it != data.end() && it->is_boolean())
                    out = it->get<bool>();
            };

        auto readString = [&](const char* key, std::string& out)
            {
                auto it = data.find(key);
                if (it != data.end() && it->is_string())
                    out = it->get<std::string>();
            };

        switch (component.GetTypeId())
        {
        case ComponentTypeId::CT_TransformComponent:
        {
            auto& tr = static_cast<TransformComponent&>(component);
            readFloat("x", tr.x);
            readFloat("y", tr.y);
            readFloat("rot", tr.rot);
            break;
        }
        case ComponentTypeId::CT_RenderComponent:
        {
            auto& rc = static_cast<RenderComponent&>(component);
            readFloat("w", rc.w);
            readFloat("h", rc.h);
            readFloat("r", rc.r);
            readFloat("g", rc.g);
            readFloat("b", rc.b);
            readFloat("a", rc.a);
            readBool("visible", rc.visible);
            readString("texture_key", rc.texture_key);
            readString("texture_path", rc.texture_path);
            break;
        }
        case ComponentTypeId::CT_CircleRenderComponent:
        {
            auto& cc = static_cast<CircleRenderComponent&>(component);
            readFloat("radius", cc.radius);
            readFloat("r", cc.r);
            readFloat("g", cc.g);
            readFloat("b", cc.b);
            readFloat("a", cc.a);
            break;
        }
        case ComponentTypeId::CT_SpriteComponent:
        {
            auto& sp = static_cast<SpriteComponent&>(component);
            readString("texture_key", sp.texture_key);
            readString("path", sp.path);
            break;
        }
        case ComponentTypeId::CT_SpriteAnimationComponent:
        {
            auto& anim = static_cast<SpriteAnimationComponent&>(component);

            readFloat("fps", anim.fps);

            if (auto it = data.find("loop"); it != data.end())
                anim.loop = it->get<bool>();

            if (auto it = data.find("play"); it != data.end())
                anim.play = it->get<bool>();

            // legacy frames
            anim.frames.clear();
            if (auto it = data.find("frames"); it != data.end() && it->is_array()) {
                for (auto const& f : *it) {
                    SpriteAnimationFrame fr;
                    if (f.contains("texture_key")) fr.texture_key = f["texture_key"];
                    if (f.contains("path"))        fr.path = f["path"];
                    anim.frames.push_back(fr);
                }
            }

            // NEW: sprite-sheet animations
            anim.animations.clear();
            if (auto it = data.find("animations"); it != data.end() && it->is_array())
            {
                for (auto const& a : *it)
                {
                    SpriteAnimationComponent::SpriteSheetAnimation sheet{};

                    if (a.contains("name"))            sheet.name = a["name"];
                    if (a.contains("textureKey"))      sheet.textureKey = a["textureKey"];
                    if (a.contains("spriteSheetPath")) sheet.spriteSheetPath = a["spriteSheetPath"];

                    if (a.contains("config"))
                    {
                        auto const& c = a["config"];
                        sheet.config.totalFrames = c["totalFrames"];
                        sheet.config.rows = c["rows"];
                        sheet.config.columns = c["columns"];
                        sheet.config.startFrame = c["startFrame"];
                        sheet.config.endFrame = c["endFrame"];
                        sheet.config.fps = c["fps"];
                        sheet.config.loop = c["loop"];
                    }

                    if (a.contains("currentFrame"))
                        sheet.currentFrame = a["currentFrame"];

                    sheet.accumulator = 0.0f;
                    sheet.textureId = 0; // will be lazily loaded

                    anim.animations.push_back(sheet);
                }
            }

            if (auto it = data.find("activeAnimation"); it != data.end())
                anim.activeAnimation = it->get<int>();

            break;
        }


        case ComponentTypeId::CT_RigidBodyComponent:
        {
            auto& rb = static_cast<RigidBodyComponent&>(component);
            readFloat("velocity_x", rb.velX);
            readFloat("velocity_y", rb.velY);
            readFloat("width", rb.width);
            readFloat("height", rb.height);
            break;
        }
        case ComponentTypeId::CT_PlayerHealthComponent:
        {
            auto& hp = static_cast<PlayerHealthComponent&>(component);
            readInt("playerHealth", hp.playerHealth);
            readInt("playerMaxhealth", hp.playerMaxhealth);
            break;
        }
        case ComponentTypeId::CT_PlayerAttackComponent:
        {
            auto& atk = static_cast<PlayerAttackComponent&>(component);
            readInt("damage", atk.damage);
            readFloat("attack_speed", atk.attack_speed);
            break;
        }
        case ComponentTypeId::CT_EnemyAttackComponent:
        {
            auto& atk = static_cast<EnemyAttackComponent&>(component);
            readInt("damage", atk.damage);
            readFloat("attack_speed", atk.attack_speed);
            if (atk.hitbox)
            {
                readFloat("hitwidth", atk.hitbox->width);
                readFloat("hitheight", atk.hitbox->height);
                readFloat("hitduration", atk.hitbox->duration);
            }
            break;
        }
        case ComponentTypeId::CT_EnemyHealthComponent:
        {
            auto& hp = static_cast<EnemyHealthComponent&>(component);
            readInt("enemyHealth", hp.enemyHealth);
            readInt("enemyMaxhealth", hp.enemyMaxhealth);
            break;
        }
        case ComponentTypeId::CT_EnemyTypeComponent:
        {
            auto& type = static_cast<EnemyTypeComponent&>(component);
            std::string typeStr;
            readString("type", typeStr);
            if (typeStr == "ranged")
                type.Etype = EnemyTypeComponent::EnemyType::ranged;
            else
                type.Etype = EnemyTypeComponent::EnemyType::physical;
            break;
        }
        case ComponentTypeId::CT_HitBoxComponent:
        {
            auto& hit = static_cast<HitBoxComponent&>(component);
            readFloat("width", hit.width);
            readFloat("height", hit.height);
            readFloat("duration", hit.duration);
            break;
        }
        case ComponentTypeId::CT_EnemyComponent:
        case ComponentTypeId::CT_PlayerComponent:
        case ComponentTypeId::CT_EnemyDecisionTreeComponent:
        case ComponentTypeId::CT_InputComponents:
        case ComponentTypeId::CT_AudioComponent:
        default:
            break;
        }
    }

    /*************************************************************************************
      \brief Take a full JSON snapshot of a single GOC for undo/redo.
      \param object The object to snapshot.
      \return JSON blob describing name, layer, and all serializable components.
      \note   Stores an internal "_undo_id" field with the original object ID.
    *************************************************************************************/
    json GameObjectFactory::SnapshotGameObject(const GOC& object) const
    {
        json objJson = json::object();
        objJson["_undo_id"] = object.ObjectId;
        if (!object.ObjectName.empty())
            objJson["name"] = object.ObjectName;
        objJson["layer"] = object.LayerName;

        json comps = json::object();
        for (auto const& up : object.Components)
        {
            if (!up)
                continue;
            const GameComponent& comp = *up;
            std::string compName = ComponentNameFromId(comp.GetTypeId());
            if (compName.empty())
                continue;
            comps[compName] = SerializeComponentToJson(comp);
        }

        objJson["Components"] = std::move(comps);
        return objJson;
    }

    /*************************************************************************************
     \brief Internal helper to instantiate a GOC from a JSON snapshot.
     \param data Snapshot produced by SnapshotGameObject().
     \return Newly created and initialized GOC*, or nullptr on failure.
     \details
       - Rebuilds name, layer, and all serializable components.
       - Always assigns a **fresh ID**; we keep _undo_id only for debugging.
   *************************************************************************************/
    GOC* GameObjectFactory::InstantiateFromSnapshotInternal(const json& data)
    {
        if (!data.is_object())
            return nullptr;

        // 1. Build a brand–new GOC from the snapshot
        auto goc = std::make_unique<GOC>();

        // Name
        if (auto it = data.find("name"); it != data.end() && it->is_string())
            goc->SetObjectName(it->get<std::string>());

        // Layer
        if (auto it = data.find("layer"); it != data.end() && it->is_string())
            goc->SetLayerName(it->get<std::string>());

        // We still read _undo_id (for debugging / logging if needed),
        // but we no longer try to reuse it when assigning the new ID.
        if (auto it = data.find("_undo_id");
            it != data.end() && it->is_number_unsigned())
        {
            (void)it; // currently unused, kept only for possible future diagnostics
        }

        // Components
        if (auto compIt = data.find("Components");
            compIt != data.end() && compIt->is_object())
        {
            for (auto& [compName, compData] : compIt->items())
            {
                auto creatorIt = ComponentMap.find(compName);
                if (creatorIt == ComponentMap.end())
                    continue;

                ComponentCreator* creator = creatorIt->second.get();
                if (!creator)
                    continue;

                std::unique_ptr<GameComponent> comp(creator->Create());
                if (!comp)
                    continue;

                if (compData.is_object())
                    DeserializeComponentFromJson(*comp, compData);

                goc->AddComponent(creator->TypeId, std::move(comp));
            }
        }

        // 2. Register with a NEW ID (do NOT pass desiredId)
        GOC* raw = IdGameObject(std::move(goc));

        // 3. Initialize so components can hook up internal pointers (like hitbox, etc.)
        if (raw)
            raw->initialize();

        return raw;
    }


    GOC* GameObjectFactory::InstantiateFromSnapshot(const json& data)
    {
        return InstantiateFromSnapshotInternal(data);
    }

    void GameObjectFactory::CancelDestroy(GOCId id)
    {
        ObjectsToBeDeleted.erase(id);
    }

} // namespace Framework
