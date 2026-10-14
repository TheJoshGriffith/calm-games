# Pure C++ Mahjong solitaire engine: no Qt dependency, so it builds and tests anywhere.
add_library(mahjong_engine STATIC
    src/Tiles.cpp
    src/Layout.cpp
    src/Board.cpp
    src/Generator.cpp
    src/Solver.cpp
)
add_library(Mahjong::Engine ALIAS mahjong_engine)
target_include_directories(mahjong_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mahjong_engine PUBLIC Calm::Core)
if(MSVC)
    target_compile_options(mahjong_engine PRIVATE /W4)
else()
    target_compile_options(mahjong_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
