include(FetchContent)

# ---- GLFW ----
macro(import_glfw)
  if (NOT TARGET glfw)
    # Configure GLFW options before fetching
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
      glfw
      GIT_REPOSITORY https://github.com/glfw/glfw.git
      GIT_TAG 3.3.8
    )

    # New, CMake-4.0 friendly API (does populate + add_subdirectory)
    FetchContent_MakeAvailable(glfw)
  endif()
endmacro()



# ---- GLM (math library) ----
macro(import_glm)
  if (NOT TARGET glm)
    FetchContent_Declare(
      glm
      GIT_REPOSITORY https://github.com/g-truc/glm.git
      GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
  endif()
endmacro()

# ---- stb_image (image loader) ----
macro(import_stb_image)
  if (NOT TARGET stb_image)
    FetchContent_Declare(
      stb
      GIT_REPOSITORY https://github.com/nothings/stb.git
      GIT_TAG master
    )
    FetchContent_MakeAvailable(stb)

    # Generate a tiny .cpp that defines STB_IMAGE_IMPLEMENTATION
    file(WRITE ${stb_BINARY_DIR}/stb_image_impl.cpp
"// Auto-generated at configure time
#define STB_IMAGE_IMPLEMENTATION
#include \"${stb_SOURCE_DIR}/stb_image.h\"
")

    add_library(stb_image STATIC ${stb_BINARY_DIR}/stb_image_impl.cpp)
    target_include_directories(stb_image PUBLIC ${stb_SOURCE_DIR})
    if (MSVC)
      target_compile_options(stb_image PRIVATE /W0)
    else()
      target_compile_options(stb_image PRIVATE -w)
    endif()
  endif()
endmacro()

# ---- ImGui ----
macro(import_imgui)
  if (NOT TARGET imgui)
    include(FetchContent)
    FetchContent_Declare(
      imgui
      GIT_REPOSITORY https://github.com/ocornut/imgui.git
      GIT_TAG docking
    )
    FetchContent_MakeAvailable(imgui)

    set(IMGUI_SOURCES
      ${imgui_SOURCE_DIR}/imgui.cpp
      ${imgui_SOURCE_DIR}/imgui_demo.cpp
      ${imgui_SOURCE_DIR}/imgui_draw.cpp
      ${imgui_SOURCE_DIR}/imgui_tables.cpp
      ${imgui_SOURCE_DIR}/imgui_widgets.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
      ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PUBLIC
      ${imgui_SOURCE_DIR}
      ${imgui_SOURCE_DIR}/backends
    )
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    target_link_libraries(imgui PUBLIC glfw)
    set_property(TARGET imgui PROPERTY CXX_STANDARD 11)
  endif()
endmacro()

# ---- FreeType (font rendering) ----
macro(import_freetype)
  if (NOT TARGET freetype)
    FetchContent_Declare(
      freetype
      GIT_REPOSITORY https://github.com/freetype/freetype.git
      GIT_TAG VER-2-13-2  # latest stable tag as of now
    )
    FetchContent_MakeAvailable(freetype)

    # FreeType exports a CMake target called 'freetype'
    # You just need to link it later: target_link_libraries(your_app PRIVATE freetype)
  endif()
endmacro()


# ---- Bundle entrypoint ----
macro(importDependencies)
  import_glfw()
  import_glm()
  import_stb_image()
  import_imgui()
  import_freetype()
endmacro()
