add_subdirectory(Vulkan)
# ShaderRuntimeContract 必须先于 ShaderGen：ShaderGen 依赖它，反向不成立。
add_subdirectory(ShaderRuntimeContract)
add_subdirectory(ShaderGen)
add_subdirectory(SceneGraph)
add_subdirectory(InlineGeometry)
add_subdirectory(Work)
add_subdirectory(ecs)
add_subdirectory(Tools)
# GUI：源码树在，但 CMGUI 模块未接入本工程（未 use_cm_module(GUI)）
#add_subdirectory(GUI)
