# GPU 剔除（T3 线）：引擎 GPUCullSystem 的**正式示例**。
# 剔除以引擎为准（EntityGPU 表 + L2W 池 + GeometryAABB 池 → cull shader → gather 四表
# → ICB 接管 + 每批计数）；示例只装配实体与相机，不建 compute / 缓冲 / BDA 地址表。

CreateProject(GPUDrivenCull                 GPUDrivenCull.cpp)
