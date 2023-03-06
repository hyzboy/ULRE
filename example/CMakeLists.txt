include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

SET(VULKAN_APP_FRAMEWORK ${CMAKE_CURRENT_SOURCE_DIR}/common/VulkanAppFramework.h)

add_subdirectory(Vulkan)
add_subdirectory(2dVector)
add_subdirectory(GUI)

macro(CreateProject name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example")
endmacro()

CreateProject(MaterialCreaterTest MaterialCreaterTest.cpp)
