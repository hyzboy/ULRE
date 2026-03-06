cmake_minimum_required(VERSION 3.28)

macro(CreateTestProject index name)
    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Test/ShaderGen")

    if(MSVC)
        set_target_properties(${name} PROPERTIES PROJECT_LABEL "${index}_${name}")
    endif()
endmacro()

add_executable(test_ComposedShaderGenerator_Verify
    test_ComposedShaderGenerator_Verify.cpp)

target_include_directories(test_ComposedShaderGenerator_Verify
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(02 test_ComposedShaderGenerator_Verify)

add_executable(test_DescriptorSetLifecycleRegression
    test_DescriptorSetLifecycleRegression.cpp)

target_link_libraries(test_DescriptorSetLifecycleRegression
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            ULRE.SceneGraph
            CMCore
            CMMath
            CMCoreType
            Vulkan::Vulkan)

target_include_directories(test_DescriptorSetLifecycleRegression
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(04 test_DescriptorSetLifecycleRegression)

add_executable(test_ShaderLogicValidation
    ShaderLogicValidationTest.cpp)

target_link_libraries(test_ShaderLogicValidation
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_ShaderLogicValidation
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(05 test_ShaderLogicValidation)

add_executable(test_ShaderGenPathMode
    ShaderGenPathModeTest.cpp)

CreateTestProject(16 test_ShaderGenPathMode)

add_executable(test_MaterialPresetExhaustiveCompile
    MaterialPresetExhaustiveCompileTest.cpp)

set_target_properties(test_MaterialPresetExhaustiveCompile PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

target_link_libraries(test_MaterialPresetExhaustiveCompile
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_MaterialPresetExhaustiveCompile
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(17 test_MaterialPresetExhaustiveCompile)

add_executable(test_RendererShaderGenAdapterStrictGate
    RendererShaderGenAdapterStrictGateTest.cpp)

target_link_libraries(test_RendererShaderGenAdapterStrictGate
    PRIVATE ULRE.SceneGraph
            ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_RendererShaderGenAdapterStrictGate
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(18 test_RendererShaderGenAdapterStrictGate)

add_executable(test_ShaderGenVertexInputAdapter
    ShaderGenVertexInputAdapterTest.cpp)

target_link_libraries(test_ShaderGenVertexInputAdapter
    PRIVATE ULRE.SceneGraph
            ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_ShaderGenVertexInputAdapter
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(19 test_ShaderGenVertexInputAdapter)

add_executable(test_ShaderGenDescriptorLayoutAdapter
    ShaderGenDescriptorLayoutAdapterTest.cpp)

target_link_libraries(test_ShaderGenDescriptorLayoutAdapter
    PRIVATE ULRE.SceneGraph
            ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_ShaderGenDescriptorLayoutAdapter
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(20 test_ShaderGenDescriptorLayoutAdapter)

add_executable(test_MaterialBuildStrictGatePolicyMatrix
    MaterialBuildStrictGatePolicyMatrixTest.cpp)

target_link_libraries(test_MaterialBuildStrictGatePolicyMatrix
    PRIVATE ULRE.SceneGraph
            ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_MaterialBuildStrictGatePolicyMatrix
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(21 test_MaterialBuildStrictGatePolicyMatrix)

add_executable(test_ShaderGenContractPathContext
    ShaderGenContractPathContextTest.cpp)

target_link_libraries(test_ShaderGenContractPathContext
    PRIVATE ULRE.SceneGraph
            ULRE.ShaderGen
            ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)

target_include_directories(test_ShaderGenContractPathContext
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(22 test_ShaderGenContractPathContext)

add_executable(test_VulkanPhysicalDeviceProfileCollector
    VulkanPhysicalDeviceProfileCollector.cpp)

target_link_libraries(test_VulkanPhysicalDeviceProfileCollector
    PRIVATE ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)

target_include_directories(test_VulkanPhysicalDeviceProfileCollector
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(23 test_VulkanPhysicalDeviceProfileCollector)

add_executable(test_ShaderGenPhysicalDeviceProfileJsonPath
    ShaderGenPhysicalDeviceProfileJsonPathTest.cpp)

target_link_libraries(test_ShaderGenPhysicalDeviceProfileJsonPath
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_ShaderGenPhysicalDeviceProfileJsonPath
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(24 test_ShaderGenPhysicalDeviceProfileJsonPath)

add_executable(test_ShaderGenContractValidatorProfile
    ShaderGenContractValidatorProfileTest.cpp)

target_link_libraries(test_ShaderGenContractValidatorProfile
    PRIVATE ULRE.ShaderGen
            CMCore)

target_include_directories(test_ShaderGenContractValidatorProfile
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(25 test_ShaderGenContractValidatorProfile)

add_executable(test_RendererShaderGenAdapterProfileCategory
    RendererShaderGenAdapterProfileCategoryTest.cpp)

target_link_libraries(test_RendererShaderGenAdapterProfileCategory
    PRIVATE ULRE.SceneGraph
            ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_RendererShaderGenAdapterProfileCategory
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

CreateTestProject(26 test_RendererShaderGenAdapterProfileCategory)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(SHADERGEN_PARITY_PYTHON "${Python3_EXECUTABLE}")
else()
    set(SHADERGEN_PARITY_PYTHON "python")
endif()

add_custom_target(test_ShaderGenProfileParitySmoke
    COMMAND ${SHADERGEN_PARITY_PYTHON} ${CMAKE_SOURCE_DIR}/tools/shadergen_profile_parity_runner.py
            --bin-dir ${CMAKE_BINARY_DIR}/out/Windows_64_$<CONFIG>
            --output ${CMAKE_SOURCE_DIR}/doc/shader-system/baseline/shadergen_profile_parity_latest.json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS test_ShaderGenPhysicalDeviceProfileJsonPath
            test_ShaderGenContractValidatorProfile
    COMMENT "Run ShaderGen profile parity smoke baseline"
)

add_custom_target(test_ShaderGenProfileParityExtended
    COMMAND ${SHADERGEN_PARITY_PYTHON} ${CMAKE_SOURCE_DIR}/tools/shadergen_profile_parity_runner.py
            --extended
            --bin-dir ${CMAKE_BINARY_DIR}/out/Windows_64_$<CONFIG>
            --output ${CMAKE_SOURCE_DIR}/doc/shader-system/baseline/shadergen_profile_parity_extended_latest.json
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS test_ShaderGenPhysicalDeviceProfileJsonPath
            test_ShaderGenContractValidatorProfile
            test_RendererShaderGenAdapterProfileCategory
            test_MaterialPresetExhaustiveCompile
    COMMENT "Run ShaderGen profile parity extended baseline"
)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME test_ComposedShaderGenerator_Verify
             COMMAND test_ComposedShaderGenerator_Verify)

    add_test(NAME test_DescriptorSetLifecycleRegression
             COMMAND test_DescriptorSetLifecycleRegression)

    add_test(NAME test_ShaderLogicValidation
             COMMAND test_ShaderLogicValidation)


    add_test(NAME test_ShaderGenPathMode
             COMMAND test_ShaderGenPathMode)

    add_test(NAME test_MaterialPresetExhaustiveCompile
             COMMAND test_MaterialPresetExhaustiveCompile)

    add_test(NAME test_RendererShaderGenAdapterStrictGate
             COMMAND test_RendererShaderGenAdapterStrictGate)

    add_test(NAME test_ShaderGenVertexInputAdapter
             COMMAND test_ShaderGenVertexInputAdapter)

    add_test(NAME test_ShaderGenDescriptorLayoutAdapter
             COMMAND test_ShaderGenDescriptorLayoutAdapter)

    add_test(NAME test_MaterialBuildStrictGatePolicyMatrix
             COMMAND test_MaterialBuildStrictGatePolicyMatrix)

    add_test(NAME test_ShaderGenContractPathContext
             COMMAND test_ShaderGenContractPathContext)

    add_test(NAME test_ShaderGenPhysicalDeviceProfileJsonPath
             COMMAND test_ShaderGenPhysicalDeviceProfileJsonPath)

    add_test(NAME test_ShaderGenContractValidatorProfile
             COMMAND test_ShaderGenContractValidatorProfile)

    add_test(NAME test_RendererShaderGenAdapterProfileCategory
             COMMAND test_RendererShaderGenAdapterProfileCategory)

    add_test(NAME test_ShaderGenProfileParitySmoke
             COMMAND ${SHADERGEN_PARITY_PYTHON} ${CMAKE_SOURCE_DIR}/tools/shadergen_profile_parity_runner.py
                     --bin-dir ${CMAKE_BINARY_DIR}/out/Windows_64_$<CONFIG>
                     --output ${CMAKE_SOURCE_DIR}/doc/shader-system/baseline/shadergen_profile_parity_latest.json)

        add_test(NAME test_ShaderGenProfileParityExtended
             COMMAND ${SHADERGEN_PARITY_PYTHON} ${CMAKE_SOURCE_DIR}/tools/shadergen_profile_parity_runner.py
                 --extended
                 --bin-dir ${CMAKE_BINARY_DIR}/out/Windows_64_$<CONFIG>
                 --output ${CMAKE_SOURCE_DIR}/doc/shader-system/baseline/shadergen_profile_parity_extended_latest.json)

    set_tests_properties(test_MaterialPresetExhaustiveCompile PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")

    set_tests_properties(test_ShaderGenProfileParitySmoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

    set_tests_properties(test_ShaderGenProfileParityExtended PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        LABELS "extended;parity"
        DISABLED TRUE)
endif()

# 需要时可直接运行：
#   cmake --build <build_dir> --target test_ComposedShaderGenerator
#   cmake --build <build_dir> --target test_ComposedShaderGenerator_Verify
#   cmake --build <build_dir> --target test_FSHelperConsistencyValidation
#   cmake --build <build_dir> --target test_DescriptorSetLifecycleRegression
#   cmake --build <build_dir> --target test_ShaderLogicValidation
#   cmake --build <build_dir> --target test_BridgeValidation3Materials
#   cmake --build <build_dir> --target test_HelperInjectionConflict
#   cmake --build <build_dir> --target test_HelperInjectionConflictMatrix
#   cmake --build <build_dir> --target test_ComposedDiagnosticsAggregation
#   cmake --build <build_dir> --target test_TextureBlinnPhongTemplateConformance
#   cmake --build <build_dir> --target test_BasicLitTemplateConformance
#   cmake --build <build_dir> --target test_Gizmo3DTemplateConformance
#   cmake --build <build_dir> --target test_VertexLuminance3DTemplateConformance
#   cmake --build <build_dir> --target test_VertexPattleColor3DTemplateConformance
#   cmake --build <build_dir> --target test_SkyMinimalTemplateConformance
# 在生成目录执行：
#   .\\test\\Debug\\test_ComposedShaderGenerator.exe
#   .\\test\\Debug\\test_ComposedShaderGenerator_Verify.exe
#   .\\test\\Debug\\test_FSHelperConsistencyValidation.exe
#   .\\test\\Debug\\test_DescriptorSetLifecycleRegression.exe
#   .\\test\\Debug\\test_ShaderLogicValidation.exe
#   .\\test\\Debug\\test_BridgeValidation3Materials.exe
#   .\\test\\Debug\\test_HelperInjectionConflict.exe
#   .\\test\\Debug\\test_HelperInjectionConflictMatrix.exe
#   .\\test\\Debug\\test_ComposedDiagnosticsAggregation.exe
#   .\\test\\Debug\\test_TextureBlinnPhongTemplateConformance.exe
#   .\\test\\Debug\\test_BasicLitTemplateConformance.exe
#   .\\test\\Debug\\test_Gizmo3DTemplateConformance.exe
#   .\\test\\Debug\\test_VertexLuminance3DTemplateConformance.exe
#   .\\test\\Debug\\test_VertexPattleColor3DTemplateConformance.exe
#   .\\test\\Debug\\test_SkyMinimalTemplateConformance.exe
