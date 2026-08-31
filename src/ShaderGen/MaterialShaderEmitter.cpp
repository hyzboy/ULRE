/// MaterialShaderEmitter.cpp — GLSL 发射层实现（自 MaterialShaderCompiler.cpp 分离）
///
/// S2-T2.1：纯函数，零决策——只把求解层已解出的状态（DescriptorSetLayoutAllocator /
/// manifest / 槽位声明 / config）转成 GLSL 文本。本文件内容为整体搬移，行为逐字节不变。

#include "MaterialShaderEmitter.h"

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/GLSLCodeModule.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <cstdio>
#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

std::string BuildCodeModuleGLSL(const ModuleResourceManifest *manifest)
{
    if (!manifest || !manifest->IsValid())
        return {};

    const auto &module_registry = mtl::GetGLSLCodeModuleRegistry();
    std::string result;
    for (uint32 i = 0; i < manifest->code_module_count; ++i)
    {
        const GLSLCodeModuleDefinition *module =
            module_registry.FindByName(manifest->code_module_names[i]);
        if (!module || !module->glsl_code)
            continue;
        result += "\n// GLSLCodeModule: ";
        result += module->name ? module->name : "Unknown";
        result += "\n";
        result += module->glsl_code;
        result += "\n";
    }
    return result;
}

std::string BuildSamplerMacros(const std::vector<std::string> &sampler_names)
{
    std::string macros;
    for (const auto &name : sampler_names)
    {
        if (name.empty())
            continue;
        const uint32_t idx = SamplerPresetLibrary::Instance().GetIndex(name.c_str());
        if (idx == ~0u)
        {
            // sampler.toml 无此名字——不生成宏，shader 编译会因未定义
            // 宏显式失败（vs 静默错位成 Nearest）
            GLogError(u8"[MaterialShaderCompiler] sampler preset not found: %s — "
                      u8"check sampler.toml ordering",
                      name.c_str());
            continue;
        }
        macros += "#define ";
        macros += name;
        macros += "Sampler ";
        macros += std::to_string(idx);
        macros += "u\n";
    }
    return macros;
}

// ── Step 5b: Material SSBO GLSL 声明 ─────────────────────────────────────────
// 材质实例 SSBO 的 struct + buffer 声明不再写死在 .glsl 中，
// 统一依据 material_private_data_slot_decls 生成并注入 Fragment 阶段。
// 单槽化：一个材质固定生成一个 buffer（MaterialPrivateData，slot 0）。
bool BuildMaterialSSBODeclarations(
    const DescriptorSetLayoutAllocator &descriptor_info,
    const std::vector<MaterialPrivateDataSlotDeclaration> *material_private_data_slot_decls,
    std::string &out_decls,
    std::string &out_macros,
    std::string &out_error)
{
    if (!material_private_data_slot_decls || material_private_data_slot_decls->empty())
        return true;

    if (material_private_data_slot_decls->size() > MaxMaterialPrivateDataSlotsPerMaterial)
    {
        out_error = "a material may declare at most one MaterialPrivateData slot";
        return false;
    }

    const MaterialPrivateDataSlotDeclaration &decl = (*material_private_data_slot_decls)[0];
    const ShaderDescriptor *sd = descriptor_info.GetSSBO(decl.name.c_str());
    if (!sd || sd->set < 0 || sd->binding < 0)
    {
        out_error = "material ssbo descriptor unresolved for GLSL generation";
        return false;
    }

    const char *struct_name  = ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
    const char *struct_codes = ssbo::GetMaterialSSBOStructGLSL(decl.ssbo_type);
    if (!struct_name || !struct_codes)
    {
        out_error = "unsupported material ssbo type for GLSL generation";
        return false;
    }

    const char *const buffer_base =
        ssbo::GetMaterialSSBOBufferName(decl.ssbo_type);
    if (!buffer_base)
    {
        out_error = "material ssbo buffer name unsupported for GLSL generation";
        return false;
    }

    out_decls += "struct ";
    out_decls += struct_name;
    out_decls += "\n{\n";

    // 规范化字段文本：去掉行首空白、统一 4 空格缩进。
    std::string line;
    const char *p = struct_codes;
    auto FlushFieldLine = [&]()
    {
        size_t start = 0;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
            ++start;
        if (start < line.size())
        {
            out_decls += "    ";
            out_decls.append(line, start, line.size() - start);
            out_decls += '\n';
        }
        line.clear();
    };
    for (; *p; ++p)
    {
        if (*p == '\n')
            FlushFieldLine();
        else
            line += *p;
    }
    FlushFieldLine();

    out_decls += "};\n";

    out_decls += "layout(set=" + std::to_string(sd->set) + ", binding=" + std::to_string(sd->binding) + ") readonly buffer ";
    out_decls += buffer_base;
    out_decls += " {\n    ";
    out_decls += struct_name;
    out_decls += " data[];\n} ";
    out_decls += decl.name;
    out_decls += ";\n";

    out_macros += "#define MTL_DATA ";
    out_macros += decl.name;
    out_macros += "\n";

    return true;
}

// ── Step 5c: 编译期宏（compile_defines）──────────────────────────────────────
// 遍历 MaterialDefinition.compile_defines，为每个名字生成 "#define <name> 1\n"。
// 用于在 GLSL 中通过 #ifdef 切换代码路径（如 TEXT_SDF_ENABLED）。
std::string BuildCompileDefineMacros(
    const CompositorMaterialBuildConfig &config)
{
    std::string macros;
    if (!config.material_definition)
        return macros;

    for (const auto &name : config.material_definition->compile_defines)
    {
        if (name.empty())
            continue;
        macros += "#define ";
        macros += name;
        macros += " 1\n";
    }
    return macros;
}

// ── Step 5d: Instance index table SSBO GLSL 声明 ─────────────────────────────
// material_private_data_index_rows / mtl_texture_layer_rows / l2w_index 的 buffer
// 声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，统一依据
// descriptor_info 生成注入：mesh 阶段提供 l2w_index / material_private_data_index_rows
//（含 ResolveTransformID / ResolveMaterialPrivateDataIndex），FS 阶段提供
// mtl_texture_layer_rows（named-slot TextureLayerRowsData，见下方注入）。
namespace
{
    struct IndexTableSpec
    {
        const char *sbs_name;        // descriptor_info 查询键（SBS_*.name）
        const char *buffer_name;
        const char *var_name;
        const char *resolve_func;    // 为空则仅生成 buffer 声明
    };

    const IndexTableSpec kMeshIndexTableSpecs[] = {
        { SBS_LocalToWorldIndex.name, "LocalToWorldIndex", "l2w_index",     "ResolveTransformID" },
        { SBS_MaterialPrivateDataIndexRows.name, "MaterialPrivateDataIndex", "mtl_private_data_index", "ResolveMaterialPrivateDataIndex" },
    };

    void AppendIndexTableDecl(
        std::string &out,
        const ShaderDescriptor *sd,
        const IndexTableSpec &spec)
    {
        if (!sd || sd->set < 0 || sd->binding < 0)
            return;

        out += "layout(set=" + std::to_string(sd->set) + ", binding=" + std::to_string(sd->binding) + ") readonly buffer ";
        out += spec.buffer_name;
        out += " { uint values[]; } ";
        out += spec.var_name;
        out += ";\n";

        if (spec.resolve_func)
        {
            // 单槽化：行表写单列（values[iid]），不再按 slot 索引。
            out += "uint ";
            out += spec.resolve_func;
            out += "(uint iid) { return ";
            out += spec.var_name;
            out += ".values[iid]; }\n";
        }
    }
}//namespace

std::string BuildMeshIndexTableDecls(
    const DescriptorSetLayoutAllocator &descriptor_info)
{
    std::string out;

    for (const IndexTableSpec &spec : kMeshIndexTableSpecs)
        AppendIndexTableDecl(out, descriptor_info.GetSSBO(spec.sbs_name), spec);

    return out;
}

std::string BuildFSIndexTableDecls(
    const DescriptorSetLayoutAllocator &descriptor_info)
{
    std::string out;

    // FS 阶段注入 bindless 纹理行表：TextureLayerRowsData struct + buffer（named slot）。
    // 字段名 = TextureSlot 的 snake_case 名（GetTextureSlotName），顺序与枚举一致；
    // 内存布局与旧扁平 values[RANGE_SIZE] 逐字节相同，故 CPU 上传无需改动。
    // 行索引即 fragDataIndexID（P1-2e：TextureLayerID varying 已删除）。
    // 仅当该材质确实注册了 mtl_texture_layer_rows（存在 MaterialTextureLayerTable
    // 描述符，即声明了纹理槽）时才注入 named-slot struct + buffer；否则跳过
    //（与 AppendIndexTableDecl 的静默跳过行为一致，无纹理槽材质 FS 不引用该 buffer）。
    const ShaderDescriptor *sd =
        descriptor_info.GetSSBO(SBS_MaterialTextureLayerRows.name);
    if (sd && sd->set >= 0 && sd->binding >= 0)
    {
        out += "struct TextureLayerRowsData\n{\n";
        for (uint32_t i = 0;
             i < static_cast<uint32_t>(TextureSlot::RANGE_SIZE); ++i)
        {
            out += "    uint ";
            out += GetTextureSlotName(static_cast<TextureSlot>(i));
            out += ";\n";
        }
        out += "};\n";
        out += "layout(set=" + std::to_string(sd->set)
              + ", binding=" + std::to_string(sd->binding)
              + ") readonly buffer TextureLayerRowsBuffer\n{\n"
              + "    TextureLayerRowsData data[];\n"
              + "} mtl_texture_layer_rows;\n";
    }

    return out;
}

// ── Step 5e: 最终 GLSL 组装（注入段列表驱动）───────────────────────────────
namespace
{
    // GLSL requires #version to be the very first token.
    std::string InsertAfterVersionLine(const std::string &glsl, const std::string &inject)
    {
        if (inject.empty())
            return glsl;
        const auto pos = glsl.find('\n');
        if (pos == std::string::npos)
            return glsl + "\n" + inject;
        return glsl.substr(0, pos + 1) + inject + glsl.substr(pos + 1);
    }

    std::string InsertBeforeSurfaceFunction(const std::string &glsl, const std::string &inject)
    {
        if (inject.empty())
            return glsl;
        // B6: 单一 marker（#include SURFACE_FUNCTION_FILE）——"#include "surface/" 旧格式
        // 回退已删（全库 0 使用——ShaderLibrary 与回归门均无）
        const std::string marker = "#include SURFACE_FUNCTION_FILE";
        const auto pos = glsl.find(marker);
        if (pos == std::string::npos)
            return glsl + "\n" + inject;
        return glsl.substr(0, pos) + inject + "\n" + glsl.substr(pos);
    }
}//namespace

void AssembleFinalGLSL(
    ShaderBuildContext *ctx,
    const std::string &ms_glsl,
    const std::string &fs_glsl,
    const std::vector<GLSLInjectSegment> &segments)
{
    // 按 point 归组（AfterVersion 组内顺序 = 列表顺序）
    std::string ms_version_injects;
    std::string fs_version_injects;
    std::string fs_surface_injects;

    for (const auto &seg : segments)
    {
        if (seg.text.empty())
            continue;

        if (seg.stage == GLSLInjectStage::Mesh)
            ms_version_injects += seg.text;
        else if (seg.point == GLSLInjectPoint::BeforeSurfaceFunction)
            fs_surface_injects += seg.text;
        else
            fs_version_injects += seg.text;
    }

    std::string ms_final = InsertAfterVersionLine(ms_glsl, ms_version_injects);

    std::string fs_final = fs_glsl;
    if (!fs_surface_injects.empty())
        fs_final = InsertBeforeSurfaceFunction(fs_final, fs_surface_injects);
    if (!fs_version_injects.empty())
        fs_final = InsertAfterVersionLine(fs_final, fs_version_injects);

    ShaderCreateInfo *mesh = ctx->GetStageShader(ShaderStage::Mesh);
    ShaderCreateInfo *frag = ctx->GetStageShader(ShaderStage::Fragment);

    // mesh shader 材质：ms_glsl 实为 mesh stage 源码，
    // 设到 mesh ShaderCreateInfo。
    if (mesh)
        mesh->SetFinalGLSL(ms_final);

    if (frag)
        frag->SetFinalGLSL(fs_final);
}

}//namespace hgl::graph::mtl
