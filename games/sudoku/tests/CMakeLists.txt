add_executable(sudoku_engine_tests engine_tests.cpp)
target_link_libraries(sudoku_engine_tests PRIVATE Sudoku::Engine)
add_test(NAME sudoku_engine_tests COMMAND sudoku_engine_tests)

add_executable(sudoku_share_tests share_tests.cpp)
target_link_libraries(sudoku_share_tests PRIVATE Sudoku::Engine)
add_test(NAME sudoku_share_tests COMMAND sudoku_share_tests)

if(CALM_BUILD_APPS)
    add_executable(sudoku_game_tests sudoku_game_tests.cpp ../app/SudokuGame.h ../app/SudokuGame.cpp)
    target_include_directories(sudoku_game_tests PRIVATE ../app)
    target_link_libraries(sudoku_game_tests PRIVATE Sudoku::Engine calm_framework Qt6::Concurrent Qt6::Test)
    add_test(NAME sudoku_game_tests COMMAND sudoku_game_tests)
    set_tests_properties(sudoku_game_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

add_executable(sudoku_variant_tests variant_tests.cpp)
target_link_libraries(sudoku_variant_tests PRIVATE Sudoku::Engine)
add_test(NAME sudoku_variant_tests COMMAND sudoku_variant_tests)
