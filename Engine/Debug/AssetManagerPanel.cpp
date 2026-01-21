#if SOFASPUDS_ENABLE_EDITOR
#include "Resource_Asset_Manager/Asset_Manager.h"
#include "Resource_Asset_Manager/Resource_Manager.h"
#include "JsonEditorPanel.h"
#include <filesystem>
#include <vector>
#include <imgui.h>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW
#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace mygame
{
	void DrawAssetManagerPanel(JsonEditorPanel* jsonPanel)
	{
		ImGui::Begin("Debug Asset Manager");
		static std::vector<AssetManager::Asset> assets;
		static int selected = -1;
		if (ImGui::Button("Refresh Assets"))
		{
			assets = AssetManager::GetAllAssets();
			selected = -1;
		}
		//Search bar
		static char searchBuffer[128] = "";
		ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));

		ImGui::Separator();
		ImGui::BeginChild("AssetList", ImVec2(0, 200), true);
		//Searcher
		for (int i = 0; i < (int)assets.size(); ++i)
		{
			if (searchBuffer[0] != '\0')
			{
				std::string nameLower = assets[i].name;
				std::string searchLower = searchBuffer;
				std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
				std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
				if (nameLower.find(searchLower) == std::string::npos)
					continue;
			}

			bool isSelected = (selected == i);
			if (ImGui::Selectable(assets[i].name.c_str(), isSelected))
				selected = i;
		}

		ImGui::EndChild();
		ImGui::Separator();
		if (selected >= 0 && selected < (int)assets.size())
		{
			const auto& asset = assets[selected];
			ImGui::Text("Path: %s", asset.path.string().c_str());
			ImGui::Text("Type: %d", (int)asset.type);
			if (ImGui::Button("Load Asset"))
			{Resource_Manager::LoadAsset(asset.path);}
			// Delete button - for all
			ImGui::SameLine();
			if (ImGui::Button("Delete Asset"))
			{ImGui::OpenPopup("ConfirmDelete");}
			if (ImGui::BeginPopupModal("ConfirmDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("Delete '%s'?", asset.name.c_str());
				ImGui::Text("This action cannot be undone.");
				ImGui::Separator();

				if (ImGui::Button("Delete", ImVec2(120, 0)))
				{
					AssetManager::DeleteAsset(asset.path);
					if (jsonPanel)
						jsonPanel->RefreshFiles();
					assets=AssetManager::GetAllAssets();
					selected = -1;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0)))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		ImGui::Separator();
		ImGui::TextDisabled("Only Prefabs (JSON) can be created via the editor.");
		ImGui::TextDisabled("Binary assets must be imported externally.");
		static char prefabName[128] = "";
		ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));
		bool createObject = ImGui::Button("Create Object Prefab");
		ImGui::SameLine();
		bool createEnemy = ImGui::Button("Create Enemy Prefab");
		if ((createObject || createEnemy) && prefabName[0] != '\0')
		{
			bool success = false;
			if (createObject)
			{success = AssetManager::CreateObjectAsset(prefabName, "json");}
			else if (createEnemy)
			{success = AssetManager::CreateEnemyAsset(prefabName, "json");}

			if (success)
			{
				prefabName[0] = '\0';
				assets = AssetManager::GetAllAssets();
				if (jsonPanel)
					jsonPanel->RefreshFiles();
			}
			else
			{
				ImGui::TextColored(
					ImVec4(1, 0, 0, 1),
					"Failed to create prefab. Name might exist or template missing."
				);
			}
		}

		ImGui::End();
	}
}
#endif