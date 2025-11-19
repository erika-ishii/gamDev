/*********************************************************************************************
 \file      UndoStack.h
 \par       SofaSpuds
 \author    ChatGPT (OpenAI)
 \brief     Lightweight editor undo system for transforms and object create/delete.
 \details   - Transform moves are recorded as before/after TransformSnapshot.
            - Object creation/deletion are recorded as full JSON snapshots via Factory.
            - Undo is editor-only; gameplay logic should treat it as a level-edit tool.
*********************************************************************************************/

#pragma once

#include <cstddef>
#include "Factory/Factory.h"   // brings in Framework::GOC, Framework::GOCId, Framework::json

namespace mygame
{
    namespace editor
    {
        /**
         * \brief Minimal snapshot of an object's spatial data used for cheap transform undo.
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
        };

        /**
         * \brief Capture the current transform/render/circle data from an object.
         */
        TransformSnapshot CaptureTransformSnapshot(const Framework::GOC& object);

        /**
         * \brief Record a before/after transform change into the undo stack.
         * \param object  The object whose transform has just changed.
         * \param before  Snapshot taken before the change.
         *
         * Call pattern:
         *   auto before = CaptureTransformSnapshot(obj);
         *   ... mutate obj transform ...
         *   RecordTransformChange(obj, before);
         */
        void RecordTransformChange(const Framework::GOC& object,
            const TransformSnapshot& before);

        /**
         * \brief Record that an object was created (so undo will delete it).
         */
        void RecordObjectCreated(const Framework::GOC& object);

        /**
         * \brief Record that an object is about to be deleted (so undo will resurrect it).
         */
        void RecordObjectDeleted(const Framework::GOC& object);

        /**
         * \brief Undo the most recent recorded editor action.
         * \return true if an action was undone; false if stack was empty.
         */
        bool UndoLastAction();

        /**
         * \brief Query if there is at least one undoable action.
         */
        bool CanUndo();

        /**
         * \brief Current number of stored undo steps.
         */
        std::size_t StackDepth();

        /**
         * \brief Maximum number of undo steps retained.
         */
        std::size_t StackCapacity();
    }
}
