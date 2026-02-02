#pragma once
#include "JsonEditorPanel.h"
namespace mygame {
    /*********************************************************************************
      \brief Draws the Asset Manager editor panel using ImGui.
      \param jsonPanel Optional pointer to a JsonEditorPanel for prefab refresh.
      \details
          - Shows a refreshable list of all assets from AssetManager.
          - Provides a search bar to filter asset names.
          - Displays selected asset path and type.
          - Allows loading or deleting selected assets.
          - Supports creation of JSON prefabs for objects or enemies via templates.
          - Prevents deletion of non-existing assets.
          - Displays error messages in red if prefab creation fails.
    *********************************************************************************/
    void DrawAssetManagerPanel(JsonEditorPanel* jsonPanel);
}