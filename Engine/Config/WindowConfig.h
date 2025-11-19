/*********************************************************************************************
 \file      WindowConfig.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of the WindowConfig structure and the LoadWindowConfig function.
            The structure holds basic window properties (width, height, title), and the 
            function loads these properties from a JSON configuration file.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <iostream>
#include <fstream>
#pragma warning(push)           // Save current warning state
#pragma warning(disable:26819) 
#include "../ThirdParty/json_dep/json.hpp"
#pragma warning(pop) 
/*********************************************************************************************
  \struct WindowConfig
  \brief Holds basic configuration data for a window.
*********************************************************************************************/
struct WindowConfig
{
    int width{};  ///Width of the window in pixels
    int height{}; ///Height of the window in pixels
    std::string title{};///Window title text
    bool fullscreen{ true }; ///Launch in fullscreen by default
};
WindowConfig LoadWindowConfig(const std::string& filename);
