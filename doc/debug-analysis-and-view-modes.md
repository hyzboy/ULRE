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
  生产路径目前仅登记 `GPUCullSystem.DrawCount`：它在图形队列上每次先清零，再由
  GPU 剔除 compute shader 原子累加，并在派发成功提交后处理请求。必须在该系统初始化前
  启用 DebugBridge；没有后续派发的请求会以 `queue_timeout` 结束。其 `frame_id`
  是该资源的成功派发序号，不是交换链帧号；其他生产 SSBO 尚未接入，不会被发现或抓取。
- `dump_render_target` 和 `dump_buffer` 是旧接口，只读取最近一条 C++ 抓取结果，
  不创建请求，也不保证结果属于调用者或当前帧。
- 纹理和缓冲 staging 读回在等待提交完成、映射后显式 invalidate 映射内存，
  invalidate 失败时不发布数据。旧 `FrameDebugger` 缓冲接口仍可能读到上传镜像，
  不能作为 GPU 写入的 SSBO 的可靠抓取接口。
- 新交换链抓帧同时发布 `ulre.capture.v1` JSON 清单：记录请求、目标、帧、
  原始文件、格式、布局和同帧 primitive-collect 相机快照；不同相机混用时标记
  `ambiguous`。颜色和深度清单还记录该目标实际 Vulkan 物理设备的 vendor/device ID、
  设备类型、API 和原始驱动版本及设备名称；Vulkan 1.2 驱动属性可用时记录 driver ID、
  名称和信息，否则标为 null。旧清单可能没有该字段；旧 C++ 抓取接口仍没有此清单。
- `set_debug_view` 现在按需请求独立离屏 Pass，复用主帧 GPU 剔除结果，
  不覆盖作者配方或主目标。`get_debug_view` 返回最近提交的模式和 `target_id`，
  失败时返回 `error`；开启请求并不意味着本帧已成功。关闭后注销目标并释放资源。
  开关是进程级，状态只反映最近处理的世界，不是世界选择接口；各世界的目标分别注册，
  多世界不能把全局状态当成指定世界的状态。
  启用期间每帧等待设备空闲后复用离屏附件，适合诊断而非性能测量；关闭会释放目标和附件，
  已创建的 shader、pipeline 和 render pass 由现有管理器缓存至其生命周期结束。
- 支持范围是带 PBRSurface 行、Normal 和 TexCoord 的 GPU-driven `StandardOpaque`
  场景批次及标准或 reversed-Z 活动相机；Position RT 还要求设备支持 RGBA32F
  颜色附件与 transfer-source 用途，否则状态明确报告 `position_format_unavailable`
  或目标创建错误，不会降级为量化颜色。不支持 alpha test、dither、混合和不兼容批次，
  遇到这些场景批次时整次视图失败（`unsupported_scene_batch`）。非 Scene 层不会被绘制，
  因而 mask 不代表天空、UI 或所有场景内容。`world_normal` 是归一化的几何世界法线，
  不包含法线贴图；`base_color` 为材质行乘基础色贴图，不包含顶点色；
  `surface` 为 roughness/metallic/AO，`emissive` 为自发光贴图，`uv0` 为 UV 小数部分。
  这些属性通道仍为 RGBA8，会量化并截断到 [0,1]，不是 HDR 或原始浮点数据。
  `world_position` 使用 RGBA32F，RGB 是 shader 的世界位置，alpha 为 0/1 有效像素 mask；
  元数据声明 IEEE-754 binary32、绝对世界坐标、Z-up 与零原点。当前变换路径没有启用
  camera-relative L2W 偏移；Position RT 与深度反投影均使用同帧矩阵和坐标语义。
  Float32 格式不代表固定物理误差，结果必须依据实际距离和交叉验证误差解读。
- `analyze_capture` 可读取 v1 清单进行亮度、R32F 深度及采样位置反投影；颜色空间未知时必须
  显式指定，深度统计使用清单中的实际清屏值；分析器按清单行跨度处理行填充。数值分析结果
  附带 `ulre.analysis.v1` 算法、参数、输入 capture ID、样本范围及限制信息。
  当前分析器限制单图最多 2,097,152 像素和 64 MiB，超限返回错误。
  Debug View 的颜色 alpha 为二值有效像素 mask；普通交换链没有 mask，
  统计包含背景，因此不能直接作为完整回归基线。
  交换链深度清单当前的 `clear_depth` 为 null；深度统计和无 mask 的反投影会显式拒绝，
  即使相机的 reversed-Z 已知也不会猜测实际清屏值。Debug View 深度的清屏值可用。
  清单反投影要求同帧绝对世界相机；由于相机快照只覆盖 primitive collect、
  未证明所有深度写入者使用同一相机，普通深度必须显式设置 `allow_unverified_depth_producers`。
  对 Debug View 使用一次 `color_depth` 请求，以返回的深度清单作为 `metadata_path`，
  颜色清单作为 `mask_metadata_path`；分析器验证 group、目标、帧、相机和 viewport，
  用同帧 alpha 排除背景，且不以清屏深度排除有效几何。
  手动反投影工具仍要求调用方传入同帧 inverse VP，不隐式获取最新相机。
  `world_position_rt` 分析直接 RT 的有效坐标范围；`position_crosscheck` 使用同一
  `color_depth` 请求的两个清单，逐样本比较 RGBA32F 坐标与反投影坐标，报告绝对误差
  及按采样相机距离三等分的 near/middle/far 误差分布。分组按本次样本等量划分，不是
  固定距离或 pass/fail 容差；一次最多比较 262,144 个采样点，超限需增大 sample_step。
- `analyze_shadow_semantics` 将显式标记为 shadow-map depth 的非清屏比例命名为
  投射物投影覆盖；它不是屏幕受影比例。只有真实同帧 receiver shadow factor/mask
  才能计算屏幕受影比例，目前没有这类生产附件，所以返回 unavailable。级联、PCF、
  bias 和 receiver mask 前提若未记录会保持未知。
- `compare_captures` 检查颜色附件的格式、尺寸、viewport 和相机矩阵，并在编码通道
  空间比较；它尚不验证清单设备身份，也缺少场景身份，必须显式允许未验证上下文。Debug View 的颜色误差、
  MAE/RMSE/PSNR、SSIM 和差异连通区域只使用两个有效 mask 的交集；mask 对称差异另行
  统计，不会被并入像素差。交换链比较包含整帧；均不能替代黄金基线。

`ULRE_DEBUG.toml` 中的 `debugbridge` 控制命名管道；诊断功能应显式启用。
SSIM 仅按下文固定窗口算法解释；当前没有可用的屏幕阴影受影比例。不要将其他未实现接口
描述为已支持。Position RT 是按需能力，
须遵守上述格式、相机、材质与 mask 前提。
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

默认通过同帧深度和 inverse VP 反投影；可按需输出 RGBA32F Position RT 并交叉验证。
像素中心依据捕获中的 viewport 原点、尺寸与深度范围转换为 Vulkan ZO NDC：

`u=(px+0.5-vx)/vw; v=(py+0.5-vy)/vh; ndc=(2u-1, 2v-1, (depth-min_depth)/(max_depth-min_depth), 1)`

正高度 viewport 的 Y 方向按 Vulkan framebuffer 映射处理；inverse VP 按列主序计算，
并执行齐次除法。清屏深度、viewport 外、非有限深度和不可除的齐次坐标会被过滤；
使用同帧有效像素 mask 时，深度恰好等于清屏值仍可代表有效几何。标准 Z/reversed-Z
依赖同帧 VP/inverse VP 与已记录 clear depth。当前坐标为绝对世界、RH、Z-up，camera-relative
原点偏移没有启用；未来启用时必须写入实际原点后才能恢复绝对坐标。误差只报告实测值，
受 float 表示、着色器插值、深度格式、距离和投影共同影响，不设固定物理阈值。

阴影贴图的非清屏比例只表示**投射物在光源投影中的覆盖**，不是屏幕上的受影比例。
后者需要真实的同帧接收者 shadow factor/mask，并注明级联、PCF 和 bias；当前生产路径
没有该附件，`analyze_shadow_semantics` 因此将 screen shadow ratio 报为 unavailable。

### 量化分析与比较

只解码明确支持且验证过的格式，考虑通道顺序、行跨度及颜色传递函数。亮度在声明的
线性颜色空间计算，sRGB 输入先线性化；NaN/Inf 单独计数并从有限值统计中排除。法线
统计仅覆盖有效像素；RGBA32F 的 XYZ 按有符号法线读取，RGBA8/10-bit 则按 UNORM
映射到 [-1,1]。Z 分布只是轴分布，不是朝向或正反面判断；缺少逐像素视线向量时不会
作正反面结论。粗糙度或金属度为 0 或 1 可能是正常材质值，端点占比仅是描述统计。
深度结果分别列出有效范围、NaN/Inf、超范围、清屏和非清屏比例；比例附有 mask/full-frame
分母说明，mask 支持时清屏值仍可属于有效几何。

帧对比以兼容的尺寸、格式、颜色空间、viewport 和相机为前提；场景/设备身份未验证时必须
显式 opt-in，结果标记 `opted_in_unverified`。MAE/RMSE 按有效交集中的 RGB 全通道样本
计算，并同时列出各通道值；PSNR 峰值为声明的 `signal_peak`（元数据色彩图像为 1）。
阈值比较的是编码通道的归一化值，严格使用任一 RGB 通道误差大于阈值的像素判定。
完全相等时 `psnr: null` 且 `psnr_infinite: true`，确保 JSON 合法。

SSIM 定义为 8×8 不重叠方窗；边缘窗口可小于 8×8，mask 交集外像素排除；每个窗口使用
总体方差/协方差及 `C1=(0.01×peak)^2`、`C2=(0.03×peak)^2`，逐 RGB 通道算 SSIM 后
等权平均，并按窗口内有效像素数加权为全图值。该 SSIM 在编码通道空间，不是感知色差指标。
差异区域只对交集 mask 内、超过阈值的像素做 4 邻接连通分析，图像最多 262,144 像素，
最多返回面积最大的 64 个区域，其余以 `regions_truncated` 标记；超过组件分析像素上限时
该项明确标记 unavailable，不影响其他差异指标。覆盖变化单独报告为 mask 对称差及 IoU。
热图采用唯一临时文件，覆盖变化用 255、通道差异用 1–254。分析输入单个文件上限为
64 MiB、图像上限为 2,097,152 像素；SSIM 和组件分析无需额外第三方库。

## 回归与验收目标

固定场景应包含已知相机、几何、法线、材质、遮罩与阴影，并固定时间、随机种子、
分辨率、预热与捕获帧。基线绑定场景版本、设备和驱动类别、格式及算法版本：
同环境可严格比较；跨环境使用显式容差；不存在匹配基线时返回 `not_covered`。
基线只能显式创建或更新，比较不得自动覆盖。

`RenderDiagnosticsFixture` 固定 640×360、静态几何和材质、相机、1/60 秒时间步、
主光与单级联 CSM；首次预热 120 帧，选择 Debug View 后再预热 120 帧，在第 241 帧
响应抓取。夹具冻结在预热点直到请求到达；不支持的 alpha test、shadow-map 采集与
屏幕受影因子不能计为已覆盖。该夹具没有引入随机场景输入。

Windows 上先构建 `RenderDiagnosticsFixture`，然后用以下命令显式建基线并比较：

```powershell
python tools\ulre-mcp-server\render_regression.py create --baseline C:\runs\normal.json --output-dir C:\runs\create --exe D:\ULRE\build\out\Windows_64_Debug\RenderDiagnosticsFixture.exe --mode world_normal
python tools\ulre-mcp-server\render_regression.py compare --baseline C:\runs\normal.json --output-dir C:\runs\compare --exe D:\ULRE\build\out\Windows_64_Debug\RenderDiagnosticsFixture.exe --mode world_normal
```

使用 `update` 明确更新已有基线；`create` 拒绝覆盖，`compare` 不修改基线。
输出 JSON 的状态为 `PASS`、`FAIL` 或 `NOT_COVERED`，进程退出码分别为 0、1、2。
结果包含捕获清单和热图路径；新基线清单绑定场景版本、固定帧配置及输入内容哈希、
实际设备/驱动、捕获格式与颜色空间、capture schema 和分析算法。相同设备及驱动
严格逐字节比较；跨设备/驱动默认 `NOT_COVERED`，仅同厂商、设备类型和 driver ID
且显式指定 `--cross-device-tolerance` 时按声明的编码通道容差比较，mask 差异仍失败。
容差须由使用者依据目标设备实测选定，不代表已认证的跨设备范围。
`--inject-fault camera|normal|material|coverage` 仅用于 compare 负向运行；
normal 需 `world_normal` 模式，material 需 `surface` 模式。
夹具对有效法线的非零和长度、固定平面与立方体顶面中心投影像素的已知 +Z 法线方向、
surface 的 metallic 均值执行数值检查；
`world_position` 输出同帧反投影数值，并支持 RGBA32F 位置基线比较：同设备逐字节严格
比较，跨设备显式容差的单位是世界坐标单位，mask 不一致始终失败。
覆盖变化故障是几何 mask 变化，不代表阴影覆盖变化已验收。
原始 `compare_captures` 仍是显式接受未验证上下文的图像比较，不能替代基线工具。
完整 BasicLitMeshes MCP 调用顺序见 `.ai/skills/SKILL_RENDER_DIAGNOSTICS.md`。

验收需覆盖已知像素的合成单元测试、协议错误和并发、实际 Vulkan 抓帧及 validation、
模式切换和资源生命周期、固定场景端到端与故障注入。GPU 不可用或某项能力未支持
应报告原因，不得作为通过。

坐标约定始终为 Vulkan、右手、Z-up、ZO；正高度 viewport 下投影 X 不翻转、Y 翻转。
引擎新增代码遵循 HGL 类型、内存及日志规范。
