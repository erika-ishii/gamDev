/*********************************************************************************************
 \file      JsonSerialization.cpp
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Implements the JsonSerializer, a concrete implementation of ISerializer that
            uses nlohmann::json to read hierarchical data from JSON files. Provides object
            and array navigation, key lookup, and primitive reading support.

 \details   This serializer maintains a stack of active JSON objects/arrays to track the
            current scope. EnterObject/ExitObject and EnterArray/ExitArray push/pop the
            stack. Keys are checked for presence before access to avoid exceptions.

            Example of supported JSON structure:
            \code
            {
                "GameObject": {
                    "Transform": { "x": 100, "y": 200 },
                    "Render"   : { "sprite": "player.png", "layer": 1 },
                    "Physics"  : { "mass": 1.0, "gravity": true }
                }
            }
            \endcode

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

#include "JsonSerialization.h"
#include <stdexcept>
#include "Common/CRTDebug.h"   // <- bring in DBG_NEW

#ifdef _DEBUG
#define new DBG_NEW       // <- redefine new AFTER all includes
#endif
namespace Framework
{
    /***************************************************************************************
      \brief Opens and parses a JSON file into memory.
      \param file Path to JSON file.
      \return true if successfully opened and parsed, false otherwise.
    ***************************************************************************************/
    bool JsonSerializer::Open(const std::string& file)
    {
        std::ifstream stream(file);
        if (!stream.is_open()) return false;
        root = json::object();
        stream >> root;            // Parse the entire file into the root JSON object
        objectStack = {};          // Clear any previous state
        objectStack.push(&root);   // Push root pointer onto stack (top of stack = current scope)
        return true;
    }

    /***************************************************************************************
      \brief Checks if the serializer is in a valid state.
      \return true if there is at least one object in the stack.
    ***************************************************************************************/
    bool JsonSerializer::IsGood()
    {
        return !objectStack.empty();
    }

    /***************************************************************************************
      \brief Attempts to enter a nested JSON object by key.
      \param key Name of the child object.
      \return true if successfully entered (key exists and is an object).
    ***************************************************************************************/
    bool JsonSerializer::EnterObject(const std::string& key)
    {
        json* current = objectStack.top();
        if (current->contains(key) && (*current)[key].is_object())
        {
            objectStack.push(&(*current)[key]); // Push nested object onto stack
            return true;
        }
        return false;
    }

    /***************************************************************************************
      \brief Exits the current JSON object scope (pops from the stack).
    ***************************************************************************************/
    void JsonSerializer::ExitObject()
    {
        if (objectStack.size() > 1)
            objectStack.pop();      // Do not pop root
    }

    /***************************************************************************************
      \brief Checks if a key exists in the current JSON object.
      \param key Key to check.
      \return true if the key exists.
    ***************************************************************************************/
    bool JsonSerializer::HasKey(const std::string& key) const
    {
        json* current = objectStack.top();
        return current->contains(key);
    }

    /***************************************************************************************
      \brief Reads an integer value from the current JSON object.
      \param key Key name.
      \param out Reference to store result.
    ***************************************************************************************/
    void JsonSerializer::ReadInt(const std::string& key, int& out)
    {
        json* current = objectStack.top();
        out = (*current)[key].get<int>();
    }

    /***************************************************************************************
      \brief Reads a float value from the current JSON object.
      \param key Key name.
      \param out Reference to store result.
    ***************************************************************************************/
    void JsonSerializer::ReadFloat(const std::string& key, float& out)
    {
        json* current = objectStack.top();
        out = (*current)[key].get<float>();
    }

    /***************************************************************************************
      \brief Reads a string value from the current JSON object.
      \param key Key name.
      \param out Reference to store result.
    ***************************************************************************************/
    void JsonSerializer::ReadString(const std::string& key, std::string& out)
    {
        json* current = objectStack.top();
        out = (*current)[key].get<std::string>();
    }

    /***************************************************************************************
      \brief Attempts to enter an array field in the current JSON object.
      \param key Key name of the array.
      \return true if successfully entered (key exists and is an array).
    ***************************************************************************************/
    bool JsonSerializer::EnterArray(const std::string& key)
    {
        json* cur = objectStack.top();
        if (cur->contains(key) && (*cur)[key].is_array()) {
            objectStack.push(&(*cur)[key]); // Push array onto stack
            return true;
        }
        return false; // Key not found or not an array
    }

    /***************************************************************************************
      \brief Exits the current array scope.
    ***************************************************************************************/
    void JsonSerializer::ExitArray()
    {
        if (objectStack.size() > 1)
            objectStack.pop();
    }

    /***************************************************************************************
      \brief Retrieves the number of elements in the current array.
      \return Array size, or 0 if current scope is not an array.
    ***************************************************************************************/
    size_t JsonSerializer::ArraySize() const
    {
        json* cur = objectStack.top();
        return cur->is_array() ? cur->size() : 0;
    }

    /***************************************************************************************
      \brief Enters an array element by index.
      \param i Index to enter.
      \return true if valid index and scope successfully moved.
    ***************************************************************************************/
    bool JsonSerializer::EnterIndex(size_t i)
    {
        json* cur = objectStack.top();
        if (cur->is_array() && i < cur->size())
        {
            objectStack.push(&(*cur)[i]); // Push element at index
            return true;
        }
        return false;
    }
}
