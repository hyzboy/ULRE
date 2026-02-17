# BufferPolicy Schema Documentation v1

## 字段说明与可选值

### Name
**含义**：策略类别的名称，用于日志和识别
**数据类型**：String
**示例**：CameraUBO, DynamicTransformVAB, ParticleData
**备注**：必填，应具有可读性

---

### Usage
**含义**：Buffer 的 Vulkan 用途类型
**可选值**：
- `UBO`：Uniform Buffer Object（只读常数缓冲）
- `SSBO`：Storage Buffer Object（读写结构化缓冲）
- `VAB`：Vertex Attribute Buffer（顶点属性，VBO）
- `IBO`：Index Buffer（索引缓冲）
- `TEX_TILE`：Texture Tile / Streaming（纹理/高度图等瓦片）
- `RAW`：Generic buffer（通用缓冲，无特定用途）
**示例**：UBO, SSBO, VAB
**备注**：必填；影响内存访问模式

---

### Priority
**含义**：提交队列中的优先级（越高越先提交）
**可选值**：
- `CRITICAL`：最高；必须优先提交（如 Camera、Dynamic Transform）
- `HIGH`：高优先级（如 Viewport、Static Transform）
- `NORMAL`：普通优先级（如 Dynamic Mesh）
- `LOW`：低优先级（如 Particle、Tile）
**示例**：CRITICAL, NORMAL, LOW
**备注**：必填；用于队列排序和预算分配

---

### UpdateRate
**含义**：更新频率（决定是否适合走 Ring Buffer）
**可选值**：
- `PER_FRAME`：每帧必须更新（如 Camera、Dynamic Transform）
- `FREQUENT`：频繁更新，多帧内多次（如 Dynamic Mesh）
- `BURST`：突发更新，一帧集中多次写，然后长期不变（如 Static Transform）
- `SPARSE`：稀疏更新，偶尔改变（如 Viewport）
- `RARE`：极少更新，几乎一次性（如 Static Mesh）
**示例**：PER_FRAME, FREQUENT, BURST
**备注**：必填；影响内存策略（REBAR vs Ring vs Staged）

---

### SubmitTiming
**含义**：提交的时间要求
**可选值**：
- `IMMEDIATE`：立即提交（CPU 完成写入后立刻入队）
- `SAME_FRAME`：同帧必须提交（写完后需在当前帧渲染前完成提交）
- `NEXT_FRAME_OK`：可以延迟到下一帧提交（如果超预算可推后）
- `DEFERRED`：随意延迟（无硬性时间要求）
**示例**：SAME_FRAME, NEXT_FRAME_OK, DEFERRED
**备注**：必填；定义队列优先级与延迟耐受度

---

### MaxLatency
**含义**：允许的最大延迟（即最多可推迟多少帧才必须提交）
**数据类型**：Integer（帧数）或 `AUTO`
**示例**：0f, 1f, 2f, 4f, AUTO
**取值范围**：
- `0f`：无延迟，当前帧必须提交
- `1f`：允许延迟 1 帧
- `2f`：允许延迟 2 帧
- `AUTO`：由系统根据优先级推断（HIGH→1f, NORMAL→2f, LOW→4f）
**备注**：必填；用于防止"长期排队饿死"问题

---

### BudgetGroup
**含义**：预算组名（同组共享字节上限）
**可选值**：
- `GLOBAL`：全局预算（所有缓冲共用）
- `TRANSFORM`：变换相关（Transform + MaterialInstance）
- `MESH`：网格相关（VBO/IBO）
- `TILE`：瓦片相关（Texture/Height Map）
- `PARTICLE`：粒子相关
- `CUSTOM`：自定义组名
**示例**：TRANSFORM, MESH, PARTICLE
**备注**：用于"预算有限时的组内优先级竞争"

---

### BudgetLimit
**含义**：预算组的最大提交字节数（同组所有 Buffer 在一帧内提交的总字节不超过此值）
**数据类型**：Bytes 或 `AUTO`
**示例**：8M, 16M, 256K, AUTO
**单位**：
- `K` = 1024 bytes
- `M` = 1024K
- `AUTO` = 系统默认（TRANSFORM: 32M, MESH: 64M, PARTICLE: 16M, 等）
**备注**：必填；若超过则触发 `DropPolicy` 或延迟

---

### Queueing
**含义**：是否参与队列化、受预算限制
**可选值**：
- `ENABLED`：参与队列化，受预算控制
- `DISABLED`：跳过队列，立即提交（用于 MANUAL 模式）
**示例**：ENABLED, DISABLED
**备注**：必填；一般只有 ManualSpecial 才设为 DISABLED

---

### SplitPolicy
**含义**：当单次更新超预算时，是否允许拆分成多次提交
**可选值**：
- `NO_SPLIT`：不拆分；超预算时选择延迟或丢弃（如 Mesh、Transform）
- `ALLOW_SPLIT`：允许拆分；大更新分成多个较小的提交（如 Particle）
- `PREFER_SPLIT`：优先拆分；尽量拆分以减少延迟（如 Tile）
**示例**：NO_SPLIT, ALLOW_SPLIT
**备注**：与 `SplitChunk` 配合使用

---

### SplitChunk
**含义**：拆分时每个块的最大字节数
**数据类型**：Bytes 或 `AUTO`
**示例**：256K, 1M, AUTO
**备注**：仅在 `SplitPolicy` != `NO_SPLIT` 时有效；AUTO 表示由系统推断

---

### DropPolicy
**含义**：当队列超出预算且无法拆分/延迟时的丢弃策略
**可选值**：
- `NEVER`：永不丢弃（如 Camera、Transform）；改用强制提交或阻塞
- `DROP_OLD`：丢弃旧的未提交数据，用新数据覆盖（如 Dynamic Mesh）
- `DROP_NEW`：丢弃**新数据**，保留已排队的旧数据
**示例**：NEVER, DROP_OLD
**备注**：必填；用于容量溢出的最后兜底

---

### DeadlinePolicy
**含义**：超过 MaxLatency 后的反应策略
**可选值**：
- `NONE`：无硬性截止
- `SOFT`：超期后自动提升优先级（软截止）
- `HARD`：超期后强制立即提交（硬截止）
**示例**：SOFT, HARD, NONE
**备注**：HARD 用于 CRITICAL；SOFT 用于 NORMAL；NONE 用于 LOW

---

### Deadline
**含义**：具体的截止时间（超过后触发 `DeadlinePolicy`）
**数据类型**：Integer（帧数） 或 Integer + 单位（如 `16ms`） 或 `AUTO`
**示例**：0f, 1f, 2f, 4f, 16ms, AUTO
**说明**：
- `0f` = 当前帧
- `1f` = 下一帧
- `16ms` = 16 毫秒（实时计算）
- `AUTO` = 根据 UpdateRate 推断（PER_FRAME→0f, FREQUENT→1f, BURST→2f）
**备注**：应 >= MaxLatency

---

### PromotePolicy
**含义**：当满足 `PromoteRule` 时的反应
**可选值**：
- `NONE`：不响应
- `AUTO_RAISE`：自动提升优先级一级（LOW→NORMAL→HIGH）
- `FORCE_HIGH`：强制设置为 HIGH（最激进）
**示例**：AUTO_RAISE, FORCE_HIGH, NONE
**备注**：用于动态避免饿死

---

### PromoteRule
**含义**：触发提升的条件表达式
**数据类型**：String（条件表达式）
**示例**：
- `latency>2f` = 延迟超过 2 帧
- `queue>80%` = 队列占用 > 80%
- `latency>1f && priority<HIGH` = 延迟超 1 帧且优先级低于 HIGH
- `always` = 始终条件（用于 CRITICAL）
**备注**：空字符串表示无条件（仅在 PromotePolicy=AUTO_RAISE/FORCE_HIGH 有效时使用）

---

### MemoryPolicy
**含义**：CPU 端内存分配策略
**可选值**：
- `REBAR`：Resizable BAR（CPU 和 GPU 共享寻址空间）；若不支持自动降级
- `RING`：Ring Buffer（固定尺寸循环缓冲，多帧回收）
- `STAGED`：Staged Buffer（CPU 端 staging + GPU 端 device）
- `AUTO`：由系统根据 UpdateRate 推断（PER_FRAME→REBAR, FREQUENT→RING, 其他→STAGED）
**示例**：REBAR, RING, STAGED, AUTO
**备注**：必填；影响 CPU 端访问模式与生命周期

---

### CpuResident
**含义**：CPU 端数据是否保留直到 GPU 消耗
**可选值**：
- `KEEP`：保留（映射指针始终有效）
- `RELEASE`：可释放（提交后可删除）
- `AUTO`：由系统推断（RING/REBAR→KEEP, STAGED one-time→RELEASE）
**示例**：KEEP, RELEASE, AUTO
**备注**：必填；影响应用层内存管理

---

### RingFrameCount
**含义**：Ring Buffer 的回收周期（最多能保持多少帧未提交）
**数据类型**：Integer（帧数）或 `AUTO`
**示例**：2, 3, 4, AUTO
**备注**：仅在 MemoryPolicy=RING 时有效；AUTO 表示系统默认（通常 3）

---

### StagedPersist
**含义**：Staged Buffer 的 CPU 端 staging 缓冲是否保留
**可选值**：
- `KEEP`：保留（方便后续再次提交）
- `RELEASE`：提交后释放
- `AUTO`：由系统推断（频繁更新→KEEP, 一次性→RELEASE）
**示例**：KEEP, RELEASE, AUTO
**备注**：仅在 MemoryPolicy=STAGED 时有效

---

### CommitPolicy
**含义**：自动提交的行为（配合 RawBufferAccessor 执行）
**可选值**：
- `AUTO`：根据 UpdateRate 和 MemoryPolicy 推断
- `STAGED_ONLY`：仅在 staged buffer 标记 dirty 时提交
- `ALWAYS`：每帧都调用 Flush（无论是否 dirty）
- `MANUAL`：由应用手动 Commit/Flush，系统不干预
**示例**：STAGED_ONLY, ALWAYS, MANUAL, AUTO
**备注**：必填；影响无感提交的自动化程度

---

### DevNotes
**含义**：开发备注（用于说明该类别的特殊考虑）
**数据类型**：String（自由文本）
**示例**：
- "UBO; rarely changes; CPU staging kept"
- "Frequent scattered writes; ring, must commit same frame"
- "Large tiles; no split; can defer; allow drop old"
**备注**：可选字段；便于日后审查和维护

---

## 推断规则（AUTO 用法）

当字段设为 `AUTO` 时，系统会根据其他字段推断：

### MaxLatency AUTO
```
Priority=CRITICAL  → 0f
Priority=HIGH      → 1f
Priority=NORMAL    → 2f
Priority=LOW       → 4f
```

### MemoryPolicy AUTO
```
UpdateRate=PER_FRAME  → REBAR (else RING or STAGED fallback)
UpdateRate=FREQUENT   → RING
UpdateRate=BURST      → STAGED
UpdateRate=SPARSE     → STAGED
UpdateRate=RARE       → STAGED (one-time)
```

### BudgetLimit AUTO
```
BudgetGroup=GLOBAL     → system default (e.g., 256M)
BudgetGroup=TRANSFORM  → 32M
BudgetGroup=MESH       → 64M
BudgetGroup=TILE       → 16M
BudgetGroup=PARTICLE   → 16M
```

### CommitPolicy AUTO
```
UpdateRate=PER_FRAME && MemoryPolicy=REBAR      → ALWAYS
UpdateRate=PER_FRAME && MemoryPolicy=RING       → ALWAYS
UpdateRate=PER_FRAME && MemoryPolicy=STAGED     → STAGED_ONLY
UpdateRate=RARE && MemoryPolicy=STAGED          → STAGED_ONLY
否则                                             → STAGED_ONLY (default)
```

### Deadline AUTO
```
UpdateRate=PER_FRAME  → 0f
UpdateRate=FREQUENT   → 1f
UpdateRate=BURST      → 2f
UpdateRate=SPARSE     → 2f
UpdateRate=RARE       → (none)
```

### SplitChunk AUTO
```
SplitPolicy=ALLOW_SPLIT  → 256K (default)
SplitPolicy=PREFER_SPLIT → 128K (more aggressive)
SplitPolicy=NO_SPLIT     → (N/A)
```

---

## 约束和验证规则

### 必填字段
- `Name`
- `Usage`
- `Priority`
- `UpdateRate`
- `SubmitTiming`
- `BudgetGroup`
- `Queueing`
- `SplitPolicy`
- `DropPolicy`
- `DeadlinePolicy`
- `MemoryPolicy`
- `CpuResident`

### 可选字段
- `MaxLatency`（可为 AUTO）
- `BudgetLimit`（可为 AUTO）
- `SplitChunk`（可为 AUTO，仅在 SplitPolicy≠NO_SPLIT 时）
- `Deadline`（可为 AUTO）
- `PromotePolicy`（可为 NONE）
- `PromoteRule`（可为空，仅在 PromotePolicy≠NONE 时）
- `RingFrameCount`（可为 AUTO，仅在 MemoryPolicy=RING 时）
- `StagedPersist`（可为 AUTO，仅在 MemoryPolicy=STAGED 时）
- `CommitPolicy`（可为 AUTO）
- `DevNotes`（自由文本，可空）

### 逻辑约束
1. `DeadlinePolicy=HARD` 时，`Deadline` 不能为空
2. `SplitPolicy=NO_SPLIT` 时，`SplitChunk` 应为 AUTO 或空
3. `Priority` 和 `MaxLatency` 应协调（如 CRITICAL→0f, LOW→4f）
4. `PromotePolicy=NONE` 时，`PromoteRule` 应为空
5. `Queueing=DISABLED` 时，`BudgetLimit` 和 `SplitPolicy` 无效

---

## 示例填表

```ini
[Category]
Name=CameraUBO
Usage=UBO
Priority=CRITICAL
UpdateRate=PER_FRAME
SubmitTiming=SAME_FRAME
MaxLatency=0f
BudgetGroup=GLOBAL
BudgetLimit=AUTO
Queueing=ENABLED
SplitPolicy=NO_SPLIT
SplitChunk=AUTO
DropPolicy=NEVER
DeadlinePolicy=HARD
Deadline=0f
PromotePolicy=FORCE_HIGH
PromoteRule=always
MemoryPolicy=REBAR
CpuResident=KEEP
RingFrameCount=AUTO
StagedPersist=AUTO
CommitPolicy=ALWAYS
DevNotes=Every frame update; prefer ReBAR, fallback to Ring
```

---

## 术语表

| 术语 | 解释 |
|------|------|
| **REBAR** | Resizable BAR：显卡支持的 CPU 与 GPU 共享虚拟地址，性能最优 |
| **Ring Buffer** | 循环缓冲：固定尺寸，N 帧后回收，适合频繁更新 |
| **Staged Buffer** | 暂存缓冲：CPU 端 staging + GPU 端 device，需显式拷贝，延迟最低 |
| **Dirty Flag** | 标记缓冲有未提交的修改，用于决定是否提交 |
| **Drop** | 放弃（丢弃）未提交的旧数据或新数据 |
| **Promote** | 提升优先级（例如 LOW→NORMAL） |
| **Budget** | 预算（单帧最大提交字节数） |
| **Latency** | 延迟（从修改到提交的帧数） |

