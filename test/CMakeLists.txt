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
endif()

# 需要时可直接运行：
#   cmake --build <build_dir> --target test_ComposedShaderGenerator
#   cmake --build <build_dir> --target test_ComposedShaderGenerator_Verify
#   cmake --build <build_dir> --target test_FSHelperConsistencyValidation
#   cmake --build <build_dir> --target test_DescriptorSetLifecycleRegression
# 在生成目录执行：
#   .\\test\\Debug\\test_ComposedShaderGenerator.exe
#   .\\test\\Debug\\test_ComposedShaderGenerator_Verify.exe
#   .\\test\\Debug\\test_FSHelperConsistencyValidation.exe
#   .\\test\\Debug\\test_DescriptorSetLifecycleRegression.exe
