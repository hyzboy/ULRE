#pragma once

#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/graph/glsl/GLSLCodeModuleFile.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/UnorderedMap.h>

namespace hgl::graph::mtl
{
    class GLSLCodeModuleRegistry
    {
        UnorderedMap<GLSLCodeModuleID, const GLSLCodeModuleDefinition *> modules;

        // File-backed module payloads. Entries are heap objects with stable
        // addresses; GLSLCodeModuleDefinition pointers inside each entry point
        // into the entry itself.
        ManagedArray<GLSLCodeModuleFileData> file_data;

        uint16 next_file_id = static_cast<uint16>(GLSLCodeModuleID::RANGE_SIZE);

    public:
        bool Register(const GLSLCodeModuleDefinition &definition);
        bool RegisterBuiltinModules();

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

        const GLSLCodeModuleDefinition *Find(const GLSLCodeModuleID id) const;
        const GLSLCodeModuleDefinition *FindByName(const char *name) const;

        int GetCount() const { return modules.GetCount(); }

        /**
         * Access a module by iteration index. The order is not guaranteed to be
         * stable across calls; consumers that need determinism must sort.
         */
        const GLSLCodeModuleDefinition *GetModuleByIndex(const int index) const;
        void Clear() { modules.Clear(); file_data.Clear(); next_file_id = static_cast<uint16>(GLSLCodeModuleID::RANGE_SIZE); }
    };
}
