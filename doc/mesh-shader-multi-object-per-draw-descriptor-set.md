# MeshShader 多对象渲染问题排查与修复（AtmosphereSkyAmbient）

## 1. 背景

MeshShader 迁移（VS 彻底废弃、mesh shader 唯一顶点路径）推进到全示例验证阶段。
AtmosphereSkyAmbient 是环境示例——动态创建 **5 个独立 mesh**（Sphere/Cube/Cone/Torus/Capsule，
各有独立 GPU buffer）+ 1 个天空盒，是迁移清单中第一个**多对象独立 buffer** 场景。

验证中发现两个串联问题：

1. **多 mesh 全部显示成最后一个添加的 Capsule**（形状错乱）
2. 修复后**模型形状正确但位置重叠**（l2w 数据"位置重复"）

两个问题性质完全不同：前者是**引擎渲染管线 bug**（descriptor set 生命周期语义），
后者是**示例代码布局 bug**（位置数据设置错误）。本文分别记录排查过程、根因与解决。

---

## 2. 问题一：多 mesh 全部显示成同一个（Capsule）

### 2.1 症状

- 5 个 mesh 全部显示成**最后添加的 Capsule** 的形状
- 每个 draw 的顶点数/索引数正确（各自模型的）
- 位置分散正确（各自 transform），但形状全是 Capsule

### 2.2 排查过程（排除法）

| 假设 | 验证 | 结论 |
|---|---|---|
| 所有 mesh 写到同一 GPU 地址（数据覆盖） | 每 VAB 独立 `CreateStagedBuffer`（独立 VkBuffer + 独立 memory） | 排除 |
| 绘制时全部绑定同一个 buffer | `[PMR-BIND]` 日志：每 draw 的 `vab0/1/2`（Position/UV/NTB）句柄**各自不同** | 排除 |
| 数据层错误（buffer 内容相同） | RenderDoc：两个模型的 position SSBO 数据确实不同 | 排除 |
| batch 合并误判（`GeometryDataBuffer::operator==`） | `operator<=>` 按 vab_list 句柄数组 `memcmp` 比较，不同 buffer 不相等 | 排除 |

### 2.3 决定性证据（RenderDoc api.txt）

VK API 调用列表（Frame #78）显示：

```
EID 6-18   vkUpdateDescriptorSets ×10   ← 所有 descriptor 更新都在提交前
EID 26-70  vkCmdBindDescriptorSets / vkCmdDrawMeshTasksEXT   ← 渲染命令里 0 条更新
```

- **渲染命令记录期间没有一条 `vkUpdateDescriptorSets`**
- 所有 draw 绑定的是**同一个 descriptor set**（`DescSet:program-...:PerObject`）
- set 内容 = 提交前最后一次更新的状态

### 2.4 根因：Vulkan descriptor set 是状态，不是快照

```
material 级单 PerObject set（material->GetMP(PerObject)）
        │
每 draw 顺序更新（BindSSBO → vkUpdateDescriptorSets）
   draw1: set 内容 = Capsule 的 buffer    ← 写入
   draw2: set 内容 = Cone 的 buffer       ← 覆盖
   draw3: set 内容 = Torus 的 buffer      ← 覆盖
   ...
   提交时：所有 draw 读同一个 set → 最后一次覆盖的内容（Capsule）
```

**Vulkan 语义**：descriptor set 是**状态对象**（不是快照）。同一个 set 被多次更新、
多个 draw 引用时，**所有 draw 在提交时刻看到的是最后一次更新的内容**。
`vkCmdBindDescriptorSets` 绑的是 set 句柄，命令执行时读取 set 的**当前**内容。

### 2.5 为什么其它示例不受影响

| 示例 | 场景 | 为什么正常 |
|---|---|---|
| SimpleCube | 单 mesh | 只有一个 draw，单 set 一次更新 ✓ |
| DrawTriangle / LineRenderTest | 单对象/单线段集 | 同上 |
| RenderBoundBox | 多对象**共享 VDM buffer** | descriptor 内容恒同（同一个 VDM buffer），对象区分走 **push constant 段偏移**（vertex_base/TransformID 查表）——descriptor 不需要切换 |
| **AtmosphereSkyAmbient** | **多对象独立 buffer** | descriptor 必须 per-draw 切换 → 单 set 覆盖 → **暴露** |

### 2.6 解决方案：per-draw 独立 descriptor set

**设计**：每个 DrawBatch 拥有**独立的 PerObject MP（MaterialParameters + descriptor set）**，
bind_ssbo 更新各自的 set、绑定各自的 set——提交时每个 draw 读自己的 set。

**实现**（7 处）：

| 文件 | 改动 |
|---|---|
| `PipelineMaterialRenderer.h` | `DrawBatch` 加 `per_object_mp`；成员加 `per_object_mp_pool`（跨帧复用，避免每帧分配）；`MaterialParameters` 前向声明 |
| `PipelineMaterialRenderer.cpp` | Render 主循环：池取用/惰性创建（`owner_batch->device->CreateMP(base_mp->GetDescManager(), material->GetPipelineLayoutData(), PerObject)`）；bind_ssbo 全部绑定（Position/UV/NTB/Color/Luminance/TransformID/Size/Index/rows）→ `batch->per_object_mp`；`Update()` + `BindDescriptorSets` → per-draw set；析构释放池 |
| `VKMaterialParameters.h` | 加 `GetDescManager()`（克隆 MP 需要） |
| `TransformAssignmentBuffer` | 加 `BindTransform(MaterialParameters*)` 重载（ShaderProgram 版委托）——l2w 绑到 per-draw set |

**效果**：多对象独立 buffer 每 draw 独立 set 各自更新+绑定 → 提交时每 draw 读自己的 buffer。
VDM 共享 buffer 场景内容相同无副作用；单 draw 场景池只用 1 个，行为不变。

---

## 3. 问题二：模型正确但位置重叠（l2w "位置重复"）

### 3.1 症状

- 5 个 mesh 形状、颜色、朝向全部正确
- 但部分 mesh 位置重叠（"位置不对"）
- RenderDoc 查看 l2w SSBO：**有些矩阵的平移分量相同**

### 3.2 排查过程

1. **用户排除 GPU**：RenderDoc 显示 GPU 侧绑定/数据正常
2. **引擎链路验证**（TransformSystem → TransformAssignmentBuffer 全链）：

| 环节 | 逻辑 | 验证 |
|---|---|---|
| `RefreshHandleOrder` | handle → group_index（排序位置，每帧重建 map，唯一） | ✓ |
| `AssignTransformIndices` | `transform_index = IsMovable() ? dynamic_base+group : group+1` | ✓ 唯一 |
| `WriteTransformIndexRows` | rows 表 = dynamic_base+i（与 Assign 同 base） | ✓ 一致 |
| `WriteStaticDirtyIndices` | l2w 槽 = 1+i（静态） / base+i（动态 ring） | ✓ 数学无重叠 |
| ring buffer | base = static+1+frame×dynamic，capacity 含 ring 帧数 | ✓ |

   数学上**不可能**产生重复槽位——引擎侧排除。

3. **l2w.csv 数据分析**（用户导出，8 个槽）：

| 槽 | 旋转 | 平移 (x,y,z) | 分析 |
|---|---|---|---|
| 0 | identity | (0,0,0) | 预留 |
| 1 | identity | (0,0,0) | sky（原点） |
| 2 | 0° | (4.50, 1.00, 0) | mesh① |
| 3 | 72° | (1.39, 1.00, 0) | mesh② |
| 4 | 144° | (-3.64, 1.00, 0) | mesh③ |
| 5 | 216° | (-3.64, 1.00, 0) | mesh④ |
| 6 | 288° | (1.39, 1.00, 0) | mesh⑤ |
| 7 | 全零 | (0,0,0) | 未使用（capacity 8 对齐） |

   **关键观察**：槽 3≈槽 6 平移相同（1.39）、槽 4≈槽 5 相同（-3.64）、**所有 y=1.00**。
   这是五边形顶点的 **x 投影**（4.5·cos72°=1.39、4.5·cos144°=-3.64），
   y 应为 4.5·sin(±72°)=±4.28、4.5·sin(±144°)=±2.64——**却统一是 1.00**。

### 3.3 根因：示例代码 `pos.y = 1.0f` 覆盖五边形顶点 y

`AtmosphereSkyAmbient.cpp` 原代码：

```cpp
float angle = glm::radians(360.0f * index / count);              // 72° 间隔
glm::quat rotation = glm::angleAxis(angle, vec3(0,0,1));          // z 轴旋转
glm::vec3 pos = glm::rotate(rotation, vec3(4.5f, 0.0f, 0.0f));   // xy 平面旋转 (4.5,0,0)
pos.y = 1.0f;   // ← 把五边形顶点的 y 全部覆盖成 1.0
```

- 本意：5 个 mesh 在 xy 平面五边形顶点（`(4.5,0) / (1.39,4.28) / (-3.64,2.64) / (-3.64,-2.64) / (1.39,-4.28)`——5 个不同位置），`pos.y=1.0f` 抬高
- 实际：`pos.y = 1.0f` **无条件覆盖** → 位置变成 x 投影 `(4.5,1.0) / (1.39,1.0) / (-3.64,1.0) / (-3.64,1.0) / (1.39,1.0)`——**只有 3 个唯一 x** → 槽 3≈槽 6、槽 4≈槽 5 重叠
- **l2w.csv 数据如实反映了 SetLocalPosition 的设置**——Matrix4f 生成、上传、绑定全部正确

### 3.4 解决方案：改为 xz 平面环绕

```cpp
float angle = glm::radians(360.0f * index / count);
// 水平环绕（xz 平面圆上 5 点）+ 高度 1.0——位置互不重叠；
// 绕 y 轴旋转（法线方向各不相同 → sky ambient 方向采样差异可见）
glm::quat rotation = glm::angleAxis(-angle, glm::vec3(0.0f, 1.0f, 0.0f));
glm::vec3 pos = glm::vec3(4.5f * glm::cos(angle), 1.0f, 4.5f * glm::sin(angle));
```

- 5 个 mesh 在水平圆上（xz 平面）均匀分布 + 高度 1.0——**位置互不重叠**
- 绕 y 轴旋转——法线朝向各异——保留原注释意图（"与 sun 方向有夹角 → sky ambient 方向采样差异可见"）

---

## 4. 教训与经验

### 4.1 descriptor set 生命周期语义（引擎级）

- **Vulkan descriptor set 是状态对象，不是快照**——同一 set 被多次更新 + 多 draw 引用时，提交时刻所有 draw 读最后一次内容
- **适用场景判断**：
  - 单 draw / 共享 buffer（VDM 多实例）→ 单 set 即可（对象区分走 push constant 段偏移）
  - **多对象独立 buffer** → 必须 **per-draw 独立 set**
- **调试特征**：C++ 端每 draw 绑定调用参数正确（日志），但 RenderDoc 显示所有 draw 同一份 buffer——**必是 descriptor 更新时机/生命周期问题**（渲染命令里 0 条 `vkUpdateDescriptorSets` 是决定性信号）

### 4.2 数据驱动调试的价值

- 本问题两个阶段都用**数据**定位：`[PMR-BIND]` 日志（绑定参数）+ `api.txt`（VK API 调用序）+ `l2w.csv`（SSBO 内容）
- **先看数据是否符合设置，再怀疑引擎**——l2w.csv 显示 y=1.00 全部相同，反查示例代码发现 `pos.y = 1.0f` 写死——引擎链路（分配/写入/绑定）全部正确

### 4.3 示例与引擎的边界

- 引擎侧正确性要用"链路完整性"验证（分配唯一性、base 一致性、写入槽位数学无重叠）
- 示例侧的布局意图（五边形/环绕）与实现（旋转平面/高度覆盖）要对照检查——**几何布局的数学投影错误**（y 覆盖导致 x 重叠）是示例代码的典型问题

---

## 5. 关联代码位置

| 模块 | 位置 | 说明 |
|---|---|---|
| per-draw set 池 | `src/ecs/support/PipelineMaterialRenderer.cpp`（Render 主循环、bind_ssbo、析构） | DrawBatch 独立 PerObject MP |
| DrawBatch 扩展 | `inc/hgl/ecs/support/PipelineMaterialRenderer.h` | `per_object_mp` + `per_object_mp_pool` |
| MP 克隆 | `inc/hgl/vk/VKMaterialParameters.h`（`GetDescManager`） | 克隆所需 |
| l2w 绑定 | `src/ecs/support/TransformAssignmentBuffer.cpp`（`BindTransform(MaterialParameters*)`） | l2w 绑到 per-draw set |
| transform 分配链 | `src/ecs/systems/tick/TransformSystem.cpp`（RefreshHandleOrder/SubmitTransformUpdates）、`src/ecs/support/PrimitiveBatchPipeline.cpp`（AssignTransformIndices） | handle → transform_index → l2w 槽 |
| 示例修复 | `example/Environment/AtmosphereSkyAmbient.cpp`（InitECS 环绕分布） | xz 平面环绕 + y 轴旋转 |
