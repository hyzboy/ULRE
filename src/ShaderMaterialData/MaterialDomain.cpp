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
        case MaterialDomain::UnlitTexture3D:  return "UnlitTexture3D";
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

        // ── 领域规范采样器预设表 ────────────────────────────────────────────────
        // 名字必须在 ShaderLibrary/contract/sampler.toml 里存在，否则构建侧会
        // 静默取索引 0（门禁 S.material-domain-sampler-presets-exist 会核对）。
        constexpr const char *PbrSurfaceSamplers[] = {
            "Trilinear", "Linear", "ShadowPCF"};
        constexpr const char *TextureSurfaceSamplers[] = {
            "Trilinear", "TrilinearClamp"};
        // SDF 文本默认线性过滤；位图形态由资产声明 samplers=["Nearest"] 覆盖。
        constexpr const char *TextSurfaceSamplers[] = {"Linear"};
        // 天空槽现在可承载三种投影形态：`.TexCube`（cubemap）、`.TexOct`（八面体
        // 展开图）与普通 `.Tex2D`（等距柱状展开图）。后两者是 2D 贴图，八面体
        // 展开图必须 ClampToEdge 才不会在对极纹素边界被邻近列污染，故预设同时
        // 声明 Trilinear 与 TrilinearClamp（与 UnlitTexture 域同一张表）。
        constexpr const char *SkySamplers[] = {"Trilinear", "TrilinearClamp"};

        // ── 领域规范编译期宏表 ──────────────────────────────────────────────────
        // SDF 文本走内部距离场采样，片元模板需要该宏切换阈值逻辑。
        constexpr const char *TextSurfaceDefines[] = {"TEXT_SDF_ENABLED"};

        constexpr InterStageSemanticMask VarSem(const InterStageSemantic semantic) noexcept
        {
            return GetInterStageSemanticMask(semantic);
        }

        // 领域规范的 alpha 混合管线配置：从默认值出发只改 alpha_blend，
        // 其余（cull/depth）保持 MaterialPipelineConfig 的默认值——直接写
        // `MaterialPipelineConfig{true}` 会把 true 落到首字段 cull_mode 上。
        constexpr MaterialPipelineConfig MakeAlphaBlendPipeline() noexcept
        {
            MaterialPipelineConfig config{};
            config.alpha_blend = true;
            return config;
        }

        constexpr MaterialDomainContract DomainContracts[] = {
            {
                MaterialDomain::StandardOpaque, "StandardOpaque",
                "material_source_pbr_surface", "ntb_tangent_vbo_normalmap",
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
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::WorldPosition)
                    | VarSem(InterStageSemantic::WorldNormal)
                    | VarSem(InterStageSemantic::UV0),
                PbrSurfaceSamplers,
                static_cast<int>(sizeof(PbrSurfaceSamplers)
                                 / sizeof(PbrSurfaceSamplers[0])),
            },
            {
                MaterialDomain::Foliage, "Foliage",
                // 尚未实现：植被需要透射/双面法线/风力顶点位移，不能直接复用
                // material_source_pbr_surface。Phase 3/4 落地 foliage_surface_source 时补齐。
                nullptr, nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal)
                    | Sem(VertexSemantic::TexCoord),
                FoliageSlots,
                static_cast<int>(sizeof(FoliageSlots) / sizeof(FoliageSlots[0])),
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::WorldPosition)
                    | VarSem(InterStageSemantic::WorldNormal)
                    | VarSem(InterStageSemantic::UV0),
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
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::WorldPosition)
                    | VarSem(InterStageSemantic::WorldNormal)
                    | VarSem(InterStageSemantic::UV0),
            },
            {
                MaterialDomain::UI2D, "UI2D",
                "material_source_texture", "ntb_orthonormal",
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::TexCoord),
                UI2DSlots,
                static_cast<int>(sizeof(UI2DSlots) / sizeof(UI2DSlots[0])),
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::UV0)
                    | VarSem(InterStageSemantic::Color),
            },
            {
                MaterialDomain::HumanSkin, "HumanSkin",
                // 尚未实现：皮肤需要 SSS/预积分曲率查表，不能直接复用
                // material_source_pbr_surface。Phase 3/4 落地 skin_surface_source 时补齐。
                nullptr, nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal)
                    | Sem(VertexSemantic::TexCoord),
                HumanSkinSlots,
                static_cast<int>(sizeof(HumanSkinSlots) / sizeof(HumanSkinSlots[0])),
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::WorldPosition)
                    | VarSem(InterStageSemantic::WorldNormal)
                    | VarSem(InterStageSemantic::UV0),
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
                // 纹理槽表为空 —— 本领域不接受任何纹理槽。
                MaterialDomain::UnlitSolid, "UnlitSolid",
                "material_source_unlit", nullptr,
                Sem(VertexSemantic::Position),
                nullptr,
                0,
                false,
                false,
                {},
                // 无光照：颜色直出，Emissive 字段组足够（PBR 字段读不到也没意义）。
                GlobalSSBOType::EmissiveSurface,
                VarSem(InterStageSemantic::DataIndexID),
            },
            {
                // 顶点色：颜色来自 Color 顶点流，逐顶点直出。
                // 这里**不接受** TransformID/Size —— 需要调色板查表或
                // 按顶点变换的形态属于 VertexPalette，那是另一份契约。
                // 领域的价值就在于这种「多一个语义就是另一个领域」的硬边界。
                MaterialDomain::VertexColor, "VertexColor",
                "material_source_vertex_color", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Color),
                nullptr,
                0,
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
                VarSem(InterStageSemantic::Color),
            },
            {
                // 调色板顶点色：Color + TransformID + Size 四语义齐全。
                //   · TransformID 选调色板/变换矩阵；
                //   · Size 是该顶点的尺度参数（与变换矩阵一同参与构造）。
                // 表面来源模块与 VertexColor 相同（material_source_vertex_color），
                // 差别在顶点侧能力（varyings 里的 use_transform_id_attr /
                // emit_vertex_color_from_palette），领域只负责钉死前提条件。
                MaterialDomain::VertexPalette, "VertexPalette",
                "material_source_vertex_color", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Color)
                    | Sem(VertexSemantic::TransformID) | Sem(VertexSemantic::Size),
                nullptr,
                0,
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
                VarSem(InterStageSemantic::Color),
                nullptr,
                0,
                nullptr,
                0,
                {},
                MaterialVertexNormalMode::None,
                MeshShaderMode::VertexPassthrough,
                true,   // emit_vertex_color_from_palette：Color 流是调色板索引(R8_UINT)，不是 RGBA
                true,   // use_transform_id_attr：顶点 ABI 需要 TransformID
            },
            {
                // 顶点亮度：逐顶点亮度索引，片元侧查调色板。无颜色顶点流。
                MaterialDomain::VertexLuminance, "VertexLuminance",
                "material_source_luminance", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Luminance),
                nullptr,
                0,
                false,
                false,
                {},
                // 逐顶点亮度 → 片元查调色板直出颜色：Emissive 字段组。
                GlobalSSBOType::EmissiveSurface,
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::Luminance),
            },
            {
                // 无光单图。与 UI2D 的区别：UI2D 是图集精灵（Sampler2DArray +
                // Sprite 角色，语义是界面元素），这里是普通的贴面颜色。
                MaterialDomain::UnlitTexture, "UnlitTexture",
                "material_source_texture", nullptr,
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
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::UV0),
                TextureSurfaceSamplers,
                static_cast<int>(sizeof(TextureSurfaceSamplers)
                                 / sizeof(TextureSurfaceSamplers[0])),
            },
            {
                // 世界空间无光贴面。片元阶段与 UnlitTexture **逐字节相同**——
                // 复用同一份固定主着色器（`domain/unlit_texture/unlit_texture.frag`）
                // 与同一个来源模块（`material_source_texture`），差异只在顶点通路：
                // UnlitTexture 是 2D NDC（只配给屏幕矩形），本领域走标准 3D
                // （Position 经相机投影），因此可以直接贴到任意世界几何上。
                //
                // 存在的理由：此前「把一张图原样铺在世界物体上」没有落点——用
                // StandardOpaque（Lit）会进 PBR 光照，图像被环境光/高光改写；
                // 用 UnlitTexture 又会被当成 NDC 顶点处理。调试视图（深度/法线/
                // 中间缓冲显示）与以后的面片类需求都落在这个领域。
                MaterialDomain::UnlitTexture3D, "UnlitTexture3D",
                "material_source_texture", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::TexCoord),
                UnlitTextureSlots,
                static_cast<int>(sizeof(UnlitTextureSlots)
                                 / sizeof(UnlitTextureSlots[0])),
                false,
                false,
                // 世界几何：走默认 3D 顶点通路（与 StandardOpaque 一致）。
                {},
                GlobalSSBOType::PBRSurface,
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::UV0),
                TextureSurfaceSamplers,
                static_cast<int>(sizeof(TextureSurfaceSamplers)
                                 / sizeof(TextureSurfaceSamplers[0])),
            },
            {
                // GPU 文本。顶点必须是 Position + UV0（字形在纹理里）。
                // SDF 与位图两种形态共用：它们的差别只在材质数值与采样器类型，
                // 表面来源模块是同一个 material_source_text_gpu。
                MaterialDomain::Text2D, "Text2D",
                "material_source_text_gpu", nullptr,
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
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::UV0)
                    | VarSem(InterStageSemantic::StyleID)
                    | VarSem(InterStageSemantic::Color),
                TextSurfaceSamplers,
                static_cast<int>(sizeof(TextSurfaceSamplers)
                                 / sizeof(TextSurfaceSamplers[0])),
                TextSurfaceDefines,
                static_cast<int>(sizeof(TextSurfaceDefines)
                                 / sizeof(TextSurfaceDefines[0])),
                // 文字必须 alpha 混合：字形边缘是 alpha 覆盖，不混就是硬边黑底。
                // 位图字（text_gpu_bitmap）同样需要，故这是领域性质而非资产选择。
                ResolvedMaterialRenderState{false, 0.5f, false,
                                            MakeAlphaBlendPipeline()},
                MaterialVertexNormalMode::None,
                MeshShaderMode::CharQuad,
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
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::FragDirection),
                SkySamplers,
                static_cast<int>(sizeof(SkySamplers) / sizeof(SkySamplers[0])),
            },

            // ══ 引擎内部用途 ════════════════════════════════════════════════════
            {
                // 调试法线可视化：顶点法线直接映射到颜色，唯一需要 Normal 的
                // 无光领域。
                MaterialDomain::DebugNormal, "DebugNormal",
                "material_source_debug_normal", nullptr,
                Sem(VertexSemantic::Position) | Sem(VertexSemantic::Normal),
                nullptr,
                0,
                false,
                false,
                {},
                // 法线→颜色直出：Emissive 字段组（与既有资产一致）。
                GlobalSSBOType::EmissiveSurface,
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::WorldPosition)
                    | VarSem(InterStageSemantic::WorldNormal),
            },
            {
                // 2D 棋盘格（屏幕/局部空间出格）。**不要求 UV0**：
                // 现有材质用 Vec2 位置直接出格，多要一个语义就会误杀它。
                MaterialDomain::Checker2D, "Checker2D",
                "material_source_checkerboard_2d", nullptr,
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
                0,
            },
            {
                // 3D 棋盘格：按世界位置出格，需要世界位置 varying 而非 UV。
                MaterialDomain::Checker3D, "Checker3D",
                "material_source_checkerboard_3d", nullptr,
                Sem(VertexSemantic::Position),
                nullptr,
                0,
                false,
                false,
                {},
                GlobalSSBOType::PBRSurface,
                VarSem(InterStageSemantic::WorldPosition),
                // 采样器/宏/默认渲染状态全部走缺省（位置初始化不能跳字段）。
                nullptr, 0, nullptr, 0,
                {},
                MaterialVertexNormalMode::OptionalFaceFallback,
            },
            {
                // 材质错误回退：与 Checker3D 同源逻辑但独立模块，
                // 因为它要额外携带 data_index_id 以便定位出错的材质行。
                MaterialDomain::ErrorChecker3D, "ErrorChecker3D",
                "material_source_error_checkerboard_3d", nullptr,
                Sem(VertexSemantic::Position),
                nullptr,
                0,
                false,
                false,
                {},
                // 错误回退棋盘格：Emissive 字段组（与既有资产一致）。
                GlobalSSBOType::EmissiveSurface,
                VarSem(InterStageSemantic::DataIndexID)
                    | VarSem(InterStageSemantic::WorldPosition),
                // 采样器/宏/默认渲染状态全部走缺省（位置初始化不能跳字段）。
                nullptr, 0, nullptr, 0,
                {},
                MaterialVertexNormalMode::OptionalFaceFallback,
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
        ValueArray<GlslModuleSemanticRequirement> &out) noexcept
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
            if (GetGlslModuleSemanticFromVertexSemantic(semantic)
                == GlslModuleSemantic::Unknown)
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

    const char *const *GetMaterialDomainSamplerPresets(
        const MaterialDomain domain, int &out_count) noexcept
    {
        out_count = 0;

        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        if (!contract || contract->sampler_count <= 0 || !contract->samplers)
            return nullptr;

        out_count = contract->sampler_count;
        return contract->samplers;
    }

    const char *const *GetMaterialDomainCompileDefines(
        const MaterialDomain domain, int &out_count) noexcept
    {
        out_count = 0;

        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        if (!contract || contract->compile_define_count <= 0 || !contract->compile_defines)
            return nullptr;

        out_count = contract->compile_define_count;
        return contract->compile_defines;
    }

    InterStageSemanticMask GetMaterialDomainDefaultVaryingMask(
        const MaterialDomain domain) noexcept
    {
        const MaterialDomainContract *contract = GetMaterialDomainContract(domain);
        return contract ? contract->default_varying_semantic_mask : 0;
    }
}// namespace hgl::graph::mtl

