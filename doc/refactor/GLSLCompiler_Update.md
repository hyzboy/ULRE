# GLSLCompiler 更新指南

> 文档目标：将 `hyzboy/GLSLCompiler` 仓库的 SPIR-V 反射层对齐到 ULRE 新设计的
> `hgl::SPVParseData`（`inc/hgl/graph/mtl/SPVParseData.h`），消除当前的 ABI 桥接代码
> (`GLSLCOMP_ABI` namespace in `src/ShaderGen/GLSLCompiler.cpp`)。

---

## 当前问题（`glsl2spv.cpp` 的已知缺陷）

| # | 位置 | 问题 | 影响 |
|---|------|------|------|
| 1 | `PushConstant::offset/size` | `uint8_t` — 截断自 `uint32_t` | push constant 块 > 255 字节时溢出 |
| 2 | `ShaderAttribute::location` | `uint8_t` | 理论上足够（Vulkan ≤ 32 locations），但不直观 |
| 3 | `ShaderAttribute` | 无 `component` 字段 | 无法反映 component-level packing |
| 4 | `Descriptor` | 无 `buffer_size` | 无法在 CPU 端验证 UBO/SSBO struct 大小 |
| 5 | `Descriptor` | 无 `array_count` | 无法区分 `sampler2D arr[4]` 与单个 sampler |
| 6 | `Descriptor` | kind 由 `resource[]` 下标隐式决定 | 调用方必须知道 VkDescriptorType 枚举值 |
| 7 | 所有结构体 | `name` 限 32 字节 | 较长的 GLSL block/member 名称会被截断 |
| 8 | `ParseSPV` | 不输出 UBO/SSBO struct 成员反射 | 无法验证 CPU struct 布局与 shader 一致 |

---

## 目标：直接输出 `hgl::SPVParseData`

### 第一步：共享头文件

将 ULRE 的 `inc/hgl/graph/mtl/SPVParseData.h` 复制（或 symlink / git submodule）到
GLSLCompiler 可以 include 的位置，例如：

```
GLSLCompiler/
  SPVParseData.h    ← 从 ULRE 复制或链接
  glsl2spv.cpp
  VKShaderParse.h
```

在 `glsl2spv.cpp` 顶部添加：
```cpp
#include "SPVParseData.h"   // hgl::SPVParseData — shared contract with ULRE
using namespace hgl;
```

### 第二步：扩展 `VKShaderParse.h`

添加以下方法到 `ShaderParse` 类：

```cpp
// 获取描述符数组长度（默认 1）
uint32_t GetArraySize(const spirv_cross::Resource &res) const
{
    const auto &type = compiler->get_type(res.type_id);
    return type.array.empty() ? 1u : type.array[0];
}

// 获取 stage attribute 的 component（分量内 packing）
uint32_t GetComponent(const spirv_cross::Resource &res) const
{
    return compiler->get_decoration(res.id, spv::DecorationComponent);
}

// 枚举 UBO/SSBO struct 成员
uint32_t GetMemberCount(const spirv_cross::Resource &res) const
{
    const auto &type = compiler->get_type(res.base_type_id);
    return (uint32_t)type.member_types.size();
}

struct MemberInfo {
    std::string name;
    uint32_t    offset;
    uint32_t    size;
    spirv_cross::SPIRType::BaseType basetype;
    uint8_t     vec_size;
    uint8_t     col_count;  // matrix columns (0 = not a matrix)
    uint8_t     array_size; // array elements (0 = not an array)
};

MemberInfo GetMember(const spirv_cross::Resource &res, uint32_t idx) const
{
    const auto &type = compiler->get_type(res.base_type_id);
    const auto &mtype = compiler->get_type(type.member_types[idx]);
    MemberInfo m;
    m.name       = compiler->get_member_name(res.base_type_id, idx);
    m.offset     = compiler->type_struct_member_offset(type, idx);
    m.size       = (uint32_t)compiler->get_declared_struct_member_size(type, idx);
    m.basetype   = mtype.basetype;
    m.vec_size   = (uint8_t)mtype.vecsize;
    m.col_count  = (uint8_t)mtype.columns;
    m.array_size = mtype.array.empty() ? 0u : (uint8_t)mtype.array[0];
    return m;
}
```

### 第三步：重写 `ParseSPV`

将 `glsl2spv.cpp` 中的 `ParseSPV` 函数改为直接填充 `hgl::SPVParseData`：

```cpp
// Helper: convert SPIRType::BaseType → hgl::SPVBaseType
hgl::SPVBaseType ToSPVBaseType(spirv_cross::SPIRType::BaseType t)
{
    if (t == spirv_cross::SPIRType::Boolean)                  return hgl::SPVBaseType::Bool;
    if (t == spirv_cross::SPIRType::SByte  || t == spirv_cross::SPIRType::Short  ||
        t == spirv_cross::SPIRType::Int    || t == spirv_cross::SPIRType::Int64)  return hgl::SPVBaseType::Int;
    if (t == spirv_cross::SPIRType::UByte  || t == spirv_cross::SPIRType::UShort ||
        t == spirv_cross::SPIRType::UInt   || t == spirv_cross::SPIRType::UInt64) return hgl::SPVBaseType::UInt;
    if (t == spirv_cross::SPIRType::Half   || t == spirv_cross::SPIRType::Float)  return hgl::SPVBaseType::Float;
    if (t == spirv_cross::SPIRType::Double)                   return hgl::SPVBaseType::Double;
    if (t == spirv_cross::SPIRType::Struct)                   return hgl::SPVBaseType::Struct;
    if (t == spirv_cross::SPIRType::Image)                    return hgl::SPVBaseType::Image;
    if (t == spirv_cross::SPIRType::Sampler)                  return hgl::SPVBaseType::Sampler;
    return hgl::SPVBaseType::MAX;
}

// Fill a SPVArray<SPVStageAttribute> from a spirv_cross resource vector.
void FillStageAttributes(SPVArray<SPVStageAttribute> &out,
                         ShaderParse *sp,
                         const SPVResVector &stages)
{
    out.count = (uint32_t)stages.size();
    if (out.count == 0) return;
    out.items = new SPVStageAttribute[out.count];
    uint32_t i = 0;
    for (const auto &res : stages) {
        auto &d = out.items[i++];
        strncpy(d.name, sp->GetName(res).c_str(), SPV_NAME_MAX - 1);
        d.name[SPV_NAME_MAX - 1] = '\0';
        d.location  = sp->GetLocation(res);
        d.component = sp->GetComponent(res);  // new: requires VKShaderParse update
        spirv_cross::SPIRType::BaseType bt; uint8_t vs;
        sp->GetFormat(res, &bt, &vs);
        d.basetype  = ToSPVBaseType(bt);
        d.vec_size  = vs;
    }
}

// Fill a flat descriptor list from one resource vector.
uint32_t FillDescriptors(SPVDescriptorBinding *dst,
                         ShaderParse *sp,
                         const SPVResVector &res,
                         SPVDescriptorKind kind)
{
    uint32_t i = 0;
    for (const auto &r : res) {
        auto &d = dst[i++];
        strncpy(d.name, sp->GetName(r).c_str(), SPV_NAME_MAX - 1);
        d.name[SPV_NAME_MAX - 1] = '\0';
        d.set         = sp->GetDescriptorSet(r);
        d.binding     = sp->GetBinding(r);
        d.kind        = kind;
        d.array_count = sp->GetArraySize(r);   // new
        d.buffer_size = (kind == SPVDescriptorKind::UniformBuffer ||
                         kind == SPVDescriptorKind::StorageBuffer)
                        ? sp->GetBufferSize(r) : 0;

        // Member reflection for UBO/SSBO
        uint32_t mc = sp->GetMemberCount(r);   // new
        d.member_count = mc;
        d.members = mc ? new SPVMember[mc] : nullptr;
        for (uint32_t m = 0; m < mc; ++m) {
            auto mi = sp->GetMember(r, m);     // new
            auto &dm = d.members[m];
            strncpy(dm.name, mi.name.c_str(), SPV_NAME_MAX - 1);
            dm.name[SPV_NAME_MAX - 1] = '\0';
            dm.offset     = mi.offset;
            dm.size       = mi.size;
            dm.basetype   = ToSPVBaseType(mi.basetype);
            dm.vec_size   = mi.vec_size;
            dm.col_count  = mi.col_count;
            dm.array_size = mi.array_size;
        }
    }
    return i;
}

SPVParseData *ParseSPV(SPVData *spv_data)
{
    ShaderParse sp(spv_data->spv_data, spv_data->spv_length);

    SPVParseData *pd = new SPVParseData;

    FillStageAttributes(pd->stage_inputs,  &sp, sp.GetStageInputs());
    FillStageAttributes(pd->stage_outputs, &sp, sp.GetStageOutputs());

    // Count all descriptors to allocate in one shot
    uint32_t total = (uint32_t)(
        sp.GetUBO().size()           + sp.GetSSBO().size()      +
        sp.GetSampledImages().size() + sp.GetSeparateSamplers().size() +
        sp.GetSeparateImages().size()+ sp.GetStorageImages().size());

    if (total > 0) {
        pd->descriptors.count = total;
        pd->descriptors.items = new SPVDescriptorBinding[total];
        uint32_t off = 0;
        off += FillDescriptors(pd->descriptors.items + off, &sp, sp.GetUBO(),              SPVDescriptorKind::UniformBuffer);
        off += FillDescriptors(pd->descriptors.items + off, &sp, sp.GetSSBO(),             SPVDescriptorKind::StorageBuffer);
        off += FillDescriptors(pd->descriptors.items + off, &sp, sp.GetSampledImages(),    SPVDescriptorKind::CombinedImageSampler);
        off += FillDescriptors(pd->descriptors.items + off, &sp, sp.GetSeparateSamplers(), SPVDescriptorKind::StorageSampler);
        off += FillDescriptors(pd->descriptors.items + off, &sp, sp.GetSeparateImages(),   SPVDescriptorKind::SampledImage);
        off += FillDescriptors(pd->descriptors.items + off, &sp, sp.GetStorageImages(),    SPVDescriptorKind::StorageImage);
    }

    // Push constants
    const auto &pc_res = sp.GetPushConstant();
    pd->push_constants.count = (uint32_t)pc_res.size();
    if (!pc_res.empty()) {
        pd->push_constants.items = new SPVPushConstantRange[pc_res.size()];
        uint32_t i = 0;
        for (const auto &r : pc_res) {
            auto &d = pd->push_constants.items[i++];
            strncpy(d.name, sp.GetName(r).c_str(), SPV_NAME_MAX - 1);
            d.name[SPV_NAME_MAX - 1] = '\0';
            d.offset = sp.GetOffset(r);       // now uint32_t — bug fixed
            d.size   = sp.GetBufferSize(r);   // now uint32_t — bug fixed
        }
    }

    // Subpass inputs
    const auto &si_res = sp.GetSubpassInputs();
    pd->subpass_inputs.count = (uint32_t)si_res.size();
    if (!si_res.empty()) {
        pd->subpass_inputs.items = new SPVSubpassInput[si_res.size()];
        uint32_t i = 0;
        for (const auto &r : si_res) {
            auto &d = pd->subpass_inputs.items[i++];
            strncpy(d.name, sp.GetName(r).c_str(), SPV_NAME_MAX - 1);
            d.name[SPV_NAME_MAX - 1] = '\0';
            d.attachment_index = sp.GetInputAttachmentIndex(r);
            d.binding          = sp.GetBinding(r);
        }
    }

    return pd;
}

void FreeSPVParse(SPVParseData *pd)
{
    if (!pd) return;
    for (uint32_t i = 0; i < pd->descriptors.count; ++i)
        delete[] pd->descriptors.items[i].members;
    delete[] pd->stage_inputs.items;
    delete[] pd->stage_outputs.items;
    delete[] pd->descriptors.items;
    delete[] pd->push_constants.items;
    delete[] pd->subpass_inputs.items;
    delete pd;
}
```

### 第四步：更新 `GLSLCompilerInterface`

```cpp
struct GLSLCompilerInterface {
    bool     (*Init)();
    void     (*Close)();
    bool     (*GetLimit)(TBuiltInResource *, const int);
    bool     (*SetLimit)(TBuiltInResource *, const int);
    uint32_t (*GetType)(const char *ext_name);
    SPVData *(*Compile)(uint32_t stage, const char *src, const CompileInfo *ci);
    SPVData *(*CompileFromPath)(uint32_t stage, const char *path, const CompileInfo *ci);
    void     (*Free)(SPVData *);

    // Updated: returns hgl::SPVParseData directly (no ABI bridge needed)
    hgl::SPVParseData *(*ParseSPV)(SPVData *spv_data);
    void               (*FreeParseSPVData)(hgl::SPVParseData *);
};
```

### 第五步：更新 ULRE 中的桥接代码

一旦 GLSLCompiler 更新完成，删除 `src/ShaderGen/GLSLCompiler.cpp` 中的：
- `namespace GLSLCOMP_ABI { ... }` 整块
- `ConvertOldSPVParseData()` 函数
- `FreeConvertedSPVParseData()` 函数

并将 `ParseSPVData`/`FreeSPVParseData` 改为直接转发：

```cpp
hgl::SPVParseData *ParseSPVData(const SPVData *spv_data)
{
    if (!gsi || !spv_data || !spv_data->result) return nullptr;
    return gsi->ParseSPV(const_cast<SPVData *>(spv_data));
}
void FreeSPVParseData(hgl::SPVParseData *pd)
{
    if (gsi && pd) gsi->FreeParseSPVData(pd);
}
```

---

## 当前 ULRE 端的桥接状态

| 字段 | 当前（老 DLL）| 更新后（新 DLL）|
|------|-------------|----------------|
| `stage_inputs/outputs.location` | ✅ uint8_t→uint32_t 安全转换 | ✅ 直接 |
| `stage_inputs/outputs.component` | ⚠️ 固定为 0 | ✅ 真实值 |
| `descriptors.kind` | ✅ 从下标映射 | ✅ 直接 |
| `descriptors.array_count` | ⚠️ 固定为 1 | ✅ 真实值 |
| `descriptors.buffer_size` | ⚠️ 固定为 0 | ✅ 真实值 |
| `descriptors.members` | ⚠️ nullptr | ✅ 完整成员反射 |
| `push_constants.offset/size` | ⚠️ uint8_t 截断 (≤255B) | ✅ uint32_t |

---

## 相关文件

- `inc/hgl/graph/mtl/SPVParseData.h` — 共享数据结构定义
- `src/ShaderGen/GLSLCompiler.cpp` — 桥接代码（更新后可删除 GLSLCOMP_ABI 部分）
- `src/ShaderGen/GLSLCompiler.h` — `ParseSPVData`/`FreeSPVParseData` 公开接口
- `inc/hgl/graph/mtl/SPVLayoutBuilder.h` — 消费 `hgl::SPVParseData` 构建 Vulkan 对象
- `doc/refactor/ShaderGen_Compiler_Loader_Separation.md` — 整体三层架构设计
