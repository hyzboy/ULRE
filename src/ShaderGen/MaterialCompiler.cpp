/// MaterialCompiler.cpp — FixedMaterialDef → ShaderProgramBuildSpec 编译器实现
///
/// 流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/MaterialResourceLayout.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/graph/ShaderBufferSources.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/ssbo/MaterialInstanceLayout.h>
#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl {

static bool CStrEq(const char *lhs, const char *rhs)
{
    // Guard against corrupted/non-string pointers from malformed descriptor entries.
    if (lhs && reinterpret_cast<uintptr_t>(lhs) < 0x10000u)
        return false;
    if (rhs && reinterpret_cast<uintptr_t>(rhs) < 0x10000u)
        return false;
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

static std::string BuildCodeModuleGLSL(const ShaderResourceManifest *manifest)
{
    if (!manifest || !manifest->IsValid())
        return {};

    std::string result;
    for (uint32 i = 0; i < manifest->code_module_count; ++i)
    {
        const GLSLCodeModuleDefinition *module =
            FindGLSLCodeModuleDefinition(manifest->code_modules[i]);
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

static bool HasDescriptorSemantic(const FixedMaterialDef &def, const DescriptorSemantic semantic)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const auto &entry = def.descriptor_entries[i];
        if (entry.semantic == semantic)
            return true;
    }

    return false;
}

static bool IsTextureSlotDeclared(const MaterialDefinition &definition, const TextureSlot slot) noexcept
{
    for (const auto &decl : definition.texture_slot_decls)
    {
        if (decl.slot == slot)
            return true;
    }

    return false;
}

static bool HasLayoutSemantic(const MaterialResourceLayout &layout, const DescriptorSemantic semantic) noexcept
{
    for (const auto &req : layout.requirements)
    {
        if (req.semantic == semantic)
            return true;
    }

    return false;
}

static bool AddMaterialSSBOSlotDescriptor(ShaderProgramBuildSpec &mci,
                                          const MaterialSSBOSlotDecl &decl,
                                          const uint32_t ssbo_slot,
                                          const uint32_t stage_bits)
{
    const char *struct_name = nullptr;
    const char *glsl_codes = nullptr;
    uint32_t struct_bytes = 0;

    if (!ssbo::TryGetMaterialInstanceLayout(decl.ssbo_type, struct_name, glsl_codes, struct_bytes))
        return false;

    if (!mci.AddStruct(struct_name, glsl_codes))
        return false;

    return mci.AddSSBO(stage_bits, DescriptorSetType::Material, struct_name, decl.name, int(ssbo_slot));
}

static bool ValidateDefinitionCapabilitySubset(
    const MaterialDefinition &definition,
    const MaterialResourceLayout &layout,
    std::vector<std::string> &diagnostics)
{
    diagnostics.clear();

    for (const auto &req : layout.requirements)
    {
        bool allowed = false;

        switch (req.semantic)
        {
        case DescriptorSemantic::ViewportInfo:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::ViewportInfo);
            break;
        case DescriptorSemantic::CameraInfo:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::CameraInfo);
            break;
        case DescriptorSemantic::SkyInfo:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::SkyInfo);
            break;
        case DescriptorSemantic::MaterialColorPalette:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::MaterialColorPalette);
            break;

        case DescriptorSemantic::LocalToWorld:
        case DescriptorSemantic::LocalToWorldIndexTable:
            allowed = definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
                   && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough;
            break;

        case DescriptorSemantic::MaterialSSBOSlotData:
            allowed = req.ssbo_slot < definition.ssbo_slot_decls.size();
            if (allowed)
                allowed = definition.ssbo_slot_decls[req.ssbo_slot].ssbo_type == req.ssbo_type;
            break;

        case DescriptorSemantic::MaterialTextureLayerTable:
        case DescriptorSemantic::MaterialSSBOIndexTable:
            allowed = !definition.ssbo_slot_decls.empty();
            break;

        case DescriptorSemantic::MaterialTexture:
        case DescriptorSemantic::MaterialSampler:
            allowed = IsTextureSlotDeclared(definition, req.texture_slot);
            break;

        // Sky cubemap sampler is injected by sky-light model and is considered
        // allowed when material declares SkyInfo capability.
        case DescriptorSemantic::SkyCubemapSampler:
            allowed = HasUBORequirement(definition, UBODescriptorSemantic::SkyInfo);
            break;

        case DescriptorSemantic::Unknown:
        case DescriptorSemantic::Custom:
            allowed = false;
            break;
        }

        if (allowed)
            continue;

        std::string message = "Definition capability subset violation: semantic=";
        message += GetDescriptorSemanticName(req.semantic);
        message += ", name=";
        message += (req.name && *req.name) ? req.name : "<unnamed>";
        message += ", def=";
        message += definition.definition_name.empty() ? "<unnamed>" : definition.definition_name;
        diagnostics.push_back(std::move(message));
    }

    return diagnostics.empty();
}

// ═══════════════════════════════════════════════════════════════════════════
// CompileCompositorMaterial — Compositor 模板完整 GLSL → ShaderProgramBuildSpec
//
// 使用 SetFinalGLSL + CreateShaderDirect 直接编译。
// ═══════════════════════════════════════════════════════════════════════════

ShaderProgramBuildSpec *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const CompositorMaterialBuildConfig &config)
{
    if (vs_glsl.empty() || fs_glsl.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s: vs_glsl or fs_glsl is empty\n",
            def.name ? def.name : "<unnamed>");
        return nullptr;
    }

    // ─────────────────────────────────────────────────────────────
    // Step 1: Config
    // ─────────────────────────────────────────────────────────────

    const PrimitiveType primitive_type = config.primitive_type;
    const uint32_t shader_stage_bits = config.shader_stage_flag_bits != 0 ? config.shader_stage_flag_bits : uint32_t(ShaderStage::VertexFragment);

    const bool infer_has_l2w = HasDescriptorSemantic(def, DescriptorSemantic::LocalToWorld);
    const bool with_local_to_world = infer_has_l2w;

    // ─────────────────────────────────────────────────────────────
    // Step 2: Create ShaderProgramBuildSpec
    // ─────────────────────────────────────────────────────────────

    ShaderProgramBuildSpec *mci = new ShaderProgramBuildSpec(primitive_type, shader_stage_bits, with_local_to_world);
    if (profile)
        mci->SetDevice(profile);

    auto FailAfterMci = [&](const char *reason) -> ShaderProgramBuildSpec *
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            def.name ? def.name : "<unnamed>",
            reason ? reason : "<unknown>");
        delete mci;
        return nullptr;
    };

    uint32_t material_ssbo_stage_bits = uint32_t(ShaderStage::Fragment);

    // ─────────────────────────────────────────────────────────────
    // Step 3: Add Descriptors from FixedDescriptorEntry[]
    // When config.ssbo_slot_decls is provided, material SSBO entries are
    // generated from it; any baked single-slot entries in def are skipped.
    // ─────────────────────────────────────────────────────────────

    const bool use_slot_decls = config.ssbo_slot_decls && !config.ssbo_slot_decls->empty();

    const uint32_t declared_material_ssbo_slot_count = use_slot_decls ? static_cast<uint32_t>(config.ssbo_slot_decls->size()) : 0u;

    std::string primary_sampler_name;

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const FixedDescriptorEntry &entry = def.descriptor_entries[i];

        // Skip baked material-SSBO entries when slot_decls takes over.
        if (use_slot_decls && entry.semantic == DescriptorSemantic::MaterialSSBOSlotData)
            continue;

        const uint32_t stage_bits = entry.stage_flags;

        switch (entry.kind)
        {
        case DescriptorKind::UBO:
            switch (entry.semantic)
            {
            case DescriptorSemantic::ViewportInfo:
                mci->AddUBOStruct(stage_bits, SBS_ViewportInfo);
                break;
            case DescriptorSemantic::CameraInfo:
                mci->AddUBOStruct(stage_bits, SBS_CameraInfo);
                break;
            case DescriptorSemantic::SkyInfo:
                mci->AddUBOStruct(stage_bits, SBS_SkyInfo);
                break;
            case DescriptorSemantic::LocalToWorld:
                mci->SetLocalToWorld(stage_bits);
                break;
            case DescriptorSemantic::MaterialSSBOSlotData:
                material_ssbo_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialColorPalette:
                mci->AddUBOStruct(stage_bits, SBS_ColorPattle);
                break;
            default:
                break;
            }
            break;

        case DescriptorKind::SSBO:
            switch (entry.semantic)
            {
            case DescriptorSemantic::LocalToWorld:
                mci->SetLocalToWorld(stage_bits);
                break;
            case DescriptorSemantic::LocalToWorldIndexTable:
                mci->AddSSBOStruct(stage_bits, SBS_LocalToWorldIndexRows);
                break;
            case DescriptorSemantic::MaterialSSBOSlotData:
                material_ssbo_stage_bits = stage_bits;
                break;
            case DescriptorSemantic::MaterialTextureLayerTable:
                if (use_slot_decls)
                {
                    if (!mci->AddStruct(SBS_MaterialTextureLayerRows.struct_name, ""))
                        return FailAfterMci("failed to add MaterialTextureLayerRows struct");
                    if (!mci->AddSSBO(stage_bits,
                                      DescriptorSetType::Material,
                                      SBS_MaterialTextureLayerRows.struct_name,
                                      SBS_MaterialTextureLayerRows.name,
                                      int(declared_material_ssbo_slot_count + 1u)))
                    {
                        return FailAfterMci("failed to add MaterialTextureLayerRows SSBO");
                    }
                }
                else
                {
                    if (!mci->AddSSBOStruct(stage_bits, SBS_MaterialTextureLayerRows))
                        return FailAfterMci("failed to add MaterialTextureLayerRows SSBO");
                }
                break;
            case DescriptorSemantic::MaterialSSBOIndexTable:
                if (use_slot_decls)
                {
                    if (!mci->AddStruct(SBS_MaterialDataIndexRows.struct_name, ""))
                        return FailAfterMci("failed to add MaterialDataIndexRows struct");
                    if (!mci->AddSSBO(stage_bits,
                                      DescriptorSetType::Material,
                                      SBS_MaterialDataIndexRows.struct_name,
                                      SBS_MaterialDataIndexRows.name,
                                      int(declared_material_ssbo_slot_count)))
                    {
                        return FailAfterMci("failed to add MaterialDataIndexRows SSBO");
                    }
                }
                else
                {
                    if (!mci->AddSSBOStruct(stage_bits, SBS_MaterialDataIndexRows))
                        return FailAfterMci("failed to add MaterialDataIndexRows SSBO");
                }
                break;
            default:
                break;
            }
            break;

        case DescriptorKind::Texture:
            if (entry.glsl_type)
            {
                TextureType tt;
                const char *glsl_type_str = entry.glsl_type;

                if (CStrEq(glsl_type_str, "sampler2D"))
                    tt = TextureType::Texture2D;
                else if (CStrEq(glsl_type_str, "sampler3D"))
                    tt = TextureType::Texture3D;
                else if (CStrEq(glsl_type_str, "samplerCube"))
                    tt = TextureType::TextureCube;
                else if (CStrEq(glsl_type_str, "sampler2DArray"))
                    tt = TextureType::Texture2DArray;
                else
                    tt = TextureType::Texture2D;

                mci->AddTexture(ShaderStage(stage_bits), entry.set_type, tt, entry.name);
            }
            break;

        case DescriptorKind::TextureSampler:
            if (entry.glsl_type)
            {
                if (primary_sampler_name.empty() && entry.name && *entry.name)
                    primary_sampler_name = entry.name;

                TextureType tt;
                SamplerType st = SamplerType::Sampler2D;
                const char *glsl_type_str = entry.glsl_type;

                if (CStrEq(glsl_type_str, "sampler2D")) {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                } else if (CStrEq(glsl_type_str, "samplerCube")) {
                    tt = TextureType::TextureCube;
                    st = SamplerType::SamplerCube;
                } else if (CStrEq(glsl_type_str, "sampler2DArray")) {
                    tt = TextureType::Texture2DArray;
                    st = SamplerType::Sampler2DArray;
                } else {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                }

                mci->AddTextureSampler(ShaderStage(stage_bits), entry.set_type, st, entry.name);
            }
            break;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 4: Add Vertex Inputs from FixedVertexEntry[]
    // ─────────────────────────────────────────────────────────────

    ShaderCreateInfoVertex *vsc = mci->GetVertexShader();
    if (vsc)
    {
        for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        {
            const FixedVertexEntry &entry = def.vertex_entries[i];
            vsc->AddInput(entry.format, entry.semantic);
        }
    }

    if (use_slot_decls)
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.ssbo_slot_decls->size()); ++i)
        {
            if (!AddMaterialSSBOSlotDescriptor(*mci, (*config.ssbo_slot_decls)[i], i, material_ssbo_stage_bits))
                return FailAfterMci("failed to add declared material ssbo slot descriptor");
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Step 6: Set complete GLSL (bypass ProcXXX pipeline)
    // ─────────────────────────────────────────────────────────────

    constexpr uint32_t texture_slot_range_size = static_cast<uint32_t>(TextureSlot::RANGE_SIZE);

    std::string binding_preamble;
    binding_preamble += "#define TEXTURE_SLOT_RANGE_SIZE " + std::to_string(texture_slot_range_size) + "u\n";

    auto AppendDescriptorBindingDefine = [&](const char *macro_name, const ShaderDescriptor *sd)
    {
        if (!macro_name || !sd || sd->set < 0 || sd->binding < 0)
            return;
        binding_preamble += "#define ";
        binding_preamble += macro_name;
        binding_preamble += " ";
        binding_preamble += std::to_string(sd->set);
        binding_preamble += "\n";
        binding_preamble += "#define ";
        binding_preamble += std::string(macro_name).replace(std::string(macro_name).find("_SET"), 4, "_BINDING");
        binding_preamble += " ";
        binding_preamble += std::to_string(sd->binding);
        binding_preamble += "\n";
    };

    const MaterialDescriptorInfo &descriptor_info = mci->GetDescriptorInfo();

    // 行表绑定（mtl_data_index_rows / mtl_texture_layer_rows / l2w_index_rows）不再注入
    // set/binding 宏：声明由下方 index table 生成逻辑依据 descriptor_info 直接以
    // layout(set=.., binding=..) 写出（统一声明生成，不再写死在 .glsl）。
    AppendDescriptorBindingDefine("L2W_SET", descriptor_info.GetSSBO(SBS_LocalToWorld.name));
    AppendDescriptorBindingDefine("VIEWPORT_SET", descriptor_info.GetUBO(SBS_ViewportInfo.name));
    AppendDescriptorBindingDefine("CAMERA_SET", descriptor_info.GetUBO(SBS_CameraInfo.name));
    AppendDescriptorBindingDefine("SKY_SET", descriptor_info.GetUBO(SBS_SkyInfo.name));

    const TextureSamplerDescriptor *primary_sampler = nullptr;
    if (!primary_sampler_name.empty())
        primary_sampler = descriptor_info.GetTextureSampler(primary_sampler_name.c_str());

    if (!primary_sampler)
        primary_sampler = descriptor_info.GetTextureSampler("TextureBaseColor");

    if (!primary_sampler && config.material_definition)
    {
        for (const auto &slot_decl : config.material_definition->texture_slot_decls)
        {
            if (!slot_decl.name || !*slot_decl.name)
                continue;

            primary_sampler = descriptor_info.GetTextureSampler(slot_decl.name);
            if (primary_sampler)
                break;
        }
    }

    if (primary_sampler)
    {
        if (primary_sampler->set >= 0 && primary_sampler->binding >= 0)
        {
            binding_preamble += "#define TEX_SET " + std::to_string(primary_sampler->set) + "\n";
            binding_preamble += "#define TEX_BINDING " + std::to_string(primary_sampler->binding) + "\n";
        }
    }

    // ── Material SSBO GLSL 声明 ─────────────────────────────────────────────
    // 材质实例 SSBO 的 struct + buffer 声明不再写死在 .glsl 中，
    // 统一由此处依据 ssbo_slot_decls 生成并注入 Fragment 阶段。
    std::string material_ssbo_decls;

    if (use_slot_decls && config.ssbo_slot_decls && !config.ssbo_slot_decls->empty())
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.ssbo_slot_decls->size()); ++i)
        {
            const MaterialSSBOSlotDecl &decl = (*config.ssbo_slot_decls)[i];
            const ShaderDescriptor *sd = descriptor_info.GetSSBO(decl.name.c_str());
            if (!sd || sd->set < 0 || sd->binding < 0)
                return FailAfterMci("material ssbo descriptor unresolved for GLSL generation");

            const char *struct_name  = ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
            const char *struct_codes = ssbo::GetMaterialInstanceGLSL(decl.ssbo_type);
            if (!struct_name || !struct_codes)
                return FailAfterMci("unsupported material ssbo type for GLSL generation");

            std::string buffer_name(struct_name);
            const size_t name_len = buffer_name.size();
            if (name_len > 4 && buffer_name.compare(name_len - 4, 4, "Data") == 0)
                buffer_name.resize(name_len - 4);
            buffer_name += "Buffer";

            material_ssbo_decls += "struct ";
            material_ssbo_decls += struct_name;
            material_ssbo_decls += "\n{\n";

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
                    material_ssbo_decls += "    ";
                    material_ssbo_decls.append(line, start, line.size() - start);
                    material_ssbo_decls += '\n';
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

            material_ssbo_decls += "};\n";
            material_ssbo_decls += "layout(set=" + std::to_string(sd->set) + ", binding=" + std::to_string(sd->binding) + ") readonly buffer ";
            material_ssbo_decls += buffer_name;
            material_ssbo_decls += " {\n    ";
            material_ssbo_decls += struct_name;
            material_ssbo_decls += " mi[];\n} ";
            material_ssbo_decls += decl.name;
            material_ssbo_decls += ";\n";
        }
    }

    // ── Instance index table SSBO GLSL 声明 ──────────────────────────────────
    // mtl_data_index_rows / mtl_texture_layer_rows / l2w_index_rows 的 buffer
    // 声明与 Resolve 函数不再写死在 instance_rows_ssbo.glsl 中，统一依据
    // descriptor_info 生成注入：VS 阶段提供 l2w_index_rows / mtl_data_index_rows
    //（含 ResolveTransformID / ResolveDataIndexID），FS 阶段提供
    // mtl_texture_layer_rows（bindless GetTextureHandle 行表，仅需 buffer 声明）。
    struct IndexTableSpec
    {
        const char *buffer_name;
        const char *var_name;
        const char *resolve_func;   // 为空则仅生成 buffer 声明
    };

    auto AppendIndexTableDecl = [](std::string &out, const ShaderDescriptor *sd, const IndexTableSpec &spec)
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
            out += "uint ";
            out += spec.resolve_func;
            out += "(uint iid) { return ";
            out += spec.var_name;
            out += ".values[iid]; }\n";
        }
    };

    std::string vs_index_table_decls;
    std::string fs_index_table_decls;

    AppendIndexTableDecl(vs_index_table_decls, descriptor_info.GetSSBO(SBS_LocalToWorldIndexRows.name),
                         { "LocalToWorldIndexRows", "l2w_index_rows", "ResolveTransformID" });
    AppendIndexTableDecl(vs_index_table_decls, descriptor_info.GetSSBO(SBS_MaterialDataIndexRows.name),
                         { "DataIndexRows", "mtl_data_index_rows", "ResolveDataIndexID" });
    AppendIndexTableDecl(fs_index_table_decls, descriptor_info.GetSSBO(SBS_MaterialTextureLayerRows.name),
                         { "TextureLayerRows", "mtl_texture_layer_rows", nullptr });

    // GLSL requires #version to be the very first token.
    auto InsertAfterVersionLine = [](const std::string &glsl, const std::string &inject) -> std::string
    {
        if (inject.empty())
            return glsl;
        const auto pos = glsl.find('\n');
        if (pos == std::string::npos)
            return glsl + "\n" + inject;
        return glsl.substr(0, pos + 1) + inject + glsl.substr(pos + 1);
    };

    auto InsertBeforeSurfaceFunction = [](const std::string &glsl, const std::string &inject) -> std::string
    {
        if (inject.empty())
            return glsl;
        const std::string marker = "#include SURFACE_FUNCTION_FILE";
        auto pos = glsl.find(marker);
        if (pos == std::string::npos)
        {
            const auto include_pos = glsl.find("#include \"surface/");
            pos = include_pos;
        }
        if (pos == std::string::npos)
            return glsl + "\n" + inject;
        return glsl.substr(0, pos) + inject + "\n" + glsl.substr(pos);
    };

    std::string vs_final = InsertAfterVersionLine(vs_glsl, binding_preamble + vs_index_table_decls);
    std::string fs_final = InsertAfterVersionLine(fs_glsl, binding_preamble + fs_index_table_decls + material_ssbo_decls);
    fs_final = InsertBeforeSurfaceFunction(fs_final, BuildCodeModuleGLSL(config.resource_manifest));

    ShaderCreateInfoVertex   *vert = mci->GetVertexShader();
    ShaderCreateInfo         *frag = mci->GetStageShader(ShaderStage::Fragment);

    if (vert)
        vert->SetFinalGLSL(vs_final);

    if (frag)
        frag->SetFinalGLSL(fs_final);

    // ─────────────────────────────────────────────────────────────
    // Step 6b: Build MaterialResourceLayout from descriptor entries.
    // When ssbo_slot_decls is provided, material SSBO entries are
    // generated from it and merged with the rest of def.descriptor_entries.
    // ─────────────────────────────────────────────────────────────

    MaterialResourceLayout material_resource_layout;
    if (use_slot_decls)
    {
        // Build augmented list: base entries (without legacy single-slot material SSBO) + slot_decls entries.
        std::vector<FixedDescriptorEntry> augmented;
        // Names from ssbo_slot_decls are runtime strings; store them here to ensure lifetime.
        std::vector<std::string> augmented_names;
        augmented.reserve(def.descriptor_entry_count + config.ssbo_slot_decls->size());
        augmented_names.reserve(config.ssbo_slot_decls->size());
        for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
        {
            if (def.descriptor_entries[i].semantic != DescriptorSemantic::MaterialSSBOSlotData)
                augmented.push_back(def.descriptor_entries[i]);
        }
        for (uint32_t i = 0; i < static_cast<uint32_t>(config.ssbo_slot_decls->size()); ++i)
        {
            const MaterialSSBOSlotDecl &decl = (*config.ssbo_slot_decls)[i];
            augmented_names.push_back(decl.name);  // owned copy guarantees lifetime
            FixedDescriptorEntry e{};
            e.set_type      = DescriptorSetType::Material;
            e.kind          = DescriptorKind::SSBO;
            e.stage_flags   = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
            e.name          = augmented_names.back().c_str();  // stable pointer into owned string
            e.struct_name   = ssbo::GetMaterialSSBOStructName(decl.ssbo_type);
            e.glsl_type     = nullptr;
            e.semantic      = DescriptorSemantic::MaterialSSBOSlotData;
            e.texture_slot  = TextureSlot::BaseColor;
            e.ssbo_slot     = i;
            e.ssbo_type     = decl.ssbo_type;
            e.semantic_layer = GetDescriptorSemanticLayerByKind(e.kind);
            e.ssbo_id       = MakeRecipeSSBOId(i);
            augmented.push_back(e);
        }
        material_resource_layout = BuildMaterialResourceLayout(augmented.data(), static_cast<uint32_t>(augmented.size()));

        // Fix up dangling name pointers: BuildMaterialResourceLayout copied the const char* pointers.
        // Any requirement whose name pointed into augmented_names must be re-pointed via owned_name.
        for (auto &req : material_resource_layout.requirements)
        {
            if (req.semantic != DescriptorSemantic::MaterialSSBOSlotData)
                continue;
            // req.ssbo_slot is the slot index, which matches augmented_names order.
            if (req.ssbo_slot < static_cast<uint32_t>(augmented_names.size()))
            {
                req.owned_name   = augmented_names[req.ssbo_slot];
                req.name         = req.owned_name.c_str();
            }
        }
    }
    else
    {
        material_resource_layout = BuildMaterialResourceLayout(def.descriptor_entries, def.descriptor_entry_count);
    }

    std::vector<std::string> contract_diagnostics;
    if (!ValidateMaterialResourceLayout(material_resource_layout, contract_diagnostics))
    {
        for (const auto &diag : contract_diagnostics)
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial][MaterialResourceLayout] material=%s: %s\n",
                def.name ? def.name : "<unnamed>",
                diag.c_str());
        }
        return FailAfterMci("MaterialResourceLayout validation failed");
    }

    if (config.material_definition)
    {
        const MaterialDefinition &material_definition = *config.material_definition;
        const bool has_stage_contract =
            !material_definition.vertex_stage.inputs.IsEmpty()
            || !material_definition.vertex_stage.outputs.IsEmpty()
            || !material_definition.fragment_stage.inputs.IsEmpty()
            || !material_definition.fragment_stage.outputs.IsEmpty();
        if (has_stage_contract)
        {
            if (material_definition.vertex_stage.stage != ShaderStage::Vertex
             || material_definition.fragment_stage.stage != ShaderStage::Fragment
             || !HasCompatibleStageInterface(material_definition.vertex_stage,
                                              material_definition.fragment_stage))
                return FailAfterMci("MaterialDefinition stage interface contract is invalid");

            ShaderProgramLinkSpec link = material_definition.program_link;
            if (!link.IsValid())
            {
                link.vertex_stage = material_definition.vertex_stage.BuildKey();
                link.fragment_stage = material_definition.fragment_stage.BuildKey();
            }
            (void)link.BuildKey();
        }

        std::vector<std::string> capability_diagnostics;
        if (!ValidateDefinitionCapabilitySubset(material_definition, material_resource_layout, capability_diagnostics))
        {
            for (const auto &diag : capability_diagnostics)
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial][DefinitionCapability] material=%s: %s\n",
                    def.name ? def.name : "<unnamed>",
                    diag.c_str());
            }
            return FailAfterMci("Definition capability subset validation failed");
        }
    }

    mci->SetMaterialResourceLayout(material_resource_layout);

    // ─────────────────────────────────────────────────────────────
    // Step 7: Compile directly → SPV
    // ─────────────────────────────────────────────────────────────

    if (!mci->CreateShaderDirect())
    {
        return FailAfterMci("CreateShaderDirect() failed (check GLSLCompiler log)");
    }

    return mci;
}

}  // namespace hgl::graph::mtl
