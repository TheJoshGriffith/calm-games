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
)
# Single-threaded Qt (WebAssembly) deals and checks moves on the event loop instead.
if(NOT DEFINED QT_FEATURE_thread OR QT_FEATURE_thread)
    target_link_libraries(CalmSolitaire PRIVATE Qt6::Concurrent)
endif()

if(CALM_BUILD_TESTS)
    # One run per game, so every table layout is checked for QML warnings.
    set(levels "Klondike" "Klondike · Draw three" "FreeCell" "Spider · Two suits" "Yukon" "Pyramid" "TriPeaks" "Golf")
    set(names klondike klondike3 freecell spider yukon pyramid tripeaks golf)
    foreach(level name IN ZIP_LISTS levels names)
        add_test(NAME solitaire_app_smoke_${name} COMMAND CalmSolitaire --play "${level}")
        set_tests_properties(solitaire_app_smoke_${name} PROPERTIES
            ENVIRONMENT "CALM_SMOKE_TEST=1;QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
    endforeach()
endif()

if(IOS)
    set(CALM_APPLE_TEAM_ID "" CACHE STRING "Apple developer team ID used for signing")
    set(CALM_BUNDLE_PREFIX "com.thejoshgriffith.calm" CACHE STRING "Bundle identifier prefix for Calm apps")
    set(CALM_BUILD_NUMBER "1" CACHE STRING "Store build number (CFBundleVersion)")

    set(solitaire_ios_resources
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/Assets.xcassets
        ${CMAKE_CURRENT_SOURCE_DIR}/ios/PrivacyInfo.xcprivacy
    )
    target_sources(CalmSolitaire PRIVATE ${solitaire_ios_resources})

    # Qt's default iOS media backend is FFmpeg, whose libraries Qt doesn't ship
    # for static iOS builds. The native AVFoundation backend covers our sounds.
    qt_import_plugins(CalmSolitaire INCLUDE_BY_TYPE multimedia Qt6::QDarwinMediaPlugin)

    set_target_properties(CalmSolitaire PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in
        QT_IOS_LAUNCH_SCREEN ${CMAKE_CURRENT_SOURCE_DIR}/ios/LaunchScreen.storyboard
        RESOURCE "${solitaire_ios_resources}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${CALM_BUNDLE_PREFIX}.solitaire"
        XCODE_ATTRIBUTE_MARKETING_VERSION "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${CALM_BUILD_NUMBER}"
        XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.0"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${CALM_APPLE_TEAM_ID}"
    )
endif()
