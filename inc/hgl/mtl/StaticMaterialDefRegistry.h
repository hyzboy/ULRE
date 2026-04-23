#pragma once
#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/MaterialKey.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    /// Thread-safe singleton registry that assigns stable integer IDs to
    /// StaticMaterialDef instances, identified by content (not by pointer).
    class StaticMaterialDefRegistry
    {
    public:
        static StaticMaterialDefRegistry &Instance();

        /// Register def and return its ID.  If a def with the same content is
        /// already registered, returns the existing ID.
        StaticMaterialDefId Register(const StaticMaterialDef &def);

        /// Look up a def by ID.  Returns nullptr if id is invalid or unknown.
        const StaticMaterialDef *Get(StaticMaterialDefId id) const;

    private:
        StaticMaterialDefRegistry() = default;
        StaticMaterialDefRegistry(const StaticMaterialDefRegistry &) = delete;
        StaticMaterialDefRegistry &operator=(const StaticMaterialDefRegistry &) = delete;
    };

    /// Convenience free function: register def and return its ID.
    /// Equivalent to StaticMaterialDefRegistry::Instance().Register(def).
    StaticMaterialDefId AcquireStaticMaterialDefId(const StaticMaterialDef &def);

} // namespace hgl::graph::mtl
