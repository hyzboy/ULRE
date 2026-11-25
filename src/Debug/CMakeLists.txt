# ============================================================================
# ULRE.Debug —— DEBUG 通道开关的唯一定义目标（ULRE_DEBUG.toml 的加载与解析）
#
# 为什么单独成库：它只依赖 CMCore，却被 Vulkan / SceneGraph / ECS / Work 各层共用，
# 放进任何一方都会让其余层为了一个符号去依赖不该依赖的模块。独立成最小目标后，
# 上层只需 PUBLIC 链接它一次。
#
# 注：2026-10 版注释曾把 ECS/SceneGraph/Vulkan/Shader* 描述为「强连通分量、无法声明
# 有向依赖」。该结论已在 2026-11 的 refactor/module-layering 中解环并失效——现在
# Vulkan 只依赖 GraphicsContracts（+ ShaderMaterialData 的数据面），SceneGraph 依赖
# Vulkan，ECS.Core 无图形依赖。详见 doc/build-and-module-structure-audit-2026-10.md
# 文末的《2026-11 模块分层复核》。
#
# 放在 src/CMakeLists.txt 的**第一个** add_subdirectory：它除了 CMCore 不依赖
# 任何 ULRE 目标。
# ============================================================================

SET(DEBUG_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/common)

SET(DEBUG_FILES ${DEBUG_INCLUDE_PATH}/DebugChannel.h
                DebugChannel.cpp)

SOURCE_GROUP("Debug" FILES ${DEBUG_FILES})

add_cm_library(ULRE.Debug "ULRE" ${DEBUG_INCLUDE_PATH} ${DEBUG_FILES})

# hgl/log、hgl/io、hgl/filesystem 来自 CMCore；PUBLIC 是为了让消费者拿到同名
# 头，不会因为本库而少一个依赖。
target_link_libraries(ULRE.Debug PUBLIC CMCore)
