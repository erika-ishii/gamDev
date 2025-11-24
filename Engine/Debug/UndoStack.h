/*********************************************************************************************
 \file      UndoStack.h
 \brief     Interface for the editor undo/redo system.
*********************************************************************************************/
#pragma once

#include "Core/Core.hpp"
#include "Component/SpriteAnimationComponent.h" // Required for SpriteSheetAnimation struct
#include <vector>
#include <string>

namespace mygame
{
    namespace editor
    {
        // Snapshot of a game object's visual properties at a specific point in time.
        struct TransformSnapshot
        {
            // Transform
            bool hasTransform = false;
            float x = 0.f, y = 0.f, rot = 0.f;
            float scaleX = 1.f, scaleY = 1.f;

            // Render / Rect
            bool hasRect = false;
            float width = 100.f, height = 100.f;
            float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
            std::string textureKey;

            // Circle
            bool hasCircle = false;
            float radius = 50.f;

            // Animation
            bool hasAnim = false;
            int animIndex = 0;
            bool animPlaying = true;

            // Legacy Frame Array State
            size_t frameIndex = 0;

            // Sprite Sheet State (RUNTIME FIX)
            int sheetFrame = 0;
            float sheetAccumulator = 0.0f;

            // DATA FIX: Store the actual list of animations. 
            // This ensures that if JSON serialization fails to save the config, 
            // the undo system can still restore the animations.
            std::vector<Framework::SpriteAnimationComponent::SpriteSheetAnimation> sheetAnimations;
        };

        // Initialize the Undo System
        void InitUndoSystem();

        // Record a change in transform (Move/Rotate/Scale)
        void RecordTransformChange(const Framework::GOC& object, const TransformSnapshot& before);

        // Record a newly created object
        void RecordObjectCreated(const Framework::GOC& object);

        // Record an object being deleted
        void RecordObjectDeleted(const Framework::GOC& object);

        // Helper to capture the current state of an object
        TransformSnapshot CaptureTransformSnapshot(const Framework::GOC& object);

        // Execute Undo
        bool UndoLastAction();

        // Status queries
        bool CanUndo();
        std::size_t StackDepth();
        std::size_t StackCapacity();
        void ShutdownUndoSystem();
    }
}