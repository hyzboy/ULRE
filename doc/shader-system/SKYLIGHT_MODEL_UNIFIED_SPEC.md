# SkyLight 统一模型接口规范

本文档定义 ULRE Shader System 中 SkyLight 的统一访问契约与模型切换规则。目标是：

- 材质侧只依赖一组稳定函数接口；
- 天光模型（Simple/IBL/EnvMap/SH）可在不改材质业务代码的前提下切换；
- Legacy 与 Composed-Business 路径保持同一语义。

---

## 1. 统一入口与契约

统一入口头文件：

- `src/ShaderGen/common/MFSkyLight.h`

统一函数契约（材质必须通过以下函数读取天光）：

- `vec3 ULRE_GetSkyLightDir()`
- `vec3 ULRE_GetSkyLightColor()`
- `vec3 ULRE_GetSkyAmbientColor()`

禁止在材质中直接复制/粘贴各自版本的 `ULRE_GetFakeSkyAmbient()` 等私有实现。

---

## 2. 模型枚举与切换规则

`MFSkyLight.h` 提供模型枚举：

- `ULRE_SKYLIGHT_MODEL_SIMPLE`
- `ULRE_SKYLIGHT_MODEL_IBL`
- `ULRE_SKYLIGHT_MODEL_ENVMAP`
- `ULRE_SKYLIGHT_MODEL_SH`

默认策略：

1. 若材质未定义 `ULRE_SKYLIGHT_MODEL`，默认使用 `ULRE_SKYLIGHT_MODEL_SIMPLE`。
2. 若存在 `USE_IBL`（历史开关），自动映射到 `ULRE_SKYLIGHT_MODEL_IBL`。

建议策略（新代码）：

- 直接定义 `ULRE_SKYLIGHT_MODEL`，不再新增 `USE_IBL` 分支。

---

## 3. 材质侧接入规范

### 3.1 Fragment 主体

片元 shader 字符串必须以统一块开头：

```cpp
constexpr const char fs_main[] = ULRE_SKYLIGHT_GLSL_COMMON R"(
// material fragment business...
)";
```

### 3.2 分支方式

材质中与天光模型有关的行为差异使用：

```glsl
#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL
    // IBL 分支
#else
    // 非 IBL 分支
#endif
```

不要再写：

```glsl
#ifdef USE_IBL
...
#endif
```

### 3.3 Legacy 路径定义注入

Legacy `CustomFragmentShader()` 若需要 IBL 模型，应注入：

```cpp
fsc->AddDefine("ULRE_SKYLIGHT_MODEL", "ULRE_SKYLIGHT_MODEL_IBL");
```

---

## 4. 当前落地状态

已对齐到统一模型分支结构：

- `src/ShaderGen/3d/M_BasicLit.cpp`
- `src/ShaderGen/3d/M_TextureBlinnPhong.cpp`

说明：

- 两者均通过 `ULRE_GetSkyLight*` 获取天光输入；
- `ULRE_SKYLIGHT_MODEL_IBL` 分支已预留并可编译运行；
- `ENVMAP/SH` 当前为接口预留，后续可在 `MFSkyLight.h` 内集中替换实现。

---

## 5. 扩展约束（后续 IBL/EnvMap/SH 实装）

实现新模型时必须满足：

1. 不修改材质业务函数签名（保持 `ULRE_GetSkyLight*` 不变）。
2. 不要求材质改 descriptor 名称（沿用 `sky` 等既有资源命名规范）。
3. 新增资源（如 cubemap / SH 系数）时，先在统一层给出降级路径（无资源时回退到 `SIMPLE`）。
4. 优先保持 Legacy 与 Composed 视觉趋势一致，允许小幅误差但禁止量级偏差。

---

## 6. 验证建议

最小验证集：

- 构建：`ULRE.ShaderGen`
- 运行：`06b_BasicLitMeshesECS.exe`
- 运行：`06c_TextureBlinnPhongMeshesECS.exe`

判定标准：

- 无 shader 编译错误；
- 无 descriptor/binding 缺失错误；
- 画面有稳定照明且不会全黑。

---

## 变更历史

| 日期 | 版本 | 变更 | 负责人 |
|---|---|---|---|
| 2026-02-26 | v1.0 | 初版：定义 SkyLight 统一接口、模型切换规则与接入规范 | Copilot |
| 2026-02-28 | v1.1 | 文档口径同步：阶段状态与时间戳对齐（无接口变更） | Copilot |
