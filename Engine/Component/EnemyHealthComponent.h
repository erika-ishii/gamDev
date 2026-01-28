/*********************************************************************************************
 \file      EnemyHealthComponent.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu)- Primary Author, 100%

 \brief     Declares the EnemyHealthComponent class, which stores and manages basic
            health data for enemy entities. This component defines current and maximum
            health values and provides serialization for data-driven initialization.

 \details
            EnemyHealthComponent functions as a simple data container representing the
            health state of an enemy. It can be used by combat systems, UI display logic,
            or death-handling routines to determine when an enemy is defeated. Although
            currently passive, it can be extended with health regeneration, damage
            modifiers, or death triggers.

            Responsibilities:
            - Store current and maximum enemy health values.
            - Provide serialization for level or prefab data.
            - Support deep-copy for prefab instancing or cloning at runtime.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"
#include <iostream>
#include <algorithm>

namespace Framework
{
    /*****************************************************************************************
      \class EnemyHealthComponent
      \brief Component that holds and manages enemy health values.

      Stores current and maximum health for an enemy entity. It acts as a data-only
      component and does not contain update logic. Other systems such as AI or combat
      can modify these values during gameplay.
    *****************************************************************************************/
    class EnemyHealthComponent : public GameComponent
    {
    public:
        int enemyHealth{ 2 };      ///< Current health of the enemy.
        int enemyMaxhealth{ 2 };   ///< Maximum health value for the enemy.
        bool isDead = false;

        /*************************************************************************************
          \brief Initializes the component.
          \note  Currently performs no setup; placeholder for future logic.
        *************************************************************************************/
        void initialize() override {}

        /*************************************************************************************
          \brief Handles messages sent to this component.
          \param m  Reference to a message object.
          \note  Currently unused; included for consistency with the component interface.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes health data from the given serializer.
          \param s  Reference to the serializer.
          \details
              Reads "enemyHealth" and "enemyMaxhealth" values if available in the
              serialization stream (e.g., from a prefab or level file).
        *************************************************************************************/
        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("enemyHealth")) StreamRead(s, "enemyHealth", enemyHealth);
            if (s.HasKey("enemyMaxhealth")) StreamRead(s, "enemyMaxhealth", enemyMaxhealth);
        }

        /*************************************************************************************
          \brief Creates a deep copy of this component.
          \return A unique_ptr holding a cloned EnemyHealthComponent instance.
          \details
              Copies both current and maximum health values to the new component.
        *************************************************************************************/
        ComponentHandle Clone() const override
        {
            auto copy = ComponentPool<EnemyHealthComponent>::CreateTyped();
            copy->enemyHealth = enemyHealth;
            copy->enemyMaxhealth = enemyMaxhealth;
            return copy;
        }

        /*************************************************************************************
          \brief Reduces enemy health by a given damage amount.
          \param dmg The amount of damage to apply.
          \details
              Ensures health does not drop below zero. Outputs debug info to console.
        *************************************************************************************/
        void TakeDamage(int dmg)
        {
            enemyHealth = std::max(enemyHealth - dmg, 0);
            std::cout << "[EnemyHealthComponent] Took " << dmg
                << " damage, current health = " << enemyHealth << "\n";
        }

        /*************************************************************************************
          \brief Increases enemy health by a given amount.
          \param amount The amount of health to restore.
          \details
              Ensures health does not exceed enemyMaxhealth. Outputs debug info to console.
        *************************************************************************************/
        void Heal(int amount)
        {
            enemyHealth = std::min(enemyHealth + amount, enemyMaxhealth);
            std::cout << "[EnemyHealthComponent] Healed " << amount
                << ", current health = " << enemyHealth << "\n";
        }
    };
}
