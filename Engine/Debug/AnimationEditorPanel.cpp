#include "Debug/AnimationEditorPanel.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>

#include "Component/SpriteAnimationComponent.h"
#include "Component/SpriteComponent.h"
#include "Debug/Selection.h"
#include "Factory/Factory.h"

namespace
{
    using Framework::SpriteAnimationComponent;

    void ClampConfig(SpriteAnimationComponent::SpriteSheetAnimation& anim)
    {
        anim.config.totalFrames = std::max(1, anim.config.totalFrames);
        anim.config.rows = std::max(1, anim.config.rows);
        anim.config.columns = std::max(1, anim.config.columns);
        anim.config.startFrame = std::clamp(anim.config.startFrame, 0, anim.config.totalFrames - 1);
        if (anim.config.endFrame < 0)
            anim.config.endFrame = anim.config.totalFrames - 1;
        anim.config.endFrame = std::clamp(anim.config.endFrame, anim.config.startFrame, anim.config.totalFrames - 1);
        anim.currentFrame = std::clamp(anim.currentFrame, anim.config.startFrame, anim.config.endFrame);
    }

    void DrawAnimConfigFields(SpriteAnimationComponent::SpriteSheetAnimation& anim)
    {
        ClampConfig(anim);

        if (ImGui::DragInt("Total Frames", &anim.config.totalFrames, 1.0f, 1, 400))
        {
            ClampConfig(anim);
        }
        if (ImGui::DragInt("Rows", &anim.config.rows, 1.0f, 1, 64))
            ClampConfig(anim);
        if (ImGui::DragInt("Columns", &anim.config.columns, 1.0f, 1, 64))
            ClampConfig(anim);

        if (ImGui::DragInt("Start Frame", &anim.config.startFrame, 1.0f, 0, anim.config.totalFrames - 1))
            ClampConfig(anim);
        if (ImGui::DragInt("End Frame", &anim.config.endFrame, 1.0f, -1, anim.config.totalFrames - 1))
            ClampConfig(anim);

        ImGui::DragFloat("FPS", &anim.config.fps, 0.1f, 0.0f, 240.0f, "%.2f");
        ImGui::Checkbox("Looping", &anim.config.loop);
    }
}

namespace mygame
{
    void DrawAnimationEditor(bool& open)
    {
        using namespace Framework;

        if (!open)
            return;

        if (!ImGui::Begin("Animation Editor", &open))
        {
            ImGui::End();
            return;
        }

        if (!FACTORY)
        {
            ImGui::TextDisabled("Factory unavailable; cannot fetch selection.");
            ImGui::End();
            return;
        }

        if (!mygame::HasSelectedObject())
        {
            ImGui::TextDisabled("Select a GameObject with a SpriteAnimationComponent to edit.");
            ImGui::End();
            return;
        }

        auto* obj = FACTORY->GetObjectWithId(mygame::GetSelectedObjectId());
        if (!obj)
        {
            ImGui::TextDisabled("Selected object is missing or was destroyed.");
            ImGui::End();
            return;
        }

        auto* animComponent = obj->GetComponentType<SpriteAnimationComponent>(ComponentTypeId::CT_SpriteAnimationComponent);
        if (!animComponent)
        {
            ImGui::TextDisabled("Selected object does not have a SpriteAnimationComponent.");
            ImGui::End();
            return;
        }

        if (!animComponent->HasSpriteSheets())
        {
            ImGui::TextDisabled("No sprite sheet animations configured.");
            if (ImGui::Button("Create Defaults"))
                animComponent->EnsureDefaultAnimations();
            ImGui::End();
            return;
        }

        const int activeIndex = animComponent->ActiveAnimationIndex();
        const auto& animations = animComponent->animations;
        int selection = activeIndex < 0 ? 0 : activeIndex;
        std::string preview = animations.empty() ? std::string{} : animations[static_cast<std::size_t>(selection)].name;

        if (ImGui::BeginCombo("Animation", preview.c_str()))
        {
            for (std::size_t i = 0; i < animations.size(); ++i)
            {
                bool isSelected = static_cast<int>(i) == selection;
                if (ImGui::Selectable(animations[i].name.c_str(), isSelected))
                {
                    selection = static_cast<int>(i);
                    animComponent->SetActiveAnimation(selection);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selection < 0 || selection >= static_cast<int>(animations.size()))
        {
            ImGui::TextDisabled("No animation selected.");
            ImGui::End();
            return;
        }

        auto& anim = animComponent->animations[static_cast<std::size_t>(selection)];

        std::array<char, 260> pathBuffer{};
        std::snprintf(pathBuffer.data(), pathBuffer.size(), "%s", anim.spriteSheetPath.c_str());
        if (ImGui::InputText("Sprite Sheet Path", pathBuffer.data(), pathBuffer.size()))
        {
            anim.spriteSheetPath = pathBuffer.data();
            anim.textureId = 0; // force reload
        }

        if (ImGui::Button("Load/Replace Spritesheet"))
        {
            animComponent->ReloadAnimationTexture(anim);
        }

        DrawAnimConfigFields(anim);

        ImGui::Separator();
        ImGui::TextDisabled("Preview");

        auto sample = animComponent->CurrentSheetSample();
        if (sample.texture)
        {
            const ImVec2 previewSize(120.0f, 120.0f);
            const ImVec2 uv0(sample.uv.x, sample.uv.y);
            const ImVec2 uv1(sample.uv.x + sample.uv.z, sample.uv.y + sample.uv.w);
            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(sample.texture)), previewSize, uv0, uv1);
        }
        else
        {
            ImGui::TextDisabled("No texture loaded for this animation.");
        }

        ImGui::End();
    }
}