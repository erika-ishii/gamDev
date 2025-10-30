/*********************************************************************************************
 \file      Serialization.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Primary Author, 100%

 \brief     Declares the generic serialization interface (ISerializer) used by components,
            factory, and level loading to read JSON-like hierarchical data. Also provides
            StreamRead helpers to read primitives and delegate object deserialization to
            their Serialize() members.

 \details   The interface supports object/array navigation, key checks, and primitive reads.
            Concrete implementations (e.g., JsonSerializer) must implement all virtual
            methods to integrate with the engine’s data-driven pipeline.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/
#pragma once
#include <string>

namespace Framework
{
    /*****************************************************************************************
      \class ISerializer
      \brief Abstract interface for hierarchical, key/value-based deserialization.

      Responsibilities:
      - Open a data source and report validity.
      - Navigate nested objects/arrays (EnterObject/ExitObject, EnterArray/ExitArray).
      - Probe for keys (HasKey) and read primitive types by key.
      - Iterate arrays using ArraySize and EnterIndex.

    
    *****************************************************************************************/
    class ISerializer
    {
    public:
        /*************************************************************************************
          \brief Opens a data source (e.g., file) for reading.
          \param file Path to the source file.
          \return true if open succeeded and data is readable; false otherwise.
        *************************************************************************************/
        virtual bool Open(const std::string& file) = 0;

        /*************************************************************************************
          \brief Indicates whether the serializer is in a good/valid state.
          \return true if ready for reads and navigation; false otherwise.
        *************************************************************************************/
        virtual bool IsGood() = 0;

        // Hierarchy navigation
        /*************************************************************************************
          \brief Enters a nested object by key.
          \param key Name of the child object.
          \return true if the key exists and the current scope moved into that object.
        *************************************************************************************/
        virtual bool EnterObject(const std::string& key) = 0;  // Move into object by key

        /*************************************************************************************
          \brief Exits the current object scope and returns to the parent.
        *************************************************************************************/
        virtual void ExitObject() = 0;                         // Move back out

        // Check if key exists
        /*************************************************************************************
          \brief Checks whether a key exists in the current object scope.
          \param key Key to query.
          \return true if the key is present, false otherwise.
        *************************************************************************************/
        virtual bool HasKey(const std::string& key) const = 0;

        // Read primitives by key
        /*************************************************************************************
          \brief Reads an integer value by key.
          \param key Key to read.
          \param out Reference to store the result.
        *************************************************************************************/
        virtual void ReadInt(const std::string& key, int& out) = 0;

        /*************************************************************************************
          \brief Reads a float value by key.
          \param key Key to read.
          \param out Reference to store the result.
        *************************************************************************************/
        virtual void ReadFloat(const std::string& key, float& out) = 0;

        /*************************************************************************************
          \brief Reads a string value by key.
          \param key Key to read.
          \param out Reference to store the result.
        *************************************************************************************/
        virtual void ReadString(const std::string& key, std::string& out) = 0;

        /*************************************************************************************
          \brief Enters an array by key.
          \param key Array key to enter.
          \return true if the key exists and is an array, false otherwise.
        *************************************************************************************/
        virtual bool EnterArray(const std::string& key) = 0;

        /*************************************************************************************
          \brief Exits the current array scope.
        *************************************************************************************/
        virtual void ExitArray() = 0;

        /*************************************************************************************
          \brief Retrieves the number of elements in the current array.
          \return Size of the array (0 if not in an array).
        *************************************************************************************/
        virtual size_t ArraySize() const = 0;

        /*************************************************************************************
          \brief Enters the array element at index i.
          \param i Zero-based index of the element to enter.
          \return true if index is valid and the scope moved into that element.
        *************************************************************************************/
        virtual bool EnterIndex(size_t i) = 0;
    };

    /*****************************************************************************************
      \brief Delegates deserialization to the object’s Serialize(ISerializer&) member.
      \tparam type         The type that exposes Serialize(ISerializer&).
      \param stream        The serializer to read from.
      \param typeInstance  The instance that will read its fields from the stream.
    *****************************************************************************************/
    template<typename type>
    inline void StreamRead(ISerializer& stream, type& typeInstance)
    {
        typeInstance.Serialize(stream);
    }

    /*****************************************************************************************
      \brief Helper to read an int by key via the serializer.
      \param stream Serializer to read from.
      \param key    Key name.
      \param out    Destination reference.
    *****************************************************************************************/
    inline void StreamRead(ISerializer& stream, const std::string& key, int& out) {
        stream.ReadInt(key, out);
    }

    /*****************************************************************************************
      \brief Helper to read a float by key via the serializer.
      \param stream Serializer to read from.
      \param key    Key name.
      \param out    Destination reference.
    *****************************************************************************************/
    inline void StreamRead(ISerializer& stream, const std::string& key, float& out) {
        stream.ReadFloat(key, out);
    }

    /*****************************************************************************************
      \brief Helper to read a string by key via the serializer.
      \param stream Serializer to read from.
      \param key    Key name.
      \param out    Destination reference.
    *****************************************************************************************/
    inline void StreamRead(ISerializer& stream, const std::string& key, std::string& out) {
        stream.ReadString(key, out);
    }

}
