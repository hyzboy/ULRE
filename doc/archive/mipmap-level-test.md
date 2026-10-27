# mipmap 级数尺测试资产与范例

> 2026-09-13 建立。用于验证"纹理 mip 链到底有没有被正确读到"——尤其是数组纹理
> （`Texture2DArray`）的整链拷入路径，以及未压缩格式链尾 `<8 字节按 8 计` 的填充规则。

## 1. 原理：为什么是"级数尺"

把每一级 mipmap 做成**颜色、图案、级别号都不同**的图，然后让屏幕上出现一串尺寸递减的方块：

- 第 L 个方块取 S = 256 >> L **像素见方**（正方形），UV 恒为 0..1；
- 纹理 0 级是 256x256，于是方块把纹理正好压成 S 像素 → 两个方向的 texel/px 都是 256/S；
- 隐式 LOD = log2(256/S) = **L**。即"第 L 个方块必然采样第 L 级"。

因此画面可以直接读出采样到了第几级；若 mip 链缺失（例如数组链断掉），**所有方块都会显示
0 级**（红色 + 黑色 "0"），一眼可辨。

**方块必须是正方形**：只压缩一个方向时，另一方向的 texel/px 更大，各向异性过滤取最大导数会把
LOD 抬高。实测：256px 宽 x 123px 高的矩形（另一个方向压成 2.08 texel/px）采样到的是 **1 级**而
不是 0 级（log2(2.08) ≈ 1.06）。

## 2. 测试资产 `res/image/mipmaps/`

| 文件 | 内容 |
|---|---|
| `level_<L>_<S>x<S>.png`（9 张） | 人眼查看用图，S = 256,128,64,32,16,8,4,2,1 |
| `MipLevels.RGBA8.Tex2D` | 未压缩 **RGBA8**，256x256，**9 级** mip 链（含 1x1/2x2 与链尾 `<8 字节按 8 计`） |
| `MipLevels.BC7.Tex2D` | **BC7**，256x256，**7 级** mip 链（BC7 最小块 4x4，到 4x4 为止） |

两份资产**每一级内容都不同**，所以能直接读出"当前采样到第几级"。

各级主色：`0 红 / 1 橙 / 2 黄 / 3 绿 / 4 青 / 5 蓝 / 6 紫 / 7 品红 / 8 白`；尺寸 ≥ 8 的级在左半
画出级别号点阵，右半是 1 像素棋盘（最细的高频内容：贴图被正确降采样时它会变成灰，若 mip 链
断了远端会出现摩尔纹）；4 = 四象限、2 = 四点、1 = 单色。

### 2.1 `.Tex2D` 布局（`inc/hgl/graph/texture/TextureLoader.h` 的 `pragma pack(1)` 头，32 字节）

| offset | 字段 | 本资产取值 |
|---|---|---|
| 0..6 | id | `"Texture"` |
| 7 | version | 0 |
| 8 | type | 1（2D） |
| 9..12 | width (LE u32) | 256 |
| 13..16 | height (LE u32) | 256 |
| 17..20 | depth/layers | 0 |
| 21 | channels | 4 |
| 22..25 | colors | `"RGBA"` |
| 26..29 | bits | 8,8,8,8 |
| 30 | datatype | 3（`VulkanBaseType::UNORM`） |
| 31 | mipmaps | 9 |

载荷按级紧跟，字节数由引擎 `ComputeMipmapBytes2D` 决定（滚动减半 + **不足 8 字节按 8 计**）：

| 级 | 尺寸 | 载荷字节 | 文件偏移 |
|---|---|---|---|
| 0 | 256x256 | 262144 | 32 |
| 1 | 128x128 | 65536 | 262176 |
| 2 | 64x64 | 16384 | 327712 |
| 3 | 32x32 | 4096 | 344096 |
| 4 | 16x16 | 1024 | 348192 |
| 5 | 8x8 | 256 | 349216 |
| 6 | 4x4 | 64 | 349472 |
| 7 | 2x2 | 16 | 349536 |
| 8 | 1x1 | **8**（4 字节 + 4 字节补齐） | 349552 |

合计载荷 349528 + 头 32 = **349560 字节**。脚本会复算这一数字再写盘，并对写出的文件做
"读回 + md5 比对 + 原子改名"。

### 2.2 BC7 资产 `MipLevels.BC7.Tex2D`

头部同 §2.1，但 `channels=0`、`colors="BC7"`、`datatype=0`、`mipmaps=7`；载荷按
`ceil(w/4)*ceil(h/4)*16` 逐级：

| 级 | 尺寸 | 载荷字节 | 文件偏移 |
|---|---|---|---|
| 0 | 256x256 | 65536 | 32 |
| 1 | 128x128 | 16384 | 65568 |
| 2 | 64x64 | 4096 | 81952 |
| 3 | 32x32 | 1024 | 86048 |
| 4 | 16x16 | 256 | 87072 |
| 5 | 8x8 | 64 | 87328 |
| 6 | 4x4 | 16 | 87392 |

合计载荷 87376 + 头 32 = **87408 字节**（与仓库内真资产 `res/image/Grid2x2.Tex2D` 同规格）。

## 3. 生成脚本

```bash
# BC7 资产需要工程内构建的 TexConv（见 §6），先编出来
cmake --build build --config Release --target TexConv

# 生成资产（会自检并写盘校验）
python scripts/gen_mipmap_level_assets.py
```

- PNG 与未压缩 RGBA8 资产由脚本逐字节写出（可精确控制每一级内容，含 1x1/2x2 与链尾填充）；
- BC7 资产由脚本**逐级调用 Release 版 TexConv**（每级单独编码，不带 `/mip`）再按 §2.2 布局拼装；
- 自检输出：逐级偏移/载荷字节与引擎公式的比对、0 级尺寸与级数；BC7 另报逐级"ASCII/零串占比"
  （编码失败时该比例会飙到 ~99%，正常 ~17%——这是当初"垃圾资产"的判别特征）。

## 4. 范例程序 `example/Texture/TextureMipLevels.cpp`

窗口 1280x900，三行"级数尺"：

| 行 | 纹理源 | 路径 | 级数 |
|---|---|---|---|
| A | `Texture2D` | `MipLevels.RGBA8.Tex2D` | 9（256→1） |
| B | `Texture2DArray`（1 层） | 同一文件，链由 `LoadTexture2DArray` 整链拷入 | 9 |
| C | `Texture2DArray`（1 层） | `MipLevels.BC7.Tex2D`（BC7） | 7（256→4） |

- 几何：每行一段 `Position(V2)+TexCoord(V2)` 的屏幕空间矩形带，材质用
  `UnlitTexture`（2D 专用，只采样 base_color，无光照干扰）+ `MakeSolid2DConfig()` +
  `Make2DNodeConfigZeroToOne(true)`，与 `TextureRectArray` 同一套路；
- **A 行与 B 行共用同一份资产**：这是"数组链"的回归验证——修复前 B 行只会显示 0 级，
  修复后 A、B 两行应逐格同色；C 行同理验证 BC7（压缩格式）+ 数组链；
- 格子尺寸按**真实 viewport 尺寸**换算（`GetViewportInfo()->GetViewportWidth/Height()`，
  首帧后若与建尺子时不一致会自动重建）——按窗口逻辑尺寸算会在 DPI 缩放下让整条尺子偏移；
- 启动日志给出每个纹理的尺寸/格式/级数，级数不符预期直接 fail-fast：

```
[MipTest] A 2D       256x256 fmt=37  mip_levels=9  (MipLevels.RGBA8.Tex2D)
[MipTest] B 2DArray  256x256 layers=1 fmt=37  mip_levels=9
[MipTest] C 2DArray  256x256 layers=1 fmt=145 mip_levels=7  (MipLevels.BC7.Tex2D, BC7)
[MipTest] 级数尺按 1280x900 建（窗口逻辑 1280x900）
```

（`fmt=37` = `VK_FORMAT_R8G8B8A8_UNORM`，`fmt=145` = `VK_FORMAT_BC7_UNORM_BLOCK`。）

## 5. 验证结果（2026-09-13）

| 项 | 结果 |
|---|---|
| 构建 | `cmake --build build --config Debug --target TextureMipLevels` → 0 error |
| 运行 | 500+ 帧、`[ERROR]` = 0、`VUID` = 0 |
| 资产字节级自检 | RGBA8：逐级偏移与 §2.1 一致、载荷 349528 == 引擎公式、文件 349560；BC7：逐级载荷 `[65536,16384,4096,1024,256,64,16]` 合计 87376 == 引擎公式、文件 87408 |
| BC7 内容级核对 | 用 Compressonator 把每一级解回 RGBA8，与源 PNG 逐一比均值：**7 级全部 Δmax = 0** |
| 级数尺映射 | **第 L 格 = 第 L 级**：0 红"0" / 1 橙"1" / 2 黄"2" / 3 绿"3" / 4 青"4"… 逐格可读；第 0 格右半 1 像素棋盘在 1:1 截图里严格 155/95 交替 = 该格确实在 LOD 0 |
| A ≡ B | 两行共用同一资产，**逐格一致** → 数组纹理的 mip 整链拷入生效 |
| C 行 | BC7 资产的 7 级链同样逐格显示不同级别（与 A/B 行同步下降） |

> 读数注意：**截图必须 1:1**（`computer_use` 有时返回缩放过的图）。用缩放图判读会得到
> "整体偏移一级"的假结论——本会话就这么被误导过一次，后经 1:1 截图复核，映射其实精确。

## 6. TexConv 集成与修复（原"BC7 载荷无效"的阻碍已解决）

### 6.1 CMake 集成（ULRE 侧）

- `src/Tools/CMakeLists.txt` 增加 `add_subdirectory(TexConv)`；产物 `build/out/<Config>/TexConv.exe`，
  VS 文件夹 `CM/Tools/Texture`；
- TexConv 自带 CMakeLists 里补了两点：① Intel ISPCTextureCompressor 的 `kernel*.obj` 需 ispc 生成
  且上游未入库 → 自动探测，缺失时关闭 Intel 编码器（`/Intel` 明确报错，默认走 AMD）；
  ② 四个附加工具（CubeMapConv/ComboTexture/DFGen/HDR2PNG）仍是旧的 `ImageLoader` API、
  当前源码树编译不过 → 默认不构建（`-DTEXCONV_BUILD_EXTRA_TOOLS=ON` 再试）。

### 6.2 必须用 **Release** 构建 TexConv

ImageMagick 的 Magick++ DLL 是 Release（STL `_ITERATOR_DEBUG_LEVEL=0`），而 ULRE 的 Debug 配置是
`/MTd`（IDL=2）——`std::string` 布局不同，路径字符串跨 DLL 会被读成乱码：**任何图片都加载失败**
（`no decode delegate for an image format`，且报的"文件名"是进程名）。最小复现（同一份 Magick++ 调用）：

| 变体 | 结果 |
|---|---|
| `/MTd`（Debug 静态 CRT, IDL=2） | FAIL |
| `/MT`（Release 静态 CRT, IDL=0） | **OK 256x256** |
| `/MDd`（Debug 动态 CRT, IDL=2） | FAIL |
| `/MD`（Release 动态 CRT, IDL=0） | **OK 256x256** |

### 6.3 工具侧两个真 bug（导致产出"内存垃圾"资产）

`TextureFileCreaterCompressAMD::Write()` 原来走 `CMP_MipSet` + `CMP_ProcessTexture`：

1. **编码根本没跑起来**：本机 Compressonator 4.5.52 只装了 SDK（无 encoder 插件文件），
   `CMP_ProcessTexture` 返回 `CMP_ERR_PLUGIN_FILE_NOT_FOUND(15)`；
2. **返回值未检查**：失败后 `MipSetOut` 里是"已按目标格式申请、从未写入"的内存，被原样落盘。

判别特征（对照真资产）：

| 文件 | 长度 | payload 字节值种类 | 4 字节 ASCII/零串占比 |
|---|---|---|---|
| 修复后（Release TexConv） | 87408 | 255 | **17.6%** |
| 修复前（旧 `res/image/TexConv.exe` / 本次未修版） | 87408 | 161 | **99.0%**（含 UTF-16 路径文本、堆指针） |
| 仓库真资产 `Grid2x2.Tex2D` | 87408 | 240 | 33.6% |

修法：改用传统 API（`CMP_Texture` + `CMP_ConvertTexture`，编码器编在库里、只需
`CMP_InitializeBCLibrary()`），并**检查返回值**，失败即报错返回、不落盘。修复后逐级解码与源图
均值完全一致（§5），这也是 §2.2 那份 BC7 测试资产的来源。

## 7. 命令

```bash
# 生成资产（会自检并写盘校验）
python scripts/gen_mipmap_level_assets.py

# 构建并运行
cmake --build build --config Debug --target TextureMipLevels --parallel 8
./build/out/Windows_64_Debug/TextureMipLevels.exe

# 只看级数日志
grep 'MipTest' <运行日志>
```

## 8. 相关

- 数组纹理 mip 链的实现与修复：`doc/texture2darray-unified-texture-binding.md` §6.1
- 资产格式与装载：`inc/hgl/graph/texture/TextureLoader.h`、`src/SceneGraph/texture/TextureLoader.cpp`、
  `src/SceneGraph/texture/VKTexture2DArrayLoader.cpp`
- 级数落库：`inc/hgl/vk/VKTextureCreateInfo.h`（`origin_mipmaps`/`target_mipmaps`）
