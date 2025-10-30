/*********************************************************************************************
 \file      Resource_Manager.h
 \par       SofaSpuds
 \author    jianwei.c (jianwei.c@digipen.edu) - Primary Author, 100%

 \brief     Declaration of the Resource_Manager class, which provides a centralized system 
            for loading, tracking, and unloading game resources. Supports textures, fonts, 
            graphics, and sounds. Also includes utility functions for file extension checks 
            and type validation.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include "../Audio/AudioManager.h"
#include "../Audio/SoundManager.h"
#include "../Graphics/Graphics.hpp"
#include <string>
#include <filesystem>
#include <algorithm>
#include <iostream>
/*****************************************************************************************
  \class Resource_Manager
  \brief Provides centralized management of game resources such as textures, fonts, 
         graphics, and sounds.

  The Resource_Manager class is a static utility that handles loading, tracking, and 
  unloading of resources used throughout the game. Resources are stored in an internal 
  map and can be retrieved or released as needed.
*****************************************************************************************/
class Resource_Manager
{
public:
  /*****************************************************************************************
  \enum Resource_Type
  \brief Describes the types of resources supported by the Resource_Manager.

  - Texture  : Image files used for rendering sprites and backgrounds.  
  - Font     : Font files used for text rendering.  
  - Graphics : General graphics objects (shaders, pipelines, etc.).  
  - Sound    : Audio resources (music or sound effects).  
  - All      : Represents all resource types, mainly used when unloading all resources.
  *****************************************************************************************/
    enum Resource_Type { Texture, Font, Graphics, Sound, All };
  /*****************************************************************************************
  \struct Resources
  \brief Represents a single resource managed by the Resource_Manager.

  Each resource has:
  - id     : A unique string identifier for lookup (e.g., "player_texture").  
  - type   : The type of resource (Texture, Font, Graphics, or Sound).  
  - handle : A numeric handle or pointer referring to the actual loaded resource 
             in memory or the graphics/audio system.
  *****************************************************************************************/
    struct Resources 
    { 
        std::string id{}; ///Unique identifier for the resource
        Resource_Type type{ Resource_Type::All }; /// Type of the resource
        unsigned int handle{};  ///Handle or pointer to the actual resource
    };
    static bool load(const std::string& name, const std::string& path);
    static void loadAll(const std::string& directory);
    static void unloadAll(Resource_Type type); 
    /// Map storing all loaded resources with unique identifiers
    static inline std::unordered_map<std::string, Resources> resources_map;
    static inline std::string GetExtension(const std::string& path);
    static bool isTexture(const std::string& ext);
    static bool isSound(const std::string& ext);
    static unsigned int getTexture(const std::string& key);
};
