/*********************************************************************************************
 \file      JsonEditorPanel.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares an ImGui-based JSON editor panel for listing, loading, editing,
            and saving JSON data files from a specified root directory.

 \details
            JsonEditorPanel scans a configured data directory (non-recursive) for files
            with the “.json” extension and presents them in a popup list. The selected
            file’s contents are loaded into a multiline text buffer for direct editing.
            The panel tracks a dirty state, supports Save/Revert, and renders transient
            status messages with color coding.

            Responsibilities:
            - Discover JSON files under a root directory.
            - Load the selected file into an editable (null-terminated) buffer.
            - Save the current buffer back to disk (overwrite).
            - Maintain a status line with auto-expiring notifications.
            - Provide an ImGui UI for refresh, selection, save, and revert actions.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#if SOFASPUDS_ENABLE_EDITOR

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

namespace mygame {

    /*****************************************************************************************
      \class JsonEditorPanel
      \brief ImGui panel for editing JSON files in-place.

      Typical usage:
      1) Call Initialize() with the root directory containing JSON files.
      2) Call Draw() every frame to render the UI and handle interactions.
    *****************************************************************************************/
    class JsonEditorPanel {
    public:
        /*************************************************************************************
          \brief Initialize the panel with the directory to scan for JSON files.
          \param dataRoot  Root directory containing JSON files (non-recursive listing).
        *************************************************************************************/
        void Initialize(const std::filesystem::path& dataRoot);

        /*************************************************************************************
          \brief Render the panel UI and handle user interaction each frame.
        *************************************************************************************/
        void Draw();

    private:
        using Clock = std::chrono::steady_clock;

        /*************************************************************************************
          \brief Rescan the data root and rebuild the internal list of JSON files.
        *************************************************************************************/
        void RefreshFiles();

        /*************************************************************************************
          \brief Load the file at \p index into the text buffer (null-terminated).
          \param index  Index into the discovered JSON file list.
        *************************************************************************************/
        void LoadFile(std::size_t index);

        /*************************************************************************************
          \brief Save the current text buffer back to the selected file on disk.
        *************************************************************************************/
        void SaveFile();

        /*************************************************************************************
          \brief Set a transient status message (with color) and start its expiration timer.
          \param message  Text to display in the status line.
          \param color    ImGui color used to render the status message.
        *************************************************************************************/
        void ShowStatus(const std::string& message, const ImVec4& color);

        /*************************************************************************************
          \brief Clear the status message once its display duration has elapsed.
        *************************************************************************************/
        void UpdateStatusTimer();

        /*************************************************************************************
          \brief ImGui callback used to resize the dynamic text buffer as the user edits.
          \param data  ImGui input text callback data.
          \return 0 on success.
        *************************************************************************************/
        static int TextEditCallback(ImGuiInputTextCallbackData* data);

        // --- State ---

        std::filesystem::path m_dataRoot;                ///< Root directory for JSON files.
        std::vector<std::filesystem::path> m_jsonFiles;  ///< Discovered JSON files (relative paths).
        std::size_t m_selectedIndex = static_cast<std::size_t>(-1); ///< Currently selected file index.
        std::vector<char> m_textBuffer{ '\0' };          ///< Editable null-terminated buffer for file contents.
        bool m_dirty = false;                            ///< True if buffer has unsaved changes.

        std::string m_statusMessage;                     ///< Transient status text (success/errors).
        ImVec4 m_statusColor{ 0.9f, 0.9f, 0.9f, 1.0f };  ///< Color for status text.
        Clock::time_point m_statusTimestamp{};           ///< When the current status message was set.
    };
}

#endif // SOFASPUDS_ENABLE_EDITOR
