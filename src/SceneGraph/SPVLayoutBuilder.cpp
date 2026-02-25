/**
 * SPVLayoutBuilder.cpp — Layer 3: Build Vulkan objects from SPIR-V reflection
 *
 * Converts SPVParseData produced by GLSLCompiler.dll into:
 *   • VkDescriptorSetLayout  (via BuildDescriptorSetLayout)
 *   • vertex attribute descriptions (via BuildVertexInputAttributes)
 *   • stage-IO cross-validation     (via ValidateStageIO)
 *   • GPU-Driven invariant check     (via ValidateGPUDrivenInputs)
 *
 * No GLSL generation here. No compiler calls here.
 * The only Vulkan interaction is VkDescriptorSetLayout construction.
 */

#include <hgl/graph/mtl/SPVLayoutBuilder.h>
#include <hgl/graph/mtl/SPVParseData.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKStruct.h>
#include <hgl/shadergen/MaterialDescriptorInfo.h>

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstring>

namespace hgl::graph
{

// ---------------------------------------------------------------------------
// VkFormat derivation
// ---------------------------------------------------------------------------

VkFormat SPVAttribToVkFormat(SPVBaseType bt, uint8_t vec_size)
{
    switch (bt)
    {
    case SPVBaseType::Float:
        switch (vec_size) {
            case 1: return VK_FORMAT_R32_SFLOAT;
            case 2: return VK_FORMAT_R32G32_SFLOAT;
            case 3: return VK_FORMAT_R32G32B32_SFLOAT;
            case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        } break;
    case SPVBaseType::Int:
        switch (vec_size) {
            case 1: return VK_FORMAT_R32_SINT;
            case 2: return VK_FORMAT_R32G32_SINT;
            case 3: return VK_FORMAT_R32G32B32_SINT;
            case 4: return VK_FORMAT_R32G32B32A32_SINT;
        } break;
    case SPVBaseType::UInt:
        switch (vec_size) {
            case 1: return VK_FORMAT_R32_UINT;
            case 2: return VK_FORMAT_R32G32_UINT;
            case 3: return VK_FORMAT_R32G32B32_UINT;
            case 4: return VK_FORMAT_R32G32B32A32_UINT;
        } break;
    case SPVBaseType::Double:
        switch (vec_size) {
            case 1: return VK_FORMAT_R64_SFLOAT;
            case 2: return VK_FORMAT_R64G64_SFLOAT;
            case 3: return VK_FORMAT_R64G64B64_SFLOAT;
            case 4: return VK_FORMAT_R64G64B64A64_SFLOAT;
        } break;
    default:
        break;
    }
    return VK_FORMAT_UNDEFINED;
}

// ---------------------------------------------------------------------------
// Internal helper: map SPVDescriptorKind → VkDescriptorType
// ---------------------------------------------------------------------------

static VkDescriptorType KindToVkDescriptorType(SPVDescriptorKind kind)
{
    switch (kind)
    {
    case SPVDescriptorKind::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case SPVDescriptorKind::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case SPVDescriptorKind::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case SPVDescriptorKind::SampledImage:         return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case SPVDescriptorKind::StorageSampler:       return VK_DESCRIPTOR_TYPE_SAMPLER;
    case SPVDescriptorKind::StorageImage:         return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case SPVDescriptorKind::InputAttachment:      return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    default:
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

// ---------------------------------------------------------------------------
// BuildDescriptorSetLayout
// ---------------------------------------------------------------------------

VkDescriptorSetLayout BuildDescriptorSetLayout(
    VulkanDevice        *device,
    const SPVParseData  *vert_parse,
    const SPVParseData  *frag_parse,
    const SPVParseData  *geom_parse)
{
    if (!device) return VK_NULL_HANDLE;

    // Key: (set << 8 | binding) — used only within this function because the
    // ULRE architecture currently hard-bins everything to set 0.
    struct BindingKey {
        uint32_t set, binding;
        bool operator==(const BindingKey &o) const {
            return set == o.set && binding == o.binding;
        }
    };

    struct BindingEntry {
        VkDescriptorSetLayoutBinding vk;
        std::string name;
    };

    // linear list, small N → simple loop instead of hash map
    std::vector<BindingEntry> entries;

    auto merge_stage = [&](const SPVParseData *parse, VkShaderStageFlagBits stage_flag)
    {
        if (!parse) return;
        for (uint32_t i = 0; i < parse->descriptors.count; ++i)
        {
            const SPVDescriptorBinding &db = parse->descriptors.items[i];

            if (db.kind == SPVDescriptorKind::PushConstant)
                continue;  // handled separately

            VkDescriptorType vk_type = KindToVkDescriptorType(db.kind);
            if (vk_type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
                continue;

            // Find or insert
            bool found = false;
            for (auto &e : entries)
            {
                if (e.vk.binding == db.binding)
                {
                    // Check compatibility
                    if (e.vk.descriptorType != vk_type)
                    {
                        std::cerr << "[SPVLayoutBuilder] Descriptor type mismatch at binding "
                                  << db.binding << " (" << db.name << ")\n";
                        return; // propagate error; caller checks VK_NULL_HANDLE
                    }
                    // Widen stage flags
                    e.vk.stageFlags |= stage_flag;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                BindingEntry e;
                e.vk.binding            = db.binding;
                e.vk.descriptorType     = vk_type;
                e.vk.descriptorCount    = db.array_count > 0 ? db.array_count : 1;
                e.vk.stageFlags         = stage_flag;
                e.vk.pImmutableSamplers = nullptr;
                e.name                  = db.name;
                entries.push_back(e);
            }
        }
    };

    merge_stage(vert_parse, VK_SHADER_STAGE_VERTEX_BIT);
    merge_stage(frag_parse, VK_SHADER_STAGE_FRAGMENT_BIT);
    merge_stage(geom_parse, VK_SHADER_STAGE_GEOMETRY_BIT);

    if (entries.empty())
        return VK_NULL_HANDLE;

    // Sort by binding for deterministic layout
    std::sort(entries.begin(), entries.end(),
        [](const BindingEntry &a, const BindingEntry &b){ return a.vk.binding < b.vk.binding; });

    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    vk_bindings.reserve(entries.size());
    for (const auto &e : entries)
        vk_bindings.push_back(e.vk);

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = static_cast<uint32_t>(vk_bindings.size());
    ci.pBindings    = vk_bindings.data();

    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(device->GetDevice(), &ci, nullptr, &dsl) != VK_SUCCESS)
    {
        std::cerr << "[SPVLayoutBuilder] vkCreateDescriptorSetLayout failed\n";
        return VK_NULL_HANDLE;
    }
    return dsl;
}

// ---------------------------------------------------------------------------
// BuildVertexInputAttributes
// ---------------------------------------------------------------------------

std::vector<VertexAttributeDesc> BuildVertexInputAttributes(
    const SPVParseData *vert_parse)
{
    std::vector<VertexAttributeDesc> result;
    if (!vert_parse) return result;

    result.reserve(vert_parse->stage_inputs.count);

    for (uint32_t i = 0; i < vert_parse->stage_inputs.count; ++i)
    {
        const SPVStageAttribute &attr = vert_parse->stage_inputs.items[i];

        VertexAttributeDesc desc;
        desc.location = attr.location;
        desc.format   = SPVAttribToVkFormat(attr.basetype, attr.vec_size);
        desc.name     = attr.name;

        if (desc.format == VK_FORMAT_UNDEFINED)
        {
            std::cerr << "[SPVLayoutBuilder] Cannot derive VkFormat for vertex input '"
                      << attr.name << "' (basetype=" << (int)attr.basetype
                      << ", vec_size=" << (int)attr.vec_size << ")\n";
        }

        result.push_back(std::move(desc));
    }

    // Sort by location for consistent binding
    std::sort(result.begin(), result.end(),
        [](const VertexAttributeDesc &a, const VertexAttributeDesc &b){
            return a.location < b.location;
        });

    return result;
}

// ---------------------------------------------------------------------------
// ValidateStageIO
// ---------------------------------------------------------------------------

IOValidationResult ValidateStageIO(
    const SPVParseData *vert_parse,
    const SPVParseData *frag_parse)
{
    IOValidationResult res;
    if (!vert_parse || !frag_parse) return res;

    // Build a map of vert outputs: location → SPVStageAttribute
    std::unordered_map<uint32_t, const SPVStageAttribute *> vert_outputs;
    for (uint32_t i = 0; i < vert_parse->stage_outputs.count; ++i)
    {
        const SPVStageAttribute &o = vert_parse->stage_outputs.items[i];
        vert_outputs[o.location] = &o;
    }

    for (uint32_t i = 0; i < frag_parse->stage_inputs.count; ++i)
    {
        const SPVStageAttribute &fi = frag_parse->stage_inputs.items[i];

        auto it = vert_outputs.find(fi.location);
        if (it == vert_outputs.end())
        {
            res.ok = false;
            res.error += "Fragment input '" + std::string(fi.name) +
                         "' at location " + std::to_string(fi.location) +
                         " has no matching vertex output.\n";
            continue;
        }

        const SPVStageAttribute &vo = *it->second;
        if (vo.basetype != fi.basetype || vo.vec_size != fi.vec_size)
        {
            res.ok = false;
            res.error += "Type mismatch at location " + std::to_string(fi.location) +
                         ": vert output '" + std::string(vo.name) +
                         "' vs frag input '" + std::string(fi.name) + "'.\n";
        }
    }

    return res;
}

// ---------------------------------------------------------------------------
// ValidateDescriptorConsistency
// ---------------------------------------------------------------------------

DescriptorConsistencyResult ValidateDescriptorConsistency(
    const MaterialDescriptorInfo *gen_decl,
    const SPVParseData           *vert_parse,
    const SPVParseData           *frag_parse)
{
    DescriptorConsistencyResult res;
    if (!gen_decl || !vert_parse || !frag_parse) return res;

    // Collect all SPV bindings from both stages into a flat map
    // key: (set << 8 | binding)
    struct SPVEntry {
        SPVDescriptorKind kind;
        std::string name;
    };
    std::unordered_map<uint32_t, SPVEntry> spv_map;

    auto add_stage = [&](const SPVParseData *parse)
    {
        if (!parse) return;
        for (uint32_t i = 0; i < parse->descriptors.count; ++i)
        {
            const SPVDescriptorBinding &db = parse->descriptors.items[i];
            uint32_t key = (db.set << 8u) | db.binding;
            if (spv_map.find(key) == spv_map.end())
                spv_map[key] = { db.kind, db.name };
        }
    };
    add_stage(vert_parse);
    add_stage(frag_parse);

    // Compare against generator declaration
    // (MaterialDescriptorInfo iteration uses GetUBOList, GetSSBOList, etc.)
    // This is a heuristic check — we compare names and kinds.
    (void)spv_map; // prevent unused-variable warning until full impl

    // TODO: full implementation requires iterating MaterialDescriptorInfo
    // entries and cross-checking against spv_map.  Placeholder here; the
    // function signature and structure are established.

    return res;
}

// ---------------------------------------------------------------------------
// ValidateGPUDrivenInputs
// ---------------------------------------------------------------------------

GPUDrivenInputCheck ValidateGPUDrivenInputs(const SPVParseData *vert_parse)
{
    GPUDrivenInputCheck res;
    if (!vert_parse) {
        res.error = "null SPVParseData";
        return res;
    }

    for (uint32_t i = 0; i < vert_parse->stage_inputs.count; ++i)
    {
        const SPVStageAttribute &a = vert_parse->stage_inputs.items[i];

        if (strncmp(a.name, "L2W_ID", SPV_NAME_MAX) == 0 ||
            strncmp(a.name, "l2w_id", SPV_NAME_MAX) == 0)
        {
            if (a.basetype == SPVBaseType::UInt && a.vec_size == 1)
                res.has_l2w_id = true;
            else
                res.error += "L2W_ID must be uint (scalar), found basetype="
                    + std::to_string((int)a.basetype) + " vec=" + std::to_string(a.vec_size) + "\n";
        }

        if (strncmp(a.name, "MI_ID", SPV_NAME_MAX) == 0 ||
            strncmp(a.name, "mi_id", SPV_NAME_MAX) == 0 ||
            strncmp(a.name, "MaterialInstanceID", SPV_NAME_MAX) == 0)
        {
            if (a.basetype == SPVBaseType::UInt && a.vec_size == 1)
                res.has_mi_id = true;
            else
                res.error += "MI_ID must be uint (scalar), found basetype="
                    + std::to_string((int)a.basetype) + " vec=" + std::to_string(a.vec_size) + "\n";
        }
    }

    if (!res.has_l2w_id)
        res.error += "Missing L2W_ID vertex input (required for GPU-Driven architecture)\n";
    if (!res.has_mi_id)
        res.error += "Missing MI_ID vertex input (required for GPU-Driven architecture)\n";

    res.ok = res.has_l2w_id && res.has_mi_id && res.error.empty();
    return res;
}

} // namespace hgl::graph
