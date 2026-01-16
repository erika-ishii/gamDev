/*********************************************************************************************
 \file      Layer.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the Layer and LayerManager classes used for organizing and grouping
            GameObjects into logical layers. Layers allow selective updates, rendering,
            and collision checks within the engine.

 \details
            The Layer class stores a list of GameObjectComposition IDs representing all
            objects that belong to that specific layer (e.g., Background, Gameplay, UI).
            The LayerManager maintains and manages all existing layers, providing lookup,
            assignment, and cleanup operations.

            Responsibilities:
            - Manage object-to-layer and layer-to-object mappings.
            - Provide quick access to per-layer object groups.
            - Automatically create and prune layers as needed.
            - Support per-layer filtering in rendering and physics systems.

            Example Usage:
            \code
                Framework::LayerManager manager;
                manager.AssignToLayer(playerId, "Gameplay");
                manager.AssignToLayer(uiElementId, "UI");
                auto* gameplayLayer = manager.FindLayer("Gameplay");
            \endcode

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstdint>

#include "Composition/Composition.h"

namespace Framework {
    enum class LayerGroup : std::uint8_t {
        Background = 0,
        Gameplay = 1,
        Foreground = 2,
        UI = 3,
        Count
    };

    constexpr int kMaxLayerSublayer = 20;

    struct LayerKey {
        LayerGroup group{ LayerGroup::Background };
        int sublayer{ 0 };
    };

    bool operator==(const LayerKey& lhs, const LayerKey& rhs);

    struct LayerKeyHasher {
        std::size_t operator()(const LayerKey& key) const noexcept;
    };

    const char* LayerGroupName(LayerGroup group);
    LayerKey ParseLayerName(std::string_view name);
    std::string NormalizeLayerName(std::string_view name);
    std::string LayerNameFromKey(LayerKey key);

    class LayerVisibility {
    public:
        LayerVisibility();

        bool IsGroupEnabled(LayerGroup group) const;
        bool IsSublayerEnabled(LayerGroup group, int sublayer) const;
        bool IsLayerEnabled(LayerKey key) const;

        void SetGroupEnabled(LayerGroup group, bool enabled);
        void SetSublayerEnabled(LayerGroup group, int sublayer, bool enabled);
        void EnableAll();
        void EnableOnly(LayerKey key);

    private:
        std::array<bool, static_cast<std::size_t>(LayerGroup::Count)> groupEnabled{};
        std::array<std::array<bool, kMaxLayerSublayer + 1>, static_cast<std::size_t>(LayerGroup::Count)> sublayerEnabled{};
    };

    /*****************************************************************************************
      \class Layer
      \brief Represents a collection of GameObjects grouped under a common logical name.

      Each Layer maintains a list of GameObjectComposition IDs belonging to it. Layers
      are primarily used by systems such as rendering, collision, or logic updates to
      selectively process subsets of entities.
    *****************************************************************************************/
    class Layer {
    public:
        Layer() = default;                      ///< Default constructor creates an empty layer.
        explicit Layer(LayerKey key);       ///< Constructs a layer with a given key.

        /*************************************************************************************
          \brief Retrieves the name of this layer.
          \return A constant reference to the layer name.
        *************************************************************************************/
        const std::string& Name() const { return LayerName; }
        const LayerKey& Key() const { return KeyValue; }

        /*************************************************************************************
          \brief Renames the layer.
          \param name  The new name to assign.
          \note  The LayerManager must handle renaming consistency in its maps.
        *************************************************************************************/
        void SetName(std::string name);

        /*************************************************************************************
          \brief Gets all object IDs currently assigned to this layer.
          \return A constant reference to the internal list of GameObject IDs.
        *************************************************************************************/
        const std::vector<GOCId>& Objects() const { return ObjectIds; }

        /*************************************************************************************
          \brief Adds an object ID to this layer if not already present.
          \param id  The GameObject ID to add.
        *************************************************************************************/
        void Add(GOCId id);

        /*************************************************************************************
          \brief Removes an object ID from this layer.
          \param id  The GameObject ID to remove.
          \note  No operation if the ID is not present.
        *************************************************************************************/
        void Remove(GOCId id);

        /*************************************************************************************
          \brief Checks if a specific object ID belongs to this layer.
          \param id  The GameObject ID to query.
          \return True if the object exists in this layer, false otherwise.
        *************************************************************************************/
        bool Contains(GOCId id) const;

        /*************************************************************************************
          \brief Removes all object IDs from this layer.
        *************************************************************************************/
        void Clear();

    private:
        std::string LayerName;        ///< Name of the layer (e.g., "Gameplay:0", "UI:3").
        LayerKey KeyValue{};
        std::vector<GOCId> ObjectIds; ///< List of GameObject IDs belonging to this layer.
    };

    /*****************************************************************************************
      \class LayerManager
      \brief Manages all active layers and their object mappings.

      The LayerManager provides centralized control for creating, querying, and assigning
      GameObjects to layers. It ensures that each GameObject belongs to at most one layer
      and automatically removes empty layers for efficiency.
    *****************************************************************************************/
    class LayerManager {
    public:
        /*************************************************************************************
          \brief Ensures a layer with the given name exists, creating it if necessary.
          \param layerName  The name of the desired layer.
          \return A reference to the ensured Layer instance.
        *************************************************************************************/
        Layer& EnsureLayer(std::string_view layerName);

        /*************************************************************************************
          \brief Finds a layer by name (read-only).
          \param layerName  The layer name to search for.
          \return Pointer to the layer, or nullptr if not found.
        *************************************************************************************/
        const Layer* FindLayer(std::string_view layerName) const;

        /*************************************************************************************
          \brief Finds a layer by name (modifiable).
          \param layerName  The layer name to search for.
          \return Pointer to the layer, or nullptr if not found.
        *************************************************************************************/
        Layer* FindLayer(std::string_view layerName);

        /*************************************************************************************
          \brief Assigns an object to a layer, removing it from any previous layer.
          \param id         The GameObject ID to assign.
          \param layerName  The target layer name.
        *************************************************************************************/
        void AssignToLayer(GOCId id, std::string_view layerName);

        /*************************************************************************************
          \brief Removes an object from a specific layer.
          \param id         The GameObject ID to remove.
          \param layerName  The layer name from which to remove it.
        *************************************************************************************/
        void RemoveFromLayer(GOCId id, std::string_view layerName);

        /*************************************************************************************
          \brief Removes an object from whichever layer it currently belongs to.
          \param id  The GameObject ID to remove.
        *************************************************************************************/
        void RemoveObject(GOCId id);

        /*************************************************************************************
          \brief Retrieves the layer name an object belongs to.
          \param id  The GameObject ID to query.
          \return The layer name, or "Default" if not assigned.
        *************************************************************************************/
        std::string LayerFor(GOCId id) const;
        LayerKey LayerKeyFor(GOCId id) const;

        /*************************************************************************************
          \brief Gets the names of all existing layers.
          \return A vector containing the names of all layers.
        *************************************************************************************/
        std::vector<std::string> LayerNames() const;

        bool IsLayerEnabled(std::string_view layerName) const;
        bool IsLayerEnabled(LayerKey key) const;

        LayerVisibility& Visibility() { return visibility; }
        const LayerVisibility& Visibility() const { return visibility; }

        void LogVisibilitySummary(std::string_view label) const;

        /*************************************************************************************
          \brief Clears all layers and object-to-layer mappings.
          \warning All existing assignments will be lost.
        *************************************************************************************/
        void Clear();

    private:
        std::unordered_map<LayerKey, Layer, LayerKeyHasher> LayersByKey; ///< Maps layer keys to Layer objects.
        std::unordered_map<GOCId, LayerKey> ObjectToLayer; ///< Reverse lookup for object-to-layer mapping.
        LayerVisibility visibility;
    };

}
