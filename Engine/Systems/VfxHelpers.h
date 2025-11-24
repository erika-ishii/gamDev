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