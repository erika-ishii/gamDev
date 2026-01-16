#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include "Resource_Manager.h"
class AssetManager
{
	public:
		enum class AssetType
		{Texture, SpriteSheet, Audio, Font, Shader, Prefab,Json,Unknown};
		struct Asset
		{std::filesystem::path path; AssetType type; std::string name;};
		static bool ImportAsset(const std::filesystem::path& sourceFile);
		static bool DeleteAsset(const std::filesystem::path& assetPath);
		static AssetType IdentifyAssetType(const std::filesystem::path& assetPath);
		static bool CreateEmptyAsset(const std::string& name,const std::string& extension);
		static bool DeletePrefab(const std::string& prefabName);
		static const std::vector<Asset>& GetAllAssets();
		static bool IsValidAssetFile(const std::filesystem::path& path);
	private:
		static std::filesystem::path ProjectRoot();




};