# ============================================================================
# ULRE.Debug —— DEBUG 通道开关的唯一定义目标（ULRE_DEBUG.toml 的加载与解析）
#
# 为什么单独成库：ULRE.Vulkan / ULRE.ShaderProgramBuild / ULRE.ShaderMaterialData /
# ULRE.SceneGraph / ULRE.ECS 之间没有可声明的有向依赖（强连通分量，见
# doc/build-and-module-structure-audit-2026-10.md），任何一方都不适合承载一个
# 「所有层都要用」的符号定义。独立成最小目标后，上层只需 PUBLIC 链接它一次。
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
