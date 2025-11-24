/*********************************************************************************************
 \file      UndoStack.cpp
 \brief     Implementation of the editor undo system.
*********************************************************************************************/

#include "Debug/UndoStack.h"
#include <vector>
#include <algorithm>

// Components
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"           
#include "Component/SpriteAnimationComponent.h"  
// PHYSICS FIX: Include RigidBodyComponent so we can sync physics during undo
#include "Physics/Dynamics/RigidBodyComponent.h"

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

            // Push the animation component's current visual state back into the sprite
            // so rendering uses the active frame (either sprite sheet or frame array).
            void SyncSpriteWithAnimation(Framework::SpriteAnimationComponent& anim, Framework::GOC& object)
            {
                // Always ensure texture handles are valid before sampling.
                anim.RebindAllTextures();

                // Prime animation UVs so the sampled frame has valid coordinates.
                anim.Advance(0.016f);

                if (auto* sprite = object.GetComponentType<Framework::SpriteComponent>(
                    Framework::ComponentTypeId::CT_SpriteComponent))
                {
                    if (anim.HasSpriteSheets())
                    {
                        auto sample = anim.CurrentSheetSample();
                        if (!sample.textureKey.empty())
                            sprite->texture_key = sample.textureKey;
                        if (sample.texture)
                            sprite->texture_id = sample.texture;
                    }
                    else if (anim.HasFrames())
                    {
                        const size_t frameIndex = anim.CurrentFrameIndex();
                        if (frameIndex < anim.frames.size())
                        {
                            const auto& frame = anim.frames[frameIndex];
                            if (!frame.texture_key.empty())
                                sprite->texture_key = frame.texture_key;

                            const unsigned tex = anim.ResolveFrameTexture(frameIndex);
                            if (tex)
                                sprite->texture_id = tex;
                        }
                    }
                }
            }

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
                        // --- MAKE SURE THIS LINE IS HERE ---
                        tr->x = state.x;
                        tr->y = state.y;
                        tr->rot = state.rot; 
                        tr->scaleX = state.scaleX;
                        tr->scaleY = state.scaleY;
                        // -----------------------------------

                        // Physics Fix (Ensures object stops moving after undo)
                        if (auto* rb = object.GetComponentType<Framework::RigidBodyComponent>(
                            Framework::ComponentTypeId::CT_RigidBodyComponent))
                        {

                            // Stop the object from moving so it stays at the undone position
                            rb->velX = 0.0f;
                            rb->velY = 0.0f;
                        }
                        // -----------------------------
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
                        // ANIMATION FIX: If the recreated object is missing animations (due to JSON save failure),
                        // restore them from the memory snapshot.
                        if (anim->animations.empty() && !state.sheetAnimations.empty()) {
                            anim->animations = state.sheetAnimations;
                            anim->RebindAllTextures();
                        }

                        // Ensure a valid index. If snapshot was < 0, default to 0.
                        int targetIndex = (state.animIndex < 0) ? 0 : state.animIndex;

                        // Set the correct animation
                        anim->SetActiveAnimation(targetIndex);

                        // Restore animation playback flags and frame index
                        anim->play = state.animPlaying;
                        anim->SetFrame(state.frameIndex);

                        // Restore runtime sheet values (Current Frame & Accumulator)
                        if (auto* active = anim->ActiveAnimation())
                        {
                            const int totalFrames = std::max(1, active->config.totalFrames);
                            const int maxFrame = totalFrames - 1;
                            active->currentFrame = std::clamp(state.sheetFrame, 0, maxFrame);
                            active->accumulator = std::max(0.0f, state.sheetAccumulator);
                        }

                        // Sync the sprite visuals immediately
                        SyncSpriteWithAnimation(*anim, object);
                    }
                }

                // If an animation component exists but wasn't fully captured/restored above,
                // ensure it is at least bound correctly.
                else if (auto* anim = object.GetComponentType<Framework::SpriteAnimationComponent>(
                    Framework::ComponentTypeId::CT_SpriteAnimationComponent))
                {
                    SyncSpriteWithAnimation(*anim, object);
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
                state.scaleX = tr->scaleX; state.scaleY = tr->scaleY;
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
                state.animPlaying = anim->play;
                state.frameIndex = anim->CurrentFrameIndex();

                // Capture structural data (Fixes "Missing Animations" bug)
                state.sheetAnimations = anim->animations;

                if (auto* active = anim->ActiveAnimation())
                {
                    state.sheetFrame = active->currentFrame;
                    state.sheetAccumulator = active->accumulator;
                }
            }

            return state;
        }

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
                        // Important: Apply snapshot *after* instantiation to override default/missing values
                        ApplyTransformSnapshot(*restored, action.before);

                        // If the restored object owns an animation component, refresh its
                        // textures and push the current animation frame back into the sprite
                        // component so animation resumes immediately after undo.
                        if (auto* anim = restored->GetComponentType<Framework::SpriteAnimationComponent>(
                            Framework::ComponentTypeId::CT_SpriteAnimationComponent))
                        {
                            SyncSpriteWithAnimation(*anim, *restored);
                        }
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

        void InitUndoSystem() { gUndoStack.reserve(kMaxUndoDepth); }
        void ShutdownUndoSystem() { gUndoStack.clear(); }
    }
}