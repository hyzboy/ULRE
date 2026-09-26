# Alpha Test 阴影（ShadowCasterMasked）修复链全记录

> 时间：2026-09-26；分支 `CSM`；起因 = csm-review-2026-09-25 的 A1/A1-4（阴影 pass 程序接线），
> 追至终极根因 = RenderPass::CreatePipeline 的 depth-only 快速路径无条件剥离片元 stage。
> 本文记录完整因果链、材质渲染管线关键机制、取证方法与经验教训。相关提交见文末清单。

---

## 0. 一句话结论

**ShadowCasterMasked 片元（含 discard）写得再对也从未生效过——
`RenderPass::CreatePipeline` 的 depth-only 快速路径（shadow map 等零颜色附件）
无条件剥离片元 stage，豁免条件只有 alpha_blend / alpha_to_coverage，
漏了 alpha_test。** 片元被剥后深度图由纯 mesh stage 光栅化，镂空材质退化为
实心；A1-4 以来所有片元侧/数据链修复均被这层屏蔽，表现为"修一层露出下一层"。

---

## 1. 背景与问题现象

| 阶段 | 现象 |
|------|------|
| A1 修复后 | 阴影 pass 从 forward 程序切到 ShadowCaster 程序（opaque 路径）正常 |
| A1-4 masked 接线后 | `shadow_caster_masked` 路由正常、SPIRV 编译正常，但 **深度图里 alpha 物体是实心方块**（RenderDoc 确认），地面影子与 opaque 一致 |
| RenderDoc 观察 | `vkCmdDrawMeshTasksIndirectEXT` 无法反推 GLSL；shadow draw 的 resources 里**无任何贴图**；两个 cube 合并为一条 draw(1,2,1) |

后两条观察的正确解读（当时误导了排查方向）：
- **合并成一条 multi-draw 是正确合批**：两个 cube 同为 ShadowCasterMasked 程序
  （FallbackCube 只是不绑 opacity_mask，程序相同）——同 shader + 同 pipeline
  合一条 multi-draw 是设计行为。
- **"resources 无贴图" = 片元被剥后的真实表现**（无采样指令），不是数据链问题。

---

## 2. 材质渲染管线关键机制（本次深挖确认）

修复过程覆盖了从 collect 到 GPU 的完整链路，关键机制如下（改动前请先读）：

```
collect（每 pass，含 4 次 shadow RenderTo + 1 次主帧）
  ├─ RenderPrimitiveCollectSystem::ResolveMaterialProgramForPrimitive
  │    ├─ shadow pass → ResolveShadowCasterProgram（MaterialComponent 双槽：shadow_program）
  │    └─ 主帧        → ResolveForwardProgram（forward 槽）
  ├─ 预扫描 prescan：any_material_work / materialize_epoch（P1-1 全干净帧快路径闸门）
  └─ 主循环：masked caster 纹理行未就绪（!valid）→ 跳过本帧深度绘制
     + BumpStaticSceneRevision（借 A3 链触发下帧级联重画，收敛）

batch（每 pass 重建）
  ├─ MaterialBatch 合批键 = shader + pipeline（同键合批为一条
  │    vkCmdDrawMeshTasksIndirectEXT multi-draw，gl_InstanceIndex = 行表行号）
  ├─ WriteBatchIndexRows：行表 SSBO 每行 {payload_index, texture_reference_index}
  │    （4-ID desc 优先，MaterialComponent 回退）
  │    └─ batch.texture_reference_base_addr = 纹理配置池基址（每帧从 comp 幂等设置）
  └─ PipelineMaterialRenderer::Render：PushRootAddresses 每 batch 一次
       （pc_root：addr_mesh_draw_params / addr_l2w / addr_mtl_data_addrs
         / addr_texture_references / ...）

GPU 侧寻址（两级）
  MTL_ROW(i) = global_addresses.addr_pbr_surface
             + values[pc_root.addr_mtl_data_addrs 行表][(i)].payload_index * stride
  MTL_TEX(i) = pc_root.addr_texture_references
             + values[...][(i)].texture_reference_index * stride
  → 池基址任一为 0 即静默 fallback（SampleOptional tex_ref.x==0 → 1.0）
```

要点：
1. **行内容是 per-primitive 共享状态**，与 program 无关——forward 物化链写的行，
   shadow program 共读（双槽设计的基础）。
2. **行表/配置池的写入与上传完全由 forward 物化链持有**——阴影帧不得代为物化
   （两条链会互踢纹理配置行：retire+重分配导致池行漂移）。
3. **pipeline 复用必须校验 program 身份**（`resolvedRuntimePipelineProgramMap`）——
   shader 更新会生成新 program 对象，仅按 RenderPass 键控会永久复用旧 SPIRV。

---

## 3. 修复因果链（由表及里共 7 层）

按发现顺序（每层修复后暴露下一层）：

| # | 层 | 问题 | 修复 |
|---|----|------|------|
| 1 | 模板路由 | masked 程序解析时 `block-order invalid`（注入的 Extension/Resource 块违反 ShaderDocument 单调序）→ Lit 定义整体构建失败 → 首帧 resolve 全失败 → 深度图缺物体 → **静态滚动缓存固化"无影"** | 注入只保留 scene_ubo include（排在 SurfaceInterface 后，SCENE_SET 宏依赖）；pc_root/扩展由 BuildMaterialStageDocument 对所有程序注入 |
| 2 | 片元组成 | `si` redefinition——手写 `SurfaceInput si` 初始化与 wiring（BuildGLSLMaterialSurfaceInput 固定输出完整声明）重复 | wiring 优先，仅 fragment_inputs 为 null 时手写兜底 |
| 3 | alpha 语义 | `EvalMaterialAlpha` 采样的是 **opacity_mask optional 槽**（fallback 1.0），不是 base_color.a | 示例绑 "opacity_mask" 槽；alpha 判定读 .r |
| 4 | **forward 管线** | **forward alpha test 从未接线**——HGLApplyAlpha 只有 shadow 模板调用；forward_lit.glsl.tmpl 靠 HGLComposeColor(color.a)，PBR 光照输出 alpha 恒 1 | 模板加 `#ifdef HGL_ALPHA_TEST HGLApplyAlpha(EvalAlpha(si, materialDataIndex)) #endif` |
| 5 | 行寻址 | `EvalAlpha(si, 0u)` 硬编码行表第 0 行（batch 内错物体） | 改用 wiring 生成的 `materialDataIndex`（fragDataIndexID，coverage 的 DataIndexID 语义驱动） |
| 6 | batch 地址 | `batch.texture_reference_base_addr`（= pc_root.addr_texture_references）只在 desc 未解析的 fallback 分支设置——稳态恒 0 | 与 resolved 分支解耦，每 item 幂等设置 |
| 7 | **pipeline 组装（终极）** | **depth-only 快速路径无条件剥片元 stage**——discard 从未进 VkPipeline | ShaderProgram::IsFragmentShaderRequired（SPIRV 扫描）+ CreatePipeline keep_fragment_shader 豁免 |

附带修复：
- **pipeline 复用加 program 身份键控**（resolvedRuntimePipelineProgramMap）——
  shader 更新生成新 program 对象后，旧 SPIRV 不再被永久复用
  （取证时三版片元深度图逐像素一致的第二原因）。
- **masked caster 行未就绪策略**：撤销"阴影帧借道 forward 物化"（两链互踢纹理
  配置行），改为跳过本帧 + BumpStaticSceneRevision 触发重画（收敛）。
- **WriteBatchIndexRows 行表探测诊断**（TEMP，已还原）确认 CPU 行表全程正确——
  排除数据链，收窄到 GPU 侧。

---

## 4. 取证方法（按有效性排序）

### 4.1 级联深度图直接读回（最有效，AlphaTestShadow 内置）

`AlphaTestShadow::DumpCascadeDepth`（第 45 帧自动执行，或参考其实现）：
immediate submit（**graphics queue**——depth aspect 的 CopyImageToBuffer 需要
GRAPHICS capability，transfer queue 会被 VVL 拒）→ CopyImageToBuffer →
8bit 灰度 BMP 落盘（工作目录）。

**2026-09-26 起自动判读（D1 契约）**：同一次读回顺带统计 c0 的**非零像素
包围盒内**填充率，打印 `[D1-CONTRACT] c0 PASS/FAIL`；`ATS_SELFCHECK=1`（或
`--selfcheck`）时按退出码结束（0=PASS / 1=FAIL），可直接当回归门：

```
ATS_SELFCHECK=1 ./build/out/Windows_64_Debug/AlphaTestShadow.exe; echo $?
[DepthDump] cascade_depth_c0.bmp saved (1024x1024)
[D1-CONTRACT] c0 PASS: bbox=112x58 filled=3740 填充率 57.6% (期望 50-65%)
[D1-CONTRACT] selfcheck: PASS (exit 0)
```

判读基准（棋盘 cube，CSM0）——**必须是包围盒内占比**：c0 包围盒实测
112x58（占全图仅 0.62%），整图口径会把 57.6% 稀释成 0.4%：

| 包围盒内填充率 | 含义 |
|----------------------------------|------|
| 50-65%（实测 57.6%） | 棋盘镂空生效（白格挡光黑格透光 + 多面投影叠加） |
| ~100% | 实心——片元被剥或 opacity 采样恒 1.0 |
| 全空 | 片元被剥 + 全 discard / 物体未进深度图 |

源码层判据链由 `TestCSMIncrementalPass` 的 **Test 11**（10 条 needle）把守，
不需要设备：剥离点检查 `keep_fragment_shader`、recipe 语义（`alpha_test`/
`dither`）参与判据、`fragment_shader_required` 有生产者、masked caster 模板真
评估 alpha。两层合起来才能覆盖"判据被改回"（Test 11 抓）与"链路实际失效"
（AlphaTestShadow 抓）。

### 4.2 SPIRV 反汇编（验证 discard/采样是否真的编译进）

- **stage 缓存容器**：`shader-cache/stage/stage-16-*.spv`（16=fragment、128=mesh）。
  容器头 40 字节（magic 'ULSP' LE `50 53 4c 55` + kind + header_size + stage +
  key_digest + payload_size + payload_hash），**SPIRV 从 offset 40 起**，
  magic LE `03 02 23 07`。
- `spirv-dis` 反汇编后 grep `OpDemoteToHelperInvocation` / `OpKill`（discard）、
  `OpImageSample` / `OpImageDref`（采样）。
- **program meta**（`program-<hash>.meta`）内含引用的 stage hash 清单——
  可确认 program ↔ SPIRV 的对应关系。

### 4.3 行表 / push constant 探测（CPU 侧）

- `WriteBatchIndexRows` 加临时日志：行表内容（payload/texref）+ comp 的
  data_index_row / 纹理配置行号 / zero_row_gpu。
- `PipelineMaterialRenderer::Render` 的 PushRootAddresses 前加日志：
  rows_buf 指针 + texture_reference_base_addr + material_is_mesh。
- 两者组合可完整验证 pc_root 两级寻址的 CPU 输入。

### 4.4 渲染策略

- **A/B 对比前先冻结动画**（movable 物体动画会污染像素差分，曾产生过
  "镂空确认"的假证据）。
- 地面上看影子不如**直接 dump 深度图**——地面棋盘/环境光/透视压缩都会
  干扰判读。

---

## 5. 经验教训

1. **因果屏蔽链**：一处结构性缺失（FS 剥除）会屏蔽所有下游修复的正确性验证。
   修复若干层后现象不变时，应当怀疑还有更靠近 GPU 组装层的根因，而不是在
   同一层继续找细节。
2. **RenderDoc 对 mesh-shading + indirect draw 的 GLSL 反推不可用**；
   descriptor buffer 模式下 resources 显示也不完整——CPU 侧行表/地址探测 +
   GPU 读回是本项目更可靠的组合。
3. **"看起来镂空"不可靠**：ClearColor 深灰 vs 黑格实心在截图上难以区分，
   必须用像素统计或深度图读回定量。
4. **静态滚动缓存的固化放大效应**：任何"首帧某物体缺席深度图"的瞬时故障
   都会被缓存固化成永久故障——A3 revision 失效链 + 固化防御（失败即 bump）
   是兜底组合。
5. **工作目录敏感**：GLSLCompiler.dll 等插件在仓库根，从 exe 目录跑会
   `cannot load GLSLCompiler plugin module`。Git Bash 下从仓库根运行示例。
6. 共享头（PrimitiveComponent.h 等）改动后 `--clean-first`，否则 ABI 错位
   假崩溃。

---

## 6. 相关提交（CSM 分支）

| 提交 | 内容 |
|------|------|
| `89323c651` | depth-only 通道按片元 discard 豁免 FS 剥离（终极根因） |
| `fcbe4d6ce` | pipeline 复用校验 program 身份（防旧 SPIRV 永久复用） |
| `a52bf630c` | WriteBatchIndexRows 行表内容诊断块（TEMP-DIAG，第 0 步已删除） |
| `b337d8a50` | masked caster 行未就绪跳过 + 固化防御 |
| `9b364a6f8` | AlphaTestShadow 深度图读回取证工具 |
| `3bd9d0c02` | AlphaTestShadow 用例 + A1-4 片元/collect 侧修复 + **纹理引用池基址与 4-ID 解析分支解耦**（base_addr 的实际归属，提交信息未提及）（用户提交） |

相关文档：`doc/csm-review-2026-09-25.md`（A1/A4/A2/A3/A1-4 的来源）；
`.ai/skills/SKILL_CASCADED_SHADOW_CSM.md`（ShadowCasterMasked 链路章节，
§4.5）；
`doc/backlog.md` **D 线**（本轮后续：深度镂空自动契约/pipeline 键内容
hash/TransformComponent 同值短路/性能账目→拆分→合并）与 **A 线 A1/A7**
（提交原语/in-flight 槽——与本链的 fence 等待问题强相关）。
