/*********************************************************************************************
 \file      LayerPanel.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100% 

 \brief     Implements the ImGui panel for layer visibility and spawn layer selection.

 \details
            Provides controls for the fixed layer groups and their sublayers. This
            panel is intended for editor testing workflows to quickly enable/disable
            groups or individual sublayers, and to select the active spawn layer.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#if SOFASPUDS_ENABLE_EDITOR

#include "Debug/LayerPanel.h"
#include "Core/Layer.h"
#include "Factory/Factory.h"
#include "imgui.h"

#include <array>
#include <string>

namespace mygame {
    namespace {
        constexpr std::array<const char*, 4> kGroupNames{
            "Background",
            "Gameplay",
            "Foreground",
            "UI"
        };
        constexpr std::array<Framework::LayerGroup, 4> kGroups{
            Framework::LayerGroup::Background,
            Framework::LayerGroup::Gameplay,
            Framework::LayerGroup::Foreground,
            Framework::LayerGroup::UI
        };

        Framework::LayerGroup gActiveGroup = Framework::LayerGroup::Background;
        int gActiveSublayer = 0;
        std::string gActiveLayerName = Framework::LayerNameFromKey({ gActiveGroup, gActiveSublayer });

        void SyncActiveLayerName()
        {
            gActiveLayerName = Framework::LayerNameFromKey({ gActiveGroup, gActiveSublayer });
        }

        void DrawSublayerGrid(Framework::LayerVisibility& visibility, Framework::LayerGroup group)
        {
            const std::string tableId = std::string("##Sublayers_") + Framework::LayerGroupName(group);
            constexpr int kColumnCount = 7;
            if (ImGui::BeginTable(tableId.c_str(), kColumnCount, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::PushID(static_cast<int>(group));
                for (int sublayer = 0; sublayer <= Framework::kMaxLayerSublayer; ++sublayer)
                {
                    ImGui::TableNextColumn();
                    bool enabled = visibility.IsSublayerEnabled(group, sublayer);
                    ImGui::PushID(sublayer);
                    const std::string label = std::to_string(sublayer);
                    if (ImGui::Checkbox(label.c_str(), &enabled))
                    {
                        visibility.SetSublayerEnabled(group, sublayer, enabled);
                    }
                    ImGui::PopID();
                }
                ImGui::PopID();
                ImGui::EndTable();
            }
        }
    }

    const std::string& ActiveLayerName()
    {
        return gActiveLayerName;
    }

    void DrawLayerPanel()
    {
        if (!ImGui::Begin("Layers"))
        {
            ImGui::End();
            return;
        }

        if (!Framework::FACTORY)
        {
            ImGui::TextDisabled("Factory is not ready.");
            ImGui::End();
            return;
        }

        auto& visibility = Framework::FACTORY->Layers().Visibility();
        auto ensureActiveLayerVisible = [&visibility]() {
            visibility.SetGroupEnabled(gActiveGroup, true);
            visibility.SetSublayerEnabled(gActiveGroup, gActiveSublayer, true);
            };

        ImGui::SeparatorText("Spawn Layer");
        int groupIndex = static_cast<int>(gActiveGroup);
        if (ImGui::Combo("Group", &groupIndex,
            "Background\0Gameplay\0Foreground\0UI\0"))
        {
            gActiveGroup = static_cast<Framework::LayerGroup>(groupIndex);
            SyncActiveLayerName();
            ensureActiveLayerVisible();
        }

        if (ImGui::SliderInt("Sublayer", &gActiveSublayer, 0, Framework::kMaxLayerSublayer))
        {
            SyncActiveLayerName();
            ensureActiveLayerVisible();
        }

        ImGui::Text("Active: %s", gActiveLayerName.c_str());

        ImGui::SeparatorText("Visibility");
        if (ImGui::Button("Enable All"))
        {
            visibility.EnableAll();
        }
        ImGui::SameLine();
        if (ImGui::Button("Enable Only Selected"))
        {
            visibility.EnableOnly({ gActiveGroup, gActiveSublayer });
        }

        for (std::size_t index = 0; index < kGroups.size(); ++index)
        {
            const Framework::LayerGroup group = kGroups[index];
            const char* groupName = kGroupNames[index];
            bool groupEnabled = visibility.IsGroupEnabled(group);
            std::string label = std::string(groupName) + " Enabled";
            if (ImGui::Checkbox(label.c_str(), &groupEnabled))
            {
                visibility.SetGroupEnabled(group, groupEnabled);
            }

            ImGui::Indent();
            DrawSublayerGrid(visibility, group);
            ImGui::Unindent();
            ImGui::Spacing();
        }

        ImGui::End();
    }
}

#endif