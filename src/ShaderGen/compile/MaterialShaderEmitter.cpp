/// MaterialShaderEmitter.cpp — GLSL 发射层实现（自 MaterialShaderCompiler.cpp 分离）
///
/// S2-T2.1：纯函数，零决策——只把求解层已解出的状态（DescriptorSetLayoutAllocator /
/// manifest / 槽位声明 / config）转成 GLSL 文本。本文件内容为整体搬移，行为逐字节不变。

#include "compile/MaterialShaderEmitter.h"
#include "../document/DocumentFragmentBuilder.h"

#include <hgl/mtl/MaterialShaderCompiler.h>
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
    {
        // 无数据槽材质没有行结构，但地址行表(FS index tables)
        // 同样使用 uint64_t/设备地址——扩展指令必须先于任何声明出现
        out_decls += "#extension GL_EXT_buffer_reference : require\n";
        out_decls += "#extension GL_ARB_gpu_shader_int64 : require\n";
        return true;
    }

    // ── 材质数据无描述符，发射 buffer_reference 行声明 ──
    // shader 侧 MTL_ROW(i) 以地址行表（mtl_data_addrs）取行指针后解引用。
    {
        const char *struct_name  = ssbo::GetMaterialSSBOStructName(material_private_data);
        const char *row_struct = ssbo::GetMaterialSSBORowName(material_private_data);
        const char *struct_codes = ssbo::GetMaterialSSBOStructGLSL(material_private_data);
        if (!row_struct || !struct_codes || !struct_name)
        {
            out_error = "unsupported material row type for GLSL generation";
            return false;
        }

        // buffer_reference 需要 GLSL 扩展指令（Vulkan core 特性、GLSL 扩展语义）
        out_decls += "#extension GL_EXT_buffer_reference : require\n";
        // 行表存 64 位设备地址，shader 侧需要 64 位整型
        out_decls += "#extension GL_ARB_gpu_shader_int64 : require\n";

        // GLSL struct 不允许空成员表——纯句柄行（TextureLayerRow）无 payload，
        // 跳过纯字段值结构的发射
        const bool has_payload = struct_codes && *struct_codes;

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

        if (has_payload)
        {
            // 纯字段值结构（与旧路径 struct 同名）：供模块以值语义拷贝行内数据字段
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
        }

        // buffer_reference 行结构：材质数据字段 + 统一 bindless 纹理句柄尾
        out_decls += "layout(buffer_reference, scalar, buffer_reference_align=16) buffer ";
        out_decls += row_struct;
        out_decls += "\n{\n";

        if (has_payload)
        {
            p = struct_codes;
            for (; *p; ++p)
            {
                if (*p == '\n')
                    FlushFieldLine();
                else
                    line += *p;
            }
            FlushFieldLine();
        }

        for (uint32_t i = 0; i < static_cast<uint32_t>(TextureSlot::RANGE_SIZE); ++i)
        {
            out_decls += "    uint tex_";
            out_decls += GetTextureSlotName(static_cast<TextureSlot>(i));
            out_decls += ";\n";
        }

        out_decls += "};\n";
        out_macros += "#define MTL_ROW(i) ";
        out_macros += row_struct;
        out_macros += "(MaterialDataAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)])\n";
        return true;
    }
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
        BuildFSIndexTableDecls(material_private_data != SSBOType::UserDefined);
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

// ── Step 5d: Instance index table SSBO GLSL 声明 ─────────────────────────────
// l2w_index 的 buffer
// 声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，统一依据
// descriptor_info 生成注入：mesh 阶段提供 l2w_index / material_private_data_index_rows
//（含 ResolveTransformID / ResolveMaterialPrivateDataIndex），FS 阶段提供
// （纹理句柄随数据槽行尾下发，行表描述符已退场）
namespace
{
    struct IndexTableSpec
    {
        const char *sbs_name;        // descriptor_info 查询键（SBS_*.name）
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
            // 单槽化：行表写单列（values[iid]），不再按 slot 索引。
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

std::string BuildFSIndexTableDecls(const bool fs_has_data_slots)
{
    std::string out;

    if (!fs_has_data_slots)
        return out;

    // ── FS 消费设备地址行表（mtl_data_addrs）──────────────
    // 行存 8B 设备地址；fragDataIndexID 即本批 draw item 序号（行表下标）。
    // 行表本体走 BDA（buffer_reference，MaterialDataAddressesRef）——表地址经
    // pc_root.addr_mtl_data_addrs 下发（FS 侧 pc_root 由 BuildMaterialStageDocument
    // Fragment 分支注入，先于本块）。MTL_ROW(i) 宏（BuildMaterialSSBODeclarations
    // 生成）以地址构造行指针。
    // A6-2b-b1：门从契约 GetSSBO 改直判——mtl_data_addrs 需求 = 材质有有效数据槽
    //（material_private_data 非 UserDefined，编译配置直判；契约不再声明行表条目）。
    out += "layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialDataAddressesRef\n";
    out += "{\n";
    out += "    uint64_t values[];\n";
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
    const DescriptorSetLayoutAllocator &descriptor_info,
    const SSBOType material_private_data,
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
            descriptor_info,
            material_private_data,
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

    const ShaderDocumentBlock &version = source_document.GetBlock(0);
    ShaderDocumentSource source = version.source;
    source.stage = stage_name;
    source.material = material ? material : "";
    out_document.Add(ShaderDocumentBlockKind::Version, version.text, source);

    AppendDocumentBlocks(out_document, injection, stage_name, material);
    // Fragment：资源声明保持原位（Version/injection 之后）——FS 的 BDA 扩展
    //（buffer_reference/int64）与 pc_root 已在 injection 先行注入，资源块无顺序
    // 风险；且必须早于模板全部 Module/MainBody（MTL_ROW 宏在函数体内展开）。
    if (stage == ShaderStage::Fragment)
    {
        AppendStageResourceBlocks(
            out_document, resources, stage, stage_name, material);
    }
    for (int index = 1; index < source_document.GetBlockCount(); ++index)
    {
        const ShaderDocumentBlock &block = source_document.GetBlock(index);
        ShaderDocumentSource block_source = block.source;
        block_source.stage = stage_name;
        block_source.material = material ? material : "";
        out_document.Add(block.kind, block.text, block_source);

        // Mesh：资源声明块（MaterialMeshIndexTables/l2w_index 等）紧跟模板 Extension
        // 块（index 1）之后追加——已 BDA 化的行表声明是 buffer_reference，必须先于
        // 它启用 GL_EXT_buffer_reference（模板 Extension 块内）。历史 bug：资源块
        // 曾前置注入（Version 后），l2w_index 的 buffer_reference 落在扩展声明之前
        // → "required extension not requested"。mesh 模板 index 1 恒为 Extension 块。
        if (index == 1 && stage == ShaderStage::Mesh)
        {
            AppendStageResourceBlocks(
                out_document, resources, stage, stage_name, material);
        }
    }
    return true;
}

}//namespace hgl::graph::mtl
