/*********************************************************************************************
 \file      CircleRenderComponent.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the CircleRenderComponent class, a drawable component that renders
            circles using configurable radius and RGBA color values. Supports JSON
            serialization for data-driven initialization and cloning for prefab instancing.

 \copyright
            All content 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#pragma once
#include "Composition/Component.h"
#include "Serialization/Serialization.h"

namespace Framework {
    /*****************************************************************************************
      \class CircleRenderComponent
      \brief A rendering component specialized for drawing circles.

      Stores radius and RGBA color values. Supports serialization to load values
      from JSON configuration files and cloning to duplicate instances.

      This component is intended to be attached to a GameObjectComposition (GOC) to
      provide circle-drawing capabilities in the rendering pipeline.
    *****************************************************************************************/
    class CircleRenderComponent : public GameComponent {
    public:
        float radius{ 0.10f };     ///< Circle radius (default 0.10)
        float r{ 1.f }, g{ 1.f }, b{ 1.f }, a{ 1.f }; ///< RGBA color values (default white)

        /*************************************************************************************
          \brief Initializes the component.
                 Default implementation does nothing but may be extended if needed.
        *************************************************************************************/
        void initialize() override {}

        /*************************************************************************************
          \brief Handles incoming messages for this component.
          \param m  Reference to a Message object.
          \note  Currently ignores the message parameter.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes the component data from a JSON stream.
          \param s  Reference to the serializer providing JSON data.
          \note  Reads radius and RGBA values if present in the JSON definition.
        *************************************************************************************/
        void Serialize(ISerializer& s) override {
            if (s.HasKey("radius")) StreamRead(s, "radius", radius);
            if (s.HasKey("r"))      StreamRead(s, "r", r);
            if (s.HasKey("g"))      StreamRead(s, "g", g);
            if (s.HasKey("b"))      StreamRead(s, "b", b);
            if (s.HasKey("a"))      StreamRead(s, "a", a);
        }

        /*************************************************************************************
          \brief Creates a clone of this CircleRenderComponent.
          \return A unique_ptr containing a copy of the component.
          \note  Copies all values (radius, r, g, b, a) into the new instance.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override {
            // Create new CircleRenderComponent on heap
            // Wrap inside unique_ptr so it is automatically clean up if something goes wrong
            auto copy = std::make_unique<CircleRenderComponent>();
            // copy the values 
            copy->radius = radius;
            copy->r = r;
            copy->g = g;
            copy->b = b;
            copy->a = a;
            // Transfer ownership to whoever call clone()
            return copy;
        }
    };
}
