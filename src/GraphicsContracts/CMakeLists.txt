# ============================================================================
# ULRE.GraphicsContracts
#
# 后端无关的图形契约层：Vulkan（后端）与 SceneGraph / ECS.Render（上层）
# 都需要、但任何一方都不该独占的最小数据类型与接口。
#
# 判定标准（收进本目标的门槛）：
#   · 只依赖 CMCore / CMMath / Vulkan 类型或纯标量，不 include hgl/graph 的业务头；
#   · 不持有资源、不做 IO、不含场景语义；
#   · 至少被两个层共享（否则直接留在使用方所在层）。
#
# 本目标是 header-only INTERFACE：契约只声明数据与接口，不产生实现符号。
# ============================================================================

set(GRAPHICS_CONTRACTS_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/graphics/contracts)

file(GLOB GRAPHICS_CONTRACTS_HEADERS CONFIGURE_DEPENDS
    "${GRAPHICS_CONTRACTS_INCLUDE_PATH}/*.h")

source_group("GraphicsContracts" FILES ${GRAPHICS_CONTRACTS_HEADERS})

add_library(ULRE.GraphicsContracts INTERFACE)

target_include_directories(ULRE.GraphicsContracts INTERFACE ${ROOT_INCLUDE_PATH})

target_sources(ULRE.GraphicsContracts INTERFACE ${GRAPHICS_CONTRACTS_HEADERS})
