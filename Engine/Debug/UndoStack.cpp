#include "Debug/UndoStack.h"

#include <vector>

#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Component/CircleRenderComponent.h"
#include "Factory/Factory.h"

namespace mygame
{
    namespace editor
    {
        namespace
        {
            enum class UndoKind
            {
                Transform,
                Created,
                Deleted
            };

            struct UndoAction
            {
                UndoKind kind = UndoKind::Transform;
                Framework::GOCId objectId = 0;
                TransformSnapshot before{};
                TransformSnapshot after{};
                Framework::json snapshot = Framework::json::object();
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
                if (state.hasTransform)
                {
                    if (auto* tr = object.GetComponentType<Framework::TransformComponent>(
                        Framework::ComponentTypeId::CT_TransformComponent))
                    {
                        tr->x = state.x;
                        tr->y = state.y;
                        tr->rot = state.rot;
                    }
                }

                if (state.hasRect)
                {
                    if (auto* rc = object.GetComponentType<Framework::RenderComponent>(
                        Framework::ComponentTypeId::CT_RenderComponent))
                    {
                        rc->w = state.width;
                        rc->h = state.height;
                    }
                }

                if (state.hasCircle)
                {
                    if (auto* cc = object.GetComponentType<Framework::CircleRenderComponent>(
                        Framework::ComponentTypeId::CT_CircleRenderComponent))
                    {
                        cc->radius = state.radius;
                    }
                }
            }
        }

        TransformSnapshot CaptureTransformSnapshot(const Framework::GOC& object)
        {
            TransformSnapshot state;

            if (auto* tr = object.GetComponentType<Framework::TransformComponent>(
                Framework::ComponentTypeId::CT_TransformComponent))
            {
                state.hasTransform = true;
                state.x = tr->x;
                state.y = tr->y;
                state.rot = tr->rot;
            }

            if (auto* rc = object.GetComponentType<Framework::RenderComponent>(
                Framework::ComponentTypeId::CT_RenderComponent))
            {
                state.hasRect = true;
                state.width = rc->w;
                state.height = rc->h;
            }

            if (auto* cc = object.GetComponentType<Framework::CircleRenderComponent>(
                Framework::ComponentTypeId::CT_CircleRenderComponent))
            {
                state.hasCircle = true;
                state.radius = cc->radius;
            }

            return state;
        }

        void RecordTransformChange(const Framework::GOC& object, const TransformSnapshot& beforeState)
        {
            if (!Framework::FACTORY)
                return;

            UndoAction action;
            action.kind = UndoKind::Transform;
            action.objectId = object.GetId();
            action.before = beforeState;
            action.after = CaptureTransformSnapshot(object);
            PushAction(std::move(action));
        }

        void RecordObjectCreated(const Framework::GOC& object)
        {
            if (!Framework::FACTORY)
                return;

            UndoAction action;
            action.kind = UndoKind::Created;
            action.objectId = object.GetId();
            action.snapshot = Framework::FACTORY->SnapshotGameObject(object);
            PushAction(std::move(action));
        }

        void RecordObjectDeleted(const Framework::GOC& object)
        {
            if (!Framework::FACTORY)
                return;

            UndoAction action;
            action.kind = UndoKind::Deleted;
            action.objectId = object.GetId();
            action.snapshot = Framework::FACTORY->SnapshotGameObject(object);
            PushAction(std::move(action));
        }

        bool UndoLastAction()
        {
            if (!Framework::FACTORY)
                return false;
            if (gUndoStack.empty())
                return false;

            UndoAction action = std::move(gUndoStack.back());
            gUndoStack.pop_back();

            bool requiresFactorySweep = false;
            switch (action.kind)
            {
            case UndoKind::Transform:
                if (auto* obj = Framework::FACTORY->GetObjectWithId(action.objectId))
                    ApplyTransformSnapshot(*obj, action.before);
                break;
            case UndoKind::Created:
                if (auto* obj = Framework::FACTORY->GetObjectWithId(action.objectId))
                {
                    Framework::FACTORY->Destroy(obj);
                    requiresFactorySweep = true;
                }
                break;
            case UndoKind::Deleted:
                Framework::FACTORY->InstantiateFromSnapshot(action.snapshot);
                break;
            }

            if (requiresFactorySweep)
                Framework::FACTORY->Update(0.0f);

            return true;
        }

        bool CanUndo()
        {
            return !gUndoStack.empty();
        }

        std::size_t StackDepth()
        {
            return gUndoStack.size();
        }

        std::size_t StackCapacity()
        {
            return kMaxUndoDepth;
        }
    }
}