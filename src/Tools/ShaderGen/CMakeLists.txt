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

add_executable(VulkanPhysicalDeviceProfileCollector
    VulkanPhysicalDeviceProfileCollector.cpp)

target_link_libraries(VulkanPhysicalDeviceProfileCollector
    PRIVATE ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)

CreateProject(VulkanPhysicalDeviceProfileCollector)

####################################################################################################

add_executable(MaterialResourceLayoutRegressionGate
    MaterialResourceLayoutRegressionGate.cpp)

target_link_libraries(MaterialResourceLayoutRegressionGate
    PRIVATE ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(MaterialResourceLayoutRegressionGate)

if(BUILD_TESTING)
    add_test(
        NAME material_resource_layout_regression_gate
        COMMAND MaterialResourceLayoutRegressionGate)
endif()

####################################################################################################


# 需要时可直接运行：
#   cmake --build <build_dir> --target MaterialPresetExhaustiveCompile
#   cmake --build <build_dir> --target MaterialResourceLayoutRegressionGate
# 在生成目录执行：
#   .\\test\\Debug\\MaterialPresetExhaustiveCompile.exe
