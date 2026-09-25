# 技术分析报告：基于 ShadowMask 的多层次混合阴影架构设计

> 文档状态：设计草案 / 预研规划  
> 适用范围：ULRE 渲染管线深度集成与多源阴影合成  
> 坐标系规范：右手系 (RH), Z-up, Vulkan NDC 深度 ZO ($z \in [0, 1]$)

---

## 一、 为什么引入 ShadowMask？（核心价值与动机）

当前引擎的 CSM（Cascaded Shadow Map）主要在**物体绘制/材质着色阶段（Forward Shading）**直接采样深度的 PCF/Poisson 滤波。随着场景复杂度增加或后续引入更多种类的阴影技术，直接在表面材质着色器中采样的架构会面临显著瓶颈：

1. **计算与带宽冗余 (Overdraw 放大开销)**：
   在复杂几何和多重遮挡场景中，被遮挡的片段仍需执行昂贵的多次 PCF 纹理采样或光线投射，导致 GPU 显存带宽与 ALQ 计算极度浪费。
2. **多阴影技术叠加困难与着色器变体爆炸**：
   若在单一表面材质 Shader 中同时塞入 CSM + 接触阴影 + 胶囊软阴影 + SDF 距离场，会导致着色器变体（Permutation）组合爆炸，指令数激增，并带来沉重的寄存器压力（VGPR Spilling）。
3. **后处理与滤波优化受限**：
   在单表面逐像素计算阴影无法方便地进行半分辨率降采样、时域累积（TAA/Temporal Filter）或基于屏幕空间的双边边缘保边滤波（Bilateral Blur）。

**ShadowMask（屏幕空间阴影遮罩）的核心机制**：
在场景最终光照着色之前，利用相机的深度缓冲（Depth Prepass 或 G-Buffer 深度），通过全屏/计算 Pass 执行阴影投射计算，将各级阴影（CSM、接触阴影、胶囊代理阴影、SDF 阴影）统一融合成一张或多张屏幕分辨率（或半分辨率）的遮罩图（例如单通道 R8_UNORM 表示主方向光遮挡，或 RGBA8_UNORM 分别存放最多 4 盏主要阴影光源）。
后续材质着色时，仅需一次标量读取（或根据屏幕 UV 读取单像素遮罩），实现**阴影判定与材质着色的彻底解耦**。

---

## 二、 演进切入点：从哪里入手？

### 2.1 依赖前置：稳定的相机深度与 RenderGraph Pass 拓扑
- **前置 1：Depth Prepass / G-Buffer 深度链**  
  屏幕空间技术依赖从深度图反推世界坐标 $P_{world}$。必须确保在执行光照之前，主视口已具有完整的 Early-Z / Camera Depth。
- **前置 2：RenderGraph 拆分管线**  
  RenderGraph 需建立明确的前后依赖关系：
  ```
  [Depth PrePass] ──> 输出 Camera Depth
          ↓
  [Shadow Cascades Pass] ──> 输出 CSM Depth Array (现有能力)
          ↓
  [ScreenShadowMaskPass] (新增) ──> 输入 Camera Depth + CSM + 辅助代理/SDF ──> 输出 ShadowMask Texture
          ↓
  [Scene Shading Pass] ──> 读取 ShadowMask，替代材质中的 PCF 采样
  ```

### 2.2 坐标系与深度契约对齐
- **手性与朝向**：Vulkan RH，Z-up（X右，Y前，Z上），相机前向在视空间映射到 $-Z$。
- **NDC 深度范围**：$[0, 1]$（ZO 约定）。如果启用 Reversed-Z，近平面对应 $1.0$，远平面对应 $0.0$。
- **屏幕空间逆变换重建世界坐标**：
  在屏幕 Shader 中利用 `inverse(ViewProjectionMatrix)` 或利用线性化深度与视锥射线（Frustum Ray）精确反求像素世界坐标，避免由于精度丢失造成的阴影撕裂。

---

## 三、 多层次混合阴影体系与合成方案

在 `ScreenShadowMaskPass` 中，各类阴影算法按**距离尺度与几何精度**进行极值合成（取最强遮挡度，遮罩值 $0$ 为全阴影，$1$ 为无遮挡）：

$$\text{ShadowMask}_{\text{final}} = \min(\text{ContactShadow}, \min(\text{CapsuleShadow}, \min(\text{CSM}, \text{RTDF})))$$

```
[极近距离 0 ~ 3m] ────────── [中近距离 0 ~ 30m] ────────── [中远距离 10 ~ 500m]
   Contact Shadow            CSM (级联 0 / 1)             CSM (级联 2 / 3)
 (补齐微缝隙漏光/漏脚)        (高精度动态与主要轮廓)               或
         │                           │                Distance Field (SDF)
   Capsule Shadow                    │               (大范围、软半影、低开销)
 (角色肢体柔和环境落地阴影)           │
         └───────────────────────────┴─────────────────────────┘
                                     ↓
                         融合成统一的 Screen ShadowMask
```

### 3.1 接触阴影 (Contact Shadows / Screen-Space Shadows)
* **定位**：解决阴影偏移（Bias）导致的漏光、悬浮（Peter-Panning），以及高精度细节（如眼窝、衣服褶皱、细小接缝）因 CSM 分辨率不足导致的阴影丢失。
* **实现机制**：
  * 在 ShadowMask Pass 中，每个像素从重建的深度位置出发，沿像素指向光源的向量在屏幕深度图上进行短步长光线步进（Screen Space Ray Marching，8~16 步）。
  * 步进时可配合 Hi-Z（分层深度缓冲）进行快速跳步。
* **开发优先级**：**最高，最容易落地**。无需新增额外的几何代理或离线烘焙资产，只需利用当前帧 Depth 即可实现。

### 3.2 胶囊体 / 盒体代理阴影 (Capsule & Box Proxy Shadows)
* **定位**：主要针对蒙皮角色骨骼关节与简单环境体。在缺乏高精度级联或室内柔光环境下，提供极致柔和且廉价的局部触地阴影和自体遮挡。
* **实现机制**：
  * 为角色骨骼挂载轻量级的 Capsule（线段两端点坐标与半径），或为箱体挂载 OBB 盒。
  * 在 Compute/Pixel 着色器中，以像素位置为起点，计算该点朝向光源方向受到胶囊体的解析立体角遮挡（Analytic Spherical Cone / Capsule Occlusion），直接求出闭式解析柔和软阴影。
* **引擎架构依赖**：
  * ECS 需支持 `ProxyCollisionComponent` 或专用 `ShadowProxyComponent`。
  * `TransformSystem` 每帧负责将代理的全局几何数据提交至 Global SSBO。

### 3.3 光线追踪距离场阴影 (RTDF / Mesh Distance Field Shadows)
* **定位**：替代中远距离（CSM 2/3）的大范围级联，实现全岛/远景大范围静态物体的低开销、平滑半影软阴影。
* **实现机制**：
  * **导入/离线期**：将静态 Mesh 离散生成 Local SDF 3D 纹理（或稀疏体素砖 Sparse Brick）。
  * **运行期**：根据场景层级拼装 Global Distance Field 体积。在 ShadowMask Compute Shader 中，自世界坐标点向主光源步进求交。
* **优势**：远景阴影不需要消耗任何光栅化顶点/三角面 Draw Call，彻底解决远景大阴影分辨率不足和锯齿闪烁问题。

---

## 四、 架构细节与性能考量

1. **半分辨率渲染与双边滤波 (Half-Res + Bilateral Upsample)**：
   - 阴影属于低频信号，接触阴影与 SDF 射线步进若在全分辨率执行可能在 4K 下负载过重。
   - 建议架构采用：在半分辨率（例如每轴 $0.5\times$）生成 ShadowMask，随后结合全分辨率深度与法线进行**保边双边上采样（Depth/Normal Bilateral Upsample）**还原至全分辨率。
2. **多光源扩展策略**：
   - 方案 A（标准紧凑）：RGBA8_UNORM，4 个通道分别记录 4 盏主导阴影光源（如 1 个太阳方向光 + 3 盏主要动态点光/聚光灯）。
   - 方案 B（分块瓦片/Forward+）：对于海量点光源，ShadowMask 仅负责主方向光；额外点光源采用基于簇（Clustered）的阴影图或屏幕空间轻量检测。
3. **符合 HGL 自研库规范**：
   - 代理组件数组与缓存容器使用 `hgl::ArrayList`。
   - 代理数据通过 Global SSBO 批量上传，避免在渲染主循环中使用 STL 容器分配或临时内存碎片。

---

## 五、 推荐的分阶段实施路线图

* **阶段 1：ShadowMask 基础骨架搭建与 CSM 剥离**
  * 在 RenderGraph 中挂接 `ScreenShadowMaskPass`，输入现有 CSM Depth Array。
  * 表面材质 Shader 改造：移除材质内 PCF 计算，改为采样 ShadowMask 单通道遮罩。
  * 验证画面与现有 CSM 渲染完全一致。
* **阶段 2：引入屏幕空间接触阴影 (Contact Shadows)**
  * 在 `ScreenShadowMaskPass` 中增加短距离光线步进算法。
  * 与 CSM 采样结果做 `min` 合成，彻底消灭角色脚底及缝隙悬浮。
* **阶段 3：ECS 胶囊代理系统与解析阴影**
  * 设计 `CapsuleShadowComponent`，建立骨骼驱动胶囊管线。
  * 在 ShadowMask Pass 中读取胶囊 SSBO 计算解析遮挡并混合。
* **阶段 4：离线 SDF 烘焙工具链与远景 RTDF**
  * 开发 Mesh SDF 离线计算工具与运行时流送系统。
  * 在 ShadowMask 中用 RTDF 替代远景 CSM 级联，实现近远景平滑过渡的混合阴影体系。
