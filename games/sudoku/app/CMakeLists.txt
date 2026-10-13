qt_add_executable(CalmSudoku main.cpp)

set_source_files_properties(qml/CellColours.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

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
        qml/CellColours.qml
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
)
# Single-threaded Qt (WebAssembly) generates puzzles on the event loop instead.
if(NOT DEFINED QT_FEATURE_thread OR QT_FEATURE_thread)
    target_link_libraries(CalmSudoku PRIVATE Qt6::Concurrent)
endif()

if(EMSCRIPTEN)
    # The web page (games/sudoku/app/web/, see docs/WEB.md) passes environment
    # variables such as CALM_SMOKE_TEST to the app, which qtloader.js needs ENV for.
    set_target_properties(CalmSudoku PROPERTIES QT_WASM_EXTRA_EXPORTED_METHODS "ENV")
endif()

if(CALM_BUILD_TESTS)
    add_test(NAME sudoku_app_smoke COMMAND CalmSudoku)
    set_tests_properties(sudoku_app_smoke PROPERTIES
        ENVIRONMENT "CALM_SMOKE_TEST=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()

if(IOS)
    set(CALM_APPLE_TEAM_ID "" CACHE STRING "Apple developer team ID used for signing")
    set(CALM_BUNDLE_PREFIX "com.thejoshgriffith.calm" CACHE STRING "Bundle identifier prefix for Calm apps")
    set(CALM_BUILD_NUMBER "1" CACHE STRING "Store build number (CFBundleVersion)")

    set(sudoku_ios_resources
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/PrivacyInfo.xcprivacy
    )
    target_sources(CalmSudoku PRIVATE ${sudoku_ios_resources})

    # Qt's default iOS media backend is FFmpeg, whose libraries Qt doesn't ship
    # for static iOS builds. The native AVFoundation backend covers our sounds.
    qt_import_plugins(CalmSudoku INCLUDE_BY_TYPE multimedia Qt6::QDarwinMediaPlugin)

    set_target_properties(CalmSudoku PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in
        QT_IOS_LAUNCH_SCREEN ${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard
        RESOURCE "${sudoku_ios_resources}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${CALM_BUNDLE_PREFIX}.sudoku"
        XCODE_ATTRIBUTE_MARKETING_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${CALM_BUILD_NUMBER}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${CALM_APPLE_TEAM_ID}"
    )
endif()
