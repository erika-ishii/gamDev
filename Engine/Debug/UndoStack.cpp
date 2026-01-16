/*********************************************************************************************
 \file      UndoStack.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Implementation of the editor undo system.
 \details   Provides a lightweight, editor-side undo stack with a fixed depth:
            - Tracks three kinds of actions: Transform changes, object creation, deletion.
            - Captures per-object TransformSnapshot (position/scale/rotation/colour/texture).
            - Captures serialized object JSON snapshots for create/delete operations.
            - Restores physics state (RigidBody velocity) when undoing transforms.
            - Restores sprite animations and rebinds textures after an undo.
            - Maintains at most kMaxUndoDepth entries, discarding the oldest on overflow.
            - Exposes CanUndo/UndoLastAction plus Init/Shutdown helpers for the editor.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#if SOFASPUDS_ENABLE_EDITOR

#include "Debug/UndoStack.h"
#include <vector>
#include <algorithm>

// Components
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Component/SpriteComponent.h"           
#include "Component/SpriteAnimationComponent.h"  

#include "Physics/Dynamics/RigidBodyComponent.h"

#include "Resource_Asset_Manager/Resource_Manager.h"
#include "Factory/Factory.h"
#include "Debug/Selection.h"
#include "Common/CRTDebug.h"  

#ifdef _DEBUG
#define new DBG_NEW      
#endif

namespace mygame
{
    namespace editor
    {
        namespace
        {
            /*************************************************************************
             \enum  UndoKind
             \brief Discriminator for the type of undo entry stored in the stack.
                    - Transform: position/rotation/scale/colour/texture changes.
                    - Created:   a new object was spawned and should be destroyed.
                    - Deleted:   an object was deleted and should be recreated.
            *************************************************************************/
            enum class UndoKind { Transform, Created, Deleted };

            /*************************************************************************
             \struct UndoAction
             \brief  Single entry in the undo stack.
                     Stores enough information to roll back one editor operation:
                     - kind:      what kind of change this is.
                     - objectId:  target game object id (if still valid).
                     - before:    TransformSnapshot prior to change (for Transform/Deleted).
                     - after:     TransformSnapshot after change (for Transform).
                     - snapshot:  serialized JSON snapshot for create/delete.
            *************************************************************************/
            struct UndoAction
            {
                UndoKind          kind = UndoKind::Transform;
                Framework::GOCId  objectId = 0;
                TransformSnapshot before;
                TransformSnapshot after;
                Framework::json   snapshot;
            };

            // Maximum number of undo entries kept in memory at once.
            constexpr std::size_t kMaxUndoDepth = 50;

            // Global undo buffer used by the editor.
            std::vector<UndoAction> gUndoStack;

            /*************************************************************************
             \brief  Helper to resync a SpriteComponent from a SpriteAnimationComponent.
             \param  anim    SpriteAnimationComponent owning animation state.
             \param  object  Game object holding both animation and sprite components.
             \details
                 - Ensures animation textures are rebound before sampling.
                 - Advances by a small fixed timestep to prime UVs.
                 - For sprite-sheet animations, copies sampled texture key/id.
                 - For frame-based animations, resolves the current frame texture
                   and pushes the id into the sprite.
            *************************************************************************/
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

            /*************************************************************************
             \brief  Push a new undo action onto the stack, trimming if over capacity.
             \param  action  UndoAction to append.
             \details
                 - If the stack is already at kMaxUndoDepth, drops the oldest entry.
                 - New action is always appended at the back.
            *************************************************************************/
            void PushAction(UndoAction action)
            {
                if (gUndoStack.size() >= kMaxUndoDepth)
                    gUndoStack.erase(gUndoStack.begin());
                gUndoStack.emplace_back(std::move(action));
            }

            /*************************************************************************
             \brief  Apply a TransformSnapshot back onto a live game object.
             \param  object  Target GOC instance to mutate.
             \param  state   Snapshot describing transform/render/sprite/anim state.
             \details
                 Restores in several stages:
                 1) TransformComponent: position/rotation/scale and zeroes out
                    RigidBody velocity so physics does not "fight" the undo.
                 2) RenderComponent: rect size, colour, and texture key/id.
                 3) CircleRenderComponent: radius and colour.
                 4) SpriteComponent: texture key/id if present.
                 5) SpriteAnimationComponent: active animation index, frame,
                    accumulator, playback flag, plus rebinds textures and samples
                    the appropriate frame into the sprite.
                 If an animation component exists but was not fully captured,
                 it is still rebound so any textures are valid.
            *************************************************************************/
            void ApplyTransformSnapshot(Framework::GOC& object, const TransformSnapshot& state)
            {
                // 1. Restore Transform
                if (state.hasTransform)
                {
                    if (auto* tr = object.GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent))
                    {
                        // Core transform fields
                        tr->x = state.x;
                        tr->y = state.y;
                        tr->rot = state.rot;
                        tr->scaleX = state.scaleX;
                        tr->scaleY = state.scaleY;

                        
                        if (auto* rb = object.GetComponentType<Framework::RigidBodyComponent>(
                            Framework::ComponentTypeId::CT_RigidBodyComponent))
                        {
                            // Stop the object from moving so it stays at the undone position.
                            rb->velX = 0.0f;
                            rb->velY = 0.0f;
                        }
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

                        if (!state.textureKey.empty())
                        {
                            rc->texture_key = state.textureKey;
                            unsigned tex = Resource_Manager::getTexture(state.textureKey);
                            if (tex)
                                rc->texture_id = tex;
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

                // 4. Restore Sprite Texture (if RenderComponent didn't handle it)
                if (!state.textureKey.empty())
                {
                    if (auto* sprite = object.GetComponentType<Framework::SpriteComponent>(
                        Framework::ComponentTypeId::CT_SpriteComponent))
                    {
                        sprite->texture_key = state.textureKey;
                        unsigned tex = Resource_Manager::getTexture(state.textureKey);
                        if (tex)
                            sprite->texture_id = tex;
                    }
                }

                // 5. Restore Animation State
                if (state.hasAnim)
                {
                    if (auto* anim = object.GetComponentType<Framework::SpriteAnimationComponent>(
                        Framework::ComponentTypeId::CT_SpriteAnimationComponent))
                    {
                      
                        if (anim->animations.empty() && !state.sheetAnimations.empty())
                        {
                            anim->animations = state.sheetAnimations;
                            anim->RebindAllTextures();
                        }

                        // Ensure a valid index. If snapshot was < 0, default to 0.
                        int targetIndex = (state.animIndex < 0) ? 0 : state.animIndex;

                        // Set the correct animation.
                        anim->SetActiveAnimation(targetIndex);

                        // Restore animation playback flags and frame index.
                        anim->play = state.animPlaying;
                        anim->SetFrame(state.frameIndex);

                        // Restore runtime sheet values (current frame & accumulator).
                        if (auto* active = anim->ActiveAnimation())
                        {
                            const int totalFrames = std::max(1, active->config.totalFrames);
                            const int maxFrame = totalFrames - 1;
                            active->currentFrame = std::clamp(state.sheetFrame, 0, maxFrame);
                            active->accumulator = std::max(0.0f, state.sheetAccumulator);
                        }

                        // Sync the sprite visuals immediately.
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

        /*************************************************************************
         \brief  Capture the current transform + visual state of a game object.
         \param  object  Source object whose state will be sampled.
         \return TransformSnapshot describing Transform/Render/Circle/Sprite/Anim.
         \details
             - Checks for TransformComponent, RenderComponent, CircleRenderComponent,
               SpriteComponent and SpriteAnimationComponent in that order.
             - For animation, records active index, play flag, frame index, full
               animations vector, current sheet frame and accumulator.
        *************************************************************************/
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
                if (!rc->texture_key.empty())
                    state.textureKey = rc->texture_key;
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
                if (!sprite->texture_key.empty())
                    state.textureKey = sprite->texture_key;
            }

            // 5. Capture Animation State
            if (auto* anim = object.GetComponentType<Framework::SpriteAnimationComponent>(
                Framework::ComponentTypeId::CT_SpriteAnimationComponent))
            {
                state.hasAnim = true;
                state.animIndex = anim->ActiveAnimationIndex();
                state.animPlaying = anim->play;
                state.frameIndex = anim->CurrentFrameIndex();

                // Capture structural data (fixes "missing animations after undo" bug).
                state.sheetAnimations = anim->animations;

                if (auto* active = anim->ActiveAnimation())
                {
                    state.sheetFrame = active->currentFrame;
                    state.sheetAccumulator = active->accumulator;
                }
            }

            return state;
        }

        /*************************************************************************
         \brief  Record a transform-only change as an undoable action.
         \param  object  Target object after the edit.
         \param  before  Snapshot from before the edit occurred.
         \details
             - Takes a fresh snapshot for "after", then pushes a Transform action.
             - Typically called by editor tools when a gizmo drag completes.
        *************************************************************************/
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

        /*************************************************************************
         \brief  Record object creation as an undoable action.
         \param  object  Object that has just been created.
         \details
             - On undo, the object will be destroyed using its id.
             - Stores a JSON snapshot so future extensions can also support redo.
        *************************************************************************/
        void RecordObjectCreated(const Framework::GOC& object)
        {
            if (!Framework::FACTORY) return;
            UndoAction action;
            action.kind = UndoKind::Created;
            action.objectId = object.GetId();
            action.snapshot = Framework::FACTORY->SnapshotGameObject(object);
            PushAction(std::move(action));
        }

        /*************************************************************************
         \brief  Record object deletion as an undoable action.
         \param  object  Object that is about to be deleted.
         \details
             - On undo, the object is re-instantiated from the snapshot and its
               transform/visual state is restored from "before".
        *************************************************************************/
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

        /*************************************************************************
         \brief  Undo the most recent action on the editor undo stack.
         \return True if an action was successfully undone, false otherwise.
         \details
             - No-op if there is no FACTORY or the stack is empty.
             - Transform: re-applies the "before" snapshot to the existing object.
             - Created:   destroys the object that was created.
             - Deleted:   re-instantiates from snapshot, reapplies transform/anim,
                          and marks the object as selected.
             - For create/delete, forces FACTORY->Update(0) afterwards to process
               any pending destruction/creation.
        *************************************************************************/
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
                if (obj)
                {
                    ApplyTransformSnapshot(*obj, action.before);
                    undoApplied = true;
                }
                break;
            }
            case UndoKind::Created:
            {
                Framework::GOC* obj = Framework::FACTORY->GetObjectWithId(action.objectId);
                if (obj)
                {
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
                        // Make the restored object the current editor selection.
                        mygame::SetSelectedObjectId(restored->GetId());

                       
                        ApplyTransformSnapshot(*restored, action.before);

                       
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

            // Pop the successfully applied action.
            gUndoStack.pop_back();

            // Process pending creation/destruction if necessary.
            if (requiresFactorySweep)
            {
                Framework::FACTORY->Update(0.0f);
            }

            return true;
        }

        /*************************************************************************
         \brief  Check if there is at least one undo action available.
         \return True if the undo stack is non-empty.
        *************************************************************************/
        bool CanUndo() { return !gUndoStack.empty(); }

        /*************************************************************************
         \brief  Get the current number of entries stored in the undo stack.
         \return Count of UndoAction entries.
        *************************************************************************/
        std::size_t StackDepth() { return gUndoStack.size(); }

        /*************************************************************************
         \brief  Get the maximum number of actions the undo stack can store.
         \return kMaxUndoDepth constant.
        *************************************************************************/
        std::size_t StackCapacity() { return kMaxUndoDepth; }

        /*************************************************************************
         \brief  Initialize the undo system by reserving stack capacity.
         \details Call once when bringing up the editor.
        *************************************************************************/
        void InitUndoSystem() { gUndoStack.reserve(kMaxUndoDepth); }

        /*************************************************************************
         \brief  Clear all undo entries and release memory.
         \details Call when shutting down the editor or reloading projects.
        *************************************************************************/
        void ShutdownUndoSystem() { gUndoStack.clear(); }
    }
}

#endif // SOFASPUDS_ENABLE_EDITOR
