cmake_minimum_required(VERSION 3.28)

project(CMLiveAudio)

include(path_config.cmake)
CMLiveAudioSetup(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add examples (optional, can be disabled)
option(BUILD_EXAMPLES "Build CMLiveAudio examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/examples)
endif()
