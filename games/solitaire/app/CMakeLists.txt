qt_add_executable(CalmSolitaire main.cpp)

qt_add_qml_module(CalmSolitaire
    URI Solitaire
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        SolitaireGame.h SolitaireGame.cpp
    QML_FILES
        qml/Main.qml
        qml/HomePage.qml
        qml/GamePage.qml
        qml/Table.qml
        qml/PlayingCard.qml
        qml/SuitIcon.qml
        qml/SolitaireSettingsPage.qml
)

set_target_properties(CalmSolitaire PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(CalmSolitaire PRIVATE
    Solitaire::Engine
    calm_framework
    calm_frameworkplugin
    Qt6::Quick
    Qt6::Concurrent
)

if(CALM_BUILD_TESTS)
    add_test(NAME solitaire_app_smoke COMMAND CalmSolitaire --play "Draw one")
    set_tests_properties(solitaire_app_smoke PROPERTIES
        ENVIRONMENT "CALM_SMOKE_TEST=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()
