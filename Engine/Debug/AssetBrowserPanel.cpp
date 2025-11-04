#include "Debug/AssetBrowserPanel.h"

#include <algorithm>
#include <cctype>
#include <imgui.h>
#include <system_error>
#include <unordered_set>

namespace mygame {

    namespace {
        constexpr float kThumbnailSize = 96.0f;
        constexpr float kPadding = 16.0f;
        std::string ToLower(std::string value)
        {
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
        m_selectedEntry.clear();
        m_importBuffer.fill('\0');
        m_replaceBuffer.fill('\0');
        m_statusMessage.clear();
        m_statusIsError = false;
        RefreshEntries();
    }

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

        if (ImGui::Button("Import Assets..."))
        {
            m_importBuffer.fill('\0');
            m_openImportPopup = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Drag and drop files from your OS to import them.");

        if (m_openImportPopup)
        {
            ImGui::OpenPopup("Import Assets");
            m_openImportPopup = false;
        }
        DrawImportPopup();


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

        const std::string header = m_currentDir == m_assetsRoot
            ? std::string("assets")
            : m_currentDir.lexically_relative(m_assetsRoot).generic_string();
        ImGui::TextUnformatted(header.c_str());
        if (!m_selectedEntry.empty())
        {
            std::string relative = m_selectedEntry.lexically_relative(m_assetsRoot).generic_string();
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

        for (const auto& entry : m_entries)
        {
            DrawEntry(entry, kThumbnailSize);
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
        DrawReplacePopup();

        ImGui::End();
    }
    std::size_t AssetBrowserPanel::QueueExternalFiles(const std::vector<std::filesystem::path>& files)
    {
        if (m_assetsRoot.empty())
            return 0u;

        std::size_t imported = 0u;

        for (const auto& file : files)
        {
            std::error_code ec;
            if (!std::filesystem::exists(file, ec) || !std::filesystem::is_regular_file(file, ec))
                continue;

            std::filesystem::path destination = ResolveImportTarget(file);
            if (destination.empty())
                continue;

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

            AddPendingImport(relative);
            ++imported;
        }

        if (!files.empty())
            RefreshEntries();

        if (!files.empty())
        {
            if (imported > 0)
            {
                SetStatus("Imported " + std::to_string(imported) + (imported == 1 ? " asset." : " assets."), false);
            }
            else
            {
                SetStatus("No supported assets were imported.", true);
            }
        }

        return imported;
    }

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

        ClearSelectionIfInvalid();
    }

    void AssetBrowserPanel::DrawEntry(const Entry& entry, float cellSize)
    {
        
        const std::string label = entry.path.filename().string();
        ImGui::PushID(label.c_str());

      

     
        const bool isDirectory = entry.isDirectory;
        const bool isSelected = IsSelected(entry.path);
        if (isSelected)
        {
            const ImVec4 header = ImGui::GetStyleColorVec4(ImGuiCol_Header);
            const ImVec4 headerHovered = ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered);
            const ImVec4 headerActive = ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive);
            ImGui::PushStyleColor(ImGuiCol_Button, header);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, headerHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, headerActive);
        }

        const std::string buttonLabel = "##" + label;
        const ImVec2 tileSize(cellSize, cellSize);
        const bool pressed = ImGui::Button(buttonLabel.c_str(), tileSize);

        if (isSelected)
            ImGui::PopStyleColor(3);

        const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const bool buttonDoubleClick = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        const char* overlay = isDirectory ? "DIR" : "FILE";
        const ImVec2 rectMin = ImGui::GetItemRectMin();
        const ImVec2 rectMax = ImGui::GetItemRectMax();
        const ImVec2 textSize = ImGui::CalcTextSize(overlay);
        const ImVec2 textPos(rectMin.x + (rectMax.x - rectMin.x - textSize.x) * 0.5f,
            rectMin.y + (rectMax.y - rectMin.y - textSize.y) * 0.5f);
        ImGui::GetWindowDrawList()->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), overlay);

        std::string payloadPath = entry.path.lexically_relative(m_assetsRoot).generic_string();
        if (payloadPath.empty() || payloadPath == "." || payloadPath.rfind("..", 0) == 0)
            payloadPath = entry.path.generic_string();

        bool dragging = false;
        if (!isDirectory && !payloadPath.empty()
            && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            dragging = true;
            ImGui::SetDragDropPayload("ASSET_BROWSER_PATH", payloadPath.c_str(), payloadPath.size() + 1);
            ImGui::TextUnformatted(payloadPath.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + tileSize.x);
        ImGui::TextWrapped(label.c_str());
        const bool textHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const bool textDoubleClick = textHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        if (ImGui::IsItemClicked())
            m_selectedEntry = entry.path;
        ImGui::PopTextWrapPos();

        if (pressed)
            m_selectedEntry = entry.path;

        if (isDirectory && !dragging && (buttonDoubleClick || textDoubleClick))
        {
            m_currentDir = entry.path;
            m_selectedEntry.clear();
            RefreshEntries();
        }
        if (!isDirectory && ImGui::BeginPopupContextItem("AssetContextMenu"))
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

    std::string AssetBrowserPanel::TrimCopy(std::string value)
    {
        auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(),
            [&](unsigned char c) { return !isSpace(c); }));
        value.erase(std::find_if(value.rbegin(), value.rend(),
            [&](unsigned char c) { return !isSpace(c); }).base(), value.end());
        return value;
    }

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

    void AssetBrowserPanel::DrawStatusLine()
    {
        if (m_statusMessage.empty())
            return;

        const ImVec4 color = m_statusIsError ? ImVec4(0.9f, 0.3f, 0.3f, 1.0f) : ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
    }

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

        AddPendingImport(relative);
        RefreshEntries();
        m_selectedEntry = canonicalTarget;

        SetStatus("Replaced texture '" + relative.generic_string() + "'.", false);
        return true;
    }

    void AssetBrowserPanel::SetStatus(const std::string& message, bool isError)
    {
        m_statusMessage = message;
        m_statusIsError = isError;
    }

    bool AssetBrowserPanel::IsSelected(const std::filesystem::path& path) const
    {
        if (m_selectedEntry.empty())
            return false;

        std::error_code ec;
        bool equivalent = std::filesystem::equivalent(m_selectedEntry, path, ec);
        return !ec && equivalent;
    }

} // namespace mygame