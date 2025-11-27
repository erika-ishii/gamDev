/*********************************************************************************************
 \file      AssetBrowserPanel.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     ImGui-based content browser for project assets (textures, audio, folders).
 \details   This module implements a small, robust asset browser panel with the following:
            - Navigation: displays folders/files under a configured assets root; safe relative
              path handling across different drives/roots.
            - Thumbnails: lazy texture preview cache for .png/.jpg/.jpeg with auto-pruning.
            - Drag & Drop: exposes texture paths for external UI to consume.
            - Import/Replace: queue OS files for import; replace a selected texture with
              overwrite confirmation and status line feedback (success/error).
            - Audio helper: modal that lists .wav/.mp3 files in the current folder.
            - Status line: user feedback after operations (import/replace/no-op), colored by
              success/error.
           
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "Debug/AssetBrowserPanel.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <imgui.h>
#include <system_error>
#include <unordered_set>
#include <iomanip>
#include <sstream>

#include "Graphics/Graphics.hpp"
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
/*************************************************************************************
  \brief  Canonicalize a path to a stable, human-readable string (generic form).
  \param  p Filesystem path.
  \return Generic (forward-slash) string of canonical or original (if canonicalization
          fails). Ensures stable IDs for ImGui and preview cache keys.
*************************************************************************************/
static std::string SafePathString(const std::filesystem::path& p) {
    if (p.empty()) return {};
    std::error_code ec;
    auto canon = std::filesystem::weakly_canonical(p, ec);
    const auto& use = ec ? p : canon;
    // generic_string() is fine here; we avoid lexically_normal()
    return use.generic_string();
}

/*************************************************************************************
  \brief  Compute a friendly relative path name from base->p, with fallbacks.
  \param  base Base directory (e.g., assets root).
  \param  p    Target path.
  \return Relative string unless it would escape with ".." or crosses different roots;
          falls back to filename() for safety and readability.
*************************************************************************************/
//This guarantees we never ask the STL to compute a weird relative path between 
// incompatible roots, and we always fall back to a safe, human - readable string.
static std::string SafeRelative(const std::filesystem::path& base,
    const std::filesystem::path& p)
{
    if (p.empty()) return {};

    std::error_code ec;
    auto b = std::filesystem::weakly_canonical(base, ec);
    if (ec) b = base;

    ec.clear();
    auto c = std::filesystem::weakly_canonical(p, ec);
    if (ec) c = p;

    // Different drive letters / roots? Don’t try to make a relative path.
    if (b.has_root_name() && c.has_root_name() && b.root_name() != c.root_name())
        return p.filename().string();

    auto rel = c.lexically_relative(b);
    auto s = rel.generic_string();

    // If the “relative” result is empty or escapes upwards, show a friendly name instead.
    if (s.empty() || s.rfind("..", 0) == 0)
        return p.filename().string();

    return s;
}

namespace mygame {

    namespace {
        constexpr float kThumbnailSize = 96.0f;
        constexpr float kPadding = 16.0f;

        /*************************************************************************************
          \brief  Lowercase copy using non-overlapping output buffer (MSVC debug-friendly).
        *************************************************************************************/
        std::string ToLower(std::string value)
        {
            std::string out;
            out.resize(value.size());
            std::transform(value.begin(), value.end(), out.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        // -------- Audio Library modal -----------------------------------------------------

        struct AudioPopupState {
            bool openRequest = false;                              // request to open next frame
            std::filesystem::path folder;                          // current folder
            std::vector<std::filesystem::path> files;              // audio files in folder
        };
        static AudioPopupState gAudioPopup;

        /*************************************************************************************
          \brief  Prepare and request to open the Audio Files modal for current folder.
          \param  folder  The folder currently displayed by the browser.
          \param  entries The list of directory entries already gathered for the UI.
        *************************************************************************************/
        template <typename Entries>
        void OpenAudioPopupFrom(const std::filesystem::path& folder,
            const Entries& entries)
        {
            gAudioPopup.folder = folder;
            gAudioPopup.files.clear();
            gAudioPopup.files.reserve(entries.size());
            for (const auto& e : entries) {
                if (!e.isDirectory) {
                    auto ext = ToLower(e.path.extension().string());
                    if ((ext == ".wav" || ext == ".mp3") && !e.path.empty()) {
                        std::error_code ec;
                        if (std::filesystem::is_regular_file(e.path, ec) && !ec) {
                            gAudioPopup.files.emplace_back(e.path);
                        }
                    }
                }
            }
            gAudioPopup.openRequest = true;
        }

        /*************************************************************************************
          \brief  Format byte sizes as a human-friendly string with units.
          \param  sz File size in bytes.
          \return e.g., "512 B", "12.8 KB", "3.4 MB", "1 GB".
        *************************************************************************************/
        static std::string PrettySize(std::uintmax_t sz) {
            const char* units[] = { "B", "KB", "MB", "GB" };
            int u = 0;
            double val = static_cast<double>(sz);
            while (val >= 1024.0 && u < 3) { val /= 1024.0; ++u; }
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(val >= 100 ? 0 : 2) << val << " " << units[u];
            return oss.str();
        }

        /*************************************************************************************
          \brief  Draw the modal window that lists audio files in the current folder.
          \param  assetsRoot Root folder used for display of relative paths.
        *************************************************************************************/
        static void DrawAudioPopup(const std::filesystem::path& assetsRoot)
        {
            if (gAudioPopup.openRequest) {
                ImGui::OpenPopup("Audio Files");
                gAudioPopup.openRequest = false;
            }
            if (!ImGui::BeginPopupModal("Audio Files", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                return;

            // Header
            std::string relFolder = SafeRelative(assetsRoot, gAudioPopup.folder);
            ImGui::Text("Folder: %s", relFolder.c_str());
            ImGui::Separator();

            if (gAudioPopup.files.empty()) {
                ImGui::TextDisabled("No .wav or .mp3 files in this folder.");
            }
            else {
                // Simple table (name + size). Add duration/sample rate when your audio system exposes it.
                if (ImGui::BeginTable("audioTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("File");
                    ImGui::TableSetupColumn("Size");
                    ImGui::TableHeadersRow();

                    for (const auto& p : gAudioPopup.files) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        std::string name = p.filename().string();
                        ImGui::TextUnformatted(name.c_str());

                        ImGui::TableSetColumnIndex(1);
                        std::error_code ec;
                        if (p.empty()
                            || !std::filesystem::exists(p, ec) || ec
                            || !std::filesystem::is_regular_file(p, ec) || ec) {
                            ImGui::TextUnformatted("-");
                        }
                        else {
                            auto sz = std::filesystem::file_size(p, ec);
                            ImGui::TextUnformatted(ec ? "-" : PrettySize(sz).c_str());
                        }
                    }

                    ImGui::EndTable();
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
       
    } 

    /*************************************************************************************
      \brief  Destroy preview textures and clear cache on panel destruction.
    *************************************************************************************/
    AssetBrowserPanel::~AssetBrowserPanel()
    {
        ClearPreviewCache();
    }

    /*************************************************************************************
      \brief  Initialize the panel with an assets root; canonicalize and pre-scan entries.
      \param  assetsRoot Path to the assets directory to browse.
    *************************************************************************************/
    void AssetBrowserPanel::Initialize(const std::filesystem::path& assetsRoot)
    {
        ClearPreviewCache();
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(assetsRoot, ec);
        m_assetsRoot = ec ? std::filesystem::absolute(assetsRoot) : canonical;
        m_currentDir = m_assetsRoot;
        m_selectedEntry.clear();
        m_replaceBuffer.fill('\0');
        m_statusMessage.clear();
        m_statusIsError = false;
        RefreshEntries();
    }

    /*************************************************************************************
      \brief  Render the ImGui “Content Browser” window + all sub-popups/modals.
      \details Handles navigation, selection, grid layout, status line, and the texture
               Replace popup. Also triggers the Audio popup render.
    *************************************************************************************/
    void AssetBrowserPanel::Draw()
    {
        if (m_assetsRoot.empty())
            return;

        ClearSelectionIfInvalid();

        if (!ImGui::Begin("Content Browser"))
        {
            ImGui::End();
            return;
        }

        DrawStatusLine();

        ImGui::TextDisabled("Drag and drop files from your OS to add or replace assets in the current folder.");

        if (m_currentDir != m_assetsRoot)
        {
            if (ImGui::Button("<--"))
            {
                m_currentDir = m_currentDir.parent_path();
                m_selectedEntry.clear();
                RefreshEntries();
            }
            ImGui::SameLine();
        }

        const std::string header = (m_currentDir == m_assetsRoot)
            ? std::string("assets")
            : SafeRelative(m_assetsRoot, m_currentDir);
        ImGui::TextUnformatted(header.c_str());
        if (!m_selectedEntry.empty())
        {
            std::string relative = SafeRelative(m_assetsRoot, m_selectedEntry);
            if (relative.empty())
                relative = m_selectedEntry.filename().string();
            ImGui::TextDisabled("Selected: %s", relative.c_str());
        }

        float panelWidth = ImGui::GetContentRegionAvail().x;
        float cellSize = kThumbnailSize + kPadding;
        int columnCount = static_cast<int>(panelWidth / cellSize);
        if (columnCount < 1)
            columnCount = 1;

        ImGui::Columns(columnCount, nullptr, false);

        bool needsRefresh = false;
        std::filesystem::path newDirectory;

        for (const auto& entry : m_entries)
        {
            bool dirClicked = DrawEntry(entry, kThumbnailSize, needsRefresh, newDirectory);
            ImGui::NextColumn();

            if (dirClicked)
                break;  // Exit loop early if we need to navigate
        }

        // Handle navigation after the loop
        if (needsRefresh && !newDirectory.empty())
        {
            m_currentDir = newDirectory;
            m_selectedEntry.clear();
            RefreshEntries();
        }

        ImGui::Columns(1);
        DrawReplacePopup();

        // <- Audio popup (lists all audio files in current folder)
        DrawAudioPopup(m_assetsRoot);

        ImGui::End();
    }

    /*************************************************************************************
      \brief  Queue a set of absolute OS file paths for import into the current assets dir.
      \param  files Absolute file paths from the OS.
      \return Number of files imported or replaced (sum).
      \note   Copies files to ResolveImportTarget(file). Adds entries to pending-imports
              list for external systems to process. Updates status line.
    *************************************************************************************/
    std::size_t AssetBrowserPanel::QueueExternalFiles(const std::vector<std::filesystem::path>& files)
    {
        if (m_assetsRoot.empty())
            return 0u;

        std::size_t imported = 0u;
        std::size_t replaced = 0u;

        for (const auto& file : files)
        {
            std::error_code ec;
            if (!std::filesystem::exists(file, ec) || !std::filesystem::is_regular_file(file, ec))
                continue;

            std::filesystem::path destination = ResolveImportTarget(file);
            if (destination.empty())
                continue;

            ec.clear();
            bool existedBefore = std::filesystem::exists(destination, ec);
            if (ec)
                existedBefore = false;

            ec.clear();
            std::filesystem::create_directories(destination.parent_path(), ec);

            ec.clear();
            std::filesystem::copy_file(file, destination, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
                continue;

            std::error_code canonicalEc;
            auto canonical = std::filesystem::weakly_canonical(destination, canonicalEc);
            if (canonicalEc)
                canonical = destination;

            auto relative = canonical.lexically_relative(m_assetsRoot);
            if (relative.empty() || relative.generic_string().rfind("..", 0) == 0)
                continue;

            if (existedBefore)
                RemovePreviewForPath(canonical);

            AddPendingImport(relative);
            if (existedBefore)
                ++replaced;
            else
                ++imported;
        }

        if (!files.empty())
            RefreshEntries();

        if (!files.empty())
        {
            if (imported > 0 || replaced > 0)
            {
                std::string message;
                if (imported > 0)
                {
                    message += "Imported " + std::to_string(imported) + (imported == 1 ? " asset" : " assets");
                }
                if (replaced > 0)
                {
                    if (!message.empty())
                        message += " and ";
                    message += "replaced " + std::to_string(replaced) + (replaced == 1 ? " asset" : " assets");
                }
                message += ".";
                SetStatus(message, false);
            }
            else
            {
                SetStatus("No supported assets were imported.", true);
            }
        }

        return imported + replaced;
    }

    /*************************************************************************************
      \brief  Return and clear the list of unique pending imports (relative to assets root).
      \return Unique relative paths collected during the last import/replace operations.
    *************************************************************************************/
    std::vector<std::filesystem::path> AssetBrowserPanel::ConsumePendingImports()
    {
        std::unordered_set<std::string> unique;
        std::vector<std::filesystem::path> pending;
        pending.reserve(m_pendingImports.size());

        for (const auto& path : m_pendingImports)
        {
            const std::string key = path.generic_string();
            if (unique.insert(key).second)
                pending.push_back(path);
        }
        m_pendingImports.clear();
        return pending;
    }

    /*************************************************************************************
      \brief  Scan the current directory and populate sorted directory/file entries.
      \note   Skips non-regular/non-directory entries. Also prunes preview cache to avoid
              holding textures for files that are no longer visible.
    *************************************************************************************/
    void AssetBrowserPanel::RefreshEntries()
    {
        m_entries.clear();
        std::error_code ec;
        if (m_currentDir.empty() || !std::filesystem::exists(m_currentDir, ec))
            return;

        std::vector<Entry> directories;
        std::vector<Entry> files;
        std::error_code ecIt;
        for (const auto& dirEntry : std::filesystem::directory_iterator(m_currentDir, ecIt)) {
            if (ecIt) break;

            Entry entry;
            entry.path = dirEntry.path();

            std::error_code ecDir, ecReg;
            const bool isDir = dirEntry.is_directory(ecDir) && !ecDir;
            const bool isReg = dirEntry.is_regular_file(ecReg) && !ecReg;

            entry.isDirectory = isDir;
            if (isDir) directories.push_back(entry);
            else if (isReg) files.push_back(entry); // only real files go to files
            // else: skip everything else (symlinks, pipes, reparse points, etc.)
        }

        auto sortByName = [](const Entry& a, const Entry& b) {
            return ToLower(a.path.filename().string()) < ToLower(b.path.filename().string());
            };

        std::sort(directories.begin(), directories.end(), sortByName);
        std::sort(files.begin(), files.end(), sortByName);

        m_entries.reserve(directories.size() + files.size());
        m_entries.insert(m_entries.end(), directories.begin(), directories.end());
        m_entries.insert(m_entries.end(), files.begin(), files.end());

        ClearSelectionIfInvalid();
        PrunePreviewCache();
    }

    /*************************************************************************************
      \brief  Render one grid tile (folder/file). Handles selection, double-click nav,
              drag-source for textures, and audio modal trigger.
      \param  entry         Directory entry represented by the tile.
      \param  cellSize      Square tile size (pixels).
      \param  needsRefresh  Out flag: set when navigation is requested.
      \param  newDir        Out: target directory when navigating into a folder.
      \return true if navigation should occur (caller will handle it after the loop).
    *************************************************************************************/
    bool AssetBrowserPanel::DrawEntry(const Entry& entry, float cellSize,
        bool& needsRefresh,
        std::filesystem::path& newDir)
    {
        if (entry.path.empty()) {
            ImGui::TextDisabled("<invalid>");
            return false;
        }
        // Unique, stable ID per tile based on full path.
        const std::string idStr = SafePathString(entry.path);
        ImGui::PushID(idStr.c_str());

        const std::string label = entry.path.filename().string();

        const bool isDirectory = entry.isDirectory;
        const bool isTexture = !isDirectory && IsTextureFile(entry.path);
        const bool isAudio = !isDirectory && IsAudioFile(entry.path);
        const bool isInteractable = isDirectory || isTexture || isAudio;
        const bool isSelected = isInteractable && IsSelected(entry.path);

        if (isSelected)
        {
            const ImVec4 header = ImGui::GetStyleColorVec4(ImGuiCol_Header);
            const ImVec4 headerHovered = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);
            const ImVec4 headerActive = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
            ImGui::PushStyleColor(ImGuiCol_Button, header);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, headerHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, headerActive);
        }

        // Neutral button label: ID is controlled by PushID above.
        const char* buttonLabel = "##tile";
        const ImVec2 tileSize(cellSize, cellSize);
        if (!isInteractable)
            ImGui::BeginDisabled(true);

        const bool pressed = ImGui::Button(buttonLabel, tileSize);

        if (!isInteractable)
            ImGui::EndDisabled();

        if (isSelected)
            ImGui::PopStyleColor(3);

        const PreviewTexture* preview = isTexture ? GetTexturePreview(entry.path) : nullptr;
        const char* overlay = nullptr;
        if (isDirectory) overlay = "DIR";
        else if (isAudio) overlay = "AUDIO";
        else if (!preview || preview->textureId == 0) overlay = "FILE";

        const ImVec2 rectMin = ImGui::GetItemRectMin();
        const ImVec2 rectMax = ImGui::GetItemRectMax();
        if (preview && preview->textureId != 0 && preview->width > 0 && preview->height > 0)
        {
            ImVec2 displayMin = rectMin;
            ImVec2 displayMax = rectMax;
            const float areaWidth = rectMax.x - rectMin.x;
            const float areaHeight = rectMax.y - rectMin.y;
            const float aspect = static_cast<float>(preview->width) / static_cast<float>(preview->height);
            const float areaAspect = areaWidth / areaHeight;

            if (aspect > areaAspect)
            {
                const float desiredHeight = areaWidth / aspect;
                const float pad = (areaHeight - desiredHeight) * 0.5f;
                displayMin.y += pad;
                displayMax.y -= pad;
            }
            else
            {
                const float desiredWidth = areaHeight * aspect;
                const float pad = (areaWidth - desiredWidth) * 0.5f;
                displayMin.x += pad;
                displayMax.x -= pad;
            }

            ImGui::GetWindowDrawList()->AddImage(
                (ImTextureID)(void*)(intptr_t)preview->textureId,
                displayMin,
                displayMax,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));
        }
        else if (overlay)
        {
            const ImVec2 textSize = ImGui::CalcTextSize(overlay);
            const ImVec2 textPos(rectMin.x + (rectMax.x - rectMin.x - textSize.x) * 0.5f,
                rectMin.y + (rectMax.y - rectMin.y - textSize.y) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), overlay);
        }
        // Drag-drop: allow textures and audio to be used by other panels
        if (isTexture || isAudio)
        {
            std::string payloadPath = SafeRelative(m_assetsRoot, entry.path);
            if (payloadPath.empty())
                payloadPath = entry.path.generic_string();

            if (!payloadPath.empty()
                && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                const char* payloadType = isAudio ? "ASSET_BROWSER_AUDIO_PATH" : "ASSET_BROWSER_PATH";
                ImGui::SetDragDropPayload(payloadType,
                    payloadPath.c_str(), payloadPath.size() + 1);
                ImGui::TextUnformatted(payloadPath.c_str());
                ImGui::EndDragDropSource();
            }
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + tileSize.x);
        ImGui::TextWrapped("%s", label.c_str());
        const bool textHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const bool textDoubleClick = textHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const bool textClicked = ImGui::IsItemClicked();
        if (textClicked)
            m_selectedEntry = entry.path;
        ImGui::PopTextWrapPos();

        if (pressed)
            m_selectedEntry = entry.path;

        // Navigate into directory
        if (isDirectory && (pressed || textClicked || textDoubleClick))
        {
            needsRefresh = true;
            newDir = entry.path;
            ImGui::PopID();
            return true;  // Signal that we're navigating
        }

        // Clicking on an audio tile opens the Audio Files modal (listing all audio in the folder)
        if (isAudio && (pressed || textClicked || textDoubleClick))
        {
            OpenAudioPopupFrom(m_currentDir, m_entries);
        }

        // Per-item popup ID to avoid collisions between different tiles.
        const std::string popupId = std::string("AssetContextMenu##") + idStr;
        if (!isDirectory && ImGui::BeginPopupContextItem(popupId.c_str()))
        {
            if (IsTextureFile(entry.path))
            {
                if (ImGui::MenuItem("Replace Texture..."))
                {
                    m_pendingReplaceTarget = entry.path;
                    m_pendingReplaceSource.clear();
                    m_replaceBuffer.fill('\0');
                    m_replaceError.clear();
                    m_openReplacePopup = true;
                }
            }
            else
            {
                ImGui::TextDisabled("No actions available for this asset.");
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
        return false;
    }

    /*************************************************************************************
      \brief  Decide where an imported OS file should be placed (current dir by default).
      \param  file Absolute source path from the OS.
      \return Destination path within the assets root (or empty if invalid).
    *************************************************************************************/
    std::filesystem::path AssetBrowserPanel::ResolveImportTarget(const std::filesystem::path& file) const
    {
        if (file.empty())
            return {};

        std::filesystem::path base = m_assetsRoot;
        if (!m_currentDir.empty() && IsPathInside(m_assetsRoot, m_currentDir))
        {
            std::error_code ec;
            if (std::filesystem::is_directory(m_currentDir, ec) && !ec)
                base = m_currentDir;
        }

        if (base.empty())
            return {};

        std::filesystem::path preferred = base / file.filename();
        if (preferred.empty())
            return {};

        return preferred;
    }

    /*************************************************************************************
      \brief  Heuristic: file extension check for supported previewable textures.
      \param  path File path to test.
      \return true if the path ends with .png/.jpg/.jpeg (case-insensitive).
    *************************************************************************************/
    bool AssetBrowserPanel::IsTextureFile(const std::filesystem::path& path)
    {
        const auto lower = ToLower(path.extension().string());
        return lower == ".png" || lower == ".jpg" || lower == ".jpeg";
    }

    /*************************************************************************************
      \brief  Heuristic: file extension check for supported audio files (.wav/.mp3).
    *************************************************************************************/
    bool AssetBrowserPanel::IsAudioFile(const std::filesystem::path& path)
    {
        const auto lower = ToLower(path.extension().string());
        return lower == ".wav" || lower == ".mp3";
    }

    /*************************************************************************************
      \brief  Trim whitespace from both ends and return a copy.
    *************************************************************************************/
    std::string AssetBrowserPanel::TrimCopy(std::string value)
    {
        auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(),
            [&](unsigned char c) { return !isSpace(c); }));
        value.erase(std::find_if(value.rbegin(), value.rend(),
            [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
        return value;
    }

    /*************************************************************************************
      \brief  Parse newline/semicolons-separated input into absolute paths.
      \param  buffer Null-terminated C-string buffer from ImGui input.
      \return Vector of candidate paths (quotes around tokens are tolerated).
    *************************************************************************************/
    std::vector<std::filesystem::path> AssetBrowserPanel::ParseInputPaths(const char* buffer)
    {
        std::vector<std::filesystem::path> paths;
        if (!buffer)
            return paths;

        std::string input(buffer);
        std::size_t start = 0;
        while (start < input.size())
        {
            std::size_t end = input.find_first_of("\n;", start);
            std::string token = input.substr(start, end - start);
            token = TrimCopy(token);
            if (!token.empty() && token.front() == '"' && token.back() == '"' && token.size() >= 2)
                token = token.substr(1, token.size() - 2);
            if (!token.empty())
                paths.emplace_back(token);
            if (end == std::string::npos)
                break;
            start = end + 1;
        }

        return paths;
    }

    /*************************************************************************************
      \brief  Check if a candidate path resides inside a given base directory.
    *************************************************************************************/
    bool AssetBrowserPanel::IsPathInside(const std::filesystem::path& base, const std::filesystem::path& candidate)
    {
        if (base.empty() || candidate.empty())
            return false;

        std::error_code ec;
        auto canonicalBase = std::filesystem::weakly_canonical(base, ec);
        if (ec)
            canonicalBase = base;

        auto canonicalCandidate = std::filesystem::weakly_canonical(candidate, ec);
        if (ec)
            canonicalCandidate = candidate;

        auto relative = canonicalCandidate.lexically_relative(canonicalBase);
        if (relative.empty())
            return false;

        const std::string rel = relative.generic_string();
        return !rel.empty() && rel.rfind("..", 0) != 0;
    }

    /*************************************************************************************
      \brief  Draw the "Import Assets" modal (multiline list of absolute file paths).
    *************************************************************************************/
    void AssetBrowserPanel::DrawImportPopup()
    {
        if (!ImGui::BeginPopupModal("Import Assets", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::TextWrapped("Enter absolute file paths (one per line) to import them into the project.");
        ImGui::InputTextMultiline("##ImportPaths", m_importBuffer.data(), m_importBuffer.size(), ImVec2(420.0f, 140.0f));

        if (ImGui::Button("Import"))
        {
            auto files = ParseInputPaths(m_importBuffer.data());
            if (files.empty())
                SetStatus("No files specified for import.", true);
            else
                QueueExternalFiles(files);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    /*************************************************************************************
      \brief  Draw the "Replace Texture Asset" modal + confirmation step.
      \details Only allows replacing .png with .png. Performs path validation and shows
               inline error messages. On success, updates previews and status line.
    *************************************************************************************/
    void AssetBrowserPanel::DrawReplacePopup()
    {
        if (m_openReplacePopup)
        {
            ImGui::OpenPopup("Replace Texture Asset");
            m_openReplacePopup = false;
        }

        if (!ImGui::BeginPopupModal("Replace Texture Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        std::string targetDisplay = m_pendingReplaceTarget.lexically_relative(m_assetsRoot).generic_string();
        if (targetDisplay.empty())
            targetDisplay = m_pendingReplaceTarget.filename().string();

        ImGui::TextWrapped("Replace the selected texture with another .png file.");
        ImGui::Text("Target: %s", targetDisplay.c_str());
        ImGui::InputText("New Texture (.png)", m_replaceBuffer.data(), m_replaceBuffer.size());

        if (!m_replaceError.empty())
        {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", m_replaceError.c_str());
        }

        bool closeModal = false;

        if (ImGui::Button("Replace"))
        {
            auto paths = ParseInputPaths(m_replaceBuffer.data());
            if (paths.empty())
            {
                m_replaceError = "Provide a valid .png file path.";
            }
            else
            {
                std::filesystem::path candidate = paths.front();
                std::error_code ec;
                if (!std::filesystem::exists(candidate, ec) || !std::filesystem::is_regular_file(candidate, ec))
                {
                    m_replaceError = "The selected file does not exist.";
                }
                else if (ToLower(candidate.extension().string()) != ".png")
                {
                    m_replaceError = "Only .png files can replace texture assets.";
                }
                else
                {
                    m_pendingReplaceSource = std::filesystem::weakly_canonical(candidate, ec);
                    if (ec)
                        m_pendingReplaceSource = candidate;
                    ImGui::OpenPopup("Confirm Texture Replace");
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_pendingReplaceTarget.clear();
            m_pendingReplaceSource.clear();
            m_replaceError.clear();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        if (ImGui::BeginPopupModal("Confirm Texture Replace", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            std::string sourceDisplay = m_pendingReplaceSource.string();
            ImGui::TextWrapped("Replace '%s' with '%s'?", targetDisplay.c_str(), sourceDisplay.c_str());
            ImGui::TextDisabled("This operation overwrites the existing file.");
            if (ImGui::Button("Yes, replace"))
            {
                if (ReplaceTextureAsset(m_pendingReplaceTarget, m_pendingReplaceSource))
                {
                    closeModal = true;
                }
                else
                {
                    m_replaceError = "Failed to replace the texture. Ensure the file is accessible.";
                    SetStatus("Failed to replace texture asset.", true);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (closeModal)
        {
            m_pendingReplaceTarget.clear();
            m_pendingReplaceSource.clear();
            m_replaceError.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    /*************************************************************************************
      \brief  Draw the transient status line (success=green, error=red).
    *************************************************************************************/
    void AssetBrowserPanel::DrawStatusLine()
    {
        if (m_statusMessage.empty())
            return;

        const ImVec4 color = m_statusIsError ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f) : ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
    }

    /*************************************************************************************
      \brief  If the current selection no longer exists or moved outside assets root,
              clear it to avoid stale references.
    *************************************************************************************/
    void AssetBrowserPanel::ClearSelectionIfInvalid()
    {
        if (m_selectedEntry.empty())
            return;

        std::error_code ec;
        if (!std::filesystem::exists(m_selectedEntry, ec))
        {
            m_selectedEntry.clear();
            return;
        }

        if (!IsPathInside(m_assetsRoot, m_selectedEntry))
            m_selectedEntry.clear();
    }

    /*************************************************************************************
      \brief  Add a path (relative to assets root) to the pending-import list (deduped).
    *************************************************************************************/
    void AssetBrowserPanel::AddPendingImport(const std::filesystem::path& relativePath)
    {
        if (relativePath.empty())
            return;

        const std::string key = relativePath.generic_string();
        auto it = std::find_if(m_pendingImports.begin(), m_pendingImports.end(),
            [&](const std::filesystem::path& existing) { return existing.generic_string() == key; });
        if (it == m_pendingImports.end())
            m_pendingImports.push_back(relativePath);
    }

    /*************************************************************************************
      \brief  Replace an existing texture asset with a new .png; updates preview + status.
      \param  target Existing project path (absolute).
      \param  newFile New absolute source .png path.
      \return true on success.
    *************************************************************************************/
    bool AssetBrowserPanel::ReplaceTextureAsset(const std::filesystem::path& target, const std::filesystem::path& newFile)
    {
        if (m_assetsRoot.empty() || target.empty() || newFile.empty())
            return false;

        std::error_code ec;
        auto canonicalTarget = std::filesystem::weakly_canonical(target, ec);
        if (ec)
            canonicalTarget = target;

        if (!std::filesystem::exists(canonicalTarget, ec) || !std::filesystem::is_regular_file(canonicalTarget, ec))
            return false;

        if (!IsPathInside(m_assetsRoot, canonicalTarget))
            return false;

        auto targetExt = ToLower(canonicalTarget.extension().string());
        if (targetExt != ".png")
            return false;

        auto canonicalSource = std::filesystem::weakly_canonical(newFile, ec);
        if (ec)
            canonicalSource = newFile;

        if (!std::filesystem::exists(canonicalSource, ec) || !std::filesystem::is_regular_file(canonicalSource, ec))
            return false;

        auto sourceExt = ToLower(canonicalSource.extension().string());
        if (sourceExt != ".png")
            return false;

        ec.clear();
        std::filesystem::create_directories(canonicalTarget.parent_path(), ec);

        ec.clear();
        std::filesystem::copy_file(canonicalSource, canonicalTarget, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
            return false;

        auto relative = canonicalTarget.lexically_relative(m_assetsRoot);
        if (relative.empty() || relative.generic_string().rfind("..", 0) == 0)
            return false;

        RemovePreviewForPath(canonicalTarget);
        AddPendingImport(relative);
        RefreshEntries();
        m_selectedEntry = canonicalTarget;

        SetStatus("Replaced texture '" + relative.generic_string() + "'.", false);
        return true;
    }

    /*************************************************************************************
      \brief  Set the status line message and style.
      \param  message Text to display.
      \param  isError If true, render in error color; otherwise success color.
    *************************************************************************************/
    void AssetBrowserPanel::SetStatus(const std::string& message, bool isError)
    {
        m_statusMessage = message;
        m_statusIsError = isError;
    }

    /*************************************************************************************
      \brief  Check if a given path is the current selection (filesystem-equivalent).
    *************************************************************************************/
    bool AssetBrowserPanel::IsSelected(const std::filesystem::path& path) const
    {
        if (m_selectedEntry.empty())
            return false;

        std::error_code ec;
        bool equivalent = std::filesystem::equivalent(m_selectedEntry, path, ec);
        return !ec && equivalent;
    }

    /*************************************************************************************
      \brief  Fetch or lazily create a preview texture for an image file.
      \param  path Absolute texture file path.
      \return Pointer to cached preview (or nullptr if not previewable/failed).
    *************************************************************************************/
    const AssetBrowserPanel::PreviewTexture* AssetBrowserPanel::GetTexturePreview(const std::filesystem::path& path)
    {
        if (path.empty())
            return nullptr;

        const std::string key = PathKey(path);
        if (key.empty())
            return nullptr;

        auto it = m_previewCache.find(key);
        if (it != m_previewCache.end())
            return &it->second;

        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);
        if (ec)
            canonical = path;

        if (!std::filesystem::exists(canonical, ec) || !std::filesystem::is_regular_file(canonical, ec))
            return nullptr;

        PreviewTexture preview;
        preview.textureId = gfx::Graphics::loadTexture(canonical.string().c_str());
        if (preview.textureId == 0)
            return nullptr;

        if (!gfx::Graphics::getTextureSize(preview.textureId, preview.width, preview.height))
        {
            gfx::Graphics::destroyTexture(preview.textureId);
            return nullptr;
        }

        auto [insertedIt, inserted] = m_previewCache.emplace(key, preview);
        return inserted ? &insertedIt->second : &m_previewCache[key];
    }

    /*************************************************************************************
      \brief  Remove preview textures for files that are no longer visible in m_entries.
    *************************************************************************************/
    void AssetBrowserPanel::PrunePreviewCache()
    {
        std::unordered_set<std::string> active;
        active.reserve(m_entries.size());
        for (const auto& entry : m_entries)
        {
            if (!entry.isDirectory && IsTextureFile(entry.path))
            {
                const std::string key = PathKey(entry.path);
                if (!key.empty())
                    active.insert(key);
            }
        }

        for (auto it = m_previewCache.begin(); it != m_previewCache.end(); )
        {
            if (active.find(it->first) == active.end())
            {
                if (it->second.textureId != 0)
                    gfx::Graphics::destroyTexture(it->second.textureId);
                it = m_previewCache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    /*************************************************************************************
      \brief  Destroy all preview textures and clear the cache.
    *************************************************************************************/
    void AssetBrowserPanel::ClearPreviewCache()
    {
        for (auto& entry : m_previewCache)
        {
            if (entry.second.textureId != 0)
                gfx::Graphics::destroyTexture(entry.second.textureId);
        }
        m_previewCache.clear();
    }

    /*************************************************************************************
      \brief  Remove (and destroy) any preview associated with a specific path.
    *************************************************************************************/
    void AssetBrowserPanel::RemovePreviewForPath(const std::filesystem::path& path)
    {
        const std::string key = PathKey(path);
        if (key.empty())
            return;

        auto it = m_previewCache.find(key);
        if (it != m_previewCache.end())
        {
            if (it->second.textureId != 0)
                gfx::Graphics::destroyTexture(it->second.textureId);
            m_previewCache.erase(it);
        }
    }

    /*************************************************************************************
      \brief  Stable key for preview cache: canonicalized generic string.
      \param  path Any path to normalize.
      \return Canonical generic string (or original generic string if canonicalization fails).
    *************************************************************************************/
    std::string AssetBrowserPanel::PathKey(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(path, ec);
        if (ec)
            canonical = path;

        return canonical.generic_string();
    }

} // namespace mygame
