# CMLiveAudio 升级计划 —— Windows 直播设备（实时混音 + 效果型变声）

> 状态：草案 v1
> 日期：2026-08-16
> 关联模块：CMAudio（DSP 复用源）、CMCore（基础库）、CMCMakeModule（构建体系）
> 形态：Windows 软件（本机直播，抓游戏声 + 变声 + 喂 OBS）
> 变声路线：效果型 DSP（萝莉/大叔/怪兽/机器人/电话/电音），**不做** AI 音色克隆（RVC）

---

## 一、目标与边界

### 1.1 产品目标

一个 Windows 直播音频设备软件，信号流：

```
输入（采集 + 播放双路径）
  ① 麦克风采集
  ② 游戏/系统声音（WASAPI Loopback）
  ③ 媒体播放（音乐/伴奏）
      │
      ▼ 每路通道 = 源 → [可选效果链] → 通道条 → 混音台
处理
  麦克风链：噪声门→去齿音→EQ→压缩→限幅→[变声器]→发送混响
  播放链：  [变声器/效果器也可挂载]→闪避(说话时压低)→EQ
  混音台： 各通道 gain/pan/mute/solo + 监听 mix / 推流 mix 分离
      │
      ▼
输出
  ① 监听耳机（超低延迟）
  ② 推流虚拟设备 → OBS
```

### 1.2 关键架构要求（本次升级的核心）

> **"音声器（变声/效果链）必须同时支持采集路径与播放路径，而不只是录音/麦克风路径。"**

因此架构必须是**统一的音频图模型**：采集源、loopback 源、媒体播放源在图中地位完全平等，
都可挂载任意效果链（含变声器 VoiceFX）。效果链挂在"通道"上，而非挂在"设备"上。
这是 CMLiveAudio 与普通声卡工具的本质区别，也是任务 T10/T22 的验收标准。

播放端效果典型场景：
- 伴奏/音乐 → 混响/回声（跟唱）
- 游戏声（loopback）→ 响度统一（压缩 + EQ）
- 播放内容变声（玩梗：给视频声音变萝莉/大叔）
- 提示音 → 电话效果

### 1.3 边界（本期不做）

- ❌ AI 音色克隆变声（RVC/So-VITS）——如未来需要，作为 VoiceFX 节点预留接口
- ❌ 嵌入式/硬件形态
- ❌ 完整的 GUI 调音台（先控制台验证 + 参数接口，GUI 为后续独立交付，见决策点 D1）
- ❌ ASIO 优先支持（WASAPI shared/exclusive 起步，ASIO 为可选后续）

---

## 二、模块归属与目录

### 2.1 决策点 D2：新建独立模块 CMLiveAudio

与 CMAudio（游戏音频库，OpenAL 后端）职责分离；DSP 单元通过链接 CMAudio 静态库复用
（`Compressor`/`BiquadFilter`/`ParametricEQ`/`TimeEffects`/`LoudnessMeter` 等均为纯 CPU
逐样本类，无 OpenAL 依赖，可直接在实时图中使用）。

- 命名空间：`hgl::audio::live`（与 CMAudio 的 `hgl::audio` 自然互操作）
- 头文件：`CMLiveAudio/inc/hgl/audio/live/`
- 源码：`CMLiveAudio/src/`
- 示例：`CMLiveAudio/examples/`（封装宏 `cm_live_audio_example`，VC 文件夹 `Examples/CMLiveAudio/<sub>`，
  风格与 `cm_audio_example`/`cm_example_project` 一致）
- 依赖：CMCore、CMAudio（静态库）、Windows SDK（WASAPI：`Mmdeviceapi.h`/`Audioclient.h`/`AudioPolicy.h`）、
  libsamplerate（经 CMAudio 链接，流式 SRC 需新包装）

目录规划：

```
CMLiveAudio/
├── CMakeLists.txt            # 仿 CMAudio：path_config + add_subdirectory
├── path_config.cmake         # CMLiveAudioSetup() 宏
├── inc/hgl/audio/live/       # 公共头
│   ├── DeviceManager.h       # WASAPI 设备枚举（播放/采集/loopback）
│   ├── WasapiInputStream.h   # 采集 + loopback
│   ├── WasapiOutputStream.h  # 低延迟输出（回调驱动）
│   ├── AudioGraph.h          # 图框架：Node/Port/块
│   ├── AudioParameter.h      # 无锁参数 + 斜坡
│   ├── SourceNode.h          # Capture/Player 源节点
│   ├── ChainNode.h           # 效果链挂载点（统一，播放端也走这里）
│   ├── MixNode.h             # 混音 + 通道条
│   ├── NoiseGate.h           # 噪声门（新写）
│   ├── PitchShifter.h        # WSOLA 变调不变速（新写）
│   ├── FormantShifter.h      # 共振峰变换（新写）
│   ├── PitchDetector.h       # YIN 基频检测（电音用）
│   ├── VoiceFX.h             # 变声器 + 预设
│   ├── CpuReverb.h           # Schroeder 混响（新写）
│   └── LiveMixer.h           # 混音台：监听/推流双 mix + 闪避
├── src/
├── examples/
│   ├── CMakeLists.txt        # cm_live_audio_example 宏
│   ├── 01_io_latency/        # 延迟测量
│   ├── 02_loopback_test/
│   ├── 03_graph_minimal/     # 麦克风→监听
│   ├── 04_voice_chain/
│   ├── 05_voicefx/           # 变声器（采集+播放双路验收）
│   └── 06_stream_output/     # 推流路由
└── doc/                      # Hugo 格式手册（与 CMAudio/doc 一致）
```

---

## 三、技术选型

| 领域 | 方案 | 理由 |
|---|---|---|
| 采集 | WASAPI shared 小缓冲（目标 10ms），loopback 抓系统声 | 原生、免驱动；exclusive 仅作可选增强（部分声卡驱动不稳） |
| 输出 | WASAPI shared 起步（原型），监听路径后续换 exclusive | 延迟预算：**环路 <20ms 达标，<10ms 理想** |
| 图模型 | 自研 pull-based 图，固定块 512 样本 @48kHz float32 | 无现成轻量 C++ 库（PortAudio 可作备选后端，见 D3） |
| 线程 | 音频线程 = WASAPI 回调内跑全图；SPSC 环形缓冲；音频线程零分配 | 直播低延迟硬要求 |
| 变调不变速 | WSOLA（时域） | 延迟 20-40ms 可控、CPU 低、语音音质好 |
| 共振峰 | LPC 分解（阶 ~24） | 男↔女自然度关键；仅面向人声 |
| 混响 | Schroeder/FDN（新写 ~200 行） | CPU 混响，与 OpenAL EFX 无关 |
| 解码 | 复用 CMAudio `AudioPlugInInterface`（WAV/Ogg/Opus 插件） | 播放源节点直接受益 |
| SRC | libsamplerate `SRC_STATE` 流式 API（新包装） | `AudioResampler` 是一次性整段，不满足流式 |
| 电平表 | 复用 `LoudnessMeter`（48kHz 流式） | 通道表头 + 响度显示 |
| 限幅 | 复用 `Compressor`（ratio≈20:1） | Master 输出防削波 |
| 配置 | TOML（与 CMAudio `SoundEventManager`/`AudioMixerScene` 风格一致） | 预设/场景保存加载 |

---

## 四、任务清单（逐任务，每任务编译 + 运行验证后再下一个）

### 阶段 A：模块与验证环境

| # | 任务 | 内容 | 验证 |
|---|---|---|---|
| T1 | 模块骨架 | CMLiveAudio 目录 + `path_config.cmake` + `CMakeLists.txt`，接入 ULRE 构建；`inc/hgl/audio/live/` 空命名空间 | `cmake --build build --target CMLiveAudio --config Debug` 通过 |
| T2 | 示例宏与空例程 | `cm_live_audio_example` 宏（仿 `cm_audio_example`），一个空例程；VC 文件夹 `Examples/CMLiveAudio/<sub>` | 例程 build + 运行，VS 工程树结构正确 |

### 阶段 B：IO 层（WASAPI）

| # | 任务 | 内容 | 验证 |
|---|---|---|---|
| T3 | WASAPI 可行性 spike | 最小程序：默认设备采集→输出环路，实测往返延迟（`GetCurrentPadding` 推算）；验证 loopback 能抓到系统声 | 延迟报告：shared 各缓冲档位实测值；loopback 抓到声音的波形证据 |
| T4 | DeviceManager | MMDevice 枚举播放/采集/loopback 设备 + FriendlyName | 例程列出全部设备名 |
| T5 | WasapiInputStream | shared 采集（普通 + loopback 模式），SPSC 环形缓冲输出块 | 例程：抓取到样本、采样率/格式正确、无撕裂 |
| T6 | WasapiOutputStream | 回调驱动输出（`GetBuffer` 内预留"拉图"入口），小缓冲 | 例程：正弦波输出无爆音 |
| T7 | 延迟仪表 | 输出测试信号 → loopback 回采 → 互相关测延迟，持续显示 | 实测环路延迟达标（<20ms，目标 <10ms）；记录到日志 |

> 里程碑 M1（T1-T7）：IO 层达标，可抓游戏声，环路延迟达标。

### 阶段 C：实时音频图（核心）

| # | 任务 | 内容 | 验证 |
|---|---|---|---|
| T8 | AudioGraph 框架 | Node/Port、固定块 512@48k float32、图连接、`ProcessBlock` 拓扑排序 | 单元测试：图拓扑正确、块输出正确 |
| T9 | AudioParameter | 无锁（原子 + SPSC）+ 参数斜坡（防爆音，借鉴 `GainRamp` 思路） | 测试：高频改参无爆音、斜坡平滑 |
| T10 | SourceNode 族 | CaptureSource（接 T5）、PlayerSource（文件播放，复用 CMAudio 解码插件） | 两源节点分别出声音 |
| T11 | ChainNode | **统一效果链挂载点**（任意源可挂链——播放端支持的关键） | 测试：同一 ChainNode 挂到采集源与播放源均生效 |
| T12 | MixNode + 通道条 | N 输入混音（float32）+ gain/pan/mute/solo + Master 输出限幅（复用 Compressor） | 多路混合无削波、通道控制正确 |
| T13 | 最小可闻回路 | 麦克风 →（无效果）→ 监听耳机（T6 输出），全图跑通 | 人耳试听无爆音、可感延迟可接受；长时间运行稳定 |

> 里程碑 M2（T8-T13）：图骨架跑通，麦克风→监听闭环。

### 阶段 D：人声链与混音台

| # | 任务 | 内容 | 验证 |
|---|---|---|---|
| T14 | NoiseGate | 新写：电平门限 + attack/release + 软过渡 | 测试：低于门限静音、过渡平滑无咔哒 |
| T15 | 人声链预设 | Gate→De-esser(PEQ 高频衰减)→EQ(PEQ)→Comp→Limiter（全部复用 CMAudio），参数走 AudioParameter | 例程：说话声音链路正确、压缩曲线可见 |
| T16 | 双混音路由 | 监听 mix / 推流 mix 分离（监听可听"干净人声"，推流带效果） | 两路输出互不影响 |
| T17 | 说话闪避 | 人声电平（侧链）→ 播放通道 Duck（概念移植自 `AudioBus::UpdateSidechainDuck`） | 说话时音乐/游戏声自动压低、平滑恢复 |

> 里程碑 M3（T14-T17）：人声链完整，直播设备基础功能可用。

### 阶段 E：变声器（效果型）+ 播放端支持

| # | 任务 | 内容 | 验证 |
|---|---|---|---|
| T18 | PitchShifter（WSOLA） | 实时变调不变速，±12 半音，块处理（512/1024），延迟可测 | 测试：固定音调输入（正弦/人声 WAV）→ 变调后语速不变、音高正确 |
| T19 | FormantShifter | LPC 分解 → 声道滤波器缩放 → 合成 | 男声→女声/女声→男声试听自然度 |
| T20 | 效果调制器 | RingMod（机器人）、失真/位深（怪兽基底）、带通+饱和（电话） | 各效果输出 WAV 对比 |
| T21 | PitchDetector（YIN） | 基频检测 → 音阶量化（电音/auto-tune） | 单音/人声检测准确率测试 |
| T22 | VoiceFX 预设 | 萝莉/大叔/怪兽/机器人/电话/电音 六预设，参数化可调 | 六预设实时切换无爆音、试听正确 |
| T23 | **播放端变声验收** | 变声器经 ChainNode 同时挂采集链与播放链：麦克风变声 + 音乐/伴奏变声（+游戏声变声） | **核心验收**：双路同时变声可听、切换/挂载运行稳定；输出 WAV 留档 |

> 里程碑 M4（T18-T23）：变声器完成，采集/播放双路径效果支持闭环（用户核心需求）。

### 阶段 F：播放端完善 + 产品化

| # | 任务 | 内容 | 验证 |
|---|---|---|---|
| T24 | PlayerSource 完善 | 播放列表/循环/淡入淡出（借鉴 `GainRamp`）、流式 SRC（libsamplerate `SRC_STATE` 新包装，支持任意采样率输入） | 不同采样率文件连续播放无卡顿、淡入淡出正确 |
| T25 | CpuReverb + Send/Return | Schroeder 混响 + 图内发送/返回节点 | K 歌场景：人声+混响、伴奏不变 |
| T26 | 输出路由 | 监听 + 虚拟设备（推流）双输出；通道电平表（复用 LoudnessMeter） | OBS 侧收到完整混音、电平表正确 |
| T27 | 预设与收尾 | 预设/场景 TOML 保存加载、参数自动化收尾、长时间稳定性测试（≥30min 不掉帧/不爆音/延迟稳定）、`doc/` 手册（Hugo 格式） | 稳定性日志 + 文档齐全 |

> 里程碑 M5（T24-T27）：产品化——双输出、预设、文档。

---

## 五、里程碑总览

| 里程碑 | 范围 | 验收 |
|---|---|---|
| M1 | T1-T7 | WASAPI 采集/loopback/输出可用，环路延迟 <20ms（目标 <10ms） |
| M2 | T8-T13 | 音频图跑通，麦克风→监听闭环无爆音 |
| M3 | T14-T17 | 人声链 + 双 mix + 闪避，基础直播设备可用 |
| M4 | T18-T23 | 变声器六预设，**采集与播放双路径效果支持**（核心） |
| M5 | T24-T27 | 推流路由、预设、文档、稳定性 |

依赖顺序说明：图框架（C）必须先于变声器（E）——变声器是 ChainNode 的一个节点；
播放端支持（T23）依赖图模型（T11）而非依赖任何采集特性，故"播放端也支持"从架构上
（而非从功能上）先行保障。IO 层（B）与 DSP 变声器（E）无相互依赖，但图骨架未就绪前
变声器无法挂载验证，故排后。

---

## 六、风险与对策

| 风险 | 影响 | 对策 |
|---|---|---|
| WASAPI exclusive 部分声卡驱动不稳 | 监听延迟无法到 <10ms | shared 小缓冲兜底（10-20ms 可接受）；exclusive 作可选模式 |
| WSOLA 延迟/音质权衡 | 变声有 ~20-40ms 附加延迟 | 语音场景可接受（监听路径可旁路变声）；窗长/重叠率参数化 |
| LPC 共振峰对音乐不稳定 | 播放端变声自然度下降 | 播放端变声以 WSOLA + 调制效果为主，Formant 主要面向人声 |
| 音频线程零分配约束下图实现复杂 | 开发周期风险 | SPSC 池化 + 预分配节点缓冲，单元测试先行 |
| 多设备时钟漂移 | 长时间运行掉帧/卡顿 | 流式 SRC + 轻微拉取补齐机制（T24） |

---

## 七、决策点（待确认）

- **D1 交付形态**：核心做静态库 + 控制台例程 + 参数接口（本期），GUI 调音台（Qt/ImGui/自研）作为后续独立交付？还是本期就要 GUI？
- **D2 模块归属**：新建独立模块 CMLiveAudio（本计划默认）；或并入 CMAudio 开 `live/` 域（省去模块注册，但混淆"游戏音频库"定位）
- **D3 后端备选**：本计划自研 WASAPI 层；若想降风险，可用 PortAudio 抽象（自带 WASAPI/ASIO/WDM-KS 后端）替换 T4-T6，DSP 图不变
