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

if(IOS)
    set(CALM_APPLE_TEAM_ID "" CACHE STRING "Apple developer team ID used for signing")
    set(CALM_BUNDLE_PREFIX "com.thejoshgriffith.calm" CACHE STRING "Bundle identifier prefix for Calm apps")
    set(CALM_BUILD_NUMBER "1" CACHE STRING "Store build number (CFBundleVersion)")

    set(minesweeper_ios_resources
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/PrivacyInfo.xcprivacy
    )
    target_sources(CalmMinesweeper PRIVATE ${minesweeper_ios_resources})

    # Qt's default iOS media backend is FFmpeg, whose libraries Qt doesn't ship
    # for static iOS builds. The native AVFoundation backend covers our sounds.
    qt_import_plugins(CalmMinesweeper INCLUDE_BY_TYPE multimedia Qt6::QDarwinMediaPlugin)

    set_target_properties(CalmMinesweeper PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in
        QT_IOS_LAUNCH_SCREEN ${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard
        RESOURCE "${minesweeper_ios_resources}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${CALM_BUNDLE_PREFIX}.minesweeper"
        XCODE_ATTRIBUTE_MARKETING_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${CALM_BUILD_NUMBER}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${CALM_APPLE_TEAM_ID}"
    )
endif()
