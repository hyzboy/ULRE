#pragma once

#include <hgl/type/String.h>
#include <hgl/asset/AssetAnnotation.h>
#include <hgl/graph/geo/InlineGeometry.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph
{
    class VulkanDevice;
    class BufferManager;
    class Geometry;

    enum class GeometryAssetKind : uint8_t
    {
        Builtin = 0,
        File,
    };

    enum class BuiltinShapeKind : uint8_t
    {
        Sphere = 0,
        Dome,
        Cube,
        PlaneGrid,
        PlaneSquare,
        Cylinder,
        HollowCylinder,
        Torus,
        Cone,
        Capsule,
        Rectangle,
        BoundingBox,
        HexSphere,
        TaperedCapsule,
        Frustum,
        Axis,

        ENUM_CLASS_RANGE(Sphere, Axis)
    };

    /**
     * @brief 顶点法线格式（几何资产 `[vertex_format] normal` 的契约枚举）
     *
     * 拼写表唯一真源 = 下面的 `VertexNormalFormatTable`：一行同时给出 C++ 枚举、
     * TOML 短名、引擎宏名、Vulkan 原名与落地的 `VkFormat`。名字 / 解析 / 格式转换
     * 三个方向**全部**由这一张表派生，不得再写第二份。
     */
    enum class VertexNormalFormat : uint8_t
    {
        V2UN8 = 0,  ///<VF_V2UN8（缺省，压缩法线）
        V2HF,       ///<VF_V2HF
        V3F,        ///<VF_V3F
        V3SN16,     ///<VF_V3SN16

        ENUM_CLASS_RANGE(V2UN8, V3SN16)
    };

    struct VertexNormalFormatDesc
    {
        VertexNormalFormat format;          ///<枚举值
        const char        *short_name;      ///<TOML 写的短名
        const char        *macro_name;      ///<引擎宏名（历史写法，兼容读入）
        const char        *vk_format_name;  ///<Vulkan 原名（历史写法，兼容读入）
        VkFormat           vk_format;       ///<顶点布局实际使用的格式
    };

    inline constexpr VertexNormalFormatDesc VertexNormalFormatTable[] =
    {
        { VertexNormalFormat::V2UN8,  "v2un8",  "VF_V2UN8",  "r8g8_unorm",       VF_V2UN8  },
        { VertexNormalFormat::V2HF,   "v2hf",   "VF_V2HF",   "r16g16_sfloat",    VF_V2HF   },
        { VertexNormalFormat::V3F,    "v3f",    "VF_V3F",    "r32g32b32_sfloat", VF_V3F    },
        { VertexNormalFormat::V3SN16, "v3sn16", "VF_V3SN16", "r16g16b16_snorm",  VF_V3SN16 },
    };

    inline VkFormat ToVkFormat(VertexNormalFormat format)
    {
        for (const VertexNormalFormatDesc &desc : VertexNormalFormatTable)
            if (desc.format == format)
                return desc.vk_format;

        return VF_V2UN8;
    }

    inline const char *GetVertexNormalFormatName(VertexNormalFormat format)
    {
        for (const VertexNormalFormatDesc &desc : VertexNormalFormatTable)
            if (desc.format == format)
                return desc.short_name;

        return "unknown";
    }

    inline bool ParseVertexNormalFormat(const char *text, VertexNormalFormat &out_format)
    {
        if (!text || !*text)
            return false;

        for (const VertexNormalFormatDesc &desc : VertexNormalFormatTable)
            if (hgl::stricmp(text, desc.short_name) == 0
             || hgl::stricmp(text, desc.macro_name) == 0
             || hgl::stricmp(text, desc.vk_format_name) == 0)
            {
                out_format = desc.format;
                return true;
            }

        return false;
    }

    struct HGL_ASSET_STRUCT BuiltinSphereParams
    {
        uint32_t slices = 32;
    };

    struct HGL_ASSET_STRUCT BuiltinDomeParams
    {
        uint32_t slices = 32;
    };

    struct HGL_ASSET_STRUCT BuiltinCubeParams
    {
        uint32_t segments_x = 1;
        uint32_t segments_y = 1;
        uint32_t segments_z = 1;
        NTBType ntb = NTBType::Normal;
        bool tex_coord = true;
    };

    struct HGL_ASSET_STRUCT BuiltinPlaneGridParams
    {
        uint32_t grid_x = 10;
        uint32_t grid_y = 10;
        uint32_t sub_x = 1;
        uint32_t sub_y = 1;
        uint8_t lum = 128;
        uint8_t sub_lum = 64;
        bool is_3d = false;
    };

    struct HGL_ASSET_STRUCT BuiltinCylinderParams
    {
        float half_extend = 1.0f;
        float radius = 1.0f;
        uint32_t slices = 32;
    };

    struct HGL_ASSET_STRUCT BuiltinHollowCylinderParams
    {
        float half_extend = 1.0f;
        float inner_radius = 0.5f;
        float outer_radius = 1.0f;
        uint32_t slices = 32;
        float cap_radial_tiles = 1.0f;
        float cap_angular_tiles = 1.0f;
    };

    struct HGL_ASSET_STRUCT BuiltinTorusParams
    {
        float inner_radius = 0.5f;
        float outer_radius = 1.0f;
        uint32_t slices = 32;
        uint32_t stacks = 16;
        float uv_scale_x = 1.0f;
        float uv_scale_y = 1.0f;
    };

    struct HGL_ASSET_STRUCT BuiltinConeParams
    {
        float half_extend = 1.0f;
        float radius = 1.0f;
        uint32_t slices = 32;
        uint32_t stacks = 4;
    };

    struct HGL_ASSET_STRUCT BuiltinCapsuleParams
    {
        float half_height = 1.0f;
        float radius = 0.5f;
        uint32_t slices = 16;
        uint32_t stacks = 8;
    };

    struct HGL_ASSET_STRUCT BuiltinRectangleParams
    {
        float left = -1.0f;
        float top = -1.0f;
        float width = 2.0f;
        float height = 2.0f;
        uint32_t segments_x = 1;
        uint32_t segments_y = 1;
        NTBType ntb = NTBType::None;
        bool tex_coord = true;
    };

    struct HGL_ASSET_STRUCT BuiltinAxisParams
    {
        float size = 1.0f;
    };

    struct HGL_ASSET_STRUCT BuiltinBoundingBoxParams
    {
        NTBType ntb = NTBType::None;
    };

    struct HGL_ASSET_STRUCT BuiltinHexSphereParams
    {
        uint32_t subdivisions = 0;
        float    radius = 1.0f;
        float    uv_scale_x = 1.0f;
        float    uv_scale_y = 1.0f;
    };

    struct HGL_ASSET_STRUCT BuiltinTaperedCapsuleParams
    {
        float    half_height = 1.0f;
        float    bottom_radius = 0.5f;
        float    top_radius = 0.5f;
        uint32_t slices = 16;
        uint32_t stacks = 8;
    };

    struct HGL_ASSET_STRUCT BuiltinFrustumParams
    {
        float    bottom_radius = 1.0f;
        float    top_radius = 0.5f;
        float    height = 2.0f;
        uint32_t slices = 16;
        bool     generate_caps = true;
    };

    /// `[vertex_format]` 表：顶点法线格式覆盖（可选，整表缺省即用类内缺省值）。
    struct HGL_ASSET_STRUCT GeometryVertexFormatSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Format",
                     Tooltip = "法线顶点格式；接受短名 v2un8 / VF_ 宏名 / Vulkan 原名")
        VertexNormalFormat normal = VertexNormalFormat::V2UN8;
    };

    /**
     * `[builtin.<shape>]` 各形状参数子表。
     *
     * `[builtin.sphere]` 这类**表名由 `shape` 字段的值决定**的写法，静态反射无法
     * 表达；这里把 14 张子表展开成 14 个固定键（键名 = TOML 表名），由
     * `ReadFromValue` 一次读全 —— 出现几张贴就读几张，没写的表全是类内缺省值，
     * 消费侧按 `shape` 直接取对应成员即可（不需要「先挑表再读」）。
     */
    struct HGL_ASSET_STRUCT GeometryBuiltinParamsSpec
    {
        /// `[builtin.sphere]`
        BuiltinSphereParams         sphere;
        /// `[builtin.dome]`
        BuiltinDomeParams           dome;
        /// `[builtin.cube]`
        BuiltinCubeParams           cube;
        /// `[builtin.plane_grid]`
        BuiltinPlaneGridParams      plane_grid;
        /// `[builtin.cylinder]`
        BuiltinCylinderParams       cylinder;
        /// `[builtin.hollow_cylinder]`
        BuiltinHollowCylinderParams hollow_cylinder;
        /// `[builtin.torus]`
        BuiltinTorusParams          torus;
        /// `[builtin.cone]`
        BuiltinConeParams           cone;
        /// `[builtin.capsule]`
        BuiltinCapsuleParams        capsule;
        /// `[builtin.rectangle]`
        BuiltinRectangleParams      rectangle;
        /// `[builtin.axis]`
        BuiltinAxisParams           axis;
        /// `[builtin.bounding_box]`
        BuiltinBoundingBoxParams    bounding_box;
        /// `[builtin.hex_sphere]`
        BuiltinHexSphereParams      hex_sphere;
        /// `[builtin.tapered_capsule]`
        BuiltinTaperedCapsuleParams tapered_capsule;
        /// `[builtin.frustum]`
        BuiltinFrustumParams        frustum;
    };

    /**
     * @brief 几何资产规约（`.geo.toml` / `.geo` 的唯一真源）
     *
     * TOML 形状：
     * ```toml
     * schema = 1
     * name   = "unit_sphere"
     * type   = "builtin"    # 或 "file"
     * shape  = "sphere"     # 仅 builtin
     * file   = "x.geometry" # 仅 file
     *
     * [builtin.sphere]
     * slices = 64
     *
     * [vertex_format]
     * normal = "VF_V2UN8"
     * ```
     * 字段顺序即 TOML 键序；`kind` / `params` / `file_path` 的键名由 `Key` 覆盖。
     */
    struct HGL_ASSET_STRUCT GeometryAssetSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Identity", Range(1, 1))
        uint32_t schema = 0;                ///<格式版本，当前只接受 1

        HGL_PROPERTY(EditAnywhere, Category = "Identity")
        AnsiString name;                    ///<资产名（缺省取文件名主干）

        HGL_PROPERTY(EditAnywhere, Category = "Source", Key = "type")
        GeometryAssetKind kind = GeometryAssetKind::Builtin;    ///<builtin 参数化 / file 读盘

        HGL_PROPERTY(EditAnywhere, Category = "Source")
        BuiltinShapeKind shape = BuiltinShapeKind::Sphere;      ///<仅 builtin：内置形状

        HGL_PROPERTY(EditAnywhere, Category = "Source", Key = "builtin")
        GeometryBuiltinParamsSpec params;   ///<仅 builtin：形状参数子表

        HGL_PROPERTY(EditAnywhere, Category = "Source", AssetRef = "geo", Key = "file")
        OSString file_path;                 ///<仅 file：几何资产路径

        HGL_PROPERTY(EditAnywhere, Category = "Format")
        GeometryVertexFormatSpec vertex_format;                 ///<顶点格式覆盖（可选）

        GeometryAssetSpec() = default;

        /// 顶点布局实际使用的法线格式（`vertex_format.normal` 的派生化结果）。
        VkFormat NormalFormat() const { return ToVkFormat(vertex_format.normal); }
    };

    /**
     * @brief 根据资产规约创建 Geometry 对象
     */
    Geometry *CreateGeometryFromSpec(VulkanDevice *device,
                                    BufferManager *bm,
                                    const GeometryAssetSpec &spec,
                                    const GeometryVertexFormat *gvf_override = nullptr);

    const char *GetBuiltinShapeKindName(BuiltinShapeKind kind);
    bool ParseBuiltinShapeKind(const char *name, BuiltinShapeKind &out_kind);
    const char *GetGeometryAssetKindName(GeometryAssetKind kind);
    bool ParseGeometryAssetKind(const char *name, GeometryAssetKind &out_kind);
    bool ParseNTBType(const char *name, NTBType &out_ntb);

}//namespace hgl::graph
