/*********************************************************************************************
 \file      RenderComponent.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the RenderComponent class, a basic rendering component responsible
            for drawing rectangular shapes (quads) with configurable size and tint color.
            Supports JSON serialization for data-driven initialization and cloning for
            prefab instancing.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>

#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Resource_Asset_Manager/Resource_Manager.h"
#include "Core/PathUtils.h"
#include <filesystem>

namespace Framework {

    // Blend mode enum used across rendering/UI code.
    enum class BlendMode {
        Alpha = 0,
        Add,
        Multiply,
        PremultipliedAlpha,
        Screen,
        Subtract,
        Lighten,
        Darken,
        None,
        SolidColor
    };

    inline constexpr std::array<const char*, 10> kBlendModeLabels = {
        "Alpha",
        "Add",
        "Multiply",
        "PremultipliedAlpha",
        "Screen",
        "Subtract",
        "Lighten",
        "Darken",
        "None",
        "SolidColor"
    };

    inline const char* BlendModeToString(BlendMode mode)
    {
        switch (mode) {
        case BlendMode::Alpha: return "alpha";
        case BlendMode::Add: return "add";
        case BlendMode::Multiply: return "multiply";
        case BlendMode::PremultipliedAlpha: return "premultipliedalpha";
        case BlendMode::Screen: return "screen";
        case BlendMode::Subtract: return "subtract";
        case BlendMode::Lighten: return "lighten";
        case BlendMode::Darken: return "darken";
        case BlendMode::None: return "none";
        case BlendMode::SolidColor: return "solidcolor";
        default: return "alpha";
        }
    }

    inline std::string NormalizeBlendModeString(std::string_view value)
    {
        std::string normalized;
        normalized.reserve(value.size());
        for (unsigned char c : value)
        {
            if (std::isalnum(c))
            {
                normalized.push_back(static_cast<char>(std::tolower(c)));
            }
        }
        return normalized;
    }

    inline bool TryParseBlendMode(std::string_view value, BlendMode& out)
    {
        const std::string key = NormalizeBlendModeString(value);
        if (key == "alpha")
            out = BlendMode::Alpha;
        else if (key == "add")
            out = BlendMode::Add;
        else if (key == "multiply")
            out = BlendMode::Multiply;
        else if (key == "premultipliedalpha")
            out = BlendMode::PremultipliedAlpha;
        else if (key == "screen")
            out = BlendMode::Screen;
        else if (key == "subtract")
            out = BlendMode::Subtract;
        else if (key == "lighten")
            out = BlendMode::Lighten;
        else if (key == "darken")
            out = BlendMode::Darken;
        else if (key == "none")
            out = BlendMode::None;
        else if (key == "solidcolor")
            out = BlendMode::SolidColor;
        else
            return false;

        return true;
    }

    /*****************************************************************************************
      \class RenderComponent
      \brief A rendering component specialized for drawing rectangles/quads.

      Stores width, height, and RGBA tint values. Supports serialization to load values
      from JSON configuration files and cloning to duplicate instances.
    *****************************************************************************************/
    class RenderComponent : public GameComponent {
    public:
        float w{ 64.f }, h{ 64.f };               ///< Width and height (treated as scale factors in NDC)
        float r{ 1.f }, g{ 1.f }, b{ 1.f }, a{ 1.f }; ///< RGBA tint color values (default white)
        int layer = 0;

        unsigned int texture_id{ 0 };
        std::string  texture_key;
        std::string  texture_path;

        bool visible{ true };

        // New: blend mode for this renderable.
        BlendMode blendMode{ BlendMode::Alpha };

        /*************************************************************************************
          \brief Initializes the component.
                 Default implementation resolves texture_key -> texture_id when possible.
        *************************************************************************************/
        void initialize() override {
            if (texture_key.empty())
                return;

            texture_id = Resource_Manager::getTexture(texture_key);
            if (texture_id)
                return;

            if (texture_path.empty())
                return;

            const auto resolvedPath = Framework::ResolveAssetPath(std::filesystem::path(texture_path));
            const std::string& pathStr = resolvedPath.empty() ? texture_path : resolvedPath.string();

            if (Resource_Manager::load(texture_key, pathStr))
                texture_id = Resource_Manager::getTexture(texture_key);
        }

        /*************************************************************************************
          \brief Handles incoming messages for this component.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes the component data from a JSON stream.
                 Reads width/height, color, visible, texture keys/paths, layer, and blend mode.
        *************************************************************************************/
        void Serialize(ISerializer& s) override {
            if (s.HasKey("w")) StreamRead(s, "w", w);
            if (s.HasKey("h")) StreamRead(s, "h", h);
            if (s.HasKey("r")) StreamRead(s, "r", r);
            if (s.HasKey("g")) StreamRead(s, "g", g);
            if (s.HasKey("b")) StreamRead(s, "b", b);
            if (s.HasKey("a")) StreamRead(s, "a", a);
            if (s.HasKey("texture_key")) StreamRead(s, "texture_key", texture_key);
            if (s.HasKey("texture_path")) StreamRead(s, "texture_path", texture_path);
            if (s.HasKey("layer")) StreamRead(s, "layer", layer);

            if (s.HasKey("visible")) {
                int visibleInt = static_cast<int>(visible);
                StreamRead(s, "visible", visibleInt);
                visible = (visibleInt != 0);
            }

            if (s.HasKey("blend_mode")) {
                std::string modeValue;
                StreamRead(s, "blend_mode", modeValue);
                BlendMode parsedMode = BlendMode::Alpha;
                if (!TryParseBlendMode(modeValue, parsedMode)) {
                    std::cerr << "[RenderComponent] Unknown blend_mode '" << modeValue
                        << "', defaulting to Alpha.\n";
                }
                blendMode = parsedMode;
            }
        }

        /*************************************************************************************
          \brief Creates a clone of this RenderComponent.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override {
            auto copy = std::make_unique<RenderComponent>();
            copy->w = w;
            copy->h = h;
            copy->r = r;
            copy->g = g;
            copy->b = b;
            copy->a = a;
            copy->texture_key = texture_key;
            copy->texture_id = texture_id;
            copy->texture_path = texture_path;
            copy->visible = visible;
            copy->layer = layer;
            copy->blendMode = blendMode;
            return copy;
        }
    };
} // namespace Framework