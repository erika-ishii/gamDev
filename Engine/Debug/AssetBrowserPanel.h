/*********************************************************************************************
 \file      AssetBrowserPanel.h
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Content browser panel for navigating, previewing, and importing project assets.
 \details   Manages an assets root, renders a thumbnail/grid view, supports drag-and-drop
            imports and texture replacement, and caches small previews for quick browsing.
            Paths are handled relative to the configured assets root where possible.
            Status messages are surfaced inline for quick feedback during import/replace.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>

namespace mygame {

    /**
     * \class AssetBrowserPanel
     * \brief Lightweight panel for browsing project assets and queuing imports.
     */
    class AssetBrowserPanel {
    public:
        ~AssetBrowserPanel();

        // Set the root folder and reset internal state.
        void Initialize(const std::filesystem::path& assetsRoot);

        // Draw the panel UI 
        void Draw();

        
        std::size_t QueueExternalFiles(const std::vector<std::filesystem::path>& files);

   
        std::vector<std::filesystem::path> ConsumePendingImports();
        const std::filesystem::path& AssetsRoot() const { return m_assetsRoot; }

    private:
        struct Entry {
            std::filesystem::path path;
            bool isDirectory = false;
        };

        // Directory & selection handling
        void RefreshEntries();
        bool DrawEntry(const Entry& entry, float cellSize,
            bool& needsRefresh,
            std::filesystem::path& newDir);
        void ClearSelectionIfInvalid();
        bool IsSelected(const std::filesystem::path& path) const;

        // Import / Replace helpers
        std::filesystem::path ResolveImportTarget(const std::filesystem::path& file) const;
        static bool IsTextureFile(const std::filesystem::path& path);
        static bool IsAudioFile(const std::filesystem::path& path);
        static std::string TrimCopy(std::string value);
        static std::vector<std::filesystem::path> ParseInputPaths(const char* buffer);
        static bool IsPathInside(const std::filesystem::path& base, const std::filesystem::path& candidate);
        void DrawImportPopup();
        void DrawReplacePopup();
        void AddPendingImport(const std::filesystem::path& relativePath);
        bool ReplaceTextureAsset(const std::filesystem::path& target, const std::filesystem::path& newFile);

        // UI feedback
        void DrawStatusLine();
        void SetStatus(const std::string& message, bool isError);

        // Preview cache (small textures for thumbnails)
        struct PreviewTexture {
            unsigned int textureId = 0;
            int width = 0;
            int height = 0;
        };

        const PreviewTexture* GetTexturePreview(const std::filesystem::path& path);
        void PrunePreviewCache();
        void ClearPreviewCache();
        void RemovePreviewForPath(const std::filesystem::path& path);
        static std::string PathKey(const std::filesystem::path& path);

        // State
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

        std::unordered_map<std::string, PreviewTexture> m_previewCache;
    };

} // namespace mygame
