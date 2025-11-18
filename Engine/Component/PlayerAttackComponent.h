/*********************************************************************************************
 \file      PlayerAttackComponent.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu)- Primary Author, 100%

 \brief     Declares the PlayerAttackComponent class, which defines the player�s attack
            properties such as base damage and attack speed. This component serves as
            a data container used by gameplay systems to determine player attack strength
            and rate of fire.

 \details
            The PlayerAttackComponent encapsulates basic combat attributes related to
            the player's offensive actions. It can be extended by gameplay systems to
            trigger attack animations, generate hitboxes, or scale attack power through
            upgrades and power-ups.

            Responsibilities:
            - Store attack-related parameters (damage, attack speed).
            - Provide serialization for prefab or level loading.
            - Support deep-copy functionality for instanced player objects.
            - Output debug logs on initialization.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Component/HitBoxComponent.h"
#include "Component/TransformComponent.h"
#include <memory>
#include <iostream>

namespace Framework
{
    /*****************************************************************************************
      \class PlayerAttackComponent
      \brief Component that defines player attack parameters such as damage and speed.

      This component acts as a lightweight data holder for combat attributes, allowing
      systems to reference and modify the player�s offensive stats during gameplay.
    *****************************************************************************************/
    class PlayerAttackComponent : public GameComponent
    {
    public:
        int damage{ 50 };             ///< Base attack damage of the player.
        float attack_speed{ 1.0f };   ///< Time interval or multiplier controlling attack rate.
        std::unique_ptr<HitBoxComponent> hitbox;

        /*************************************************************************************
          \brief Default constructor initializes attack values to their defaults.
        *************************************************************************************/
        PlayerAttackComponent() {
            if (!hitbox) hitbox = std::make_unique<HitBoxComponent>();
        }

        /*************************************************************************************
          \brief Parameterized constructor to set custom attack stats.
          \param dmg  Base damage value for the player.
          \param spd  Attack speed multiplier or delay.
        *************************************************************************************/
        PlayerAttackComponent(int dmg, float spd) : damage(dmg), attack_speed(spd) {}

        /*************************************************************************************
          \brief Called when the component is initialized.
          \details Prints a message confirming that this GameObject has a PlayerAttackComponent.
        *************************************************************************************/
        void initialize() override {
            if (!hitbox) hitbox = std::make_unique<HitBoxComponent>();
            // reasonable defaults if JSON doesn’t specify
            hitbox->width = hitbox->width == 0.f ? 0.5f : hitbox->width;
            hitbox->height = hitbox->height == 0.f ? 0.5f : hitbox->height;
            hitbox->active = false;
            std::cout << "This object has a PlayerAttackComponent!\n"; }

        /*************************************************************************************
          \brief Handles incoming messages sent to this component.
          \param m  Reference to the message.
          \note  Currently unused; reserved for future event-driven behavior.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes the attack properties from the provided serializer.
          \param s  Reference to the serializer.
          \details
              Reads the "damage" and "attack_speed" keys if present in the serialized data.
        *************************************************************************************/
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("damage")) StreamRead(s, "damage", damage);
            if (s.HasKey("attack_speed")) StreamRead(s, "attack_speed", attack_speed);
        }

        /*************************************************************************************
          \brief Creates a deep copy of this PlayerAttackComponent.
          \return A unique_ptr holding a cloned instance of the component.
          \details
              Copies both damage and attack_speed values into the new instance.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override
        {
            auto copy = std::make_unique<PlayerAttackComponent>();
            copy->damage = damage;
            copy->attack_speed = attack_speed;
            return copy;
        }

        /*************************************************************************************
          \brief Performs an attack by spawning or updating the hitbox relative to the player.
          \param playerTr Pointer to the player's TransformComponent to determine spawn position.
        *************************************************************************************/
        void PerformAttack(TransformComponent* playerTr)
        {
            if (!playerTr) return;
            if (!hitbox) hitbox = std::make_unique<HitBoxComponent>();

            hitbox->spawnX = playerTr->x;
            hitbox->spawnY = playerTr->y;
            hitbox->width = 50;
            hitbox->height = 50;
            hitbox->damage = static_cast<float>(damage);
            hitbox->duration = 0.2f;
            hitbox->ActivateHurtBox();
        }

        /*************************************************************************************
          \brief Updates the hitbox duration and deactivates it when time expires.
          \param dt Delta time since last update.
          \param tr Pointer to the player's TransformComponent (optional for future use).
        *************************************************************************************/
        void Update(float dt, TransformComponent* tr)
        {
            (void)tr;
            if (hitbox && hitbox->active)
            {
                hitbox->duration -= dt;
                if (hitbox->duration <= 0.0f)
                {
                    hitbox->DeactivateHurtBox();
                    hitbox->duration = 0.2f; // reset for next attack
                }
            }
        }





    };
}
