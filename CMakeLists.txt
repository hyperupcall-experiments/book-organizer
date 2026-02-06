cmake_minimum_required(VERSION 3.16)
project(BookOrganizer LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C++_COMPILER gcc)

find_package(imgui REQUIRED)
find_package(glfw3 REQUIRED)
find_package(opengl_system REQUIRED)
find_package(efsw REQUIRED)

# Hacky, but it works.
if (imgui_PACKAGE_FOLDER_DEBUG)
	set(IMGUI_BACKENDS_DIR ${imgui_PACKAGE_FOLDER_DEBUG}/res/bindings)
else()
	set(IMGUI_BACKENDS_DIR ${imgui_PACKAGE_FOLDER_RELEASE}/res/bindings)
endif()

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

# Installation rules
include(GNUInstallDirs)

# Install the executable
install(TARGETS ${PROJECT_NAME}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install XDG desktop entry
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/book-organizer.desktop
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)

# Install application icon (rename assets/icon.png to book-organizer.png on install)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png
		RENAME book-organizer.png
		DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
	)
endif()
