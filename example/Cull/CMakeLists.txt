# GPU 剔除（T3 线）：引擎 GPUCullSystem 的**正式示例**。
# 剔除以引擎为准（EntityGPU 表 + L2W 池 + GeometryAABB 池 → cull shader → gather 四表
# → ICB 接管 + 每批计数）；示例只装配实体与相机，不建 compute / 缓冲 / BDA 地址表。

macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Cull")
endmacro()

CreateProject(GPUDrivenCull                 GPUDrivenCull.cpp)