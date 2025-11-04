#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

namespace mygame {
    class JsonEditorPanel {
    public:
        void Initialize(const std::filesystem::path& dataRoot);
        void Draw();

    private:
        using Clock = std::chrono::steady_clock;

        void RefreshFiles();
        void LoadFile(std::size_t index);
        void SaveFile();
        void ShowStatus(const std::string& message, const ImVec4& color);
        void UpdateStatusTimer();
        static int TextEditCallback(ImGuiInputTextCallbackData* data);

        std::filesystem::path m_dataRoot;
        std::vector<std::filesystem::path> m_jsonFiles;
        std::size_t m_selectedIndex = static_cast<std::size_t>(-1);
        std::vector<char> m_textBuffer{ '\0' };
        bool m_dirty = false;

        std::string m_statusMessage;
        ImVec4 m_statusColor{ 0.9f, 0.9f, 0.9f, 1.0f };
        Clock::time_point m_statusTimestamp{};
    };
}