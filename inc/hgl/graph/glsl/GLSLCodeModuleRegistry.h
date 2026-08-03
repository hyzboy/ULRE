#pragma once

#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/type/UnorderedMap.h>

namespace hgl::graph::mtl
{
    class GLSLCodeModuleRegistry
    {
        UnorderedMap<GLSLCodeModuleID, const GLSLCodeModuleDefinition *> modules;

    public:
        bool Register(const GLSLCodeModuleDefinition &definition);
        bool RegisterBuiltinModules();

        const GLSLCodeModuleDefinition *Find(const GLSLCodeModuleID id) const;
        const GLSLCodeModuleDefinition *FindByName(const char *name) const;

        int GetCount() const { return modules.GetCount(); }
        void Clear() { modules.Clear(); }
    };
}
