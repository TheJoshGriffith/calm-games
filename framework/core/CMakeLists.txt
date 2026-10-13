# Game-agnostic C++ building blocks shared by every Calm game.
add_library(calm_core INTERFACE)
add_library(Calm::Core ALIAS calm_core)
target_include_directories(calm_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(calm_core INTERFACE cxx_std_20)
