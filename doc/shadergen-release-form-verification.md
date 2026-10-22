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

[发行形态树 build_probe_nosg/]
  RuntimeFormProbe build/release-cache-fixture        ← 只链 ULRE::Runtime
      ├─ 链接通过 = 运行时零反向依赖生成器（有就算 LNK2019，构建即红）
      ├─ 重算 ComputeMaterialProgramManifestKey   → 必须等于 expectation.lookup_key
      ├─ 只读装清单 + 两 stage SPV                → digest / 字节数逐项对拍
      ├─ CreateBuildContext                       → 必须成功（能装配出可提交的 build context）
      ├─ 未知 ambient 名                          → 必须硬失败（证明不是「反正都能过」）
      └─ 结束后清单文件 mtime/size 不变            → 证明只读模式没写盘
```

`expectation.txt` 是把烘焙侧的期望值**落到磁盘**，而不是让探针自己推。
探针不知道「正确答案是什么」，只知道「磁盘上说答案是这个」，因此它无法与烘焙侧
一起错，也无法单方面放宽标准。

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
| `ShaderProgramManifestBackend` / `ComputeMaterialProgramManifestKey` | `verify-release-form.ps1` + `Gate all`（50 条） |
| `ShaderArtifactStore` 磁盘布局 | `verify-release-form.ps1` + `Gate cache` |
| 新增 stage 类型 / compute 程序 | `verify-release-form.ps1` + `Gate pipeline` |

`Gate all` 与 `ctest -L "ecs|csm|shadergen"` 是常规回归，与本验收互补：
前者验证**同形态内**的数据面一致性，后者验证**跨形态**的可切分性。

## 7. 已知边界

- 探针当前只覆盖 `mesh + fragment` 两个 stage 的 forward-lit 程序。
  compute 程序（`GPUCullSystem`）仍走吃 GLSL 源码的重载，属后续阶段。
- 探针在 ON 树里也能跑通，但此时 `InitShaderCompiler()` 已抢先装好生成器后端，
  `EnsureShaderProgramManifestBackend()` 按「先到先得」不会顶替 ——
  ON 树里跑探针验证的是**夹具与清单后端逻辑**，不是纯发行行为。
  真正的发行行为只在 `build_probe_nosg` 里成立。
