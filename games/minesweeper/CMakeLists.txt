add_subdirectory(engine)

if(CALM_BUILD_APPS)
    add_subdirectory(app)
endif()

if(CALM_BUILD_TESTS)
    add_subdirectory(tests)
endif()
