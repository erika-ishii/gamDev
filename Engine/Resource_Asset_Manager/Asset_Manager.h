#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <unordered_set>
#include "Resource_Manager.h"
/*************************************************************************************
  \file Asset_Manager.h
  \brief Handles importing, managing, and organizing project assets such as textures,
		 spritesheets, audio, fonts, shaders, and prefab JSON files.

  The AssetManager provides utilities for:
	  - Importing and deleting assets
	  - Creating and deleting prefab templates
	  - Identifying asset types automatically based on file extension
	  - Listing all assets in the project
	  - Validating asset file names and extensions

  \details
	  The AssetManager is entirely static and provides global access to asset
	  functionality. It is designed to work with the project's folder structure,
	  including 'assets' and 'Data_Files'.
*************************************************************************************/
class AssetManager
{
	public:
		enum class AssetType
		{Texture, SpriteSheet, Audio, Font, Shader, Prefab,Json,Unknown};
		struct Asset
		{std::filesystem::path path; AssetType type = AssetType::Unknown; std::string name;};
		static bool ImportAsset(const std::filesystem::path& sourceFile);
		static bool DeleteAsset(const std::filesystem::path& assetPath);
		static AssetType IdentifyAssetType(const std::filesystem::path& assetPath);
		static bool CreateEnemyAsset(const std::string& name,const std::string& extension);
		static bool CreateObjectAsset(const std::string& name, const std::string& extension);
		static bool DeletePrefab(const std::string& prefabName);
		static const std::vector<Asset>& GetAllAssets();
		static bool IsValidAssetFile(const std::filesystem::path& path);
		static std::filesystem::path ProjectRoot();
		




};