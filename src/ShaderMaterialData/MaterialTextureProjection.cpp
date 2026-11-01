#include<hgl/mtl/MaterialTextureProjection.h>

namespace hgl::graph::mtl
{
    namespace
    {
        // 大小写不敏感的"以某个 ASCII 后缀结尾"判定。
        // 这里不用 strcasecmp/strnicmp：前者不在 <cstring> 里（POSIX），后者是
        // MSVC 私有名，跨平台两套名字比十行循环更麻烦。
        bool EndsWithIgnoreCase(const char *text, const size_t text_length,
                                const char *suffix, const size_t suffix_length) noexcept
        {
            if (!text || !suffix || text_length < suffix_length)
                return false;

            const char *tail = text + (text_length - suffix_length);
            for (size_t index = 0; index < suffix_length; ++index)
            {
                char a = tail[index];
                char b = suffix[index];

                if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 'a' + 'A');
                if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 'a' + 'A');

                if (a != b)
                    return false;
            }
            return true;
        }

        size_t TextLength(const char *text) noexcept
        {
            if (!text)
                return 0;

            size_t length = 0;
            while (text[length])
                ++length;
            return length;
        }
    }// namespace

    MaterialTextureProjection ResolveMaterialTextureProjection(const char *resource_id) noexcept
    {
        if (!resource_id || !resource_id[0])
            return MaterialTextureProjection::Texture2D;

        const size_t length = TextLength(resource_id);

        // 顺序有意义：`.TexOct` 与 `.TexCube` 互不为后缀，先判谁都行，
        // 但两者都不能被 `.Tex2D` 之类的通用规则抢先吃掉，所以这里只有这两条显式标记。
        if (EndsWithIgnoreCase(resource_id, length, ".TexOct", 7))
            return MaterialTextureProjection::Octahedral;

        if (EndsWithIgnoreCase(resource_id, length, ".TexCube", 8))
            return MaterialTextureProjection::CubeMap;

        // 其余：普通贴图、图集、2D 数组、资产 ID（`texarray:@...`）、空串。
        return MaterialTextureProjection::Texture2D;
    }

    MaterialTextureProjection ResolveMaterialSourceProjection(const MaterialRecipe &recipe) noexcept
    {
        // 两个槽共用这套判定：
        //   base_color ← 无光单图材质源（`material_source_texture`）
        //   sky_cube   ← Sky 域的贴图形态（`surface_sky_cube`）
        // 两者都是"领域里唯一被采样的那张贴图"，投影语义完全一样（同一份
        // `SampleMaterialProjected`）。`base_color` 优先：真出现两个槽同时存在
        // 的配方（目前没有），材质源的投影仍是它自己那一张决定的。
        for (const RecipeTextureBinding &binding : recipe.textures)
        {
            if (binding.texture_name == MaterialSourceTextureSlotName)
                return ResolveMaterialTextureProjection(binding.resource_id.c_str());
        }

        for (const RecipeTextureBinding &binding : recipe.textures)
        {
            if (binding.texture_name == SkyTextureSlotName)
                return ResolveMaterialTextureProjection(binding.resource_id.c_str());
        }

        return MaterialTextureProjection::Texture2D;
    }

    const char *GetMaterialTextureProjectionIncludePath(const MaterialTextureProjection projection) noexcept
    {
        switch (projection)
        {
            case MaterialTextureProjection::Texture2D:
                return "module/provider/material_source/texture_projection/texture_projection_2d.glsl";
            case MaterialTextureProjection::CubeMap:
                return "module/provider/material_source/texture_projection/texture_projection_cube.glsl";
            case MaterialTextureProjection::Octahedral:
                return "module/provider/material_source/texture_projection/texture_projection_octahedral.glsl";
        }

        return nullptr;
    }

    const char *GetMaterialTextureProjectionName(const MaterialTextureProjection projection) noexcept
    {
        switch (projection)
        {
            case MaterialTextureProjection::Texture2D:  return "Texture2D";
            case MaterialTextureProjection::CubeMap:    return "CubeMap";
            case MaterialTextureProjection::Octahedral: return "Octahedral";
        }

        return "Unknown";
    }
}// namespace hgl::graph::mtl
