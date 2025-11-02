/*********************************************************************************************
 \file      Selection.h
 \par       SofaSpuds
 \author    erika.ishii

 \brief     Declares helper functions used by the editor to keep track of the currently
            selected game object. The selection is shared between panels (e.g. hierarchy)
            and the in-viewport picking/dragging code.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include "Composition/Composition.h"

namespace mygame {

    /// Clear any active object selection.
    void ClearSelection();

    /// Remember the provided object id as the active selection (0 clears).
    void SetSelectedObjectId(Framework::GOCId id);

    /// Retrieve the currently selected object id (0 when nothing is selected).
    Framework::GOCId GetSelectedObjectId();

    /// Convenience helper to test whether an object is selected.
    bool HasSelectedObject();

} // namespace mygame
