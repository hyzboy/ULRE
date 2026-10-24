#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    const char *GetMaterialDomainName(const MaterialDomain domain) noexcept
    {
        switch (domain)
        {
        case MaterialDomain::Custom:         return "Custom";
        case MaterialDomain::StandardOpaque: return "StandardOpaque";
        case MaterialDomain::Foliage:        return "Foliage";
        case MaterialDomain::Terrain:        return "Terrain";
        case MaterialDomain::UI2D:           return "UI2D";
        case MaterialDomain::HumanSkin:      return "HumanSkin";
        default:                             return "Unknown";
        }
    }

    bool ParseMaterialDomain(const char *name, MaterialDomain &out_domain) noexcept
    {
        if (!name || !*name)
            return false;

        for (int i = 0; i <= static_cast<int>(MaterialDomain::HumanSkin); ++i)
        {
            const MaterialDomain domain = static_cast<MaterialDomain>(i);
            const char *candidate = GetMaterialDomainName(domain);
            if (candidate && hgl::strcmp(candidate, name) == 0)
            {
                out_domain = domain;
                return true;
            }
        }
        return false;
    }

    const char *GetMaterialDomainTextureRoleName(
        const MaterialDomainTextureRole role) noexcept
    {
        switch (role)
        {
        case MaterialDomainTextureRole::BaseColor:    return "BaseColor";
        case MaterialDomainTextureRole::Albedo:       return "Albedo";
        case MaterialDomainTextureRole::Normal:       return "Normal";
        case MaterialDomainTextureRole::Metallic:     return "Metallic";
        case MaterialDomainTextureRole::Roughness:    return "Roughness";
        case MaterialDomainTextureRole::Occlusion:    return "Occlusion";
        case MaterialDomainTextureRole::OpacityMask:  return "OpacityMask";
        case MaterialDomainTextureRole::Transmission: return "Transmission";
        case MaterialDomainTextureRole::SplatMap:     return "SplatMap";
        case MaterialDomainTextureRole::Curvature:    return "Curvature";
        case MaterialDomainTextureRole::Sprite:       return "Sprite";
        default:                                      return "Unknown";
        }
    }

    namespace
    {
        // ── 契约表 ──────────────────────────────────────────────────────────────
        // 槽名即材质 TOML `[resources] textures` 里 `name = "..."` 的写法，
        // 也是 GLSL 侧 `MTL_TEX(row).tex_<name>` 的后缀。
        //
        // 顺序有意义：ShaderGen 按声明顺序排布引用行，顺序变化 = layout_hash 变化
        // = 缓存失效。故**不要**重排既有领域的槽顺序。

        // StandardOpaque —— 通用硬表面。
        //
        // 顶点：Position/Normal/UV0。
        //   · **不含 Tangent**：法线贴图走 `ntb_tangent_vbo_normalmap` 的余切标架
        //     （cotangent frame，由世界位置与 UV 的屏幕空间导数还原切线基），
        //     引擎也确实不向 Fragment 传 Tangent/Binormal varying。契约若要求
        //     Tangent 就等于要求一条并不存在的顶点流。
        //   · **不含 Color/Luminance/Size**：那些是纯色/顶点色/精灵/文本形态，
        //     不属于硬表面。
        //
        // 纹理：六槽全声明。`must_declare = true` 保证**布局稳定**（同一领域所有
        // 材质的引用行布局一致 ⇒ 可共享 layout_hash），但 `required = false`
        // 仍是作者可选项（未绑定时 SampleOptional 以中性值保底）。
        constexpr MaterialDomainTextureSlot StandardOpaqueSlots[] = {
            { "base_color",   MaterialDomainTextureRole::BaseColor,   true },
            { "roughness",    MaterialDomainTextureRole::Roughness,   true },
            { "metallic",     MaterialDomainTextureRole::Metallic,    true },
            { "occlusion",    MaterialDomainTextureRole::Occlusion,   true },
            { "opacity_mask", MaterialDomainTextureRole::OpacityMask, true },
            { "normal",       MaterialDomainTextureRole::Normal,      true },
        };

        // Foliage —— 植被。顶点增加风力参数（走 TransformID 之外的独立通路时再扩），
        // 纹理增加透射槽。当前无实现材质，先立契约占位。
        constexpr MaterialDomainTextureSlot FoliageSlots[] = {
            { "base_color",     MaterialDomainTextureRole::BaseColor,    true },
            { "normal",         MaterialDomainTextureRole::Normal,       true },
            { "roughness",      MaterialDomainTextureRole::Roughness,    true },
            { "transmission",   MaterialDomainTextureRole::Transmission, true },
            { "opacity_mask",   MaterialDomainTextureRole::OpacityMask,  true },
        };

        // Terrain —— 多层地形。权重贴图 + 层纹理（层数组走同一槽名的数组采样器）。
        constexpr MaterialDomainTextureSlot TerrainSlots[] = {
            { "splat_map",  MaterialDomainTextureRole::SplatMap,  true },
            { "base_color", MaterialDomainTextureRole::BaseColor, true },
            { "normal",     MaterialDomainTextureRole::Normal,    true },
        };

        // UI2D —— 界面/文本，无光照。
        constexpr MaterialDomainTextureSlot UI2DSlots[] = {
            { "sprite", MaterialDomainTextureRole::Sprite, true },
        };

        // HumanSkin —— 人体皮肤。除标准四槽外增加曲率（预积分 SSS）。
        constexpr MaterialDomainTextureSlot HumanSkinSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor,  true },
            { "normal",     MaterialDomainTextureRole::Normal,     true },
            { "roughness",  MaterialDomainTextureRole::Roughness,  true },
            { "curvature",  MaterialDomainTextureRole::Curvature,  true },
        };

        constexpr uint32 Sem(VertexSemantic semantic) noexcept
        {
            return MaterialDomainSemanticBit(semantic);
        }

        constexpr MaterialDomainContract DomainContracts[] = {
            {
                MaterialDomain::StandardOpaque, "StandardOpaque",
                "pbr_surface_source", "ntb_tangent_vbo_normalmap",
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal)
                    | Sem(VertexSemantic::TexCoord),
                StandardOpaqueSlots,
                static_cast<int>(sizeof(StandardOpaqueSlots)
                                 / sizeof(StandardOpaqueSlots[0])),
                false,
            },
            {
                MaterialDomain::Foliage, "Foliage",
                // 尚未实现：植被需要透射/双面法线/风力顶点位移，不能直接复用
                // pbr_surface_source。Phase 3/4 落地 foliage_surface_source 时补齐。
                nullptr, nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal)
                    | Sem(VertexSemantic::TexCoord),
                FoliageSlots,
                static_cast<int>(sizeof(FoliageSlots) / sizeof(FoliageSlots[0])),
                false,
            },
            {
                MaterialDomain::Terrain, "Terrain",
                // 尚未实现：地形需要 splat 混合 + 世界空间导数标架。
                nullptr, nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal)
                    | Sem(VertexSemantic::TexCoord),
                TerrainSlots,
                static_cast<int>(sizeof(TerrainSlots) / sizeof(TerrainSlots[0])),
                false,
            },
            {
                MaterialDomain::UI2D, "UI2D",
                "texture_source", "ntb_orthonormal",
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::TexCoord),
                UI2DSlots,
                static_cast<int>(sizeof(UI2DSlots) / sizeof(UI2DSlots[0])),
                false,
            },
            {
                MaterialDomain::HumanSkin, "HumanSkin",
                // 尚未实现：皮肤需要 SSS/预积分曲率查表，不能直接复用
                // pbr_surface_source。Phase 3/4 落地 skin_surface_source 时补齐。
                nullptr, nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal)
                    | Sem(VertexSemantic::TexCoord),
                HumanSkinSlots,
                static_cast<int>(sizeof(HumanSkinSlots) / sizeof(HumanSkinSlots[0])),
                false,
            },
        };

        static_assert(
            static_cast<int>(sizeof(DomainContracts) / sizeof(DomainContracts[0]))
                == MaterialDomainContractCount,
            "MaterialDomainContractCount 必须与契约表条目数一致");
    }// namespace

    const MaterialDomainContract *GetMaterialDomainContract(
        const MaterialDomain domain) noexcept
    {
        if (domain == MaterialDomain::Custom)
            return nullptr;

        for (const MaterialDomainContract &contract : DomainContracts)
            if (contract.domain == domain)
                return &contract;

        return nullptr;
    }

    const MaterialDomainContract *GetMaterialDomainContractAt(
        const int index) noexcept
    {
        if (index < 0 || index >= MaterialDomainContractCount)
            return nullptr;

        return &DomainContracts[index];
    }

    const char *GetMaterialDomainValidationResultName(
        const MaterialDomainValidationResult result) noexcept
    {
        switch (result)
        {
        case MaterialDomainValidationResult::OK:                        return "OK";
        case MaterialDomainValidationResult::UnknownDomain:             return "UnknownDomain";
        case MaterialDomainValidationResult::NoContract:                return "NoContract";
        case MaterialDomainValidationResult::MissingVertexSemantic:     return "MissingVertexSemantic";
        case MaterialDomainValidationResult::UndeclaredRequiredTexture: return "UndeclaredRequiredTexture";
        case MaterialDomainValidationResult::UnexpectedTextureSlot:     return "UnexpectedTextureSlot";
        case MaterialDomainValidationResult::VertexProviderPolicyMismatch:
            return "VertexProviderPolicyMismatch";
        case MaterialDomainValidationResult::MissingMaterialSource:     return "MissingMaterialSource";
        default:                                                        return "Invalid";
        }
    }

    MaterialDomainValidationResult ValidateMaterialDefinitionDomain(
        const MaterialDefinition &definition,
        const MaterialDomain domain,
        AnsiString &out_detail) noexcept
    {
        out_detail.Clear();

        // 未声明领域 = Custom = 完全豁免契约校验（向后兼容 schema-4 历史材质）。
        // 这里必须返回 OK 而不是 NoContract：调用方只把 OK 当通过，
        // 返回 NoContract 会让每一个既有材质都变成解析失败。
        if (domain == MaterialDomain::Custom)
            return MaterialDomainValidationResult::OK;

        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        if (!contract)
            return MaterialDomainValidationResult::NoContract;

        // ── 1. 顶点语义覆盖 ─────────────────────────────────────────────────────
        uint32 declared_mask = 0;
        for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
        {
            const ShaderCodeModuleSemantic semantic =
                definition.vertex_semantic_requirements[i].semantic;
            const VertexSemantic mapped = GetVertexSemanticFromShaderCodeModuleSemantic(semantic);
            if (mapped != VertexSemantic::Unknown)
                declared_mask |= MaterialDomainSemanticBit(mapped);
        }

        const uint32 missing_mask = contract->required_vertex_semantic_mask & ~declared_mask;
        if (missing_mask != 0)
        {
            out_detail = "[vertex] requirements 缺少领域 ";
            out_detail += contract->name;
            out_detail += " 必需的顶点语义：";
            bool first = true;
            for (int bit = static_cast<int>(VertexSemantic::Position);
                 bit <= static_cast<int>(VertexSemantic::TransformID); ++bit)
            {
                if ((missing_mask & (1u << bit)) == 0)
                    continue;
                if (!first)
                    out_detail += ", ";
                out_detail += GetVertexSemanticName(static_cast<VertexSemantic>(bit));
                first = false;
            }
            return MaterialDomainValidationResult::MissingVertexSemantic;
        }

        // ── 2. 顶点提供策略 ────────────────────────────────────────────────────
        if (contract->require_allow_derived
            && definition.vertex_provider_policy != MaterialVertexProviderPolicy::AllowDerived)
        {
            out_detail = "[root] provider_policy 必须为 AllowDerived（领域 ";
            out_detail += contract->name;
            out_detail += " 需要从几何流推导顶点）";
            return MaterialDomainValidationResult::VertexProviderPolicyMismatch;
        }

        // ── 3. 表面来源模块 ────────────────────────────────────────────────────
        // UI2D 是纯纹理/无光照形态，允许 surface_module_name 走天空式直通路径；
        // 其余契约领域必须声明 material_source_module_name，否则表面数据无来源。
        if (domain != MaterialDomain::UI2D
            && (!definition.material_source_module_name
                || !*definition.material_source_module_name))
        {
            out_detail = "[fragment] material_source_module_name 必须声明（领域 ";
            out_detail += contract->name;
            out_detail += " 需要 EvalMaterialSource 提供表面数据）";
            return MaterialDomainValidationResult::MissingMaterialSource;
        }

        // ── 4. 纹理槽白名单 + 必声明项 ──────────────────────────────────────────
        for (const MaterialTextureDeclaration &declaration : definition.texture_declarations)
        {
            bool known = false;
            for (int i = 0; i < contract->texture_slot_count; ++i)
            {
                const MaterialDomainTextureSlot &slot = contract->texture_slots[i];
                if (slot.name && declaration.name == slot.name)
                {
                    known = true;
                    break;
                }
            }
            if (!known)
            {
                out_detail = "[resources] textures 声明了领域 ";
                out_detail += contract->name;
                out_detail += " 白名单之外的槽 \"";
                out_detail += declaration.name.c_str();
                out_detail += "\"（允许：";
                for (int i = 0; i < contract->texture_slot_count; ++i)
                {
                    if (i)
                        out_detail += ", ";
                    out_detail += contract->texture_slots[i].name;
                }
                out_detail += "）";
                return MaterialDomainValidationResult::UnexpectedTextureSlot;
            }
        }

        for (int i = 0; i < contract->texture_slot_count; ++i)
        {
            const MaterialDomainTextureSlot &slot = contract->texture_slots[i];
            if (!slot.must_declare)
                continue;

            bool declared = false;
            for (const MaterialTextureDeclaration &declaration : definition.texture_declarations)
            {
                if (slot.name && declaration.name == slot.name)
                {
                    declared = true;
                    break;
                }
            }
            if (!declared)
            {
                out_detail = "[resources] textures 缺少领域 ";
                out_detail += contract->name;
                out_detail += " 必须声明的槽 \"";
                out_detail += slot.name;
                out_detail += "\"（角色 ";
                out_detail += GetMaterialDomainTextureRoleName(slot.role);
                out_detail += "；声明后绑定仍可为 optional）";
                return MaterialDomainValidationResult::UndeclaredRequiredTexture;
            }
        }

        return MaterialDomainValidationResult::OK;
    }
}// namespace hgl::graph::mtl


