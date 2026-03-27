cmake_minimum_required(VERSION 3.2)

project(graph VERSION 0.1.0)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -ggdb")
set(CMAKE_CXX_STANDARD 23)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

include(FetchContent)
message(STATUS "Fetching Simple Geometry")
FetchContent_Declare(SimpleGeometry
    GIT_REPOSITORY git@github.com:thom9258/SimpleGeometry.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(SimpleGeometry)

set(PROGRAM_SOURCES
  log.cpp
  arena.cpp
  core.cpp
  find_memory_type.cpp
  memory_buffer.cpp
  renderpass.cpp
  presentation_context.cpp
  texture_storage.cpp
  geometry_pipeline.cpp
  graph.cpp
)

add_executable(${PROJECT_NAME} ${PROGRAM_SOURCES} main.cpp)

target_include_directories(${PROJECT_NAME}
  PRIVATE 
    ${Vulkan_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} 
  PRIVATE
  ${Vulkan_LIBRARIES}
  ${SDL2_LIBRARIES}
  SimpleGeometry
)
