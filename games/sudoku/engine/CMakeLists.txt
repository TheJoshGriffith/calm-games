# Pure C++ Sudoku engine: no Qt dependency, so it builds and tests anywhere.
add_library(sudoku_engine STATIC
    src/Board.cpp
    src/Solver.cpp
    src/Grader.cpp
    src/Generator.cpp
    src/Share.cpp
)
add_library(Sudoku::Engine ALIAS sudoku_engine)
target_include_directories(sudoku_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(sudoku_engine PUBLIC Calm::Core)
if(MSVC)
    target_compile_options(sudoku_engine PRIVATE /W4)
else()
    target_compile_options(sudoku_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
