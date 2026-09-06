# Vertex BDA 化 v2 计划（超细粒度版）

基线：`62159109a`（全部示例正常，gate 41/41）
分支：`vertex-bda-v2`
核心纪律：**每个步骤改完必须过三道验证才能进下一步**
1. C++ 编译零错误
2. gate 41/41
3. **glslangValidator 直编 dump**（改了 GLSL 的步骤）
4. BasicLitMeshes 运行正常（改了运行时的步骤）

每步一个 commit，出问题 `git revert HEAD` 即回退。

---

## Phase A：基础设施（纯 C++，零 GLSL 改动）

### A1. VAB/IBO 的 BDA usage flag（~20 行）
- [ ] `CreateVAB` staged 路径 usage 加 `SHADER_DEVICE_ADDRESS_BIT`
- [ ] `CreateVAB` rebar 路径同上
- [ ] `CreateIBO` 两条路径同上
- [ ] `CreateStagedBuffer` 设备侧内存按 usage 自动加 `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT`
- [ ] 私有 `CreateBuffer(mem_usage)` 路径同上
- 验证：编译 + gate + BasicLitMeshes 跑一帧

### A2. GetBufferDeviceAddressAligned16 辅助（~15 行）
- [ ] `VulkanDevice` 新增方法（取地址 + %16 断言 + 错误日志）
- 验证：编译（无消费者）

### A3. MeshDrawParams 扩 8 个基址字段（~40 行）
- [ ] X-macro 加 8 个 `uint64_t addr_*` 字段
- [ ] 布局断言重写（显式 offsetof 检查 + sizeof=88）
- [ ] GLSL 侧 struct 发射自动跟随（遍历 X-macro）
- [ ] `WriteMeshDrawCommands` 写入基址（从 GeometryDataVDM→VDM→GetVAB 取）
- [ ] Text/Line 两个单行写入点补 0 或实际地址
- [ ] VDM 加 `GetVABStreamCount()` getter
- 验证：编译 + gate + BasicLitMeshes 渲染不变（shader 不消费基址，纯双写）

---

## Phase B：GLSL 垫片（每步只改 1-2 个文件，glslangValidator 验证）

### B0. adapter 补扩展（~3 行）
- [ ] `MeshShaderVertexAdapter.h` 的 int64 扩展行确认在位
- 验证：编译 + gate + BasicLitMeshes

### B1. s1_position_vec3 垫片（第一个，最关键）
- [ ] 声明改为：
  ```glsl
  #ifdef ULRE_MATERIAL_ARENA_BDA
  layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionDataRef
  {
      vec3 data[];
  };
  #define sbo_vertex_position VertexPositionDataRef(pc_vertex_index.addr_position)
  #else
  ...原声明...
  #endif
  ```
- [ ] **手动编写，不用脚本**
- [ ] `LoadVertexData()` 函数体不动
- [ ] 加地址守卫：`addr==0` 时 Position=探针值 return
- 验证：glslangValidator 编译 dump 零错误 + BasicLitMeshes 渲染
- **如果渲染坏了，此步单独 revert**

### B2. s1_uv（BasicLitMeshes 第二个依赖）
- [ ] 同 B1 模式（`vec2 data[]`，B1 的 Ref 类型声明因 s1_uv 与 s1_position 同 TU 不冲突——不同块名）
- 验证：同 B1

### B3. s1_ntb_rg8（BasicLitMeshes 第三个依赖）
- [ ] 同 B1 模式（`uint data[]`）
- 验证：同 B1

### B4. BasicLitMeshes 完整验证
- [ ] 全示例走查
- [ ] gate 41/41
- **里程碑：BasicLitMeshes 走 BDA 顶点路径**

### B5-B12. 其余 s1 模块（每次 1 个文件，逐个验证）
- [ ] s1_uv_rg16f（`uint data[]`）
- [ ] s1_color（`vec4 data[]`）
- [ ] s1_ntb / s1_ntb_rg16f / s1_ntb_a2bgr10（各按实际元素类型）
- [ ] s1_palette_index（`uint data[]`）
- [ ] s1_luminance（`uint data[]`）
- [ ] s1_transform_id / s1_size / s1_position_vec2 / s1_position_vec2i
- 每个验证：glslangValidator + 用到该语义的示例渲染

---

## Phase C：Vertex 集退场（纯删除）

### C1. ShaderGen 描述符链删除
- [ ] `DefinitionDescriptorBuilder` 的 need_*/PushVertexResource 块
- [ ] `DescriptorResourceCatalog` VertexGeometry 8 行
- [ ] `ShaderBufferSources` SBS_Vertex* 8 行
- [ ] `MaterialShaderCompiler` VertexGeometry case
- [ ] `ShaderBuildContext::AddSSBOVertex*`
- 验证：编译 + gate（golden 需重生成）

### C2. 枚举/宏删除
- [ ] `VertexBinding` 枚举 + 锚点断言
- [ ] 宏表 VERTEX_*_BINDING 8 行
- [ ] `DescriptorMacroGen --emit` 重生成
- [ ] `DescriptorSetType::Vertex` 枚举值
- 验证：编译 + gate + DescriptorMacroGen 交叉校验

### C3. 运行时绑定删除
- [ ] `PipelineMaterialRenderer` Vertex 集绑定块 + vertex_mp 池
- [ ] `LineRenderPipeline` Vertex 绑定
- [ ] `RDBS` VertexGeometry case
- 验证：编译 + 全示例

### C4. golden 重生成
- [ ] 5 个 golden 全部重生成
- [ ] gate 41/41
- **里程碑：Vertex 集完全退场**

---

## Phase D：texture_layer_rows 消灭 + Material 集退场

### D1. UnlitTexture 挂行
### D2. Text 挂行
### D3. 双轨删除
### D4. DescriptorSetType::Material 退场
（详见 v1 计划 W4，此处不展开）

---

## 经验教训（v1 的坑，v2 必须避免）

| # | 教训 | 对策 |
|---|---|---|
| 1 | 批量脚本改 GLSL 丢失 struct 体 | **禁用批量脚本改 GLSL**；逐文件手写 |
| 2 | 同名 buffer_reference 块在不同模块冲突 | 每模块的 Ref 类型名带模块语义前缀，或只声明一次 |
| 3 | 元素类型与解码逻辑不匹配 | 从各模块的 legacy 声明原样复制元素类型 |
| 4 | adapter 统一声明与模块声明冲突 | adapter 只做扩展，不做类型声明 |
| 5 | BOM/换行/转义污染 | Python 写 GLSL 用 `encoding='utf-8'`（非 utf-8-sig），newline 保持 |
| 6 | 缓存不失效 | 每次 GLSL 变化后 `SHADERGEN_CODEGEN_GENERATION` +1 |
| 7 | GPU fault 无法定位 | shader 内加地址守卫 + ULRE_ARENA_DEBUG 输出实际写入值 |
