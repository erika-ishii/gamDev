/*********************************************************************************************
 \file      TransformComponent.h
 \par       SofaSpuds
 \author    elvisshengjie.lim ( elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the TransformComponent class, which provides spatial data (position
            and rotation) for game objects. Supports JSON serialization and cloning for
            prefab instancing. Intended to be attached to GameObjectComposition instances
            as the foundation for positioning and orientation in the game world.

 \copyright
            All content ?2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once
#include "Composition/Component.h"
#include <iostream>

namespace Framework {

    /*****************************************************************************************
      \class TransformComponent
      \brief Component responsible for storing spatial information of a game object.

      Holds x and y coordinates representing the position, and rot representing the
      rotation angle. Provides serialization for data-driven initialization, logging
      during initialization, and cloning support for prefab duplication.
    *****************************************************************************************/
    class TransformComponent : public GameComponent {
    public:
        float x{ 0.0f };   ///< X-coordinate position of the object
        float y{ 0.0f };   ///< Y-coordinate position of the object
        float rot{ 0.0f }; ///< Rotation angle (in radians or degrees depending on convention)
        float scaleX{ 1.0f }; ///< Scale factor along the X axis
        float scaleY{ 1.0f }; ///< Scale factor along the Y axis
        /*************************************************************************************
          \brief Initializes the transform component.
          \note  Logs the current transform state to the console for debugging purposes.
        *************************************************************************************/
        void initialize() override {
            std::cout << "[TransformComponent] init: x=" << x << " y=" << y << " rot=" << rot << " sx=" << scaleX << " sy=" << scaleY << "\n";
        }

        /*************************************************************************************
          \brief Handles incoming messages.
          \param m  Reference to a Message object.
          \note  Currently unused; provided for consistency with GameComponent interface.
        *************************************************************************************/
        void SendMessage(Message& m) override {
            (void)m; // nothing for now
        }

        /*************************************************************************************
          \brief Serializes transform data from a JSON stream.
          \param s  Serializer reference providing JSON data.
          \note  Reads x, y, and rot values if present in the JSON definition.
        *************************************************************************************/
        void Serialize(ISerializer& s) override {
            
            if (s.HasKey("x"))   StreamRead(s, "x", x);
            if (s.HasKey("y"))   StreamRead(s, "y", y);
            if (s.HasKey("rot")) StreamRead(s, "rot", rot);
            if (s.HasKey("scale_x")) StreamRead(s, "scale_x", scaleX);
            if (s.HasKey("scale_y")) StreamRead(s, "scale_y", scaleY);
        }

        /*************************************************************************************
          \brief Creates a clone of this TransformComponent.
          \return A unique_ptr containing a copy of the component.
          \note  Copies x, y, and rot values into the new instance.
        *************************************************************************************/
        std::unique_ptr<GameComponent> Clone() const override {
            // Create new CircleRenderComponent on heap
            // Wrap inside unique_ptr so it is automatically clean up if something goes wrong
            auto copy = std::make_unique<TransformComponent>();
            //copy the values 
            copy->x = x;
            copy->y = y;
            copy->rot = rot;
            copy->scaleX = scaleX;
            copy->scaleY = scaleY;
            //Transfer ownership to whoever call clone()
            return copy;
        }
    };

} // namespace Framework
