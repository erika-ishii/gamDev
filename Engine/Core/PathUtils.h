#pragma once

#include <filesystem>
#include <string_view>

namespace Framework
{
    /// \brief Return the directory of the currently running executable.
    std::filesystem::path GetExecutableDir();

    /// \brief Probe upwards from CWD and exe dir to locate the first matching directory name.
    std::filesystem::path FindNearestDirectory(std::string_view dirname);

    /// \brief Locate the root assets/ directory (if any).
    std::filesystem::path FindAssetsRoot();

    /// \brief Locate the root Data_Files/ directory (if any).
    std::filesystem::path FindDataFilesRoot();

    /// \brief Resolve a relative path inside assets/ with fallback probing.
    std::filesystem::path ResolveAssetPath(const std::filesystem::path& relative);

    /// \brief Resolve a relative path inside Data_Files/ with fallback probing.
    std::filesystem::path ResolveDataPath(const std::filesystem::path& relative);
}