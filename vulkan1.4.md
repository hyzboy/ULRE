分析完成。基于对当前代码的实际阅读（`VKInstance.cpp` 已声明 1.4、`ShaderGenProfileTargetVersion.h` 的版本阶梯、`VKDeviceCreater.cpp` 的扩展/特性探测、`DescriptorSet`/`MaterialDescriptorManager`/`BindlessTextureManager`/`VKPipelineLayoutData` 的整套 descriptor 机制），给出以下结论。



\## 一、ShaderGen 侧可简化



\### 1. 版本/SPIR-V 阶梯彻底塌缩为常量

`ResolveShaderTargetVersionsFromApi()` 目前有 1.0→1.3 四条分支 + `has\_spirv\_1\_4\_extension` 参数，产出 Vulkan/SPIR-V 目标版本并喂给 `GetShaderCompilerProfileHash`。硬性 1.4 后：

\- 整段函数删除，目标版本恒为 \*\*Vulkan 1.4 + SPIR-V 1.6\*\*；

\- `ShaderGenContract.h` 里 `PhysicalDeviceFeaturesLite` 的 `descriptor\_indexing`、`sampler\_mirror\_clamp\_to\_edge`、`index\_type\_uint8`（分别是 1.2/1.2/1.4 核心）变成恒 true，可整体移除；

\- `PhysicalDeviceProfileLite` 的 `target\_vulkan\_version`/`target\_spv\_version` 字段失去意义。



\### 2. 特性探测与 profile hash 塌缩

`GetPhysicalDeviceProfileHash` 目前把 vendor/device/版本/一堆 feature 位都算进缓存键——这正是此前"双路径、cache 键分歧"问题的根源之一。1.4 后：

\- 所有"因版本而异"的位消失，hash 只保留 `vendor\_id`/`device\_id`（仅用于个别驱动的 workaround 差异）；

\- `VKDeviceCreater.cpp::SetDeviceExtension` 里 `require\_ext\_list` 的条件探测（extended\_dynamic\_state 1/2/3、dynamic\_rendering、SPIRV\_1\_4、primitive\_topology\_list\_restart、line\_rasterization、index\_uint8）——这些在 1.3/1.4 已是核心，`CheckExtensionSupport` 分支全部删掉；

\- `SetDeviceFeatures` 的 `VulkanHardwareRequirement::SupportLevel` Want/Require 分级，对版本门控的特性（wide\_lines、geometry、tessellation、texture compression 等）只剩"设备能力"语义，不再是"版本探测"语义，可收敛为一次 `VkPhysicalDeviceFeatures2` 直填。



\### 3. Scalar Block Layout 已可用，进一步统一 SSBO 布局

`VertexShaderAssembler.h` / `glsl2spv.cpp` 已在 GLSL 里 emit `GL\_EXT\_scalar\_block\_layout`。1.4（1.2 核心）下这是无条件能力，可以放心让 ShaderGen 生成的 SSBO 布局\*\*直接按 C++ 结构体对齐\*\*，去掉 std140/std430 的 padding 折算逻辑。



\## 二、ECS / Descriptor 侧可简化（改动最大）



\### 4. Descriptor Set 体系 → Descriptor Buffer（1.4 核心新能力）

这是最激进也最值得做的一步。当前 ECS 有 \*\*4 个 descriptor set\*\*（Scene/Transform/Material/Bindless），涉及一整套机制：

\- `MaterialDescriptorManager`：逐材质拼 `VkDescriptorSetLayoutBinding` 数组；

\- `DescriptorSetLayoutAllocator`：逐材质分配 set/binding 编号；

\- `DescriptorSet`：`is\_dirty` 脏标记 + `VkWriteDescriptorSet` 数组组装 + `UpdateOrAppend\*` 合并；

\- `CreateDescriptorPool`：8 种 pool size 的大池；

\- `VKPipelineLayoutData::CreatePipelineLayoutData`：逐材质建 4 个 layout + 空 layout + fallback bindless layout。



换成 `VK\_EXT\_descriptor\_buffer`（1.4 核心）后：

\- 上述 \*\*descriptor set layout / pool / `vkUpdateDescriptorSets` / 脏标记 diff\*\* 全部消失；

\- 每帧只做 `vkCmdBindDescriptorBuffersEXT` + `vkCmdSetDescriptorBufferOffsetsEXT`，材质/对象数据就是 buffer 里的一段偏移，\*\*没有 set 句柄、没有生命周期追踪\*\*；

\- `ShaderDescriptor` 里的 `set`/`binding`/`preferred\_binding` 字段、`DescriptorSetType` 的四分类、`DescriptorSetLayoutAllocator` 整个类可删。



\### 5. Push Descriptor 吸收小绑定（1.4 核心新能力）

`VK\_KHR\_push\_descriptor` 在 1.4 已是核心（当前 `VKDeviceCreater.cpp` L59 还被注释着）。`Transform` set（每实例的 UBO）、以及单材质的少量 UBO/SSBO 可直接 `vkCmdPushDescriptorSet`，无 pool、无 set、无分配。配合上面第 4 条，"Scene/Transform/Material" 三个 set 可能收敛为 \*\*一个大 SSBO + 少量 push descriptor + push constant\*\*。



\### 6. 单一超大 SSBO + BDA 落地后，Bindless 管理器可进一步瘦身

`BindlessTextureManager` 目前仍是"pool + layout + set + 双 handle\_cache"的经典 set 实现。1.4 下可以：

\- 纹理数组改为 \*\*descriptor buffer 里的 sampled-image 数组\*\*，或用 BDA 在 SSBO 里直接存 image/sampler 句柄（`GL\_EXT\_nonuniform\_qualifier` 已在用）；

\- 删除 `CreateFallbackBindlessSetLayout` 这条 fallback 路径（`VKPipelineLayoutData.cpp` L44-80）——既然全量 bindless，fallback 是死代码；

\- `kMax2D=4096`/`kMax2DArray=4096` 的硬上限可直接对齐 `maxDescriptorSetSampledImages`，不再需要 set 里分 binding 槽位。



\### 7. null descriptor / maintenance6 → required/optional 语义简化

`ShaderGenContract.h` 的 `ResourceRequirement::required` 目前靠应用侧判断"资源是否绑定"。1.4 + robustness2 语义下，未绑定的 descriptor 直接读硬件 null descriptor（返回 0），`required` 分支、bindless 的 "0=无效 handle" 约定可以统一为"不绑就是 0"，删掉一套 sentinel 逻辑。



\### 8. Dynamic State（1.3 核心）→ pipeline 状态烘焙减少

extended\_dynamic\_state 1/2/3 在 1.3 已核心。当前 ShaderGen 的"resolved render state"（此前 P0 议题）之所以复杂，部分是因为要烘焙成静态 pipeline state。1.4 下 vertex input、rasterizer、depth/stencil 全动态，\*\*同一种类材质可复用同一个 pipeline\*\*，减少 pipeline 变体数量，`ShaderProgramManager` 的 pipeline 缓存键也能缩小。



\## 三、一句话总结



硬性 Vulkan 1.4 后，最大的收益是把"\*\*版本阶梯 + 特性探测 + descriptor set 池化\*\*"这三套通用机制全部删掉，收敛为：\*\*一个大 SSBO（BDA）+ descriptor buffer + push descriptor + 全动态管线状态\*\*。ShaderGen 的 contract/profile hash 塌缩成常量，ECS 的 `MaterialDescriptorManager`、`DescriptorSetLayoutAllocator`、`DescriptorSet`、多 set 布局这套逐材质绑定体系可以整体移除。



\*\*唯一要注意\*\*：1.4 不解决 mesh shader / ray tracing / opacity micromap 等（仍是扩展），如果未来要这些，`SetDeviceExtension` 的扩展探测机制还要保留一部分，不能全部删光。建议只删"版本门控"的那部分探测，保留"纯扩展能力"探测。

