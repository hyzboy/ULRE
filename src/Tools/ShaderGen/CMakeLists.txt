cmake_minimum_required(VERSION 3.28)

# 注意：本宏只"修饰"已由 add_executable 建好的 target（补 include 目录 / IDE 分组 / 调试工作目录），
# 不创建 target。此前它与 example/ 下同名宏语义完全不同，容易误用，故改名以示区别。
macro(SetupToolTarget name)

    target_include_directories(${name}
        PRIVATE ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/other_engines/niagara/extern/glm
                ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include)

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Tools/ShaderProgramBuild")

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

SetupToolTarget(VulkanPhysicalDeviceProfileCollector)

####################################################################################################

# descriptor_macros.glsl 生成器（真源：inc/hgl/common/DescriptorSetTypeDef.h）
add_executable(DescriptorMacroGen
    DescriptorMacroGen.cpp)

target_link_libraries(DescriptorMacroGen
    PRIVATE ULRE.ShaderProgramBuild
            CMCore
            CMCoreType)

SetupToolTarget(DescriptorMacroGen)

if(BUILD_TESTING)
    add_test(
        NAME VerifyDescriptorMacros
        COMMAND DescriptorMacroGen --verify ${CMAKE_SOURCE_DIR}/ShaderLibrary/common/descriptor_macros.glsl)

    set_tests_properties(
        VerifyDescriptorMacros
        PROPERTIES LABELS "shadergen;baseline")
endif()

# 构建期校验（不止 ctest）：生成物一旦与真源漂移即构建失败。
# 真源 = inc/hgl/common/DescriptorSetTypeDef.h 的 kDescriptorBindingMacros 规范表（SceneBinding 枚举已随集退场）。
add_custom_target(descriptor_macros_verify ALL
    COMMAND $<TARGET_FILE:DescriptorMacroGen> --verify ${CMAKE_SOURCE_DIR}/ShaderLibrary/common/descriptor_macros.glsl
    DEPENDS DescriptorMacroGen
    COMMENT "校验 descriptor_macros.glsl 与真源一致")

####################################################################################################

# interstage_locations.glsl 生成器（真源：ShaderSemanticRegistry 规范表）
# stable_location 进 program 缓存键，GLSL 侧数值单边漂移不会被编译期抓到（两个 stage
# 都用宏、各自仍自洽），故生成物必须与注册表逐条相等，构建期强制校验。
add_executable(InterStageLocationMacroGen
    InterStageLocationMacroGen.cpp)

target_link_libraries(InterStageLocationMacroGen
    PRIVATE ULRE.ShaderProgramBuild
            CMCore
            CMCoreType)

SetupToolTarget(InterStageLocationMacroGen)

if(BUILD_TESTING)
    add_test(
        NAME VerifyInterStageLocations
        COMMAND InterStageLocationMacroGen --verify ${CMAKE_SOURCE_DIR}/ShaderLibrary/common/interstage_locations.glsl)

    set_tests_properties(
        VerifyInterStageLocations
        PROPERTIES LABELS "shadergen;baseline")
endif()

add_custom_target(interstage_locations_verify ALL
    COMMAND $<TARGET_FILE:InterStageLocationMacroGen> --verify ${CMAKE_SOURCE_DIR}/ShaderLibrary/common/interstage_locations.glsl
    DEPENDS InterStageLocationMacroGen
    COMMENT "校验 interstage_locations.glsl 与 ShaderSemanticRegistry 一致")

####################################################################################################

# s1_* 顶点 loader 宏一致性校验（规则从 GLSL 文本推导，无平行清单）
# 漏展开 HGL_*_LOADER = 属性静默为 0，无编译/运行期报错——故必须 CI 断言
add_executable(VertexLoaderConsistency
    VertexLoaderConsistency.cpp)

SetupToolTarget(VertexLoaderConsistency)

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
    PRIVATE ULRE.ShaderProgramBuild
            ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)
target_include_directories(ShaderResourceSchemaRegressionGate
    PRIVATE ${CMAKE_SOURCE_DIR}/src/Tools/GLSLCompiler)

SetupToolTarget(ShaderResourceSchemaRegressionGate)
add_dependencies(ShaderResourceSchemaRegressionGate descriptor_macros_verify)

target_compile_definitions(ShaderResourceSchemaRegressionGate
    PRIVATE ULRE_REPO_ROOT="${CMAKE_SOURCE_DIR}")

if(BUILD_TESTING)
    add_test(
        NAME shader_resource_schema_regression_gate
        COMMAND ShaderResourceSchemaRegressionGate)

    set_tests_properties(
        shader_resource_schema_regression_gate
        PROPERTIES LABELS "shadergen;baseline"
                   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # 每个分组单独成一个 ctest 用例，供 -L <group> 定向运行；全部检查由
    # 上面不带参数的用例（等价于 "all"）覆盖。
    foreach(regression_group
            glsl
            interface
            descriptor
            cache
            materialization
            pipeline
            reflection
            module-invariants)
        add_test(
            NAME shader_resource_schema_regression_gate.${regression_group}
            COMMAND ShaderResourceSchemaRegressionGate ${regression_group})
        set_tests_properties(
            shader_resource_schema_regression_gate.${regression_group}
            PROPERTIES LABELS "shadergen;baseline;${regression_group}"
                       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endforeach()
endif()

####################################################################################################

add_executable(SceneRenderTemplateResolverRegression
    SceneRenderTemplateResolverRegression.cpp)

target_link_libraries(SceneRenderTemplateResolverRegression
    PRIVATE ULRE.ShaderProgramBuild
            CMCore
            CMCoreType)

SetupToolTarget(SceneRenderTemplateResolverRegression)

if(BUILD_TESTING)
    add_test(
        NAME scene_render_template_resolver_regression
        COMMAND SceneRenderTemplateResolverRegression
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        scene_render_template_resolver_regression
        PROPERTIES LABELS "shadergen;template")
    add_test(
        NAME shader_include_root_regression
        COMMAND SceneRenderTemplateResolverRegression --compiler-root
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        shader_include_root_regression
        PROPERTIES LABELS "shadergen;compiler")
endif()

####################################################################################################

add_executable(ShaderDocumentProductionRegression
    ShaderDocumentProductionRegression.cpp)

target_link_libraries(ShaderDocumentProductionRegression
    PRIVATE ULRE.ShaderProgramBuild
            ULRE.Vulkan
            CMCore
            CMMath
            CMCoreType)

SetupToolTarget(ShaderDocumentProductionRegression)

if(BUILD_TESTING)
    add_test(
        NAME shader_document_production_regression
        COMMAND ShaderDocumentProductionRegression --smoke
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(
        shader_document_production_regression
        PROPERTIES LABELS "shadergen;document;production")
endif()

####################################################################################################
