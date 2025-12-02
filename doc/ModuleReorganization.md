# ULRE 工程模块重新分类建议

## 一、当前模块分析

### 1. 现有模块结构

| 模块名 | 定位 | 主要内容 |
|--------|------|----------|
| **CMCore** | 基础库 | 数学类型(Matrix/Vector)、OS接口定义、基础数据结构 |
| **CMPlatform** | 平台实现 | 各平台的OS接口实际实现代码 |
| **CMSceneGraph** | 场景图库 | 部分几何计算、场景节点管理 |
| **CMUtil** | 工具库 | 通用工具类 |
| **CMGUI** | GUI库 | GUI相关组件 |
| **CM2D** | 2D库 | 2D渲染相关 |
| **CMAssetsManage** | 资源管理库 | 资源加载与管理 |
| **ULRE** | 主工程 | Vulkan渲染引擎、场景渲染、材质系统 |

### 2. 当前问题分析

#### 问题1: 数学/几何计算分散
- **Matrix/Vector** 在 CMCore
- **部分几何计算** 在 CMSceneGraph  
- **渲染相关几何** 在 ULRE

**影响**: 开发时需要在多个库之间切换，依赖关系复杂。

#### 问题2: OS抽象层割裂
- **OS接口定义** 在 CMCore
- **OS接口实现** 在 CMPlatform

**影响**: 修改接口需要同时修改两个库。

#### 问题3: 渲染相关代码分散
从当前ULRE工程的代码结构可以看到：
```
inc/hgl/graph/          # 渲染相关头文件 (131个文件)
src/SceneGraph/         # 场景图和Vulkan实现 (129个文件)
src/ShaderGen/          # Shader生成
src/GUI/                # GUI实现
```

这些代码虽然在ULRE中，但实际上与CMSceneGraph有功能重叠。

---

## 二、重新分类建议

### 方案A: 按职责划分（推荐）

#### 1. **CMMath** (新建) - 数学与几何库
```
包含内容:
├── Vector (2D/3D/4D)
├── Matrix (3x3/4x4/Transform)
├── Quaternion
├── Geometry (几何计算)
│   ├── AABB (轴对齐包围盒)
│   ├── OBB (有向包围盒)
│   ├── Ray/Plane/Frustum
│   ├── Intersection (相交测试)
│   └── Distance (距离计算)
├── Spline (样条曲线)
└── Color
```

**建议**: 将 CMCore 中的数学类型和 CMSceneGraph 中的几何计算合并。

#### 2. **CMPlatform** - 平台抽象层
```
包含内容:
├── OS 接口定义 + 实现 (合并自CMCore)
│   ├── File System
│   ├── Thread
│   ├── Memory
│   ├── Time
│   └── Console
├── Window System
│   ├── Win32
│   ├── X11/Wayland
│   ├── macOS/iOS
│   └── Android
└── Input System
```

**建议**: 将 CMCore 中的 OS 接口定义移入 CMPlatform，实现定义与实现的统一。

#### 3. **CMCore** - 核心工具库
```
包含内容:
├── DataType (基础数据类型定义)
├── Container (容器类)
│   ├── Array/List
│   ├── Map/Set
│   └── String
├── Memory (内存管理)
├── Log (日志系统)
├── Plugin (插件系统)
└── Serialize (序列化)
```

**建议**: CMCore 只保留最基础的、与平台/渲染无关的核心功能。

#### 4. **CMRender** (新建或重命名ULRE部分) - 渲染抽象层
```
包含内容:
├── RenderDevice (渲染设备抽象)
├── Buffer/Texture (GPU资源)
├── Shader (着色器管理)
├── Pipeline (渲染管线)
├── RenderTarget (渲染目标)
└── RenderContext (渲染上下文)
```

#### 5. **CMVulkan** (新建) - Vulkan后端实现
```
包含内容:
├── VKInstance/VKDevice
├── VKBuffer/VKTexture
├── VKShaderModule
├── VKPipeline
├── VKRenderPass
├── VKFramebuffer
├── VKCommandBuffer
└── Platform Specific
    ├── WinVulkan
    ├── LinuxVulkan
    ├── AndroidVulkan
    └── macOSVulkan (MoltenVK)
```

**建议**: 当前 `src/SceneGraph/Vulkan/` 目录可独立为 Vulkan 后端模块。

#### 6. **CMSceneGraph** - 场景图系统
```
包含内容:
├── SceneNode (场景节点)
├── Transform (变换)
├── Camera (相机)
├── Light (灯光)
├── Mesh (网格)
├── Material (材质)
└── Culling (剔除)
```

#### 7. **ULRE** - 引擎主体
```
包含内容:
├── RenderFramework (渲染框架)
├── SceneRenderer (场景渲染器)
├── MaterialSystem (材质系统)
├── ShaderGen (Shader生成)
├── Font (字体渲染)
├── GUI (界面系统)
└── Tools (工具)
```

---

### 方案B: 最小改动方案

如果想最小化改动，可以考虑以下调整：

#### 1. 保持现有库结构
- 保留 CMCore/CMPlatform/CMSceneGraph 等现有模块

#### 2. 明确职责边界
| 模块 | 调整建议 |
|------|----------|
| CMCore | 只保留：基础类型、容器、日志、序列化 |
| CMPlatform | 合并OS定义+实现，增加窗口系统 |
| CMMath (新) | 从CMCore分离数学类型，从CMSceneGraph分离几何计算 |
| CMSceneGraph | 只保留场景图相关：节点、变换、相机、灯光 |
| CMRender (新) | 渲染抽象层，当前在ULRE中的渲染接口独立 |
| ULRE | 保持为应用层引擎 |

---

## 三、依赖关系图

### 当前依赖关系
```
ULRE
├── CMCore (基础 + 数学 + OS定义)
├── CMPlatform (OS实现)
├── CMSceneGraph (场景 + 几何)
├── CMUtil
├── CM2D
├── CMGUI
└── CMAssetsManage
```

### 建议的依赖关系 (方案A)
```
ULRE (应用层引擎)
├── CMRender (渲染抽象)
│   └── CMVulkan (Vulkan后端)
├── CMSceneGraph (场景图)
├── CMGUI (GUI)
├── CM2D (2D渲染)
└── CMAssetsManage (资源管理)

CMRender/CMSceneGraph/CMGUI/CM2D/CMAssetsManage
├── CMMath (数学几何)
├── CMPlatform (平台层)
└── CMCore (核心工具)

CMPlatform
└── CMCore

CMMath
└── CMCore
```

---

## 四、具体文件迁移建议

### 从 CMCore 迁出到 CMMath
```
// 数学类型
Vector2/3/4
Matrix3/4
Quaternion
Color
```

### 从 CMCore 迁出到 CMPlatform
```
// OS 接口定义
File/FileSystem
Thread/Mutex/Semaphore
Time/Timer
Memory/Allocator
Console/Terminal
```

### 从 ULRE/src/SceneGraph/Vulkan 独立到 CMVulkan
```
VK*.cpp/h (约50+文件)
Platform/* (各平台Vulkan初始化)
Debug/* (调试工具)
pipeline/* (管线相关)
```

### 从 CMSceneGraph 迁出到 CMMath
```
// 几何计算
AABB/OBB
Ray/Plane
Frustum
IntersectionTests
```

---

## 五、实施建议

### 阶段1: 创建 CMMath 库 (优先级: 高)
1. 创建新的 CMMath 子模块
2. 将 CMCore 中的数学类型移入
3. 将 CMSceneGraph 中的几何计算移入
4. 更新所有引用

### 阶段2: 合并 OS 接口 (优先级: 中)
1. 将 CMCore 中的 OS 接口定义移入 CMPlatform
2. 修改接口，使定义和实现在同一模块
3. 更新所有引用

### 阶段3: 分离 Vulkan 后端 (优先级: 中低)
1. 创建渲染抽象层接口
2. 将 Vulkan 实现独立为后端模块
3. 为将来支持其他图形API做准备

### 阶段4: 清理 CMCore (优先级: 低)
1. 移除已迁出的代码
2. 重新组织剩余代码
3. 更新文档

---

## 六、预期收益

1. **降低耦合度**: 各模块职责清晰，减少跨模块依赖
2. **提高可维护性**: 修改某一功能只需要关注单一模块
3. **便于复用**: 数学库、平台层可被其他项目直接复用
4. **支持扩展**: 渲染抽象层为未来支持其他图形API提供基础
5. **编译优化**: 模块化后可以更精细地控制编译依赖

---

## 七、注意事项

1. **向后兼容**: 迁移过程中保持API兼容，可使用类型别名过渡
2. **渐进式重构**: 不要一次性全部修改，分阶段进行
3. **自动化测试**: 确保有足够的测试覆盖，每次迁移后运行测试
4. **文档更新**: 每次迁移后及时更新文档和示例代码

---

*文档生成时间: 2024年*
*基于对 ULRE 工程代码结构的分析*
