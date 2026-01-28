/*********************************************************************************************
 \file      GlowComponent.h
 \par       SofaSpuds
 \author    OpenAI Assistant

 \brief     Declares the GlowComponent class, a procedural glow renderer that supports
            freehand point strokes, configurable color/opacity, and radial falloff.
            Supports JSON serialization for data-driven initialization and cloning for
            prefab instancing.

 \copyright
            All content 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"
#include <glm/vec2.hpp>
#include <vector>

namespace Framework {
    /*****************************************************************************************
      \class GlowComponent
      \brief A rendering component specialized for procedural glow blobs and strokes.

      Stores color/opacity, brightness, inner/outer radius, falloff exponent, and a list
      of local-space points that define a freehand stroke. Each point renders a radial
      glow without needing a texture.
    *****************************************************************************************/
    class GlowComponent : public GameComponent {
    public:
        float r{ 1.f }, g{ 0.8f }, b{ 0.3f };
        float opacity{ 1.f };
        float brightness{ 1.f };
        float innerRadius{ 0.05f };
        float outerRadius{ 0.2f };
        float falloffExponent{ 1.0f };
        bool  visible{ true };

        std::vector<glm::vec2> points{}; ///< Local-space stroke points (relative to owner transform).

        void initialize() override {}
        void SendMessage(Message& m) override { (void)m; }

        void Serialize(ISerializer& s) override {
            if (s.HasKey("r")) StreamRead(s, "r", r);
            if (s.HasKey("g")) StreamRead(s, "g", g);
            if (s.HasKey("b")) StreamRead(s, "b", b);
            if (s.HasKey("opacity")) StreamRead(s, "opacity", opacity);
            if (s.HasKey("a")) StreamRead(s, "a", opacity);
            if (s.HasKey("brightness")) StreamRead(s, "brightness", brightness);
            if (s.HasKey("inner_radius")) StreamRead(s, "inner_radius", innerRadius);
            if (s.HasKey("outer_radius")) StreamRead(s, "outer_radius", outerRadius);
            if (s.HasKey("falloff_exponent")) StreamRead(s, "falloff_exponent", falloffExponent);
            if (s.HasKey("visible")) StreamRead(s, "visible", visible);

            points.clear();
            if (s.EnterArray("points"))
            {
                const size_t count = s.ArraySize();
                points.reserve(count);
                for (size_t i = 0; i < count; ++i)
                {
                    if (!s.EnterIndex(i))
                        continue;
                    float x = 0.f;
                    float y = 0.f;
                    if (s.HasKey("x")) StreamRead(s, "x", x);
                    if (s.HasKey("y")) StreamRead(s, "y", y);
                    points.emplace_back(x, y);
                    s.ExitObject();
                }
                s.ExitArray();
            }
        }

        ComponentHandle Clone() const override {
            auto copy = ComponentPool<GlowComponent>::CreateTyped();
            copy->r = r;
            copy->g = g;
            copy->b = b;
            copy->opacity = opacity;
            copy->brightness = brightness;
            copy->innerRadius = innerRadius;
            copy->outerRadius = outerRadius;
            copy->falloffExponent = falloffExponent;
            copy->visible = visible;
            copy->points = points;
            return copy;
        }
    };
}
