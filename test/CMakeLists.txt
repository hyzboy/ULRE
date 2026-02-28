cmake_minimum_required(VERSION 3.28)

add_executable(test_ComposedShaderGenerator
    test_ComposedShaderGenerator.cpp)

target_link_libraries(test_ComposedShaderGenerator
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan)

target_include_directories(test_ComposedShaderGenerator
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

add_executable(test_ComposedShaderGenerator_Verify
    test_ComposedShaderGenerator_Verify.cpp)

target_include_directories(test_ComposedShaderGenerator_Verify
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

add_executable(test_FSHelperConsistencyValidation
    test_FSHelperConsistencyValidation.cpp)

target_link_libraries(test_FSHelperConsistencyValidation
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore
            Vulkan::Vulkan)

target_include_directories(test_FSHelperConsistencyValidation
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

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

add_executable(test_ShaderLogicValidation
    ShaderLogicValidationTest.cpp)

target_link_libraries(test_ShaderLogicValidation
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_ShaderLogicValidation
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

add_executable(test_BridgeValidation3Materials
    BridgeValidation3MaterialsTest.cpp)

target_link_libraries(test_BridgeValidation3Materials
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_BridgeValidation3Materials
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

add_executable(test_HelperInjectionConflict
    HelperInjectionConflictTest.cpp)

target_link_libraries(test_HelperInjectionConflict
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_HelperInjectionConflict
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

add_executable(test_HelperInjectionConflictMatrix
    HelperInjectionConflictMatrixTest.cpp)

target_link_libraries(test_HelperInjectionConflictMatrix
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_HelperInjectionConflictMatrix
    PRIVATE ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

add_executable(test_ComposedDiagnosticsAggregation
    ComposedDiagnosticsAggregationTest.cpp)

target_link_libraries(test_ComposedDiagnosticsAggregation
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore
            Vulkan::Vulkan)

target_include_directories(test_ComposedDiagnosticsAggregation
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

add_executable(test_TextureBlinnPhongTemplateConformance
    TextureBlinnPhongTemplateConformanceTest.cpp)

target_link_libraries(test_TextureBlinnPhongTemplateConformance
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore)

target_include_directories(test_TextureBlinnPhongTemplateConformance
    PRIVATE ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
            ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME test_ComposedShaderGenerator
             COMMAND test_ComposedShaderGenerator)

    add_test(NAME test_ComposedShaderGenerator_Verify
             COMMAND test_ComposedShaderGenerator_Verify)

    add_test(NAME test_FSHelperConsistencyValidation
             COMMAND test_FSHelperConsistencyValidation)

    add_test(NAME test_DescriptorSetLifecycleRegression
             COMMAND test_DescriptorSetLifecycleRegression)

    add_test(NAME test_ShaderLogicValidation
             COMMAND test_ShaderLogicValidation)

    add_test(NAME test_BridgeValidation3Materials
             COMMAND test_BridgeValidation3Materials)

    add_test(NAME test_HelperInjectionConflict
             COMMAND test_HelperInjectionConflict)

    add_test(NAME test_HelperInjectionConflictMatrix
             COMMAND test_HelperInjectionConflictMatrix)

    add_test(NAME test_ComposedDiagnosticsAggregation
             COMMAND test_ComposedDiagnosticsAggregation)

    add_test(NAME test_TextureBlinnPhongTemplateConformance
             COMMAND test_TextureBlinnPhongTemplateConformance)
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
