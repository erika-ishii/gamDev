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
// Component/RenderComponent.hpp
#pragma once
#include <string>
#include "Composition/Component.h"
#include "Serialization/Serialization.h"
#include "Resource_Asset_Manager/Resource_Manager.h"
#include "Core/PathUtils.h"
#include <filesystem>


namespace Framework {
    /*****************************************************************************************
      \class RenderComponent
      \brief A rendering component specialized for drawing rectangles/quads.

      Stores width, height, and RGBA tint values. Supports serialization to load values
      from JSON configuration files and cloning to duplicate instances.

      This component is intended to be attached to a GameObjectComposition (GOC) to
      provide basic rectangle/quad rendering functionality in the graphics pipeline.
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

        /*************************************************************************************
          \brief Initializes the component.
                 Default implementation does nothing but may be extended if needed.
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
          \param m  Reference to a Message object.
          \note  Currently ignores the message parameter.
        *************************************************************************************/
        void SendMessage(Message& m) override { (void)m; }

        /*************************************************************************************
          \brief Serializes the component data from a JSON stream.
          \param s  Reference to the serializer providing JSON data.
          \note  Reads width, height, and RGBA values if present in the JSON definition.
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
        }

        /*************************************************************************************
          \brief Creates a clone of this RenderComponent.
          \return A unique_ptr containing a copy of the component.
          \note  Copies all values (w, h, r, g, b, a) into the new instance.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override {
            // Create new CircleRenderComponent on heap
            // Wrap inside unique_ptr so it is automatically clean up if something goes wrong
            auto copy = std::make_unique<RenderComponent>();
            //copy the values
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

            //Transfer ownership to whoever call clone()
            return copy;
        }
    };
}
