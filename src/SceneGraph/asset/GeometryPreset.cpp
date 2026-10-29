#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/graph/asset/GeometryAssetFile.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/filesystem/Path.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    GeometryPresetLibrary::GeometryPresetLibrary()
        :root(OS_TEXT("res/geo/presets"))
    {
    }

    GeometryPresetLibrary &GeometryPresetLibrary::Instance()
    {
        static GeometryPresetLibrary instance;
        return instance;
    }

    void GeometryPresetLibrary::SetRoot(const OSString &new_root)
    {
        root = new_root;

        // 去掉尾部分隔符：`Path::Append` 会自己补一个，否则拼出 `res/geo/presets//x`。
        while (root.Length() > 1
            && (root.c_str()[root.Length() - 1] == OS_TEXT('/')
             || root.c_str()[root.Length() - 1] == OS_TEXT('\\')))
            root.Resize(root.Length() - 1);

        Clear();
    }

    void GeometryPresetLibrary::Clear()
    {
        presets.Clear();
    }

    const GeometryPresetLibrary::Preset *GeometryPresetLibrary::FindOrLoad(const char *name)
    {
        if (!name || !*name)
            return nullptr;

        for (Preset *preset : presets)
        {
            if (preset && preset->name == name)
                return preset->failed ? nullptr : preset;
        }

        Preset *preset = presets.Create();

        if (!preset)
            return nullptr;

        preset->name = AnsiString(name);

        const OSString filename = hgl::filesystem::Path(root) / (OSString(ToOSString(AnsiString(name))) + OS_TEXT(".geo.toml"));

        if (!LoadGeometryAssetSpecFromTOML(filename, preset->spec))
        {
            preset->failed = true;
            GLogError(u8"[GeometryPreset] 预设加载失败：%s",
                      reinterpret_cast<const char *>(ToU8String(filename).c_str()));
            return nullptr;
        }

        if (preset->spec.kind != GeometryAssetKind::Builtin)
        {
            preset->failed = true;
            GLogError(u8"[GeometryPreset] 预设不是内置几何体（不能创建进共享 creater）：%s",
                      reinterpret_cast<const char *>(ToU8String(filename).c_str()));
            return nullptr;
        }

        return preset;
    }

    const GeometryAssetSpec *GeometryPresetLibrary::Get(const char *name)
    {
        const Preset *preset = FindOrLoad(name);

        return preset ? &preset->spec : nullptr;
    }

    Geometry *GeometryPresetLibrary::Create(GeometryCreater *creater, const char *name)
    {
        const GeometryAssetSpec *spec = Get(name);

        if (!spec)
            return nullptr;

        Geometry *geo = CreateGeometryFromSpec(creater, *spec);

        if (!geo)
            GLogError(u8"[GeometryPreset] 创建失败：%s", name ? name : "(null)");

        return geo;
    }
}//namespace hgl::graph
