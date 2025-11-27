/*********************************************************************************************
 \file      PathUtils.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Declares cross-platform path utility functions for resolving engine resources.
 \details   This header exposes helpers to:
            - Query the directory of the currently running executable.
            - Search for nearby directories named "assets" or "Data_Files" starting from
              both the current working directory and the executable directory.
            - Locate the most appropriate assets and Data_Files roots using simple
              heuristics (see PathUtils.cpp).
            - Resolve relative asset/data paths against those discovered roots, with
              fallback probing for common layouts.
            These utilities allow the engine and editor to find resources reliably
            across different build configurations (Debug/Release), IDEs, and packaged
            game distributions without hardcoding absolute paths.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <filesystem>
#include <string_view>

namespace Framework
{
    /*************************************************************************************
      \brief Return the directory of the currently running executable.

      The implementation is platform-specific:
      - Windows: uses GetModuleFileNameA().
      - macOS:   uses _NSGetExecutablePath().
      - Linux:   uses readlink("/proc/self/exe").

      If all platform-specific queries fail, it falls back to std::filesystem::current_path().

      \return A std::filesystem::path pointing to the executable's parent directory.
    *************************************************************************************/
    std::filesystem::path GetExecutableDir();

    /*************************************************************************************
      \brief Probe upwards from the current working directory and executable directory
             to locate the first matching directory name.

      Starting from:
      - std::filesystem::current_path()
      - GetExecutableDir()

      this function walks up a limited number of parent directories and checks for a
      child directory named \c dirname at each level. The first discovered directory
      (in search order) is returned.

      \param dirname Name of the directory to search for (e.g., "assets", "Data_Files").
      \return Path to the nearest matching directory, or an empty path if none are found.
    *************************************************************************************/
    std::filesystem::path FindNearestDirectory(std::string_view dirname);

    /*************************************************************************************
      \brief Locate the root \c assets/ directory (if any).

      Uses a combination of:
      - Nearby directory probing (using both current path and executable path).
      - Simple scoring heuristics that prefer more "complete" assets roots (e.g.,
        those containing "Textures", "Fonts", etc.).

      \return Canonicalized path to the best candidate assets root, or an empty path.
    *************************************************************************************/
    std::filesystem::path FindAssetsRoot();

    /*************************************************************************************
      \brief Locate the root \c Data_Files/ directory (if any).

      Uses a combination of:
      - Nearby directory probing (using both current path and executable path).
      - Heuristics that prefer source-tree copies over build outputs (e.g., folders
        adjacent to ".git" or "Engine").

      \return Canonicalized path to the best candidate Data_Files root, or an empty path.
    *************************************************************************************/
    std::filesystem::path FindDataFilesRoot();

    /*************************************************************************************
      \brief Resolve a relative path inside the \c assets/ tree with fallback probing.

      First locates the assets root via FindAssetsRoot(), then attempts to resolve
      \c relative against that root and a few common relative layouts. The result is
      typically an absolute or canonicalized path suitable for file I/O.

      \param relative Path relative to the assets hierarchy (e.g. "Textures/player.png").
      \return Resolved path to the asset, or a best-effort combination if no candidate exists.
    *************************************************************************************/
    std::filesystem::path ResolveAssetPath(const std::filesystem::path& relative);

    /*************************************************************************************
      \brief Resolve a relative path inside the \c Data_Files/ tree with fallback probing.

      First locates the Data_Files root via FindDataFilesRoot(), then attempts to
      resolve \c relative against that root and a few common relative layouts. The
      result is typically an absolute or canonicalized path suitable for file I/O.

      \param relative Path relative to the Data_Files hierarchy (e.g. "level1.json").
      \return Resolved path to the data file, or a best-effort combination if no candidate exists.
    *************************************************************************************/
    std::filesystem::path ResolveDataPath(const std::filesystem::path& relative);
}