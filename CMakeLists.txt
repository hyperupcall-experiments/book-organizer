cmake_minimum_required(VERSION 3.16)
project(BookOrganizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable compile commands export for better IDE/editor support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Conan dependencies
find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(opengl_system REQUIRED)
find_package(efsw REQUIRED)

# Use ImGui package folder variable to access backends
# This variable is set by the Conan-generated imgui-release-x86_64-data.cmake
set(IMGUI_BACKENDS_DIR "${imgui_PACKAGE_FOLDER_RELEASE}/res/bindings")
message(STATUS "Using ImGui backends from: ${IMGUI_BACKENDS_DIR}")

# Application sources
set(APP_SOURCES
    src/main.cpp
    src/BookOrganizer.cpp
    src/FileWatcher.cpp
    src/BookMetadata.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp
)

add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_BACKENDS_DIR}
    ${imgui_PACKAGE_FOLDER_RELEASE}/include
    ${efsw_PACKAGE_FOLDER_RELEASE}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui::imgui
    glfw
    opengl::opengl
    efsw::efsw
    pthread
    m
    rt
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()
