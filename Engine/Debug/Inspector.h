/*********************************************************************************************
 \file      Inspector.h
 \par       SofaSpuds
 \author    

 \brief     Declares the ImGui inspector panel that surfaces the properties of the
            currently selected game object. The inspector mirrors the active selection that
            comes from viewport picking or the hierarchy panel and exposes component data so
            that designers can review or tweak values without leaving the editor viewport.

 \copyright
            All content �2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

namespace mygame {

    /// Draw the inspector window. Shows identity data and editable component values
    /// for the currently selected object (if any).
    void DrawInspectorWindow();

} // namespace mygame