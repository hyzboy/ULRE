/// MaterialShaderEmitter.cpp — GLSL 发射层实现（自 MaterialShaderCompiler.cpp 分离）
///
/// S2-T2.1：纯函数，零决策——只把求解层已解出的状态（DescriptorSetLayoutAllocator /
/// manifest / 槽位声明 / config）转成 GLSL 文本。本文件内容为整体搬移，行为逐字节不变。

#include "compile/MaterialShaderEmitter.h"

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/ShaderDocumentLegacyAdapter.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <cstdio>
#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

std::string BuildCodeModuleGLSL(const ShaderCodeResourceManifest *manifest)
{
    ShaderDocument document;
    if (!BuildCodeModuleDocument(manifest, nullptr, nullptr, document))
        return {};
    ShaderDocumentDiagnostics diagnostics;
    AnsiString serialized;
    if (!document.SerializeFragment(serialized, diagnostics))
        return {};
    return std::string(serialized.c_str(), serialized.Length());
}

bool BuildCodeModuleDocument(
    const ShaderCodeResourceManifest *manifest,
    const char *stage,
    const char *material,
    ShaderDocument &out_document)
{
    out_document.Clear();
    if (!manifest || !manifest->IsValid())
        return true;

    const auto &module_registry = mtl::GetShaderCodeModuleRegistry();
    for (uint32 i = 0; i < manifest->code_module_count; ++i)
    {
        const ShaderCodeModuleDefinition *module =
            module_registry.FindByName(manifest->code_module_names[i]);
        if (!module || !module->glsl_code)
            return false;

        AnsiString code = "\n// ShaderCodeModule: ";
        code += module->name ? module->name : "Unknown";
        code += "\n";
        code += module->glsl_code;
        code += "\n";
        ShaderDocumentSource source;
        source.module = module->name ? module->name : "Unknown";
        source.logical_name = "ShaderCodeModule";
        source.path = module->name ? module->name : "Unknown";
        source.stage = stage ? stage : "";
        source.material = material ? material : "";
        out_document.Add(ShaderDocumentBlockKind::Module, code, source);
    }
    return true;
}

ShaderDocument BuildCodeModuleDocument(
    const ShaderCodeResourceManifest *manifest,
    const char *stage,
    const char *material)
{
    ShaderDocument document;
    BuildCodeModuleDocument(manifest, stage, material, document);
    return document;
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
// 统一依据单槽 material_private_data 生成并注入 Fragment 阶段。
// 单槽化：一个材质固定生成一个 buffer（MaterialPrivateData，slot 0，
// 变量名固定 DefaultMaterialPrivateDataSlotName）。
bool BuildMaterialSSBODeclarations(
    const DescriptorSetLayoutAllocator &descriptor_info,
    const SSBOType material_private_data,
    std::string &out_decls,
    std::string &out_macros,
    std::string &out_error)
{
    if (material_private_data == SSBOType::UserDefined)
        return true;

    const char *slot_name = DefaultMaterialPrivateDataSlotName;
    const ShaderDescriptor *sd = descriptor_info.GetSSBO(slot_name);
    if (!sd || sd->set < 0 || sd->binding < 0)
    {
        out_error = "material ssbo descriptor unresolved for GLSL generation";
        return false;
    }

    const char *struct_name  = ssbo::GetMaterialSSBOStructName(material_private_data);
    const char *struct_codes = ssbo::GetMaterialSSBOStructGLSL(material_private_data);
    if (!struct_name || !struct_codes)
    {
        out_error = "unsupported material ssbo type for GLSL generation";
        return false;
    }

    const char *const buffer_base =
        ssbo::GetMaterialSSBOBufferName(material_private_data);
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
    out_decls += slot_name;
    out_decls += ";\n";

    out_macros += "#define MTL_DATA ";
    out_macros += slot_name;
    out_macros += "\n";

    return true;
}

bool BuildMaterialResourceDocument(
    const DescriptorSetLayoutAllocator &descriptor_info,
    const SSBOType material_private_data,
    ShaderDocument &out_document,
    std::string &out_error)
{
    out_document.Clear();
    out_error.clear();

    std::string declarations;
    std::string macros;
    if (!BuildMaterialSSBODeclarations(
            descriptor_info,
            material_private_data,
            declarations,
            macros,
            out_error))
        return false;

    ShaderDocumentSource source;
    source.stage = "fragment";
    source.logical_name = "MaterialResources";
    if (!declarations.empty())
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(declarations.c_str()),
            source);
    if (!macros.empty())
        out_document.Add(
            ShaderDocumentBlockKind::Define,
            AnsiString(macros.c_str()),
            source);

    const std::string mesh_index_tables =
        BuildMeshIndexTableDecls(descriptor_info);
    if (!mesh_index_tables.empty())
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(mesh_index_tables.c_str()),
            source);

    const std::string fragment_index_tables =
        BuildFSIndexTableDecls(descriptor_info);
    if (!fragment_index_tables.empty())
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(fragment_index_tables.c_str()),
            source);
    return true;
}

// ── Step 5c: 编译期宏（compile_defines）──────────────────────────────────────
// 遍历 MaterialDefinition.compile_defines，为每个名字生成 "#define <name> 1\n"。
// 用于在 GLSL 中通过 #ifdef 切换代码路径（如 TEXT_SDF_ENABLED）。
std::string BuildCompileDefineMacros(
    const CompositorMaterialBuildConfig &config)
{
    ShaderDocument document;
    if (!BuildCompileDefineDocument(config, document))
        return {};

    ShaderDocumentDiagnostics diagnostics;
    AnsiString serialized;
    if (!document.SerializeFragment(serialized, diagnostics))
        return {};
    return std::string(serialized.c_str(), serialized.Length());
}

bool BuildCompileDefineDocument(
    const CompositorMaterialBuildConfig &config,
    ShaderDocument &out_document)
{
    out_document.Clear();
    std::string macros;
    if (!config.material_definition)
        return true;

    for (const auto &name : config.material_definition->compile_defines)
    {
        if (name.empty())
            continue;
        macros += "#define ";
        macros += name;
        macros += " 1\n";
    }

    ShaderDocumentSource source;
    source.logical_name = "MaterialCompileDefines";
    out_document.Add(
        ShaderDocumentBlockKind::Define,
        AnsiString(macros.c_str()),
        source);
    return true;
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
    // 行索引即 fragDataIndexID。
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
        ShaderDocument document;
        ShaderDocumentDiagnostics diagnostics;
        if (!BuildFinalStageDocument(glsl, inject, "unknown", document, diagnostics))
            return {};

        AnsiString serialized;
        if (!document.Serialize(serialized, diagnostics))
            return {};
        return std::string(serialized.c_str(), serialized.Length());
    }
}//namespace

bool BuildFinalStageDocument(
    const std::string &stage_glsl,
    const std::string &version_inject,
    const char *stage,
    ShaderDocument &out_document,
    ShaderDocumentDiagnostics &out_diagnostics)
{
    return BuildInjectedShaderDocument(
        AnsiString(stage_glsl.c_str()),
        AnsiString(version_inject.c_str()),
        AnsiString(stage ? stage : "unknown"),
        out_document,
        out_diagnostics);
}

void AssembleFinalGLSL(
    ShaderBuildContext *ctx,
    const std::string &ms_glsl,
    const std::string &fs_glsl,
    const std::string &ms_inject,
    const std::string &fs_inject)
{
    std::string ms_final = InsertAfterVersionLine(ms_glsl, ms_inject);

    std::string fs_final = InsertAfterVersionLine(fs_glsl, fs_inject);

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
