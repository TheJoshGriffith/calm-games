# The Calm QML module: app shell, theme, sounds, haptics, settings, saving,
# statistics and the GameSession base class every game derives from.
qt_add_library(calm_framework STATIC)

# Must be set before qt_add_qml_module reads the files.
set_source_files_properties(qml/Theme.qml qml/Icons.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(calm_framework
    URI Calm
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Calm
    SOURCES
        src/GameSession.h src/GameSession.cpp
        src/SoundBank.h src/SoundBank.cpp
        src/Haptics.h src/Haptics.cpp
        src/Settings.h src/Settings.cpp
        src/SaveStore.h src/SaveStore.cpp
        src/Stats.h src/Stats.cpp
    QML_FILES
        qml/Theme.qml
        qml/Icons.qml
        qml/AppWindow.qml
        qml/CalmPage.qml
        qml/Icon.qml
        qml/IconButton.qml
        qml/ActionButton.qml
        qml/CalmText.qml
        qml/SegmentedControl.qml
        qml/CardButton.qml
        qml/ToggleRow.qml
        qml/SettingsGroup.qml
        qml/SettingsPage.qml
    RESOURCES
        fonts/Nunito-Regular.ttf fonts/Nunito-SemiBold.ttf fonts/Nunito-Bold.ttf fonts/Nunito-ExtraBold.ttf
        sounds/tap_1.wav sounds/tap_2.wav sounds/tap_3.wav
        sounds/soft_1.wav sounds/soft_2.wav sounds/soft_3.wav
        sounds/erase.wav sounds/error.wav sounds/win.wav
        sounds/complete_0.wav sounds/complete_1.wav sounds/complete_2.wav sounds/complete_3.wav
        sounds/complete_4.wav sounds/complete_5.wav sounds/complete_6.wav sounds/complete_7.wav
)

target_include_directories(calm_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(calm_framework PUBLIC Calm::Core Qt6::Quick Qt6::QuickControls2 Qt6::Multimedia)

# Platform hooks (audio session, haptics).
if(IOS)
    target_sources(calm_framework PRIVATE src/platform/Platform.h src/platform/Platform_ios.mm)
    set_source_files_properties(src/platform/Platform_ios.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(calm_framework PUBLIC "-framework AVFoundation" "-framework UIKit")
else()
    target_sources(calm_framework PRIVATE src/platform/Platform.h src/platform/Platform_generic.cpp)
endif()
