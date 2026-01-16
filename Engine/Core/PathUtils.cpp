/*********************************************************************************************
 \file      PathUtils.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Implements cross-platform helpers for resolving asset and data file paths.
 \details   Responsibilities:
            - Determine the directory of the current executable (Windows/macOS/Linux).
            - Search for nearby directories named "assets" or "Data_Files" starting from
              both the current working directory and the executable directory.
            - Use simple scoring heuristics to prefer the "best" candidate root (e.g.
              repo copy vs. copied build output).
            - Resolve relative asset/data paths against the discovered roots, with
              fallbacks for typical relative layouts.
 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#define _CRT_SECURE_NO_WARNINGS
#include "PathUtils.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <Windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include <array>
#include <cstdlib>
#include <iostream>
#include <unordered_set>
#include <vector>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework
{
    namespace
    {
        /*************************************************************************************
          \brief  Return the weakly canonical version of a path if possible.

          Uses std::filesystem::weakly_canonical, but falls back to the input path if an
          error occurs (so callers do not have to handle exceptions or error codes).
        *************************************************************************************/
        std::filesystem::path CanonicalIfPossible(const std::filesystem::path& p)
        {
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(p, ec);
            return ec ? p : canonical;
        }

        /*************************************************************************************
          \brief Collect all matching directories named \c dirname near key roots.

          Starting from:
          - std::filesystem::current_path()
          - GetExecutableDir()

          the search will go "up" a small number of parent levels and probe for a child
          directory named \c dirname at each step (e.g., root/dirname, root/../dirname,
          root/../../dirname, ...).

          Each successfully found directory is canonicalized (if possible) and deduplicated
          using a set of generic string paths.

          \param dirname Name of the directory to search for (e.g. "assets", "Data_Files").
          \return A list of unique candidate directories, ordered by discovery.
        *************************************************************************************/
        std::vector<std::filesystem::path> CollectNearbyDirectories(std::string_view dirname)
        {
            namespace fs = std::filesystem;
            std::vector<fs::path> found;
            std::vector<fs::path> roots{ fs::current_path(), GetExecutableDir() };
            std::unordered_set<std::string> seen;

            auto consider = [&](const fs::path& p)
                {
                    if (p.empty())
                        return;

                    std::error_code ec;
                    if (!fs::exists(p, ec) || !fs::is_directory(p, ec))
                        return;

                    auto canonical = CanonicalIfPossible(p);
                    const std::string key = canonical.generic_string();
                    if (seen.insert(key).second)
                        found.emplace_back(std::move(canonical));
                };

            for (const auto& root : roots)
            {
                if (root.empty())
                    continue;

                auto probe = root;
                // Walk up to 7 levels up the directory tree, probing at each step.
                for (int up = 0; up < 7 && !probe.empty(); ++up)
                {
                    consider(probe / dirname);
                    probe = probe.parent_path();
                }
            }

            return found;
        }

        /*************************************************************************************
           \brief Attempt to create and canonicalize a directory.

           Returns an empty path if creation fails, otherwise the canonicalized directory.
         *************************************************************************************/
        std::filesystem::path EnsureCanonicalDir(const std::filesystem::path& candidate)
        {
            if (candidate.empty())
                return {};

            std::error_code ec;
            if (!std::filesystem::exists(candidate, ec))
                std::filesystem::create_directories(candidate, ec);

            if (ec)
                return {};

            return CanonicalIfPossible(candidate);
        }

        /*************************************************************************************
          \brief Check if \c root contains a subdirectory named \c child.

          \param root  Directory to start from.
          \param child Name of the child directory.
          \return True if root/child exists and is a directory.
        *************************************************************************************/
        bool HasSubdirectory(const std::filesystem::path& root, const char* child)
        {
            std::error_code ec;
            auto candidate = root / child;
            return std::filesystem::exists(candidate, ec)
                && std::filesystem::is_directory(candidate, ec);
        }

        /*************************************************************************************
          \brief Check if \c root has a sibling directory named \c sibling.

          Example: HasSiblingDirectory("/repo/Data_Files", ".git") will check for
          "/repo/.git".

          \param root    Base path whose parent will be probed.
          \param sibling Name of the sibling directory.
          \return True if parent(root)/sibling exists and is a directory.
        *************************************************************************************/
        bool HasSiblingDirectory(const std::filesystem::path& root, const char* sibling)
        {
            std::error_code ec;
            auto candidate = root.parent_path() / sibling;
            return std::filesystem::exists(candidate, ec)
                && std::filesystem::is_directory(candidate, ec);
        }

        /*************************************************************************************
          \brief Heuristic scoring for an assets root candidate.

          Currently prefers:
          - Directories with a "Textures" subdirectory (worth 2 points).
          - Directories with a "Fonts" subdirectory (worth 1 point).

          \param root Candidate assets directory.
          \return Integer score; higher is considered more likely to be the "main" assets.
        *************************************************************************************/
        int ScoreAssetsRoot(const std::filesystem::path& root)
        {
            int score = 0;
            if (HasSubdirectory(root, "Textures"))
                score += 2; // Textures are the main bulk of visual assets
            if (HasSubdirectory(root, "Fonts"))
                score += 1;
            return score;
        }

        /*************************************************************************************
          \brief Heuristic scoring for a Data_Files root candidate.

          Prefers:
          - Folders that live next to a ".git" directory (likely the source repo copy).
          - Folders that live next to an "Engine" directory (top-level tree vs. build copy).

          \param root Candidate Data_Files directory.
          \return Integer score; higher is more likely to be the canonical source tree.
        *************************************************************************************/
        int ScoreDataFilesRoot(const std::filesystem::path& root)
        {
            int score = 0;

            // Prefer the repository copy of Data_Files when available so edits are
            // picked up immediately without needing a rebuild/recopy step.
            if (HasSiblingDirectory(root, ".git"))
                score += 3;

            // Fallback heuristic: Data_Files that sits next to the top-level CMakeLists
            // is more likely to be the canonical source tree than a copied build output.
            if (HasSiblingDirectory(root, "Engine"))
                score += 1;

            return score;
        }

        /*************************************************************************************
          \brief Resolve \c rel against a specific root and some fallback relative hints.

          Primary candidate:
          - root / rel  (if root is non-empty)

          Fallback candidates (searched in order):
          - "" / dirname / rel
          - ".." / dirname / rel
          - "../.." / dirname / rel
          - "../../.." / dirname / rel

          Each candidate is checked for existence and canonicalized on success.

          \param root Base directory chosen as the best candidate root.
          \param rel  Relative path inside that root (e.g. "player.png").
          \param dirname Directory name to use when probing fallback hints
                         (e.g. "assets", "Data_Files").
          \return A canonicalized path if a candidate exists; otherwise root / rel or rel.
        *************************************************************************************/
        std::filesystem::path ResolveAgainstRoot(const std::filesystem::path& root,
            const std::filesystem::path& rel,
            std::string_view dirname)
        {
            namespace fs = std::filesystem;
            std::vector<fs::path> candidates;
            if (!root.empty())
            {
                candidates.emplace_back(root / rel);
            }

            std::array<const char*, 4> relHints{
                "", "..", "../..", "../../.."
            };

            for (auto hint : relHints)
            {
                fs::path candidate = fs::path(hint) / dirname / rel;
                candidates.emplace_back(candidate);
            }

            for (const auto& candidate : candidates)
            {
                std::error_code ec;
                if (fs::exists(candidate, ec))
                    return CanonicalIfPossible(candidate);
            }

            // Fall back to either rel or root/rel if nothing exists yet.
            return root.empty() ? rel : (root / rel);
        }
    } // anonymous namespace

    /*************************************************************************************
      \brief Get the directory of the current executable (platform-specific).

      Windows:
      - Uses GetModuleFileNameA(nullptr, ...).

      macOS:
      - Uses _NSGetExecutablePath() via <mach-o/dyld.h>, with fallback buffer sizing.

      Linux/Unix (non-Apple):
      - Uses readlink("/proc/self/exe", ...) to retrieve the path.

      If any of these mechanisms fail, falls back to std::filesystem::current_path().

      \return std::filesystem::path pointing to the directory containing the executable.
    *************************************************************************************/
    std::filesystem::path GetExecutableDir()
    {
#if defined(_WIN32)
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path();
#elif defined(__APPLE__)
        char buf[2048];
        uint32_t sz = sizeof(buf);
        if (_NSGetExecutablePath(buf, &sz) == 0) return std::filesystem::path(buf).parent_path();
        std::string s; s.resize(sz);
        if (_NSGetExecutablePath(s.data(), &sz) == 0) return std::filesystem::path(s).parent_path();
        return std::filesystem::current_path();
#else
        char buf[4096] = {};
        ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n > 0) { buf[n] = 0; return std::filesystem::path(buf).parent_path(); }
        return std::filesystem::current_path();
#endif
    }

    /*************************************************************************************
      \brief Find the nearest directory named \c dirname around current/executable roots.

      Uses CollectNearbyDirectories() and returns the first discovered candidate
      (which is already canonicalized and deduplicated by that helper).

      \param dirname Name of directory to search for (e.g. "assets").
      \return Path of the nearest matching directory, or an empty path if none found.
    *************************************************************************************/
    std::filesystem::path FindNearestDirectory(std::string_view dirname)
    {
        auto candidates = CollectNearbyDirectories(dirname);
        return candidates.empty() ? std::filesystem::path{} : candidates.front();
    }

    /*************************************************************************************
      \brief Locate the "best" assets root directory.

      Strategy:
      - Use CollectNearbyDirectories("assets") to find all candidate "assets" directories
        near the current path and executable path.
      - Score each candidate with ScoreAssetsRoot() to prefer the most complete folder
        (containing "Textures", "Fonts", etc.).
      - If no candidates pass the heuristic, fall back to a few common relative paths:
        "assets", "../assets", "../../assets", "../../../assets".

      \return Canonicalized assets root path, or an empty path if nothing is found.
    *************************************************************************************/
    std::filesystem::path FindAssetsRoot()
    {
        namespace fs = std::filesystem;

        auto candidates = CollectNearbyDirectories("assets");

        // Pick the candidate that actually contains textures/fonts (likely the full repo
        // assets directory). If none match the heuristic, fall back to the nearest one.
        int bestScore = -1;
        fs::path best;
        for (const auto& c : candidates)
        {
            const int score = ScoreAssetsRoot(c);
            if (score > bestScore)
            {
                bestScore = score;
                best = c;
            }
        }

        if (!best.empty())
            return CanonicalIfPossible(best);

        // Fallback: probe some common relative paths from the current directory.
        static const char* rels[] = {
            "assets",
            "../assets",
            "../../assets",
            "../../../assets"
        };

        for (auto rel : rels)
        {
            fs::path candidate = rel;
            std::error_code ec;
            if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec))
                return CanonicalIfPossible(candidate);
        }

        return {};
    }

    /*************************************************************************************
      \brief Locate the "best" Data_Files root directory.

      Strategy:
      - Use CollectNearbyDirectories("Data_Files") to find all candidates.
      - Score each candidate with ScoreDataFilesRoot(), preferring repo copies over
        build copies (e.g. those adjacent to ".git" and "Engine").
      - If no scored candidate is found, fall back to common relative layouts:
        "Data_Files", "../Data_Files", "../../Data_Files", "../../../Data_Files".

      \return Canonicalized Data_Files root path, or an empty path if nothing is found.
    *************************************************************************************/
    std::filesystem::path FindDataFilesRoot()
    {
        namespace fs = std::filesystem;

        auto candidates = CollectNearbyDirectories("Data_Files");

        int bestScore = -1;
        fs::path best;
        for (const auto& c : candidates)
        {
            const int score = ScoreDataFilesRoot(c);
            if (score > bestScore)
            {
                bestScore = score;
                best = c;
            }
        }

        if (!best.empty())
            return CanonicalIfPossible(best);

        static const char* rels[] = {
            "Data_Files",
            "../Data_Files",
            "../../Data_Files",
            "../../../Data_Files"
        };

        for (auto rel : rels)
        {
            fs::path candidate = rel;
            std::error_code ec;
            if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec))
                return CanonicalIfPossible(candidate);
        }

        return {};
    }

    /*************************************************************************************
      \brief Resolve an asset-relative path against the discovered assets root.

      First calls FindAssetsRoot() to locate a base directory, then uses
      ResolveAgainstRoot() to check a few likely layouts and return a canonical path.

      \param relative Relative path into the assets hierarchy (e.g. "Textures/player.png").
      \return Resolved absolute (or canonical) path to the asset.
    *************************************************************************************/
    std::filesystem::path ResolveAssetPath(const std::filesystem::path& relative)
    {
        return ResolveAgainstRoot(FindAssetsRoot(), relative, "assets");
    }

    /*************************************************************************************
      \brief Resolve a data-relative path against the discovered Data_Files root.

      First calls FindDataFilesRoot() to locate a base directory, then uses
      ResolveAgainstRoot() to check a few likely layouts and return a canonical path.

      \param relative Relative path into the Data_Files hierarchy (e.g. "level1.json").
      \return Resolved absolute (or canonical) path to the data file.
    *************************************************************************************/
    std::filesystem::path ResolveDataPath(const std::filesystem::path& relative)
    {
        return ResolveAgainstRoot(FindDataFilesRoot(), relative, "Data_Files");
    }

      /*************************************************************************************
      \brief Return a user-writable Documents directory (or best-effort fallback).

      Prioritizes the current user's Documents directory to avoid admin-only locations.
      If unavailable, attempts the Windows Public Documents folder, then the user's home
      directory, and finally the current working directory.

      The function ensures the returned directory exists so callers can immediately write
      subdirectories/files without additional checks.
    *************************************************************************************/
    std::filesystem::path GetUserDocumentsDir()
    {
        namespace fs = std::filesystem;

#ifdef _WIN32
        if (auto path = EnsureCanonicalDir(fs::path(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "") / "Documents"); !path.empty())
            return path;

        if (auto path = EnsureCanonicalDir(fs::path(std::getenv("PUBLIC") ? std::getenv("PUBLIC") : "") / "Documents"); !path.empty())
            return path;
#else
        if (const char* home = std::getenv("HOME"))
        {
            if (auto docs = EnsureCanonicalDir(fs::path(home) / "Documents"); !docs.empty())
                return docs;

            if (auto homeDir = EnsureCanonicalDir(fs::path(home)); !homeDir.empty())
                return homeDir;
        }
#endif

        return CanonicalIfPossible(fs::current_path());
    }
}
