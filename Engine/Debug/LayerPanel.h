/*********************************************************************************************
 \file      LayerPanel.h
 \par       SofaSpuds
 \author   Elvis

 \brief     Declares the ImGui panel for managing layer visibility and spawn layer selection.

 \details
            The panel exposes fixed layer groups (Background, Gameplay, Foreground, UI)
            and sublayers (0..20). It allows enabling/disabling groups or sublayers, as
            well as "Enable All" and "Enable Only Selected" utilities for testing.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include <string>

#if SOFASPUDS_ENABLE_EDITOR
namespace mygame {
    void DrawLayerPanel();
    const std::string& ActiveLayerName();
}
#endif
