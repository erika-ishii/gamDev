#include "Asset_Manager.h"

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
bool AssetManager::ImportAsset(const std::filesystem::path& sourceFile) 
{
	std::filesystem::path target = ProjectRoot() / "assets" / sourceFile.filename();
	if (std::filesystem::exists(target)) return false;
	std::filesystem::copy_file(sourceFile, target, std::filesystem::copy_options::overwrite_existing);
	return true;
}
bool AssetManager::DeleteAsset(const std::filesystem::path& assetPath) 
{ 
	if (!std::filesystem::exists(assetPath)) return false;
	std::string id = assetPath.stem().string();
	Resource_Manager::Unload(id);
	std::filesystem::remove(assetPath);
	return true;
}
bool AssetManager::CreateEmptyAsset(
	const std::string& name,
	const std::string& extension)
{
	if (extension != "json")
		return false;
	std::filesystem::path basePath = ProjectRoot() / "Data_Files";
	std::filesystem::create_directories(basePath);
	std::filesystem::path path = basePath / (name + ".json");
	if (std::filesystem::exists(path))
		return false;
	std::ofstream file(path);
	if (!file.is_open())
		return false;
	file <<
		"{\n"
		"  \"type\": \"Prefab\",\n"
		"  \"name\": \"" << name << "\",\n"
		"  \"entities\": []\n"
		"}\n";

	return true;
}
bool AssetManager::DeletePrefab(const std::string& prefabName)
{
	std::filesystem::path prefabPath =
		std::filesystem::path("Data_Files") / (prefabName + ".json");
	if (!std::filesystem::exists(prefabPath))
		return false;
	std::filesystem::remove(prefabPath);
	return true;
}
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