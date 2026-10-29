#pragma once

#include <hgl/type/String.h>
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

        ENUM_CLASS_RANGE(Sphere, BoundingBox)
    };

    struct BuiltinSphereParams
    {
        uint32_t slices = 32;
    };

    struct BuiltinDomeParams
    {
        uint32_t slices = 32;
    };

    struct BuiltinCubeParams
    {
        uint32_t segments_x = 1;
        uint32_t segments_y = 1;
        uint32_t segments_z = 1;
        NTBType ntb = NTBType::Normal;
        bool tex_coord = true;
    };

    struct BuiltinPlaneGridParams
    {
        uint32_t grid_x = 10;
        uint32_t grid_y = 10;
        uint32_t sub_x = 1;
        uint32_t sub_y = 1;
        uint8_t lum = 128;
        uint8_t sub_lum = 64;
        bool is_3d = false;
    };

    struct BuiltinCylinderParams
    {
        float half_extend = 1.0f;
        float radius = 1.0f;
        uint32_t slices = 32;
    };

    struct BuiltinHollowCylinderParams
    {
        float half_extend = 1.0f;
        float inner_radius = 0.5f;
        float outer_radius = 1.0f;
        uint32_t slices = 32;
        float cap_radial_tiles = 1.0f;
        float cap_angular_tiles = 1.0f;
    };

    struct BuiltinTorusParams
    {
        float inner_radius = 0.5f;
        float outer_radius = 1.0f;
        uint32_t slices = 32;
        uint32_t stacks = 16;
        float uv_scale_x = 1.0f;
        float uv_scale_y = 1.0f;
    };

    struct BuiltinConeParams
    {
        float half_extend = 1.0f;
        float radius = 1.0f;
        uint32_t slices = 32;
        uint32_t stacks = 4;
    };

    struct BuiltinCapsuleParams
    {
        float half_height = 1.0f;
        float radius = 0.5f;
        uint32_t slices = 16;
        uint32_t stacks = 8;
    };

    struct BuiltinRectangleParams
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

    struct BuiltinBoundingBoxParams
    {
        NTBType ntb = NTBType::None;
    };

    struct GeometryAssetSpec
    {
        AnsiString name;
        GeometryAssetKind kind = GeometryAssetKind::Builtin;

        // Builtin
        BuiltinShapeKind shape = BuiltinShapeKind::Sphere;
        struct Params
        {
            BuiltinSphereParams sphere{};
            BuiltinDomeParams dome{};
            BuiltinCubeParams cube{};
            BuiltinPlaneGridParams plane_grid{};
            BuiltinCylinderParams cylinder{};
            BuiltinHollowCylinderParams hollow_cylinder{};
            BuiltinTorusParams torus{};
            BuiltinConeParams cone{};
            BuiltinCapsuleParams capsule{};
            BuiltinRectangleParams rectangle{};
            BuiltinBoundingBoxParams bounding_box{};
        } params;

        // File
        OSString file_path;

        // Format overrides (optional)
        VkFormat normal_format = VF_V2UN8;

        GeometryAssetSpec() = default;
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
    bool ParseNTBType(const char *name, NTBType &out_ntb);

}//namespace hgl::graph
