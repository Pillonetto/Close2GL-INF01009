cmake_minimum_required(VERSION 3.14)

project(CMP143)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
LINK_DIRECTORIES( ${CMAKE_SOURCE_DIR}/lib )

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Second, for multi-config builds (e.g. msvc)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/bin)
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_SOURCE_DIR}/lib )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set (CMAKE_DEBUG_POSTFIX "_d")

find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# Must run before COMMON_LIBS: GLFW_LIBRARIES (Cocoa, IOKit, etc.) is set here.
# Linking the static GLFW library on macOS requires those frameworks on the final link.
option(GLFW_BUILD_EXAMPLES OFF)
option(GLFW_BUILD_TESTS OFF)
option(GLFW_BUILD_DOCS OFF)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/glfw)

if(WIN32)
set(COMMON_LIBS ${OPENGL_LIBRARIES} optimized glfw debug glfw)
elseif(APPLE)
# GLFW's CMake already appends OpenGL + Cocoa + IOKit + Core* for static lib consumers.
set(COMMON_LIBS glfw ${GLFW_LIBRARIES})
elseif(UNIX)
set(COMMON_LIBS ${OPENGL_LIBRARIES} glfw ${GLFW_LIBRARIES})
else()
set(COMMON_LIBS)
endif()
set(COMMON_LIBS ${COMMON_LIBS} ${EXTRA_LIBS})

set(RUN_DIR ${PROJECT_SOURCE_DIR}/bin)

set(IMGUI_DIR "$ENV{HOME}/imgui-1.92.6" CACHE PATH "Path to Dear ImGui root (imgui.h, backends/, etc.)")
if(NOT EXISTS "${IMGUI_DIR}/imgui.cpp")
  message(FATAL_ERROR "IMGUI_DIR does not point to ImGui (missing imgui.cpp): ${IMGUI_DIR}")
endif()

add_library(imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)
# GLFW's glfw3native.h requires a context-API macro on macOS (ImGui only sets Cocoa).
if(APPLE)
  target_compile_definitions(imgui PRIVATE GLFW_EXPOSE_NATIVE_NSGL)
endif()

add_executable(CMP143
  src/triangles/main.cpp
  src/camera_gui.cpp
  src/open_gl_matrices.cpp
  include/open_gl_matrices.hpp
  src/fps_counter.cpp
  include/fps_counter.hpp
  src/close-to-gl/close_to_gl_model.cpp
  include/close_to_gl_model.hpp
  src/close-to-gl/close_to_gl_camera.cpp
  include/close_to_gl_camera.hpp
  src/model-loader.cpp
  lib/gl3w.c
  triangles.vert
  triangles.frag
)
if(APPLE)
  target_sources(CMP143 PRIVATE src/camera_gui_mac.mm)
endif()
set_property(TARGET CMP143 PROPERTY DEBUG_POSTFIX _d)
target_include_directories(CMP143 PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/lib/glfw/include)
target_link_libraries(CMP143 PRIVATE ${COMMON_LIBS} glm::glm imgui)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_LINUX")
ENDIF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

include_directories( include )
include_directories(lib/glfw/include)
