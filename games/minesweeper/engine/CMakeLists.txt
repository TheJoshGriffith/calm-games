# Pure C++ Minesweeper engine: no Qt dependency, so it builds and tests anywhere.
add_library(minesweeper_engine STATIC
    src/Field.cpp
    src/Solver.cpp
    src/Generator.cpp
)
add_library(Minesweeper::Engine ALIAS minesweeper_engine)
target_include_directories(minesweeper_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(minesweeper_engine PUBLIC Calm::Core)
if(MSVC)
    target_compile_options(minesweeper_engine PRIVATE /W4)
else()
    target_compile_options(minesweeper_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
