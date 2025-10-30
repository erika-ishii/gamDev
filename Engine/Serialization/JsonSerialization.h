/*********************************************************************************************
 \file      JsonSerialization.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%
 \brief     Declaration of JsonSerializer, an ISerializer implementation backed by
            nlohmann::json. Provides stack-based navigation of nested objects/arrays and
            strongly-typed reads for integers, floats, and strings.

 \details   This serializer owns a root JSON object and maintains an internal stack of
            pointers to the current object/array node. Callers can:
              - Open files and check stream health (Open/IsGood)
              - Navigate into objects (EnterObject/ExitObject) and arrays
                (EnterArray/ExitArray/ArraySize/EnterIndex)
              - Query for keys (HasKey) and read typed values (ReadInt/ReadFloat/ReadString)

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once

#include "Serialization.h"
#include <fstream>
#include <stack>

#pragma warning(push)            // Save current warning state
#pragma warning(disable : 26819) // Disable 'unannotated fallthrough' from third-party header
#include "../ThirdParty/json_dep/json.hpp"
#pragma warning(pop)

namespace Framework
{
    /*****************************************************************************************
      \typedef json
      \brief Alias for nlohmann::json to reduce verbosity within the serializer.
    ******************************************************************************************/
    using json = nlohmann::json;

    /*****************************************************************************************
      \class  JsonSerializer
      \brief  Concrete ISerializer that reads from JSON files and supports nested traversal.

      The class uses an internal stack of `json*` to keep track of the "current" node while
      walking nested objects/arrays. All navigation functions push/pop that stack safely,
      and the typed read functions query the top node for the requested keys.
    ******************************************************************************************/
    class JsonSerializer : public ISerializer
    {
    public:
        /*************************************************************************************
          \brief  Opens a JSON file into memory and initializes traversal state.
          \param  file  Path to the JSON file.
          \return True if the file was loaded and parsed successfully; false otherwise.
        **************************************************************************************/
        bool Open(const std::string& file) override;

        /*************************************************************************************
          \brief  Reports whether the serializer currently holds a parsed, navigable JSON.
          \return True if ready for use; false otherwise.
        **************************************************************************************/
        bool IsGood() override;

        /*************************************************************************************
          \brief  Enters a nested object by key (pushes it onto the traversal stack).
          \param  key  Property name in the current JSON object.
          \return True if the key existed and was an object; false otherwise.
        **************************************************************************************/
        bool EnterObject(const std::string& key) override;

        /*************************************************************************************
          \brief  Exits the current object/array (pops one level from the traversal stack).
        **************************************************************************************/
        void ExitObject() override;

        /*************************************************************************************
          \brief  Checks whether the current object has a given key.
          \param  key  Property name to test.
          \return True if the key exists at the current level; false otherwise.
        **************************************************************************************/
        bool HasKey(const std::string& key) const override;

        /*************************************************************************************
          \brief  Reads an integer from the current object into \a out if present.
        **************************************************************************************/
        void ReadInt(const std::string& key, int& out) override;

        /*************************************************************************************
          \brief  Reads a float from the current object into \a out if present.
        **************************************************************************************/
        void ReadFloat(const std::string& key, float& out) override;

        /*************************************************************************************
          \brief  Reads a string from the current object into \a out if present.
        **************************************************************************************/
        void ReadString(const std::string& key, std::string& out) override;

        /*************************************************************************************
          \name   Array Navigation Helpers
          \brief  Utilities for entering arrays and iterating by index.
        **************************************************************************************/
        ///@{
        bool   EnterArray(const std::string& key) override; ///< Push array by key if valid
        void   ExitArray() override;                         ///< Pop current node if nested
        size_t ArraySize() const override;                   ///< Size if current is array
        bool   EnterIndex(size_t i) override;                ///< Push element i if valid
        ///@}

    private:
        json              root{};          ///< Owning root JSON document
        std::stack<json*> objectStack{};   ///< Traversal stack; top is current node
    };
} // namespace Framework