#include <hgl/graph/asset/PrimitiveAssetFile.h>
#include <hgl/asset/generated/PrimitiveSerialize.gen.h>
#include <hgl/serialize/TomlCodec.h>
#include <hgl/serialize/Value.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/type/Smart.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

#include <cstring>

DEFINE_LOGGER_MODULE(PrimitiveAsset)
USE_MODULE_LOGGER(PrimitiveAsset)


namespace hgl::graph
{

    bool ParsePrimitiveAssetSpec(const char *content, size_t size,
                                 const os_char *debug_name,
                                 PrimitiveAssetSpec &out_spec)
    {
        // 先归零：同一个 `out_spec` 连续解析多份文件时，上一次的 `extra_variants`
        // 之类的容器内容不能残留下来冒充本次输入。归零放在最前面，失败返回时
        // `out_spec` 也是一份干净的、纯类内缺省值的对象。
        out_spec = PrimitiveAssetSpec{};

        if (!content || size == 0)
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: empty content for ") + OSString(debug_name));
            return false;
        }

        hgl::serialize::Value          root;
        hgl::serialize::TomlParseError toml_error;

        if (!hgl::serialize::FromTomlText(content, size, root, &toml_error))
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: TOML parse error in ")
                     + OSString(debug_name) + OS_TEXT(": ") + ToOSString(toml_error.Format().c_str()));
            return false;
        }

        if (!root.IsTable())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: root is not a table in ") + OSString(debug_name));
            return false;
        }

        // `schema` 单独先看：它决定这张字段表还能不能按当前契约解释，报错也要有
        // 独立措辞。缺键时 `schema_node` 为空，与「写了别的版本」同样被挡住。
        {
            const hgl::serialize::Value *schema_node = root.FindFieldExact("schema");
            int64_t                      schema_value = 0;

            if (!schema_node || !schema_node->GetInt(schema_value) || schema_value != 1)
            {
                MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: unsupported schema in ") + OSString(debug_name));
                return false;
            }
        }

        // `variants` 必须是数组。这条比通用的「字段类型不合法」更具体，所以前置。
        {
            const hgl::serialize::Value *variants_node = root.FindFieldExact("variants");

            if (variants_node && !variants_node->IsArray())
            {
                MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: 'variants' must be an array in ") + OSString(debug_name));
                return false;
            }
        }

        // 逐字段解析（`geometry` / `recipe` 的键名覆盖、`lod`、`purpose` 与
        // `primitive_type` 的枚举拼写、`[[variants]]` 的数组展开）全部来自
        // `PrimitiveSerialize.gen.h` —— 本文件不再认识任何键名。
        {
            hgl::serialize::Archive archive;

            if (!hgl::serialize::ReadFromValue(root, out_spec, &archive))
            {
                const AnsiString failed_path(archive.FailedPath());

                if (failed_path == "primitive_type")
                {
                    const hgl::serialize::Value *node = root.FindFieldExact("primitive_type");
                    const hgl::u8char         *raw = (node && node->IsString())
                                                     ? node->GetString().c_str()
                                                     : U8_TEXT("");

                    MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: unknown primitive_type '")
                             + ToOSString(reinterpret_cast<const char *>(raw)) + OS_TEXT("' in ") + OSString(debug_name));
                }
                else if (failed_path.StartsWith("variants"))
                {
                    MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: invalid variant entry at '")
                             + ToOSString(failed_path.c_str()) + OS_TEXT("' in ") + OSString(debug_name));
                }
                else
                {
                    MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: field '")
                             + ToOSString(failed_path.c_str()) + OS_TEXT("' is invalid (")
                             + ToOSString(archive.ErrorMessage()) + OS_TEXT(") in ") + OSString(debug_name));
                }

                return false;
            }
        }

        // 以下全是**跨字段策略**，不属于「字段规约」，因此仍留在解析层。
        if (out_spec.geometry_path.IsEmpty())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: missing 'geometry' in ") + OSString(debug_name));
            return false;
        }

        if (out_spec.recipe_name.IsEmpty())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: missing 'recipe' in ") + OSString(debug_name));
            return false;
        }

        for (int i = 0; i < out_spec.extra_variants.GetCount(); i++)
        {
            const PrimitiveVariantSpec *variant = out_spec.extra_variants.At(i);

            if (!variant)
                continue;

            if (variant->recipe_name.IsEmpty())
            {
                MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: variant missing 'recipe' in ") + OSString(debug_name));
                return false;
            }

            if (variant->purpose == PrimitiveVariantPurpose::Surface)
            {
                // Surface 变体就是主配方，重复声明会与 recipe 打架
                MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: extra variant must not use surface purpose in ")
                         + OSString(debug_name));
                return false;
            }
        }

        if (!out_spec.IsValid())
        {
            MLogError(PrimitiveAsset, OS_TEXT("ParsePrimitiveAssetSpec: incomplete spec in ") + OSString(debug_name));
            return false;
        }

        return true;
    }

    bool LoadPrimitiveAssetSpecFromTOML(const OSString &filename, PrimitiveAssetSpec &out_spec)
    {
        hgl::io::OpenFileInputStream opener(filename);

        if (!opener)
        {
            MLogError(PrimitiveAsset, OS_TEXT("LoadPrimitiveAssetSpecFromTOML: cannot open file ") + filename);
            return false;
        }

        const int64 size = opener->GetSize();

        if (size <= 0 || size > 16 * 1024 * 1024)
        {
            MLogError(PrimitiveAsset, OS_TEXT("LoadPrimitiveAssetSpecFromTOML: invalid file size in ") + filename);
            return false;
        }

        hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);

        if (!buffer || opener->Read(buffer.data(), size) != size)
        {
            MLogError(PrimitiveAsset, OS_TEXT("LoadPrimitiveAssetSpecFromTOML: read failed in ") + filename);
            return false;
        }

        buffer[static_cast<size_t>(size)] = '\0';

        return ParsePrimitiveAssetSpec(buffer.data(), static_cast<size_t>(size), filename.c_str(), out_spec);
    }
}//namespace hgl::graph
