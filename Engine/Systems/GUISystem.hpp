/*********************************************************************************************
 \file      GUISystem.h
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Primary Author, 100%
 \brief     Immediate-mode GUI for clickable buttons (text or textured).
 \details   Manages a flat list of buttons, updates hover and rising-edge click state from
            InputSystem, and renders via RenderSystem. Absolute positioning is used; the
            renderer interprets coordinates and draws rectangles or textures as needed.
 \copyright
            All content ©2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Framework {
    class InputSystem;
    class RenderSystem;
}

/**
 * \class GUISystem
 * \brief Minimal button-based GUI updated per frame and drawn by the renderer.
 */
class GUISystem {
public:
    /**
     * \struct Button
     * \brief Rectangular widget with optional textures and a click callback.
     */
    struct Button {
        float x{}, y{}, w{}, h{};     
        std::string label;            
        std::function<void()> onClick;
        bool hovered{ false };        
        unsigned idleTexture{ 0 };    
        unsigned hoverTexture{ 0 };   
        bool useTextures{ false };    
        bool drawLabelOnTexture{ false };
    };

    //Remove all buttons.
    void Clear();

    // Add a text-only button.
    void AddButton(float x, float y, float w, float h,
        const std::string& label,
        std::function<void()> onClick);

    // Add a textured button with idle/hover textures; label may be drawn on top.
    void AddButton(float x, float y, float w, float h,
        const std::string& label,
        unsigned idleTexture,
        unsigned hoverTexture,
        std::function<void()> onClick,
        bool drawLabelOnTexture = false);

    // Update hover and click state from the input system (rising-edge detection).
    void Update(Framework::InputSystem* input);

    // Draw all buttons using the render system.
    void Draw(Framework::RenderSystem* render);

private:
    std::vector<Button> buttons_;   
    bool prevMouseDown_{ false };   

    // Point-in-rect test.
    static bool Contains(const Button& b, double mx, double my);

    // Detect left-mouse rising edge; returns true on transition false->true and updates \p prev.
    static bool RisingEdgeLeftClick(bool now, bool& prev);
};
