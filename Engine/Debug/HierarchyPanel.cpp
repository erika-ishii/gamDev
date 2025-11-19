/*********************************************************************************************
 \file      HierarchyPanel.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the ImGui hierarchy panel that lists all active GameObjects
            with selection, hover, and context menu functionality.
*********************************************************************************************/

#include "HierarchyPanel.h"
#include "Factory/Factory.h"
#include "Composition/Composition.h"
#include "Selection.h"
#include "Debug/UndoStack.h"
#include <imgui.h>
#include <array>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace
{
    [[nodiscard]] const char* DisplayName(const Framework::GOC* obj, std::string& storage)
    {
        if (!obj)
        {
            storage = "<null object>";
            return storage.c_str();
        }

        const std::string& name = obj->GetObjectName();
        storage = name.empty() ? "<unnamed>" : name;
        return storage.c_str();
    }
    std::string TrimCopy(std::string value)
    {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    }

    std::string DefaultNameForId(Framework::GOCId id)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "GameObject_%u", id);
        return buffer;
    }

 

    void DestroyObject(Framework::GOCId id)
    {
        if (!Framework::FACTORY || id == 0)
            return;

        if (auto* target = Framework::FACTORY->GetObjectWithId(id))
        {
            mygame::editor::RecordObjectDeleted(*target);
            Framework::FACTORY->Destroy(target);
        }
    }
}

void mygame::DrawHierarchyPanel()
{
    using namespace Framework;

    if (!FACTORY)
        return;

    mygame::SetHoverObjectId(0);

    const auto& objects = FACTORY->Objects();

    if (mygame::HasSelectedObject())
    {
        GOCId selectedId = mygame::GetSelectedObjectId();
        if (objects.find(selectedId) == objects.end())
            mygame::ClearSelection();
    }

    if (ImGui::Begin("Hierarchy"))
    {
        ImGui::TextDisabled("Objects: %zu", objects.size());
        ImGui::Separator();

      

        ImGui::PushID("HierarchyControls");
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
    
        ImGui::SameLine();

      

        ImGui::SameLine();
        const bool hasSelection = mygame::HasSelectedObject();
        ImGui::BeginDisabled(!hasSelection);
        if (ImGui::Button("Delete Selected"))
        {
            const GOCId selected = mygame::GetSelectedObjectId();
            DestroyObject(selected);
            mygame::ClearSelection();
        }
        ImGui::EndDisabled();
        ImGui::PopID();

        ImGui::Separator();
        if (objects.empty())
        {
            ImGui::TextDisabled("No objects available.");
        }
        else if (ImGui::BeginTable("HierarchyTable", 2,
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            std::string nameBuffer;
            for (const auto& [id, objPtr] : objects)
            {
                ImGui::TableNextRow();

                const GOC* obj = objPtr.get();
                const char* displayName = DisplayName(obj, nameBuffer);

                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(static_cast<int>(id));

                const bool isSelected = (mygame::GetSelectedObjectId() == id);
                if (ImGui::Selectable(displayName, isSelected, ImGuiSelectableFlags_SpanAllColumns))
                    mygame::SetSelectedObjectId(id);

                if (ImGui::IsItemHovered())
                {
                    mygame::SetHoverObjectId(id);

                    ImGui::BeginTooltip();
                    ImGui::Text("Name : %s", displayName);
                    ImGui::Text("ID   : %u", id);
                    ImGui::EndTooltip();
                }

                // Right-click context menu (renamed variable to avoid shadowing)
                if (ImGui::BeginPopupContextItem("##hier_ctx"))
                {
                    if (ImGui::MenuItem("Delete"))
                    {
                        DestroyObject(id);

                        if (mygame::GetSelectedObjectId() == id)
                            mygame::ClearSelection();
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", id);
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}
