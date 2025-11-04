/*********************************************************************************************
 \file      Resource_Manager.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the Resource_Manager class, providing functions to load, track,
            and unload game resources such as textures, sounds, fonts, and graphics. Includes
            helper methods for file extension handling and resource type validation.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Resource_Manager.h"

/*****************************************************************************************
     \brief Get the file extension from a path string.
    \param path  Path to the file.
    \return File extension string (e.g., "png", "wav").
*****************************************************************************************/
inline std::string Resource_Manager::GetExtension(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    if (!ext.empty() && ext[0]=='.'){ext.erase(0,1);}
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}
/*****************************************************************************************
     \brief Check if a given file extension corresponds to a texture type.
    \param ext  File extension string.
    \return true if it is a texture, false otherwise.
*****************************************************************************************/
bool Resource_Manager::isTexture(const std::string& ext){return (ext == "png"||ext == "jpg");}
/*****************************************************************************************
     \brief Check if a given file extension corresponds to a sound type.
    \param ext  File extension string.
    \return true if it is a sound, false otherwise.
*****************************************************************************************/
bool Resource_Manager::isSound(const std::string& ext){return ext == "mp3"|| ext == "wav";}
/*****************************************************************************************
     \brief Retrieve the handle of a texture resource by its unique key.
    \param key  Resource identifier.
    \return Handle of the texture, or 0 if not found.
*****************************************************************************************/
unsigned int Resource_Manager::getTexture(const std::string& key)
{
    auto it = resources_map.find(key);
    if (it != resources_map.end() && it->second.type == Resource_Type::Graphics) {
        return it->second.handle;
    }
    return 0; // Not found
}


namespace fs = std::filesystem;
/*****************************************************************************************
     \brief Load a single resource by name and path.
    \param name  Unique identifier for the resource.
    \param path  Path to the resource file.
    \param loop  Optional flag for sound looping (default false).
    \return true if the resource was successfully loaded, false otherwise.
*****************************************************************************************/
bool Resource_Manager::load(const std::string& id, const std::string& path)
{
    fs::path filePath(path);
    if (!fs::exists(filePath) || !fs::is_regular_file(filePath))
    {std::cerr << "[Resource_Manager] File not found: " << path << std::endl; return false;}
    std::string ext = GetExtension(path);
    if (isTexture(ext))
    {  
        unsigned int texID = gfx::Graphics::loadTexture(path.c_str());
        if (texID != 0)
        {
            auto existing = resources_map.find(id);
            if (existing != resources_map.end() && existing->second.type == Resource_Type::Graphics)
            {
                if (existing->second.handle != 0 && existing->second.handle != texID)
                    gfx::Graphics::destroyTexture(existing->second.handle);
            }

            resources_map[id] = { id, Resource_Type::Graphics, texID };
            return true;
        }
        return false;
    }
    else if (isSound(ext))
    {
        auto existing = resources_map.find(id);
        if (existing != resources_map.end() && existing->second.type == Resource_Type::Sound)
        {
            SoundManager::getInstance().stopSound(id);
            SoundManager::getInstance().unloadSound(id);
        }

        bool success = SoundManager::getInstance().loadSound(id, path);
        if (success)
        {
            resources_map[id] = { id, Resource_Type::Sound ,0 };
        }
        return success;
    }
    else {std::cerr << "[Resource_Manager] Unsupported file type: "  << path << std::endl; return false;}
}
/*****************************************************************************************
     \brief Load all resources from a specified directory.
    \param directory  Path to the directory containing resource files.
*****************************************************************************************/
void Resource_Manager::loadAll(const std::string& directory)
{
    for (auto& entry : fs::directory_iterator(directory)) 
    {
        if (!entry.is_regular_file()) continue;
        fs::path path = entry.path();
        std::string ext = GetExtension(path.string());
        std::string name = path.stem().string(); // filename without extension
        std::string stem = path.stem().string();
        size_t pos = stem.find_first_of("-_.");
        std::string id = (pos == std::string::npos) ? stem : stem.substr(0, pos);
        if (isTexture(ext) || isSound(ext)) 
        {
            if (load(id, path.string())) 
            {
                std::cout << "[Resource_Manager] Loaded: " << id
                    << " (" << path.string() << ")"
                    << std::endl;
            }
        }
    }
}
/*****************************************************************************************
     \brief Unload all resources of a specified type.
    \param type  Resource type to unload (Texture, Font, Graphics, Sound, or All).
*****************************************************************************************/
void Resource_Manager::unloadAll(Resource_Type type)
{
    std::cout << "[Resource_Manager] Unloading resources of type: "
        << (type == Resource_Type::All ? "All"
            : (type == Resource_Type::Sound ? "Sound" : "Graphics"))
        << std::endl;

    // If type is Sound or All, ensure SoundManager unloads all sounds first
    if (type == Resource_Type::Sound) {
        std::cout << "[Resource_Manager] Stopping and unloading all sounds..." << std::endl;
        SoundManager::getInstance().shutdown();
        std::cout << "[Resource_Manager] All sounds unloaded." << std::endl;
    }

    // If type is Graphics or All, call Graphics cleanup
    if (type == Resource_Type::Graphics) {
        std::cout << "[Resource_Manager] Cleaning up graphics..." << std::endl;
        gfx::Graphics::cleanup();
        std::cout << "[Resource_Manager] Graphics cleanup complete." << std::endl;
    }

    // Iterate resources map and remove entries of the requested type
    for (auto it = resources_map.begin(); it != resources_map.end(); ) 
    {
        Resources res = it->second;
        bool matchType = (type == Resource_Type::All || res.type == type);
        if (matchType) {
            std::cout << "[Resource_Manager] Removing resource: " << it->first << std::endl;
            it = resources_map.erase(it); // erase and move forward
        }
        else {++it;}
    }
    std::cout << "[Resource_Manager] UnloadAll finished." << std::endl;
}





    

