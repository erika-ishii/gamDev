#include "HierarchyPanel.h"
#include "Factory/Factory.h"
#include "Composition/Composition.h"

#include "Selection.h"
#include <imgui.h>
#include <string>

namespace 
{
	
	[[nodiscard]] const char* DisplayName(const Framework::GOC* obj, std::string& storage) { // must use what return
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
void mygame::DrawHierarchyPanel()
{
	using namespace Framework;

	if (!FACTORY)
		return;
	const auto& objects = FACTORY->Objects();
	// check if there is currently selected object 0 == no selection and if the object is no longer in factory remove
		if (mygame::HasSelectedObject())
		{
			Framework::GOCId selectedId = mygame::GetSelectedObjectId();
			if (objects.find(selectedId) == objects.end())
				mygame::ClearSelection();
		}
	if (ImGui::Begin("Hierarchy"))
	{
		// static text Text Disabled
		ImGui::TextDisabled("Objects: %zu", objects.size());
		ImGui::Separator();

		if (objects.empty()) {
			ImGui::TextDisabled("No object");
		}
		else if (ImGui::BeginTable("HierarchyTable", 2, ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 0.0f))) {
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
			ImGui::TableHeadersRow(); // Show name ID


			std::string nameBuffer;
			for (const auto& [id, objPtr] : objects) {
				ImGui::TableNextRow();
				const GOC* obj = objPtr.get();
				const char* displayName = DisplayName(obj, nameBuffer);
				// switch column 0
				ImGui::TableSetColumnIndex(0);
				//Push a unique IS onto Imgui stack
				ImGui::PushID(static_cast<int>(id));
				//Check whether this object is currently selected by comparing it ID with the globally stored ID
				const bool isSelected = (mygame::GetSelectedObjectId() == id);
				//Draw a selectable using object name
				//if row click update gSelectedId to remember which object user selected
				//ImGuiSelectableFlags_SpanAllColumns let you click anywhere acroess row
				if (ImGui::Selectable(displayName, isSelected, ImGuiSelectableFlags_SpanAllColumns))
					mygame::SetSelectedObjectId(id);

				//Every time you call a function that create a widget like ImGui::Selectable() that widget become the current item
				// it then will refer to the last Ui item you just created and check is the mouse currently inside the bounding box of Ui item
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("Name : %s", displayName);
					ImGui::Text("ID   : %u", id);
					ImGui::EndTooltip();
				}
				//pop ID to keep stack clean
				ImGui::PopID();
				// switch to column 1
				ImGui::TableSetColumnIndex(1);
				//Display ID
				ImGui::Text("%u", id);
			}
			ImGui::EndTable();
		}
		
		
	}
	ImGui::End();
}
