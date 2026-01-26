/*********************************************************************************************
 \file      ComponentType.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Defines the ComponentTypeId enumeration which uniquely identifies each
            component type within the ECS framework. Used for registration, lookup, and
            retrieval of components from GameObjectComposition instances.

 \copyright
            All content � 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include <cstdint>

namespace Framework
{
    /*****************************************************************************************
      \enum ComponentTypeId
      \brief Strongly typed 16-bit identifiers for all supported component types.

      Each enum value corresponds to a specific component class. This ID is used internally
      by the ECS factory and GameObjectComposition to register, query, and retrieve
      components in a type-safe and memory-efficient way.

      \note Uses std::uint16_t to ensure compact storage (always 2 bytes) and disallows
            negative values. This helps minimize memory usage when storing many IDs.
    *****************************************************************************************/
    enum class ComponentTypeId : std::uint16_t // save memory always 2 bytes, it also cannot hold negative values
    {
        /// Invalid component ID (used as a sentinel value)
        CT_None = 0,

        /// TransformComponent: position, rotation, scale
        CT_TransformComponent,

        /// RenderComponent: rectangular shapes and colored quads
        CT_RenderComponent,

        /// CircleRenderComponent: specialized rendering for circles
        CT_CircleRenderComponent,

        /// GlowComponent: procedural glow blobs/strokes
        CT_GlowComponent,

        /// InputComponents: manages input bindings and player controls
        CT_InputComponents,

        /// RigidBodyComponent: physics velocity, size, and collision data
        CT_RigidBodyComponent,

        /// HurtBoxComponent: The hitbox for the attacks
        CT_HitBoxComponent,

        /// SpriteComponent: textured sprite rendering
        CT_SpriteComponent,

        /// SpriteAnimationComponent: drives sprite-frame swapping for animations
        CT_SpriteAnimationComponent,
        
        //PlayerComponent
        CT_PlayerComponent,
        CT_PlayerHealthComponent,
        CT_PlayerAttackComponent,
        CT_PlayerHUDComponent,

        /// EnemyComponent: Enemy Component
        CT_EnemyComponent,
        //Decision Tree Component
        CT_EnemyDecisionTreeComponent,
        //EnemyAttackComponent
        CT_EnemyAttackComponent,
        //Enemy Health Component
        CT_EnemyHealthComponent,
        //Enemy Type Component
        CT_EnemyTypeComponent,
        //Audio Component
        CT_AudioComponent,
        CT_ZoomTriggerComponent,
        // Gate target component
        CT_GateTargetComponent,

        /// Maximum enum value marker (not a real component, used for iteration/validation)
        CT_MaxComponent
    };
}
