# Shader System 下一步执行清单（2026-02-27）

**目标**：在当前“可运行、可渲染、可截帧”的稳定基线上，完成可维护性与可回归能力建设。  
**基线状态**：`06b_BasicLitMeshesECS`、`06c_TextureBlinnPhongMeshesECS` 编译运行通过；Brickwall 三贴图链路稳定；descriptor 生命周期问题已修复。

---

## P0（本周）

### 1) 将 normal strength 升级为运行时参数

**背景**：当前 `ULRE_NORMAL_STRENGTH` 为编译期宏，调参需要重编译 shader。  
**任务**：
- 在 `BasicLitMaterialInstance` 与 `TextureBlinnPhong` 对应实例参数中加入 `normal_strength`（或统一命名）。
- shader 中用材质实例字段替代 `ULRE_NORMAL_STRENGTH` 宏。
- 例子 `06b/06c` 提供一组稳定默认值（建议 `0.30~0.45`）。

**验收标准**：
- 不重编 shader 即可调凹凸强度。
- `06b/06c` 运行无回归，视觉效果与当前默认接近。

---

### 2) 增加 descriptor 生命周期回归用例

**背景**：本轮修复了 descriptor 写入悬挂指针/扩容失稳问题，需自动回归防止复发。  
**任务**：
- 增加测试：连续大量材质/纹理绑定，覆盖 `BindUBO/BindSSBO/BindTextureSampler`。
- 增加断言：`vkUpdateDescriptorSets` 前后 `pBufferInfo/pImageInfo` 指针有效性与对象数量一致性。

**验收标准**：
- 启用验证层时无 descriptor invalid / null imageView / null sampler 类错误。
- 测试可重复通过。

---

### 3) 增加 business/main GLSL 一致性校验

**背景**：此前出现过 `FragmentShaderBusiness` 缺失 helper 的编译故障。  
**任务**：
- 在生成阶段增加一致性检查：`fs_main` 与 `FS business` 必需 helper 列表一致。
- 对缺失 helper 给出明确错误信息（包含材质名、helper 名）。

**验收标准**：
- 缺失 helper 时在编译前被拦截。
- 错误信息可直接定位到材质定义。

---

## P1（下周）

### 4) 完成 Phase B 未落地项

**任务**：
- 实装 `ValidateMaterialLogicDef()` 并接入材质编译入口。
- 完成 `MFGetPosition.h` / `MFCommon.h` 的规范接口收口（保留 legacy 回退）。
- 在 `ResourceLayoutGenerator` 头部补策略注释与示例。

**验收标准**：
- 规范文档中的“待执行”项显著收敛。
- 新增材质定义时可得到一致的编译期诊断。

---

### 5) 文档与示例同步治理

**任务**：
- 在示例说明里记录 `06b/06c` 当前策略：
  - `VertexDataManager` 路径
  - Brickwall 三贴图
  - 默认 roughness/normal 生效
- 文档中新增“常见视觉问题排查”小节（暴白、凹凸反向、全黑）。

**验收标准**：
- 新同事可按文档在 15 分钟内复现示例并定位常见问题。

---

## 建议执行顺序

1. P0-1（运行时 normal_strength）  
2. P0-2（descriptor 生命周期回归）  
3. P0-3（business/main 一致性校验）  
4. P1-4（Phase B 收口）  
5. P1-5（文档/示例同步）

---

## 风险与回滚

- 若运行时参数改造引入布局兼容问题，先保留宏路径作为临时回退开关。  
- 若新增验证导致历史材质批量报错，先提供 warning 模式，再分阶段提升为 error。

---

## 完成定义（DoD）

- `06b/06c` 在验证层开启下稳定运行。  
- 新增自动化回归覆盖 descriptor 生命周期与 shader helper 一致性。  
- 文档索引、阶段总结、执行清单三者一致且可追踪。
