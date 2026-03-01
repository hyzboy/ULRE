# 环境光来源

此文档设定目标为多种环境光提供统一的来源

目标为在Shader逻辑中，环境光由一个固定函数提供，而这个函数的具体实现由合成器根据玩家选择等级提供。

1.最简单的环境光

    本质就是:

        exp2(仰角)*预设的天空基色

        这段glsl会产生一个越接近地平线越白的过渡效果。

2.一些较为简易shader实现的假的大气渲染

3.CubeMap 普通CubeMap渲染
    这个其实分两种，一种就是扔个CubeMap不管了。
    还有一种是走ComputeShader算大气渲染，输出到一张CubeMap上。但这里不关心它，就当是普通Cubemap

4.使用SH技术的环境光

5.使用IBL的环境光
    注：SHADER不区分IBL的来源，如有多个IBL相交，由ComputeShader做混合，shader中只取一个IBL CUBEMAP就行。

---

## ECS 对接约定（阶段 1：仅接口，不做完整实现）

目标：SkyLight 的资源需求由 ECS 环境系统统一管理，Shader 侧只调用统一函数（如 `ULRE_GetSkyAmbientColor()`）获取结果，不关心数据来源细节。

### 设计分工

- `SkyLightAmbientModel` 仍是唯一模型枚举（定义于 `SkyLight.h`）。
- `EnvironmentSystem` 持有当前 `SkyLightAmbientModel`，并负责声明该模型的资源需求（`CubeMap` 需要环境 CubeMap，`SphericalHarmonics` 需要 SH UBO，`IBL` 需要 IBL CubeMap）。
- 业务材质模块只做“采样后的混合与着色”，不负责判断模型需要哪类资源。

### 当前已落地接口（仅占位）

位于 `EnvironmentSystem.h`：

- `SetSkyLightAmbientModel(...)` / `GetSkyLightAmbientModel()`
- `GetSkyLightResourceRequirement()`
- `SetSkyLightResourceBinding(...)` / `GetSkyLightResourceBinding()`
- `IsSkyLightResourceReady()`

说明：这些接口当前是“声明性 + 最小状态管理”，不触发真实资源创建、GPU 上传或生命周期管理。

### 后续实现阶段（未做）

1. 将 `SkyLightResourceBinding` 对接实际资源系统句柄（而非字符串名）
2. 在 `EnvironmentSystem::Update/Sync` 中增加资源热切换与一致性更新
3. 将 SH UBO / CubeMap / IBL CubeMap 绑定到统一的 shader 输入位
4. 让 `ULRE_GetSkyAmbientColor()` 在各模型下无缝返回正确结果（不依赖业务模块分支）

### 当前状态（2026-03-01）

- 已完成框架层：SkyLight 模型资源需求可由注入结构动态追加到 descriptor 与 required_resources。
- `BasicLit` / `TextureBlinnPhong` 已接入动态资源注入机制，业务层不再硬编码 `SkyCubeMap` 依赖。
- `CubeMap` / `IBL` / `SH` 具体采样与计算实现仍为后续阶段（当前保持接口和占位实现）。
