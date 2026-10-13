add_executable(sudoku-gen main.cpp)
target_link_libraries(sudoku-gen PRIVATE Sudoku::Engine)
