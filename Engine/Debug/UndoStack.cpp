/*********************************************************************************************
 \file      UndoStack.cpp
 \brief     Implementation of the editor undo system.
*********************************************************************************************/

#include "Debug/UndoStack.h"
#include <vector>

// Components
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"           // Needed for texture key capture
#include "Component/SpriteAnimationComponent.h"  // Needed for animation state capture/restore
#include "Resource_Manager/Resource_Manager.h"

#include "Factory/Factory.h"
#include "Debug/Selection.h"

namespace mygame
{
    namespace editor
    {
        namespace
        {
            enum class UndoKind { Transform, Created, Deleted };

            struct UndoAction
            {
                UndoKind          kind = UndoKind::Transform;
                Framework::GOCId  objectId = 0;
                TransformSnapshot before;
                TransformSnapshot after;
                Framework::json   snapshot;
            };

            constexpr std::size_t kMaxUndoDepth = 50;
            std::vector<UndoAction> gUndoStack;

            void PushAction(UndoAction action)
            {
                if (gUndoStack.size() >= kMaxUndoDepth)
                    gUndoStack.erase(gUndoStack.begin());
                gUndoStack.emplace_back(std::move(action));
            }

            void ApplyTransformSnapshot(Framework::GOC& object, const TransformSnapshot& state)
            {
                // 1. Restore Transform
                if (state.hasTransform)
                {
                    if (auto* tr = object.GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent))
                    {
                        tr->x = state.x; tr->y = state.y; tr->rot = state.rot;
                    }
                }

                // 2. Restore Rect / Color / Texture
                if (state.hasRect)
                {
                    if (auto* rc = object.GetComponentType<Framework::RenderComponent>(
                        Framework::ComponentTypeId::CT_RenderComponent))
                    {
                        rc->w = state.width; rc->h = state.height;
                        rc->r = state.r; rc->g = state.g; rc->b = state.b; rc->a = state.a;
                        if (!state.textureKey.empty()) {
                            rc->texture_key = state.textureKey;
                            unsigned tex = Resource_Manager::getTexture(state.textureKey);
                            if (tex) rc->texture_id = tex;
                        }
                    }
                }

                // 3. Restore Circle
                if (state.hasCircle)
                {
                    if (auto* cc = object.GetComponentType<Framework::CircleRenderComponent>(
                        Framework::ComponentTypeId::CT_CircleRenderComponent))
                    {
                        cc->radius = state.radius;
                        cc->r = state.r; cc->g = state.g; cc->b = state.b; cc->a = state.a;
                    }
                }

                // 4. Restore Sprite Texture (If RenderComponent didn't handle it)
                if (!state.textureKey.empty())
                {
                    if (auto* sprite = object.GetComponentType<Framework::SpriteComponent>(
                        Framework::ComponentTypeId::CT_SpriteComponent))
                    {
                        sprite->texture_key = state.textureKey;
                        unsigned tex = Resource_Manager::getTexture(state.textureKey);
                        if (tex) sprite->texture_id = tex;
                    }
                }

                // 5. Restore Animation State
                if (state.hasAnim)
                {
                    if (auto* anim = object.GetComponentType<Framework::SpriteAnimationComponent>(
                        Framework::ComponentTypeId::CT_SpriteAnimationComponent))
                    {
                        // Ensure a valid index. If snapshot was < 0, default to 0 (Idle).
                        int targetIndex = (state.animIndex < 0) ? 0 : state.animIndex;

                        // Set the correct animation (this resets currentFrame and accumulator)
                        anim->SetActiveAnimation(targetIndex);

                        // CRITICAL FIX: Force Advance/Update. Use 0.02f (approx 1 frame @ 60fps) 
                        // to force the component to calculate its UV coordinates immediately.
                        // This prevents the whole sprite sheet from rendering.
                        anim->Advance(0.02f);
                    }
                }
            }
        } // anonymous namespace

        TransformSnapshot CaptureTransformSnapshot(const Framework::GOC& object)
        {
            TransformSnapshot state;

            // 1. Capture Transform
            if (auto* tr = object.GetComponentType<Framework::TransformComponent>(
                Framework::ComponentTypeId::CT_TransformComponent))
            {
                state.hasTransform = true;
                state.x = tr->x; state.y = tr->y; state.rot = tr->rot;
            }

            // 2. Capture Rect
            if (auto* rc = object.GetComponentType<Framework::RenderComponent>(
                Framework::ComponentTypeId::CT_RenderComponent))
            {
                state.hasRect = true;
                state.width = rc->w; state.height = rc->h;
                state.r = rc->r; state.g = rc->g; state.b = rc->b; state.a = rc->a;
                if (!rc->texture_key.empty()) state.textureKey = rc->texture_key;
            }

            // 3. Capture Circle
            if (auto* cc = object.GetComponentType<Framework::CircleRenderComponent>(
                Framework::ComponentTypeId::CT_CircleRenderComponent))
            {
                state.hasCircle = true;
                state.radius = cc->radius;
                state.r = cc->r; state.g = cc->g; state.b = cc->b; state.a = cc->a;
            }

            // 4. Capture Sprite Texture (if RenderComponent was not found/used)
            if (auto* sprite = object.GetComponentType<Framework::SpriteComponent>(
                Framework::ComponentTypeId::CT_SpriteComponent))
            {
                if (!sprite->texture_key.empty()) state.textureKey = sprite->texture_key;
            }

            // 5. Capture Animation State
            if (auto* anim = object.GetComponentType<Framework::SpriteAnimationComponent>(
                Framework::ComponentTypeId::CT_SpriteAnimationComponent))
            {
                state.hasAnim = true;
                state.animIndex = anim->ActiveAnimationIndex();
            }

            return state;
        }

        // ... (RecordTransformChange, RecordObjectCreated, RecordObjectDeleted, UndoLastAction, CanUndo, StackDepth, StackCapacity functions remain unchanged)
        void RecordTransformChange(const Framework::GOC& object, const TransformSnapshot& before)
        {
            if (!Framework::FACTORY) return;
            UndoAction action;
            action.kind = UndoKind::Transform;
            action.objectId = object.GetId();
            action.before = before;
            action.after = CaptureTransformSnapshot(object);
            PushAction(std::move(action));
        }

        void RecordObjectCreated(const Framework::GOC& object)
        {
            if (!Framework::FACTORY) return;
            UndoAction action;
            action.kind = UndoKind::Created;
            action.objectId = object.GetId();
            action.snapshot = Framework::FACTORY->SnapshotGameObject(object);
            PushAction(std::move(action));
        }

        void RecordObjectDeleted(const Framework::GOC& object)
        {
            if (!Framework::FACTORY) return;
            UndoAction action;
            action.kind = UndoKind::Deleted;
            action.objectId = object.GetId();
            action.snapshot = Framework::FACTORY->SnapshotGameObject(object);
            action.before = CaptureTransformSnapshot(object);
            PushAction(std::move(action));
        }

        bool UndoLastAction()
        {
            if (!Framework::FACTORY) return false;
            if (gUndoStack.empty()) return false;

            UndoAction action = gUndoStack.back();
            bool requiresFactorySweep = false;
            bool undoApplied = false;

            switch (action.kind)
            {
            case UndoKind::Transform:
            {
                Framework::GOC* obj = Framework::FACTORY->GetObjectWithId(action.objectId);
                if (obj) {
                    ApplyTransformSnapshot(*obj, action.before);
                    undoApplied = true;
                }
                break;
            }
            case UndoKind::Created:
            {
                Framework::GOC* obj = Framework::FACTORY->GetObjectWithId(action.objectId);
                if (obj) {
                    Framework::FACTORY->Destroy(obj);
                    requiresFactorySweep = true;
                    undoApplied = true;
                }
                break;
            }
            case UndoKind::Deleted:
            {
                if (action.snapshot.is_object())
                {
                    Framework::GOC* restored = Framework::FACTORY->InstantiateFromSnapshot(action.snapshot);
                    if (restored)
                    {
                        mygame::SetSelectedObjectId(restored->GetId());
                        // Important: Apply snapshot *after* instantiation
                        ApplyTransformSnapshot(*restored, action.before);
                        undoApplied = true;
                        requiresFactorySweep = true;
                    }
                }
                break;
            }
            }

            if (!undoApplied) return false;

            gUndoStack.pop_back();

            if (requiresFactorySweep) {
                Framework::FACTORY->Update(0.0f);
            }

            return true;
        }

        bool CanUndo() { return !gUndoStack.empty(); }
        std::size_t StackDepth() { return gUndoStack.size(); }
        std::size_t StackCapacity() { return kMaxUndoDepth; }
    }
}