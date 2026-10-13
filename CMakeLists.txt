cmake_minimum_required(VERSION 3.21)

project(CalmGames VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CALM_BUILD_TESTS "Build unit tests" ON)
option(CALM_BUILD_TOOLS "Build command-line tools" ON)

if(CALM_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(framework)
add_subdirectory(games/sudoku)

if(CALM_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
