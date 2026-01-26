/*********************************************************************************************
 \file      Layer.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the Layer and LayerManager classes, which provide object grouping
            and filtering within the engine. Layers allow systems such as rendering or
            collision detection to operate on subsets of GameObjects.

 \details
            The Layer class maintains a list of GameObjectComposition IDs belonging to
            the same logical group (e.g., Background, Gameplay, UI). The LayerManager
            oversees the creation, retrieval, and cleanup of layers, ensuring each object
            maps to at most one layer at a time. Layers are created on demand and pruned
            when empty.

            Key Features:
            - Efficient mapping between objects and layers.
            - Automatic default layer assignment.
            - Utility for per-layer iteration, rendering, or collision checks.
            - Safe removal and automatic cleanup of empty layers.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Layer.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <iostream>
#include <string>
#include <utility>

#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework {

    //--------------------------------------------------------------------------------------
    // Helpers (anonymous namespace limits visibility to this translation unit)
    //--------------------------------------------------------------------------------------
    namespace {
        constexpr LayerKey kDefaultLayer{ LayerGroup::Gameplay, 0 };

        constexpr std::size_t ToIndex(LayerGroup group)
        {
            return static_cast<std::size_t>(group);
        }

        std::string_view TrimView(std::string_view value)
        {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
                value.remove_prefix(1);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
                value.remove_suffix(1);
            return value;
        }

        LayerGroup ParseLayerGroup(std::string_view name, bool& ok)
        {
            std::string lowered;
            lowered.resize(name.size());
            std::transform(name.begin(), name.end(), lowered.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lowered == "background")
            {
                ok = true;
                return LayerGroup::Background;
            }
            if (lowered == "gameplay")
            {
                ok = true;
                return LayerGroup::Gameplay;
            }
            if (lowered == "foreground")
            {
                ok = true;
                return LayerGroup::Foreground;
            }
            if (lowered == "ui")
            {
                ok = true;
                return LayerGroup::UI;
            }
            if (lowered == "default")
            {
                ok = true;
                return LayerGroup::Gameplay;
            }

            ok = false;
            return LayerGroup::Gameplay;
        }

        int ParseSublayer(std::string_view value)
        {

            value = TrimView(value);
            if (value.empty())
                return 0;

            int parsed = 0;
            auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec != std::errc())
                return 0;

            return std::clamp(parsed, 0, kMaxLayerSublayer);
        }
    }

    bool operator==(const LayerKey& lhs, const LayerKey& rhs)
    {
        return lhs.group == rhs.group && lhs.sublayer == rhs.sublayer;
    }

    std::size_t LayerKeyHasher::operator()(const LayerKey& key) const noexcept
    {
        return (static_cast<std::size_t>(key.group) << 8) ^ static_cast<std::size_t>(key.sublayer);
    }

    const char* LayerGroupName(LayerGroup group)
    {
        switch (group)
        {
        case LayerGroup::Background:
            return "Background";
        case LayerGroup::Gameplay:
            return "Gameplay";
        case LayerGroup::Foreground:
            return "Foreground";
        case LayerGroup::UI:
            return "UI";
        default:
            return "Background";
        }
    }

    LayerKey ParseLayerName(const std::string& name)
    {

        std::string_view nameView = TrimView(name);
        if (nameView.empty())
            return kDefaultLayer;

        const auto split = nameView.find(':');
        const std::string_view groupPart = TrimView(nameView.substr(0, split));
        const std::string_view sublayerPart = (split == std::string_view::npos)
            ? std::string_view{}
        : nameView.substr(split + 1);

        bool ok = false;
        LayerGroup group = ParseLayerGroup(groupPart, ok);
        if (!ok)
            return kDefaultLayer;

        int sublayer = ParseSublayer(sublayerPart);
        return LayerKey{ group, sublayer };
    }

    std::string LayerNameFromKey(LayerKey key)
    {
        return std::string{ LayerGroupName(key.group) } + ":" + std::to_string(std::clamp(key.sublayer, 0, kMaxLayerSublayer));
    }

    std::string NormalizeLayerName(const std::string& name)
    {
        return LayerNameFromKey(ParseLayerName(name));
    }

    //--------------------------------------------------------------------------------------
    // LayerVisibility
    //--------------------------------------------------------------------------------------
    LayerVisibility::LayerVisibility()
    {
        EnableAll();
    }

    bool LayerVisibility::IsGroupEnabled(LayerGroup group) const
    {
        return groupEnabled[ToIndex(group)];
    }

    bool LayerVisibility::IsSublayerEnabled(LayerGroup group, int sublayer) const
    {
        if (sublayer < 0 || sublayer > kMaxLayerSublayer)
            return false;
        return sublayerEnabled[ToIndex(group)][static_cast<std::size_t>(sublayer)];
    }

    bool LayerVisibility::IsLayerEnabled(LayerKey key) const
    {
        return IsGroupEnabled(key.group) && IsSublayerEnabled(key.group, key.sublayer);
    }

    void LayerVisibility::SetGroupEnabled(LayerGroup group, bool enabled)
    {
        groupEnabled[ToIndex(group)] = enabled;
    }

    void LayerVisibility::SetSublayerEnabled(LayerGroup group, int sublayer, bool enabled)
    {
        if (sublayer < 0 || sublayer > kMaxLayerSublayer)
            return;
        sublayerEnabled[ToIndex(group)][static_cast<std::size_t>(sublayer)] = enabled;
    }

    void LayerVisibility::EnableAll()
    {
        for (std::size_t groupIndex = 0; groupIndex < groupEnabled.size(); ++groupIndex)
        {
            groupEnabled[groupIndex] = true;
            for (auto& enabled : sublayerEnabled[groupIndex])
            {
                enabled = true;
            }
        }

    }

    void LayerVisibility::EnableOnly(LayerKey key)
    {
        for (std::size_t groupIndex = 0; groupIndex < groupEnabled.size(); ++groupIndex)
        {
            groupEnabled[groupIndex] = false;
            for (auto& enabled : sublayerEnabled[groupIndex])
            {
                enabled = false;
            }
        }
        const std::size_t groupIndex = ToIndex(key.group);
        groupEnabled[groupIndex] = true;
        const int clampedSublayer = std::clamp(key.sublayer, 0, kMaxLayerSublayer);
        sublayerEnabled[groupIndex][static_cast<std::size_t>(clampedSublayer)] = true;
    }

    //--------------------------------------------------------------------------------------
    // Layer
    //--------------------------------------------------------------------------------------

    /**
     * \brief Construct a Layer with a given key.
     * \note Name is stored as-is; callers should use NormalizeLayerName() when appropriate.
     */
    Layer::Layer(LayerKey key)
        : LayerName(LayerNameFromKey(key))
        , KeyValue(key)
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
    Layer& LayerManager::EnsureLayer(const std::string& layerName)
    {
        LayerKey key = ParseLayerName(layerName);
        auto it = LayersByKey.find(key);
        if (it == LayersByKey.end())
        {
            // Create an empty layer with this key.
            it = LayersByKey.emplace(key, Layer{ key }).first;
        }
        return it->second;
    }

    /**
     * \brief Find a layer by name (read-only).
     * \return nullptr if the layer does not exist.
     */
    const Layer* LayerManager::FindLayer(const std::string& layerName) const
    {
        LayerKey key = ParseLayerName(layerName);
        auto it = LayersByKey.find(key);
        return it == LayersByKey.end() ? nullptr : &it->second;
    }

    /**
     * \brief Find a layer by name (mutable).
     * \return nullptr if the layer does not exist.
     */
    Layer* LayerManager::FindLayer(const std::string& layerName)
    {
        LayerKey key = ParseLayerName(layerName);
        auto it = LayersByKey.find(key);
        return it == LayersByKey.end() ? nullptr : &it->second;
    }

    /**
     * \brief Assign an object to a layer (moving it from any previous layer if needed).
     * \details
     *  - If the object is already in the desired layer, this is a no-op.
     *  - Otherwise, remove from old layer and add to the new one.
     *  - Empty layers are automatically pruned.
     */
    void LayerManager::AssignToLayer(GOCId id, const std::string& layerName)
    {
        LayerKey key = ParseLayerName(layerName);

        // If already mapped, and the layer is the same, early-out.
        auto existing = ObjectToLayer.find(id);
        if (existing != ObjectToLayer.end())
        {
            if (existing->second == key)
                return;
            // Move from old layer to new layer.
            RemoveFromLayer(id, LayerNameFromKey(existing->second));
        }

        // Ensure destination layer exists and insert.
        Layer& layer = EnsureLayer(LayerNameFromKey(key));
        layer.Add(id);
        ObjectToLayer[id] = key;
    }

    /**
     * \brief Remove an object from a specific layer.
     * \details
     *  - No error if layer or object mapping doesn't exist.
     *  - If a layer becomes empty, it is erased from the map to keep things tidy.
     */
    void LayerManager::RemoveFromLayer(GOCId id, const std::string& layerName)
    {
        LayerKey key = ParseLayerName(layerName);

        // Remove from the named layer if present.
        auto it = LayersByKey.find(key);
        if (it != LayersByKey.end())
        {
            it->second.Remove(id);
            if (it->second.Objects().empty())
            {
                // Prune empty layers to keep LayerNames() compact/accurate.
                LayersByKey.erase(it);
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

        RemoveFromLayer(id, LayerNameFromKey(it->second));
    }

    /**
     * \brief Query the layer name for an object.
     * \return The layer name, or "Default" if the object has no mapping.
     */
    std::string LayerManager::LayerFor(GOCId id) const
    {
        auto it = ObjectToLayer.find(id);
        if (it == ObjectToLayer.end())
            return LayerNameFromKey(kDefaultLayer);
        return LayerNameFromKey(it->second);
    }

    LayerKey LayerManager::LayerKeyFor(GOCId id) const
    {
        auto it = ObjectToLayer.find(id);
        if (it == ObjectToLayer.end())
            return kDefaultLayer;
        return it->second;
    }

    /**
     * \brief Get a copy of all known layer names.
     * \note Order is unspecified (unordered_map iteration order).
     */
    std::vector<std::string> LayerManager::LayerNames() const
    {
        std::vector<std::string> names;
        names.reserve(LayersByKey.size());
        for (auto const& [key, layer] : LayersByKey)
        {
            (void)key;
            names.push_back(layer.Name());
        }
        std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b)
            {
                const LayerKey keyA = ParseLayerName(a);
                const LayerKey keyB = ParseLayerName(b);
                if (keyA.group != keyB.group)
                    return static_cast<int>(keyA.group) < static_cast<int>(keyB.group);
                return keyA.sublayer < keyB.sublayer;
            });

        return names;
    }

    bool LayerManager::IsLayerEnabled(const std::string& layerName) const
    {
        return visibility.IsLayerEnabled(ParseLayerName(layerName));
    }

    bool LayerManager::IsLayerEnabled(LayerKey key) const
    {
        return visibility.IsLayerEnabled(key);
    }

    void LayerManager::LogVisibilitySummary(std::string_view label) const
    {
        std::cout << "[LayerManager] Visibility Summary (" << label << ")\n";
        for (std::size_t groupIndex = 0; groupIndex < static_cast<std::size_t>(LayerGroup::Count); ++groupIndex)
        {
            const auto group = static_cast<LayerGroup>(groupIndex);
            std::cout << "  Group " << LayerGroupName(group)
                << " enabled=" << (visibility.IsGroupEnabled(group) ? "true" : "false") << "\n";
            for (int sublayer = 0; sublayer <= kMaxLayerSublayer; ++sublayer)
            {
                if (visibility.IsSublayerEnabled(group, sublayer))
                {
                    std::cout << "    Sublayer " << sublayer << " enabled\n";
                }
            }
        }

        std::cout << "  Layers with objects:\n";
        for (auto const& [key, layer] : LayersByKey)
        {
            std::cout << "    " << layer.Name() << " (" << layer.Objects().size() << " objects)\n";
        }
    }
    /**
     * \brief Clear all layers and object-to-layer mappings.
     * \warning This forgets all assignments; objects will appear as "Default" until reassigned.
     */
    void LayerManager::Clear()
    {
        LayersByKey.clear();
        ObjectToLayer.clear();
    }

}
