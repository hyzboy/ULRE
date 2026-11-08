# EnvSunPicker —— .env 环境资产太阳方向拾取工具

打开一个统一天空资产（`.env` 二进制 / `.env.toml` 文本），站在天空穹顶**内部**
预览它，从世界原点画一根指向当前太阳方向的线段；**左键点击**球面即把该方向
写回 `[sun].direction`，`Ctrl+S` 保存（双格式同写），`Esc` 退出。

## 运行

```
EnvSunPicker.exe [资产路径]
```

- 不带参数：弹系统「打开」框选择 `.env` / `.env.toml`（仅 Windows）。
- 带参数：直接打开该文件（相对当前工作目录解析；建议从仓库根或 `build\out\<配置>` 运行，
  以便找到 `asset_mount.toml` 与 ShaderLibrary 资产）。
- CMake 目标 `EnvSunPicker`（VS 里调试时工作目录已设为运行时目录）。

## 操作

| 按键 / 鼠标 | 作用 |
|---|---|
| 左键拖拽 | 环视（FirstPerson 相机，位置钉在世界原点） |
| 左键点击（位移 < 6px） | 把射线方向写入 `[sun].direction`（并同步主光方向、重装配档位） |
| `1`..`5` | 在资产**实际存在**的技术档位间切换预览（CubeMap / Octahedral / LambertDome / Atmosphere / SH9） |
| `Ctrl+S` | 保存为 `<stem>.env` 与 `<stem>.env.toml` 双格式 |
| `Esc` | 退出（有未保存修改时先警告，再按一次才放弃退出） |

起步档位 = 资产 `[ambient].mode` 指向的技术（若该技术在资产中存在），否则第一个可用档位。

## 语义约定（重要）

- 点击**永远只写 `[sun].direction`**，不改任何烘焙贴图。CubeMap / Octahedral /
  LambertDome 等烘焙档位的贴图里太阳是烤死的 —— 选点只改变**光源方向**
  （主光/阴影、以及 Atmosphere / SH 重投影等程序化消费方）。烘焙档天空视觉
  是否随 `[sun]` 走由引擎装配逻辑决定；若开发者希望烘焙档位下太阳固定，
  那是该档位资产配置自己的约束，工具不代做取舍。
- 若资产原本没有 `[sun]` 段，点击将新建它（只写 `direction`；color/intensity
  缺省走引擎默认）。
- 画质档位在资产**外部**选择（示例/游戏从同一 `.env` 里选一种技术用），
  数字键仅用于预览对比，不代表画质分级配置。

## 注意：重新生成会覆盖

`[sun].direction` 是运行时字段。若资产由 `BuiltinAssetExport`（预设生成器）
再生成，`[sun]` 会被预设脚本的值覆盖 —— 手改后请保留 `.env.toml` 作为真源，
或在生成脚本里同步该方向。

## 自检

```
set SUNPICKER_SELFCHECK=1
EnvSunPicker.exe <资产路径>
```

加载 → 写入探测方向 → 双格式落盘（临时目录）→ 重载比对；
日志打 `SELFCHECK PASS/FAIL`，退出码 0/1。用于 CI 或改动后快速回归。

```powershell
# 本地示例（仓库根）
$env:VK_LOADER_LAYERS_DISABLE="~implicit~"; $env:SUNPICKER_SELFCHECK="1"
build\out\Windows_64_Debug\EnvSunPicker.exe ShaderLibrary/asset/environment/sky_34.env
```
