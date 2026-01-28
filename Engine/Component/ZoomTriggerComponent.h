/*********************************************************************************************
 \file      ZoomTriggerComponent.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Declares the ZoomTriggerComponent, a gameplay trigger used to modify the cameras
            zoom level when the player enters a designated region.
 \details   Responsibilities:
            - Stores a target zoom value used by systems (e.g., PhysicSystem or CameraSystem)
              to adjust rendering scale when activated.
            - Supports one-shot triggers that only activate once per level.
            - Provides JSON serialization for data-driven level editing (targetZoom, oneShot).
            - Implements polymorphic Clone() for prefab instancing and copying in the editor.
            - Used by zoom trigger objects placed in the level to drive camera transitions.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"

namespace Framework {

    class ZoomTriggerComponent : public GameComponent
    {
    public:
        // How far to zoom out (smaller = more zoomed out)
        float targetZoom{ 2.f };

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
        ComponentHandle Clone() const override
        {
            return ComponentPool<ZoomTriggerComponent>::Create(*this);
        }
    };

} // namespace Framework
