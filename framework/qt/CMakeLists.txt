# The Calm QML module: app shell, theme, sounds, haptics, settings, saving,
# statistics and the GameSession base class every game derives from.
qt_add_library(calm_framework STATIC)

# Sound themes from tools/make_sounds.py: sounds/<theme>/<file>.wav. The themes
# themselves are listed in SoundBank.cpp.
set(calm_sounds)
foreach(theme water chimes felt kalimba driftwood)
    foreach(file tap_1 tap_2 tap_3 soft_1 soft_2 soft_3 erase error win
                 complete_0 complete_1 complete_2 complete_3 complete_4 complete_5 complete_6 complete_7)
        list(APPEND calm_sounds sounds/${theme}/${file}.wav)
    endforeach()
endforeach()

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
        src/Sharing.h src/Sharing.cpp
        src/SystemBars.h src/SystemBars.cpp
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
        qml/StatsPage.qml
        qml/BottomSheet.qml
        qml/CalmTextField.qml
        qml/Toast.qml
    RESOURCES
        fonts/Nunito-Regular.ttf fonts/Nunito-SemiBold.ttf fonts/Nunito-Bold.ttf fonts/Nunito-ExtraBold.ttf
        ${calm_sounds}
)

target_include_directories(calm_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(calm_framework PUBLIC Calm::Core Qt6::Quick Qt6::Multimedia)

# The web build leaves out what Calm doesn't use (cmake/CalmWeb.cmake).
if(EMSCRIPTEN)
    include(cmake/CalmWeb.cmake)
endif()
if(NOT EMSCRIPTEN OR NOT "controls" IN_LIST CALM_WEB_TRIM)
    target_link_libraries(calm_framework PUBLIC Qt6::QuickControls2)
endif()

# With a static Qt (iOS), qmlimportscanner only scans each app's own QML, so the
# plugins behind the framework's imports would be left out and Main.qml fails
# to load ("module QtQuick.Shapes is not installed"). Link them here instead;
# each plugin target brings its own initializer.
if(NOT QT6_IS_SHARED_LIBS_BUILD)
    find_package(Qt6 REQUIRED COMPONENTS QuickShapesPrivate QuickTemplates2)
    foreach(plugin qtquick2plugin qtquicktemplates2plugin qmlshapesplugin)
        if(NOT TARGET Qt6::${plugin})
            message(FATAL_ERROR "Static Qt has no QML plugin target Qt6::${plugin}")
        endif()
        target_link_libraries(calm_framework PUBLIC Qt6::${plugin})
    endforeach()
endif()

# Platform hooks (audio session, haptics, sharing).
if(IOS)
    target_sources(calm_framework PRIVATE src/platform/Platform.h src/platform/Platform_ios.mm)
    set_source_files_properties(src/platform/Platform_ios.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(calm_framework PUBLIC "-framework AVFoundation" "-framework UIKit")
else()
    target_sources(calm_framework PRIVATE src/platform/Platform.h src/platform/Platform_generic.cpp)
endif()
