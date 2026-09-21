/// MaterialShaderEmitter.cpp — GLSL 发射层实现（自 MaterialShaderCompiler.cpp 分离）
///
/// S2-T2.1：纯函数，零决策——只把求解层已解出的状态（schema / manifest /
/// 槽位声明 / config）转成 GLSL 文本。

#include "compile/MaterialShaderEmitter.h"
#include "../document/DocumentFragmentBuilder.h"

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderCacheRoot.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/mtl/ShaderCodeModuleFile.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/type/StdString.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace hgl::graph::mtl
{
    // ── 内部前向声明（2026-09 de-export：仅本文件消费，不再导出）────────
    std::string BuildMeshIndexTableDecls();
    std::string BuildFSIndexTableDecls(const bool fs_has_data_slots);

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

// ── 诊断用 GLSL 落盘（声明见 MaterialShaderEmitter.h）─────────────────────────
void DumpShaderGenGLSL(const char *stage, const std::string &text)
{
#ifndef _DEBUG
    (void)stage;
    (void)text;
#else
    if (!stage || text.empty())
        return;

    const char *enabled = std::getenv("ULRE_DUMP_GLSL");
    if (!enabled || !enabled[0])
        return;

    const OSString root = GetShaderCacheRootPath();
    if (root.IsEmpty())
        return;

    filesystem::Path directory(root);
    directory /= ToOSString("glsldump");
    const OSString directory_path = directory.ToOSString();

    if (!filesystem::IsDirectory(directory_path)
     && !filesystem::MakePath(directory_path))
        return;

    static std::atomic<uint32_t> dump_seq{0};
    const uint32_t seq = dump_seq.fetch_add(1);

    std::string file_name = "ulre_dump_";
    file_name += std::to_string(seq);
    file_name += "_";
    file_name += stage;
    file_name += ".glsl";

    filesystem::Path file_path(directory_path);
    file_path /= ToOSString(file_name);

    filesystem::SaveMemoryToFile(
        file_path.ToOSString(),
        text.data(),
        static_cast<int64>(text.size()));
#endif
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

// 材质源是否读取 payload 行（MTL_ROW）——由模块元数据判定，而不是靠模块名
// 里是否含 "texture_source" 反推（旧实现的 std::strstr 会误命中任何名字里
// 含该子串的模块，且无法表达新模块的诉求）。
//
// 判定依据：模块注解声明 `require Resource MaterialData`，即它会读材质数据行。
//   有（如 pbr_surface_source，用 MTL_ROW）→ true
//   无（如 texture_source，只用 MTL_TEX）  → false
static bool MaterialSourceRequiresMaterialDataRow(
    const char *source_module_path) noexcept
{
    AnsiString module_name;
    if (!ExtractShaderCodeModuleNameFromIncludePath(
            source_module_path, module_name))
        return true;   // 路径无法解析：保守按「需要 payload」处理

    const ShaderCodeModuleDefinition *module =
        GetShaderCodeModuleRegistry().FindByName(module_name.c_str());
    if (!module)
        return true;   // 模块未注册：同上，保守处理

    for (uint32 i = 0; i < module->semantic_requirement_count; ++i)
    {
        const ShaderCodeModuleSemanticRequirement &requirement =
            module->semantic_requirements[i];

        if (requirement.source == ShaderCodeModuleCapabilitySource::Resource
         && requirement.semantic == ShaderCodeModuleSemantic::MaterialData)
            return true;
    }

    return false;
}

static bool MaterialDefinitionRequiresPayloadRow(
    const GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition) noexcept
{
    if (!IsGlobalSSBOType(material_private_data))
        return false;

    if (!material_definition)
        return true;

    const bool has_texture_refs =
        material_definition->texture_declarations.size() > 0;
    if (!has_texture_refs)
        return true;

    const char *source_module = material_definition->material_source_module;
    if (!source_module || !source_module[0])
        return false;

    return MaterialSourceRequiresMaterialDataRow(source_module);
}

// ── Step 5b: Material SSBO GLSL 声明 ─────────────────────────────────────────
// 材质实例 SSBO 的 struct + buffer 声明不再写死在 .glsl 中，
// 统一依据单一 material_private_data 声明生成并注入 Fragment 阶段。
// 变量名固定为 DefaultMaterialPrivateDataName。
bool BuildMaterialSSBODeclarations(
    const GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition,
    const MaterialTextureReferenceLayout *texture_layout,
    std::string &out_decls,
    std::string &out_macros,
    std::string &out_error)
{
    const bool has_payload = MaterialDefinitionRequiresPayloadRow(
        material_private_data,
        material_definition);
    const bool has_texture_references =
        texture_layout && texture_layout->HasReferences();
    if (!has_payload && !has_texture_references)
        return true;

    // payload 行和纹理引用行都通过 BDA 地址解引用。
    out_decls += "#extension GL_EXT_buffer_reference : require\n";
    out_decls += "#extension GL_ARB_gpu_shader_int64 : require\n";

    if (has_payload)
    {
        const char *struct_name  = ssbo::GetGlobalSSBOStructName(material_private_data);
        const char *row_struct = ssbo::GetGlobalSSBORowName(material_private_data);
        const char *struct_codes = ssbo::GetGlobalSSBOStructGLSL(material_private_data);
        if (!row_struct || !struct_codes || !struct_name)
        {
            out_error = "unsupported material row type for GLSL generation";
            return false;
        }

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

        out_decls += "struct ";
        out_decls += struct_name;
        out_decls += "\n{\n";

        for (; *p; ++p)
        {
            if (*p == '\n')
                FlushFieldLine();
            else
                line += *p;
        }
        FlushFieldLine();

        out_decls += "};\n";

        out_decls += "layout(buffer_reference, scalar, buffer_reference_align=16) buffer ";
        out_decls += row_struct;
        out_decls += "\n{\n";

        p = struct_codes;
        for (; *p; ++p)
        {
            if (*p == '\n')
                FlushFieldLine();
            else
                line += *p;
        }
        FlushFieldLine();

        out_decls += "};\n";

        const char *ubo_field = nullptr;
        switch (material_private_data)
        {
        case GlobalSSBOType::PBRSurface:          ubo_field = "addr_pbr_surface"; break;
        case GlobalSSBOType::EmissiveSurface:     ubo_field = "addr_emissive_surface"; break;
        case GlobalSSBOType::TransmissionSurface: ubo_field = "addr_transmission_surface"; break;
        default:                                    ubo_field = nullptr; break;
        }
        if (!ubo_field)
        {
            out_error = "unsupported material private data for UBO mapping";
            return false;
        }

        const uint32_t stride = GetGlobalSSBOTypeStructStride(material_private_data);

        out_macros += "#define MTL_ROW(i) ";
        out_macros += row_struct;
        out_macros += "(global_addresses.";
        out_macros += ubo_field;
        out_macros += " + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_index) * uint64_t(";
        out_macros += std::to_string(stride);
        out_macros += "))\n";
    }

    if (has_texture_references)
    {
        if (!material_definition
         || material_definition->texture_declarations.size()
               != texture_layout->reference_count)
        {
            out_error = "texture reference layout declarations are incomplete";
            return false;
        }

        out_decls +=
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef\n";
        out_decls += "{\n";
        for (const auto &declaration :
            material_definition->texture_declarations)
        {
            out_decls += "    uvec2 tex_";
            out_decls += declaration.name;
            out_decls += ";\n";
        }
        out_decls += "};\n";

        out_macros += "#define MTL_TEX(i) ";
        out_macros += "MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(";
        out_macros += std::to_string(texture_layout->row_stride);
        out_macros += "))\n";
    }

    return true;
}

bool BuildMaterialResourceDocument(
    const GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition,
    ShaderDocument &out_document,
    std::string &out_error)
{
    out_document.Clear();
    out_error.clear();

    MaterialTextureReferenceLayout texture_layout{};
    const MaterialTextureReferenceLayout *texture_layout_ptr = nullptr;
    if (material_definition)
    {
        if (!BuildMaterialTextureReferenceLayout(
               *material_definition,
               texture_layout))
        {
            out_error = "invalid material texture reference layout";
            return false;
        }
        texture_layout_ptr = &texture_layout;
    }

    std::string declarations;
    std::string macros;
    if (!BuildMaterialSSBODeclarations(
            material_private_data,
            material_definition,
            texture_layout_ptr,
            declarations,
            macros,
            out_error))
        return false;

    ShaderDocumentSource source;
    source.stage = "fragment";
    if (!declarations.empty())
    {
        source.logical_name = "MaterialSSBO";
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(declarations.c_str()),
            source);
    }
    if (!macros.empty())
    {
        source.logical_name = "MaterialSSBOAlias";
        out_document.Add(
            ShaderDocumentBlockKind::Define,
            AnsiString(macros.c_str()),
            source);
    }

    const std::string mesh_index_tables =
        BuildMeshIndexTableDecls();
    if (!mesh_index_tables.empty())
    {
        source.logical_name = "MaterialMeshIndexTables";
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(mesh_index_tables.c_str()),
            source);
    }

    const std::string fragment_index_tables =
        BuildFSIndexTableDecls(
            IsGlobalSSBOType(material_private_data)
         || (texture_layout_ptr && texture_layout_ptr->HasReferences()));
    if (!fragment_index_tables.empty())
    {
        source.logical_name = "MaterialFragmentIndexTables";
        out_document.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(fragment_index_tables.c_str()),
            source);
    }
    return true;
}

bool BuildCompileDefineDocument(
    const MaterialCompileConfig &config,
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

    if (macros.empty())
        return true;

    ShaderDocumentSource source;
    source.logical_name = "MaterialCompileDefines";
    ShaderDocumentDiagnostics diagnostics;
    DocumentFragmentBuilder builder(out_document, diagnostics, source);
    if (!builder.Add(
        ShaderDocumentBlockKind::Define,
        AnsiString(macros.c_str())))
        return false;
    return true;
}

// ── Step 5d: Instance index table 行表 GLSL 声明 ─────────────────────────────
// l2w_index 行表声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，
// 统一由本生成器注入（A6-2b 去契约门后恒发射）：mesh 阶段提供 l2w_index
//（ResolveTransformID）；FS 行表描述符已随 BDA 化退场（纹理句柄随数据槽行尾下发）。
namespace
{
    struct IndexTableSpec
    {
        const char *sbs_name;        // 历史名称记录（描述符时代键名，仅注释性）
        const char *buffer_name;     // buffer_reference 类型名（加 Ref 后缀）
        const char *var_name;        // （旧描述符对象名——BDA 化后无对象，仅保留作注释性记录）
        const char *resolve_func;    // 为空则仅生成 buffer 声明
        const char *element_type;    // 行元素 GLSL 类型（默认 uint；Arena 地址表为 uint64_t）
        const char *root_addr_field; // pc_root 字段名（如 addr_l2w_index）
    };

    // mesh 阶段只需 l2w_index；材质地址表在 FS 消费（见 BuildFSIndexTableDecls）。
    // A6-2a：行表本体 buffer_reference——地址经 pc_root.addr_l2w_index 下发；
    // 恒发射（去契约门）。sbs_name 字段为历史名称记录（SBS_LocalToWorldIndex
    // 常量已随 A6-2b-b2 删除——行表不再有描述符名，仅 buffer_reference 类型名）。
    const IndexTableSpec kMeshIndexTableSpecs[] = {
        { "l2w_index", "LocalToWorldIndex", "l2w_index",
          "ResolveTransformID", "uint", "addr_l2w_index" },
    };

    void AppendIndexTableDecl(
        std::string &out,
        const IndexTableSpec &spec)
    {
        // A6-2a：无 set 无 binding——类型声明 + resolve 函数经 pc_root 地址解引用。
        // l2w_index 恒发射（与 l2w_ssbo 恒注入一致；无 L2W 材质不消费 ResolveTransformID，
        // 多余 unused 声明无害）。不再经契约 GetSSBO 门（契约 L2W/L2WIndex 条目已删）。
        out += "layout(buffer_reference, scalar, buffer_reference_align=16) buffer ";
        out += spec.buffer_name;
        out += "Ref { ";
        out += spec.element_type;
        out += " values[]; };\n";

        if (spec.resolve_func)
        {
            // 行表写单列（values[iid]），每个值直接对应一个材质数据行。
            out += spec.element_type;
            out += " ";
            out += spec.resolve_func;
            out += "(uint iid) { return ";
            out += spec.element_type;
            out += "(";
            out += spec.buffer_name;
            out += "Ref(pc_root.";
            out += spec.root_addr_field;
            out += ").values[iid]); }\n";
        }
    }
}//namespace

std::string BuildMeshIndexTableDecls()
{
    std::string out;

    // A6-2a：恒发射——不再查契约（GetSSBO）。与 l2w_ssbo 的无条件注入对应：
    // mesh 侧需要 ResolveTransformID/l2w_index 的材质恒可得，无需契约中间层。
    for (const IndexTableSpec &spec : kMeshIndexTableSpecs)
        AppendIndexTableDecl(out, spec);

    return out;
}

    // FS 侧 RootAddresses push constant block 发射——与 mesh 侧（MeshShaderHeaderGen）
    // 遍历同一 HGL_ROOT_ADDRESSES_FIELD_LIST，布局逐字段一致（不同 stage 访问同一
    // push constant range 必须布局一致）。FS 消费 pc_root.addr_mtl_data_addrs。
    void EmitFSRootAddressesPushConstant(std::string &out)
    {
        out += "layout(push_constant) uniform RootAddresses\n";
        out += "{\n";
        for (uint32 field_index = 0;
             field_index < kRootAddressesFieldCount;
             ++field_index)
        {
            out += "    ";
            out += kRootAddressesFieldGLSLTypes[field_index];
            out += " ";
            out += kRootAddressesFieldNames[field_index];
            out += ";\n";
        }
        out += "} pc_root;\n";
        out += "\n";
    }

std::string BuildFSIndexTableDecls(const bool fs_has_runtime_rows)
{
    std::string out;

    if (!fs_has_runtime_rows)
        return out;

    // 每个 draw item 同时携带 payload 与纹理引用配置两个索引（8B）。
    out += "struct MaterialInstanceAddresses\n";
    out += "{\n";
    out += "    uint payload_index;\n";
    out += "    uint texture_reference_index;\n";
    out += "};\n";
    out +=
        "layout(buffer_reference, scalar, buffer_reference_align=8) buffer MaterialInstanceAddressesRef\n";
    out += "{\n";
    out += "    MaterialInstanceAddresses values[];\n";
    out += "};\n";

    return out;
}

namespace
{
    void AddStageDiagnostic(
        ShaderDocumentDiagnostics &diagnostics,
        const char *code,
        const char *message)
    {
        ShaderDocumentDiagnostic *diagnostic = diagnostics.Create();
        diagnostic->code = code;
        diagnostic->message = message;
    }

    void AppendDocumentBlocks(
        ShaderDocument &out_document,
        const ShaderDocument &fragment,
        const char *stage,
        const char *material)
    {
        for (int i = 0; i < fragment.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = fragment.GetBlock(i);
            ShaderDocumentSource source = block.source;
            source.stage = stage;
            source.material = material ? material : "";
            out_document.Add(block.kind, block.text, source);
        }
    }

    void AppendStageResourceBlocks(
        ShaderDocument &out_document,
        const ShaderDocument &resources,
        const ShaderStage stage,
        const char *stage_name,
        const char *material)
    {
        for (int i = 0; i < resources.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = resources.GetBlock(i);
            const bool mesh_index_tables =
                std::strcmp(
                    block.source.logical_name.c_str(),
                    "MaterialMeshIndexTables") == 0;
            if ((stage == ShaderStage::Mesh) != mesh_index_tables)
                continue;

            ShaderDocumentSource source = block.source;
            source.stage = stage_name;
            source.material = material ? material : "";
            out_document.Add(block.kind, block.text, source);
        }
    }
}

bool BuildMaterialStageDocument(
    const ShaderDocument &source_document,
    const ShaderStage stage,
    const char *material,
    const MaterialCompileConfig &config,
    const GlobalSSBOType material_private_data,
    ShaderDocument &out_document,
    ShaderDocumentDiagnostics &out_diagnostics)
{
    out_document.Clear();
    out_diagnostics.Clear();

    const char *stage_name = nullptr;
    if (stage == ShaderStage::Mesh)
        stage_name = "mesh";
    else if (stage == ShaderStage::Fragment)
        stage_name = "fragment";
    else
    {
        AddStageDiagnostic(
            out_diagnostics,
            "unsupported-stage",
            "Material stage document requires Mesh or Fragment stage");
        return false;
    }

    ShaderDocument injection;
    if (stage == ShaderStage::Fragment)
    {
        ShaderDocumentSource source;
        source.stage = stage_name;
        source.material = material ? material : "";
        source.logical_name = "MaterialMeshShaderExtension";
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_EXT_mesh_shader : require\n",
            source);

        // BDA：pc_root(uint64_t push constant)与 MaterialDataAddressesRef/材质行
        //（buffer_reference）依赖以下扩展——Fragment 侧 pc_root 在 injection 最先
        // 发出，扩展必须先于它（scalar_block_layout 由 GLSLCompiler 编译前全局注入）
        source.logical_name = "MaterialBDAShaderExtension";
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_EXT_buffer_reference : require\n",
            source);
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_ARB_gpu_shader_int64 : require\n",
            source);
        injection.Add(
            ShaderDocumentBlockKind::Extension,
            "#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable\n",
            source);

        // pc_root push constant（Fragment 侧）——MTL_ROW 宏与
        // MaterialDataAddressesRef(pc_root.addr_mtl_data_addrs) 引用；
        // 与 mesh 侧声明同一 X 列表，布局逐字段一致
        source.logical_name = "MaterialRootAddressesPC";
        std::string fs_pc_root;
        EmitFSRootAddressesPushConstant(fs_pc_root);
        injection.Add(
            ShaderDocumentBlockKind::Resource,
            AnsiString(fs_pc_root.c_str()),
            source);

        ShaderDocument compile_defines;
        if (!BuildCompileDefineDocument(config, compile_defines))
            return false;
        AppendDocumentBlocks(
            injection, compile_defines, stage_name, material);

        if (config.material_definition)
        {
            const std::string sampler_macros =
                BuildSamplerMacros(config.material_definition->sampler_names);
            if (!sampler_macros.empty())
            {
                source.logical_name = "MaterialSamplerMacros";
                injection.Add(
                    ShaderDocumentBlockKind::Define,
                    AnsiString(sampler_macros.c_str()),
                    source);
            }
        }
    }

    ShaderDocument resources;
    std::string error;
    if (!BuildMaterialResourceDocument(
            material_private_data,
            config.material_definition,
            resources,
            error))
    {
        AddStageDiagnostic(
            out_diagnostics,
            "material-resource",
            error.c_str());
        return false;
    }

    if (source_document.GetBlockCount() == 0
     || source_document.GetBlock(0).kind != ShaderDocumentBlockKind::Version)
    {
        AddStageDiagnostic(
            out_diagnostics,
            "source-version",
            "Material stage source document must start with a Version block");
        return false;
    }

    struct StageBlockEntry
    {
        ShaderDocumentBlock block{};
        int order = 0;
        size_t sequence = 0;
    };

    std::vector<StageBlockEntry> merged_blocks;
    merged_blocks.reserve(
        static_cast<size_t>(source_document.GetBlockCount())
        + static_cast<size_t>(injection.GetBlockCount())
        + static_cast<size_t>(resources.GetBlockCount()));

    const auto add_block = [&merged_blocks, stage_name, material](
        const ShaderDocumentBlock &block)
    {
        StageBlockEntry entry{};
        entry.block = block;
        entry.block.source.stage = stage_name;
        entry.block.source.material = material ? material : "";
        entry.order = ShaderDocument::GetBlockOrder(entry.block.kind);
        entry.sequence = merged_blocks.size();
        merged_blocks.push_back(entry);
    };

    const ShaderDocumentBlock &version = source_document.GetBlock(0);
    ShaderDocumentSource source = version.source;
    source.stage = stage_name;
    source.material = material ? material : "";
    ShaderDocumentBlock version_block = version;
    version_block.source = source;
    add_block(version_block);

    for (int i = 0; i < injection.GetBlockCount(); ++i)
        add_block(injection.GetBlock(i));

    if (stage == ShaderStage::Fragment)
    {
        for (int i = 0; i < resources.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = resources.GetBlock(i);
            if (std::strcmp(
                    block.source.logical_name.c_str(),
                    "MaterialMeshIndexTables") == 0)
                continue;
            add_block(block);
        }
    }
    else if (stage == ShaderStage::Mesh)
    {
        for (int i = 0; i < resources.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = resources.GetBlock(i);
            if (std::strcmp(
                    block.source.logical_name.c_str(),
                    "MaterialMeshIndexTables") != 0)
                continue;
            add_block(block);
        }
    }

    for (int index = 1; index < source_document.GetBlockCount(); ++index)
    {
        add_block(source_document.GetBlock(index));
    }

    std::sort(
        merged_blocks.begin(),
        merged_blocks.end(),
        [](const StageBlockEntry &lhs, const StageBlockEntry &rhs)
        {
            if (lhs.order != rhs.order)
                return lhs.order < rhs.order;
            return lhs.sequence < rhs.sequence;
        });

    for (const StageBlockEntry &entry : merged_blocks)
        out_document.Add(entry.block.kind, entry.block.text, entry.block.source);
    return true;
}

}//namespace hgl::graph::mtl
