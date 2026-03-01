# Shader Helper Function 规范 v1.0

本文档定义了 ULRE Shader System 中所有 helper 函数的统一签名、命名与语义规则。

**最后更新**：2026-03-01  
**适用范围**：当前及后续所有材质开发  
**规则约束**：违反本规范的代码不得通过 code review

---

## 1. 核心设计原则

1. **唯一签名**：每个语义目标只允许一个主签名（可有带参数的变体，但不允许同义名）
2. **显式语义**：函数名清晰表达空间变换、输入来源、输出用途
3. **按空间分类**：Local / World / View / Clip / Screen，不允许混淆
4. **统一缩写**：`MI` = MaterialInstance，`L2W` = LocalToWorld，`FS` = FragmentShader，`VS` = VertexShader

---

## 2. Position 获取 Helper（最高优先级统一）

### 2.1 规范化前的混乱状态（禁止使用）

❌ **已废弃**（禁止继续使用）：
```glsl
// 这些名字会被移除，请立即迁移
GetWorldPosition3D()      // 语义不明确
GetPosition3D()           // 不知道是哪个空间的
GetPosition3DL2W()        // 命名风格不一致
GetPosition3DCamera()     // 混合了两个空间
```

### 2.2 新统一签名（强制使用）

✅ **Vertex Shader 专用**：
```glsl
// 从顶点输入获取 Local Space Position（不经过变换）
vec3 GetLocalPosition()
{
    return Position;  // 直接读取顶点属性
}

// 应用 L2W 变换后的 World Space Position
vec4 GetWorldPosition()
{
    return GetLocalToWorld() * vec4(Position, 1.0);
}

// 应用 L2W + Camera VP 变换后的 Clip Space Position
vec4 GetClipPosition()
{
    return camera.vp * GetLocalToWorld() * vec4(Position, 1.0);
}
```

✅ **Fragment Shader / Geometry Shader 专用**：
```glsl
// 从插值输入获取 World Space Position
vec4 GetWorldPosition()
{
    return Input.WorldPosition;  // 读取 VS 传递的插值数据
}

// 如果 FS 需要屏幕空间坐标
vec2 GetScreenPosition()
{
    return gl_FragCoord.xy;
}
```

### 2.3 命名规则总结

| 函数名              | 空间      | 适用 Stage     | 输入来源                     |
|---------------------|-----------|----------------|------------------------------|
| `GetLocalPosition`  | Local     | VS only        | 顶点属性 `Position`          |
| `GetWorldPosition`  | World     | VS / FS / GS   | VS: L2W * Position<br>FS: 插值 |
| `GetClipPosition`   | Clip      | VS only        | camera.vp * WorldPos         |
| `GetScreenPosition` | Screen    | FS only        | gl_FragCoord.xy              |

**禁止事项**：
- ❌ 不允许在函数名中使用 `3D` 后缀（如 `GetPosition3D`）— 空间名已明确，无需加维度标记
- ❌ 不允许在函数名中混合多个空间（如 `GetPosition3DL2WCamera`）— 应拆分为独立步骤
- ❌ 不允许同时存在 `GetWorldPosition()` 和 `GetWorldPos()` — 只保留完整单词形式

---

## 3. 矩阵获取 Helper

### 3.1 基础变换矩阵

```glsl
// 获取 LocalToWorld 矩阵（从 Transform Buffer）
mat4 GetLocalToWorld()
{
    return l2w.mats[TransformID];  // 通过顶点属性 TransformID 索引
}

// 获取 Normal 矩阵（去除平移+缩放，只保留旋转）
mat3 GetNormalMatrix()
{
    return mat3(camera.view * GetLocalToWorld());
}
```

### 3.2 相机相关矩阵

```glsl
// 获取 Model-View 矩阵
mat4 GetModelViewMatrix()
{
    return camera.view * GetLocalToWorld();
}

// 获取 Model-View-Projection 矩阵
mat4 GetMVPMatrix()
{
    return camera.vp * GetLocalToWorld();
}
```

**命名规则**：
- ✅ 使用完整描述：`GetModelViewMatrix` 而非 `GetMVMat`
- ✅ 使用标准术语：`View` / `Projection` / `MVP` / `Normal`
- ❌ 禁止缩写：`GetCameraViewMatrix` → 应为 `GetModelViewMatrix`（View 指的是相机 View，不是 View Matrix）

---

## 4. MaterialInstance 获取 Helper

### 4.1 统一签名

✅ **唯一标准名称**（其他名称禁止使用）：
```glsl
MaterialInstance GetMI()
{
    // VS: 直接读取顶点属性 MaterialInstanceID
    return mtl.mi[MaterialInstanceID];
    
    // FS / GS: 读取插值的 MaterialInstanceID
    return mtl.mi[Input.MaterialInstanceID];
}
```

### 4.2 废弃的变体

❌ **禁止使用**：
```glsl
GetMaterialInstance()  // 名字过长，使用 GetMI()
GetMtl()               // 缩写不规范
GetMat()               // 与 Material 混淆
```

### 4.3 调用示例

```glsl
// ✅ 正确
vec4 color = GetMI().Color;

// ❌ 错误
vec4 color = GetMaterialInstance().Color;  // 使用了废弃名称
```

---

## 5. Normal 变换 Helper

### 5.1 规范签名

```glsl
// VS: 从顶点属性获取变换后的 Normal
vec3 GetNormal(vec3 localNormal)
{
    return normalize(GetNormalMatrix() * localNormal);
}

// FS: 从插值输入获取 Normal
vec3 GetNormal()
{
    return normalize(Input.Normal);  // 插值后需要重新归一化
}
```

### 5.2 变体说明

- VS 中如果顶点属性名为 `Normal`，可简化调用为 `GetNormal(Normal)`
- FS 中无参数版本直接返回插值的 Normal
- 不允许混淆 `GetNormal()` 和 `GetNormalMatrix()` 的用途

---

## 6. 2D 材质专用 Helper

### 6.1 Position Helper（NDC / ZeroToOne / Ortho）

```glsl
// 根据 CoordinateSystem2D 枚举注入不同实现
vec4 GetPosition2D()
{
    // NDC 模式
    return vec4(Position, 0, 1);
    
    // ZeroToOne 模式
    return vec4(Position.xy * 2 - 1, 0, 1);
    
   // Ortho 模式
    return viewport.ortho_matrix * vec4(Position, 0, 1);
}
```

**注意**：2D 材质固定返回 `vec4`，z=0，w=1

---

## 7. 高级光照 Helper（Phase D 补充）

暂时占位，待 Phase D 实现 GBuffer / PBR 时补充：

```glsl
// 计算 Blinn-Phong 高光
vec3 ComputeBlinnPhong(vec3 normal, vec3 viewDir, vec3 lightDir, vec3 albedo, float shininess);

// 计算 PBR
vec3 ComputePBR(vec3 normal, vec3 viewDir, vec3 lightDir, float roughness, float metallic, vec3 F0);

// 阴影计算
float ComputeShadow(vec4 shadowCoord, sampler2D shadowMap);
```

---

## 8. 验证与执行

### 8.1 当前验收标准

- [ ] 所有 `MF*.h` 文件已重命名/删除废弃 helper
- [ ] 已迁移材质（PureColor3D / VertexColor3D）使用新签名
- [ ] 运行时无 helper注入冲突（binding 分配器检查通过）
- [ ] 示例代码编译通过且渲染正确

### 8.2 迁移路径

1. 更新 `MFGetPosition.h`，删除所有 `GetPosition3D*` 变体，只保留 `GetLocalPosition` / `GetWorldPosition` / `GetClipPosition`
2. 更新 `MFCommon.h`，`GetMaterialInstance` → `GetMI`
3. 更新所有材质定义文件（`S_*.h`），使用新 helper
4. 运行测试验证渲染正确性

### 8.3 Code Review 拦截规则

提交代码时如果包含以下模式将被拒绝：
- `GetWorldPosition3D`
- `GetPosition3D`
- `GetMaterialInstance`（应为 `GetMI`）
- `GetWorldPos`（应为 `GetWorldPosition`）

---

## 9. FAQ

### Q1: 为什么不允许 `GetPos()` 这样的缩写？
**A**: 空间语义比字符数更重要。`GetWorldPosition()` 清晰传达"世界空间位置"，`GetPos()` 无法区分 Local / World / Clip。

### Q2: 旧材质中已经使用了 `GetPosition3D`，如何兼容？
**A**: 不再保留旧 helper 兼容路径。统一按本规范改造到新签名，并在评审中拦截旧写法。

### Q3: FS 中为什么 `GetWorldPosition()` 不需要参数？
**A**: FS 中 WorldPosition 已由 VS 插值传递，直接从 `Input.WorldPosition` 读取。VS 中需要手动应用 L2W 变换。

### Q4: `GetMI()` 和 `GetMaterialInstance()` 功能完全一样，为什么要统一？
**A**: 避免"一个功能两种写法"导致的认知负担。代码库中只有一种标准写法，降低学习成本。

---

## 10. 变更历史

| 版本 | 日期       | 变更内容                                           |
|------|------------|----------------------------------------------------|
| 1.0  | 2026-02-26 | 初版发布，规范 Position / Matrix / MI / Normal   |
| 1.1  | 2026-02-28 | 文档口径同步：更新时间与阶段状态说明对齐（无规范变更） |

---

**责任人**：Shader System 维护团队  
**审核周期**：每月 review 一次，Phase 结束时强制 review  
**冻结日期**：Phase E 完成后冻结，不再接受新 helper 添加
