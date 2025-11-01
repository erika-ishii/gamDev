#include "Debug/AssetBrowserPanel.h"

#include <algorithm>
#include <cctype>
#include <imgui.h>
#include <system_error>

namespace mygame {

    namespace {
        constexpr float kThumbnailSize = 96.0f;
        constexpr float kPadding = 16.0f;

        std::string ToLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
                });
            return value;
        }
    }

    void AssetBrowserPanel::Initialize(const std::filesystem::path& assetsRoot)
    {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(assetsRoot, ec);
        m_assetsRoot = ec ? std::filesystem::absolute(assetsRoot) : canonical;
        m_currentDir = m_assetsRoot;
        RefreshEntries();
    }

    void AssetBrowserPanel::Draw()
    {
        if (m_assetsRoot.empty())
            return;

        if (!ImGui::Begin("Content Browser"))
        {
            ImGui::End();
            return;
        }

        if (m_currentDir != m_assetsRoot)
        {
            if (ImGui::Button("<--"))
            {
                m_currentDir = m_currentDir.parent_path();
                RefreshEntries();
            }
            ImGui::SameLine();
        }

        const std::string header = m_currentDir == m_assetsRoot
            ? std::string("assets")
            : m_currentDir.lexically_relative(m_assetsRoot).generic_string();
        ImGui::TextUnformatted(header.c_str());

        float panelWidth = ImGui::GetContentRegionAvail().x;
        float cellSize = kThumbnailSize + kPadding;
        int columnCount = static_cast<int>(panelWidth / cellSize);
        if (columnCount < 1)
            columnCount = 1;

        ImGui::Columns(columnCount, nullptr, false);

        for (const auto& entry : m_entries)
        {
            DrawEntry(entry, kThumbnailSize);
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::End();
    }

    void AssetBrowserPanel::QueueExternalFiles(const std::vector<std::filesystem::path>& files)
    {
        if (m_assetsRoot.empty())
            return;

        for (const auto& file : files)
        {
            if (!std::filesystem::exists(file) || !std::filesystem::is_regular_file(file))
                continue;

            std::filesystem::path destination = ResolveImportTarget(file);
            if (destination.empty())
                continue;

            std::error_code ec;
            std::filesystem::create_directories(destination.parent_path(), ec);

            std::filesystem::copy_file(file, destination, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
                continue;

            std::error_code canonicalEc;
            auto canonical = std::filesystem::weakly_canonical(destination, canonicalEc);
            if (!canonicalEc) {
                auto relative = canonical.lexically_relative(m_assetsRoot);
                if (!relative.empty())
                    m_pendingImports.push_back(relative);
            }
        }

        if (!files.empty())
            RefreshEntries();
    }

    std::vector<std::filesystem::path> AssetBrowserPanel::ConsumePendingImports()
    {
        auto pending = m_pendingImports;
        m_pendingImports.clear();
        return pending;
    }

    void AssetBrowserPanel::RefreshEntries()
    {
        m_entries.clear();
        std::error_code ec;
        if (m_currentDir.empty() || !std::filesystem::exists(m_currentDir, ec))
            return;

        std::vector<Entry> directories;
        std::vector<Entry> files;

        for (const auto& dirEntry : std::filesystem::directory_iterator(m_currentDir, ec))
        {
            if (ec)
                break;

            Entry entry;
            entry.path = dirEntry.path();
            entry.isDirectory = dirEntry.is_directory();
            if (entry.isDirectory)
                directories.push_back(entry);
            else
                files.push_back(entry);
        }

        auto sortByName = [](const Entry& a, const Entry& b) {
            return ToLower(a.path.filename().string()) < ToLower(b.path.filename().string());
            };

        std::sort(directories.begin(), directories.end(), sortByName);
        std::sort(files.begin(), files.end(), sortByName);

        m_entries.reserve(directories.size() + files.size());
        m_entries.insert(m_entries.end(), directories.begin(), directories.end());
        m_entries.insert(m_entries.end(), files.begin(), files.end());
    }

    void AssetBrowserPanel::DrawEntry(const Entry& entry, float cellSize)
    {
        ImGui::PushID(entry.path.filename().c_str());

        const bool isDirectory = entry.isDirectory;
        const std::string label = entry.path.filename().string();

        ImVec2 size(cellSize, cellSize);
        if (ImGui::Button(label.c_str(), size) && isDirectory)
        {
            m_currentDir = entry.path;
            RefreshEntries();
        }

        if (!isDirectory && ImGui::BeginDragDropSource())
        {
            auto relative = entry.path.lexically_relative(m_assetsRoot).generic_string();
            ImGui::SetDragDropPayload("ASSET_BROWSER_PATH", relative.c_str(), relative.size() + 1);
            ImGui::TextUnformatted(relative.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && isDirectory)
        {
            m_currentDir = entry.path;
            RefreshEntries();
        }

        ImGui::PopID();
    }

    std::filesystem::path AssetBrowserPanel::ResolveImportTarget(const std::filesystem::path& file) const
    {
        const auto ext = ToLower(file.extension().string());
        std::filesystem::path base = m_assetsRoot;

        if (IsTextureFile(file))
            base /= "Textures";
        else if (IsAudioFile(file))
            base /= "Audio";

        std::filesystem::path candidate = base / file.filename();
        if (!std::filesystem::exists(candidate))
            return candidate;

        std::string stem = file.stem().string();
        std::string extension = file.extension().string();
        for (int i = 1; i < 1000; ++i)
        {
            std::filesystem::path numbered = base / (stem + "_" + std::to_string(i) + extension);
            if (!std::filesystem::exists(numbered))
                return numbered;
        }

        return {};
    }

    bool AssetBrowserPanel::IsTextureFile(const std::filesystem::path& path)
    {
        const auto lower = ToLower(path.extension().string());
        return lower == ".png" || lower == ".jpg" || lower == ".jpeg";
    }

    bool AssetBrowserPanel::IsAudioFile(const std::filesystem::path& path)
    {
        const auto lower = ToLower(path.extension().string());
        return lower == ".wav" || lower == ".mp3";
    }

} // namespace mygame