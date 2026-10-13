cmake_minimum_required(VERSION 3.21)

project(CalmGames VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CALM_BUILD_TESTS "Build unit tests" ON)
option(CALM_BUILD_TOOLS "Build command-line tools" ON)

# The Qt apps build only when Qt 6 is available; the pure C++ engines and
# their tests build everywhere.
find_package(Qt6 6.4 QUIET COMPONENTS Quick QuickControls2 Multimedia Concurrent Test)
if(Qt6_FOUND)
    option(CALM_BUILD_APPS "Build the Qt apps" ON)
else()
    option(CALM_BUILD_APPS "Build the Qt apps" OFF)
endif()

if(CALM_BUILD_APPS)
    qt_standard_project_setup()
endif()

if(CALM_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(framework)

# Every folder under games/ with a CMakeLists.txt is a game. Adding a game
# needs no change here, so games can be developed in parallel.
file(GLOB CALM_GAME_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/games/*)
list(SORT CALM_GAME_DIRS)
foreach(dir IN LISTS CALM_GAME_DIRS)
    if(EXISTS ${dir}/CMakeLists.txt)
        add_subdirectory(${dir})
    endif()
endforeach()

if(CALM_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
