#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Framework {
    class InputSystem;
    class RenderSystem;
}

class GUISystem {
public:
    struct Button {
        float x{}, y{}, w{}, h{};
        std::string label;
        std::function<void()> onClick;
        bool hovered{ false };
    };

    void Clear();
    void AddButton(float x, float y, float w, float h,
        const std::string& label,
        std::function<void()> onClick);

    void Update(Framework::InputSystem* input);
    void Draw(Framework::RenderSystem* render);

private:
    std::vector<Button> buttons_;
    bool prevMouseDown_{ false };

    static bool Contains(const Button& b, double mx, double my);
    static bool RisingEdgeLeftClick(bool now, bool& prev);
};
