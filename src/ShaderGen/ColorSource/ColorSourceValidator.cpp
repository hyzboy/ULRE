#include <hgl/shadergen/ColorSourceValidator.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderGenDiagnostic.h>
#include <hgl/mtl/SamplerSlot.h>
#include "../GLSLCompiler.h"
#include "../SPVParseData.h"
#include <bitset>
#include <cstring>
#include <cstdio>

namespace hgl::graph
{

ColorSourceValidationResult ValidateColorSources(const std::vector<ColorSource> &sources)
{
    ColorSourceValidationResult result;

    std::bitset<size_t(mtl::SamplerSlot::RANGE_SIZE)> seen_slots{};

    for (size_t i = 0; i < sources.size(); ++i)
    {
        const auto &cs = sources[i];

        // 检查 kind
        if (cs.kind == ColorSourceKind::None)
        {
            result.ok = false;
            result.errors.push_back({i, "ColorSourceKind::None is not allowed in a finalized source list"});
            continue;
        }

        // 检查槽位范围
        const auto slot_idx = size_t(cs.slot);
        if (slot_idx >= size_t(mtl::SamplerSlot::RANGE_SIZE))
        {
            result.ok = false;
            result.errors.push_back({i, "SamplerSlot value out of range"});
            continue;
        }

        // 检查槽位重复
        if (seen_slots.test(slot_idx))
        {
            result.ok = false;
            result.errors.push_back({i,
                std::string("Duplicate SamplerSlot: ") + mtl::SamplerSlotNameList[slot_idx]});
        }
        seen_slots.set(slot_idx);

        // 内置 sampler 恰好需要 1 个 binding
        if (cs.kind == ColorSourceKind::BuiltinSampler2D ||
            cs.kind == ColorSourceKind::BuiltinSampler2DArray)
        {
            if (cs.bindings.size() != 1)
            {
                result.ok = false;
                result.errors.push_back({i,
                    "BuiltinSampler* must declare exactly 1 DescriptorRequirement"});
            }
        }

        // UserPCG 允许 0 个 binding（纯函数 PCG），但不是 None
        // 其他 kind 暂不加约束，等具体实现时补充
    }

    return result;
}

// ── G4 实现 ───────────────────────────────────────────────────────────────────

/// (name, set, binding) 三元组，用于 G4 诊断表输出
struct BindingEntry
{
    std::string name;
    int         set     = -1;
    int         binding = -1;
};

/// 从 MaterialDescriptorDB 收集所有 COMBINED_IMAGE_SAMPLER 的 (glsl_name, set, binding)。
/// glsl_name 使用 ToGLSLSamplerSymbol(slot)，与 SPIR-V 反射侧保持一致。
/// Resort() 调用完成后 set/binding 已经确定。
static std::vector<BindingEntry> BuildDeclaredBindingTable(const MaterialDescriptorDB &db)
{
    std::vector<BindingEntry> table;
    table.reserve(mtl::SamplerSlotCount);
    for (size_t i = 0; i < mtl::SamplerSlotCount; ++i)
    {
        const mtl::SamplerSlot slot = static_cast<mtl::SamplerSlot>(i);
        const TextureSamplerDescriptor *ts = db.GetTextureSampler(slot);
        if (!ts || ts->set < 0 || ts->binding < 0)
            continue;
        // GLSL シンボル名（"Sampler_BaseColor" など）を使う。
        // これが SPIR-V 反射側の名前と一致する唯一の権威名。
        table.push_back({ mtl::ToGLSLSamplerSymbol(slot), ts->set, ts->binding });
    }
    return table;
}

/// 从 FS GLSL 源码重新编译，用 ParseShaderSPV 提取 COMBINED_IMAGE_SAMPLER 的
/// (name, set, binding) 三元组。
static std::vector<BindingEntry> ExtractReflectedBindingTable(const std::string &fs_glsl)
{
    std::vector<BindingEntry> result;
    if (fs_glsl.empty())
        return result;

    SPVData *spv = CompileShader(uint32_t(ShaderStage::Fragment), fs_glsl.c_str());
    if (!spv)
        return result;

    SPVParseData *parse_data = ParseShaderSPV(spv);
    FreeSPVData(spv);
    if (!parse_data)
        return result;

    // COMBINED_IMAGE_SAMPLER = 1 (VkDescriptorTypeLite)
    const auto &combined = parse_data->resource[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER];
    for (uint32_t i = 0; i < combined.count; ++i)
    {
        const Descriptor &d = combined.items[i];
        if (d.name && d.name[0] != '\0')
            result.push_back({ std::string(d.name), int(d.set), int(d.binding) });
    }

    FreeShaderSPVParseData(parse_data);
    return result;
}

/// 将 BindingEntry 列表格式化为单行字符串，用于诊断消息。
static std::string FormatBindingTable(const std::vector<BindingEntry> &table)
{
    if (table.empty()) return "(empty)";
    std::string s;
    for (const auto &e : table)
    {
        if (!s.empty()) s += ", ";
        s += e.name + "(set=" + std::to_string(e.set) + ",b=" + std::to_string(e.binding) + ")";
    }
    return s;
}

bool G4ValidateReflectedSamplers(const mtl::MaterialCreateInfo &mci,
                                 std::vector<ShaderGenDiagnostic> &diag)
{
    const ShaderCreateInfo *fs_sci = mci.GetStageShader(ShaderStage::Fragment);
    if (!fs_sci)
    {
        // 没有 FS 阶段（如 depth-only pass），G4 静默跳过
        return true;
    }

    const std::string &fs_glsl = fs_sci->GetFinalGLSL();
    if (fs_glsl.empty())
        return true;

    // ── 1. Declared 表：从 MaterialDescriptorDB 取 (name, set, binding) ────────
    const std::vector<BindingEntry> declared =
        BuildDeclaredBindingTable(mci.GetDescriptorInfo());

    // 没有任何 sampler 声明 → G4 不需要运行
    if (declared.empty())
        return true;

    // ── 2. Reflected 表：从 FS SPIR-V 取 (name, set, binding) ─────────────────
    const std::vector<BindingEntry> reflected =
        ExtractReflectedBindingTable(fs_glsl);

    bool all_ok = true;

    // ── 3. 反向检查：reflected ⊆ declared（按名字匹配）──────────────────────
    for (const auto &re : reflected)
    {
        bool found = false;
        for (const auto &de : declared)
        {
            if (re.name == de.name) { found = true; break; }
        }

        if (!found)
        {
            // Mismatch → 一律 FATAL；包含两份 (set, binding) 表
            const std::string msg =
                std::string("[G4][FATAL] FS sampler '") + re.name
                + "' (set=" + std::to_string(re.set) + ",b=" + std::to_string(re.binding)
                + ") is referenced in SPIR-V but NOT found in the declared binding table.\n"
                "  declared  : " + FormatBindingTable(declared) + "\n"
                "  reflected : " + FormatBindingTable(reflected);

            std::fprintf(stderr, "%s\n", msg.c_str());
            diag.push_back({
                ShaderGenSeverity::Error,
                ShaderGenErrorCode::ReflectionMismatch,
                ShaderStage::Fragment,
                "G4.ReflectedSamplerNotDeclared",
                msg
            });
            all_ok = false;
        }
    }

    // ── 4. 正向检查：declared 中的 sampler 被 SPIR-V 剪掉是正常的，输出 INFO ──
    for (const auto &de : declared)
    {
        bool used = false;
        for (const auto &re : reflected) { if (re.name == de.name) { used = true; break; } }
        if (!used)
        {
            std::fprintf(stderr,
                "[G4][INFO] declared sampler '%s'(set=%d,b=%d) not referenced in FS SPIR-V"
                " (pruned by optimizer — OK)\n",
                de.name.c_str(), de.set, de.binding);
        }
    }

    return all_ok;
}

} // namespace hgl::graph
