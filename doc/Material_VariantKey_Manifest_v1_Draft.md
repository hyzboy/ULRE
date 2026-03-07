# MaterialVariantKey v1 + Manifest v1 草案

## 1. 目的

本草案用于支撑目标：运行时不再执行 ShaderGen/GLSL 编译，仅通过离线 SPV 产物完成材质创建。

本草案定义两件核心协议：

- `MaterialVariantKey v1`：稳定、可序列化、可版本化的材质变种键
- `Manifest v1`：`key -> SPV + layout metadata` 的离线索引格式

## 2. 设计原则

- 稳定优先：同一输入必须得到同一 key 字符串与同一 key hash。
- 前向兼容：通过 schema version 管理升级。
- 可调试：key 字符串要求人可读，便于日志定位 miss。
- 可裁剪：支持 profile tier 与 feature bitset，控制包体膨胀。

## 3. MaterialVariantKey v1

## 3.1 C++ 草案结构

```cpp
#pragma once

#include <cstdint>
#include <string>

namespace hgl::graph::mtl
{
    enum class MaterialVariantSchemaVersion : uint16_t
    {
        V1 = 1,
    };

    enum class MaterialProfileTier : uint8_t
    {
        MobileLow = 0,
        MobileMid,
        DesktopMid,
        DesktopHigh,
    };

    enum class CoverageMode : uint8_t
    {
        Solid = 0,
        Alpha,
        Mask,
        DepthOnlySolid,
        DepthOnlyMask,
    };

    enum class DitherMode : uint8_t
    {
        Off = 0,
        Bayer4x4,
        Bayer8x8,
        BlueNoise,
    };

    struct MaterialVariantKeyV1
    {
        MaterialVariantSchemaVersion schema = MaterialVariantSchemaVersion::V1;

        // Core preset axis
        uint16_t preset_id = 0; // mtl::MaterialPreset

        // Config axes (normalized)
        uint8_t primitive_type = 0;
        uint8_t coordinate_system_2d = 0;
        uint8_t position_format = 0;
        uint8_t camera = 0;
        uint8_t sky = 0;
        uint8_t local_to_world = 0;
        uint8_t sky_ambient_model = 0;
        uint8_t material_instance = 0;

        // Permutation key axes
        uint8_t perm_ambient = 0;
        uint8_t perm_light = 0;
        uint8_t perm_specular = 0;
        uint8_t perm_shadow = 0;

        // Coverage / transparency axes
        CoverageMode coverage = CoverageMode::Solid;
        DitherMode dither_mode = DitherMode::Off;
        uint8_t dither_threshold_q = 0; // quantized [0,255]

        // Runtime target axis
        MaterialProfileTier profile_tier = MaterialProfileTier::DesktopMid;

        // Extension slots for v1 (must be zero for stable hash)
        uint32_t feature_bits = 0;
        uint32_t reserved0 = 0;

        bool operator==(const MaterialVariantKeyV1 &rhs) const = default;
    };

    // Stable canonical text: "mvk1|preset=...|prim=...|..."
    std::string ToCanonicalString(const MaterialVariantKeyV1 &key);

    // Stable 64-bit hash from canonical string (xxhash64/fnv1a64)
    uint64_t HashCanonical(const MaterialVariantKeyV1 &key);
}
```

## 3.2 规范说明

- `schema` 必须写入 key 与 manifest，每次不兼容变更升级版本。
- `preset_id` 使用 `MaterialPreset` 数值，但建议 manifest 保留 `preset_name` 便于排错。
- `dither_threshold_q` 采用量化值，避免浮点序列化漂移。
- `feature_bits` 用于后续扩展（如 normal compression、forward/deferred 特性位）。
- v1 要求 `reserved0 == 0`，否则视为非法 key。

## 3.3 规范化规则（关键）

生成 key 前必须先做 normalize：

- 未使用字段写 0（不能保留随机值）。
- `coverage=Solid` 时，`dither_mode` 强制归一为 `Off`，`dither_threshold_q=0`。
- 2D 材质时 `camera/sky/local_to_world` 按规则归一（无效字段置 0）。
- profile 映射在离线阶段完成，运行时只做选 tier，不改 key 结构。

## 3.4 Canonical String 格式

建议格式（固定字段顺序，禁止重排）：

```text
mvk1|preset=BasicLit|preset_id=15|prim=Triangles|cs2d=0|posfmt=VEC3|camera=1|sky=1|l2w=1|sky_amb=IBL|mi=1|perm_amb=IBL|perm_light=BlinnPhong|perm_spec=Combined|perm_shadow=None|coverage=Alpha|dither=Off|dither_q=0|tier=DesktopHigh|feat=0x00000000
```

要求：

- 字段顺序固定。
- 枚举以符号名输出，便于日志阅读。
- hash 始终基于 canonical string 计算。

## 4. Manifest v1

## 4.1 推荐结构（JSON）

```json
{
  "schema": "material_manifest_v1",
  "key_schema": "mvk1",
  "build_id": "2026-03-06T11:50:00Z",
  "compiler": {
    "name": "glslang",
    "vulkan_target": "1.2",
    "spv_target": "1.5"
  },
  "entries": [
    {
      "key_canonical": "mvk1|preset=BasicLit|...",
      "key_hash64": "0xA17E3C1D6C45F902",
      "preset_name": "BasicLit",
      "profile_tier": "DesktopHigh",
      "stages": [
        {
          "stage": "vertex",
          "stage_mask": 1,
          "spv_path": "BasicLit/abc123/vs.spv",
          "word_count": 1024,
          "sha256": "..."
        },
        {
          "stage": "fragment",
          "stage_mask": 16,
          "spv_path": "BasicLit/abc123/fs.spv",
          "word_count": 3429,
          "sha256": "..."
        }
      ],
      "layout": {
        "descriptor_layout_sig": "...",
        "vertex_input_sig": "...",
        "mi_struct_bytes": 24
      },
      "diagnostics": {
        "mirror_diff_all_match": true,
        "warnings": []
      }
    }
  ]
}
```

## 4.2 运行时最小必需字段

运行时加载最少需要：

- `key_hash64` / `key_canonical`
- `stages[].spv_path`
- `layout.descriptor_layout_sig`
- `layout.vertex_input_sig`
- `layout.mi_struct_bytes`

其余字段可作为 debug/审计字段。

## 4.3 可选 CSV 兼容层

为兼容你现在的 `spv_manifest.csv`，可保留 CSV 导出，但 JSON 作为主协议。

CSV 最低列建议：

```text
key_hash64,key_canonical,preset_name,profile_tier,stage,stage_mask,spv_path,word_count,sha256,descriptor_layout_sig,vertex_input_sig,mi_struct_bytes
```

## 5. 运行时接口草案

## 5.1 新接口

```cpp
bool BuildMaterialVariantKeyV1(MaterialVariantKeyV1 &out_key,
                               MaterialPreset preset,
                               const MaterialCreateConfig &cfg,
                               const ShaderPermutationKey &perm,
                               CoverageMode coverage,
                               DitherMode dither,
                               uint8_t dither_threshold_q,
                               MaterialProfileTier tier);

bool LoadPrecompiledMaterialCreateInfo(MaterialCreateInfo &out_mci,
                                       const MaterialVariantKeyV1 &key,
                                       const char *manifest_root,
                                       std::string *out_reason);
```

## 5.2 MaterialManager 接入策略

- `CreateMaterial(preset, cfg)` 内先 build key。
- 先查 manifest，命中则走 `CreateShaderModuleFromSPV(...)`。
- miss 时：
  - 开发模式可 fallback runtime ShaderGen
  - 发布模式直接失败并记录 key_canonical

## 6. 离线流水线草案

1. 遍历预设与变种轴，生成 `MaterialVariantKeyV1`。
2. 调用当前编译路径得到 `MaterialCreateInfo`（过渡阶段仍可复用现有 ShaderGen）。
3. 导出各 stage SPV + layout metadata。
4. 写入 manifest v1。
5. 校验：
   - key 唯一性
   - stage 完整性
   - SHA 完整性
   - 运行时 dry-load 全通过

## 7. 必须落地的工程约束

- 编译器版本与目标版本必须记录到 manifest。
- key 构建逻辑必须在 runtime/offline 共用同一实现，避免偏差。
- manifest 查找优先使用 `key_hash64`，冲突时再比对 `key_canonical`。
- 所有 miss 日志必须输出完整 canonical key。

## 8. 迁移建议

- M1：先引入 key + manifest 数据结构，不切运行路径。
- M2：离线工具产出 manifest，但 runtime 仅采样验证。
- M3：runtime 加载优先，保留 fallback。
- M4：发布构建禁用 runtime ShaderGen。

## 9. 待决策项

- `feature_bits` 的 bit 分配规范（需要单独文档）。
- profile tier 是否需要按平台拆分（Windows/Linux/Android）。
- canonical string 是否保留符号名 + 数值双写（建议双写，便于审计）。
- manifest 主格式 JSON 还是二进制（建议 JSON 起步，后续可增量引入 bin 索引）。
