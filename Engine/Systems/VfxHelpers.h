/*********************************************************************************************
 \file      VfxHelpers.h
 \par       SofaSpuds
 \author      yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Utility helpers for spawning common gameplay VFX as GameObjectCompositions.
 \details   This module centralizes logic for creating one-off visual effects such as hit
            impact flashes. Responsibilities:
             - Ensures required VFX textures are loaded into the Resource_Manager.
             - Spawns lightweight GameObjects with Transform / Render / Sprite /
               SpriteAnimation components configured for the desired effect.
             - Returns a pointer to the newly created composition so systems can
               track or immediately forget the object (letting the level handle
               lifetime via animation/health systems).
            Current helpers:
             - SpawnHitImpactVFX(): Creates a short non-looping impact sprite-sheet
               animation at a given world position.
            Designed to keep gameplay/attack code clean by hiding the boilerplate
            VFX setup behind simple functions.
 © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*********************************************************************************************/
#pragma once

#include "Composition/Composition.h"
#include <glm/vec2.hpp>
#include <string>
#include <string_view>

namespace Framework {

    constexpr std::string_view kHitImpactVfxName = "HitImpactVFX";

    /// Spawn a transient hit impact VFX at the given world position.
    /// Returns a non-owning pointer to the created entity (owned by the factory).
    GOC* SpawnHitImpactVFX(const glm::vec2& worldPos);

    inline bool IsImpactVfxObject(const GOC* obj)
    {
        return obj && obj->GetObjectName() == kHitImpactVfxName;
    }

} // namespace Framework