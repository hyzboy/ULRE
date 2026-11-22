# 渲染自省：调试视图与图像分析

## 当前状态

引擎提供 GPU 时间戳、管线统计、健康账本、C++ 抓取接口和 Windows 命名管道；
`tools/ulre-mcp-server` 提供桥接与图像分析。当前实现仍是诊断原型，不能视为完整、
可靠的端到端抓帧系统：

- `list_capture_targets` 列出交换链、Debug View 和管理器创建的普通离屏目标；`capture_frame` 接受 target ID 和
  `color`/`depth`/`color_depth`，返回请求 ID，随后可通过 `get_capture_status` 查询。
  `color_depth` 在同一个提交帧内读取两个附件，返回各自的清单与文件。抓取只在该
  交换链提交后、present 前执行；普通离屏目标在该目标提交成功后执行。`cancel_capture` 可取消排队请求；终态需调用
  `release_capture` 释放状态槽，文件不会自动删除。排队超时为 30 秒。
  深度模板格式仅读回深度 aspect，返回的 `pixel_size`/`row_pitch` 描述副本，
  不等于深度模板联合像素布局。独立请求可能属于不同帧，必须检查 `frame_id`。
- 普通离屏目标的资源所有者必须在渲染线程显式调用 `EnableManagedDebugCapture()`，
  保证附件只由托管渲染路径写入，且所有访问均由图形队列执行并遵守资源生命周期。
  未 opt-in 的目标显示 `availability: unavailable`、无可抓取附件。当前只支持零或一个颜色附件，
  且只在完整清屏、非 scissor 的托管 pass 成功结束并提交到图形队列后读回；
  load、scissor、其他队列和不符合契约的帧返回 `frame_unavailable`。
  多颜色附件、缺少 transfer-source usage、未知格式不可抓取。
  `attachment_details` 描述至多前 8 个颜色附件的索引、格式和可用性，以及深度附件；
  `color_attachment_count` 始终报告实际数量。
  普通离屏 `frame_id` 是该目标的成功托管提交序号，不是交换链全局帧号；相机、viewport
  标记为 unavailable/null，不可用于需要这些信息的深度分析；符合条件的 pass
  以 0 清空深度，清单记录该实际值。
- `list_capture_buffers` 和 `capture_buffer` 是独立的 SSBO 请求接口，不接受客户端 GPU
  指针或输出路径。资源所有者须在渲染线程登记 `DeviceBuffer`，保证资源存活、无并发修改，
  所有 GPU 访问均在图形队列并按提交顺序同步；每次生产者成功提交后在同一线程调用
  `ProcessBuffer`，传入实际图形队列和该资源单调递增的非零提交序号。销毁在同一线程执行，
  `DeviceBuffer` 析构时自动注销并取消排队请求，也可提前注销。
  没有这一契约的资源不得登记为可用；处理失败或队列不匹配不会返回旧数据。
  只允许包含 `STORAGE_BUFFER` 和 `TRANSFER_SRC` 的真实 device buffer，offset 与 size
  必须四字节对齐、位于实际 VkBuffer 大小内，单次最多 4 MiB。拷贝从 device buffer
  到 staging，非从 CPU 上传镜像读取；清单 `ulre.buffer_capture.v1` 记录 resource ID、
  request ID、提交序号、字节范围和真实 usage。注册但不满足 usage 的资源标记不可用。
  当前只有测试夹具登记了缓冲；生产 SSBO 尚未接入此契约，因此默认不会被发现或抓取。
- `dump_render_target` 和 `dump_buffer` 是旧接口，只读取最近一条 C++ 抓取结果，
  不创建请求，也不保证结果属于调用者或当前帧。
- 纹理和缓冲 staging 读回在等待提交完成、映射后显式 invalidate 映射内存，
  invalidate 失败时不发布数据。旧 `FrameDebugger` 缓冲接口仍可能读到上传镜像，
  不能作为 GPU 写入的 SSBO 的可靠抓取接口。
- 新交换链抓帧同时发布 `ulre.capture.v1` JSON 清单：记录请求、目标、帧、
  原始文件、格式、布局和同帧 primitive-collect 相机快照；不同相机混用时标记
  `ambiguous`。旧 C++ 抓取接口仍没有此清单。
- `set_debug_view` 现在请求独立 RGBA8 UNORM 离屏 Pass，复用主帧 GPU 剔除结果，
  不覆盖作者配方或主目标。`get_debug_view` 返回最近提交的模式和 `target_id`，
  失败时返回 `error`；开启请求并不意味着本帧已成功。关闭后注销目标并释放资源。
  开关是进程级，状态只反映最近处理的世界，不是世界选择接口；各世界的目标分别注册，
  多世界不能把全局状态当成指定世界的状态。
  启用期间每帧等待设备空闲后复用离屏附件，适合诊断而非性能测量；关闭会释放目标和附件，
  已创建的 shader、pipeline 和 render pass 由现有管理器缓存至其生命周期结束。
- 支持范围是带 PBRSurface 行、Normal 和 TexCoord 的 GPU-driven `StandardOpaque`
  场景批次及 reversed-Z 活动相机；不支持 alpha test、dither、混合和不兼容批次，
  遇到这些场景批次时整次视图失败（`unsupported_scene_batch`）。非 Scene 层不会被绘制，
  因而 mask 不代表天空、UI 或所有场景内容。`world_normal` 是归一化的几何世界法线，
  不包含法线贴图；`base_color` 为材质行乘基础色贴图，不包含顶点色；
  `surface` 为 roughness/metallic/AO，`emissive` 为自发光贴图，`uv0` 为 UV 小数部分。
  RGBA8 会量化并截断到 [0,1]，不是 HDR 或原始浮点数据。
- `analyze_capture` 可读取 v1 清单进行亮度、R32F 深度及采样位置反投影；颜色空间未知时必须
  显式指定，深度统计使用清单中的实际清屏值；分析器按清单行跨度处理行填充。
  当前分析器限制单图最多 2,097,152 像素和 64 MiB，超限返回错误。
  Debug View 的颜色 alpha 为二值有效像素 mask；普通交换链没有 mask，
  统计包含背景，因此不能直接作为完整回归基线。
  清单反投影要求同帧绝对世界相机；由于相机快照只覆盖 primitive collect、
  未证明所有深度写入者使用同一相机，普通深度必须显式设置 `allow_unverified_depth_producers`。
  对 Debug View 使用一次 `color_depth` 请求，以返回的深度清单作为 `metadata_path`，
  颜色清单作为 `mask_metadata_path`；分析器验证 group、目标、帧、相机和 viewport，
  用同帧 alpha 排除背景，且不以清屏深度排除有效几何。
  手动反投影工具仍要求调用方传入同帧 inverse VP，不隐式获取最新相机。
- `compare_captures` 检查颜色附件的格式、尺寸、viewport 和相机矩阵，并在编码通道
  空间比较；当前缺少场景和设备身份，必须显式允许未验证上下文。Debug View 使用
  两个 mask 的并集并报告覆盖变化，交换链比较包含背景；均不能替代黄金基线。

`ULRE_DEBUG.toml` 中的 `debugbridge` 控制命名管道；诊断功能应显式启用。
不要将未实现的接口、SSIM、阴影受影比例或 Position RT 描述为已支持。
管道按单次写入的一条消息接收请求，当前请求上限为 4095 字节；方法和参数按
JSON 字段匹配，响应保留请求 ID。桥接线程的连接与 I/O 可在停机时取消。

## 补强目标

引擎渲染线程负责资源注册、抓取时序和数据回读；桥线程解析请求并投递命令，
不直接持有跨线程的 Vulkan 资源。MCP 接收结构化元数据及文件引用，再在本地分析。
默认禁用诊断，不分配额外目标、不增加调试绘制或回读。

### 请求与生命周期

目标协议通过稳定资源 ID 发起抓取并返回唯一 request ID，状态为 `queued`、
`capturing`、`succeeded`、`failed` 或 `cancelled`。每个结果与请求和帧绑定，
失败需保留原因，禁止用旧结果代替新结果。队列容量、超时、资源失效与关闭行为
须明确。交换链颜色仅可在 acquire 后、present 前的合法窗口读取。

现有读取最近结果的旧接口在迁移期间保留，但必须明确标注其不会触发抓帧。
命名管道仅面向本机、显式启用，限制消息大小，正确解析 JSON-RPC 字段、转义与 ID。

### Capture v1 元数据

当前 Windows 交换链抓取先写临时原始数据与 JSON，再依次发布原始文件和清单，
只有两者均完成后请求才进入 `succeeded`；状态返回绝对路径。清单中的 `data_file`
相对于清单目录，原始数据不包含行头。清单目前包括请求、目标、帧、尺寸、VkFormat、
编码、行跨度、字节数、aspect、viewport 和 primitive-collect 相机；颜色空间未知时
明确标为 `unknown`，mask 不可用时标为 `unavailable`。

例如：`capture_frame({"target_id":1,"attachment":"depth"})` 返回 request ID；
随后 `get_capture_status({"request_id":1})` 在成功时提供 `dump_path` 和
`metadata_path`；`analyze_capture({"metadata_path":"<绝对路径>","operation":"depth"})`
读取清单和数据。不同请求即使连续发出也可能捕获不同帧。

目标契约还需包括 schema 版本、capture/request/frame ID、目标及 pass、相机标识、
附件类型、尺寸、VkFormat、像素布局和数值类型、颜色空间、行跨度、字节序、实际
字节数、采样数、mip/层、编码和有效像素 mask。深度附件还需声明清屏值、深度范围、
viewport、reversed-Z、矩阵布局和同帧 VP/inverse VP；相机相对坐标还需明确原点。
未知格式、损坏文件及缺失必需字段必须显式拒绝，不能猜测布局。

### 独立 Debug Override Pass

按需插入独立离屏 Pass，复用主场景的几何、实例、变换和剔除结果，不修改作者配方。
当前支持范围和限制如上；实际着色法线以及 alpha test/dither 覆盖尚未实现。
不支持的场景批次不会被静默当成有效结果。关闭后不改变主帧或作者状态。

### 世界坐标与阴影

默认通过同帧深度和 inverse VP 反投影；可按需输出高精度 Position RT 交叉验证。
像素中心依据 capture 中的 viewport 原点、尺寸和深度范围转换为 Vulkan ZO NDC；
矩阵顺序、Y 方向以及是否需要补回相机相对原点均由同帧元数据决定。需过滤清屏、
无效深度和不可除的齐次坐标。精度依赖格式、距离及投影，不保证固定物理误差。

阴影贴图的非清屏比例表示**投射物覆盖**，不是屏幕上的受影比例。后者需要同帧
受影因子或 mask，并注明级联、PCF 和 bias；缺少数据时返回 unavailable。

### 量化分析与比较

只解码明确支持且验证过的格式，考虑通道顺序、行跨度及颜色传递函数。亮度在声明的
线性颜色空间计算；法线统计仅覆盖有效像素，朝向判断需要真实视线向量。粗糙度或
金属度为 0 或 1 可能是正常材质值，不应仅凭该值判错。

帧对比以兼容的尺寸、格式、颜色空间、相机及有效 mask 为前提。MAE 和 RMSE 按
参与比较的全部通道样本计算；像素变化比例使用明确的像素阈值。完全相等时用
JSON 合法的显式语义表示无限 PSNR。热图路径必须唯一，不能覆盖先前结果。
SSIM、连通区域和任何阈值只有在实现及测试后才能作为可用能力公布。

## 回归与验收目标

固定场景应包含已知相机、几何、法线、材质、遮罩与阴影，并固定时间、随机种子、
分辨率、预热与捕获帧。基线绑定场景版本、设备和驱动类别、格式及算法版本：
同环境可严格比较；跨环境使用显式容差；不存在匹配基线时返回 `not_covered`。
基线只能显式创建或更新，比较不得自动覆盖。

验收需覆盖已知像素的合成单元测试、协议错误和并发、实际 Vulkan 抓帧及 validation、
模式切换和资源生命周期、固定场景端到端与故障注入。GPU 不可用或某项能力未支持
应报告原因，不得作为通过。

坐标约定始终为 Vulkan、右手、Z-up、ZO；正高度 viewport 下投影 X 不翻转、Y 翻转。
引擎新增代码遵循 HGL 类型、内存及日志规范。
