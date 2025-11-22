/*********************************************************************************************
 \file      WindowConfig.cpp
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Implementation of the LoadWindowConfig function, which reads a JSON configuration
            file and populates a WindowConfig structure with window properties such as width,
            height, and title.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "WindowConfig.h"
#include "Core/PathUtils.h"
#include <filesystem>
#include <iostream>
#include <vector>
/*********************************************************************************************
  \brief Loads window configuration from a JSON file.
  \param filename Path to the JSON configuration file.
  \return WindowConfig structure populated with data from the file.
*********************************************************************************************/
WindowConfig LoadWindowConfig(const std::string& filename)
{
//    std::ifstream file(filename);
//    if (!file.is_open()) {throw std::runtime_error("Could not open " + filename);}
//    nlohmann::json j = nlohmann::json::object();
//    file >> j;  
//    WindowConfig WConfig;
//    if (!j.contains("window")) {throw std::runtime_error("JSON does not contain 'window' object");}
//    WConfig.width  = j["window"]["width"].get<int>();
//    WConfig.height = j["window"]["height"].get<int>();
//    WConfig.title  = j["window"]["title"].get<std::string>();
//    if (j["window"].contains("fullscreen"))
//        WConfig.fullscreen = j["window"]["fullscreen"].get<bool>();
//    return WConfig;

namespace fs = std::filesystem;

std::vector<fs::path> candidates;
candidates.emplace_back(filename);
candidates.emplace_back(Framework::ResolveDataPath(filename));

// If the caller passed a bare filename, also try to resolve it inside Data_Files.
fs::path bareName(filename);
if (!bareName.has_parent_path())
candidates.emplace_back(Framework::ResolveDataPath(bareName));

for (const auto& path : candidates)
{
    std::ifstream file(path);
    if (!file.is_open())
        continue;

    nlohmann::json j = nlohmann::json::object();
    file >> j;
    WindowConfig WConfig;
    if (!j.contains("window"))
    {
        std::cerr << "[WindowConfig] JSON does not contain 'window' object: " << path << "\n";
        return {};
    }

    WConfig.width = j["window"]["width"].get<int>();
    WConfig.height = j["window"]["height"].get<int>();
    WConfig.title = j["window"]["title"].get<std::string>();
    if (j["window"].contains("fullscreen"))
        WConfig.fullscreen = j["window"]["fullscreen"].get<bool>();
    return WConfig;
}

std::cerr << "[WindowConfig] Could not open config file. Tried: " << filename << " and resolved Data_Files fallback.\n";
return {};
}