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
/*********************************************************************************************
  \brief Loads window configuration from a JSON file.
  \param filename Path to the JSON configuration file.
  \return WindowConfig structure populated with data from the file.
*********************************************************************************************/
WindowConfig LoadWindowConfig(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {throw std::runtime_error("Could not open " + filename);}
    nlohmann::json j = nlohmann::json::object();
    file >> j;  
    WindowConfig WConfig;
    if (!j.contains("window")) {throw std::runtime_error("JSON does not contain 'window' object");}
    WConfig.width  = j["window"]["width"].get<int>();
    WConfig.height = j["window"]["height"].get<int>();
    WConfig.title  = j["window"]["title"].get<std::string>();
    return WConfig;
}