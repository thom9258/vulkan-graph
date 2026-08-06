cmake_minimum_required(VERSION 3.2)

project(game VERSION 0.1.0)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -ggdb -pg")
set(CMAKE_CXX_STANDARD 26)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)
find_package(cereal REQUIRED)

include(FetchContent)
message(STATUS "Fetching Simple Geometry")
FetchContent_Declare(SimpleGeometry
    GIT_REPOSITORY git@github.com:thom9258/SimpleGeometry.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SimpleGeometry)

FetchContent_Declare(imgui_external
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.8-docking
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(imgui_external)

add_library(imgui
  ${imgui_external_SOURCE_DIR}/imgui.cpp
  ${imgui_external_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_external_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_external_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_external_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_external_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui
PUBLIC
   ${imgui_external_SOURCE_DIR}
   ${SDL2_INCLUDE_DIRS}
)

FetchContent_Declare(imguizmo_external
  GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
  GIT_TAG 5ab7676
  #GIT_TAG master
  #GIT_TAG 1.83
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)

# Populate means pull but dont use the official CMakeLists.txt in ImGuizmo
FetchContent_Populate(imguizmo_external)

add_library(imguizmo
  ${imguizmo_external_SOURCE_DIR}/src/ImGuizmo.cpp
)

target_include_directories(imguizmo
PUBLIC
   ${imguizmo_external_SOURCE_DIR} 
)

target_link_libraries(imguizmo PUBLIC imgui) 

set(PROGRAM_SOURCES
  ../../alex/log.cpp
  ../../alex/arena.cpp
  ../../alex/core.cpp
  ../../alex/find_memory_type.cpp
  ../../alex/memory_buffer.cpp
  ../../alex/presentation_context.cpp
  ../../alex/texture.cpp
  ../../alex/texture_storage.cpp
  ../../alex/geometrypass_builder.cpp
  ../../alex/overlaypass_builder.cpp
  ../../alex/pipeline_builder.cpp
  ../../alex/uniform_descriptorsets.cpp
  
  
  # Project specific
  thirdparty/stb_image.cpp
  imgui_context.cpp
  static_resources.cpp
  resource_loader.cpp
  bitmap.cpp
  glm_transform_hierarchy.cpp
)

add_executable(${PROJECT_NAME} ${PROGRAM_SOURCES} main.cpp)

target_include_directories(${PROJECT_NAME}
PRIVATE 
    ${imgui_external_SOURCE_DIR}
    ${imguizmo_external_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
	../../
	../
)

target_link_libraries(${PROJECT_NAME} 
  PRIVATE
  ${Vulkan_LIBRARIES}
  ${SDL2_LIBRARIES}
  SimpleGeometry
  glm::glm
  imgui
  imguizmo
  assimp::assimp
  cereal::cereal
)
