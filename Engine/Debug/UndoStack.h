#pragma once

#include <cstddef>

#include "Serialization/JsonSerialization.h"
#include "Composition/Composition.h"

namespace mygame
{
    namespace editor
    {
        struct TransformSnapshot
        {
            bool hasTransform = false;
            float x = 0.0f;
            float y = 0.0f;
            float rot = 0.0f;

            bool hasRect = false;
            float width = 0.0f;
            float height = 0.0f;

            bool hasCircle = false;
            float radius = 0.0f;
        };

        TransformSnapshot CaptureTransformSnapshot(const Framework::GOC& object);

        void RecordTransformChange(const Framework::GOC& object, const TransformSnapshot& beforeState);
        void RecordObjectCreated(const Framework::GOC& object);
        void RecordObjectDeleted(const Framework::GOC& object);

        bool UndoLastAction();
        bool CanUndo();
        std::size_t StackDepth();
        std::size_t StackCapacity();
    }
}