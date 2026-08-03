#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>

#include <cstring>

namespace hgl::graph::mtl
{
    bool GLSLCodeModuleRegistry::Register(const GLSLCodeModuleDefinition &definition)
    {
        if (!IsValidGLSLCodeModuleDefinition(definition))
            return false;

        if (modules.ContainsKey(definition.id))
            return false;

        return modules.Add(definition.id, &definition);
    }

    bool GLSLCodeModuleRegistry::RegisterBuiltinModules()
    {
        for (uint32 i = 0; i < static_cast<uint32>(GLSLCodeModuleID::RANGE_SIZE); ++i)
        {
            const auto id = static_cast<GLSLCodeModuleID>(i);
            const auto *definition = FindGLSLCodeModuleDefinition(id);
            if (!definition || !Register(*definition))
                return false;
        }

        return true;
    }

    const GLSLCodeModuleDefinition *GLSLCodeModuleRegistry::Find(const GLSLCodeModuleID id) const
    {
        const GLSLCodeModuleDefinition *definition = nullptr;
        modules.Get(id, definition);
        return definition;
    }

    const GLSLCodeModuleDefinition *GLSLCodeModuleRegistry::FindByName(const char *name) const
    {
        if (!name || !*name)
            return nullptr;

        for (uint32 i = 0; i < static_cast<uint32>(GLSLCodeModuleID::RANGE_SIZE); ++i)
        {
            const auto *definition = Find(static_cast<GLSLCodeModuleID>(i));
            if (definition && definition->name && std::strcmp(definition->name, name) == 0)
                return definition;
        }

        return nullptr;
    }
}
