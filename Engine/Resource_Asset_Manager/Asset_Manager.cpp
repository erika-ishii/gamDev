#include "Asset_Manager.h"
/*********************************************************************************
*\file    Asset_Manager.cpp
 \par       SofaSpuds
 \author     jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100% 
  \brief Get the project root folder by searching upwards for 'assets' and 'Data_Files'.
  \return Filesystem path to the root of the project.
  \throws std::runtime_error if the root cannot be found.
  \details
	  Caches the result for future calls. Skips directories named 'build'.
*********************************************************************************/
std::filesystem::path AssetManager::ProjectRoot()
{
	static std::filesystem::path cachedRoot; // Cache it so we only search once
	static bool initialized = false;
	if (initialized)
		return cachedRoot;
	std::filesystem::path path = std::filesystem::current_path();
	while (true)
	{
		// Skip if we're in a build directory
		if (path.string().find("\\build\\") != std::string::npos ||
			path.string().find("/build/") != std::string::npos)
		{
			path = path.parent_path();
			continue;
		}

		if (std::filesystem::exists(path / "assets") &&
			std::filesystem::exists(path / "Data_Files"))
		{
			// Found the root - change working directory to it
			std::filesystem::current_path(path);
			cachedRoot = path;
			initialized = true;
			return path;
		}
		if (path == path.root_path()) // Reached filesystem root
			break;
		path = path.parent_path(); // Move up one directory
	}
	// If not found, throw or fallback
	throw std::runtime_error("Engine root not found! Make sure 'assets' and 'Data_Files' exist.");
}
/*********************************************************************************
  \brief Check if a file is a valid asset based on its extension and name.
  \param path Path to the file.
  \return true if the file is allowed and not blocked.
  \details
	  Allowed extensions: png, jpg, jpeg, wav, mp3, ttf, otf, vert, frag, json
	  Blocked names: error, log, debug
*********************************************************************************/
bool AssetManager::IsValidAssetFile(const std::filesystem::path& path)
{
	if (!path.has_extension())
		return false;

	std::string ext = path.extension().string();
	if (!ext.empty() && ext[0] == '.')
		ext.erase(0, 1);

	// Allowed extensions ONLY
	static const std::unordered_set<std::string> allowedExtensions = {
		"png", "jpg", "jpeg",
		"wav", "mp3",
		"ttf", "otf",
		"vert", "frag",
		"json"
	};

	if (!allowedExtensions.contains(ext))
		return false;

	// Optional: block known internal/debug files
	static const std::unordered_set<std::string> blockedNames = {
		"error", "log", "debug"
	};

	if (blockedNames.contains(path.stem().string()))
		return false;

	return true;
}

/*********************************************************************************
  \brief Identify the asset type by extension and location.
  \param assetPath Path to the asset file.
  \return AssetType enum value.
*********************************************************************************/
AssetManager::AssetType AssetManager::IdentifyAssetType(const std::filesystem::path& assetPath)
{
	std::string ext = assetPath.extension().string();
	if (!ext.empty() && ext[0] == '.')
		ext.erase(0, 1);
	std::string stem = assetPath.stem().string();
	if (Resource_Manager::isTexture(ext))
	{
		std::filesystem::path animMeta =
			assetPath.parent_path() /
			(stem + ".anim.json");
		if (std::filesystem::exists(animMeta))
			return AssetType::SpriteSheet;
		return AssetType::Texture;
	}
	if (Resource_Manager::isSound(ext)) return AssetType::Audio;
	if (ext == "ttf" || ext == "otf") return AssetType::Font;
	if (ext == "vert" || ext == "frag") return AssetType::Shader;
	if (ext == "json")
	{
		if (assetPath.string().find("Data_Files") != std::string::npos) 
			return AssetType::Prefab;
		else
			return AssetType::Json;
	}
	return AssetType::Unknown;
}
/*********************************************************************************
  \brief Import a file into the project's 'assets' folder.
  \param sourceFile The path to the file to import.
  \return true if the file was copied successfully; false if the target already exists.
  \details
	  The file is copied to "<ProjectRoot>/assets/". Existing files with the same name
	  will not be overwritten. Uses std::filesystem::copy_file with overwrite flag.
*********************************************************************************/
bool AssetManager::ImportAsset(const std::filesystem::path& sourceFile) 
{
	std::filesystem::path target = ProjectRoot() / "assets" / sourceFile.filename();
	if (std::filesystem::exists(target)) return false;
	std::filesystem::copy_file(sourceFile, target, std::filesystem::copy_options::overwrite_existing);
	return true;
}
/*********************************************************************************
  \brief Delete an asset from disk and unload it from memory.
  \param assetPath Path to the asset file.
  \return true if the file existed and was deleted; false otherwise.
  \details
	  First unloads the asset using Resource_Manager::Unload(), then deletes the file
	  from disk.
*********************************************************************************/
bool AssetManager::DeleteAsset(const std::filesystem::path& assetPath) 
{ 
	if (!std::filesystem::exists(assetPath)) return false;
	std::string id = assetPath.stem().string();
	Resource_Manager::Unload(id);
	std::filesystem::remove(assetPath);
	return true;
}
/*********************************************************************************
  \brief Create a new enemy prefab based on the template "enemy_template.json".
  \param name Name of the new enemy prefab (without extension).
  \param extension File extension (must be "json").
  \return true if the prefab was successfully created.
  \details
	  Creates "<ProjectRoot>/Data_Files/<name>.json" using a predefined template.
	  Does not overwrite existing files. If the template does not exist, returns false.
*********************************************************************************/
bool AssetManager::CreateEnemyAsset(
	const std::string& name,
	const std::string& extension)
{
    // Only allow JSON prefabs
    if (extension != "json")
        return false;
    // Destination path in Data_Files
    std::filesystem::path basePath = ProjectRoot() / "Data_Files";
    std::filesystem::create_directories(basePath);
    std::filesystem::path path = basePath / (name + ".json");
    // Do not overwrite existing prefabs
    if (std::filesystem::exists(path))
        return false;
    // Path to the base enemy template INSIDE Data_Files
    std::filesystem::path templatePath = basePath / "enemy_template.json";
    if (!std::filesystem::exists(templatePath))
        return false; // template must exist
    try {std::filesystem::copy_file(templatePath, path,std::filesystem::copy_options::overwrite_existing);}
    catch (const std::exception& e) 
	{
        std::cerr << "Failed to create prefab from template: " << e.what() << std::endl;
        return false;
    }
    return true;
}
/*********************************************************************************
  \brief Create a new object prefab based on the template "object_template.json".
  \param name Name of the new object prefab (without extension).
  \param extension File extension (must be "json").
  \return true if the prefab was successfully created.
  \details
	  Creates "<ProjectRoot>/Data_Files/<name>.json" using a predefined object template.
	  Does not overwrite existing files. If the template does not exist, returns false.
*********************************************************************************/
bool AssetManager::CreateObjectAsset(
	const std::string& name,
	const std::string& extension)
{
	// Only allow JSON prefabs
	if (extension != "json")
		return false;
	// Destination path in Data_Files
	std::filesystem::path basePath = ProjectRoot() / "Data_Files";
	std::filesystem::create_directories(basePath);
	std::filesystem::path path = basePath / (name + ".json");
	// Do not overwrite existing prefabs
	if (std::filesystem::exists(path))
		return false;
	// Path to the base enemy template INSIDE Data_Files
	std::filesystem::path templatePath = basePath / "object_template.json";
	if (!std::filesystem::exists(templatePath))
		return false; // template must exist
	try { std::filesystem::copy_file(templatePath, path, std::filesystem::copy_options::overwrite_existing); }
	catch (const std::exception& e)
	{
		std::cerr << "Failed to create prefab from template: " << e.what() << std::endl;
		return false;
	}
	return true;
}
/*********************************************************************************
  \brief Delete a prefab JSON file from the Data_Files folder.
  \param prefabName Name of the prefab (without extension).
  \return true if the file existed and was deleted; false otherwise.
*********************************************************************************/
bool AssetManager::DeletePrefab(const std::string& prefabName)
{
	std::filesystem::path prefabPath =
		std::filesystem::path("Data_Files") / (prefabName + ".json");
	if (!std::filesystem::exists(prefabPath))
		return false;
	std::filesystem::remove(prefabPath);
	return true;
}
/*********************************************************************************
  \brief Retrieve all assets from both 'assets' and 'Data_Files' directories.
  \return Reference to a static vector containing all Asset objects.
  \details
	  - Scans 'assets' for textures, audio, fonts, shaders, etc.
	  - Scans 'Data_Files' for prefabs and animation metadata.
	  - Skips invalid files and unknown asset types.
	  - Returns a reference to a static vector; vector is cleared and rebuilt on each call.
*********************************************************************************/
const std::vector<AssetManager::Asset>& AssetManager::GetAllAssets()
{
	static std::vector<Asset> allAssets;
	allAssets.clear();
	std::filesystem::path assetsRoot = ProjectRoot() / "assets";
	if (std::filesystem::exists(assetsRoot) && std::filesystem::is_directory(assetsRoot))
	{
		for (auto& p : std::filesystem::recursive_directory_iterator(assetsRoot))
		{
			if (!p.is_regular_file())
				continue;
			if (!IsValidAssetFile(p.path()))
				continue;
			Asset a;
			a.path = p.path();
			a.name = p.path().stem().string();
			a.type = IdentifyAssetType(p.path());
			if (a.type == AssetType::Unknown)
				continue; // extra safety
			allAssets.push_back(a);
		}
	}

	// Scan Data_Files for prefabs and animations
	std::filesystem::path dataRoot = ProjectRoot() / "Data_Files";
	if (std::filesystem::exists(dataRoot) && std::filesystem::is_directory(dataRoot))
	{
		for (auto& p : std::filesystem::recursive_directory_iterator(dataRoot))
		{
			if (!p.is_regular_file()) continue;
			Asset a;
			a.path = p.path();
			a.name = p.path().stem().string();
			a.type = IdentifyAssetType(p.path());
			allAssets.push_back(a);
		}
	}

	return allAssets;
}