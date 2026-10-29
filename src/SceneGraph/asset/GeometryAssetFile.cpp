#include <hgl/graph/asset/GeometryAssetFile.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/geo/LoadGeometry.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/type/Smart.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

#include <toml/toml.hpp>
#include <string>
#include <cstring>

DEFINE_LOGGER_MODULE(GeometryAsset)

namespace hgl::graph
{
    namespace
    {
        bool ReadBool(const toml::value &table, const char *key, bool def_val)
        {
            if (!table.contains(key))
                return def_val;
            const auto &v = table.at(key);
            return v.is_boolean() ? v.as_boolean() : def_val;
        }

        uint32_t ReadUInt(const toml::value &table, const char *key, uint32_t def_val)
        {
            if (!table.contains(key))
                return def_val;
            const auto &v = table.at(key);
            if (!v.is_integer())
                return def_val;
            const int64_t val = v.as_integer();
            return val >= 0 ? static_cast<uint32_t>(val) : def_val;
        }

        float ReadFloat(const toml::value &table, const char *key, float def_val)
        {
            if (!table.contains(key))
                return def_val;
            const auto &v = table.at(key);
            if (v.is_floating())
                return static_cast<float>(v.as_floating());
            if (v.is_integer())
                return static_cast<float>(v.as_integer());
            return def_val;
        }

        bool ReadString(const toml::value &table, const char *key, std::string &out)
        {
            if (!table.contains(key))
                return false;
            const auto &v = table.at(key);
            if (!v.is_string())
                return false;
            out = v.as_string();
            return true;
        }

        bool ParseNormalFormat(const char *text, VkFormat &out_fmt)
        {
            if (!text || !*text)
                return false;

            // 接受三种写法：引擎短名（v2un8）、VF_ 宏名（VF_V2UN8）、Vulkan 后缀名（r8g8_unorm）
            if (hgl::stricmp(text, "v2un8") == 0
             || hgl::stricmp(text, "vf_v2un8") == 0
             || hgl::stricmp(text, "r8g8_unorm") == 0)
            {
                out_fmt = VF_V2UN8;
                return true;
            }
            if (hgl::stricmp(text, "v2hf") == 0
             || hgl::stricmp(text, "vf_v2hf") == 0
             || hgl::stricmp(text, "r16g16_sfloat") == 0)
            {
                out_fmt = VF_V2HF;
                return true;
            }
            if (hgl::stricmp(text, "v3f") == 0
             || hgl::stricmp(text, "vf_v3f") == 0
             || hgl::stricmp(text, "r32g32b32_sfloat") == 0)
            {
                out_fmt = VF_V3F;
                return true;
            }
            if (hgl::stricmp(text, "v3sn16") == 0
             || hgl::stricmp(text, "vf_v3sn16") == 0
             || hgl::stricmp(text, "r16g16b16_snorm") == 0)
            {
                out_fmt = VF_V3SN16;
                return true;
            }
            return false;
        }
    }//namespace

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
        if (hgl::stricmp(name, "bounding_box") == 0)   { out_kind = BuiltinShapeKind::BoundingBox; return true; }

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
        if (!content || size == 0)
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: empty content for ") + OSString(debug_name));
            return false;
        }

        toml::value root;
        try
        {
            root = toml::parse_str(std::string(content, size));
        }
        catch (const toml::exception &e)
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: TOML parse error in ") + OSString(debug_name) + OS_TEXT(": ") + ToOSString(e.what()));
            return false;
        }

        if (!root.is_table())
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: root is not a table in ") + OSString(debug_name));
            return false;
        }

        const uint32_t schema = ReadUInt(root, "schema", 0);
        if (schema != 1)
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unsupported schema in ") + OSString(debug_name));
            return false;
        }

        std::string name_str;
        if (ReadString(root, "name", name_str))
            out_spec.name = AnsiString(name_str.c_str(), static_cast<int>(name_str.size()));

        std::string type_str;
        if (!ReadString(root, "type", type_str))
            type_str = "builtin";

        if (hgl::stricmp(type_str.c_str(), "builtin") == 0)
        {
            out_spec.kind = GeometryAssetKind::Builtin;

            std::string shape_str;
            if (!ReadString(root, "shape", shape_str))
            {
                MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: missing 'shape' for builtin geometry in ") + OSString(debug_name));
                return false;
            }

            if (!ParseBuiltinShapeKind(shape_str.c_str(), out_spec.shape))
            {
                MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unknown shape '") + ToOSString(shape_str) + OS_TEXT("' in ") + OSString(debug_name));
                return false;
            }

            // Check if there is a shape parameter table [builtin.<shape>]
            const toml::value *params_table = nullptr;
            if (root.contains("builtin") && root.at("builtin").is_table())
            {
                const auto &builtin_tbl = root.at("builtin");
                if (builtin_tbl.contains(shape_str) && builtin_tbl.at(shape_str).is_table())
                    params_table = &builtin_tbl.at(shape_str);
            }

            switch (out_spec.shape)
            {
            case BuiltinShapeKind::Sphere:
            {
                out_spec.params.sphere = BuiltinSphereParams{};
                if (params_table)
                    out_spec.params.sphere.slices = ReadUInt(*params_table, "slices", 32);
                break;
            }
            case BuiltinShapeKind::Dome:
            {
                out_spec.params.dome = BuiltinDomeParams{};
                if (params_table)
                    out_spec.params.dome.slices = ReadUInt(*params_table, "slices", 32);
                break;
            }
            case BuiltinShapeKind::Cube:
            {
                out_spec.params.cube = BuiltinCubeParams{};
                if (params_table)
                {
                    out_spec.params.cube.segments_x = ReadUInt(*params_table, "segments_x", 1);
                    out_spec.params.cube.segments_y = ReadUInt(*params_table, "segments_y", 1);
                    out_spec.params.cube.segments_z = ReadUInt(*params_table, "segments_z", 1);
                    out_spec.params.cube.tex_coord = ReadBool(*params_table, "tex_coord", true);

                    std::string ntb_str;
                    if (ReadString(*params_table, "ntb", ntb_str))
                        ParseNTBType(ntb_str.c_str(), out_spec.params.cube.ntb);
                }
                break;
            }
            case BuiltinShapeKind::PlaneGrid:
            {
                out_spec.params.plane_grid = BuiltinPlaneGridParams{};
                if (params_table)
                {
                    out_spec.params.plane_grid.grid_x = ReadUInt(*params_table, "grid_x", 10);
                    out_spec.params.plane_grid.grid_y = ReadUInt(*params_table, "grid_y", 10);
                    out_spec.params.plane_grid.sub_x = ReadUInt(*params_table, "sub_x", 1);
                    out_spec.params.plane_grid.sub_y = ReadUInt(*params_table, "sub_y", 1);
                    out_spec.params.plane_grid.lum = static_cast<uint8_t>(ReadUInt(*params_table, "lum", 128));
                    out_spec.params.plane_grid.sub_lum = static_cast<uint8_t>(ReadUInt(*params_table, "sub_lum", 64));
                    out_spec.params.plane_grid.is_3d = ReadBool(*params_table, "is_3d", false);
                }
                break;
            }
            case BuiltinShapeKind::PlaneSquare:
            {
                break;
            }
            case BuiltinShapeKind::Cylinder:
            {
                out_spec.params.cylinder = BuiltinCylinderParams{};
                if (params_table)
                {
                    out_spec.params.cylinder.half_extend = ReadFloat(*params_table, "half_extend", 1.0f);
                    out_spec.params.cylinder.radius = ReadFloat(*params_table, "radius", 1.0f);
                    out_spec.params.cylinder.slices = ReadUInt(*params_table, "slices", 32);
                }
                break;
            }
            case BuiltinShapeKind::HollowCylinder:
            {
                out_spec.params.hollow_cylinder = BuiltinHollowCylinderParams{};
                if (params_table)
                {
                    out_spec.params.hollow_cylinder.half_extend = ReadFloat(*params_table, "half_extend", 1.0f);
                    out_spec.params.hollow_cylinder.inner_radius = ReadFloat(*params_table, "inner_radius", 0.5f);
                    out_spec.params.hollow_cylinder.outer_radius = ReadFloat(*params_table, "outer_radius", 1.0f);
                    out_spec.params.hollow_cylinder.slices = ReadUInt(*params_table, "slices", 32);
                    out_spec.params.hollow_cylinder.cap_radial_tiles = ReadFloat(*params_table, "cap_radial_tiles", 1.0f);
                    out_spec.params.hollow_cylinder.cap_angular_tiles = ReadFloat(*params_table, "cap_angular_tiles", 1.0f);
                }
                break;
            }
            case BuiltinShapeKind::Torus:
            {
                out_spec.params.torus = BuiltinTorusParams{};
                if (params_table)
                {
                    out_spec.params.torus.inner_radius = ReadFloat(*params_table, "inner_radius", 0.5f);
                    out_spec.params.torus.outer_radius = ReadFloat(*params_table, "outer_radius", 1.0f);
                    out_spec.params.torus.slices = ReadUInt(*params_table, "slices", 32);
                    out_spec.params.torus.stacks = ReadUInt(*params_table, "stacks", 16);
                    out_spec.params.torus.uv_scale_x = ReadFloat(*params_table, "uv_scale_x", 1.0f);
                    out_spec.params.torus.uv_scale_y = ReadFloat(*params_table, "uv_scale_y", 1.0f);
                }
                break;
            }
            case BuiltinShapeKind::Cone:
            {
                out_spec.params.cone = BuiltinConeParams{};
                if (params_table)
                {
                    out_spec.params.cone.half_extend = ReadFloat(*params_table, "half_extend", 1.0f);
                    out_spec.params.cone.radius = ReadFloat(*params_table, "radius", 1.0f);
                    out_spec.params.cone.slices = ReadUInt(*params_table, "slices", 32);
                    out_spec.params.cone.stacks = ReadUInt(*params_table, "stacks", 4);
                }
                break;
            }
            case BuiltinShapeKind::Capsule:
            {
                out_spec.params.capsule = BuiltinCapsuleParams{};
                if (params_table)
                {
                    out_spec.params.capsule.half_height = ReadFloat(*params_table, "half_height", 1.0f);
                    out_spec.params.capsule.radius = ReadFloat(*params_table, "radius", 0.5f);
                    out_spec.params.capsule.slices = ReadUInt(*params_table, "slices", 16);
                    out_spec.params.capsule.stacks = ReadUInt(*params_table, "stacks", 8);
                }
                break;
            }
            case BuiltinShapeKind::Rectangle:
            {
                out_spec.params.rectangle = BuiltinRectangleParams{};
                if (params_table)
                {
                    out_spec.params.rectangle.left = ReadFloat(*params_table, "left", -1.0f);
                    out_spec.params.rectangle.top = ReadFloat(*params_table, "top", -1.0f);
                    out_spec.params.rectangle.width = ReadFloat(*params_table, "width", 2.0f);
                    out_spec.params.rectangle.height = ReadFloat(*params_table, "height", 2.0f);
                    out_spec.params.rectangle.segments_x = ReadUInt(*params_table, "segments_x", 1);
                    out_spec.params.rectangle.segments_y = ReadUInt(*params_table, "segments_y", 1);
                    out_spec.params.rectangle.tex_coord = ReadBool(*params_table, "tex_coord", true);

                    std::string ntb_str;
                    if (ReadString(*params_table, "ntb", ntb_str))
                        ParseNTBType(ntb_str.c_str(), out_spec.params.rectangle.ntb);
                }
                break;
            }
            case BuiltinShapeKind::BoundingBox:
            {
                out_spec.params.bounding_box = BuiltinBoundingBoxParams{};
                if (params_table)
                {
                    std::string ntb_str;
                    if (ReadString(*params_table, "ntb", ntb_str))
                        ParseNTBType(ntb_str.c_str(), out_spec.params.bounding_box.ntb);
                }
                break;
            }
            }
        }
        else if (hgl::stricmp(type_str.c_str(), "file") == 0)
        {
            out_spec.kind = GeometryAssetKind::File;
            std::string file_str;
            if (!ReadString(root, "file", file_str))
            {
                MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: missing 'file' attribute for file geometry in ") + OSString(debug_name));
                return false;
            }
            out_spec.file_path = ToOSString(file_str);
        }
        else
        {
            MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unknown type '") + ToOSString(type_str) + OS_TEXT("' in ") + OSString(debug_name));
            return false;
        }

        // Vertex format overrides
        if (root.contains("vertex_format") && root.at("vertex_format").is_table())
        {
            const auto &vf_tbl = root.at("vertex_format");
            std::string normal_fmt_str;
            if (ReadString(vf_tbl, "normal", normal_fmt_str)
             || ReadString(vf_tbl, "normal_format", normal_fmt_str))
            {
                if (!ParseNormalFormat(normal_fmt_str.c_str(), out_spec.normal_format))
                {
                    MLogError(GeometryAsset, OS_TEXT("ParseGeometryAssetSpec: unknown normal vertex format '")
                             + ToOSString(normal_fmt_str) + OS_TEXT("' in ") + OSString(debug_name));
                    return false;
                }
            }
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

    Geometry *CreateGeometryFromSpec(VulkanDevice *device,
                                    BufferManager *bm,
                                    const GeometryAssetSpec &spec,
                                    const GeometryVertexFormat *gvf_override)
    {
        if (!device)
            return nullptr;

        const GeometryVertexFormat gvf = gvf_override
            ? *gvf_override
            : GetStandardGeometryVertexFormat(spec.normal_format);

        if (spec.kind == GeometryAssetKind::File)
        {
            return LoadGeometry(device, gvf, spec.file_path);
        }

        // Builtin geometry creation via GeometryCreater
        GeometryCreater creater(device, gvf, bm);

        switch (spec.shape)
        {
        case BuiltinShapeKind::Sphere:
            return inline_geometry::CreateSphere(&creater, spec.params.sphere.slices);

        case BuiltinShapeKind::Dome:
            return inline_geometry::CreateDome(&creater, spec.params.dome.slices);

        case BuiltinShapeKind::Cube:
        {
            inline_geometry::CubeCreateInfo cci;
            cci.segments_x = spec.params.cube.segments_x;
            cci.segments_y = spec.params.cube.segments_y;
            cci.segments_z = spec.params.cube.segments_z;
            cci.ntb = spec.params.cube.ntb;
            cci.tex_coord = spec.params.cube.tex_coord;
            return inline_geometry::CreateCube(&creater, &cci);
        }

        case BuiltinShapeKind::PlaneGrid:
        {
            inline_geometry::PlaneGridCreateInfo pgci;
            pgci.grid_size.Set(spec.params.plane_grid.grid_x, spec.params.plane_grid.grid_y);
            pgci.sub_count.Set(spec.params.plane_grid.sub_x, spec.params.plane_grid.sub_y);
            pgci.lum = spec.params.plane_grid.lum;
            pgci.sub_lum = spec.params.plane_grid.sub_lum;

            if (spec.params.plane_grid.is_3d)
                return inline_geometry::CreatePlaneGrid3D(&creater, &pgci);
            else
                return inline_geometry::CreatePlaneGrid2D(&creater, &pgci);
        }

        case BuiltinShapeKind::PlaneSquare:
            return inline_geometry::CreatePlaneSqaure(&creater);

        case BuiltinShapeKind::Cylinder:
        {
            inline_geometry::CylinderCreateInfo cci;
            cci.halfExtend = spec.params.cylinder.half_extend;
            cci.radius = spec.params.cylinder.radius;
            cci.numberSlices = spec.params.cylinder.slices;
            return inline_geometry::CreateCylinder(&creater, &cci);
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
            return inline_geometry::CreateHollowCylinder(&creater, &hcci);
        }

        case BuiltinShapeKind::Torus:
        {
            inline_geometry::TorusCreateInfo tci;
            tci.innerRadius = spec.params.torus.inner_radius;
            tci.outerRadius = spec.params.torus.outer_radius;
            tci.numberSlices = spec.params.torus.slices;
            tci.numberStacks = spec.params.torus.stacks;
            tci.uv_scale = { spec.params.torus.uv_scale_x, spec.params.torus.uv_scale_y };
            return inline_geometry::CreateTorus(&creater, &tci);
        }

        case BuiltinShapeKind::Cone:
        {
            inline_geometry::ConeCreateInfo cci;
            cci.halfExtend = spec.params.cone.half_extend;
            cci.radius = spec.params.cone.radius;
            cci.numberSlices = spec.params.cone.slices;
            cci.numberStacks = spec.params.cone.stacks;
            return inline_geometry::CreateCone(&creater, &cci);
        }

        case BuiltinShapeKind::Capsule:
        {
            inline_geometry::CapsuleCreateInfo cci;
            cci.halfHeight = spec.params.capsule.half_height;
            cci.radius = spec.params.capsule.radius;
            cci.numberSlices = spec.params.capsule.slices;
            cci.numberStacks = spec.params.capsule.stacks;
            return inline_geometry::CreateCapsule(&creater, &cci);
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
            return inline_geometry::CreateRectangle(&creater, &rci);
        }

        case BuiltinShapeKind::BoundingBox:
        {
            inline_geometry::BoundingBoxCreateInfo bbci;
            bbci.ntb = spec.params.bounding_box.ntb;
            return inline_geometry::CreateBoundingBox(&creater, &bbci);
        }

        default:
            return nullptr;
        }
    }

}//namespace hgl::graph
