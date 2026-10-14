# Pure C++ rules for every Solitaire variant, solver and deal generator: no Qt dependency.
add_library(solitaire_engine STATIC
    src/Game.cpp
    src/Solver.cpp
)
add_library(Solitaire::Engine ALIAS solitaire_engine)
target_include_directories(solitaire_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(solitaire_engine PUBLIC Calm::Core)
if(MSVC)
    target_compile_options(solitaire_engine PRIVATE /W4)
else()
    target_compile_options(solitaire_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
