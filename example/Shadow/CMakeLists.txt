# 阴影：级联阴影(CSM)、Alpha 遮罩阴影、双世界阴影。
# 2026-10-11：原 ShadowMap（depth-only 单张阴影图直采示例）已删除 —— 该路线已被 CSM 取代，
# 如需单张阴影图，把 CascadeShadowMap 的 cascade_count 设为 1 即可（宏/预设无需改动）。

CreateProject(CascadeShadowMap               CascadeShadowMap.cpp)
CreateProject(AlphaTestShadow                AlphaTestShadow.cpp)
CreateProject(DualWorldShadows               DualWorldShadows.cpp)
