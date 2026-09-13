# BC5 双通道法线贴图

> 2026-09-13 落地。法线贴图从三通道 BC7 改为两通道 BC5：只存 X/Y，Z 在 shader 里用
> `z = sqrt(1 - x² - y²)` 还原（UE/Unity 的通行做法）。

## 1. 为什么是 BC5

| 方案 | 平均角误差(vs 源图, Brickwall 1024×512 / 9 级) | P95 | 文件大小 |
|---|---|---|---|
| BC7 三通道 | 1.413° | 3.646° | 699088 B |
| **BC5 + 还原 Z** | **0.948°** | 3.006° | 699088 B |

体积**完全一样**（BC5 与 BC7 都是 16 字节/块；BC5 = 两块 BC4 = 8+8）。收益在精度：Z 不占码率、
X/Y 各按 8bit 独立编码，法线更平滑、高光更干净。逐资产实测误差 0.049°~1.546°。

补充：只要单通道可以用 BC4（8 字节/块）；但**两通道法线必须 BC5** —— 单个 BC4 装不下 XY，
也就无从还原 Z。

## 2. 工具侧：TexConv

```bash
# 法线贴图：一律两通道 BC5，并生成完整 mip 链
TexConv /normal /mip Normal.tga        # -> Normal.Tex2D (BC5)

# 等价写法（按源图通道数指定目标压缩格式）
TexConv /RGB:BC5 /mip Normal.png
```

| 参数 | 含义 |
|---|---|
| `/R: /RG: /RGB: /RGBA:` | 分别指定 1/2/3/4 通道源图的目标压缩格式（默认 BC4 / BC5 / BC7 / BC7） |
| `/normal` | 法线模式：不论源图几通道，一律输出 BC5 |
| `/mip` | 生成完整 mip 链（法线贴图必须带链，引擎不会给压缩格式自动补 mip） |
| `/AMD` / `/Intel` | 选压缩后端（默认 AMD Compressonator；Intel ISPC 需先备好 `kernel*.obj`） |

**TexConv 必须用 Release 版构建**：它的 ImageMagick 依赖是 Release（STL IDL=0），Debug 版
（`/MTd`, IDL=2）跨 DLL 传 `std::string` 会 ABI 错位，任何图片都加载失败。

## 3. 材质侧：声明两通道

`ShaderLibrary/material/lit.material.toml`：

```toml
[resources]
textures = [
    # ...
    { name = "normal", sampler = "Sampler2DArray", required = false, channels = 2 }
]
```

- `channels = 0`（缺省）= 由纹素格式决定，行为与以前相同（三通道）。
- `channels = 2` → ShaderGen 注入 `#define MTL_TEX_<纹理名大写>_CHANNELS 2`
  （即 `MTL_TEX_NORMAL_CHANNELS`）。宏名按纹理名生成，将来别的双通道贴图可直接复用同一条通道。

## 4. shader 侧：还原 Z

`ShaderLibrary/ntb/ntb_tangent_vbo_normalmap.glsl` 与 `ntb/ntb_derivative_normalmap.glsl` 两个变体
都做了分支：

```glsl
const vec4 normal_sample =
    Sample2DArray(normalTexHandle, TrilinearSampler, si.uv0, float(normalTexture.y));

vec3 nm = normal_sample.xyz * 2.0 - 1.0;
nm.y = -nm.y;                                   // GLSL/Vulkan Green 通道约定

#if defined(MTL_TEX_NORMAL_CHANNELS) && (MTL_TEX_NORMAL_CHANNELS == 2)
    // BC5：只存 XY，Z 用球面公式还原
    const vec3 tangentNormal =
        normalize(vec3(nm.xy * ntb_input.normalScale,
                       sqrt(max(0.0, 1.0 - dot(nm.xy, nm.xy)))));
#else
    const vec3 tangentNormal = normalize(vec3(nm.xy * ntb_input.normalScale, nm.z));
#endif
```

没有该宏时走 `#else` 三通道路径，所以旧资产/未声明的材质行为不变。

## 5. 数据流（这条链共 6 个文件）

| 位置 | 职责 |
|---|---|
| `inc/hgl/mtl/MaterialRecipe.h` | `MaterialTextureDeclaration` 增加 `channels` 字段，并纳入 layout 哈希 |
| `src/ShaderGen/material_definition/MaterialDefinitionFile.cpp` | 解析 TOML 的 `channels`（1–4）+ 未知键白名单 |
| `inc/hgl/mtl/FragmentTemplateComposer.h` | `ComposeInput` 携带纹理声明 |
| `src/ShaderGen/template/FragmentTemplateComposer.cpp` | `AppendTextureChannelDefines()`：声明 → `#define MTL_TEX_*_CHANNELS` |
| `src/ShaderGen/builder/GenericMaterialBuilder.cpp` | 把材质声明传给模板编辑器 |
| `ShaderLibrary/ntb/*.glsl` | 按宏分支，还原 Z |

改到 `MaterialRecipe.h` / `FragmentTemplateComposer.h` 这类跨模块头时，**必须清 obj 重编**
（否则首次重编可能报 `LNK1236: corrupt or invalid COFF sections`，再跑一次增量构建即可）。

## 6. 换 / 加法线资产的三条纪律

1. **声明与资产成对改**。材质写了 `channels = 2` 就必须配 BC5 资产，反之亦然 —— 否则 shader 会用
   还原的 z、忽略贴图自带的 z（对规范法线图差异极小，但语义已经错了）。
2. **先看"夹紧率"**：源图 XY 是否都落在单位圆内（`x²+y² ≤ 1`）。有像素越界就说明这张图不是规范的
   切线空间法线，不能走 BC5。实测反例：
   - `res/image/circle/NormalMap.png` —— 34.4% 像素越界
   - `res/image/flat_normal.tga` —— 100% 像素越界

   这两张至今保持 BC7（它们也没有被任何代码引用）。
3. **mip 链一起生成**（`/mip`）。

## 7. 怎么验证（三层，都在本机跑过）

1. **产物结构**：文件头格式名字段 = `BC5`；逐级字节 `ceil(w/4)*ceil(h/4)*16` 累加后等于文件长度。
2. **精度**：把 BC5 解回 RGBA（Compressonator `CMP_ConvertTexture`，BC5 → RGBA_8888），与源图逐像素
   算法线夹角（平均值 / P95 / 越界率）。
3. **生成链**：跑 `ShaderCooker.exe` 后 grep `build/out/<Cfg>/shader-cache/stage/*.frag`，确认出现
   `#define MTL_TEX_NORMAL_CHANNELS 2` 与 `sqrt(max(0.0, 1.0 - dot(nm.xy, nm.xy)))`；再跑一个用该材质的
   示例（`PBRSpheres` 覆盖 10 张 pbr 法线，`BasicLitMeshes` 覆盖 Brickwall）。
   回归门 `ShaderResourceSchemaRegressionGate all` 与 cooker 的 `cooked/failed/skipped` 计数应逐字等于基线
   （本次：38 PASS / 1 FAIL 既有项；cooked=28 failed=0 skipped=10）。

## 8. 当前状态（2026-09-13）

- **已是 BC5**：`res/image/Brickwall/Normal.Tex2D`、`res/image/pbr/*/normal.Tex2D`（10 个）、
  `res/image/terrain/Normal.Tex2D`
- **保持 BC7**：`res/image/circle/NormalMap.Tex2D`、`res/image/flat_normal.Tex2D`（原因见 §6.2）
- **不走这条链**：`res/model/**/*_normal.Tex2D`（glTF/模型自带资产，用的是别的材质，声明里没有
  `channels`）

## 9. 回滚

切换前的 14 个 BC7 资产备份在 `%TEMP%\normal_switch_backup\`（命名形如
`res_image_Brickwall_Normal.Tex2D`）：

```bash
cp "$LOCALAPPDATA/Temp/normal_switch_backup/res_image_Brickwall_Normal.Tex2D" \
   /e/ULRE/res/image/Brickwall/Normal.Tex2D
```
