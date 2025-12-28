cmake_minimum_required(VERSION 3.16)
project(BookOrganizer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 QUIET)

# Try to find GLFW via CMake first, fallback to pkg-config
if(NOT glfw3_FOUND)
    pkg_check_modules(GLFW3 REQUIRED glfw3)
endif()

# efsw (file watcher) - use as subdirectory
set(EFSW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/efsw)
if(EXISTS ${EFSW_DIR}/CMakeLists.txt)
    add_subdirectory(${EFSW_DIR} EXCLUDE_FROM_ALL)
    set(EFSW_AVAILABLE TRUE)
else()
    set(EFSW_AVAILABLE FALSE)
    message(WARNING "efsw not found, file watching will be disabled")
endif()

# Dear ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Application sources
set(APP_SOURCES
    src/main.cpp
    src/BookOrganizer.cpp
    src/FileWatcher.cpp
    src/EfswFileWatcher.cpp
    src/FallbackFileWatcher.cpp
    src/BookMetadata.cpp
)

# Test sources
set(TEST_SOURCES
    test_window.cpp
)

# efsw test sources
set(EFSW_TEST_SOURCES
    test_efsw.cpp
)

add_executable(${PROJECT_NAME} ${APP_SOURCES} ${IMGUI_SOURCES})
add_executable(TestWindow ${TEST_SOURCES} ${IMGUI_SOURCES})

if(EFSW_AVAILABLE)
    add_executable(TestEfsw ${EFSW_TEST_SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENGL_INCLUDE_DIRS}
)

if(EFSW_AVAILABLE)
    target_include_directories(${PROJECT_NAME} PRIVATE ${EFSW_DIR}/include)
    target_compile_definitions(${PROJECT_NAME} PRIVATE EFSW_AVAILABLE=1)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE EFSW_AVAILABLE=0)
endif()

target_include_directories(TestWindow PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OPENGL_INCLUDE_DIRS}
)

if(EFSW_AVAILABLE)
    target_include_directories(TestEfsw PRIVATE
        ${EFSW_DIR}/include
    )
    target_compile_definitions(TestEfsw PRIVATE EFSW_AVAILABLE=1)
endif()

# No need for manual platform libraries - efsw handles this

# Link libraries based on what we found
if(glfw3_FOUND)
    target_link_libraries(${PROJECT_NAME}
        glfw
        ${OPENGL_LIBRARIES}
        pthread
    )
    target_link_libraries(TestWindow
        glfw
        ${OPENGL_LIBRARIES}
        pthread
    )
    if(EFSW_AVAILABLE)
        target_link_libraries(${PROJECT_NAME} efsw)
        target_link_libraries(TestEfsw efsw pthread)
    endif()
else()
    target_link_libraries(${PROJECT_NAME}
        ${GLFW3_LIBRARIES}
        ${OPENGL_LIBRARIES}
        pthread
    )
    target_link_libraries(TestWindow
        ${GLFW3_LIBRARIES}
        ${OPENGL_LIBRARIES}
        pthread
    )
    if(EFSW_AVAILABLE)
        target_link_libraries(${PROJECT_NAME} efsw)
        target_link_libraries(TestEfsw efsw pthread)
    endif()
    target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW3_INCLUDE_DIRS})
    target_include_directories(TestWindow PRIVATE ${GLFW3_INCLUDE_DIRS})
    target_compile_options(${PROJECT_NAME} PRIVATE ${GLFW3_CFLAGS_OTHER})
    target_compile_options(TestWindow PRIVATE ${GLFW3_CFLAGS_OTHER})
endif()

# Create directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/external)
