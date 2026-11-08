// TestTextureArraySpec.cpp —— 「示例把数组层清单搬进文件」这件事的门禁（无需图形设备）：
//
//   1. `ShaderLibrary/asset/texarray/` 下每个 `.texarray.toml` 都能解析，层数与真源一致；
//   2. 层名缺省值取自「父目录/文件名主干」—— 只取主干会让 10 层 baseColor 撞车；
//   3. 坏输入一律**整张拒绝**并给出定位（缺 schema / 空 layers / rt: 层 / 嵌套数组 /
//      层名重复 / 未知键 / 超层数），不返回半成品；
//   4. `texarray:` URI → 规约文件路径的映射（`@name` / `file:` / 非数组→空）；
//   5. 四个迁移过的示例源码里已经没有裸 `CreateTexture2DArray` 与文件夹名数组，
//      且其 `texarray:` 引用可以出现在**配套配方文件**里（示例只留"每格图层号 = 行号"
//      这类运行期差异）。
//
// 第 5 条是这次迁移的验收线：数组的手工装配流程（探针 → 建空数组 → 逐层填）必须
// 只出现在 `TextureManager::BuildTextureArray` 里。

#include <hgl/filesystem/FileSystem.h>
#include <hgl/graph/asset/TextureArraySpec.h>
#include <hgl/graph/asset/TextureURI.h>
#include <hgl/mtl/MaterialTextureProjection.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

using namespace hgl;

namespace graph = hgl::graph;

namespace
{
    int g_failed = 0;

    void Check(const bool condition, const char *what)
    {
        if (condition)
            return;

        GLogError(u8"TestTextureArraySpec 失败: %s", what);
        ++g_failed;
    }

    bool ReadText(const OSString &path, AnsiString &out_text)
    {
        hgl::io::OpenFileInputStream fis(path);
        if (!fis || fis->GetSize() <= 0)
            return false;

        char chunk[4096];
        int64 got;
        while ((got = fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
            out_text.Strcat(chunk, static_cast<int>(got));

        return true;
    }

    /// 顶层 schema/name/mipmaps + 若干层，拼成一份合法规约文本。
    AnsiString MakeSpec(int layer_count, const char *name = "unit_test_array")
    {
        AnsiString text("schema = 1\nname = \"");
        text += name;
        text += "\"\n";

        for (int i = 0; i < layer_count; ++i)
        {
            text += "\n[[layers]]\nuri = \"file:res/image/folder";
            text += AnsiString::numberOf(static_cast<uint>(i));
            text += "/baseColor.Tex2D\"\n";
        }

        return text;
    }

    graph::TextureArraySpecParseResult Parse(const AnsiString &text,
                                             graph::TextureArraySpec &out,
                                             graph::TextureArraySpecParseError *err = nullptr)
    {
        return graph::ParseTextureArraySpecText(text.c_str(),
                                                static_cast<int>(text.Length()),
                                                out, err);
    }

    /// 1. 真实规约文件：层数与示例用法对齐。
    struct FileLedger
    {
        const char *path;
        const char *name;
        int         layers;
        uint32_t    mipmaps;   ///<0 = 由首层探针决定
    };

    const FileLedger kLedger[] =
    {
        // PBRSpheres + CascadeShadowMap 共用的 10x10 贴图集
        { "ShaderLibrary/asset/texarray/pbr_10x10_baseColor.texarray.toml", "pbr_10x10_baseColor", 10, 0 },
        { "ShaderLibrary/asset/texarray/pbr_10x10_normal.texarray.toml",    "pbr_10x10_normal",    10, 0 },

        // alpha test 遮罩：必须钉死 mip=1，否则缩小采样把黑白混成灰
        { "ShaderLibrary/asset/texarray/alpha_checker_baseColor.texarray.toml", "alpha_checker_baseColor", 1, 1 },
        { "ShaderLibrary/asset/texarray/alpha_checker_white.texarray.toml",     "alpha_checker_white",     1, 1 },

        // SingleSphereMaterialSwitch 的 near 材质
        { "ShaderLibrary/asset/texarray/single_sphere_near_baseColor.texarray.toml", "single_sphere_near_baseColor", 1, 0 },
        { "ShaderLibrary/asset/texarray/single_sphere_near_normal.texarray.toml",    "single_sphere_near_normal",    1, 0 },
    };

    bool TestRealFiles()
    {
        const int count = int(sizeof(kLedger) / sizeof(kLedger[0]));

        for (int i = 0; i < count; ++i)
        {
            const FileLedger &entry = kLedger[i];

            graph::TextureArraySpec spec;
            graph::TextureArraySpecParseError err;

            if (graph::ParseTextureArraySpecFile(ToOSString(entry.path), spec, &err)
                != graph::TextureArraySpecParseResult::OK)
            {
                GLogError(u8"TestTextureArraySpec: 规约解析失败 %s：%s",
                          entry.path, err.Format().c_str());
                return false;
            }

            Check(spec.name == entry.name, "规约名与文件名主干一致");
            Check(spec.layers.GetCount() == entry.layers, "层数与台账一致");
            Check(spec.mipmaps == entry.mipmaps, "mipmaps 覆盖与台账一致");
            Check(spec.schema == graph::kTextureArraySpecSchema, "schema 与引擎版本一致");

            // 每层都是磁盘纹理，且名字非空
            for (int k = 0; k < spec.layers.GetCount(); ++k)
            {
                const graph::TextureArrayLayerSpec &layer = *spec.layers[k];

                Check(!layer.name.IsEmpty(), "每层都有名字");
                Check(!layer.uri.IsEmpty(), "每层都有来源 URI");

                const graph::TextureURI uri = graph::TextureURI::Parse(layer.uri);
                Check(uri.IsValid() && !uri.IsRenderTarget() && !uri.IsTextureArray(),
                      "每层都是可上传的磁盘纹理");
            }
        }

        return true;
    }

    /// 2. 缺省层名带父目录，10 层同文件名也不撞车。
    bool TestDefaultLayerName()
    {
        AnsiString text("schema = 1\nname = \"naming\"\n");
        text += "\n[[layers]]\nuri = \"file:res/image/pbr/Concrete_Plain/baseColor.Tex2D\"\n";
        text += "\n[[layers]]\nuri = \"file:res/image/pbr/Fresco_Decor_Wallpaper/baseColor.Tex2D\"\n";

        graph::TextureArraySpec spec;
        if (Parse(text, spec) != graph::TextureArraySpecParseResult::OK)
            return false;

        Check(spec.layers.GetCount() == 2, "两层都被接受");

        // 只取主干会是两个 "baseColor"，那样第二层必报「层名重复」
        Check(spec.layers[0]->name == "Concrete_Plain/baseColor", "首层名 = 父目录/主干");
        Check(spec.layers[1]->name == "Fresco_Decor_Wallpaper/baseColor", "次层名 = 父目录/主干");

        // 显式写的 name 优先于推导值
        AnsiString explicit_text("schema = 1\nname = \"explicit\"\n");
        explicit_text += "\n[[layers]]\nname = \"first\"\nuri = \"file:res/image/a/baseColor.Tex2D\"\n";
        explicit_text += "\n[[layers]]\nuri = \"file:res/image/a/baseColor.Tex2D\"\n";

        graph::TextureArraySpec explicit_spec;
        Check(Parse(explicit_text, explicit_spec) == graph::TextureArraySpecParseResult::OK,
              "显式名与推导名不同即允许（同文件不同层也算一层）");
        Check(explicit_spec.layers[0]->name == "first", "显式 name 覆盖推导值");

        return true;
    }

    /// 3. 坏输入整张拒绝。
    bool TestRejectsBadInput()
    {
        struct Case
        {
            const char *text;
            graph::TextureArraySpecParseResult expect;
            const char *what;
        };

        const Case cases[] =
        {
            { "", graph::TextureArraySpecParseResult::Skipped, "空输入只算 Skipped" },

            { "name = \"a\"\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "缺 schema 被拒" },

            { "schema = 2\nname = \"a\"\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "schema 版本不符被拒" },

            { "schema = 1\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "缺 name 被拒" },

            { "schema = 1\nname = \"\"\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "空 name 被拒" },

            { "schema = 1\nname = \"a\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "缺 layers 被拒" },

            { "schema = 1\nname = \"a\"\nlayers = []\n",
              graph::TextureArraySpecParseResult::InvalidValue, "空 layers 被拒" },

            { "schema = 1\nname = \"a\"\nmipmaps = 33\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "mipmaps 越界被拒" },

            { "schema = 1\nname = \"a\"\n[[layers]]\nuri = \"rt:GBuffer:depth\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "rt: 层被拒" },

            { "schema = 1\nname = \"a\"\n[[layers]]\nuri = \"texarray:@other\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "嵌套数组被拒" },

            { "schema = 1\nname = \"a\"\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n"
              "\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::InvalidValue, "层名重复被拒" },

            { "schema = 1\nname = \"a\"\nunknown = 1\n[[layers]]\nuri = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::UnknownKey, "未知根键被拒" },

            { "schema = 1\nname = \"a\"\n[[layers]]\nsource = \"file:x/baseColor.Tex2D\"\n",
              graph::TextureArraySpecParseResult::UnknownKey, "未知层键被拒" },
        };

        const int count = int(sizeof(cases) / sizeof(cases[0]));

        for (int i = 0; i < count; ++i)
        {
            graph::TextureArraySpec spec;
            graph::TextureArraySpecParseError err;

            const AnsiString text(cases[i].text);
            const graph::TextureArraySpecParseResult got = Parse(text, spec, &err);

            if (got != cases[i].expect)
            {
                GLogError(u8"TestTextureArraySpec: %s（得到 %s）",
                          cases[i].what,
                          graph::GetTextureArraySpecParseResultName(got));
                return false;
            }

            if (got != graph::TextureArraySpecParseResult::Skipped
             && got != graph::TextureArraySpecParseResult::OK)
                Check(!err.detail.IsEmpty(), "拒绝时给出原因");
        }

        // 超层数：64 层是上限，65 层必须拒
        graph::TextureArraySpec ok_spec;
        Check(Parse(MakeSpec(graph::kTextureArrayMaxLayers), ok_spec)
                  == graph::TextureArraySpecParseResult::OK,
              "64 层（上限）被接受");

        graph::TextureArraySpec too_many;
        Check(Parse(MakeSpec(graph::kTextureArrayMaxLayers + 1), too_many)
                  == graph::TextureArraySpecParseResult::InvalidValue,
              "65 层被拒");

        return true;
    }

    /// 4. URI → 规约路径映射。
    bool TestUriResolution()
    {
        const graph::TextureURI named  = graph::TextureURI::Parse(OS_TEXT("texarray:@pbr_10x10_baseColor"));
        const graph::TextureURI filed  = graph::TextureURI::Parse(OS_TEXT("texarray:file:ShaderLibrary/asset/texarray/x.texarray.toml"));
        const graph::TextureURI plain  = graph::TextureURI::Parse(OS_TEXT("res/image/a/baseColor.Tex2D"));
        const graph::TextureURI rt     = graph::TextureURI::Parse(OS_TEXT("rt:GBuffer:depth"));

        Check(named.IsTextureArray(), "texarray: 认成数组方案");
        Check(filed.IsTextureArray(), "texarray:file: 认成数组方案");
        Check(!plain.IsTextureArray(), "裸路径不是数组");
        Check(!rt.IsTextureArray(), "rt: 不是数组");

        const graph::TextureURI octa = graph::TextureURI::Parse(OS_TEXT("file:res/image/sky.TexOct"));
        const graph::TextureURI octa_upper = graph::TextureURI::Parse(OS_TEXT("C:\\image\\sky.TEXOCT"));
        const graph::TextureURI cube_octa = graph::TextureURI::Parse(OS_TEXT("cubefile:res/image/sky.TexOct"));
        const graph::TextureURI rt_octa = graph::TextureURI::Parse(OS_TEXT("rt:sky.TexOct"));
        const graph::TextureURI suffix = graph::TextureURI::Parse(OS_TEXT("res/image/sky.TexOct.backup"));
        Check(octa.IsOctahedralFile() && octa_upper.IsOctahedralFile(),
              ".TexOct 文件按扩展名识别 Octahedral，大小写不敏感");
        Check(!plain.IsOctahedralFile() && !cube_octa.IsOctahedralFile()
              && !rt_octa.IsOctahedralFile() && !suffix.IsOctahedralFile(),
              "普通纹理、立方纹理、RT 和非末尾扩展名不视为 Octahedral 文件");

        // pack(): 包裹寻址仍是 File 形态，path 保留原文，ToString() 不补 file:
        const graph::TextureURI pack = graph::TextureURI::Parse(OS_TEXT("pack(res):image/lena.Tex2D"));
        Check(pack.IsValid() && pack.IsFile() && pack.IsPackURI(), "pack() 认成 File 包裹寻址");
        Check(pack.GetPackName() == OS_TEXT("res")
              && pack.GetPackTarget() == OS_TEXT("image/lena.Tex2D"),
              "pack() 名字与目标切分正确");
        Check(pack.ToString() == OS_TEXT("pack(res):image/lena.Tex2D"),
              "pack() 原文回写，不退化成 file:pack()");

        const graph::TextureURI pack_octa = graph::TextureURI::Parse(OS_TEXT("pack(res):image/sky.TexOct"));
        Check(pack_octa.IsPackURI() && pack_octa.IsOctahedralFile(),
              "pack() 的 .TexOct 仍按扩展名识别");

        const graph::TextureURI file_pack = graph::TextureURI::Parse(OS_TEXT("file:pack(res):image/a.Tex2D"));
        Check(file_pack.IsPackURI() && file_pack.GetPackName() == OS_TEXT("res"),
              "file:pack() 归一成包裹寻址");

        // ── 类型前缀写法：tex2d/texcube/texoct（目标可不带容器扩展名，可叠加 pack()）──
        const graph::TextureURI t2d    = graph::TextureURI::Parse(OS_TEXT("tex2d:res/image/lena.Tex2D"));
        const graph::TextureURI t2d_p  = graph::TextureURI::Parse(OS_TEXT("tex2d:pack(res):image/lena"));
        const graph::TextureURI tcube  = graph::TextureURI::Parse(OS_TEXT("texcube:pack(res):image/sky_irradiance"));
        const graph::TextureURI toct   = graph::TextureURI::Parse(OS_TEXT("texoct:res/image/sky"));
        const graph::TextureURI toct_p = graph::TextureURI::Parse(OS_TEXT("texoct:pack(res):image/sky_prefilter"));
        Check(t2d.IsFile() && t2d.path == OS_TEXT("res/image/lena.Tex2D"),
              "tex2d: 解析为 File 形态");
        Check(t2d_p.IsFile() && t2d_p.IsPackURI() && t2d_p.GetPackTarget() == OS_TEXT("image/lena"),
              "tex2d: 的目标可以是 pack() 形态");
        Check(tcube.IsCubeFile() && tcube.IsPackURI() && tcube.GetPackName() == OS_TEXT("res"),
              "texcube: 解析为 CubeFile 且支持 pack()");
        Check(toct.IsOctaFile() && toct.IsOctahedralFile() && toct.IsDiskFile(),
              "texoct: 解析为 OctaFile，前缀即判八面体");
        Check(toct_p.IsPackURI() && toct_p.IsOctahedralFile(),
              "texoct:pack() 同时认包裹与八面体");
        Check(toct.ToString() == OS_TEXT("texoct:res/image/sky"),
              "texoct: 的 ToString() 原文回写");
        Check(tcube.ToString() == OS_TEXT("cubefile:pack(res):image/sky_irradiance"),
              "texcube: 与 cubefile: 同形态回写");
        Check(!tcube.IsOctahedralFile() && !t2d_p.IsOctahedralFile(),
              "texcube/tex2d 前缀不受八面体判定影响");

        // 坏形态一律判非法：缺定界符、空名字、空目标、名字里带冒号
        Check(!graph::TextureURI::Parse(OS_TEXT("pack(res)image/a.Tex2D")).IsValid()
              && !graph::TextureURI::Parse(OS_TEXT("pack():image/a.Tex2D")).IsValid()
              && !graph::TextureURI::Parse(OS_TEXT("pack(res):")).IsValid()
              && !graph::TextureURI::Parse(OS_TEXT("pack(a:b):image/a.Tex2D")).IsValid(),
              "坏 pack() 形态判非法，不当磁盘路径放行");

        Check(named.IsValid() && named.IsArrayByName()
              && named.GetArrayName() == OS_TEXT("pbr_10x10_baseColor"),
              "@name 的 GetArrayName() 只留名字");
        Check(named.path == OS_TEXT("@pbr_10x10_baseColor"),
              "@name 的 path 保留 @ 前缀");

        const OSString resolved = graph::ResolveTextureArraySpecPath(named);
        Check(resolved == OSString(graph::kTextureArrayDefaultDirectory)
                        + OSString(OS_TEXT("pbr_10x10_baseColor"))
                        + OSString(graph::kTextureArraySpecExtension),
              "@name → 默认目录 + 名字 + .texarray.toml");

        Check(graph::ResolveTextureArraySpecPath(filed)
                  == OS_TEXT("ShaderLibrary/asset/texarray/x.texarray.toml"),
              "file: 原样返回路径");

        Check(graph::ResolveTextureArraySpecPath(plain).IsEmpty(), "非数组 URI 得到空路径");
        Check(graph::ResolveTextureArraySpecPath(rt).IsEmpty(), "rt: URI 得到空路径");

        Check(graph::MakeTextureArrayNameFromPath(
                  OS_TEXT("ShaderLibrary/asset/texarray/pbr_10x10_baseColor.texarray.toml"))
              == "pbr_10x10_baseColor",
              "文件名主干（去掉 .texarray.toml）推导数组名");

        // URI 文本往返
        Check(named.ToString() == OS_TEXT("texarray:@pbr_10x10_baseColor"),
              "ToString 输出 texarray:@name");

        return true;
    }

    /// `ProbeTextureURI`：无扩展名 URI 的磁盘探测（用仓库真实夹具，CWD=仓库根）。
    bool TestProbeTextureURI()
    {
        const OSString stem   = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k");
        const OSString irrad  = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance");

        // 裸路径：默认按 2D 候选（.Tex2D 优先），同名词干的 .TexCube/.TexOct 不许抢
        Check(graph::ProbeTextureURI(OS_TEXT("res/image/lena")) == OS_TEXT("res/image/lena.Tex2D"),
              "裸无扩展名路径探测出 .Tex2D 别名");
        Check(graph::ProbeTextureURI(stem) == stem + OS_TEXT(".Tex2D"),
              "同名多形态词干按 File 假设命中 .Tex2D");

        // 调用方声明形态：cube / octa 各自命中自己的别名，互不串
        Check(graph::ProbeTextureURI(stem, graph::TextureURIKind::CubeFile) == stem + OS_TEXT(".TexCube"),
              "CubeFile 假设命中 .TexCube（与 .Tex2D 同词干不混淆）");
        Check(graph::ProbeTextureURI(stem, graph::TextureURIKind::OctaFile) == stem + OS_TEXT(".TexOct"),
              "OctaFile 假设命中 .TexOct");
        Check(graph::ProbeTextureURI(irrad, graph::TextureURIKind::CubeFile) == irrad + OS_TEXT(".TexCube"),
              "只有 .TexCube/.TexOct 的 irradiance 按 cube 探测成功");
        Check(graph::ProbeTextureURI(irrad) == irrad,
              "irradiance 按 2D 候选全部落空 ⇒ 原样返回，绝不乱补扩展名");

        // scheme 形态：前缀保留 + 补全；cubefile:/texcube: 同候选
        Check(graph::ProbeTextureURI(OS_TEXT("tex2d:res/image/lena")) == OS_TEXT("tex2d:res/image/lena.Tex2D"),
              "tex2d: 前缀在探测结果里保留");
        Check(graph::ProbeTextureURI(OS_TEXT("texcube:") + irrad) == OS_TEXT("texcube:") + irrad + OS_TEXT(".TexCube"),
              "texcube: 前缀决定候选，忽略 assume");
        Check(graph::ProbeTextureURI(OS_TEXT("texoct:") + stem) == OS_TEXT("texoct:") + stem + OS_TEXT(".TexOct"),
              "texoct: 前缀探测 .TexOct");
        Check(graph::ProbeTextureURI(OS_TEXT("cubefile:") + irrad) == OS_TEXT("cubefile:") + irrad + OS_TEXT(".TexCube"),
              "cubefile: 旧写法同样探测");

        // 已带扩展名 / 非磁盘形态 / 不存在 ⇒ 原样返回
        Check(graph::ProbeTextureURI(OS_TEXT("res/image/lena.Tex2D")) == OS_TEXT("res/image/lena.Tex2D"),
              "已带扩展名不重复探测");
        // 扩展名判定取**最后一个**点：前面还能有点，只要末段落在容器表里就算已带扩展名
        Check(graph::ProbeTextureURI(OS_TEXT("res/image/mipmaps/MipLevels.RGBA8.Tex2D"))
                  == OS_TEXT("res/image/mipmaps/MipLevels.RGBA8.Tex2D"),
              "多点的完整名按最后一个点判定为已带扩展名");

        // 名字里的点不是扩展名：这种词干照样得按 kind 补全（历史漏网的一支）。
        Check(graph::ProbeTextureURI(OS_TEXT("res/image/mipmaps/MipLevels.RGBA8"))
                  == OS_TEXT("res/image/mipmaps/MipLevels.RGBA8.Tex2D"),
              "词干含点必须补出 .Tex2D（MipLevels.RGBA8）");
        Check(graph::ProbeTextureURI(OS_TEXT("res/image/mipmaps/MipLevels.BC7"))
                  == OS_TEXT("res/image/mipmaps/MipLevels.BC7.Tex2D"),
              "词干含点必须补出 .Tex2D（MipLevels.BC7）");
        // 原文本身就能命名时不改写：`.toml` 不在容器后缀表里，但文件真实存在。
        Check(graph::ProbeTextureURI(OS_TEXT("ShaderLibrary/asset/recipe/TextureQuad.recipe.toml"))
                  == OS_TEXT("ShaderLibrary/asset/recipe/TextureQuad.recipe.toml"),
              "原文命中的含点路径不许被补成 .Tex2D");
        Check(graph::ProbeTextureURI(OS_TEXT("rt:GBuffer:depth")) == OS_TEXT("rt:GBuffer:depth"),
              "rt: 非磁盘形态不探测");
        Check(graph::ProbeTextureURI(OS_TEXT("texarray:@pbr_10x10_baseColor"))
                  == OS_TEXT("texarray:@pbr_10x10_baseColor"),
              "texarray: 不探测");
        Check(graph::ProbeTextureURI(OS_TEXT("res/image/__no_such_texture_xyz"))
                  == OS_TEXT("res/image/__no_such_texture_xyz"),
              "全候选落空的原样返回");

        return true;
    }

    /// 去掉行注释后再做「不得出现 X」的断言 —— 注释里可以留历史说明，代码里不行。
    AnsiString StripLineComments(const AnsiString &src)
    {
        AnsiString out;
        const char *p = src.c_str();

        while (*p)
        {
            if (p[0] == '/' && p[1] == '/')
            {
                while (*p && *p != '\n')
                    ++p;

                continue;
            }

            out.Strcat(p, 1);
            ++p;
        }

        return out;
    }

    /// 5. 迁移过的示例不再手工装配数组。
    bool TestExampleSources()
    {
        struct ExampleEntry
        {
            const char *cpp_file;
            const char *recipe_files[3];
        };

        const ExampleEntry targets[] =
        {
            {
                "example/Shadow/CascadeShadowMap.cpp",
                {
                    "ShaderLibrary/asset/recipe/Examples/Shadow/CascadeShadowMap.Lit.recipe.toml",
                    "ShaderLibrary/asset/recipe/Examples/Shadow/CascadeShadowMap.AlphaTest.recipe.toml",
                    nullptr
                }
            },
            {
                "example/Material/PBRSpheres.cpp",
                {
                    "ShaderLibrary/asset/recipe/PBRSpheres.recipe.toml",
                    "ShaderLibrary/asset/recipe/Examples/Material/PBRSpheres.LitBillboard.recipe.toml",
                    nullptr
                }
            },
            {
                "example/Material/SingleSphereMaterialSwitch.cpp",
                {
                    "ShaderLibrary/asset/recipe/Examples/Material/06e.SingleSphereSwitch.Near.recipe.toml",
                    nullptr,
                    nullptr
                }
            },
            {
                "example/Shadow/AlphaTestShadow.cpp",
                {
                    "ShaderLibrary/asset/recipe/Examples/Shadow/AlphaTestShadow.Lit.recipe.toml",
                    "ShaderLibrary/asset/recipe/Examples/Shadow/AlphaTestShadow.Ground.recipe.toml",
                    nullptr
                }
            },
        };

        const int count = int(sizeof(targets) / sizeof(targets[0]));

        for (int i = 0; i < count; ++i)
        {
            AnsiString src;
            if (!ReadText(ToOSString(targets[i].cpp_file), src))
            {
                GLogError(u8"TestTextureArraySpec: 读不到示例源码（需在仓库根目录运行）: %s",
                          targets[i].cpp_file);
                return false;
            }

            const AnsiString code = StripLineComments(src);
            const char *text = code.c_str();

            if (strstr(text, "CreateTexture2DArray") != nullptr)
            {
                GLogError(u8"TestTextureArraySpec: %s 里还有裸 CreateTexture2DArray",
                          targets[i].cpp_file);
                return false;
            }

            if (strstr(text, "LoadTexture2DArray") != nullptr)
            {
                GLogError(u8"TestTextureArraySpec: %s 里还有逐层 LoadTexture2DArray",
                          targets[i].cpp_file);
                return false;
            }

            if (strstr(text, "PBR_FOLDER") != nullptr)
            {
                GLogError(u8"TestTextureArraySpec: %s 里还有文件夹名数组 PBR_FOLDER",
                          targets[i].cpp_file);
                return false;
            }

            // 数组句柄必须来自 texarray 规约（代码中直接引用，或在其关联的配方文件 [[textures]] 中引用）
            bool has_texarray = (strstr(text, "texarray:@") != nullptr);
            if (!has_texarray)
            {
                for (int r = 0; r < 3 && targets[i].recipe_files[r] != nullptr; ++r)
                {
                    AnsiString recipe_text;
                    if (ReadText(ToOSString(targets[i].recipe_files[r]), recipe_text))
                    {
                        if (strstr(recipe_text.c_str(), "texarray:@") != nullptr)
                        {
                            has_texarray = true;
                            break;
                        }
                    }
                }
            }

            if (!has_texarray)
            {
                GLogError(u8"TestTextureArraySpec: %s 与其关联配方均未引用任何 texarray 规约",
                          targets[i].cpp_file);
                return false;
            }
        }

        return true;
    }

    // 投影推导：URI 类型前缀是第一真源，`.TexOct/.TexCube` 后缀仅作旧写法兜底。
    bool TestProjectionPrefix()
    {
        using graph::mtl::MaterialTextureProjection;
        using graph::mtl::ResolveMaterialTextureProjection;

        Check(ResolveMaterialTextureProjection("texcube:pack(res):image/sky34")
                  == MaterialTextureProjection::CubeMap,
              "texcube: 前缀推导为 CubeMap");
        Check(ResolveMaterialTextureProjection("cubefile:image/sky.TexCube")
                  == MaterialTextureProjection::CubeMap,
              "cubefile: 旧 scheme 同样推导为 CubeMap");
        Check(ResolveMaterialTextureProjection("TEXOCT:image/Environments/sky_34")
                  == MaterialTextureProjection::Octahedral,
              "texoct: 前缀大小写不敏感");
        Check(ResolveMaterialTextureProjection("tex2d:weird/suffix.TexCube")
                  == MaterialTextureProjection::Texture2D,
              "tex2d: 显式声明短路后缀兜底");
        Check(ResolveMaterialTextureProjection("pack(res):image/Brickwall/Albedo.Tex2D")
                  == MaterialTextureProjection::Texture2D,
              "pack 包裹普通贴图维持 2D");
        Check(ResolveMaterialTextureProjection("image/sky.TexOct")
                  == MaterialTextureProjection::Octahedral,
              "旧 .TexOct 后缀兜底仍生效");
        Check(ResolveMaterialTextureProjection("image/sky.TexCube")
                  == MaterialTextureProjection::CubeMap,
              "旧 .TexCube 后缀兜底仍生效");
        Check(ResolveMaterialTextureProjection(nullptr)
                  == MaterialTextureProjection::Texture2D,
              "空资源 id 回落 2D");

        return true;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestTextureArraySpec"));

    Check(TestRealFiles(), "六个规约文件按台账解析");
    Check(TestDefaultLayerName(), "缺省层名带父目录");
    Check(TestRejectsBadInput(), "坏输入整张拒绝");
    Check(TestUriResolution(), "texarray: URI → 规约路径");
    Check(TestProbeTextureURI(), "无扩展名 URI 磁盘探测");
    Check(TestProjectionPrefix(), "URI 前缀推导采样投影");
    Check(TestExampleSources(), "示例不再手工装配数组");

    if (g_failed > 0)
    {
        GLogError(u8"TestTextureArraySpec: %d 项失败", g_failed);
        return 1;
    }

    GLogInfo(u8"TestTextureArraySpec: 全部通过");
    return 0;
}
