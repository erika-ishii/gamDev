# Set the Engine
set(ENGINE_NAME MyEngine)

#Get the Source Files (cpp)
# Source files
file(GLOB_RECURSE ENGINE_SRC_FILES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/**/*.[cC]pp
)

# Headers
file(GLOB_RECURSE ENGINE_HEADER_FILES
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/**/*.[hH]
  ${CMAKE_CURRENT_LIST_DIR}/**/*.hpp
)

# Exclude vendored code handled separately (glad)
list(FILTER ENGINE_SRC_FILES     EXCLUDE REGEX ".*/ThirdParty/.*")
list(FILTER ENGINE_HEADER_FILES  EXCLUDE REGEX ".*/ThirdParty/.*")

set(ALL_ENGINE_FILES ${ENGINE_SRC_FILES} ${ENGINE_HEADER_FILES})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${ALL_ENGINE_FILES})

# --- Manual GLAD (vendored) ---
add_library(glad STATIC
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/glad/src/glad.c
)
target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/glad/include
)
#Get the Source Files (h)
add_library(${ENGINE_NAME}
    ${ENGINE_SRC_FILES}
    ${ENGINE_HEADER_FILES} 
)
# Add FMOD to your libraries list
set(ALL_LIBS
    PRIVATE glfw
    PRIVATE libglew_static
    PRIVATE imgui
    PRIVATE fmod_vc  # Note: Use fmod_vc.lib, not fmod.lib
)

# FMOD setup
set(FMOD_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/fmod/inc)
set(FMOD_LIBRARY_DIR ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/fmod/lib/x64)

# Include FMOD headers
target_include_directories(${ENGINE_NAME} PUBLIC ${FMOD_INCLUDE_DIR})

# Link FMOD libraries
target_link_directories(${ENGINE_NAME} PRIVATE ${FMOD_LIBRARY_DIR})
target_link_libraries(${ENGINE_NAME} PRIVATE fmod_vc)

# json_dep file added
target_include_directories(${ENGINE_NAME} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/ThirdParty/json_dep
)



set(ALL_ENGINE_FILES ${ENGINE_SRC_FILES} ${ENGINE_HEADER_FILES})
if(COMMAND GROUP_FILES_BY_FOLDER)
    GROUP_FILES_BY_FOLDER(${ALL_ENGINE_FILES})
endif()

set_property(TARGET ${ENGINE_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${ENGINE_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)


# Expose top-level engine folder so headers are visible
target_include_directories(${ENGINE_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# Make sure glfw is added with add_subdirectory() or find_package()
target_link_libraries(${ENGINE_NAME} PUBLIC glfw glad stb_image glm::glm imgui freetype)

#Complier Warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${ENGINE_NAME} PRIVATE -Wall -Wextra)
elseif (MSVC)
    target_compile_options(${ENGINE_NAME} PRIVATE /W4 /WX-)
endif()

# Platform OpenGL (required)
if (WIN32)
  target_link_libraries(${ENGINE_NAME} PRIVATE opengl32)
elseif (APPLE)
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  find_library(OpenGL_LIBRARY OpenGL REQUIRED)
  target_link_libraries(${ENGINE_NAME} PRIVATE ${OpenGL_LIBRARY} ${COCOA_LIBRARY})
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(${ENGINE_NAME} PRIVATE OpenGL::GL)
endif()