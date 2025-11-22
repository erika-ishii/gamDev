/*********************************************************************************************
 \file      UndoStack.h
 \brief     Lightweight editor undo system for transforms and object create/delete.
*********************************************************************************************/

#pragma once

#include <cstddef>
#include <string> // Required for textureKey
#include "Factory/Factory.h"

namespace mygame
{
    namespace editor
    {
        /**
         * \brief Minimal snapshot of an object's spatial/visual data used for cheap transform undo.
         */
        struct TransformSnapshot
        {
            bool  hasTransform = false;
            float x = 0.0f;
            float y = 0.0f;
            float rot = 0.0f;

            bool  hasRect = false;
            float width = 1.0f;
            float height = 1.0f;

            bool  hasCircle = false;
            float radius = 0.0f;

            // Visual State: Color (for Render/Circle)
            float r = 1.0f;
            float g = 1.0f;
            float b = 1.0f;
            float a = 1.0f;

            // Visual State: Texture (for Sprite/Render)
            std::string textureKey; // <--- ADDED

            // Visual State: Animation
            bool hasAnim = false;
            int  animIndex = -1; // -1 means no active animation <--- ADDED
        };

        TransformSnapshot CaptureTransformSnapshot(const Framework::GOC& object);
        void RecordTransformChange(const Framework::GOC& object, const TransformSnapshot& before);
        void RecordObjectCreated(const Framework::GOC& object);
        void RecordObjectDeleted(const Framework::GOC& object);
        bool UndoLastAction();
        bool CanUndo();
        std::size_t StackDepth();
        std::size_t StackCapacity();
    }
}