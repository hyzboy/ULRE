# ULRE 架构重构 - 阶段零：Transform 局部数据 TRS 统一与 GLTFConvert 改造技术规范 (Phase 0 Specification)

## 一、 阶段目标与背景 (Goals & Background)

在全面推进 ECS 全 ID 模式与访问器化之前，必须首先建立**底层的变换数据标准真理源（Single Source of Truth）**。
* **现状痛点**：当前场景中部分构件依然依赖 4x4 矩阵（`Matrix4f`），且在 `GLTFConvert` 导入转换链路中存在 glTF 原始矩阵未标准化分解、GLM 四元数构造顺序倒错、多根节点映射脱节以及图元/节点双重方向变换等隐患。
* **阶段目标**：
  1. 彻底改造 `GLTFConvert` 工具，建立无损仿射矩阵极分解（`DecomposeMatrixToTRS`），输出标准四元数与 TRS 结构；
  2. 修复场景树的导出与父子关联逻辑，确保多根节点、Multi-Primitive 拆分节点的拓扑完整性；
  3. 引擎端全面规范化局部变换为原生 48 字节 `TransformTRS`，彻底废除局部 `Matrix4f` 存储。

---

## 二、 GLTFConvert 改造详细设计与修复规范

### 2.1 修复 GLM 与 glTF 四元数构造顺序倒错
* **涉及文件**：`math/TRS.cpp`, `math/TRS.h`, `gltf/ToNodeTransform.cpp`
* **问题原理**：
  * glTF 2.0 规范中四元数数组定义为 `[x, y, z, w]`；
  * GLM 构造函数 `glm::quat(w, x, y, z)` 要求实部 `w` 在第一位。若直接传入 `glm::quat(rot[0], rot[1], rot[2], rot[3])`，会导致四元数严重畸变翻转。
* **修复规范**：
  ```cpp
  // 必须显式将第 3 号索引 (w) 放在最前面传递：
  glm::quat q(rot[3], rot[0], rot[1], rot[2]); // (w, x, y, z)
  ```

### 2.2 仿射矩阵极分解实现 (`DecomposeMatrixToTRS`)
针对 glTF 节点使用 `matrix` 字段定义局部变换的情况，在加载期强制执行仿射极分解，将其统一收敛为 TRS：

```cpp
bool DecomposeMatrixToTRS(const glm::mat4& mat, glm::vec3& outT, glm::quat& outR, glm::vec3& outS) {
    // 1. 提取平移 Translation (列主序第 4 列)
    outT = glm::vec3(mat[3]);

    // 2. 提取基向量
    glm::vec3 c0 = glm::vec3(mat[0]);
    glm::vec3 c1 = glm::vec3(mat[1]);
    glm::vec3 c2 = glm::vec3(mat[2]);

    // 3. 计算缩放模长 (Scale)
    outS = glm::vec3(glm::length(c0), glm::length(c1), glm::length(c2));

    // 4. 负缩放 / 镜像检测 (Reflection Check)
    // 检查 3x3 矩阵的行列式，若为负，说明存在镜像变换，需反转 X 轴基底
    glm::mat3 rot_scale_mat(c0, c1, c2);
    if (glm::determinant(rot_scale_mat) < 0.0f) {
        outS.x = -outS.x;
        c0 = -c0;
    }

    // 5. 归一化正交基提取纯旋转矩阵并生成四元数
    glm::mat3 rot_mat(c0 / outS.x, c1 / outS.y, c2 / outS.z);
    outR = glm::quat_cast(rot_mat);
    outR = glm::normalize(outR);

    return true;
}
```

### 2.3 消除双重坐标系方向变换 (Double Orientation Conversion)
* **涉及文件**：`gltf/import/GLTFOrientationNodes.cpp`, `gltf/import/GLTFOrientationPrimitives.cpp`
* **问题原理**：若 `GLTFOrientationPrimitives` 已经修改了顶点坐标（如 Y-up 转 Z-up），而 `GLTFOrientationNodes` 又对 Node Transform 乘了方向旋转矩阵，会导致有深度的子部件位置与旋转发生二次发散。
* **规整原则**：
  * **统一由 Node 承担方向变换**：图元顶点数据（Primitive Vertex Positions/Normals）保持 glTF 原生不变；仅在场景根节点（Root Node）乘上坐标系转换矩阵；
  * 或者只在离线静态烘焙期将顶点与 LocalTransform 一并原地转换，切忌两端重复乘算。

### 2.4 场景树拓扑完整性与多根节点支持
* **涉及文件**：`gltf/convert/CopyScenes.cpp`, `export/SceneExportNodes.cpp`, `export/SceneExportTransforms.cpp`
* **规则**：
  1. **多根支持**：glTF 的 `scene.nodes` 数组允许有多个独立的根节点，导出时严禁假定“Node 0 为唯一根”。若存在多个根节点，应在导出时为它们建立虚拟根或平铺挂载；
  2. **Multi-Primitive 节点拆分保护**：若一个 glTF Mesh 包含多个 Primitive 并被拆分成多个物理 Node，所有原本挂在该 Mesh 节点下的子节点的 `parent_index` 必须严格重定向到主拆分节点，避免子树断连悬空；
  3. **明确变换语义**：`SceneExportTransforms` 必须明确只导出局部相对变换（Local TRS），严禁导出预乘后的世界变换，防止引擎端加载时二次连乘发散。

---

## 三、 ULRE 引擎端物理数据模型规范

### 3.1 原生 48 字节 `TransformTRS` 物理结构
在引擎头文件 `inc/hgl/ecs/support/TransformTypes.h` 中建立跨平台且与 GPU `std430` 100% 对齐的物理结构：

```cpp
#pragma once
#include <hgl/math/Vector.h>
#include <hgl/math/Quaternion.h>
#include <hgl/math/Matrix.h>

namespace hgl::ecs {
    // 严格 48 字节，16 字节对齐 (3 个 vec4)
    struct alignas(16) TransformTRS {
        Vector4f translation; // xyz: 平移, w: 保留
        Vector4f rotation;    // xyzw: 旋转四元数 (qx, qy, qz, qw)
        Vector4f scale;       // xyz: 缩放, w: 保留
    };
    static_assert(sizeof(TransformTRS) == 48, "TransformTRS must be exactly 48 bytes");

    // 变换节点层级拓扑数据 (解耦分离)
    struct TransformHierarchyData {
        uint32_t parent_id;   // 父节点 ID (根节点为 ~0u)
        uint32_t depth;       // 层级深度
    };
}
```

### 3.2 局部存储与世界输出的职责绝对解耦
* **局部数据（Local Transform）**：全系统内存中存储、编辑与序列化时，一律强制为 `TransformTRS`，彻底消灭局部的 `Matrix4f`。
* **世界数据（World Transform）**：仅在求值层级与渲染提交时，才通过 `TRS_To_Matrix4x3` 展开为 `Matrix4x3f` 全局世界矩阵供给 GPU 顶点着色器和视锥剔除。

---

## 四、 AI Coding Agent 落地任务清单 (Task Checklist)

* [ ] **Task 0.1**：在 `GLTFConvert` 的 `math/TRS.cpp` 中检查并纠正 `glm::quat` 构造实参，保证 `(w, x, y, z)` 映射到 glTF 的 `[x, y, z, w]`。
* [ ] **Task 0.2**：在 `GLTFConvert` 中实现 `DecomposeMatrixToTRS`，全面覆盖含有 `matrix` 定义的 glTF 节点，支持负缩放镜像检测。
* [ ] **Task 0.3**：清理 `GLTFOrientationNodes` 与 `GLTFOrientationPrimitives`，确立统一的坐标系转换规则，杜绝二次旋转。
* [ ] **Task 0.4**：检查 `SceneExportNodes` 与 `SceneExportTransforms`，确保多根节点正常导出，节点拆分后父子索引正确重定向。
* [ ] **Task 0.5**：在 ULRE 引擎中创建 `inc/hgl/ecs/support/TransformTypes.h`，定义 48 字节 `TransformTRS` 并加入 `static_assert`。
* [ ] **Task 0.6**：在 `example/Geometry/` 中加载带多级旋转和位移的测试模型，验证各子构件在空间中的位置与朝向 100% 准确。

