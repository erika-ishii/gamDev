#include "Layer.h"
#include <algorithm>
#include <utility>

namespace Framework {

    //--------------------------------------------------------------------------------------
    // Helpers (anonymous namespace limits visibility to this translation unit)
    //--------------------------------------------------------------------------------------
    namespace {
        // Default bucket for objects that don't specify a layer.
        constexpr std::string_view kDefaultLayerName{ "Default" };

        /**
         * \brief Normalize a layer name for storage/lookup.
         * Empty names collapse to "Default" so we never store empty keys.
         */
        std::string NormalizeLayerName(std::string_view name)
        {
            if (name.empty())
             return std::string{ kDefaultLayerName };
            return std::string{ name };
        }
    }

    //--------------------------------------------------------------------------------------
    // Layer
    //--------------------------------------------------------------------------------------

    /**
     * \brief Construct a Layer with a given name.
     * \note Name is stored as-is; callers should use NormalizeLayerName() when appropriate.
     */
    Layer::Layer(std::string name)
        : LayerName(std::move(name))
    {
    }

    /**
     * \brief Rename this layer.
     * \warning Callers (LayerManager) must ensure maps are updated if renaming keys.
     */
    void Layer::SetName(std::string name)
    {
        LayerName = std::move(name);
    }

    /**
     * \brief Add an object ID to the layer if not already present.
     * \note Keeps a simple vector of IDs; duplicates are avoided by Contains() check.
     */
    void Layer::Add(GOCId id)
    {
        if (!Contains(id))
            ObjectIds.push_back(id);
    }

    /**
     * \brief Remove an object ID from the layer (no-op if not present).
     * \note Uses erase-remove idiom for O(n) removal.
     */
    void Layer::Remove(GOCId id)
    {
        auto it = std::remove(ObjectIds.begin(), ObjectIds.end(), id);
        ObjectIds.erase(it, ObjectIds.end());
    }

    /**
     * \brief Check whether an object ID is part of this layer.
     */
    bool Layer::Contains(GOCId id) const
    {
        return std::find(ObjectIds.begin(), ObjectIds.end(), id) != ObjectIds.end();
    }

    /**
     * \brief Remove all object IDs from this layer.
     */
    void Layer::Clear()
    {
        ObjectIds.clear();
    }

    //--------------------------------------------------------------------------------------
    // LayerManager
    //--------------------------------------------------------------------------------------

    /**
     * \brief Find an existing layer or create a new one if not found.
     * \return Reference to the ensured layer.
     */
    Layer& LayerManager::EnsureLayer(std::string_view layerName)
    {
        auto key = NormalizeLayerName(layerName);
        auto it = LayersByName.find(key);
        if (it == LayersByName.end())
        {
            // Create an empty layer with this name.
            it = LayersByName.emplace(key, Layer{ key }).first;
        }
        return it->second;
    }

    /**
     * \brief Find a layer by name (read-only).
     * \return nullptr if the layer does not exist.
     */
    const Layer* LayerManager::FindLayer(std::string_view layerName) const
    {
        auto key = NormalizeLayerName(layerName);
        auto it = LayersByName.find(key);
        return it == LayersByName.end() ? nullptr : &it->second;
    }

    /**
     * \brief Find a layer by name (mutable).
     * \return nullptr if the layer does not exist.
     */
    Layer* LayerManager::FindLayer(std::string_view layerName)
    {
        auto key = NormalizeLayerName(layerName);
        auto it = LayersByName.find(key);
        return it == LayersByName.end() ? nullptr : &it->second;
    }

    /**
     * \brief Assign an object to a layer (moving it from any previous layer if needed).
     * \details
     *  - If the object is already in the desired layer, this is a no-op.
     *  - Otherwise, remove from old layer and add to the new one.
     *  - Empty layers are automatically pruned.
     */
    void LayerManager::AssignToLayer(GOCId id, std::string_view layerName)
    {
        auto key = NormalizeLayerName(layerName);

        // If already mapped, and the layer is the same, early-out.
        auto existing = ObjectToLayer.find(id);
        if (existing != ObjectToLayer.end())
        {
            if (existing->second == key)
                return;
            // Move from old layer to new layer.
            RemoveFromLayer(id, existing->second);
        }

        // Ensure destination layer exists and insert.
        Layer& layer = EnsureLayer(key);
        layer.Add(id);
        ObjectToLayer[id] = key;
    }

    /**
     * \brief Remove an object from a specific layer.
     * \details
     *  - No error if layer or object mapping doesn't exist.
     *  - If a layer becomes empty, it is erased from the map to keep things tidy.
     */
    void LayerManager::RemoveFromLayer(GOCId id, std::string_view layerName)
    {
        auto key = NormalizeLayerName(layerName);

        // Remove from the named layer if present.
        auto it = LayersByName.find(key);
        if (it != LayersByName.end())
        {
            it->second.Remove(id);
            if (it->second.Objects().empty())
            {
                // Prune empty layers to keep LayerNames() compact/accurate.
                LayersByName.erase(it);
            }
        }

        // Remove reverse mapping if it matches this layer.
        auto mapIt = ObjectToLayer.find(id);
        if (mapIt != ObjectToLayer.end() && mapIt->second == key)
        {
            ObjectToLayer.erase(mapIt);
        }
    }

    /**
     * \brief Remove an object from whichever layer it's currently assigned to.
     * \note Safe to call for objects without a layer mapping.
     */
    void LayerManager::RemoveObject(GOCId id)
    {
        auto it = ObjectToLayer.find(id);
        if (it == ObjectToLayer.end())
            return;

        RemoveFromLayer(id, it->second);
    }

    /**
     * \brief Query the layer name for an object.
     * \return The layer name, or "Default" if the object has no mapping.
     */
    std::string LayerManager::LayerFor(GOCId id) const
    {
        auto it = ObjectToLayer.find(id);
        if (it == ObjectToLayer.end())
            return std::string{ kDefaultLayerName };
        return it->second;
    }

    /**
     * \brief Get a copy of all known layer names.
     * \note Order is unspecified (unordered_map iteration order).
     */
    std::vector<std::string> LayerManager::LayerNames() const
    {
        std::vector<std::string> names;
        names.reserve(LayersByName.size());
        for (auto const& [name, _] : LayersByName)
        {
            (void)_; // suppress unused warning
            names.push_back(name);
        }
        return names;
    }

    /**
     * \brief Clear all layers and object-to-layer mappings.
     * \warning This forgets all assignments; objects will appear as "Default" until reassigned.
     */
    void LayerManager::Clear()
    {
        LayersByName.clear();
        ObjectToLayer.clear();
    }

}
