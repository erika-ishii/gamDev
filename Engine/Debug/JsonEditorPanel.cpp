/*********************************************************************************************
 \file      JsonEditorPanel.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements an ImGui-based JSON editor panel for viewing and editing data files.

 \details
			The panel scans a configured data directory for *.json files, presents them
			in a popup list, and loads the selected file into a multiline text editor.
			It tracks a dirty state, supports Save/Revert, and shows transient status
			messages (e.g., success or error) with color coding.

			Responsibilities:
			- Discover JSON files under a root directory (non-recursive).
			- Load the selected file into an editable buffer (null-terminated for ImGui).
			- Save the current buffer back to disk with overwrite semantics.
			- Maintain a status line with auto-expiring notifications.
			- Provide a small toolbar: Refresh list, Select file, Save, Revert.

 \copyright
			All content © 2025 DigiPen Institute of Technology Singapore.
			All rights reserved.
*********************************************************************************************/

#include "Debug/JsonEditorPanel.h"

#if SOFASPUDS_ENABLE_EDITOR

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <system_error>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif

namespace mygame {

	namespace {
		// How long (in seconds) a status message should remain visible.
		constexpr float kStatusDurationSeconds = 5.0f;

		// Helper: detect ".json" extension (case-insensitive).
		bool IsJsonFile(const std::filesystem::path& path)
		{
			auto ext = path.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
				});
			return ext == ".json";
		}
	} // namespace

	// Set up panel: remember where JSON files live, build the initial file list,
	// clear selection and editor state.
	void JsonEditorPanel::Initialize(const std::filesystem::path& dataRoot)
	{
		// Base directory for JSON files.
		m_dataRoot = std::filesystem::absolute(dataRoot);
		// Scan directory and build list.
		RefreshFiles();
		// No file selected yet.
		m_selectedIndex = static_cast<std::size_t>(-1);
		// One null char so InputText has a buffer.
		m_textBuffer.assign(1, '\0');
		// No edit pending.
		m_dirty = false;
		// Clear any previous status text.
		m_statusMessage.clear();
	}

	// Draw the entire panel UI every frame.
	void JsonEditorPanel::Draw()
	{
		// Auto-clear old status messages after a timeout.
		UpdateStatusTimer();

		// The panel window.
		if (!ImGui::Begin("Json Editor"))
		{
			// If window collapsed, still call End() to match ImGui call.
			ImGui::End();
			return;
		}

		// No directory == no editor.
		if (m_dataRoot.empty())
		{
			ImGui::TextDisabled("No data directory detected");
			ImGui::End();
			return;
		}

		// Toolbar: refresh list and open file list popup.
		if (ImGui::Button("Refresh JSON Files")) {
			RefreshFiles();
		}
		ImGui::SameLine();

		if (ImGui::Button("Select JSON File"))
		{
			ImGui::OpenPopup("JsonEditor.FileList");
		}

		// Popup containing the list of JSON files.
		if (ImGui::BeginPopup("JsonEditor.FileList"))
		{
			if (m_jsonFiles.empty())
			{
				ImGui::TextDisabled("No JSON files found under %s", m_dataRoot.generic_string().c_str());
			}
			else
			{
				// Show each file as a selectable item.
				for (std::size_t i = 0; i < m_jsonFiles.size(); ++i)
				{
					const bool selected = (i == m_selectedIndex);
					const auto label = m_jsonFiles[i].generic_string();
					if (ImGui::Selectable(label.c_str(), selected))
					{
						LoadFile(i);
						ImGui::CloseCurrentPopup();
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndPopup();
		}
		ImGui::Separator();

		// If no file selected, prompt user to select one.
		if (m_selectedIndex == static_cast<std::size_t>(-1) || m_selectedIndex >= m_jsonFiles.size())
		{
			ImGui::TextDisabled("Select a JSON file to begin editing.");
		}
		else
		{
			// Header: show relative path and a "modified" marker if dirty.
			const auto relative = m_jsonFiles[m_selectedIndex];
			ImGui::TextUnformatted(relative.generic_string().c_str());
			if (m_dirty)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "*modified");
			}

			// Save / Revert buttons.
			if (ImGui::Button("Save"))
			{
				SaveFile();  // Write buffer to disk.
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!m_dirty); // Prevent revert when no changes.
			if (ImGui::Button("Revert"))
			{
				LoadFile(m_selectedIndex);
			}
			ImGui::EndDisabled();

			ImGui::Spacing();

			// Text editor area for raw JSON content.
			ImVec2 size = ImGui::GetContentRegionAvail();
			if (size.y < 200.0f)
				size.y = 200.0f;

			// Flags:
			// - CallbackResize: we manage dynamic buffer resizing
			// - AllowTabInput: allow inserting tab characters
			// - NoUndoRedo: keep memory simple; OS clipboard still works
			ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize
				| ImGuiInputTextFlags_AllowTabInput
				| ImGuiInputTextFlags_NoUndoRedo;

			// If any change happens in the editor, mark as dirty.
			if (ImGui::InputTextMultiline("##JsonEditorContent", m_textBuffer.data(),
				m_textBuffer.size(), size, flags, &JsonEditorPanel::TextEditCallback, this))
			{
				m_dirty = true;
			}
		}

		// Status line (e.g., "Saved file", "Error reading file", etc.).
		if (!m_statusMessage.empty())
		{
			ImGui::Spacing();
			ImGui::TextColored(m_statusColor, "%s", m_statusMessage.c_str());
		}
		ImGui::End(); // end Json editor
	}

	// Rescan m_dataRoot for *.json and rebuild m_jsonFiles.
	void JsonEditorPanel::RefreshFiles()
	{
		m_jsonFiles.clear();
		if (m_dataRoot.empty())
			return;

		namespace fs = std::filesystem;
		std::error_code ec;

		// Validate the root directory.
		if (!fs::exists(m_dataRoot, ec) || !fs::is_directory(m_dataRoot, ec))
		{
			ShowStatus("Data directory not found.", ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			return;
		}

		// Iterate only the top level of the directory (not recursive).
		for (fs::directory_iterator it(m_dataRoot, ec), end; it != end; it.increment(ec))
		{
			if (ec)
				break;

			const fs::directory_entry& entry = *it;
			if (!entry.is_regular_file(ec))
				continue;

			if (!IsJsonFile(entry.path()))
				continue;

			// Canonicalize path if possible (not fatal if it fails).
			std::error_code canonicalEc;
			auto canonical = fs::weakly_canonical(entry.path(), canonicalEc);
			if (canonicalEc)
				continue;

			// Store as a path relative to m_dataRoot for neat display.
			std::error_code relEc;
			auto relative = fs::relative(canonical, m_dataRoot, relEc);
			if (relEc)
				relative = entry.path().lexically_relative(m_dataRoot);

			m_jsonFiles.emplace_back(relative);
		}

		// If iteration itself failed, report the error.
		if (ec)
		{
			ShowStatus(std::string("Error scanning JSON files: ") + ec.message(),
				ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			return;
		}

		// Sort alphabetically for predictable UX.
		std::sort(m_jsonFiles.begin(), m_jsonFiles.end(), [](const fs::path& a, const fs::path& b) {
			return a.generic_string() < b.generic_string();
			});

		// Status message depending on results.
		if (m_jsonFiles.empty())
		{
			ShowStatus("No JSON files found in data directory.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
		}
		else
		{
			ShowStatus("JSON file list refreshed.", ImVec4(0.6f, 0.85f, 0.6f, 1.0f));
		}
		
		// If the currently selected file no longer exists, close it.
		if (m_selectedIndex != static_cast<std::size_t>(-1))
		{
			if (m_selectedIndex >= m_jsonFiles.size())
			{
				m_selectedIndex = static_cast<std::size_t>(-1);
				m_textBuffer.assign(1, '\0');
				m_dirty = false;
				ShowStatus("Previously open file was deleted.", ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
			}
		}
	}

	// Load file content at index into the text buffer (null-terminated).
	void JsonEditorPanel::LoadFile(std::size_t index)
	{
		if (index >= m_jsonFiles.size())
			return;

		const auto absolute = m_dataRoot / m_jsonFiles[index];

		// Open file in binary to read raw bytes (no newline conversions).
		std::ifstream file(absolute, std::ios::binary);
		if (!file)
		{
			ShowStatus("Failed to open file: " + absolute.generic_string(),
				ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			return;
		}

		// Read whole file into a std::string.
		std::string content;
		file.seekg(0, std::ios::end);
		const std::streamsize length = file.tellg();
		file.seekg(0, std::ios::beg);
		if (length > 0)
		{
			content.resize(static_cast<std::size_t>(length));
			file.read(content.data(), length);
		}

		// If read failed and it wasn't just EOF at the end, report error.
		if (!file && !file.eof())
		{
			ShowStatus("Failed to read file: " + absolute.generic_string(),
				ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			return;
		}

		// Copy content into ImGui text buffer plus terminating '\0'.
		m_textBuffer.assign(content.begin(), content.end());
		m_textBuffer.push_back('\0');
		m_selectedIndex = index;
		m_dirty = false;
		ShowStatus("Loaded " + m_jsonFiles[index].generic_string(),
			ImVec4(0.6f, 0.85f, 0.6f, 1.0f));
	}

	// Save current text buffer to disk (overwrites the selected file).
	void JsonEditorPanel::SaveFile()
	{
		if (m_selectedIndex == static_cast<std::size_t>(-1) || m_selectedIndex >= m_jsonFiles.size())
			return;

		const auto absolute = m_dataRoot / m_jsonFiles[m_selectedIndex];

		// Truncate existing file; write raw bytes.
		std::ofstream file(absolute, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			ShowStatus("Failed to save file: " + absolute.generic_string(),
				ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			return;
		}

		// Only write the meaningful chars (up to the first '\0').
		const std::size_t length = m_textBuffer.empty() ? 0 : std::strlen(m_textBuffer.data());
		if (length > 0)
		{
			file.write(m_textBuffer.data(), static_cast<std::streamsize>(length));
		}

		if (!file)
		{
			ShowStatus("Error writing file: " + absolute.generic_string(),
				ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			return;
		}

		m_dirty = false;
		ShowStatus("Saved " + m_jsonFiles[m_selectedIndex].generic_string(),
			ImVec4(0.6f, 0.85f, 0.6f, 1.0f));
	}

	// Helper: set a status message (with color) and start its timer.
	void JsonEditorPanel::ShowStatus(const std::string& message, const ImVec4& color)
	{
		m_statusMessage = message;
		m_statusColor = color;
		m_statusTimestamp = Clock::now();
	}

	// Clears status message once its time is up.
	void JsonEditorPanel::UpdateStatusTimer()
	{
		if (m_statusMessage.empty())
			return;

		const auto elapsed = std::chrono::duration<float>(Clock::now() - m_statusTimestamp).count();
		if (elapsed > kStatusDurationSeconds)
		{
			m_statusMessage.clear();
		}
	}

	// ImGui callback for dynamic buffer resizing:
	// when InputText needs a bigger/smaller buffer, this lets us resize m_textBuffer.
	int JsonEditorPanel::TextEditCallback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			auto* panel = static_cast<JsonEditorPanel*>(data->UserData);
			panel->m_textBuffer.resize(static_cast<std::size_t>(data->BufSize));
			data->Buf = panel->m_textBuffer.data(); // update ImGui to new buffer address
		}
		return 0;
	}
}

#endif // SOFASPUDS_ENABLE_EDITOR
