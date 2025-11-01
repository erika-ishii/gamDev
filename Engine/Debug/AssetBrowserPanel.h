#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mygame {

    class AssetBrowserPanel {
    public:
        void Initialize(const std::filesystem::path& assetsRoot);
        void Draw();

        void QueueExternalFiles(const std::vector<std::filesystem::path>& files);
        std::vector<std::filesystem::path> ConsumePendingImports();

        const std::filesystem::path& AssetsRoot() const { return m_assetsRoot; }

    private:
        struct Entry {
            std::filesystem::path path;
            bool isDirectory = false;
        };

        void RefreshEntries();
        void DrawEntry(const Entry& entry, float cellSize);
        std::filesystem::path ResolveImportTarget(const std::filesystem::path& file) const;
        static bool IsTextureFile(const std::filesystem::path& path);
        static bool IsAudioFile(const std::filesystem::path& path);

        std::filesystem::path m_assetsRoot;
        std::filesystem::path m_currentDir;
        std::vector<Entry> m_entries;
        std::vector<std::filesystem::path> m_pendingImports;
    };

} // namespace mygame