# 死绑定 Camera UBO 清理：统计结论与执行方案

> **注（2026-09-28）**：本文的清理已落地（相机数据统一走 BDA）。**相机数据的后续归属定稿**：
> 相机是**世界级观察者数据**，将从设备级 `GlobalSSBOBufferRegistry` 的行池下沉为**世界私有存储**
> （16 槽 × 8 帧槽，0 号槽恒为本世界默认相机），地址从全局表迁到世界表 `WorldAddresses` ——
> 见 `doc/world-addresses-and-camera-model-plan.md`。本文其余结论（删死绑定、删 `CAMERA_BINDING`）不变。

> 结论先行：**`DescriptorSemantic::CameraInfo` 本身可以删**（无按序号消费者），
> 上一轮“删了就错位/材质编译失败”的表象来自 **陈旧的着色器产物缓存**，不是源码。
> 本文给出完整删除清单、三个真正的坑与验证顺序。

## 1. 现状（要删什么）

相机数据已全部走 BDA：`ShaderLibrary/ubo/scene_ubo.glsl`（C1-3 后 `camera` 宏读**世界表** `WorldAddresses`）
`#define camera CameraInfoBufferRef(world_addresses.addr_camera_info).cameras[pc_root.camera_row]`，
`CameraInfoData`/`CameraInfoBufferRef` 是 `buffer_reference`（不参与任何 binding）。
但 Scene 集 Set 0 仍**声明并每帧写入** binding 0 的相机 UBO，且没有任何 GLSL 读取它
（全 ShaderLibrary 无 `binding=0` 的 Scene UBO，`CAMERA_BINDING` 只被定义、从未被使用）。

## 2. 统计结论：`DescriptorSemantic` 的消费方式（决定“能不能删”）

全仓 `DescriptorSemantic` 引用 148 处，分布在 10 个文件。逐个核对后：

| 消费方式 | 位置 | 删成员是否安全 |
|---|---|---|
| X-macro 生成枚举 + `#name`（按构造正确） | `DescriptorSemantic.h:19-53` | 安全 |
| `switch` / `==` 比较 | `DescriptorSemantic.h:72-102`、`ShaderResourceSchema.h:50-99/167-330`、`DescriptorContract.cpp:71-190`、`MaterialShaderCompiler.cpp:166-172`、`GenericMaterialBuilder.cpp:388-391` | 安全 |
| 目录行的语义列（按值查表 `FindResourceCatalogEntry`） | `DescriptorResourceCatalog.h:47-52/63` | 安全（删行即可） |
| 名字/结构默认值（按语义查目录 SBS） | `ShaderResourceSchema.h:105-162` | 安全 |
| **并行有序数组 `names[]` ↔ `semantic_values[]`** | `MaterialDefinitionFile.cpp:860-869` | **必须成对删同一项** |
| 结构转储打印（`GetDescriptorSemanticName`） | `ShaderStructureDump.cpp:164-165` | 安全 |
| 材质 UBO 需求列表（顺序容器 + 线性查找） | `MaterialRecipe.h:304/533` | 安全 |

**没有任何 `int(semantic)` 取序号、按序号索引数组、或按序号哈希的地方**（已 grep 确认）。
因此：删 `HGL_SEMANTIC(CameraInfo)` 只需同步 `MaterialDefinitionFile.cpp` 那两个并行数组。

## 3. 三个真正的坑

1. **材质 TOML 的 `ubos` 不只是声明**：列表为空会让生成的 GLSL 头不再 `#include "ubo/scene_ubo.glsl"`
   ⇒ 全材质 `CompileMaterial failed`。删掉 `"CameraInfo"` 后必须补回引擎必然需要的 `"ViewportInfo"`
   （例如 `ubos = ["CameraInfo", "SkyInfo"]` → `ubos = ["ViewportInfo", "SkyInfo"]`）。
2. **Scene 绑定号必须 0 起连续**：`DescriptorResourceCatalog.h` 的 `SceneBindingsFullyCovered` 是位图断言
   （`seen == (1<<slot_count)-1` 且计数等于 `SceneBinding::RANGE_SIZE`）。删掉 `Camera=0` 后必须把
   Sky/Viewport/ColorPalette/GlobalAddresses/Shadow 整体重编号 0..4，并同步
   `descriptor_macros.glsl` 的 5 个默认宏、`static_assert` 锚点、以及所有注释里的绑定号。
3. **陈旧着色器产物缓存会把“正确改动”伪装成坏改动**：`build/cache-hot/shader-cache/`
   （`ShaderArtifactStore` 缓存阶段 GLSL/产物）。schema/材质变更后不清它，门会拿旧产物比较
   ⇒ 出现 `semantic=SkyInfo` 却 `name=camera struct=CameraInfo` 的错配、`CompileMaterial failed`、
   甚至示例渲染异常。**改 schema/材质前后必须清空或挪走该目录再验**。

## 4. 删除清单（缺一项即失败）

1. `inc/hgl/common/DescriptorSetTypeDef.h`：枚举 `Camera=0` 行、`ENUM_CLASS_RANGE(Sky,Shadow)`、
   `static_assert` 明确值、`kSceneBindingCamera` 别名、`kDescriptorBindingMacros` 的 `CAMERA_BINDING` 行（其
   `"// ── Scene set ──"` 注释头移到 SKY 行），其余 5 项重编号 0..4。
2. `ShaderLibrary/common/descriptor_macros.glsl`：`CAMERA_BINDING` 默认宏块；5 个宏默认值改 0..4。
3. `ShaderLibrary/ubo/scene_ubo.glsl`：文件头绑定注释（`binding 0: CameraInfo camera`）与其后列表重编号。
4. `inc/hgl/mtl/DescriptorSemantic.h`：`HGL_SEMANTIC(CameraInfo)` 与 `GetDescriptorSemanticLayer` 的 case。
5. `inc/hgl/graph/ubo/UBOShaderSources.h`：`SBS_CameraInfo`。
6. `inc/hgl/mtl/DescriptorResourceCatalog.h`：CameraInfo 目录行。
7. `src/ShaderGen/builder/DescriptorBuilderCommon.h`：`PushCamera()` 与其 `case`。
8. `src/ShaderGen/compile/MaterialShaderCompiler.cpp`：`kDefinitionCapabilityRules` 的 CameraInfo 行。
9. `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp`：`ubos` 名表与语义值表**成对**删。
10. `src/ShaderGen/meshgen/MeshModeDescriptor.h`：`ResolveVertexPassthroughUbos` 的 `needs_camera` 块与 `ResolveLineQuadUbos` 的 add。
11. `src/ShaderGen/meshgen/MeshShaderHeaderGen.h`：`force_camera_ubo` 形参（两处签名 + 调用点）与 `needs_camera` 块
    （删形参务必连逗号一起处理，否则 `syntax error: ')'`）。
12. `ShaderLibrary/material/*.material.toml`：11 个文件的 `ubos`（见坑 1）。
13. `src/Vulkan/VKGlobalSceneUBOSet.cpp`：**两处** layout builder 删 binding 0 条目并把数组紧凑化成 0..4
    （`kBindingCount = RANGE_SIZE` 随重编号自动为 5）+ 注释。
14. `inc/hgl/vk/VKGlobalSceneUBOSet.h`：绑定注释列表。
15. `src/ecs/systems/render/RenderSceneUBOSystem.{h,cpp}`：`ResolveCameraUBO()` 声明/实现、局部变量、
    `&& camera_ubo` 条件、`UpdateUBO(kSceneBindingCamera, ...)`、告警格式串与参数。
16. `inc/hgl/graph/ubo/ShadowInfo.h`：注释里的 `Binding 5` → `4`。
17. `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp`：夹具 `ubos = ["CameraInfo","SkyInfo"]` → `["ViewportInfo","SkyInfo"]`。
18. golden：`src/Tools/ShaderGen/golden/*.txt` 里 CameraInfo 资源行与 `resource_count`。

## 5. 验证顺序（本仓基线已知）

1. **清/挪 `build/cache-hot/shader-cache`**（坑 3）。
2. purge + 构建 `ULRE.Vulkan`/`SceneGraph`/`ECS`/`ShaderResourceSchemaRegressionGate`/示例。
3. `ShaderResourceSchemaRegressionGate`：**本仓基线 39 PASS / 0 FAIL**，改动后应仅剩 golden 差异；
   用 `diff --strip-trailing-cr golden/x.txt golden/x.txt.actual` 核对（两侧行尾不同，直接 diff 全是噪声），
   确认差异**只有预期的资源行变化**后再刷新 golden，跑完删 `.actual`。
4. Test 21 Passed；ATS `D1 57.6%` / `D3 18189+600662 px` / `D4 拒绝 0` / selfcheck PASS / 0 VUID；CSM 多轮 `不一致=0`。
5. 注意：**构建失败时 MSBuild 不会重链 exe**，那一轮的 Test/ATS/CSM“绿”都是旧二进制的假绿。

## 6. 编辑手法提醒（本仓 CRLF）

- 不要用行号定位补丁目标（编辑中文件会偏移，曾因此误删目录里的 ShadowInfo 行）。
- `patch` 工具对多行 `old_string` 常失配（CRLF/LF 混用）⇒ 用单行锚点，
  或带断言的 Python 字节级替换（备份 → 临时文件 → `os.replace`），替换后立刻打印核对。
