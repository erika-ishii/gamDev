#pragma once
#include <string_view>
/*********************************************************************************************
 \file      Perf.h
 \par       SofaSpuds
 \author    yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Lightweight per-frame CPU timing HUD for debugging/performance profiling.
 \details   Tracks CPU times for Update / Render / ImGui and exposes a small ImGui overlay
            showing section breakdowns, a rolling FPS history, and engine FPS derived from
            the Core loop dt (not ImGui). Call PerfFrameStart(dt, toggleKeyDown) once per
            frame, then setUpdate/setRender/setImGui around your profiled scopes, and finally
            draw the window via DrawPerformanceWindow().
 \copyright
            All content ?2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

namespace Framework {

    // ----- Frame book-keeping ---------------------------------------------------------------
    /// Roll ¡°current ¡ú last¡± and clear current (called once at the very start of a frame).
    void FlipFrame();

    /// Record CPU time (ms) for engine Update() this frame.
    void setUpdate(double ms);

    /// Record CPU time (ms) for non-ImGui rendering work this frame.
    void setRender(double ms);

    /// Record CPU time (ms) for ImGui build + draw this frame.
    void setImGui(double ms);

    /// Accumulate CPU time (ms) spent inside a named engine system for the current frame.
    /// Can be called multiple times per frame for the same system name (times are summed).
    void RecordSystemTiming(std::string_view systemName, double milliseconds);

    // ----- Minimal embed summary (draws into the current ImGui window; no Begin/End) --------
    void DrawInCurrentWindow();

    // ----- Overlay lifecycle ----------------------------------------------------------------
    /// Call once per frame from your main loop. `dt` is the Core/measured frame delta (seconds).
    /// `toggleKeyDown` is an edge-toggled key (e.g., F1) to show/hide the overlay.
    void PerfFrameStart(float dt, bool toggleKeyDown);

    /// Draw the floating Performance window (no-op if hidden).
    void DrawPerformanceWindow();

    // ----- Optional helpers (nice for HUDs/menus/tools) -------------------------------------
    /// Show/hide the overlay explicitly.
    void SetVisible(bool visible);

    /// Toggle overlay visibility.
    void ToggleVisible();

    /// Query current overlay visibility.
    bool IsVisible();

    /// Last measured Core delta time (seconds) passed to PerfFrameStart().
    float GetLastDtSec();

    /// Instantaneous engine FPS computed from last dt (1/dt).
    float GetFps();

    /// Smoothed/average FPS over a small recent window.
    float GetAvgFps();

    /// Set the averaging window (number of recent frames) used by GetAvgFps().
    /// Values <= 1 disable smoothing.
    void SetFpsAvgWindow(int frameCount);
} // namespace Framework
