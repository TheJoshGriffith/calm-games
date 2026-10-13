qt_add_executable(CalmSudoku main.cpp)

qt_add_qml_module(CalmSudoku
    URI Sudoku
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        SudokuGame.h SudokuGame.cpp
    QML_FILES
        qml/Main.qml
        qml/HomePage.qml
        qml/GamePage.qml
        qml/Board.qml
        qml/NumberPad.qml
        qml/SudokuSettingsPage.qml
)

set_target_properties(CalmSudoku PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(CalmSudoku PRIVATE
    Sudoku::Engine
    calm_framework
    calm_frameworkplugin
    Qt6::Quick
    Qt6::Concurrent
)

if(CALM_BUILD_TESTS)
    add_test(NAME sudoku_app_smoke COMMAND CalmSudoku)
    set_tests_properties(sudoku_app_smoke PROPERTIES
        ENVIRONMENT "CALM_SMOKE_TEST=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
