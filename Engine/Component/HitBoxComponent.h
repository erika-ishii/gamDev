/*********************************************************************************************
 \file      HitBoxComponent.h
 \par       SofaSpuds
 \author    Ho Jun (h.jun@digipen.edu) - Primary Author, 100%

 \brief     Declares the HitBoxComponent class, which defines the active area used for
            hit detection or damage application during attacks. Each hitbox instance
            represents a temporary collision region that can damage other entities.

 \details
            The HitBoxComponent acts as a data-driven representation of an attacks
            active region. It defines the hitboxs dimensions, position offset, duration,
            and damage value. During gameplay, it can be activated or deactivated by
            systems such as combat or animation logic.

            Responsibilities:
            - Store hitbox attributes (width, height, duration, damage, etc.).
            - Allow runtime activation and deactivation.
            - Provide serialization for prefab or level data.
            - Support cloning for instanced attacks or prefabs.

 \copyright
            All content 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"
#include "Common/ComponentTypeID.h"
#include "Common/System.h"
#include "Factory/Factory.h"

namespace Framework
{
    /*****************************************************************************************
      \class HitBoxComponent
      \brief Component that defines a temporary hitbox for detecting attack collisions.

      Stores the properties of an attack area such as size, duration, and damage amount.
      The hitbox can be activated or deactivated by gameplay systems to control when it
      affects other entities.
    *****************************************************************************************/
    class HitBoxComponent : public GameComponent
    {
    public:
        enum class Team
        {
            Player,
            Enemy,
            Neutral,
            Thrown
        };                           /// For determining hurtbox for player or enemy
        Team team = Team::Neutral;

        float width = 5.0f;          ///< Width of the hitbox in world units.
        float height = 5.0f;         ///< Height of the hitbox in world units.
        float duration = 1.0f;       ///< Lifetime of the hitbox in seconds.
        bool active = false;         ///< Whether the hitbox is currently active.
        float spawnX = 0.0f;         ///< X-position offset relative to the attacker.
        float spawnY = 0.0f;         ///< Y-position offset relative to the attacker.
        GOC* owner = nullptr;        ///< Optional reference to the entity that owns this hitbox.
        float damage = 1.0f;         ///< Amount of damage this hitbox inflicts.
        bool soundTriggered = false; //< To be used with HitBox Sounds so it can play mutiple SlashSounds
        float soundDelay{ 0.0f }; //< To be used for delaying a sound

        /*************************************************************************************
          \brief Initializes the hitbox component.
          \details Sets the hitbox to inactive by default upon creation.
        *************************************************************************************/
        void initialize() override { active = false; }

        /*************************************************************************************
          \brief Handles incoming messages sent to this component.
          \param m  Reference to the message being sent.
          \note  Currently unused but reserved for future message-based logic.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes hitbox parameters from the provided serializer.
          \param s  Reference to the serializer.
          \details Reads the width, height, and duration from serialized data.
        *************************************************************************************/
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("width")) StreamRead(s, "width", width);
            if (s.HasKey("height")) StreamRead(s, "height", height);
            if (s.HasKey("duration")) StreamRead(s, "duration", duration);
        }

        /*************************************************************************************
          \brief Creates a deep copy of this hitbox component.
          \return A unique_ptr holding a cloned HitBoxComponent instance.
          \details Copies size, duration, and active state data.
        *************************************************************************************/
        ComponentHandle Clone() const override
        {
            auto copy = ComponentPool<HitBoxComponent>::CreateTyped();
            copy->width = width;
            copy->height = height;
            copy->duration = duration;
            copy->active = active;
            return copy;
        }

        /*************************************************************************************
          \brief Activates the hitbox, enabling collision or damage detection.
        *************************************************************************************/
        void ActivateHurtBox() { active = true; }

        /*************************************************************************************
          \brief Deactivates the hitbox, disabling collision or damage detection.
        *************************************************************************************/
        void DeactivateHurtBox() { active = false; }
    };
}
