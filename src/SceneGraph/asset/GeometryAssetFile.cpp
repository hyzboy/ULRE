#include <hgl/graph/asset/GeometryAssetFile.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/geo/LoadGeometry.h>
#include <hgl/asset/generated/GeometrySerialize.gen.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/type/Smart.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

DEFINE_LOGGER_MODULE(GeometryAsset)
USE_MODULE_LOGGER(GeometryAsset)

// 字段遍历代码全部在 `GeometrySerialize.gen.h` 里（`hgl::asset::generated`），
// 本文件只保留枚举拼写、文本入口与跨字段策略。

namespace hgl::graph
{
    namespace
    {
        /**
         * 失败键的原始写法（如 `"hypercube"`）只对诊断有价值：值树丢掉了「作者
         * 写了什么」这一层，按失败路径回原节点把它取出来。
         */
        AnsiString OriginalText(const hgl::serialize::Value &root, const char *path)
        {
            const hgl::serialize::Value *node = root.FindByPath(path);

            if (!node || !node->IsString())
                return AnsiString();

            const U8String &text = node->GetString();
            return AnsiString(reinterpret_cast<const char *>(text.c_str()), text.Length());
        }
    }//namespace

    const char *GetGeometryAssetKindName(GeometryAssetKind kind)
    {
        switch (kind)
        {
        case GeometryAssetKind::Builtin:    return "builtin";
        case GeometryAssetKind::File:       return "file";
        default:                            return "unknown";
        }
    }

    bool ParseGeometryAssetKind(const char *name, GeometryAssetKind &out_kind)
    {
        if (!name || !*name)
            return false;

        if (hgl::stricmp(name, "builtin") == 0) { out_kind = GeometryAssetKind::Builtin; return true; }
        if (hgl::stricmp(name, "file") == 0)    { out_kind = GeometryAssetKind::File;    return true; }

        return false;
    }

    const char *GetBuiltinShapeKindName(BuiltinShapeKind kind)
    {
        switch (kind)
        {
        case BuiltinShapeKind::Sphere:          return "sphere";
        case BuiltinShapeKind::Dome:            return "dome";
        case BuiltinShapeKind::Cube:            return "cube";
        case BuiltinShapeKind::PlaneGrid:       return "plane_grid";
        case BuiltinShapeKind::PlaneSquare:     return "plane_square";
        case BuiltinShapeKind::Cylinder:        return "cylinder";
        case BuiltinShapeKind::HollowCylinder:  return "hollow_cylinder";
        case BuiltinShapeKind::Torus:           return "torus";
        case BuiltinShapeKind::Cone:            return "cone";
        case BuiltinShapeKind::Capsule:         return "capsule";
        case BuiltinShapeKind::Rectangle:       return "rectangle";
        case BuiltinShapeKind::BoundingBox:     return "bounding_box";
        case BuiltinShapeKind::HexSphere:       return "hex_sphere";
        case BuiltinShapeKind::TaperedCapsule:  return "tapered_capsule";
        case BuiltinShapeKind::Frustum:         return "frustum";
        case BuiltinShapeKind::Axis:            return "axis";
        default:                                return "unknown";
        }
    }

    bool ParseBuiltinShapeKind(const char *name, BuiltinShapeKind &out_kind)
    {
        if (!name || !*name)
            return false;

        if (hgl::stricmp(name, "sphere") == 0)          { out_kind = BuiltinShapeKind::Sphere; return true; }
        if (hgl::stricmp(name, "dome") == 0)            { out_kind = BuiltinShapeKind::Dome; return true; }
        if (hgl::stricmp(name, "cube") == 0)            { out_kind = BuiltinShapeKind::Cube; return true; }
        if (hgl::stricmp(name, "plane_grid") == 0)      { out_kind = BuiltinShapeKind::PlaneGrid; return true; }
        if (hgl::stricmp(name, "plane_square") == 0)    { out_kind = BuiltinShapeKind::PlaneSquare; return true; }
        if (hgl::stricmp(name, "cylinder") == 0)        { out_kind = BuiltinShapeKind::Cylinder; return true; }
        if (hgl::stricmp(name, "hollow_cylinder") == 0) { out_kind = BuiltinShapeKind::HollowCylinder; return true; }
        if (hgl::stricmp(name, "torus") == 0)           { out_kind = BuiltinShapeKind::Torus; return true; }
        if (hgl::stricmp(name, "cone") == 0)            { out_kind = BuiltinShapeKind::Cone; return true; }
        if (hgl::stricmp(name, "capsule") == 0)         { out_kind = BuiltinShapeKind::Capsule; return true; }
        if (hgl::stricmp(name, "rectangle") == 0)       { out_kind = BuiltinShapeKind::Rectangle; return true; }
        if (hgl::stricmp(name, "bounding_box") == 0)    { out_kind = BuiltinShapeKind::BoundingBox; return true; }
        if (hgl::stricmp(name, "hex_sphere") == 0)      { out_kind = BuiltinShapeKind::HexSphere; return true; }
        if (hgl::stricmp(name, "tapered_capsule") == 0) { out_kind = BuiltinShapeKind::TaperedCapsule; return true; }
        if (hgl::stricmp(name, "frustum") == 0)         { out_kind = BuiltinShapeKind::Frustum; return true; }
        if (hgl::stricmp(name, "axis") == 0)            { out_kind = BuiltinShapeKind::Axis; return true; }

        return false;
    }

    bool ParseNTBType(const char *name, NTBType &out_ntb)
    {
        if (!name || !*name)
            return false;

        if (hgl::stricmp(name, "none") == 0)                     { out_ntb = NTBType::None; return true; }
        if (hgl::stricmp(name, "normal") == 0)                   { out_ntb = NTBType::Normal; return true; }
        if (hgl::stricmp(name, "normal_tangent") == 0)           { out_ntb = NTBType::NormalTangent; return true; }
        if (hgl::stricmp(name, "normal_tangent_bitangent") == 0) { out_ntb = NTBType::NormalTangentBitangent; return true; }

        return false;
    }

    bool ParseGeometryAssetSpec(const char *content, size_t size, const os_char *debug_name, GeometryAssetSpec &out_spec)
    {
        // 先归零：解析器会被复用在同一个 `out_spec` 上，上一次的 `Presence` 记录
        // （哪几张 `[builtin.<shape>]` 表存在过）不能残留下来冒充本次输入。
        out_spec = GeometryAssetSpec{};

        if (!content || size == 0)
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: empty content for ") + OSString(debug_name));
            return false;
        }

        hgl::serialize::Value          root;
        hgl::serialize::TomlParseError toml_error;

        if (!hgl::serialize::FromTomlText(content, size, root, &toml_error))
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: TOML parse error in ")
                     + OSString(debug_name) + OS_TEXT(": ") + ToOSString(toml_error.Format().c_str()));
            return false;
        }

        if (!root.IsTable())
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: root is not a table in ") + OSString(debug_name));
            return false;
        }

        // `schema` 单独先看：它决定这张字段表还能不能按当前契约解释，报错也要有
        // 独立措辞。缺键时 `schema_node` 为空，与「写了别的版本」同样被挡住。
        {
            const hgl::serialize::Value *schema_node  = root.FindFieldExact("schema");
            int64_t                      schema_value = 0;

            if (!schema_node || !schema_node->GetInt(schema_value) || schema_value != 1)
            {
                MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unsupported schema in ") + OSString(debug_name));
                return false;
            }
        }

        // 逐字段解析（`type` / `builtin` / `file` 的键名覆盖、形状与顶点格式的拼写、
        // `[builtin.<shape>]` 各子表的展开）全部来自 `GeometrySerialize.gen.h` ——
        // 本文件不再认识任何键名。
        {
            hgl::serialize::Archive archive;

            if (!hgl::serialize::ReadFromValue(root, out_spec, &archive))
            {
                const AnsiString failed_path(archive.FailedPath());
                const AnsiString raw_text(OriginalText(root, failed_path.c_str()));

                if (failed_path == "type")
                {
                    MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unknown type '")
                             + ToOSString(raw_text.c_str()) + OS_TEXT("' in ") + OSString(debug_name));
                }
                else if (failed_path == "shape")
                {
                    MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unknown shape '")
                             + ToOSString(raw_text.c_str()) + OS_TEXT("' in ") + OSString(debug_name));
                }
                else if (failed_path.StartsWith("vertex_format"))
                {
                    MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unknown normal vertex format '")
                             + ToOSString(raw_text.c_str()) + OS_TEXT("' in ") + OSString(debug_name));
                }
                else if (failed_path.StartsWith("builtin"))
                {
                    MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: invalid shape parameter value/type at '")
                             + ToOSString(failed_path.c_str()) + OS_TEXT("' in ") + OSString(debug_name));
                }
                else
                {
                    MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: field '")
                             + ToOSString(failed_path.c_str()) + OS_TEXT("' is invalid (")
                             + ToOSString(archive.ErrorMessage()) + OS_TEXT(") in ") + OSString(debug_name));
                }

                return false;
            }
        }

        // 以下全是**跨字段策略**（`type` 决定哪条分支必填哪些键），不属于字段规约。
        if (out_spec.kind == GeometryAssetKind::File)
        {
            if (out_spec.file_path.IsEmpty())
            {
                MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: missing 'file' attribute for file geometry in ") + OSString(debug_name));
                return false;
            }

            return true;
        }

        // builtin：`shape` 必填。类内缺省值（球体）不能顶替作者的声明 —— 静默变成
        // 球体比报错更难查。`[builtin.<shape>]` 的那张表读没读到不影响这条判断：
        // 表不存在时该形状的参数全是类内缺省值。
        if (!root.FindFieldExact("shape"))
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: missing 'shape' for builtin geometry in ") + OSString(debug_name));
            return false;
        }

        return true;
    }

    bool LoadGeometryAssetSpecFromTOML(const OSString &filename, GeometryAssetSpec &out_spec)
    {
        hgl::io::OpenFileInputStream opener(filename);
        if (!opener)
        {
            MLogError(GeometryAsset, OS_TEXT("LoadGeometryAssetSpecFromTOML: cannot open file ") + filename);
            return false;
        }

        const int64 size = opener->GetSize();
        if (size <= 0 || size > 16 * 1024 * 1024)
        {
            MLogError(GeometryAsset, OS_TEXT("LoadGeometryAssetSpecFromTOML: invalid file size in ") + filename);
            return false;
        }

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);
        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            MLogError(GeometryAsset, OS_TEXT("LoadGeometryAssetSpecFromTOML: read failed in ") + filename);
            return false;
        }
        buffer[static_cast<size_t>(size)] = '\0';

        if (!ParseGeometryAssetSpec(buffer.data(), static_cast<size_t>(size), filename.c_str(), out_spec))
            return false;

        if (out_spec.name.IsEmpty())
        {
            const OSString stem = hgl::filesystem::GetStem(filename);
            const U8String u8_stem = ToU8String(stem);
            out_spec.name = AnsiString(reinterpret_cast<const char *>(u8_stem.c_str()), u8_stem.Length());
        }

        return true;
    }

    Geometry *CreateGeometryFromSpec(GeometryCreater *creater,
                                    const GeometryAssetSpec &spec)
    {
        if (!creater)
            return nullptr;

        if (spec.kind == GeometryAssetKind::File)
        {
            // 文件来源要自己管 VDM/缓冲池，无法塞进外部 creater。
            // 调用方应改用 (device,bm) 重载或走异步流送。
            MLogError(GeometryAsset, OS_TEXT("CreateGeometryFromSpec(creater): file-side geometry needs its own VDM, use the device overload"));
            return nullptr;
        }

        switch (spec.shape)
        {
        case BuiltinShapeKind::Sphere:
            return inline_geometry::CreateSphere(creater, spec.params.sphere.slices);

        case BuiltinShapeKind::Dome:
            return inline_geometry::CreateDome(creater, spec.params.dome.slices);

        case BuiltinShapeKind::Cube:
        {
            inline_geometry::CubeCreateInfo cci;
            cci.segments_x = spec.params.cube.segments_x;
            cci.segments_y = spec.params.cube.segments_y;
            cci.segments_z = spec.params.cube.segments_z;
            cci.ntb = spec.params.cube.ntb;
            cci.tex_coord = spec.params.cube.tex_coord;
            return inline_geometry::CreateCube(creater, &cci);
        }

        case BuiltinShapeKind::PlaneGrid:
        {
            inline_geometry::PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(spec.params.plane_grid.grid_x, spec.params.plane_grid.grid_y);
            pgci.sub_count.Set(spec.params.plane_grid.sub_x, spec.params.plane_grid.sub_y);
            pgci.lum = spec.params.plane_grid.lum;
            pgci.sub_lum = spec.params.plane_grid.sub_lum;

            if (spec.params.plane_grid.is_3d)
                return inline_geometry::CreatePlaneGrid3D(creater, &pgci);
            else
                return inline_geometry::CreatePlaneGrid2D(creater, &pgci);
        }

        case BuiltinShapeKind::PlaneSquare:
            return inline_geometry::CreatePlaneSqaure(creater);

        case BuiltinShapeKind::Cylinder:
        {
            inline_geometry::CylinderCreateInfo cci;
            cci.halfExtend = spec.params.cylinder.half_extend;
            cci.radius = spec.params.cylinder.radius;
            cci.numberSlices = spec.params.cylinder.slices;
            return inline_geometry::CreateCylinder(creater, &cci);
        }

        case BuiltinShapeKind::HollowCylinder:
        {
            inline_geometry::HollowCylinderCreateInfo hcci;
            hcci.halfExtend = spec.params.hollow_cylinder.half_extend;
            hcci.innerRadius = spec.params.hollow_cylinder.inner_radius;
            hcci.outerRadius = spec.params.hollow_cylinder.outer_radius;
            hcci.numberSlices = spec.params.hollow_cylinder.slices;
            hcci.cap_radial_tiles = spec.params.hollow_cylinder.cap_radial_tiles;
            hcci.cap_angular_tiles = spec.params.hollow_cylinder.cap_angular_tiles;
            return inline_geometry::CreateHollowCylinder(creater, &hcci);
        }

        case BuiltinShapeKind::Torus:
        {
            inline_geometry::TorusCreateInfo tci;
            tci.innerRadius = spec.params.torus.inner_radius;
            tci.outerRadius = spec.params.torus.outer_radius;
            tci.numberSlices = spec.params.torus.slices;
            tci.numberStacks = spec.params.torus.stacks;
            tci.uv_scale = { spec.params.torus.uv_scale_x, spec.params.torus.uv_scale_y };
            return inline_geometry::CreateTorus(creater, &tci);
        }

        case BuiltinShapeKind::Cone:
        {
            inline_geometry::ConeCreateInfo cci;
            cci.halfExtend = spec.params.cone.half_extend;
            cci.radius = spec.params.cone.radius;
            cci.numberSlices = spec.params.cone.slices;
            cci.numberStacks = spec.params.cone.stacks;
            return inline_geometry::CreateCone(creater, &cci);
        }

        case BuiltinShapeKind::Capsule:
        {
            inline_geometry::CapsuleCreateInfo cci;
            cci.halfHeight = spec.params.capsule.half_height;
            cci.radius = spec.params.capsule.radius;
            cci.numberSlices = spec.params.capsule.slices;
            cci.numberStacks = spec.params.capsule.stacks;
            return inline_geometry::CreateCapsule(creater, &cci);
        }

        case BuiltinShapeKind::Rectangle:
        {
            inline_geometry::RectangleCreateInfo rci;
            rci.scope.Set(spec.params.rectangle.left, spec.params.rectangle.top,
                          spec.params.rectangle.width, spec.params.rectangle.height);
            rci.segments_x = spec.params.rectangle.segments_x;
            rci.segments_y = spec.params.rectangle.segments_y;
            rci.ntb = spec.params.rectangle.ntb;
            rci.tex_coord = spec.params.rectangle.tex_coord;
            return inline_geometry::CreateRectangle(creater, &rci);
        }

        case BuiltinShapeKind::BoundingBox:
        {
            inline_geometry::BoundingBoxCreateInfo bbci;
            bbci.ntb = spec.params.bounding_box.ntb;
            return inline_geometry::CreateBoundingBox(creater, &bbci);
        }

        case BuiltinShapeKind::HexSphere:
        {
            inline_geometry::HexSphereCreateInfo hsci;
            hsci.subdivisions = spec.params.hex_sphere.subdivisions;
            hsci.radius       = spec.params.hex_sphere.radius;
            hsci.uv_scale     = { spec.params.hex_sphere.uv_scale_x, spec.params.hex_sphere.uv_scale_y };
            return inline_geometry::CreateHexSphere(creater, &hsci);
        }

        case BuiltinShapeKind::TaperedCapsule:
        {
            inline_geometry::TaperedCapsuleCreateInfo tcci;
            tcci.halfHeight   = spec.params.tapered_capsule.half_height;
            tcci.bottomRadius = spec.params.tapered_capsule.bottom_radius;
            tcci.topRadius    = spec.params.tapered_capsule.top_radius;
            tcci.numberSlices = spec.params.tapered_capsule.slices;
            tcci.numberStacks = spec.params.tapered_capsule.stacks;
            return inline_geometry::CreateTaperedCapsule(creater, &tcci);
        }

        case BuiltinShapeKind::Frustum:
        {
            inline_geometry::FrustumCreateInfo fci;
            fci.bottom_radius = spec.params.frustum.bottom_radius;
            fci.top_radius    = spec.params.frustum.top_radius;
            fci.height        = spec.params.frustum.height;
            fci.numberSlices  = spec.params.frustum.slices;
            fci.generate_caps = spec.params.frustum.generate_caps;
            return inline_geometry::CreateFrustum(creater, &fci);
        }

        case BuiltinShapeKind::Axis:
        {
            inline_geometry::AxisCreateInfo aci;
            aci.size = spec.params.axis.size;
            return inline_geometry::CreateAxis(creater, &aci);
        }

        default:
            return nullptr;
        }
    }

    Geometry *CreateGeometryFromAssetFile(GeometryCreater *creater,
                                          const OSString &toml_filename)
    {
        if (!creater || toml_filename.IsEmpty())
            return nullptr;

        GeometryAssetSpec spec;

        if (!LoadGeometryAssetSpecFromTOML(toml_filename, spec))
            return nullptr;

        Geometry *geo = CreateGeometryFromSpec(creater, spec);

        if (!geo)
            MLogError(GeometryAsset, OS_TEXT("CreateGeometryFromAssetFile failed: ") + toml_filename);

        return geo;
    }

    Geometry *CreateGeometryFromSpec(VulkanDevice *device,
                                    BufferManager *bm,
                                    const GeometryAssetSpec &spec,
                                    const GeometryVertexFormat *gvf_override)
    {
        if (!device)
            return nullptr;

        const GeometryVertexFormat gvf = gvf_override
            ? *gvf_override
            : GetStandardGeometryVertexFormat(spec.NormalFormat());

        if (spec.kind == GeometryAssetKind::File)
        {
            return LoadGeometry(device, gvf, spec.file_path);
        }

        // 内置几何体：一次性 creater；形状分发与「共享 creater」重载完全同源。
        GeometryCreater creater(device, gvf, bm);

        return CreateGeometryFromSpec(&creater, spec);
    }

}//namespace hgl::graph
