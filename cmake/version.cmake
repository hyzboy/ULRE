macro(check_system_version)

    message("Host system: " ${CMAKE_HOST_SYSTEM})
    message("Host system name: " ${CMAKE_HOST_SYSTEM_NAME})
    message("Host system version: " ${CMAKE_HOST_SYSTEM_VERSION})

    message("Compile features: " ${CMAKE_CXX_COMPILE_FEATURES})
    message("Compile Flags: " ${CMAKE_C_FLAGS})
    message("C++ Compile Flags: " ${CMAKE_CXX_FLAGS})
    message("Build type: " ${CMAKE_BUILD_TYPE})

    add_definitions(-DHGL_HOST_SYSTEM="${CMAKE_HOST_SYSTEM}")

    add_definitions(-DHGL_COMPILE_C_FLAGS="${CMAKE_C_FLAGS}")
    add_definitions(-DHGL_COMPILE_CXX_FLAGS="${CMAKE_CXX_FLAGS}")

    add_definitions(-DCMAKE_VERSION="${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION}")

    MESSAGE("C Compile Features: " ${CMAKE_C_COMPILE_FEATURES})
    MESSAGE("C++ Compile Features: " ${CMAKE_CXX_COMPILE_FEATURES})

    # add_definitions(-DHGL_COMPILE_C_FEATURES="${CMAKE_C_COMPILE_FEATURES}")
    # add_definitions(-DHGL_COMPILE_CXX_FEATURES="${CMAKE_CXX_COMPILE_FEATURES}")

endmacro()
