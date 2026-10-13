qt_add_executable(CalmMinesweeper main.cpp)

qt_add_qml_module(CalmMinesweeper
    URI Minesweeper
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        MinesweeperGame.h MinesweeperGame.cpp
    QML_FILES
        qml/Main.qml
        qml/HomePage.qml
        qml/GamePage.qml
        qml/Board.qml
        qml/MinesweeperSettingsPage.qml
)

set_target_properties(CalmMinesweeper PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(CalmMinesweeper PRIVATE
    Minesweeper::Engine
    calm_framework
    calm_frameworkplugin
    Qt6::Quick
)

if(CALM_BUILD_TESTS)
    add_test(NAME minesweeper_app_smoke COMMAND CalmMinesweeper)
    set_tests_properties(minesweeper_app_smoke PROPERTIES
        ENVIRONMENT "CALM_SMOKE_TEST=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    add_test(NAME minesweeper_app_smoke_play COMMAND CalmMinesweeper --play Hard --tap-centre)
    set_tests_properties(minesweeper_app_smoke_play PROPERTIES
        ENVIRONMENT "CALM_SMOKE_TEST=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
