/*********************************************************************************************
 \file      HierarchyPanel.h
 \par       SofaSpuds
 \author    
 \brief     Declaration of the ImGui hierarchy panel.
 \details   Exposes a single entry point to draw a Name/ID table sourced from the Factory.
            Selection state is managed externally via Selection.h utilities used in the .cpp.
            Include this header where the editor UI is assembled and call DrawHierarchyPanel().
 © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*********************************************************************************************/

#pragma once

namespace mygame {

    /**
     * \brief Draw the Hierarchy editor panel (objects list with selection).
     */
    void DrawHierarchyPanel();

} // namespace mygame
