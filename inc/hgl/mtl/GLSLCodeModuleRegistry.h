#pragma once

#include <hgl/mtl/GLSLCodeModule.h>
#include <hgl/mtl/GLSLCodeModuleFile.h>
#include <hgl/type/ManagedArray.h>
#include <vector>

namespace hgl::graph::mtl
{
    class GLSLCodeModuleRegistry
    {
        // Registered modules in registration order. Identity is the module
        // name (unique per registry); lookups are linear over a small set
        // (~tens of modules), avoiding a second identity track.
        std::vector<const GLSLCodeModuleDefinition *> modules;

        // File-backed module payloads. Entries are heap objects with stable
        // addresses; GLSLCodeModuleDefinition pointers inside each entry point
        // into the entry itself.
        ManagedArray<GLSLCodeModuleFileData> file_data;

    public:
        bool Register(const GLSLCodeModuleDefinition &definition);

        /**
         * Recursively scan a directory for `.glsl` files and register every
         * file that carries a `// @ulre` metadata block.
         *
         * @param directory    Root directory to scan recursively.
         * @param out_file_count  Receives the number of registered modules
         *                        (may be nullptr).
         * @param out_error_count Receives the number of files skipped due to
         *                        read/parse/duplicate errors (may be nullptr).
         * @return false when the directory could not be scanned.
         */
        bool LoadDirectory(const OSString &directory,
                           int *out_file_count = nullptr,
                           int *out_error_count = nullptr);

        const GLSLCodeModuleDefinition *FindByName(const char *name) const;

        /// Remove the module registered under `name` (used when a module's
        /// dependency/conflict graph is incomplete). Returns false when absent.
        bool RemoveByName(const char *name);

        int GetCount() const { return static_cast<int>(modules.size()); }

        /**
         * Access a module by iteration index. The order is not guaranteed to be
         * stable across calls; consumers that need determinism must sort.
         */
        const GLSLCodeModuleDefinition *GetModuleByIndex(const int index) const;
        void Clear() { modules.clear(); file_data.Clear(); }
    };

    // 全局 GLSL 模块注册表单例（懒加载，首次调用时扫描 ShaderLibrary）。
    // 原挂在 MaterialDefinitionRegistry 上（生命周期被材质注册表"劫持"）——
    // 2026-08-31 下沉回 glsl_module 自身，材质注册表只保留自己的文件注册表。
    GLSLCodeModuleRegistry &GetGLSLCodeModuleRegistry();
}
