/*********************************************************************************************
 \file      Selection.cpp
 \par       SofaSpuds
 \author    erika.ishii (erika.ishii@digipen.edu) - Main Author, 100%
 \brief     Implements the shared editor selection helpers declared in Selection.h.
            Stores the currently selected game object id in a single translation unit so
            that both ImGui panels and the viewport picking logic can keep each other in
            sync.

 \copyright
            All content ?2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#include "Selection.h"

namespace mygame {

    namespace {
        /// Global editor selection (0 means "no selection").
        Framework::GOCId gSelectedObjectId = 0;
    }

    void ClearSelection()
    {
        gSelectedObjectId = 0;
    }

    void SetSelectedObjectId(Framework::GOCId id)
    {
        gSelectedObjectId = id;
    }

    Framework::GOCId GetSelectedObjectId()
    {
        return gSelectedObjectId;
    }

    bool HasSelectedObject()
    {
        return gSelectedObjectId != 0;
    }

} // namespace mygame
