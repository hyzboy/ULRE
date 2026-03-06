cmake_minimum_required(VERSION 3.28)

macro(CreateProject name)

    target_include_directories(${name}
        PRIVATE ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
                ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Tools/ShaderGen")

    if(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    endif()
endmacro()

####################################################################################################

add_executable(MaterialPresetExhaustiveCompile
    MaterialPresetExhaustiveCompileTest.cpp)

target_link_libraries(MaterialPresetExhaustiveCompile
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

CreateProject(MaterialPresetExhaustiveCompile)

####################################################################################################

add_executable(VulkanPhysicalDeviceProfileCollector
    VulkanPhysicalDeviceProfileCollector.cpp)

target_link_libraries(VulkanPhysicalDeviceProfileCollector
    PRIVATE ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)

CreateProject(VulkanPhysicalDeviceProfileCollector)

####################################################################################################

include(CTest)
if(BUILD_TESTING)

    add_test(NAME MaterialPresetExhaustiveCompile
             COMMAND MaterialPresetExhaustiveCompile)

    set_tests_properties(MaterialPresetExhaustiveCompile PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# 需要时可直接运行：
#   cmake --build <build_dir> --target MaterialPresetExhaustiveCompile
# 在生成目录执行：
#   .\\test\\Debug\\MaterialPresetExhaustiveCompile.exe
