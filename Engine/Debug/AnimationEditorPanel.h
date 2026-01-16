/*********************************************************************************************
 \file      AnimationEditorPanel.h
 \par       SofaSpuds
 \author    yimo.kong (yimo.kong@digipen.edu) - Primary Author, 100%
 \brief     Declaration of the ImGui-based Animation Editor panel API.
 \details   Exposes a single entry point used by the in-engine editor to render the
            sprite animation editing UI. The panel:
              - Attaches to the current editor context and draws an "Animation Editor"
                window when toggled on.
              - Operates on the currently selected GameObject and its
                SpriteAnimationComponent (selection and component logic are implemented
                in the corresponding .cpp file).
              - Uses the provided visibility flag to allow the editor to open/close
                the panel via ImGui.
            This header is intentionally lightweight and only declares the UI hook,
            keeping implementation details in AnimationEditorPanel.cpp.
 \copyright
            All content ? 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#pragma once

#include <string>

namespace mygame
{
    /*****************************************************************************************
     \brief Draws the sprite Animation Editor window when enabled.
     \param open Reference to the visibility flag; updated when the user closes the window.
     \note  The function is a no-op if \p open is false. When the ImGui close button is
            pressed, the flag is set to false so the caller can track panel visibility.
    *****************************************************************************************/
    void DrawAnimationEditor(bool& open);
}
