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
#include <iostream>
#include <unordered_set>
#include <vector>

namespace Framework
{
    namespace
    {
        std::filesystem::path CanonicalIfPossible(const std::filesystem::path& p)
        {
            std::error_code ec;
            auto canonical = std::filesystem::weakly_canonical(p, ec);
            return ec ? p : canonical;
        }

        // Collect all matching directories near current/executable paths so we can
          // pick the most complete candidate (helps packaged builds that ship with a
          // partial assets folder alongside the executable).
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
                for (int up = 0; up < 7 && !probe.empty(); ++up)
                {
                    consider(probe / dirname);
                    probe = probe.parent_path();
                }
            }

            return found;
        }

        bool HasSubdirectory(const std::filesystem::path& root, const char* child)
        {
            std::error_code ec;
            auto candidate = root / child;
            return std::filesystem::exists(candidate, ec)
                && std::filesystem::is_directory(candidate, ec);
        }

        int ScoreAssetsRoot(const std::filesystem::path& root)
        {
            int score = 0;
            if (HasSubdirectory(root, "Textures"))
                score += 2; // Textures are the main bulk of visual assets
            if (HasSubdirectory(root, "Fonts"))
                score += 1;
            return score;
        }

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

            return root.empty() ? rel : (root / rel);
        }
    } // anonymous namespace

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

    std::filesystem::path FindNearestDirectory(std::string_view dirname)
    {
        auto candidates = CollectNearbyDirectories(dirname);
        return candidates.empty() ? std::filesystem::path{} : candidates.front();
    }

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

    std::filesystem::path FindDataFilesRoot()
    {
        namespace fs = std::filesystem;

        if (auto nearest = FindNearestDirectory("Data_Files"); !nearest.empty())
            return nearest;

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

    std::filesystem::path ResolveAssetPath(const std::filesystem::path& relative)
    {
        return ResolveAgainstRoot(FindAssetsRoot(), relative, "assets");
    }

    std::filesystem::path ResolveDataPath(const std::filesystem::path& relative)
    {
        return ResolveAgainstRoot(FindDataFilesRoot(), relative, "Data_Files");
    }
}