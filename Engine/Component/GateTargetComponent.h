/*********************************************************************************************
 \file      GateTargetComponent.h
 \par       SofaSpuds
 \author    

 \brief     Declares GateTargetComponent, which stores the level file to load when a gate
            is activated.
 \details   GateTargetComponent provides a data-driven way to bind a gate to a level JSON
            file via serialization. GateController reads this component to determine the
            transition target when the player contacts an unlocked gate.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <string>

#include "Composition/Component.h"
#include "Memory/ComponentPool.h"
#include "Serialization/Serialization.h"

namespace Framework {

    class GateTargetComponent : public GameComponent
    {
    public:
        //Level JSON path to load when entering this gate 
        std::string levelPath{};

        void Serialize(ISerializer& s) override
        {
            if (s.HasKey("level_path")) {
                StreamRead(s, "level_path", levelPath);
            }
        }

        ComponentHandle Clone() const override
        {
            return ComponentPool<GateTargetComponent>::Create(*this);
        }
    };

} //namespace Framework