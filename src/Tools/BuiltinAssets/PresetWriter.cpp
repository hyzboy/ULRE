#include "PresetWriter.h"

#include <hgl/serialize/ArchiveFile.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/log/Log.h>

namespace hgl::graph::builtin
{
    PresetWriter::PresetWriter(const OSString &in_output_dir, bool in_export_toml, bool in_export_flat)
        : output_dir(in_output_dir)
        , export_toml(in_export_toml)
        , export_flat(in_export_flat)
    {
    }

    OSString PresetWriter::MakeFilePath(const os_char *subdir, const os_char *stem,
                                        const os_char *extension, bool with_toml_suffix) const
    {
        OSString path = output_dir;

        if (subdir && subdir[0] != 0)
        {
            if (!path.EndsWith(OS_TEXT("/")) && !path.EndsWith(OS_TEXT("\\")))
                path += OS_TEXT("/");

            path += subdir;
        }

        if (!path.EndsWith(OS_TEXT("/")) && !path.EndsWith(OS_TEXT("\\")))
            path += OS_TEXT("/");

        path += stem;
        path += extension;

        if (with_toml_suffix)
            path += OS_TEXT(".toml");

        return path;
    }

    bool PresetWriter::EnsureDirectory(const os_char *subdir)
    {
        if (dir_ready)
            return true;

        if (output_dir.IsEmpty())
            return false;

        OSString dir = output_dir;

        if (subdir && subdir[0] != 0)
        {
            if (!dir.EndsWith(OS_TEXT("/")) && !dir.EndsWith(OS_TEXT("\\")))
                dir += OS_TEXT("/");

            dir += subdir;
        }

        hgl::filesystem::MakePath(dir);
        dir_ready = true;
        return true;
    }

    bool PresetWriter::Save(hgl::asset::AssetObject &asset, const os_char *subdir,
                            const os_char *stem, const os_char *extension)
    {
        if (!EnsureDirectory(subdir))
        {
            GLogError(u8"[BuiltinAsset] 输出目录为空，拒绝导出 %ls", stem);
            return false;
        }

        AnsiString error;

        if (export_toml)
        {
            const OSString path = MakeFilePath(subdir, stem, extension, true);
            if (!hgl::serialize::SaveAssetObjectToFile(asset, path,
                                                       hgl::serialize::ArchiveFormat::Auto, &error))
            {
                GLogError(u8"[BuiltinAsset] 导出文本格式失败: %ls (%s)", path.c_str(), error.c_str());
                return false;
            }
        }

        if (export_flat)
        {
            const OSString path = MakeFilePath(subdir, stem, extension, false);
            if (!hgl::serialize::SaveAssetObjectToFile(asset, path,
                                                       hgl::serialize::ArchiveFormat::Auto, &error))
            {
                GLogError(u8"[BuiltinAsset] 导出二进制格式失败: %ls (%s)", path.c_str(), error.c_str());
                return false;
            }
        }

        return true;
    }
}//namespace hgl::graph::builtin
