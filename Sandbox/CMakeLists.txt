cmake_minimum_required(VERSION 3.20)

# Create the game executable
add_executable(MyGame)

# Automatically collect source files from MyGame folder
file(GLOB_RECURSE MYGAME_SRC
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/MyGame/*.[cC]pp
)

file(GLOB_RECURSE MYGAME_HDR
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/MyGame/*.[hH]
  ${CMAKE_CURRENT_LIST_DIR}/MyGame/*.hpp
)

# Attach files to MyGame
target_sources(MyGame PRIVATE ${MYGAME_SRC} ${MYGAME_HDR})

# Link against the engine
target_link_libraries(MyGame PRIVATE MyEngine)


# ------------------------------
# FMOD setup
# ------------------------------
set(FMOD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Engine/ThirdParty/fmod/inc)
set(FMOD_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/Engine/ThirdParty/fmod/lib/x64)

# Include FMOD headers
target_include_directories(MyGame PRIVATE ${FMOD_INCLUDE_DIR})

# Link FMOD libraries
target_link_directories(MyGame PRIVATE ${FMOD_LIBRARY_DIR})
target_link_libraries(MyGame PRIVATE fmod_vc)

# Includes for MyGame headers
target_include_directories(MyGame PRIVATE ${CMAKE_CURRENT_LIST_DIR}/MyGame)

# C++ standard & warnings
set_property(TARGET MyGame PROPERTY CXX_STANDARD 20)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(MyGame PRIVATE -Wall -Wextra)
elseif (MSVC)
  target_compile_options(MyGame PRIVATE /W4 /WX-)
endif()

# Copy FMOD DLL files to build directory
set(FMOD_DLL_DIR "${CMAKE_SOURCE_DIR}/Engine/ThirdParty/fmod/lib/x64")
set(FMOD_DLL_FILE "${FMOD_DLL_DIR}/fmod.dll")

# Copy FMOD DLL after build
add_custom_command(TARGET MyGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DLL_FILE}"
    "$<TARGET_FILE_DIR:MyGame>/"
    COMMENT "Copying FMOD DLL to build directory"
    VERBATIM
)


# Copy ALL runtime folders next to the exe
add_custom_command(TARGET MyGame POST_BUILD
    # --- Reset destination folders to avoid copy_directory failures ---
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:MyGame>/assets"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:MyGame>/Data_Files"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:MyGame>/logs"

    # Recreate them
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MyGame>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MyGame>/Data_Files"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:MyGame>/logs"

    # --- Now copy cleanly ---
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:MyGame>/assets"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Data_Files"
            "$<TARGET_FILE_DIR:MyGame>/Data_Files"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/logs"
            "$<TARGET_FILE_DIR:MyGame>/logs"

    COMMENT "Reset & Copy assets, Data_Files, logs"
    VERBATIM
)
# Optional: Nice grouping in IDEs
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/MyGame FILES ${MYGAME_SRC} ${MYGAME_HDR})
