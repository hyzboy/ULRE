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

add_executable(ShaderResourceSchemaRegressionGate
    ShaderResourceSchemaRegressionGate.cpp)

target_link_libraries(ShaderResourceSchemaRegressionGate
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)

CreateProject(ShaderResourceSchemaRegressionGate)

target_compile_definitions(ShaderResourceSchemaRegressionGate
    PRIVATE ULRE_REPO_ROOT="${CMAKE_SOURCE_DIR}")

if(BUILD_TESTING)
    add_test(
        NAME shader_resource_schema_regression_gate
        COMMAND ShaderResourceSchemaRegressionGate)

    set_tests_properties(
        shader_resource_schema_regression_gate
        PROPERTIES LABELS "shadergen;baseline")

    foreach(regression_group
            glsl
            interface
            descriptor
            cache
            materialization
            pipeline)
        add_test(
            NAME shader_resource_schema_regression_gate.${regression_group}
            COMMAND ShaderResourceSchemaRegressionGate ${regression_group})
        set_tests_properties(
            shader_resource_schema_regression_gate.${regression_group}
            PROPERTIES LABELS "shadergen;baseline;${regression_group}")
    endforeach()
endif()

####################################################################################################


# 需要时可直接运行：
#   cmake --build <build_dir> --target MaterialPresetExhaustiveCompile
#   cmake --build <build_dir> --target ShaderResourceSchemaRegressionGate
# 在生成目录执行：
#   .\\test\\Debug\\MaterialPresetExhaustiveCompile.exe
