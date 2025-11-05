/*********************************************************************************************
 \file      HierarchyPanel.cpp
 \par       SofaSpuds
 \author    
 \brief     ImGui hierarchy panel listing live objects with selection and tooltips.
 \details   Builds a scrollable Name/ID table from Factory::Objects(). Keeps the global
            selection valid (clears it if the object was removed) and shows basic info
            in a tooltip. Display names prefer object names, falling back to placeholders.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "HierarchyPanel.h"
#include "Factory/Factory.h"
#include "Composition/Composition.h"

#include "Selection.h"
#include <imgui.h>
#include <string>

namespace
{
    /*****************************************************************************************
      \brief Produce a stable C-string for an object's display name.
      \param obj     Object to name (may be null).
      \param storage Scratch buffer that owns the returned memory.
      \return const char* backed by \p storage; "<null object>" or "<unnamed>" if needed.
    *****************************************************************************************/
    [[nodiscard]] const char* DisplayName(const Framework::GOC* obj, std::string& storage)
    {
        if (!obj)
        {
            storage = "<null object>";
            return storage.c_str();
        }
        const std::string& name = obj->GetObjectName();
        if (!name.empty())
        {
            storage = name;
        }
        else
        {
            storage.clear();
            storage += "<unnamed>";
        }
        return storage.c_str();
    }
}

/*****************************************************************************************
  \brief Draw the Hierarchy panel window (Name/ID table + selection maintenance).
  \details
    - Verifies the current global selection still exists; clears it if not.
    - Renders a 2-column table (Name, ID) with selectable rows.
    - Updates global selection via Selection.h helpers when a row is clicked.
    - Shows a tooltip with Name and ID on hover.
*****************************************************************************************/
void mygame::DrawHierarchyPanel()
{
    using namespace Framework;

    if (!FACTORY)
        return;

    const auto& objects = FACTORY->Objects();

    // Keep global selection consistent with current factory contents.
    if (mygame::HasSelectedObject())
    {
        Framework::GOCId selectedId = mygame::GetSelectedObjectId();
        if (objects.find(selectedId) == objects.end())
            mygame::ClearSelection();
    }

    if (ImGui::Begin("Hierarchy"))
    {
        ImGui::TextDisabled("Objects: %zu", objects.size());
        ImGui::Separator();

        if (objects.empty())
        {
            ImGui::TextDisabled("No object");
        }
        else if (ImGui::BeginTable("HierarchyTable", 2,
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY, ImVec2(0.0f, 0.0f)))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableHeadersRow();

            std::string nameBuffer;
            for (const auto& [id, objPtr] : objects)
            {
                ImGui::TableNextRow();

                const GOC* obj = objPtr.get();
                const char* displayName = DisplayName(obj, nameBuffer);

                // Column 0: name + selectable row
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(static_cast<int>(id));

                const bool isSelected = (mygame::GetSelectedObjectId() == id);
                if (ImGui::Selectable(displayName, isSelected, ImGuiSelectableFlags_SpanAllColumns))
                    mygame::SetSelectedObjectId(id);

                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("Name : %s", displayName);
                    ImGui::Text("ID   : %u", id);
                    ImGui::EndTooltip();
                }

                ImGui::PopID();

                // Column 1: ID
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", id);
            }

            ImGui::EndTable();
        }
    }
    ImGui::End();
}
