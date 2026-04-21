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

add_executable(MaterialPresetExhaustiveCompile
    MaterialPresetExhaustiveCompile.cpp)

target_link_libraries(MaterialPresetExhaustiveCompile
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(MaterialPresetExhaustiveCompile)

####################################################################################################

add_executable(MaterialFeatureWarningSmoke
    MaterialFeatureWarningSmoke.cpp)

target_link_libraries(MaterialFeatureWarningSmoke
    PRIVATE ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(MaterialFeatureWarningSmoke)

####################################################################################################

add_executable(MaterialVariantKeyEquivalenceTest
    MaterialVariantKeyEquivalenceTest.cpp)

target_link_libraries(MaterialVariantKeyEquivalenceTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(MaterialVariantKeyEquivalenceTest)

####################################################################################################

add_executable(MaterialVariantKeyCacheKeyTest
    MaterialVariantKeyCacheKeyTest.cpp)

target_link_libraries(MaterialVariantKeyCacheKeyTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(MaterialVariantKeyCacheKeyTest)

####################################################################################################

add_executable(ProgramFeatureMaskIntegrationTest
    ProgramFeatureMaskIntegrationTest.cpp)

target_link_libraries(ProgramFeatureMaskIntegrationTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(ProgramFeatureMaskIntegrationTest)

####################################################################################################

add_executable(FeatureIntentValidationTest
    FeatureIntentValidation.cpp
    FeatureIntentValidationTest.cpp)

target_link_libraries(FeatureIntentValidationTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(FeatureIntentValidationTest)

####################################################################################################

add_executable(CompositorInferenceDiagnosticsTest
    CompositorInferenceDiagnosticsTest.cpp)

target_link_libraries(CompositorInferenceDiagnosticsTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(CompositorInferenceDiagnosticsTest)

####################################################################################################

add_executable(VariantRegistryRoutingBehaviorTest
    VariantRegistryRoutingBehaviorTest.cpp)

target_link_libraries(VariantRegistryRoutingBehaviorTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(VariantRegistryRoutingBehaviorTest)

####################################################################################################

add_executable(CompositorTemplatePathGuardTest
    CompositorTemplatePathGuardTest.cpp)

target_link_libraries(CompositorTemplatePathGuardTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(CompositorTemplatePathGuardTest)

####################################################################################################

add_executable(VariantRegistryDescAgnosticRoutingTest
    VariantRegistryDescAgnosticRoutingTest.cpp)

target_link_libraries(VariantRegistryDescAgnosticRoutingTest
    PRIVATE ULRE.Vulkan
            ULRE.ShaderGen
            CMCore
            CMMath
            CMCoreType)

CreateProject(VariantRegistryDescAgnosticRoutingTest)
