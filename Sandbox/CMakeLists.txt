cmake_minimum_required(VERSION 3.20)

# Create the game executable
add_executable(BloodyGoodCurry)

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

# Attach files to BloodyGoodCurry
target_sources(BloodyGoodCurry PRIVATE ${MYGAME_SRC} ${MYGAME_HDR})

# Link against the engine
target_link_libraries(BloodyGoodCurry PRIVATE MyEngine)


# ------------------------------
# FMOD setup
# ------------------------------
set(FMOD_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Engine/ThirdParty/fmod/inc)
set(FMOD_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/Engine/ThirdParty/fmod/lib/x64)

# Include FMOD headers
target_include_directories(BloodyGoodCurry PRIVATE ${FMOD_INCLUDE_DIR})

# Link FMOD libraries
target_link_directories(BloodyGoodCurry PRIVATE ${FMOD_LIBRARY_DIR})
target_link_libraries(BloodyGoodCurry PRIVATE fmod_vc)

# Includes for headers
target_include_directories(BloodyGoodCurry PRIVATE ${CMAKE_CURRENT_LIST_DIR}/MyGame)

# C++ standard & warnings
set_property(TARGET BloodyGoodCurry PROPERTY CXX_STANDARD 20)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(BloodyGoodCurry PRIVATE -Wall -Wextra)
elseif (MSVC)
  target_compile_options(BloodyGoodCurry PRIVATE /W4 /WX-)
endif()

# Copy FMOD DLL files to build directory
set(FMOD_DLL_DIR "${CMAKE_SOURCE_DIR}/Engine/ThirdParty/fmod/lib/x64")
set(FMOD_DLL_FILE "${FMOD_DLL_DIR}/fmod.dll")

# Copy FMOD DLL after build
add_custom_command(TARGET BloodyGoodCurry POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${FMOD_DLL_FILE}"
    "$<TARGET_FILE_DIR:BloodyGoodCurry>/"
    COMMENT "Copying FMOD DLL to build directory"
    VERBATIM
)

# Copy runtime folders next to exe
add_custom_command(TARGET BloodyGoodCurry POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:BloodyGoodCurry>/assets"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:BloodyGoodCurry>/Data_Files"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:BloodyGoodCurry>/logs"

    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BloodyGoodCurry>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BloodyGoodCurry>/Data_Files"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BloodyGoodCurry>/logs"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:BloodyGoodCurry>/assets"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Data_Files"
            "$<TARGET_FILE_DIR:BloodyGoodCurry>/Data_Files"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/logs"
            "$<TARGET_FILE_DIR:BloodyGoodCurry>/logs"

    COMMENT "Reset & Copy assets, Data_Files, logs"
    VERBATIM
)

# Nice grouping in IDEs
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/MyGame FILES ${MYGAME_SRC} ${MYGAME_HDR})
