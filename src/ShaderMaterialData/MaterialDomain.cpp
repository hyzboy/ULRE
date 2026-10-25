#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/type/String.h>

#include <cstring>

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

        case MaterialDomain::UnlitSolid:      return "UnlitSolid";
        case MaterialDomain::VertexColor:     return "VertexColor";
        case MaterialDomain::VertexPalette:   return "VertexPalette";
        case MaterialDomain::VertexLuminance: return "VertexLuminance";
        case MaterialDomain::UnlitTexture:    return "UnlitTexture";
        case MaterialDomain::Text2D:          return "Text2D";

        case MaterialDomain::Sky:             return "Sky";
        case MaterialDomain::DebugNormal:     return "DebugNormal";
        case MaterialDomain::Checker2D:       return "Checker2D";
        case MaterialDomain::Checker3D:       return "Checker3D";
        case MaterialDomain::ErrorChecker3D:  return "ErrorChecker3D";

        default:                             return "Unknown";
        }
    }

    bool ParseMaterialDomain(const char *name, MaterialDomain &out_domain) noexcept
    {
        if (!name || !*name)
            return false;

        // 上界必须跟着枚举最后一项走；漏改会让新领域名解析不出来，
        // 表现为「TOML 里写对了却报 unknown domain」。
        constexpr int kLastDomain = static_cast<int>(MaterialDomain::ErrorChecker3D);
        for (int i = 0; i <= kLastDomain; ++i)
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
        // 顺序有意义：程序构建阶段按声明顺序排布引用行，顺序变化 = layout_hash 变化。
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
        // 材质的引用行布局一致 ⇒ 可共享 layout_hash），但 `binding_required = false`
        // 仍是作者可选项（未绑定时 SampleOptional 以中性值保底）。
        //
        // 阶段三之二：`sampler / binding_required / channels` 也从资产上移到契约。
        // 同一个领域里这三个值必然相同——它们是「这个槽在领域里长什么样」，
        // 不是「这份资产想怎么用」。
        constexpr MaterialDomainTextureSlot StandardOpaqueSlots[] = {
            { "base_color",   MaterialDomainTextureRole::BaseColor,   true,
              SamplerType::Sampler2DArray, false, 0 },
            { "roughness",    MaterialDomainTextureRole::Roughness,   true,
              SamplerType::Sampler2DArray, false, 0 },
            { "metallic",     MaterialDomainTextureRole::Metallic,    true,
              SamplerType::Sampler2DArray, false, 0 },
            { "occlusion",    MaterialDomainTextureRole::Occlusion,   true,
              SamplerType::Sampler2DArray, false, 0 },
            { "opacity_mask", MaterialDomainTextureRole::OpacityMask, true,
              SamplerType::Sampler2DArray, false, 0 },
            // normal 固定双通道（BC5：只存 XY，Z 由 shader 还原）。
            { "normal",       MaterialDomainTextureRole::Normal,      true,
              SamplerType::Sampler2DArray, false, 2 },
        };

        // Foliage —— 植被。顶点增加风力参数（走 TransformID 之外的独立通路时再扩），
        // 纹理增加透射槽。当前无实现材质，先立契约占位。
        constexpr MaterialDomainTextureSlot FoliageSlots[] = {
            { "base_color",     MaterialDomainTextureRole::BaseColor,    true,
              SamplerType::Sampler2DArray, false, 0 },
            { "normal",         MaterialDomainTextureRole::Normal,       true,
              SamplerType::Sampler2DArray, false, 2 },
            { "roughness",      MaterialDomainTextureRole::Roughness,    true,
              SamplerType::Sampler2DArray, false, 0 },
            { "transmission",   MaterialDomainTextureRole::Transmission, true,
              SamplerType::Sampler2DArray, false, 0 },
            { "opacity_mask",   MaterialDomainTextureRole::OpacityMask,  true,
              SamplerType::Sampler2DArray, false, 0 },
        };

        // Terrain —— 多层地形。权重贴图 + 层纹理（层数组走同一槽名的数组采样器）。
        constexpr MaterialDomainTextureSlot TerrainSlots[] = {
            { "splat_map",  MaterialDomainTextureRole::SplatMap,  true,
              SamplerType::Sampler2DArray, false, 0 },
            { "base_color", MaterialDomainTextureRole::BaseColor, true,
              SamplerType::Sampler2DArray, false, 0 },
            { "normal",     MaterialDomainTextureRole::Normal,    true,
              SamplerType::Sampler2DArray, false, 2 },
        };

        // UI2D —— 界面/文本，无光照。
        constexpr MaterialDomainTextureSlot UI2DSlots[] = {
            { "sprite", MaterialDomainTextureRole::Sprite, true,
              SamplerType::Sampler2DArray, false, 0 },
        };

        // HumanSkin —— 人体皮肤。除标准四槽外增加曲率（预积分 SSS）。
        constexpr MaterialDomainTextureSlot HumanSkinSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor,  true,
              SamplerType::Sampler2DArray, false, 0 },
            { "normal",     MaterialDomainTextureRole::Normal,     true,
              SamplerType::Sampler2DArray, false, 2 },
            { "roughness",  MaterialDomainTextureRole::Roughness,  true,
              SamplerType::Sampler2DArray, false, 0 },
            { "curvature",  MaterialDomainTextureRole::Curvature,  true,
              SamplerType::Sampler2DArray, false, 0 },
        };

        // ── 无光表面族的槽表 ────────────────────────────────────────────────────
        //
        // 这一族共同点：**没有 must_declare 的强制槽**（除贴图类）。
        // 纯色/顶点色材质结构上就不该有纹理，若给它们开一个槽白名单，
        // 就等于允许「纯色材质偷偷挂贴图」这种自相矛盾的组合。

        // UnlitTexture —— 无光单图。只有一张基础色。
        constexpr MaterialDomainTextureSlot UnlitTextureSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor, true,
              SamplerType::Sampler2DArray, true, 0 },
        };

        // Text2D —— GPU 文本。SDF 与位图两种形态都只有一张图集。
        // （SDF 的阈值参数走材质数值，不占纹理槽。）
        //
        // 注意：这里定的是**采样器种类**，不是过滤器。SDF 用 Linear、位图用
        // Nearest，那是 `[resources].samplers` 里的过滤设置，仍留在资产上——
        // 同一领域的不同资产在过滤器上本来就该不同。
        constexpr MaterialDomainTextureSlot Text2DSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor, true,
              SamplerType::Sampler2DArray, true, 0 },
        };

        // Sky —— 天空。cube 贴图**可选**：sky_minimal 是纯解析大气，无贴图。
        // must_declare = false 让两种形态共用一个领域，避免为「有没有贴图」
        // 分裂出两个领域。
        constexpr MaterialDomainTextureSlot SkySlots[] = {
            { "sky_cube", MaterialDomainTextureRole::BaseColor, false,
              SamplerType::SamplerCube, true, 0 },
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
                false,
                {},
                // 通用硬表面 = PBR 表面字段组。
                GlobalSSBOType::PBRSurface,
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
                false,
                {},
                GlobalSSBOType::PBRSurface,
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
                false,
                {},
                GlobalSSBOType::PBRSurface,
            },
            {
                MaterialDomain::UI2D, "UI2D",
                "texture_source", "ntb_orthonormal",
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::TexCoord),
                UI2DSlots,
                static_cast<int>(sizeof(UI2DSlots) / sizeof(UI2DSlots[0])),
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
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
                false,
                {},
                GlobalSSBOType::PBRSurface,
            },

            // ══ 无光表面族 ═══════════════════════════════════════════════════════
            // 这些领域原先都是 `domain = "Custom"` 的引擎内部材质。领域化之后，
            // 「新增一种无光形态」必须走新增领域（要过门禁），而不是随手写个
            // Custom 材质绕过所有检查。
            //
            // 这一族全部把 reference_ntb_module 置 nullptr，理由：
            //   · 无光表面不做切线空间法线贴图，NTB 标架没有消费方；
            //   · 校验器对 ntb == nullptr 的处理是「不校验、也不要求声明」，
            //     而填一个名字会让解析层**兜底注入** NTB 模块代码，
            //     凭空改变既有材质的 shader 输出。契约不该干这种事。
            // 需要法线的领域（DebugNormal）直接从顶点 Normal varying 取值，
            // 同样不需要标架。

            {
                // 纯色：材质行的颜色直出，顶点只提供位置。
                // 纹理槽表为空 —— 见 ValidateMaterialDefinitionDomain 的
                // 「本领域不接受任何纹理槽」分支。
                MaterialDomain::UnlitSolid, "UnlitSolid",
                "unlit_source", nullptr,
                Sem(VertexSemantic::Position),
                nullptr,
                0,
                false,
                false,
                {},
                // 无光照：颜色直出，Emissive 字段组足够（PBR 字段读不到也没意义）。
                GlobalSSBOType::EmissiveSurface,
            },
            {
                // 顶点色：颜色来自 Color 顶点流，逐顶点直出。
                // 这里**不接受** TransformID/Size —— 需要调色板查表或
                // 按顶点变换的形态属于 VertexPalette，那是另一份契约。
                // 领域的价值就在于这种「多一个语义就是另一个领域」的硬边界。
                MaterialDomain::VertexColor, "VertexColor",
                "vertex_color_source", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Color),
                nullptr,
                0,
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
            },
            {
                // 调色板顶点色：Color + TransformID + Size 四语义齐全。
                //   · TransformID 选调色板/变换矩阵；
                //   · Size 是该顶点的尺度参数（与变换矩阵一同参与构造）。
                // 表面来源模块与 VertexColor 相同（vertex_color_source），
                // 差别在顶点侧能力（varyings 里的 use_transform_id_attr /
                // emit_vertex_color_from_palette），领域只负责钉死前提条件。
                MaterialDomain::VertexPalette, "VertexPalette",
                "vertex_color_source", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Color)
                    | Sem(VertexSemantic::TransformID) | Sem(VertexSemantic::Size),
                nullptr,
                0,
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
            },
            {
                // 顶点亮度：逐顶点亮度索引，片元侧查调色板。无颜色顶点流。
                MaterialDomain::VertexLuminance, "VertexLuminance",
                "luminance_source", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Luminance),
                nullptr,
                0,
                false,
                false,
                {},
                // 逐顶点亮度 → 片元查调色板直出颜色：Emissive 字段组。
                GlobalSSBOType::EmissiveSurface,
            },
            {
                // 无光单图。与 UI2D 的区别：UI2D 是图集精灵（Sampler2DArray +
                // Sprite 角色，语义是界面元素），这里是普通的贴面颜色。
                MaterialDomain::UnlitTexture, "UnlitTexture",
                "texture_source", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::TexCoord),
                UnlitTextureSlots,
                static_cast<int>(sizeof(UnlitTextureSlots)
                                 / sizeof(UnlitTextureSlots[0])),
                false,
                false,
                // 贴面 2D：顶点是 NDC，直接升维后乘 L2W（无相机投影）。
                VertexShaderNodeConfig{VertexInputMode::Vec2Position,
                                       PositionMappingMode::NDCLift,
                                       OrientationMode::World,
                                       ScaleMode::World,
                                       ProjectionMode::LocalToWorldOnly},
                GlobalSSBOType::PBRSurface,
            },
            {
                // GPU 文本。顶点必须是 Position + UV0（字形在纹理里）。
                // SDF 与位图两种形态共用：它们的差别只在材质数值与采样器类型，
                // 表面来源模块是同一个 text_source_gpu。
                MaterialDomain::Text2D, "Text2D",
                "text_source_gpu", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::TexCoord),
                Text2DSlots,
                static_cast<int>(sizeof(Text2DSlots) / sizeof(Text2DSlots[0])),
                false,
                false,
                // 文字：无顶点位置输入（字形自 SSBO），像素坐标经 viewport ortho 出裁剪空间。
                VertexShaderNodeConfig{VertexInputMode::None,
                                       PositionMappingMode::PixelToLocal,
                                       OrientationMode::World,
                                       ScaleMode::World,
                                       ProjectionMode::OrthoViewport},
                GlobalSSBOType::PBRSurface,
            },

            // ══ 环境 ════════════════════════════════════════════════════════════
            {
                // 天空：由片元方向矢量直出颜色，**没有**表面来源模块。
                // no_material_source = true 表示「结构上不需要」，资产不写
                // material_source_module_name 是正确写法，写了反而是错配。
                MaterialDomain::Sky, "Sky",
                nullptr, nullptr,
                Sem(VertexSemantic::Position),
                SkySlots,
                static_cast<int>(sizeof(SkySlots) / sizeof(SkySlots[0])),
                false,
                true,
                {},
                // 天空不走材质 payload 行；类型只为满足契约完整性。
                GlobalSSBOType::PBRSurface,
            },

            // ══ 引擎内部用途 ════════════════════════════════════════════════════
            {
                // 调试法线可视化：顶点法线直接映射到颜色，唯一需要 Normal 的
                // 无光领域。
                MaterialDomain::DebugNormal, "DebugNormal",
                "debug_normal_source", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal),
                nullptr,
                0,
                false,
                false,
                {},
                // 法线→颜色直出：Emissive 字段组（与既有资产一致）。
                GlobalSSBOType::EmissiveSurface,
            },
            {
                // 2D 棋盘格（屏幕/局部空间出格）。**不要求 UV0**：
                // 现有材质用 Vec2 位置直接出格，多要一个语义就会误杀它。
                MaterialDomain::Checker2D, "Checker2D",
                "checkerboard_2d_source", nullptr,
                Sem(VertexSemantic::Position),
                nullptr,
                0,
                false,
                false,
                // 屏幕/局部空间出格：与 UnlitTexture 同为 2D NDC 顶点通路。
                VertexShaderNodeConfig{VertexInputMode::Vec2Position,
                                       PositionMappingMode::NDCLift,
                                       OrientationMode::World,
                                       ScaleMode::World,
                                       ProjectionMode::LocalToWorldOnly},
                GlobalSSBOType::PBRSurface,
            },
            {
                // 3D 棋盘格：按世界位置出格，需要世界位置 varying 而非 UV。
                MaterialDomain::Checker3D, "Checker3D",
                "checkerboard_3d_source", nullptr,
                Sem(VertexSemantic::Position),
                nullptr,
                0,
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
            },
            {
                // 材质错误回退：与 Checker3D 同源逻辑但独立模块，
                // 因为它要额外携带 data_index_id 以便定位出错的材质行。
                MaterialDomain::ErrorChecker3D, "ErrorChecker3D",
                "error_checkerboard_3d_source", nullptr,
                Sem(VertexSemantic::Position),
                nullptr,
                0,
                false,
                false,
                {},
                // 错误回退棋盘格：Emissive 字段组（与既有资产一致）。
                GlobalSSBOType::EmissiveSurface,
            },
        };

        static_assert(
            static_cast<int>(sizeof(DomainContracts) / sizeof(DomainContracts[0]))
                == MaterialDomainContractCount,
            "MaterialDomainContractCount 必须与契约表条目数一致");

        // material_data_type 必须显式填成「材质表面字段组」之一。
        // 漏填时聚合初始化会把枚举置 0 = MeshDrawParams——那是个合法枚举值，
        // 门禁的 IsGlobalSSBOType 检查抓不住它，只会在运行期按错误的行结构读数据。
        // 所以这条判据必须落在编译期。
        constexpr bool AreAllMaterialDataTypesSurface() noexcept
        {
            for (const MaterialDomainContract &contract : DomainContracts)
            {
                switch (contract.material_data_type)
                {
                case GlobalSSBOType::PBRSurface:
                case GlobalSSBOType::EmissiveSurface:
                case GlobalSSBOType::TransmissionSurface:
                    break;
                default:
                    return false;
                }
            }
            return true;
        }

        static_assert(AreAllMaterialDataTypesSurface(),
                      "每个领域契约都必须显式填写 material_data_type"
                      "（PBRSurface / EmissiveSurface / TransmissionSurface）");
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

    const char *GetMaterialDomainReferenceMaterialSource(
        const MaterialDomain domain) noexcept
    {
        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        return contract ? contract->reference_material_source_module : nullptr;
    }

    const char *GetMaterialDomainReferenceNtb(
        const MaterialDomain domain) noexcept
    {
        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        return contract ? contract->reference_ntb_module : nullptr;
    }

    bool GetMaterialDomainCanonicalVertexNodeConfig(
        const MaterialDomain domain,
        VertexShaderNodeConfig &out) noexcept
    {
        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        if (!contract)
            return false;

        out = contract->canonical_vertex_node_config;
        return true;
    }

    bool GetMaterialDomainVertexSemanticRequirements(
        const MaterialDomain domain,
        ValueArray<ShaderCodeModuleSemanticRequirement> &out) noexcept
    {
        out.Clear();

        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        if (!contract)
            return false;

        for (uint32 i = 1; i <= static_cast<uint32>(VertexSemantic::TransformID); ++i)
        {
            const VertexSemantic semantic = static_cast<VertexSemantic>(i);
            if ((contract->required_vertex_semantic_mask
                 & MaterialDomainSemanticBit(semantic)) == 0)
                continue;

            // 契约位里存在、但模块语义表没有对应项的（AO / Rotation）不能变成
            // 一条需求——那会生成一条语义为 Unknown 的垃圾需求。当前无领域使用。
            if (GetShaderCodeModuleSemanticFromVertexSemantic(semantic)
                == ShaderCodeModuleSemantic::Unknown)
                continue;

            out.Add(MakeMaterialVertexSemanticRequirement(semantic));
        }
        return true;
    }

    const MaterialDomainTextureSlot *FindMaterialDomainTextureSlot(
        const MaterialDomain domain, const char *slot_name) noexcept
    {
        if (!slot_name || !*slot_name)
            return nullptr;

        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        if (!contract)
            return nullptr;

        for (int i = 0; i < contract->texture_slot_count; ++i)
        {
            const MaterialDomainTextureSlot &slot = contract->texture_slots[i];
            if (slot.name && strcmp(slot.name, slot_name) == 0)
                return &slot;
        }
        return nullptr;
    }

    bool GetMaterialDomainCanonicalMaterialDataType(
        const MaterialDomain domain, GlobalSSBOType &out) noexcept
    {
        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        if (!contract)
            return false;

        out = contract->material_data_type;
        return true;
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
        case MaterialDomainValidationResult::MaterialSourceModuleMismatch:
            return "MaterialSourceModuleMismatch";
        case MaterialDomainValidationResult::NtbModuleMismatch:
            return "NtbModuleMismatch";
        case MaterialDomainValidationResult::TransformMismatch:
            return "TransformMismatch";
        case MaterialDomainValidationResult::TextureSlotMismatch:
            return "TextureSlotMismatch";
        case MaterialDomainValidationResult::MaterialDataTypeMismatch:
            return "MaterialDataTypeMismatch";
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
        // 契约给出规范模块名时，**资产不必重复声明**（阶段四：资产只填领域 + 值 + 图）。
        //   · 未声明  → 通过，解析层会用契约里的规范名兜底填入；
        //   · 声明了但与规范不符 → 失败。领域一旦选定，算法就由领域决定，
        //     资产不得自选——否则「同领域可共享 layout/合批」立刻失效。
        // 契约尚未实现（reference 为 nullptr）时才退回「必须自行声明」。
        const char *reference_source = contract->reference_material_source_module;
        const bool has_declared_source =
            definition.material_source_module_name && *definition.material_source_module_name;

        if (reference_source)
        {
            if (has_declared_source
                && strcmp(definition.material_source_module_name, reference_source) != 0)
            {
                out_detail = "[fragment] material_source_module_name 不得自选（领域 ";
                out_detail += contract->name;
                out_detail += " 规范模块为 ";
                out_detail += reference_source;
                out_detail += "）";
                return MaterialDomainValidationResult::MaterialSourceModuleMismatch;
            }
        }
        else if (contract->no_material_source)
        {
            // 领域结构上没有表面来源模块（如 Sky 由方向矢量直出颜色）。
            // 此时「不写」才是正确写法；写了任何模块名都属于错配。
            if (has_declared_source)
            {
                out_detail = "[fragment] material_source_module_name 必须删除（领域 ";
                out_detail += contract->name;
                out_detail += " 不使用表面来源模块，表面数据由专用通路直接产出）";
                return MaterialDomainValidationResult::MaterialSourceModuleMismatch;
            }
        }
        else if (!has_declared_source)
        {
            out_detail = "[fragment] material_source_module_name 必须声明（领域 ";
            out_detail += contract->name;
            out_detail += " 尚无规范模块，且需要 EvalMaterialSource 提供表面数据）";
            return MaterialDomainValidationResult::MissingMaterialSource;
        }

        // 法线标架模块同理：契约给出规范名时资产可省略，但不得换成别的。
        const char *reference_ntb = contract->reference_ntb_module;
        const bool has_declared_ntb =
            definition.ntb_module_name && *definition.ntb_module_name;

        if (reference_ntb && has_declared_ntb
            && strcmp(definition.ntb_module_name, reference_ntb) != 0)
        {
            out_detail = "[fragment] ntb_module_name 不得自选（领域 ";
            out_detail += contract->name;
            out_detail += " 规范模块为 ";
            out_detail += reference_ntb;
            out_detail += "；不需要法线贴图时请删除该键而非改指其它模块）";
            return MaterialDomainValidationResult::NtbModuleMismatch;
        }

        // ── 3c. 顶点变换 ──────────────────────────────────────────────────────
        // `[transform]` 同样由领域决定：三种顶点通路（默认 3D / 2D NDC / 文字 Ortho）
        // 与领域一一对应，资产重复声明只会制造「同领域不同通路」的不一致。
        // 解析层在缺省时已用契约兜底，所以这里只可能捕捉到「显式写错」。
        if (!IsSameVertexNodeConfig(definition.vertex_node_config,
                                    contract->canonical_vertex_node_config))
        {
            const VertexShaderNodeConfig &canonical =
                contract->canonical_vertex_node_config;
            out_detail = "[transform] 不得自选（领域 ";
            out_detail += contract->name;
            out_detail += " 规范为 source=";
            out_detail += GetVertexInputModeName(canonical.input);
            out_detail += " / mapping=";
            out_detail += GetPositionMappingModeName(canonical.position_mapping);
            out_detail += " / orientation=";
            out_detail += GetOrientationModeName(canonical.orientation);
            out_detail += " / scale=";
            out_detail += GetScaleModeName(canonical.scale);
            out_detail += " / projection=";
            out_detail += GetProjectionModeName(canonical.projection);
            out_detail += "；建议直接删除 [transform] 表）";
            return MaterialDomainValidationResult::TransformMismatch;
        }

        // ── 4. 纹理槽白名单 + 必声明项 + 槽形态 ────────────────────────────────
        for (const MaterialTextureDeclaration &declaration : definition.texture_declarations)
        {
            const MaterialDomainTextureSlot *slot = nullptr;
            for (int i = 0; i < contract->texture_slot_count; ++i)
            {
                if (contract->texture_slots[i].name
                    && declaration.name == contract->texture_slots[i].name)
                {
                    slot = &contract->texture_slots[i];
                    break;
                }
            }
            if (!slot)
            {
                out_detail = "[resources] textures 声明了领域 ";
                out_detail += contract->name;
                out_detail += " 白名单之外的槽 \"";
                out_detail += declaration.name.c_str();
                if (contract->texture_slot_count <= 0)
                {
                    // 无纹理领域：把「本领域一个槽都不接受」说清楚，
                    // 否则作者会以为只是名字写错了。
                    out_detail += "\"（本领域不接受任何纹理槽，请删除该声明）";
                }
                else
                {
                    out_detail += "\"（允许：";
                    for (int i = 0; i < contract->texture_slot_count; ++i)
                    {
                        if (i)
                            out_detail += ", ";
                        out_detail += contract->texture_slots[i].name;
                    }
                    out_detail += "）";
                }
                return MaterialDomainValidationResult::UnexpectedTextureSlot;
            }

            // ── 4b. 槽形态（采样器种类 / 绑定必要性 / 通道数）─────────────────
            // 这三项由领域决定，资产不必写（解析层已按契约兜底）；显式写了不同值
            // 就是「同领域不同布局」的源头，必须挡在校验期而不是留到运行期。
            if (declaration.sampler_type != slot->sampler)
            {
                out_detail = "[resources] textures 槽 \"";
                out_detail += declaration.name.c_str();
                out_detail += "\" 的 sampler 不得自选（领域 ";
                out_detail += contract->name;
                out_detail += " 规范为 ";
                out_detail += GetSamplerTypeName(slot->sampler);
                out_detail += "，实际 ";
                out_detail += GetSamplerTypeName(declaration.sampler_type);
                out_detail += "；建议直接删除 sampler 键）";
                return MaterialDomainValidationResult::TextureSlotMismatch;
            }

            if (declaration.required != slot->binding_required)
            {
                out_detail = "[resources] textures 槽 \"";
                out_detail += declaration.name.c_str();
                out_detail += "\" 的 required 不得自选（领域 ";
                out_detail += contract->name;
                out_detail += slot->binding_required ? " 规范为 true" : " 规范为 false";
                out_detail += "；建议直接删除 required 键）";
                return MaterialDomainValidationResult::TextureSlotMismatch;
            }

            if (declaration.channels != slot->channels)
            {
                out_detail = "[resources] textures 槽 \"";
                out_detail += declaration.name.c_str();
                out_detail += "\" 的 channels 不得自选（领域 ";
                out_detail += contract->name;
                out_detail += " 规范为 ";
                out_detail += AnsiString::numberOf(slot->channels);
                out_detail += "，实际 ";
                out_detail += AnsiString::numberOf(declaration.channels);
                out_detail += "；建议直接删除 channels 键）";
                return MaterialDomainValidationResult::TextureSlotMismatch;
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

        // ── 5. 材质私有数据类型 ────────────────────────────────────────────────
        // 类型决定 shader 按哪个结构读 payload 行；同领域混用两种类型 = 按 A 结构
        // 读 B 结构的字节，不会报错，只会读到看似合理的垃圾值。
        // 解析层在 `[resources].material_data` 缺省时已用契约兜底，所以这里
        // 只可能捕捉到「显式写错」。
        if (definition.material_private_data != contract->material_data_type)
        {
            out_detail = "[resources].material_data.type 不得自选（领域 ";
            out_detail += contract->name;
            out_detail += " 规范为 ";
            out_detail += GetGlobalSSBOTypeName(contract->material_data_type);
            out_detail += "，实际 ";
            out_detail += GetGlobalSSBOTypeName(definition.material_private_data);
            out_detail += "；建议直接删除 [resources].material_data 表）";
            return MaterialDomainValidationResult::MaterialDataTypeMismatch;
        }

        return MaterialDomainValidationResult::OK;
    }
}// namespace hgl::graph::mtl

