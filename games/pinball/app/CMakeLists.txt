qt_add_executable(CalmPinball main.cpp)

qt_add_qml_module(CalmPinball
    URI Pinball
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        PinballGame.h PinballGame.cpp
    QML_FILES
        qml/Main.qml
        qml/HomePage.qml
        qml/GamePage.qml
        qml/PinballTable.qml
)

set_target_properties(CalmPinball PROPERTIES
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(CalmPinball PRIVATE
    Pinball::Engine
    calm_framework
    calm_frameworkplugin
    Qt6::Quick
)

if(CALM_BUILD_TESTS)
    # Loads the home screen, then a table in play; fails on any QML warning.
    add_test(NAME pinball_app_smoke COMMAND CalmPinball)
    add_test(NAME pinball_app_smoke_play COMMAND CalmPinball --play Classic)
    set_tests_properties(pinball_app_smoke pinball_app_smoke_play PROPERTIES
        ENVIRONMENT "CALM_SMOKE_TEST=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()

if(IOS)
    set(CALM_APPLE_TEAM_ID "" CACHE STRING "Apple developer team ID used for signing")
    set(CALM_BUNDLE_PREFIX "com.thejoshgriffith.calm" CACHE STRING "Bundle identifier prefix for Calm apps")
    set(CALM_BUILD_NUMBER "1" CACHE STRING "Store build number (CFBundleVersion)")

    set(pinball_ios_resources
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/PrivacyInfo.xcprivacy
    )
    target_sources(CalmPinball PRIVATE ${pinball_ios_resources})

    # Static iOS Qt only bundles the QML plugins the app's own QML imports are
    # scanned for. The table is drawn with QtQuick.Shapes, so link it here too
    # (the framework also links it, but don't rely on that).
    find_package(Qt6 REQUIRED COMPONENTS QuickShapesPrivate)
    target_link_libraries(CalmPinball PRIVATE Qt6::qmlshapesplugin)

    # Qt's default iOS media backend is FFmpeg, whose libraries Qt doesn't ship
    # for static iOS builds. The native AVFoundation backend covers our sounds.
    qt_import_plugins(CalmPinball INCLUDE_BY_TYPE multimedia Qt6::QDarwinMediaPlugin)

    set_target_properties(CalmPinball PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in
        QT_IOS_LAUNCH_SCREEN ${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard
        RESOURCE "${pinball_ios_resources}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${CALM_BUNDLE_PREFIX}.pinball"
        XCODE_ATTRIBUTE_MARKETING_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${CALM_BUILD_NUMBER}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${CALM_APPLE_TEAM_ID}"
    )
endif()
