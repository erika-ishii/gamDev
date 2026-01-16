/*********************************************************************************************
 \file      Perf.cpp
 \par       SofaSpuds
 \author    yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Implements a lightweight per-frame CPU profiler for Update / Render / ImGui.
            FPS is computed from the engine's Core dt (not ImGui).
*********************************************************************************************/

#include "Perf.h"
#if SOFASPUDS_ENABLE_EDITOR
#include "imgui.h"
#endif
#include <algorithm>   // std::max
#include <cstddef>     // size_t
#include <numeric>
#include <string>
#include <string_view>
#include <vector>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
/// \internal Anonymous namespace for private state
namespace {
    /// Aggregated timings for one frame (CPU-side, milliseconds).
    struct Values {
        double gUpdateMs = 0.0;   // CPU ms in Update
        double gRenderMs = 0.0;   // CPU ms in Render (aggregate)
        double gImGuIMs = 0.0;    // CPU ms in ImGui (build + draw)
        double TrackedTotal() const { return gUpdateMs + gRenderMs + gImGuIMs; }
    };

    // Double-buffer for last/current measurements (so UI shows a stable "last frame").
    static Values gCurr;   // being written this frame
    static Values gLast;   // shown by UI (previous frame)

    struct SystemTiming {
        std::string name;
        double milliseconds = 0.0;
    };
    static std::vector<SystemTiming> gCurrSystemTimings;
    static std::vector<SystemTiming> gLastSystemTimings;

    void accumulateSystemTiming(std::vector<SystemTiming>& container, std::string_view name, double ms) {

        if (name.empty()) return;
        auto it = std::find_if(container.begin(), container.end(), [&](SystemTiming const& entry) {return entry.name == name; });
        if (it != container.end()) {
            it->milliseconds += ms;
        }
        else {
            container.push_back(SystemTiming{ std::string(name), ms });
        }
    }
    // Overlay state (F1 edge-toggle). Hidden by default so players won't see it
        // until they explicitly toggle it via the hotkey (F1).
    static bool  sPerfVisible = false;
    static bool  sPrevToggleKey = false;

    // Our own engine timing (from Core), in seconds
    static float sLastDtSec = 0.0f;

    // FPS history ring buffer (computed from our dt)
    static float sFpsPlot[120] = { 0.f };
    static int   sFpsPlotIdx = 0;

    // Simple moving average of FPS for a steadier readout
    static float sAvgFps = 0.0f;
    static int   sSamplesForAvg = 60;     // average over ~60 most recent samples (clamped by buffer size)

    inline float pushFpsSampleAndReturn(float dtSec) {
        sLastDtSec = dtSec;
        const float fpsNow = (dtSec > 1e-6f) ? (1.0f / dtSec) : 0.f;

        sFpsPlot[sFpsPlotIdx] = fpsNow;
        sFpsPlotIdx = (sFpsPlotIdx + 1) % (int)(sizeof(sFpsPlot) / sizeof(sFpsPlot[0]));

        // recompute moving average over up to sSamplesForAvg samples actually filled
        const int bufSize = (int)(sizeof(sFpsPlot) / sizeof(sFpsPlot[0]));
        const int count = std::max(1, std::min(sSamplesForAvg, bufSize));
        float sum = 0.f;
        for (int i = 0; i < count; ++i) sum += sFpsPlot[i];
        sAvgFps = sum / count;

        return fpsNow;
    }
} // anonymous namespace

// ---------- public API ----------
void Framework::FlipFrame() {
    gLast = gCurr;     // promote current to last
    gCurr = Values{};  // clear current for fresh measurements
    gLastSystemTimings = gCurrSystemTimings;
    gCurrSystemTimings.clear();
}

void Framework::setUpdate(double ms) { gCurr.gUpdateMs = ms; }
void Framework::setRender(double ms) { gCurr.gRenderMs = ms; }
void Framework::setImGui(double ms) { gCurr.gImGuIMs = ms; }

void Framework::RecordSystemTiming(std::string_view systemName, double milliseconds) {
    if (milliseconds < 0.0) return;
    accumulateSystemTiming(gCurrSystemTimings, systemName, milliseconds);
}


// ---------- mini summary (embed-only, no Begin/End) ----------
#if SOFASPUDS_ENABLE_EDITOR
void Framework::DrawInCurrentWindow() {
    const double totalTracked = gLast.TrackedTotal();
    const double denom = (totalTracked > 0.0) ? totalTracked : 0.0001; // avoid divide-by-zero

    ImGui::SeparatorText("Performance (last frame)");
    ImGui::Text("Tracked CPU total: %.2f ms", totalTracked);
    ImGui::TextDisabled("(no Core/swap/vsync/driver included)");
    ImGui::Spacing();

    ImGui::Text("Update:   %.3f ms (%.1f%%)", gLast.gUpdateMs, (gLast.gUpdateMs / denom) * 100.0);
    ImGui::Text("Render:   %.3f ms (%.1f%%)", gLast.gRenderMs, (gLast.gRenderMs / denom) * 100.0);
    ImGui::Text("ImGui:    %.3f ms (%.1f%%)", gLast.gImGuIMs, (gLast.gImGuIMs / denom) * 100.0);
}
#else
void Framework::DrawInCurrentWindow() {}
#endif

// ---------- per-frame hook + full overlay window ----------
void Framework::PerfFrameStart(float dt, bool toggleKeyDown) {
    // Edge toggle for visibility (e.g., F1)
    if (toggleKeyDown && !sPrevToggleKey) sPerfVisible = !sPerfVisible;
    sPrevToggleKey = toggleKeyDown;

    // Roll last/current buffers at the start of the frame
    FlipFrame();

    // Store FPS sample for plot (OUR dt, not ImGui's)
    pushFpsSampleAndReturn(dt);
}

#if SOFASPUDS_ENABLE_EDITOR
void Framework::DrawPerformanceWindow() {
    if (!sPerfVisible) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGui::SetNextWindowDockID(ImGui::GetMainViewport()->ID, ImGuiCond_FirstUseEver);
    }
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("Performance", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing);

    // Show our own FPS (derived from Core dt)
    const float fpsNow = (sLastDtSec > 1e-6f) ? (1.0f / sLastDtSec) : 0.f;
    const float frameMs = (fpsNow > 1e-6f) ? (1000.0f / fpsNow) : 0.f;
    const float avgMs = (sAvgFps > 1e-6f) ? (1000.0f / sAvgFps) : 0.f;

    ImGui::Text("Engine FPS: %.1f (%.2f ms)   |   Avg: %.1f (%.2f ms over ~%d frames)",
        fpsNow, frameMs, sAvgFps, avgMs, sSamplesForAvg);
    ImGui::TextDisabled("Derived from Core dt (full frame), not ImGui.");
    ImGui::Separator();





    const double totalSystemMs = std::accumulate(
        gLastSystemTimings.begin(), gLastSystemTimings.end(), 0.0,
        [](double sum, const SystemTiming& e) { return sum + e.milliseconds; });

    ImGui::Text("Tracked systems total:   %.2f ms", totalSystemMs);
    ImGui::TextDisabled("Percentages below are relative to tracked systems (sum = 100%%).");

    for (const auto& entry : gLastSystemTimings) {
        const double pctOfSystems = (totalSystemMs > 1e-9)
            ? (entry.milliseconds / totalSystemMs) * 100.0 : 0.0;
        ImGui::Text("%s: %.3f ms (%.1f%% of systems)",
            entry.name.c_str(), entry.milliseconds, pctOfSystems);
    }


    // Last ~120 FPS samples
    ImGui::Separator();
    ImGui::PlotLines("FPS history", sFpsPlot, IM_ARRAYSIZE(sFpsPlot),
        sFpsPlotIdx, nullptr, 0.0f, 240.0f, ImVec2(260, 80));

    ImGui::Spacing();
    // Also embed the "last frame" breakdown table
    Framework::DrawInCurrentWindow();

    ImGui::End();
}
#else
void Framework::DrawPerformanceWindow() {}
#endif

// ---------- Optional helpers / getters ----------
void Framework::SetVisible(bool v) { sPerfVisible = v; }
void Framework::ToggleVisible() { sPerfVisible = !sPerfVisible; }
bool Framework::IsVisible() { return sPerfVisible; }

float Framework::GetLastDtSec() { return sLastDtSec; }
float Framework::GetFps() { return (sLastDtSec > 1e-6f) ? (1.0f / sLastDtSec) : 0.0f; }
float Framework::GetAvgFps() { return sAvgFps; }
void  Framework::SetFpsAvgWindow(int n) { sSamplesForAvg = (n <= 1) ? 1 : n; }
