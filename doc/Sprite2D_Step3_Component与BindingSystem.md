# Sprite2D 迁移 — Step 3：Sprite2DComponent + Sprite2DMaterialBindingSystem

> 状态：可独立编译；运行时仍无副作用（系统未被 group 注册）
> 风险等级：中
> 预计耗时：4–6 小时
> 关键原则：**只增不删**。`QuadComponent`、`QuadMaterialBindingSystem` 完全不动。

---

## 0. 目标

1. 新建 `Sprite2DComponent`，包含 `pivot` / `size` / `rotation` / `tint` 等新字段。
2. 新建 `Sprite2DMaterialBindingSystem`，负责：
   - 把 `Sprite2DComponent` 解析成 `PrimitiveComponent`（共享 unit-square 几何 + Sprite2D material）。
   - 写入 per-MI `Sprite2DTransform` SSBO。
3. 不接入 `RenderPipelineGroup`（Step 4）。
4. 不修改任何旧代码。

---

## 1. 前置条件

- Step 1、Step 2 已通关。
- `Sprite2DResourcePrepareSystem` 编译通过、自检通过、未被注册。

---

## 2. 涉及文件

| 路径 | 改动 |
|---|---|
| `inc/hgl/ecs/components/Sprite2DComponent.h` | **新建** |
| `src/ecs/components/Sprite2DComponent.cpp` | **新建** |
| `inc/hgl/ecs/systems/render/Sprite2DMaterialBindingSystem.h` | **新建** |
| `src/ecs/systems/render/Sprite2DMaterialBindingSystem.cpp` | **新建** |
| `src/ecs/CMakeLists.txt` | 加上面 4 个文件 + `source_group` |
| 任何旧 Quad / Billboard 文件 | **不动** |

---

## 3. 执行步骤

### 3.1 拷贝 `QuadComponent` → `Sprite2DComponent`

```pwsh
Copy-Item inc/hgl/ecs/components/QuadComponent.h `
          inc/hgl/ecs/components/Sprite2DComponent.h
Copy-Item src/ecs/components/QuadComponent.cpp `
          src/ecs/components/Sprite2DComponent.cpp
```

改名：

- `QuadComponent` → `Sprite2DComponent`
- `GetSerializationType()` 返回 `"Sprite2D"`（而非 `"Quad"`）
- include guard 同步

### 3.2 字段定义

`Sprite2DComponent.h`（最小可用版）：

```cpp
namespace hgl::ecs
{
class Sprite2DComponent : public PrimitiveComponent
{
public:
    explicit Sprite2DComponent(const std::string& name = "Sprite2D");
    ~Sprite2DComponent() override = default;

    // ─── 尺寸 ───
    void SetFixedSize(bool v)                   { fixed_size = v; MarkDirty(); }
    bool IsFixedSize() const                    { return fixed_size; }

    void SetPixelSize(const hgl::math::Vector2u& s) { pixel_size = s; MarkDirty(); }
    const hgl::math::Vector2u& GetPixelSize() const { return pixel_size; }

    void SetWorldSize(const glm::vec2& s)       { world_size = s; MarkDirty(); }
    const glm::vec2& GetWorldSize() const       { return world_size; }

    // ─── 锚点 / 旋转 / Tint ───
    void SetPivot(const glm::vec2& p)           { pivot = p; MarkDirty(); }
    const glm::vec2& GetPivot() const           { return pivot; }

    void SetRotation(float radians)             { rotation = radians; MarkDirty(); }
    float GetRotation() const                   { return rotation; }

    void SetTint(const glm::u8vec4& c)          { tint = c; MarkDirty(); }
    const glm::u8vec4& GetTint() const          { return tint; }

    // ─── front face / 纹理 ───
    void SetFrontFace(VkFrontFace ff)           { front_face = ff; MarkDirty(); }
    VkFrontFace GetFrontFace() const            { return front_face; }

    void SetTexturePath(const hgl::OSString& p);
    const hgl::OSString& GetTexturePath() const { return texture_path; }

    void SetDomainTag(const std::string& tag)   { domain_tag = tag; MarkDirty(); }
    const std::string& GetDomainTag() const     { return domain_tag; }

    // 序列化
    static const char* GetSerializationType();
    static bool SerializeToRecord(const std::shared_ptr<Component>&,
                                  const hgl::UnorderedMap<EntityID,int32_t>&,
                                  ComponentRecord&);
    static void DeserializeFromRecord(const ComponentRecord&, Entity*,
        std::vector<std::pair<std::shared_ptr<TransformComponent>,int32_t>>&);

private:
    bool                fixed_size  = true;
    hgl::math::Vector2u pixel_size{256, 256};
    glm::vec2           world_size{1.f, 1.f};
    glm::vec2           pivot{0.5f, 0.5f};
    float               rotation = 0.0f;
    glm::u8vec4         tint{255, 255, 255, 255};
    VkFrontFace         front_face = VK_FRONT_FACE_CLOCKWISE;

    hgl::OSString       texture_path;
    hgl::OSString       applied_texture;
    bool                texture_dirty = false;
    std::string         domain_tag;
    graph::Texture2D*   texture = nullptr;
    graph::Sampler*     sampler = nullptr;

    friend class Sprite2DMaterialBindingSystem;
};
}
```

> ⚠️ 序列化 record 字段顺序新增字段必须放到结构体末尾，以保证旧资产能反序列化为前缀（如果 ULRE 用 protobuf-style 是 OK 的；如果是 raw memcpy 则需要 version 字段）。**Step 7 删除 QuadComponent 之前**，需要在 `DeserializeFromRecord` 里加一段 legacy `"Quad"` → `"Sprite2D"` 转换（写在 §6.1 的迁移注意里）。

### 3.3 实现 `Sprite2DMaterialBindingSystem`

参考 `QuadMaterialBindingSystem.cpp`，**整体结构复用**，关键差异：

| 项 | 旧 QuadMaterialBindingSystem | 新 Sprite2DMaterialBindingSystem |
|---|---|---|
| 监听 component | `QuadComponent` | `Sprite2DComponent` |
| 几何来源 | `QuadResourcePrepareSystem::GetSharedPrimitive()`（单点） | `Sprite2DResourcePrepareSystem::GetSharedPrimitive()`（unit-square） |
| Material variant | `BillboardCameraFacing/AxisLocked` | `Sprite2DCameraFacing/AxisLocked` |
| Per-MI schema | `BillboardSizeUVec2` | `Sprite2DTransform` |
| Per-MI 数据写入 | size 1 项 | size + pivot + rotation + tint + flags |

Per-MI 写入伪代码：

```cpp
Sprite2DTransform mi{};
mi.size       = sprite->IsFixedSize()
                    ? glm::vec2(sprite->GetPixelSize().x, sprite->GetPixelSize().y)
                    : sprite->GetWorldSize();
mi.pivot      = sprite->GetPivot();
mi.rotation   = sprite->GetRotation();
mi.tint_rgba8 = glm::packUnorm4x8(glm::vec4(sprite->GetTint()) / 255.f);
mi.flags      = (sprite->IsFixedSize() ? 1u : 0u)
              | (sprite->IsFixedSize() ? 2u : 0u);   // axis_locked == fixed_size 的初版约定
mi._pad0      = 0;

prim->SetMaterialRecipe(BuildSprite2DRecipe(...), &mi, sizeof(mi));
```

### 3.4 系统依赖与执行顺序

```cpp
Sprite2DMaterialBindingSystem::Sprite2DMaterialBindingSystem(ECSContext* ctx)
{
    SetSystemType(SystemType::Sprite2DMaterialBinding);   // 如果不想加新枚举，复用 QuadMaterialBinding
    SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
    AddDependency<Sprite2DResourcePrepareSystem>();
    AddDependency<MaterialResolveSystem>();
}
```

> 注意：与旧 `QuadMaterialBindingSystem` 在同一 phase；后续 group 会决定哪一个被注册。本步两个都存在但都不被注册，不会冲突。

### 3.5 CMake 接入

参考 Step 2 的 CMake 模板，把 4 个新文件加到 ECS_SOURCE 与 source_group。

---

## 4. 验证

### 4.1 编译

- 0 error。
- 所有 4 个新文件出现在 IDE 项目树。

### 4.2 运行时无副作用

- 跑 `03_BillboardPerspectiveECS`：完全等同 Step 2 的行为。
- 用 RenderDoc / `[ECS] Update Begin` 日志确认：**不应**出现 `Sprite2DMaterialBindingSystem`。

### 4.3 离线 sanity

临时在 `BillboardPerspectiveECS::Init()` 里加：

```cpp
auto e = ecs_context->CreateEntity<Entity>("Sprite2DTest");
auto s = e->AddComponent<Sprite2DComponent>();
s->SetPixelSize({128, 128});
s->SetPivot({0.5f, 0.5f});
s->SetRotation(0.3f);
s->SetTint({255, 200, 100, 255});
LogInfo("[Sprite2D Step3] component created, fixed=%d size=(%u,%u)",
        s->IsFixedSize(), s->GetPixelSize().x, s->GetPixelSize().y);
```

跑一帧，确认能 add 成功且日志正常。**测完删除**。

> 此时 sprite **不会渲染**（系统未注册），是预期行为。

---

## 5. 常见坑

- ❌ `Sprite2DComponent` 继承自 `PrimitiveComponent` 而非 `Component`，注意 `MarkDirty()` 等基类方法的可见性。仿照 `QuadComponent` 的 `public:` 声明即可。
- ❌ `glm::packUnorm4x8` 在 `glm/gtc/packing.hpp`，没 include 会编译错。
- ❌ `Sprite2DTransform` 的字节布局必须与 Step 1 GLSL 一致；改任何字段顺序都要同步改 GLSL，否则旋转 / 颜色全乱。
- ❌ Per-MI schema 的 `flags` 用 `uint`，C++ 端用 `uint32_t`，不能错写成 `int32_t`（GLSL `uint` ≠ `int`，会触发 SPIR-V validation 报警）。
- ❌ `QuadMaterialBindingSystem` 与 `Sprite2DMaterialBindingSystem` 都监听同一个 entity 时（debug 阶段）会重复 `SetMaterialRecipe`，产生 mi_data 错乱。**保证测试 entity 只挂一种 component。**

---

## 6. 序列化兼容性（Step 7 删除 Quad 时再做，本步只是预留）

`Sprite2DComponent::DeserializeFromRecord`：

```cpp
if (record.type == "Quad") {
    // Legacy Quad → Sprite2D 默认参数转换
    auto sprite = entity->AddComponent<Sprite2DComponent>();
    sprite->SetFixedSize(/* 从 record 读旧字段 */);
    sprite->SetPixelSize(/* ... */);
    sprite->SetPivot({0.5f, 0.5f});
    sprite->SetRotation(0.0f);
    sprite->SetTint({255,255,255,255});
    return;
}
// 正常 Sprite2D 反序列化路径...
```

**Step 3 不需要写这段**，但 Step 7 删除 `QuadComponent` 之前必须先在这里加好。

---

## 7. 回滚方案

```pwsh
git rm inc/hgl/ecs/components/Sprite2DComponent.h
git rm src/ecs/components/Sprite2DComponent.cpp
git rm inc/hgl/ecs/systems/render/Sprite2DMaterialBindingSystem.h
git rm src/ecs/systems/render/Sprite2DMaterialBindingSystem.cpp
git restore src/ecs/CMakeLists.txt
```

---

## 8. Step 3 通关条件

- [ ] 全量构建 0 error。
- [ ] 旧 baseline 跑通且日志中**不出现** Sprite2D 系统。
- [ ] 临时 sanity 代码确认 `Sprite2DComponent::AddComponent` 成功。
- [ ] `Sprite2DTransform` C++ 大小 == 32 字节（`static_assert(sizeof(Sprite2DTransform) == 32)` 加在 .cpp 里）。
- [ ] 本 .md 末尾追加 `已通过：YYYY-MM-DD by xxx`。
