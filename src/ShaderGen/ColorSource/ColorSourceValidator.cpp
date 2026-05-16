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

/// 构建 MaterialDescriptorDB 中已声明的 sampler GLSL 符号名集合
/// 遍历所有已知 SamplerSlot；若 GetTextureSampler(slot) 非 null 则该 slot 已被声明。
static std::vector<std::string> BuildDeclaredSamplerNames(const MaterialDescriptorDB &db)
{
    std::vector<std::string> names;
    names.reserve(mtl::SamplerSlotCount);
    for (size_t i = 0; i < mtl::SamplerSlotCount; ++i)
    {
        const mtl::SamplerSlot slot = static_cast<mtl::SamplerSlot>(i);
        if (db.GetTextureSampler(slot))
            names.push_back(mtl::ToGLSLSamplerSymbol(slot));
    }
    return names;
}

/// 从 FS GLSL 源码重新编译并用 ParseShaderSPV 提取 COMBINED_IMAGE_SAMPLER 名称列表
static std::vector<std::string> ExtractFSSamplerNamesFromGLSL(const std::string &fs_glsl)
{
    std::vector<std::string> result;
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
        const char *name = combined.items[i].name;
        if (name && name[0] != '\0')
            result.push_back(name);
    }

    FreeShaderSPVParseData(parse_data);
    return result;
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

    // 1. 从 DB 收集已声明的 sampler 名
    const std::vector<std::string> declared = BuildDeclaredSamplerNames(mci.GetDescriptorInfo());

    // 如果没有任何 sampler 声明，G4 不需要运行
    if (declared.empty())
        return true;

    // 2. 从 FS GLSL 重新提取 SPIR-V 中实际引用的 sampler 名
    const std::vector<std::string> reflected = ExtractFSSamplerNamesFromGLSL(fs_glsl);

    bool all_ok = true;

    // 3. 反向检查：reflected 中的每个名字必须在 declared 中能找到
    for (const std::string &rname : reflected)
    {
        bool found = false;
        for (const std::string &dname : declared)
        {
            if (rname == dname)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            // 尝试反查 SamplerSlot 以提供更详细的诊断
            bool slot_known = false;
            for (size_t i = 0; i < mtl::SamplerSlotCount; ++i)
            {
                if (rname == mtl::ToGLSLSamplerSymbol(static_cast<mtl::SamplerSlot>(i)))
                {
                    slot_known = true;
                    break;
                }
            }

            const char *severity_tag = slot_known ? "[G4][WARN]" : "[G4][FATAL]";
            std::string msg = std::string(severity_tag)
                + " FS sampler '" + rname
                + "' is referenced in SPIR-V but NOT declared in MaterialDescriptorDB";

            std::fprintf(stderr, "%s\n", msg.c_str());

            diag.push_back({
                ShaderGenSeverity::Error,
                ShaderGenErrorCode::ReflectionMismatch,
                ShaderStage::Fragment,
                "G4.ReflectedSamplerNotDeclared",
                std::move(msg)
            });

            all_ok = false;
        }
    }

    // 4. 正向信息：declared 中有 SPIR-V 未出现的（被编译器剔除）→ INFO 日志
    for (const std::string &dname : declared)
    {
        bool used = false;
        for (const std::string &rname : reflected)
        {
            if (rname == dname) { used = true; break; }
        }
        if (!used)
        {
            std::fprintf(stderr,
                "[G4][INFO] declared sampler '%s' not referenced in FS SPIR-V (pruned by optimizer — OK)\n",
                dname.c_str());
        }
    }

    return all_ok;
}

} // namespace hgl::graph
