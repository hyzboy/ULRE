add_subdirectory(Debug)
add_subdirectory(Vulkan)
# ShaderRuntimeContract 必须先于 ShaderGen：ShaderGen 依赖它，反向不成立。
add_subdirectory(ShaderRuntimeContract)
# ShaderMaterialData：材质/着色器数据面（零 GLSL 发射、零 glslang），发行形态照常构建。
add_subdirectory(ShaderMaterialData)
# ShaderCompiler：glslang 插件桥（编译/反射）。运行时（SceneGraph）也要用，故不受
# ULRE_ENABLE_SHADERGEN 约束；它不认识任何构建后端，后端由上层注册生命周期钩子。
add_subdirectory(ShaderCompiler)
# 生成器侧：发行形态（ULRE_ENABLE_SHADERGEN=OFF）下不构建。
# 注意：ULRE.SceneGraph / ULRE.Vulkan / ULRE.Work / ULRE.ECS 里仍有 TU include
# ShaderGen 的头文件——头文件在源码树里始终存在，所以这里收掉目标不会断编译，
# 但**任何运行时目标的 TU 都不许再引用 ShaderGen 定义的符号**（否则发行形态链接失败）。
IF(ULRE_ENABLE_SHADERGEN)
    add_subdirectory(ShaderGen)
ENDIF()
add_subdirectory(SceneGraph)
add_subdirectory(InlineGeometry)
add_subdirectory(Work)
add_subdirectory(ecs)
add_subdirectory(Tools)
# GUI：源码树在，但 CMGUI 模块未接入本工程（未 use_cm_module(GUI)）
#add_subdirectory(GUI)
