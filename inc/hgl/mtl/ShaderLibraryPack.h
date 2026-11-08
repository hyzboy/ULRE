#pragma once

/**
 * ShaderLibrary 包内访问统一入口。
 *
 * 背景：着色器库在 asset_mount.toml 中以名为 `ShaderLibrary` 的包挂载。
 * 历史上各运行时消费者都自行用环境变量/向上搜索解析物理目录
 * （mtl::GetShaderLibraryPath()），导致开发与发行形态行为分叉，也无法
 * 统一收口文件访问。本头文件把访问渠道收敛为一条：
 *
 *   · 文本读取（.glsl / contract/*.toml 等非索引裸文件）：
 *       ReadShaderLibraryText / ReadShaderLibraryModuleText ——
 *       包已挂载时走 `pack(ShaderLibrary):<相对路径>` → OpenAssetInputStream；
 *   · 必须拿到物理目录的场景（glslang include root、目录扫描等）：
 *       GetShaderLibraryRoot —— 优先取包挂载根（归一化为绝对路径）。
 *
 * 包未挂载时（离线工具、独立测试）两者均回退到原 GetShaderLibraryPath()
 * 解析链，保证 ShaderTooling 等工具行为零变化。
 */

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/io/InputStream.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/type/String.h>
#include <hgl/utf.h>

namespace hgl::graph::mtl
{
    /// ShaderLibrary 包的挂载名（与 asset_mount.toml 的 [[mount]] name 一致）。
    inline constexpr const char *SHADER_LIBRARY_PACK_NAME = "ShaderLibrary";

    /**
     * ShaderLibrary 物理根目录（绝对路径、已归一化）。
     * 优先取包挂载根；包未挂载时回退 GetShaderLibraryPath() 的
     * 环境变量/向上搜索链（仅供离线工具与降级路径使用）。
     */
    inline bool GetShaderLibraryRoot(OSString &out_root)
    {
        out_root.Clear();

        OSString root;
        if (hgl::graph::GetAssetMountRoot(AnsiString(SHADER_LIBRARY_PACK_NAME), root)
            && !root.IsEmpty())
        {
            // 挂载根可能写成相对路径：相对当前工作目录绝对化后归一化，
            // 保证插件 include 解析与依赖哈希的键稳定。
            const bool drive_absolute = root.Length() > 1 && root[1] == ':';
            if (!drive_absolute)
            {
                OSString current_path;
                if (hgl::filesystem::GetCurrentPath(current_path))
                    root = (hgl::filesystem::Path(current_path) / root).ToOSString();
            }
            out_root = hgl::filesystem::NormalizeFilename(root);
            return !out_root.IsEmpty();
        }

        const std::string legacy = GetShaderLibraryPath();
        if (legacy.empty())
            return false;

        out_root = hgl::filesystem::NormalizeFilename(ToOSString(legacy.c_str()));
        return !out_root.IsEmpty();
    }

    /**
     * 打开包内着色器库文件流（rel_path 为含扩展名的 '/'分隔包内相对路径）。
     * 仅在包已挂载时尝试；成功返回的流由调用方 Close() 并 delete。
     */
    inline hgl::io::InputStream *OpenShaderLibraryStream(const OSString &rel_path,
                                                          AnsiString *out_error = nullptr)
    {
        if (rel_path.IsEmpty())
            return nullptr;
        if (hgl::graph::GetMountedAssetSource(SHADER_LIBRARY_PACK_NAME) == nullptr)
            return nullptr;

        OSString uri = ToOSString("pack(");
        uri += ToOSString(SHADER_LIBRARY_PACK_NAME);
        uri += ToOSString("):");
        uri += rel_path;

        return hgl::graph::OpenAssetInputStream(uri, hgl::asset::AssetType::Unknown, out_error);
    }

    /// 把流一次性读入 AnsiString；尺寸非法或读取不完整返回 false（out_text 保持空）。
    inline bool ReadStreamToText(hgl::io::InputStream *stream, AnsiString &out_text)
    {
        out_text.Clear();
        if (!stream)
            return false;

        const int64 size = stream->GetSize();
        if (size <= 0 || size > 0x7FFFFFFF)
            return false;

        char *buffer = out_text.Resize(static_cast<int>(size));
        return buffer != nullptr && stream->Read(buffer, size) == size;
    }

    /**
     * 读取 ShaderLibrary 包内裸文本文件（如 "contract/sampler.toml"、
     * "module/descriptor/ecs/entity_gpu_cull.glsl" —— 物理相对路径，含扩展名）。
     *
     * 包已挂载时只认包内结果（失败即报错，不回退物理树试探，避免
     * 开发/发行行为分叉）；包未挂载时回退物理根直读（工具形态）。
     */
    inline bool ReadShaderLibraryText(const OSString &rel_path, AnsiString &out_text)
    {
        out_text.Clear();
        if (rel_path.IsEmpty())
            return false;

        if (hgl::graph::GetMountedAssetSource(SHADER_LIBRARY_PACK_NAME) != nullptr)
        {
            AnsiString err;
            hgl::io::InputStream *stream = OpenShaderLibraryStream(rel_path, &err);
            const bool ok = ReadStreamToText(stream, out_text);
            if (stream)
            {
                stream->Close();
                delete stream;
            }
            if (!ok)
            {
                GLogError(u8"[ShaderLibraryPack] 读取包内文件失败：pack(%s):%ls（%s）",
                          SHADER_LIBRARY_PACK_NAME, rel_path.c_str(),
                          err.IsEmpty() ? reinterpret_cast<const char *>(u8"流不可用") : err.c_str());
            }
            return ok;
        }

        OSString root;
        if (!GetShaderLibraryRoot(root))
            return false;

        const OSString full_name = hgl::filesystem::NormalizeFilename(
            (hgl::filesystem::Path(root) / rel_path).ToOSString());

        hgl::io::OpenFileInputStream opener(full_name);
        if (!opener)
        {
            GLogError(u8"[ShaderLibraryPack] 包未挂载且物理直读失败：%ls", full_name.c_str());
            return false;
        }

        const int64 size = opener->GetSize();
        if (size <= 0 || size > 0x7FFFFFFF)
            return false;

        char *buffer = out_text.Resize(static_cast<int>(size));
        if (!buffer || opener->Read(buffer, size) != size)
        {
            out_text.Clear();
            return false;
        }
        return true;
    }

    /**
     * 按 GLSL 模块 include 路径（ShaderLibrary 内相对路径，可含或不含扩展名）
     * 读取模块文本。相对路径归一化与 ResolveShaderLibraryModulePath 一致
     * （拒绝 `..`、盘符、前导分隔符），但不做存在性检查——由包后端决定。
     */
    inline bool ReadShaderLibraryModuleText(const AnsiString &include_path, AnsiString &out_text)
    {
        out_text.Clear();
        if (include_path.IsEmpty())
            return false;

        AnsiString relative = include_path;
        if (!hgl::shader::NormalizeRootIncludePath(relative.c_str()))
            return false;
        relative.Resize(static_cast<int>(hgl::strlen(relative.c_str())));

        // generated/ 前缀属于每次构建的临时 job 目录，永远不在库根/包内，
        // 直接判定缺失（由调用方按 "<module-missing>" 处理），避免包探测噪音。
        if (hgl::shader::HasGeneratedPrefix(relative.c_str()))
            return false;

        return ReadShaderLibraryText(ToOSString(relative.c_str()), out_text);
    }
}
