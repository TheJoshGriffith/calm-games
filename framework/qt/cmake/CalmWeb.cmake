# WebAssembly only: leave out the parts of Qt that Calm games never use.
#
# Qt for WebAssembly links Qt, its plugins and their resources statically into
# one .wasm, and by default that includes every plugin of every linked module
# and every resource Qt ships with them. Each item below can be switched off
# for a measurement with -DCALM_WEB_TRIM="..." (docs/WEB.md has the numbers):
#
#   plugins       image format plugins (GIF, ICO, JPEG, SVG), the SVG icon
#                 engine and the TLS backend. Calm draws with QML items and
#                 Shapes, and loads only fonts and sounds from resources.
#   controls      Qt Quick Controls. Calm builds its controls on
#                 QtQuick.Templates, so the Controls styles are never loaded.
#   wasmfonts     the DejaVu Sans fallback fonts the wasm platform plugin
#                 embeds. Calm brings its own font (Nunito) and loads it before
#                 the first screen.
#   videoshaders  Qt Multimedia's video shaders; Calm only plays sounds.
#   curveshaders  Qt Quick's curve-renderer shaders, used only by
#                 Shape.CurveRenderer and Text.CurveRendering.

set(CALM_WEB_TRIM "plugins;controls;wasmfonts;videoshaders;curveshaders" CACHE STRING
    "Parts of Qt left out of the WebAssembly build (see framework/qt/cmake/CalmWeb.cmake)")

# Qt resources come as imported object libraries whose objects register the
# data at startup, so the linker can never drop them. Emptying the object list
# leaves them out. Matched by the generated qrc_<name>_init object's name.
function(_calm_web_drop_resources)
    set(patterns)
    if("wasmfonts" IN_LIST CALM_WEB_TRIM)
        list(APPEND patterns "/qrc_wasmfonts_init\\.cpp\\.o$")
    endif()
    if("videoshaders" IN_LIST CALM_WEB_TRIM)
        list(APPEND patterns "/qrc_qtmultimedia_shaders[a-z_]*_init\\.cpp\\.o$")
    endif()
    if("curveshaders" IN_LIST CALM_WEB_TRIM)
        list(APPEND patterns "/qrc_scenegraph_curve_shaders[a-z_]*_init\\.cpp\\.o$")
    endif()
    if(NOT patterns)
        return()
    endif()
    get_directory_property(targets DIRECTORY ${CMAKE_SOURCE_DIR} IMPORTED_TARGETS)
    get_directory_property(here IMPORTED_TARGETS)
    list(APPEND targets ${here})
    list(REMOVE_DUPLICATES targets)
    set(dropped)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if(NOT type STREQUAL "OBJECT_LIBRARY")
            continue()
        endif()
        get_target_property(configs ${target} IMPORTED_CONFIGURATIONS)
        foreach(config IN ITEMS "" ${configs})
            set(property IMPORTED_OBJECTS)
            if(config)
                set(property IMPORTED_OBJECTS_${config})
            endif()
            get_target_property(objects ${target} ${property})
            if(NOT objects)
                continue()
            endif()
            foreach(pattern IN LISTS patterns)
                if(objects MATCHES "${pattern}")
                    set_target_properties(${target} PROPERTIES ${property} "")
                    list(APPEND dropped ${target})
                    break()
                endif()
            endforeach()
        endforeach()
    endforeach()
    list(REMOVE_DUPLICATES dropped)
    message(STATUS "Calm web: leaving out Qt resources ${dropped}")
endfunction()
_calm_web_drop_resources()

# Plugins are chosen per executable (qt_import_plugins), and the games' apps
# are added after the framework, so this runs once the whole project is read.
function(_calm_web_trim_plugins)
    if(NOT "plugins" IN_LIST CALM_WEB_TRIM)
        return()
    endif()
    set(dirs ${CMAKE_SOURCE_DIR})
    set(apps)
    while(dirs)
        list(POP_FRONT dirs dir)
        get_directory_property(subdirs DIRECTORY ${dir} SUBDIRECTORIES)
        list(APPEND dirs ${subdirs})
        get_directory_property(targets DIRECTORY ${dir} BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            get_target_property(type ${target} TYPE)
            get_target_property(libs ${target} LINK_LIBRARIES)
            if(type STREQUAL "EXECUTABLE" AND "calm_framework" IN_LIST libs)
                qt_import_plugins(${target} EXCLUDE_BY_TYPE imageformats iconengines tls)
                list(APPEND apps ${target})
            endif()
        endforeach()
    endwhile()
    message(STATUS "Calm web: no image format, icon engine or TLS plugins in ${apps}")
endfunction()
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR} CALL _calm_web_trim_plugins)
