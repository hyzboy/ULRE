#pragma once

#include <hgl/mtl/SurfaceProfileRegistry.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::mtl
{
    enum class SurfaceProfileFileParseResult : uint8
    {
        Skipped = 0,
        OK,
        InvalidValue,
        UnsupportedSchema
    };

    struct SurfaceProfileFileData
    {
        ManagedArray<SurfaceImplementationProfile> profiles;
        ManagedArray<SurfaceIntentDefinition> intents;
        ValueArray<SurfaceProfileDowngrade> downgrades;
    };

    const char *GetSurfaceProfileFileParseResultName(
        SurfaceProfileFileParseResult result) noexcept;

    SurfaceProfileFileParseResult ParseSurfaceProfileFile(
        const char *content,
        int content_size,
        SurfaceProfileFileData &out_data) noexcept;

    bool LoadSurfaceProfileFile(
        const OSString &path,
        SurfaceProfileFileData &out_data);

    bool RegisterSurfaceProfileFileData(
        const SurfaceProfileFileData &data,
        SurfaceProfileRegistry &registry,
        SurfaceProfileValidationDiagnostic &out_diagnostic);
}
