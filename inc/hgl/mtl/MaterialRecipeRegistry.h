#pragma once

#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/type/ManagedArray.h>

namespace hgl::graph
{
    class TextureManager;
}

namespace hgl::graph::mtl
{
    class MaterialRecipeRegistry
    {
        ManagedArray<MaterialRecipeFileData> files;

        bool ResolveAndValidate(MaterialRecipeFileData &data,
                                hgl::graph::TextureManager *texture_manager);

    public:
        bool LoadFile(const OSString &path,
                      hgl::graph::TextureManager *texture_manager);
        bool LoadDirectory(const OSString &directory,
                           hgl::graph::TextureManager *texture_manager,
                           int *out_file_count = nullptr,
                           int *out_error_count = nullptr);
        bool RegisterRecipeSource(const os_char *label,
                                  const char *content,
                                  int content_size,
                                  hgl::graph::TextureManager *texture_manager);

        const MaterialRecipe *FindByName(const char *recipe_name) const;
        int GetCount() const { return files.GetCount(); }
        void Clear() { files.Clear(); }
    };
}
