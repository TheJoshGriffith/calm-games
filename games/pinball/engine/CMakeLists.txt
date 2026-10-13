# Pure C++ pinball table and physics: no Qt dependency, so it builds and tests anywhere.
add_library(pinball_engine STATIC
    src/Table.cpp
)
add_library(Pinball::Engine ALIAS pinball_engine)
target_include_directories(pinball_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pinball_engine PUBLIC Calm::Core)
if(MSVC)
    target_compile_options(pinball_engine PRIVATE /W4)
else()
    target_compile_options(pinball_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
