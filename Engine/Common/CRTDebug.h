/*********************************************************************************************
 \file      CRTDebug.h
 \par       SofaSpuds
 \author    elvisshengjie.lim (elvisshengjie.lim@digipen.edu) - Author, 100%

 \brief     Enables Microsoft Visual C++ debug heap leak detection.
            - Maps heap allocations to source file and line number in Debug builds.
            - Provides DBG_NEW macro for debug tracking.
            - Must only be included in .cpp files (or PCH) to avoid breaking 3rd-party headers.
            - Do NOT include in headers used by external libraries (json, ImGui, GLFW, FMOD, etc.)

 \details   Usage:
            In a .cpp file:
                #include "Common/CRTDebug.h"
                #ifdef _DEBUG
                    #define new DBG_NEW
                #endif

            This enables leak tracking for allocations made *after* this point in the file.
            Third-party headers must be included BEFORE redefining `new`.

 \copyright
            All content © 2025 DigiPen Institute of Technology Singapore.
            All rights reserved.
*********************************************************************************************/

// Common/CRTDebug.h
#pragma once

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

// Provide DBG_NEW, but don't automatically #define new here.
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#endif

#endif // _DEBUG