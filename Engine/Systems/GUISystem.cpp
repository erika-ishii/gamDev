/*********************************************************************************************
 \file      GUISystem.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     Lightweight immediate-mode GUI system for in-game/menu buttons.
 \details   This module provides a tiny GUI layer used by pages/menus:
            - Storage: keeps a simple vector<Button> with position, size, label and callbacks.
            - Input: polls GLFW cursor/mouse, flips Y to bottom-left origin, and does hit-tests.
            - Interaction: rising-edge left click dispatches exactly one onClick() per frame.
            - Rendering: draws textured buttons if textures are provided; otherwise flat rects;
                        optionally overlays a hover highlight and draws a label using TextHint.
            - Integration: sized using RenderSystem's screen width/height; uses Graphics.cpp
                          UI helpers (renderRectangleUI/renderSpriteUI) for pixel-space draw.
            No layout engine is included—caller is responsible for positioning buttons.
            This keeps the system predictable and easy to reason about for simple menus.
*********************************************************************************************/

#include "GUISystem.hpp"
#include "Systems/RenderSystem.h"
#include "Graphics/Graphics.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>

/*************************************************************************************
  \brief  Remove all buttons and reset mouse state so no click carries over.
*************************************************************************************/
void GUISystem::Clear() {
    buttons_.clear();
    prevMouseDown_ = false;
}

/*************************************************************************************
  \brief  Add a basic rectangle button with a text label and a click callback.
  \param  x,y,w,h    Button rectangle in UI pixels (bottom-left origin).
  \param  label      Text shown on top of the button.
  \param  onClick    Function to invoke on a rising-edge left click while hovered.
  \note   This overload creates a non-textured button (flat color visual).
*************************************************************************************/
void GUISystem::AddButton(float x, float y, float w, float h,
    const std::string& label,
    std::function<void()> onClick) {
    Button b;
    b.x = x; b.y = y; b.w = w; b.h = h;
    b.label = label;
    b.onClick = std::move(onClick);
    buttons_.push_back(std::move(b));
}

/*************************************************************************************
  \brief  Add a textured button with optional hover texture and label rendering.
  \param  x,y,w,h               Button rectangle in UI pixels (bottom-left origin).
  \param  label                 Text label (can be drawn on top of the texture).
  \param  idleTexture           GL texture id used normally (0 => fallback to flat color).
  \param  hoverTexture          GL texture id used on hover (0 => reuse idleTexture).
  \param  onClick               Function to invoke on rising-edge left click while hovered.
  \param  drawLabelOnTexture    If true, draws the label even when a texture is used.
  \details Internally forwards to the basic AddButton(), then augments the just-added
           button with texture fields and behavior flags.
*************************************************************************************/
void GUISystem::AddButton(float x, float y, float w, float h,
    const std::string& label,
    unsigned idleTexture,
    unsigned hoverTexture,
    std::function<void()> onClick,
    bool drawLabelOnTexture) {
    AddButton(x, y, w, h, label, std::move(onClick));
    if (buttons_.empty()) {
        return;
    }

    Button& b = buttons_.back();
    b.idleTexture = idleTexture;
    b.hoverTexture = hoverTexture ? hoverTexture : idleTexture;
    b.useTextures = (b.idleTexture != 0);
    b.drawLabelOnTexture = drawLabelOnTexture;
}

/*************************************************************************************
  \brief  Return true if point (mx,my) lies inside button's axis-aligned rectangle.
  \param  b    Button rectangle to test against.
  \param  mx   Cursor X in UI pixels (bottom-left origin).
  \param  my   Cursor Y in UI pixels (bottom-left origin).
*************************************************************************************/
bool GUISystem::Contains(const Button& b, double mx, double my) {
    return (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h);
}

/*************************************************************************************
  \brief  Detect a rising-edge on the left mouse button.
  \param  now   Current boolean state (true if pressed this frame).
  \param  prev  In/Out previous state (updated to now before return).
  \return True exactly on frames where the state goes false -> true.
  \note   This logic ensures a single click only triggers once per press.
*************************************************************************************/
bool GUISystem::RisingEdgeLeftClick(bool now, bool& prev) {
    const bool edge = (now && !prev);
    prev = now;
    return edge;
}

/*************************************************************************************
  \brief  Poll the mouse and update each button's hover state; dispatch clicks.
  \param  input  Optional input system (currently unused—GLFW is queried directly).
  \details Steps:
           1) Read cursor position and convert to bottom-left origin (flip Y).
           2) Mark each button's hovered flag via Contains().
           3) On rising-edge LMB, invoke onClick() for the first hovered button.
*************************************************************************************/
void GUISystem::Update(Framework::InputSystem* /*input*/) {
    GLFWwindow* w = glfwGetCurrentContext();
    if (!w) return;

    // Cursor in window coords (top-left origin). Convert to framebuffer space so it matches
  // the projection used by UI rendering (which relies on framebuffer dimensions).
    double mxLogical = 0.0, myTopLogical = 0.0;
    glfwGetCursorPos(w, &mxLogical, &myTopLogical);
    int winW = 1, winH = 1;
    glfwGetWindowSize(w, &winW, &winH);
  
    int fbW = winW, fbH = winH;
    glfwGetFramebufferSize(w, &fbW, &fbH);

    const double scaleX = winW > 0 ? static_cast<double>(fbW) / static_cast<double>(winW) : 1.0;
    const double scaleY = winH > 0 ? static_cast<double>(fbH) / static_cast<double>(winH) : 1.0;

    const double mx = mxLogical * scaleX;
    const double my = (static_cast<double>(winH) - myTopLogical) * scaleY;
    // Hover test for all buttons.
    for (auto& b : buttons_) {
        b.hovered = Contains(b, mx, my);
    }

    // Rising-edge click dispatch.
    const bool mouseNow = (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    if (RisingEdgeLeftClick(mouseNow, prevMouseDown_)) {
        for (auto& b : buttons_) {
            if (b.hovered && b.onClick) {
                b.onClick();
                break; // one click -> one button
            }
        }
    }
}

/*************************************************************************************
  \brief  Draw all buttons in UI pixel space using Graphics.cpp helpers.
  \param  render  RenderSystem (used for screen size and text rendering).
  \details For each button:
           - If textures are configured, draw the appropriate texture (hover vs idle).
           - Overlay a subtle white highlight when hovered for feedback.
           - Otherwise draw a flat rectangle with a hover-lifted brightness.
           - If label drawing is enabled (or no texture), draw the label with TextHint.
  \note    Screen size defaults (1280x720) are used if no RenderSystem is supplied.
*************************************************************************************/
void GUISystem::Draw(Framework::RenderSystem* render) {

    const int screenW = render ? render->ScreenWidth() : 1280;
    const int screenH = render ? render->ScreenHeight() : 720;

    // Render each button as textured sprite or flat rectangle.
    for (const auto& b : buttons_) {
        bool renderedTexture = false;
        if (b.useTextures) {
            const unsigned tex = (b.hovered && b.hoverTexture) ? b.hoverTexture : b.idleTexture;
            if (tex != 0) {
                gfx::Graphics::renderSpriteUI(tex, b.x, b.y, b.w, b.h,
                    1.f, 1.f, 1.f, 1.f, screenW, screenH);
                renderedTexture = true;

                if (b.hovered) {
                    // Subtle highlight overlay for hover feedback.
                    gfx::Graphics::renderRectangleUI(b.x, b.y, b.w, b.h,
                        1.f, 1.f, 1.f, 0.18f, screenW, screenH);
                }
            }
        }

        // Fallback: flat rectangle if no textures were drawn.
        if (!renderedTexture) {
            const float c = b.hovered ? 0.85f : 0.55f;
            gfx::Graphics::renderRectangleUI(b.x, b.y, b.w, b.h,
                c, c, c, 0.95f, screenW, screenH);
        }

        // Draw label if textures are not used, or explicitly requested.
        const bool shouldDrawLabel = (!b.useTextures || b.drawLabelOnTexture || !renderedTexture);
        if (shouldDrawLabel && render && render->IsTextReadyHint()) {
            // Vertically center-ish the label in the button.
            const float labelX = b.x + 24.f;
            const float labelY = b.y + (b.h * 0.5f) - 8.f;
            render->GetTextHint().RenderText(b.label.c_str(), labelX, labelY, 0.9f, { 1.f,1.f,1.f });
        }
    }
}
