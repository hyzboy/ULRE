# ShaderGen Runtime/JSON 一致性基线（2026-03-05）

## 1. 目标

建立 `Runtime profile` 与 `JSON profile` 在同输入下行为一致的核对基线，用于 Phase D 收口与后续回归。

---

## 2. 当前主链路

1. Runtime：`VulkanPhyDevice` 生成 `PhysicalDeviceProfileLite`（初始化时缓存）。
2. Device creation：`VKDeviceCreater` 一次性把缓存 profile 下发给 `GLSLCompiler`。
3. JSON：collector JSON 解析为同一 `PhysicalDeviceProfileLite`。
4. 编译器：统一进入同一 profile apply 逻辑（Vulkan/SPV 目标 + limits）。

---

## 3. 对齐检查项（同输入应一致）

| 检查项 | 期望 |
|---|---|
| Vulkan target version | 一致 |
| SPV target version | 一致 |
| 顶点属性相关 limit | 一致 |
| UBO/SSBO 预算相关 limit | 一致 |
| descriptor set 约束结果 | 一致 |
| geometry/tessellation 开关行为 | 一致 |
| strict gate 分类 | 一致（profile 违规为 `StrictGate.Profile`） |

---

## 4. 基线执行清单（手工）

- 构建：`ULRE.ShaderGen`、`ULRE.Vulkan`、`ULRE.SceneGraph`
- 测试目标：
  - `test_ShaderGenPhysicalDeviceProfileJsonPath`
  - `test_ShaderGenContractValidatorProfile`
  - `test_RendererShaderGenAdapterProfileCategory`
  - `test_MaterialPresetExhaustiveCompile`
- 示例目标：`03_BasicLitSunDirectionECS`

执行后记录：
- 编译目标版本摘要（Vulkan/SPV）
- 关键 diagnostics 差异（若有）
- strict gate 分类统计差异（若有）

### 4.1 自动化脚本（建议）

可使用脚本一次性执行 profile 相关基线测试并导出 JSON 摘要：

- 脚本：`tools/shadergen_profile_parity_runner.py`
- 默认输出：`doc/shader-system/baseline/shadergen_profile_parity_latest.json`

示例（仓库根目录）：

```powershell
python tools/shadergen_profile_parity_runner.py
```

可选参数：

- `--bin-dir`：测试 exe 所在目录（默认 `build/windows-msvc-debug/out/Windows_64_Debug`）
- `--timeout`：单测超时秒数
- `--output`：输出 JSON 路径
- `--tests`：自定义测试 exe 列表
- `--extended`：追加运行集成型测试（可能依赖更完整运行上下文）

说明：

- 默认执行“稳定 smoke 集”（JSON profile 路径 + validator profile 规则）。
- 集成型测试（如 SceneGraph/大规模材质编译）建议在本地完整环境下用 `--extended` 单独执行。

### 4.2 CMake/CTest 入口

- CMake 目标：
  - `test_ShaderGenProfileParitySmoke`
  - `test_ShaderGenProfileParityExtended`
- CTest：
  - `test_ShaderGenProfileParitySmoke` 默认启用
  - `test_ShaderGenProfileParityExtended` 默认 `DISABLED`（需手动启用后执行）

### 4.3 一条命令封装（PowerShell）

- 脚本：`tools/run_shadergen_parity.ps1`

示例：

```powershell
# 默认 smoke（Debug + build/windows-msvc-debug）
powershell -ExecutionPolicy Bypass -File tools/run_shadergen_parity.ps1

# extended
powershell -ExecutionPolicy Bypass -File tools/run_shadergen_parity.ps1 -Mode extended
```

### 4.4 一条命令封装（Batch）

- 脚本：`tools/run_shadergen_parity.bat`

示例：

```bat
REM 默认 smoke
tools\run_shadergen_parity.bat

REM extended
tools\run_shadergen_parity.bat extended

REM 自定义构建目录与配置
tools\run_shadergen_parity.bat smoke build\windows-msvc-debug Debug
```

---

## 5. 当前结论（截至 2026-03-05）

- 代码路径已统一到同一 profile DTO 与同一 apply 入口。
- 材质层已去除 profile 参数透传，避免每材质重复切换导致行为漂移。
- 核心构建与关键示例目标通过；可进入“持续对比回归”阶段。

---

## 6. 后续自动化建议

- 在 CI 中新增一个轻量 parity job：
  - 固定一个 profile 输入（runtime dump + JSON）
  - 对比目标版本与关键 diagnostics 摘要
- 将差异阈值定义为：
  - 版本差异：不允许
  - strict gate 分类差异：不允许
  - diagnostics 文本差异：允许顺序差异，不允许语义差异
