#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <array>

namespace mygame {

    class AssetBrowserPanel {
    public:
        void Initialize(const std::filesystem::path& assetsRoot);
        void Draw();

        std::size_t QueueExternalFiles(const std::vector<std::filesystem::path>& files);
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
        static std::string TrimCopy(std::string value);
        static std::vector<std::filesystem::path> ParseInputPaths(const char* buffer);
        static bool IsPathInside(const std::filesystem::path& base, const std::filesystem::path& candidate);

        void DrawImportPopup();
        void DrawReplacePopup();
        void DrawStatusLine();
        void ClearSelectionIfInvalid();
        void AddPendingImport(const std::filesystem::path& relativePath);
        bool ReplaceTextureAsset(const std::filesystem::path& target, const std::filesystem::path& newFile);
        void SetStatus(const std::string& message, bool isError);

        bool IsSelected(const std::filesystem::path& path) const;
        std::filesystem::path m_assetsRoot;
        std::filesystem::path m_currentDir;
        std::filesystem::path m_selectedEntry;
        std::vector<Entry> m_entries;
        std::vector<std::filesystem::path> m_pendingImports;
        std::array<char, 512> m_importBuffer{};
        std::array<char, 512> m_replaceBuffer{};
        std::filesystem::path m_pendingReplaceTarget;
        std::filesystem::path m_pendingReplaceSource;
        std::string m_replaceError;
        bool m_openImportPopup = false;
        bool m_openReplacePopup = false;

        std::string m_statusMessage;
        bool m_statusIsError = false;
    };

} // namespace mygame