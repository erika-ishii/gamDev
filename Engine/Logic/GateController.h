/*********************************************************************************************
 \file      GateController.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu ) - Primary Author, 100%

 \brief     Declares the GateController class responsible for all gate-related gameplay logic.

            GateController encapsulates the behavior for tracking gates in a level, determining
            when they should unlock based on enemy state, updating their visual appearance upon
            unlock, and detecting player–gate interactions to trigger level transitions.

            This controller is used by LogicSystem to keep gate logic modular and cleanly
            separated from general player and world logic. It supports safe resetting during
            level reloads, factory-driven object queries, and collision-based transition checks.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include <vector>
#include <string_view>

namespace Framework
{
    class GameObjectFactory;
    class GameObjectComposition;

    /*****************************************************************************************
      \class GateController
      \brief Manages gate-related gameplay logic (unlocking, player entry, level transition).

      The GateController encapsulates all logic for interacting with level gates. It tracks
      whether enemies remain alive in the level, updates the gate's unlock state, detects
      player–gate collisions, and determines whether a level transition should occur. This
      keeps gate logic cleanly separated from LogicSystem, making the system modular and
      easier to maintain.

      Responsibilities:
      - Cache and maintain a reference to the gate GOC within the current level.
      - Track player reference for collision evaluation.
      - Detect whether all enemies have been defeated.
      - Update gate's unlocked state (e.g., triggering color changes).
      - Check player-gate collision and signal when level transitions should occur.
      - Reset state safely on level reload.
    *****************************************************************************************/
    class GateController
    {
    public:
        /*************************************************************************************
          \brief Assigns the active GameObjectFactory to this controller.
          \param factory Pointer to the factory responsible for owning all GOCs.
          \note  Must be called before using enemy or object queries.
        *************************************************************************************/
        void SetFactory(GameObjectFactory* factory);

        /*************************************************************************************
          \brief Sets the player reference used for collision and unlock checks.
          \param player Pointer to the player GameObject (GOC).
          \note  Can be nullptr during level reload; safe to reset.
        *************************************************************************************/
        void SetPlayer(GameObjectComposition* player);

        /*************************************************************************************
          \brief Attempts to locate the gate object within the provided level object list.
          \param levelObjects Vector of raw GOC pointers representing the current level.
          \note  Safe to call every frame; caches reference internally.
        *************************************************************************************/
        void RefreshGateReference(const std::vector<GameObjectComposition*>& levelObjects);

        /*************************************************************************************
          \brief Resets internal cached state (gate pointer, unlocked flag).
          \note  Called during level reload to ensure clean gating behavior.
        *************************************************************************************/
        void Reset();

        /*************************************************************************************
          \brief Updates whether the gate should be unlocked based on enemy state.
          \note  Changes gate appearance (via RenderComponent) when first unlocked.
        *************************************************************************************/
        void UpdateGateUnlockState();

        /*************************************************************************************
          \brief Determines whether level transition should occur due to player–gate contact.
          \param pendingLevelTransition If true, transition is already in progress.
          \return True if player collides with unlocked gate and transition may begin.
        *************************************************************************************/
        bool ShouldTransitionOnPlayerContact(bool pendingLevelTransition) const;

    private:
        /*************************************************************************************
          \brief Checks if the level still contains any living enemies.
          \return True if any enemy GOC has remaining health; false otherwise.
          \note  Uses components rather than name-based checks for robustness.
        *************************************************************************************/
        bool HasRemainingEnemies() const;

        /*************************************************************************************
          \brief Utility helper to confirm that a GOC pointer refers to a valid, alive object.
          \param obj Pointer to the object.
          \return True if non-null and still owned by the factory.
        *************************************************************************************/
        bool IsAlive(GameObjectComposition* obj) const;

        /*************************************************************************************
          \brief Searches the level list for a GOC representing the gate.
          \param levelObjects Vector of GOCs in the current level.
          \return Pointer to the gate GOC if found; otherwise nullptr.
        *************************************************************************************/
        GameObjectComposition* FindGateInLevel(const std::vector<GameObjectComposition*>& levelObjects) const;

        /*************************************************************************************
          \brief Checks whether the player’s collider overlaps the gate’s collider.
          \return True if a valid collision occurs; false otherwise.
        *************************************************************************************/
        bool PlayerIntersectsGate() const;

        // === Internal Cached State ===

        GameObjectFactory* factory{ nullptr }; ///< Pointer to owning factory for object queries.
        GameObjectComposition* player{ nullptr };                ///< Cached player reference (for collision tests).
        GameObjectComposition* gate{ nullptr };                  ///< Cached gate reference within current level.
        bool gateUnlocked{ false };            ///< Indicates whether the gate has been unlocked.
    };
}

