/*********************************************************************************************
 \file      HierarchyPanel.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the ImGui-based Hierarchy Panel for the in-engine editor UI.
            Displays all active GameObjects in a scrollable table with:
             - Selection
             - Hover preview
             - Right-click context menu
             - Deletion support
             - Live synchronization with the Factory’s object map

 \details   Key responsibilities:
            - Reflects the current state of FACTORY->Objects() every frame.
            - Safely handles stale selections when objects are deleted.
            - Provides tooltips showing name + ID.
            - Integrates with UndoStack for proper undo/redo of object deletions.
            - Supports renaming and debugging view through ImGui’s table features.

            Used by the editor to visualize and interact with the scene hierarchy, similar
            to Unity’s Hierarchy or Unreal’s World Outliner.
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
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace
{
    /*****************************************************************************************
     \brief  Returns a display-safe object name for the Hierarchy view.

     \param obj      Pointer to the GameObjectComposition to inspect.
     \param storage  Temporary string used to hold formatted text.

     \return const char* to a human-readable label such as:
             - "<null object>"
             - "<unnamed>"
             - user-defined object name

     \note  This helper avoids empty names appearing as blanks in the UI.
    *****************************************************************************************/
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

    /*****************************************************************************************
     \brief  Returns a trimmed version of a string (removes whitespace at both ends).

     \param value A copy of the string to trim.
     \return A new string with leading and trailing whitespace removed.
    *****************************************************************************************/
    std::string TrimCopy(std::string value)
    {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    }

    /*****************************************************************************************
     \brief  Generates a fallback name for objects that lack a user-defined one.

     \param id GameObject ID.
     \return A formatted name such as "GameObject_42".
    *****************************************************************************************/
    std::string DefaultNameForId(Framework::GOCId id)
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "GameObject_%u", id);
        return buffer;
    }

    /*****************************************************************************************
     \brief  Safely deletes a GameObject by ID.

     \param id Unique object identifier.

     \details
        - Validates factory and ID.
        - Records deletion to UndoStack (for proper undo support).
        - Uses FACTORY->Destroy() rather than manual deletion.
        - Clears selection if the deleted object was selected.
    *****************************************************************************************/
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
} // anonymous namespace


/*********************************************************************************************
 \brief  Draws the Hierarchy Panel UI using ImGui.
         This is the main entry point for rendering the hierarchy window.

 \details
     Workflow:
       1. Validate factory.
       2. Clear hover state.
       3. Validate current selection (remove if stale).
       4. Render window, controls, and table of objects.
       5. Handle per-item selection, hover tooltip, and context menu.
*********************************************************************************************/
void mygame::DrawHierarchyPanel()
{
    using namespace Framework;

    if (!FACTORY)
        return;

    // Clear hover state for this frame.
    mygame::SetHoverObjectId(0);

    const auto& objects = FACTORY->Objects();

    // Remove selection if object was deleted.
    if (mygame::HasSelectedObject())
    {
        GOCId selectedId = mygame::GetSelectedObjectId();
        if (objects.find(selectedId) == objects.end())
            mygame::ClearSelection();
    }

    // Begin ImGui panel
    if (ImGui::Begin("Hierarchy"))
    {
        ImGui::TextDisabled("Objects: %zu", objects.size());
        ImGui::Separator();

        // ----------- Controls Row -----------
        ImGui::PushID("HierarchyControls");

        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        // (Reserved for future search bar or filter)

        ImGui::SameLine();

        ImGui::SameLine();
        const bool hasSelection = mygame::HasSelectedObject();
        ImGui::BeginDisabled(!hasSelection);

        // Delete button (toolbar)
        if (ImGui::Button("Delete Selected"))
        {
            const GOCId selected = mygame::GetSelectedObjectId();
            DestroyObject(selected);
            mygame::ClearSelection();
        }
        ImGui::EndDisabled();
        ImGui::PopID();

        ImGui::Separator();


        // ----------- Main Table -----------
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

                // ---- Column 0: Selectable Name ----
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(static_cast<int>(id));

                const bool isSelected = (mygame::GetSelectedObjectId() == id);
                if (ImGui::Selectable(displayName, isSelected, ImGuiSelectableFlags_SpanAllColumns))
                    mygame::SetSelectedObjectId(id);

                // Hover tooltip
                if (ImGui::IsItemHovered())
                {
                    mygame::SetHoverObjectId(id);

                    ImGui::BeginTooltip();
                    ImGui::Text("Name : %s", displayName);
                    ImGui::Text("ID   : %u", id);
                    ImGui::EndTooltip();
                }

                // Right-click → context menu
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

                // ---- Column 1: ID ----
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", id);
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}
