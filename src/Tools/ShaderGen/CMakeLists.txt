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

# descriptor_macros.glsl 生成器（真源：inc/hgl/common/DescriptorSetTypeDef.h）
add_executable(DescriptorMacroGen
    DescriptorMacroGen.cpp)

target_link_libraries(DescriptorMacroGen
    PRIVATE ULRE.ShaderGen
            CMCore
            CMCoreType)

CreateProject(DescriptorMacroGen)

if(BUILD_TESTING)
    add_test(
        NAME VerifyDescriptorMacros
        COMMAND DescriptorMacroGen --verify ${CMAKE_SOURCE_DIR}/ShaderLibrary/common/descriptor_macros.glsl)

    set_tests_properties(
        VerifyDescriptorMacros
        PROPERTIES LABELS "shadergen;baseline")
endif()

####################################################################################################

# s1_* 顶点 loader 宏一致性校验（规则从 GLSL 文本推导，无平行清单）
# 漏展开 HGL_*_LOADER = 属性静默为 0，无编译/运行期报错——故必须 CI 断言
add_executable(VertexLoaderConsistency
    VertexLoaderConsistency.cpp)

CreateProject(VertexLoaderConsistency)

if(BUILD_TESTING)
    add_test(
        NAME VerifyVertexLoaderConsistency
        COMMAND VertexLoaderConsistency ${CMAKE_SOURCE_DIR}/ShaderLibrary/vertex)

    set_tests_properties(
        VerifyVertexLoaderConsistency
        PROPERTIES LABELS "shadergen;baseline")
endif()

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

add_executable(ShaderDocumentRegression
    ShaderDocumentRegression.cpp)

target_link_libraries(ShaderDocumentRegression
    PRIVATE ULRE.ShaderGen
            CMCore
            CMCoreType)

CreateProject(ShaderDocumentRegression)

if(BUILD_TESTING)
    add_test(
        NAME shader_document_regression
        COMMAND ShaderDocumentRegression)
    set_tests_properties(
        shader_document_regression
        PROPERTIES LABELS "shadergen;document")
endif()

####################################################################################################

add_executable(ShaderLegacyDocumentCompare
    ShaderLegacyDocumentCompare.cpp)

target_link_libraries(ShaderLegacyDocumentCompare
    PRIVATE ULRE.ShaderGen
            CMCore
            CMCoreType)

CreateProject(ShaderLegacyDocumentCompare)

if(BUILD_TESTING)
    add_test(
        NAME shader_legacy_document_compare
        COMMAND ShaderLegacyDocumentCompare)
    set_tests_properties(
        shader_legacy_document_compare
        PROPERTIES LABELS "shadergen;document;comparison")
endif()

####################################################################################################

# 需要时可直接运行：
#   cmake --build <build_dir> --target MaterialPresetExhaustiveCompile
#   cmake --build <build_dir> --target ShaderResourceSchemaRegressionGate
# 在生成目录执行：
#   .\\test\\Debug\\MaterialPresetExhaustiveCompile.exe


####################################################################################################

# 离线 shader 预编译（方向三：离线 cook + 运行时 readonly 加载）
# 枚举 材质×purpose×图元 变体，经生产管线编译 SPV 写入 ShaderArtifactStore
add_executable(ShaderCooker
    ShaderCooker.cpp)

target_link_libraries(ShaderCooker
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)

CreateProject(ShaderCooker)

if(BUILD_TESTING)
    # 冒烟：只 cook 一个内置材质（需要 cwd=仓库根以定位 GLSLCompiler.dll 与 ShaderLibrary）
    add_test(
        NAME ShaderCookSmoke
        COMMAND ShaderCooker --material builtin/pure_color
                --store ${CMAKE_BINARY_DIR}/shader-cook-smoke
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        ShaderCookSmoke
        PROPERTIES LABELS "shadergen;baseline")
endif()

####################################################################################################
