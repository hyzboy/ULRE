#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/common/PositionProvider.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/mtl/LightingModel.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/SurfaceType.h>
#include <hgl/shadergen/ColorSource.h>
#include <vector>

namespace hgl::graph::mtl
{

    enum class VertexTransformPolicy : uint8
    {
        Unknown = 0,
        Mesh3D,
        Quad2D,
        BillboardCameraFacing,
        BillboardAxisLocked,
        TerrainGrid,
        Sky,
        Text2D,
        FullscreenTriangle,
        Position2DTransform,    ///< @deprecated — kept for snapshot compat; use Position2DNdc/ZeroToOne/Ortho
        Position2DNdc,          ///< 2D passthrough: input coords are already in NDC [-1,1]
        Position2DZeroToOne,    ///< 2D linear remap: input coords in [0,1] -> NDC
        Position2DOrtho,        ///< 2D pixel-space ortho: uses viewport UBO ortho_matrix
    };

    enum class SurfaceShadingModel : uint8
    {
        Unknown = 0,
        PureColor,
        VertexColor,
        VertexLuminance,
        Texture2D,
        Text,
        UnlitTexture3D,
        Gizmo,
        TerrainGrid,
        SkyMinimal,
        StandardLambert,
        StandardBlinnPhong,
        StandardPBR,
        PBRColor,
        CheckerboardFallback,
    };

    enum class StaticMaterialDefIdHint : uint8
    {
        None = 0,
        PureColor3D,
        Gizmo3D,
        Standard2D,
        Standard3D,
        TerrainGrid,
        SkyMinimal,
        VertexColor3D,
        VertexPaletteColor3D,
        VertexLuminance3D,
        UnlitTexture3D,
        Text2D,
        FullscreenTriangle,
    };

    struct ShaderStageFeatureDesc
    {
        bool vertex_attribs[static_cast<size_t>(VertexAttrib::RANGE_SIZE)] = {};
        bool has_direction = false;
        bool has_clip_pos = false;

        bool HasVertexAttrib(const VertexAttrib attrib) const noexcept
        {
            return vertex_attribs[static_cast<size_t>(attrib)];
        }

        void SetVertexAttrib(const VertexAttrib attrib, const bool enabled = true) noexcept
        {
            vertex_attribs[static_cast<size_t>(attrib)] = enabled;
        }
    };

    /// 材质变体的资源需求声明。
    ///
    /// **语义**：此结构是"显式 override 层"而非"唯一真相来源"。
    ///   - SFM（Shader Fragment Manifest）通过解析 @sfm:require 注解自动推导
    ///     needs_viewport / needs_camera / needs_transform / needs_sky /
    ///     needs_color_palette 等资源绑定字段；
    ///   - 此结构中显式设置的字段具有最高优先级，会覆盖 SFM 的推导结果；
    ///   - 对于绝大多数 builtin row，保持默认值即可，SFM 会自动填补缺失的绑定；
    ///   - enable_lighting 是 preset 语义决策（"此材质是否参与光照计算"），
    ///     不受 SFM 覆盖，由 preset 层或 ECS 系统写入；
    ///   - lighting_model / sky_ambient_model 是排列轴，由 ECS LOD 系统通过
    ///     MaterialVariantKey 注入，不在此结构中存储。
    struct MaterialResourceRequirements
    {
        bool needs_viewport = false;
        bool needs_camera = false;
        bool needs_transform = false;
        bool needs_material_instance = false;
        bool needs_material_texture_index = false;

        bool needs_sky      = false;  ///< Row requires SkyInfo UBO (SFM-driven; set by VariantRegistry autofill).
        bool enable_lighting = false;
    };

    struct MaterialVariantRow
    {
        const char *name = "";
        MaterialPreset preset = MaterialPreset::PureColor;
        MaterialPreset factory_type = MaterialPreset::PureColor;

        PrimitiveType primitive = PrimitiveType::Triangles;
        SurfaceType surface_type = SurfaceType::Unlit;
        GeometryMode geometry_mode = GeometryMode::Mesh3D;
        PositionProviderId position_provider = PositionProviderId::VAB_Vec3;

        VertexTransformPolicy vertex_policy = VertexTransformPolicy::Unknown;
        SurfaceShadingModel surface_model = SurfaceShadingModel::Unknown;

        RenderAlphaMode blend = RenderAlphaMode::Opaque;
        PassType pass = PassType::ForwardOpaque;

        const char *vs_template_path = "";
        const char *fs_template_path = "";
        const char *surface_path = "";

        ShaderStageFeatureDesc vs_features{};
        ShaderStageFeatureDesc fs_features{};
        MaterialResourceRequirements resources{};

        /// Explicit ColorSource list — the shader routing source of truth.
        std::vector<graph::ColorSource> color_sources;

        ShaderDataSchema schema = ShaderDataSchema::None;
        StaticMaterialDefIdHint def_hint = StaticMaterialDefIdHint::None;

        // Phase 3: identity-axis flags.
        // When false (default), that dimension is a resource-policy attribute and
        // does NOT participate in row-hash matching or registry lookup.
        // Set to true only when distinct shader templates exist for each value.
        bool sky_is_routing_axis = false;
    };

    inline const char *GetVertexTransformPolicyName(const VertexTransformPolicy policy) noexcept
    {
        switch (policy)
        {
        case VertexTransformPolicy::Unknown: return "Unknown";
        case VertexTransformPolicy::Mesh3D: return "Mesh3D";
        case VertexTransformPolicy::Quad2D: return "Quad2D";
        case VertexTransformPolicy::BillboardCameraFacing: return "BillboardCameraFacing";
        case VertexTransformPolicy::BillboardAxisLocked: return "BillboardAxisLocked";
        case VertexTransformPolicy::TerrainGrid: return "TerrainGrid";
        case VertexTransformPolicy::Sky: return "Sky";
        case VertexTransformPolicy::Text2D: return "Text2D";
        case VertexTransformPolicy::FullscreenTriangle: return "FullscreenTriangle";
        case VertexTransformPolicy::Position2DTransform: return "Position2DTransform";
        case VertexTransformPolicy::Position2DNdc:       return "Position2DNdc";
        case VertexTransformPolicy::Position2DZeroToOne: return "Position2DZeroToOne";
        case VertexTransformPolicy::Position2DOrtho:     return "Position2DOrtho";
        default: return "Unknown";
        }
    }

    inline const char *GetSurfaceShadingModelName(const SurfaceShadingModel model) noexcept
    {
        switch (model)
        {
        case SurfaceShadingModel::Unknown: return "Unknown";
        case SurfaceShadingModel::PureColor: return "PureColor";
        case SurfaceShadingModel::VertexColor: return "VertexColor";
        case SurfaceShadingModel::VertexLuminance: return "VertexLuminance";
        case SurfaceShadingModel::Texture2D: return "Texture2D";
        case SurfaceShadingModel::Text: return "Text";
        case SurfaceShadingModel::UnlitTexture3D: return "UnlitTexture3D";
        case SurfaceShadingModel::Gizmo: return "Gizmo";
        case SurfaceShadingModel::TerrainGrid: return "TerrainGrid";
        case SurfaceShadingModel::SkyMinimal: return "SkyMinimal";
        case SurfaceShadingModel::StandardLambert: return "StandardLambert";
        case SurfaceShadingModel::StandardBlinnPhong: return "StandardBlinnPhong";
        case SurfaceShadingModel::StandardPBR: return "StandardPBR";
        case SurfaceShadingModel::PBRColor: return "PBRColor";
        case SurfaceShadingModel::CheckerboardFallback: return "CheckerboardFallback";
        default: return "Unknown";
        }
    }

    inline const char *GetStaticMaterialDefIdHintName(const StaticMaterialDefIdHint hint) noexcept
    {
        switch (hint)
        {
        case StaticMaterialDefIdHint::None: return "None";
        case StaticMaterialDefIdHint::PureColor3D: return "PureColor3D";
        case StaticMaterialDefIdHint::Gizmo3D: return "Gizmo3D";
        case StaticMaterialDefIdHint::Standard2D: return "Standard2D";
        case StaticMaterialDefIdHint::Standard3D: return "Standard3D";
        case StaticMaterialDefIdHint::TerrainGrid: return "TerrainGrid";
        case StaticMaterialDefIdHint::SkyMinimal: return "SkyMinimal";
        case StaticMaterialDefIdHint::VertexColor3D: return "VertexColor3D";
        case StaticMaterialDefIdHint::VertexPaletteColor3D: return "VertexPaletteColor3D";
        case StaticMaterialDefIdHint::VertexLuminance3D: return "VertexLuminance3D";
        case StaticMaterialDefIdHint::UnlitTexture3D: return "UnlitTexture3D";
        case StaticMaterialDefIdHint::Text2D: return "Text2D";
        case StaticMaterialDefIdHint::FullscreenTriangle: return "FullscreenTriangle";
        default: return "None";
        }
    }
}
