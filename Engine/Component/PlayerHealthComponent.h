/*********************************************************************************************
 \file      PlayerHealthComponent.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu)- Primary Author, 100%

 \brief     Declares the PlayerHealthComponent class, which stores and manages the player
            current and maximum health values. This component provides basic functionality
            to apply damage, heal the player, and serialize health-related data.

 \details
            The PlayerHealthComponent serves as a fundamental gameplay data holder, tracking
            the playerâs vitality throughout the game. It can be queried or modified by
            systems such as:
            - Combat (to apply or calculate damage)
            - Healing (to restore lost health)
            - UI (to display health bars or damage indicators)

            Responsibilities:
            - Store current and maximum player health values.
            - Apply health modifications (damage and healing).
            - Provide serialization for prefabs or level data.
            - Support deep-copy functionality for prefab instancing.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"
#include "Component/AudioComponent.h"
#include <iostream>
#include <algorithm>

namespace Framework
{
    /*****************************************************************************************
      \class PlayerHealthComponent
      \brief Component that stores and manages the player's current and maximum health values.

      This component acts as a data container for player health, which can be accessed or
      modified by combat, healing, or UI systems during gameplay.
    *****************************************************************************************/
    class PlayerHealthComponent : public GameComponent
    {
    public:
        int playerHealth{ 100 };        ///< Current health of the player.
        int playerMaxhealth{ 100 };     ///< Maximum health value of the player.
        bool isInvulnerable = false;
        float invulnTime = 0.0f;
        bool isDead = false;
        bool deathSoundPlayed = false;

        /*************************************************************************************
          \brief Initializes the component.
          \details Prints a debug message confirming the component exists on this GameObject.
        *************************************************************************************/
        void initialize() override
        {
            std::cout << "This object has a PlayerHealthComponent!\n";
        }

        /*************************************************************************************
          \brief Handles messages sent to this component.
          \param m Reference to the message object.
          \note Currently unused; included for interface consistency.
        *************************************************************************************/
        void SendMessage(Message& m) override
        {
            (void)m;
        }

        /*************************************************************************************
          \brief Serializes health data using the given serializer.
          \param s Reference to the serializer.
          \details Reads "playerHealth" and "playerMaxhealth" keys if they exist in the
                   serialized data (e.g., prefab or level file).
        *************************************************************************************/
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("playerHealth")) StreamRead(s, "playerHealth", playerHealth);
            if (s.HasKey("playerMaxhealth")) StreamRead(s, "playerMaxhealth", playerMaxhealth);
        }

        /*************************************************************************************
          \brief Creates a deep copy of this component.
          \return A unique_ptr holding the cloned PlayerHealthComponent instance.
          \details Copies both current and maximum health values to the new component.
        *************************************************************************************/
        ComponentHandle Clone() const override
        {
            auto copy = ComponentPool<PlayerHealthComponent>::CreateTyped();
            copy->playerHealth = playerHealth;
            copy->playerMaxhealth = playerMaxhealth;
            copy->isInvulnerable = isInvulnerable;
            copy->invulnTime = invulnTime;
            copy->isDead = isDead;
            return copy;
        }

        /*************************************************************************************
          \brief Reduces the player's health by a specified damage amount.
          \param dmg Amount of damage to apply.
          \details Ensures health does not drop below zero. Outputs debug information to console.
        *************************************************************************************/
        void TakeDamage(int dmg)
        {
            if (isInvulnerable)
                return;
            isInvulnerable = true;
            invulnTime = 2.0f;
            playerHealth = std::max(playerHealth - dmg, 0);
            std::cout << "[PlayerHealthComponent] Took " << dmg
                << " damage, current health = " << playerHealth << "\n";

            if (playerHealth <= 0)
            {
                isDead = true;
                std::cout << "[PlayerHealthComponent] Player is DEAD.\n";
            }
        }

        /*************************************************************************************
          \brief Increases the player's health by a specified amount.
          \param amount Amount of health to restore.
          \details Ensures health does not exceed playerMaxhealth. Outputs debug information to console.
        *************************************************************************************/
        void Heal(int amount)
        {
            playerHealth = std::min(playerHealth + amount, playerMaxhealth);
            std::cout << "[PlayerHealthComponent] Healed " << amount
                << ", current health = " << playerHealth << "\n";
        }
    };
}
