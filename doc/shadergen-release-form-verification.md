# ShaderGen 发行形态与运行形态验收

> 2026-10。回答一个问题：**「把 Shader 生成器从发行版里彻底切掉」这件事，靠什么证明它成立？**

## 1. 两种形态

| | 开发形态（`ULRE_ENABLE_SHADERGEN=ON`） | 发行形态（`ULRE_ENABLE_SHADERGEN=OFF`） |
|---|---|---|
| 链接图 | `ULRE::Engine` = `ULRE::Runtime` + `ULRE.ShaderGen` | `ULRE::Runtime` 单独成立 |
| 有 GLSL 源码 | 有（`ShaderLibrary/`） | **没有** |
| 有 glslang | 有 | **没有** |
| 程序怎么来 | 现场生成 → 落 shader-cache | **只读查表**，未命中硬失败 |
| 产物 | `.lib` + shader-cache | `.exe/.dll` + 预烘焙 SPV |

两种形态共用同一条运行时装配路径（`ShaderProgramManager::AcquireShaderProgram`），
差别只在**装的是哪个 `ShaderProgramBuildBackend`**：

- `InitShaderCompiler()` 装生成器后端（开发形态，先到先得）；
- `EnsureShaderProgramManifestBackend()` 装清单后端（发行形态）；
- `UninstallShaderProgramManifestBackend()` 显式卸下。

清单后端在 `ShaderCacheMode::ReadOnly` 下**绝不回写**，未命中即硬失败 —— 不存在
「悄悄回落编译器」这条路，也就不会出现「发行版其实还在生成」的假绿。

## 2. 为什么不能只靠「编译通过」验收

`ULRE_ENABLE_SHADERGEN=OFF` 时 `ULRE.ShaderGen` 根本不进构建图，所以「OFF 树能构建绿」
只证明**没有源码级反向引用**。它证明不了：

1. 运行时算出的语义键，与烘焙期算出的语义键是否**同一个值**；
2. 清单/stage 文件的磁盘布局，两端是否**同一个约定**；
3. 只读装载路径上有没有残留的写入尝试。

这三条只能在**跨进程**、**跨构建树**的真实链路上验证。

## 3. 验收链路

```
[开发形态树 build/]
  ShaderResourceSchemaRegressionGate emit-release-cache build/release-cache-fixture
      └─ 用真实 MaterialDefinition + 真实请求装配 → 生成 stage SPV
      └─ RecordShaderProgramManifestEntry → <fixture>/shader-cache/program_manifest.pmf
      └─ WriteManifestExpectation         → <fixture>/shader-cache/expectation.txt
                                              （固化 8 个字段：program/lookup/stage digest、
                                                SPV 字节数、requires_recipe_runtime_rows）
      └─ 编 GPU cull 的 GLSL → SPV
      └─ RecordShaderComputeProgramArtifact → <fixture>/shader-cache/compute_manifest.pmf
      └─ 写 compute 期望                    → <fixture>/shader-cache/compute_expectation.txt
                                              （program_name / spv_bytes / spv_digest /
                                                push_constant_size）

[发行形态树 build_probe_nosg/]
  RuntimeFormProbe build/release-cache-fixture        ← 只链 ULRE::Runtime
      ├─ 链接通过 = 运行时零反向依赖生成器（有就算 LNK2019，构建即红）
      ├─ 重算 ComputeMaterialProgramManifestKey   → 必须等于 expectation.lookup_key
      ├─ 只读装清单 + 两 stage SPV                → digest / 字节数逐项对拍
      ├─ CreateBuildContext                       → 必须成功（能装配出可提交的 build context）
      ├─ 未知 ambient 名                          → 必须硬失败（证明不是「反正都能过」）
      ├─ LoadShaderComputeProgramArtifact         → 按名字取 compute SPV，
      │                                             字节数 / FNV1a digest / push constant 逐项对拍
      ├─ 未知 compute 程序名                      → 必须硬失败
      └─ 结束后两份清单文件 mtime/size 不变         → 证明只读模式没写盘
```

`expectation.txt` / `compute_expectation.txt` 是把烘焙侧的期望值**落到磁盘**，而不是让
探针自己推。探针不知道「正确答案是什么」，只知道「磁盘上说答案是这个」，因此它无法与
烘焙侧一起错，也无法单方面放宽标准。

**为什么 compute 单独一份清单**：材质清单的键是材质语义维度（definition + request +
profile），记录形状是 mesh+fragment 两段定长 `ShaderLinkSpec`；compute 程序没有材质
定义、也没有两段 link，身份就是一个程序名。把它硬塞进材质清单，要么复用
`fragment_stage` 字段（语义撒谎），要么给定长记录加形态判别位（污染 `IsValid`）。
而且一份文件损坏只该作废自己。所以 compute 走 `compute_manifest.pmf`，header / stamp /
校验和机制与材质清单完全同构，只换 `ShaderArtifactKind`。

## 4. 一条命令跑完

```powershell
pwsh scripts/ci/verify-release-form.ps1
```

默认参数：`-DevBuildDir build -ProdBuildDir build_probe_nosg -Config Debug`。
需要两棵树共四个阶段产物（gate / 夹具 / 探针 / 探针运行结果），任一步失败退出码非 0，
且失败信息能直接区分是「烘焙错」「链接错」还是「运行错」。

挂进常规基线：

```powershell
pwsh scripts/ci/build-baseline.ps1 -VerifyReleaseForm
```

## 5. 为什么探针不注册成 ctest

夹具不是构建产物。干净树上 ctest 只会**恒红**；而把它做成 ctest 的 POST_BUILD，
又会把「两棵树 + 跨进程」的语义压成一次同进程调用，失去验收意义。
所以探针是一个**显式驱动的工具**，由 `verify-release-form.ps1` 编排。

## 6. 变更这类代码时要跑什么

| 改动 | 必须跑 |
|---|---|
| `ULRE::Runtime` 成员列表 | `verify-release-form.ps1` |
| `ShaderProgramManifestBackend` / `ComputeMaterialProgramManifestKey` | `verify-release-form.ps1` + `Gate all`（58 条） |
| `ShaderArtifactStore` 磁盘布局 | `verify-release-form.ps1` + `Gate cache` |
| `compute_manifest.pmf` 格式 / `LoadShaderComputeProgramArtifact` | `verify-release-form.ps1` + `Gate cache-compute-lookup` |
| `GPUCullSystem` 的 push constant 布局 | `verify-release-form.ps1` + `ctest -L ecs` |

`Gate all` 与 `ctest -L "ecs|csm|shadergen"` 是常规回归，与本验收互补：
前者验证**同形态内**的数据面一致性，后者验证**跨形态**的可切分性。

## 7. 已知边界

- 探针覆盖 `mesh + fragment` 两段的 forward-lit 材质程序，以及 GPU cull 的 compute
  程序。材质走 `program_manifest.pmf`，compute 走 `compute_manifest.pmf`。
- 探针**不建 VkPipeline**：它没有 `VkDevice`，且只链 `ULRE::Runtime`
  （`ShaderProgramManager` 属 `ULRE.SceneGraph`）。compute 的验证停在「数据面能取到
  与烘焙期逐字节相同的 SPV + push constant 契约一致」这层；真正的
  `vkCreateComputePipelines` 由引擎运行时在目标机器上完成。若要在这条链路里验到
  `CreateShaderModuleFromSPV`，探针需改链 `ULRE.SceneGraph` —— 但那会削弱「零生成器
  符号」这条断言的可读性，故暂不做。
- 探针在 ON 树里也能跑通，但此时 `InitShaderCompiler()` 已抢先装好生成器后端，
  `EnsureShaderProgramManifestBackend()` 按「先到先得」不会顶替 ——
  ON 树里跑探针验证的是**夹具与清单后端逻辑**，不是纯发行行为。
  真正的发行行为只在 `build_probe_nosg` 里成立。
- **`-SkipBuild` 会产生假 FAIL**。它复用旧的 `RuntimeFormProbe.exe`，而 `ULRE::Runtime`
  的 ABI 会随运行时改造变化 ⇒ 探针静默 `abort()`：MSVC 退出码 **3**、stdout/stderr **全空**。
  看到「exit=3 且零输出」就是二进制陈旧，**去掉 `-SkipBuild` 重建即 PASS**，
  不要去查夹具或清单后端（这条踩过一次，白查半小时）。
  `-SkipConfigure` 与 `-SkipBake` 是安全的：前者只重读 `CMakeCache.txt`，后者产出的夹具
  本身会被探针逐字节校验。
