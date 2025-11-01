#include "GUISystem.hpp"
#include "Systems/RenderSystem.h"
#include "Graphics/Graphics.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>

void GUISystem::Clear() {
    buttons_.clear();
    prevMouseDown_ = false;
}

void GUISystem::AddButton(float x, float y, float w, float h,
    const std::string& label,
    std::function<void()> onClick) {
    Button b;
    b.x = x; b.y = y; b.w = w; b.h = h;
    b.label = label;
    b.onClick = std::move(onClick);
    buttons_.push_back(std::move(b));
}

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


bool GUISystem::Contains(const Button& b, double mx, double my) {
    return (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h);
}

bool GUISystem::RisingEdgeLeftClick(bool now, bool& prev) {
    const bool edge = (now && !prev);
    prev = now;
    return edge;
}

void GUISystem::Update(Framework::InputSystem* /*input*/) {
    GLFWwindow* w = glfwGetCurrentContext();
    if (!w) return;

    double mx, myTop;
    glfwGetCursorPos(w, &mx, &myTop);

    // flip Y so (0,0) is bottom-left (to match your render space)
    int winW = 0, winH = 0;
    glfwGetWindowSize(w, &winW, &winH);
    const double my = static_cast<double>(winH) - myTop;

    for (auto& b : buttons_) {
        b.hovered = Contains(b, mx, my);
    }

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

void GUISystem::Draw(Framework::RenderSystem* render) {

    const int screenW = render ? render->ScreenWidth() : 1280;
    const int screenH = render ? render->ScreenHeight() : 720;

    // simple visual: darker when idle, brighter on hover
    for (const auto& b : buttons_) {
        bool renderedTexture = false;
        if (b.useTextures) {
            const unsigned tex = (b.hovered && b.hoverTexture) ? b.hoverTexture : b.idleTexture;
            if (tex != 0) {
                gfx::Graphics::renderSpriteUI(tex, b.x, b.y, b.w, b.h,
                    1.f, 1.f, 1.f, 1.f, screenW, screenH);
                renderedTexture = true;

                if (b.hovered) {
                    // subtle highlight overlay for hover feedback
                    gfx::Graphics::renderRectangleUI(b.x, b.y, b.w, b.h,
                        1.f, 1.f, 1.f, 0.18f, screenW, screenH);
                }
            }
        }

        if (!renderedTexture) {
            const float c = b.hovered ? 0.85f : 0.55f;
            gfx::Graphics::renderRectangleUI(b.x, b.y, b.w, b.h,
                c, c, c, 0.95f, screenW, screenH);
        }
        const bool shouldDrawLabel = (!b.useTextures || b.drawLabelOnTexture || !renderedTexture);
        if (shouldDrawLabel && render && render->IsTextReadyHint()) {
            // vertically center-ish the label in the button
            const float labelX = b.x + 24.f;
            const float labelY = b.y + (b.h * 0.5f) - 8.f;
            render->GetTextHint().RenderText(b.label.c_str(), labelX, labelY, 0.9f, { 1.f,1.f,1.f });
        }
    }
}
