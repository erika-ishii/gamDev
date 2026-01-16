#if SOFASPUDS_ENABLE_EDITOR
#include "AssetManagerPanel.h"
#include "Resource_Asset_Manager/Asset_Manager.h"
#include "Resource_Asset_Manager/Resource_Manager.h"
#include <filesystem>
#include <vector>
#include <imgui.h>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW
#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace mygame
{
	void DrawAssetManagerPanel()
	{
		ImGui::Begin("Debug Asset Manager");
		static std::vector<AssetManager::Asset> assets;
		static int selected = -1;
		if (ImGui::Button("Refresh Assets"))
		{
			assets = AssetManager::GetAllAssets();
			selected = -1;
		}
		static char searchBuffer[128] = "";
		ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));

		ImGui::Separator();
		ImGui::BeginChild("AssetList", ImVec2(0, 200), true);
		
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
			if (asset.type != AssetManager::AssetType::Prefab && asset.type != AssetManager::AssetType::Json)
			{
				ImGui::SameLine();
				if (ImGui::Button("Delete Asset"))
				{
					AssetManager::DeleteAsset(asset.path);
					assets = AssetManager::GetAllAssets();
					selected = -1;
				}
			}
			else
			{
				ImGui::TextDisabled("JSON assets cannot be deleted here");
			}
		}
		ImGui::Separator();
		// ---- Create Prefab (JSON only) ----
		ImGui::TextDisabled("Only Prefabs (JSON) can be created via the editor.");
		ImGui::TextDisabled("Binary assets must be imported externally.");
		static char newPrefabName[128] = "";
		ImGui::InputText("Prefab Name", newPrefabName, sizeof(newPrefabName));
		if (ImGui::Button("Create Prefab"))
		{
			if (strlen(newPrefabName) > 0)
			{
				AssetManager::CreateEmptyAsset(newPrefabName, "json");
				assets = AssetManager::GetAllAssets();
				newPrefabName[0] = '\0';
			}
		}
		ImGui::End();
	}
}
#endif