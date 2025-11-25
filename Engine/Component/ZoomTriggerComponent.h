#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"

namespace Framework {

    class ZoomTriggerComponent : public GameComponent
    {
    public:
        // How far to zoom out (smaller = more zoomed out)
        float targetZoom{ 0.7f };

        // If true, only triggers once
        bool oneShot{ true };

        // Runtime flag: has this trigger already fired?
        bool triggered{ false };

        // ---------------------------
        // Serialization (JSON -> C++)
        // ---------------------------
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("targetZoom")) {
                StreamRead(s, "targetZoom", targetZoom);
            }

            // Serializer can't read bool directly -> use int as proxy
            if (s.HasKey("oneShot")) {
                int oneShotInt = oneShot ? 1 : 0;
                StreamRead(s, "oneShot", oneShotInt);
                oneShot = (oneShotInt != 0);
            }
        }

        // ---------------------------
        // Polymorphic clone support
        // ---------------------------
        std::unique_ptr<GameComponent> Clone() const override
        {
            return std::make_unique<ZoomTriggerComponent>(*this);
        }
    };

} // namespace Framework
