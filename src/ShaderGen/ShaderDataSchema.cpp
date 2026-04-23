#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

// Implemented in GLSLStructSize.cpp
std::size_t CalculateGLSLStructSize(const std::string &glslStructText);

namespace hgl::graph::mtl {

namespace
{
    static ShaderDataSchemaInfo kSchemaTable[] = {
        // None
        { nullptr,                        0 },
        // Color4f
        { "schema_color4f.glsl",          0 },
        // TextColor
        { "schema_text_color.glsl",       0 },
        // BillboardSizeUVec2
        { "schema_billboard_size.glsl",   0 },
        // PBRColorParams
        { "schema_pbr_color_params.glsl", 0 },
        // StandardParams
        { "schema_standard_params.glsl",  0 },
        // TextureArrayID
        { "schema_texture_array_id.glsl", 0 },
        // Sprite2DTransform
        { "schema_sprite2d_transform.glsl", 0 },
    };

    static bool g_schema_sizes_initialized = false;

    static bool ReadTextFile(const std::string &path, std::string &out)
    {
        std::ifstream ifs(path, std::ios::in);
        if (!ifs.is_open())
            return false;

        std::ostringstream ss;
        ss << ifs.rdbuf();
        out = ss.str();
        return true;
    }

    static uint32_t ComputeSchemaByteSize(const char *schema_file)
    {
        if (!schema_file || !schema_file[0])
            return 0;

        std::string glsl;
        const std::string path = graph::GetShaderLibraryPath() + "/common/schema/" + schema_file;
        if (!ReadTextFile(path, glsl))
            return 0;

        const std::size_t size = CalculateGLSLStructSize(glsl);
        return static_cast<uint32_t>(size);
    }

    static void EnsureSchemaByteSizes()
    {
        if (g_schema_sizes_initialized)
            return;

        for (uint32_t i = 1; i < static_cast<uint32_t>(ShaderDataSchema::COUNT); ++i)
        {
            ShaderDataSchemaInfo &info = kSchemaTable[i];

            std::string glsl;
            const std::string path = graph::GetShaderLibraryPath() + "/common/schema/" + info.glsl_schema_file;
            if (!ReadTextFile(path, glsl))
            {
                std::fprintf(stderr,
                    "[ShaderDataSchema] failed to read schema file: schema=%u file=%s\n",
                    i,
                    info.glsl_schema_file ? info.glsl_schema_file : "<null>");
                info.byte_size = 0;
                continue;
            }

            info.byte_size = static_cast<uint32_t>(CalculateGLSLStructSize(glsl));
            if (info.byte_size == 0)
            {
                std::fprintf(stderr,
                    "[ShaderDataSchema] failed to compute schema byte size: schema=%u file=%s\n",
                    i,
                    info.glsl_schema_file ? info.glsl_schema_file : "<null>");
            }
        }

        g_schema_sizes_initialized = true;
    }
}

static_assert(sizeof(kSchemaTable) / sizeof(kSchemaTable[0]) ==
              static_cast<uint32_t>(ShaderDataSchema::COUNT),
              "kSchemaTable entry count must match ShaderDataSchema::COUNT");

const ShaderDataSchemaInfo & GetShaderDataSchemaInfo(ShaderDataSchema schema)
{
    EnsureSchemaByteSizes();

    const uint32_t idx = static_cast<uint32_t>(schema);
    assert(idx < static_cast<uint32_t>(ShaderDataSchema::COUNT));
    return kSchemaTable[idx];
}

} // namespace hgl::graph::mtl
