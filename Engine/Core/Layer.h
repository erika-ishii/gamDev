#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "Composition/Composition.h"

namespace Framework {

    class Layer {
    public:
        Layer() = default;
        explicit Layer(std::string name);

        const std::string& Name() const { return LayerName; }
        void SetName(std::string name);

        const std::vector<GOCId>& Objects() const { return ObjectIds; }

        void Add(GOCId id);
        void Remove(GOCId id);
        bool Contains(GOCId id) const;
        void Clear();

    private:
        std::string LayerName;
        std::vector<GOCId> ObjectIds;
    };

    class LayerManager {
    public:
        Layer& EnsureLayer(std::string_view layerName);
        const Layer* FindLayer(std::string_view layerName) const;
        Layer* FindLayer(std::string_view layerName);

        void AssignToLayer(GOCId id, std::string_view layerName);
        void RemoveFromLayer(GOCId id, std::string_view layerName);
        void RemoveObject(GOCId id);

        std::string LayerFor(GOCId id) const;
        std::vector<std::string> LayerNames() const;
        void Clear();

    private:
        std::unordered_map<std::string, Layer> LayersByName;
        std::unordered_map<GOCId, std::string> ObjectToLayer;
    };

}