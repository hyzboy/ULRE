#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    constexpr uint16 SurfaceProfileSchemaVersion = 1;

    using SurfaceProfileID = uint64;
    using SurfaceIntentID = uint64;
    using SurfaceProjectionID = uint64;

    constexpr SurfaceProfileID InvalidSurfaceProfileID = 0;
    constexpr SurfaceIntentID InvalidSurfaceIntentID = 0;
    constexpr SurfaceProjectionID InvalidSurfaceProjectionID = 0;

    inline uint64 GetSurfaceStableID(
        const char *value,
        const uint32 length) noexcept
    {
        if (!value || length == 0)
            return 0;

        return hgl::hash::FNV1aAppendBytes(
            hgl::hash::FNV1aInit<uint64>(), value, length);
    }

    inline uint64 GetSurfaceStableID(const AnsiString &value) noexcept
    {
        return GetSurfaceStableID(
            value.c_str(), static_cast<uint32>(value.Length()));
    }

    inline uint64 GetSurfaceStableID(const char *value) noexcept
    {
        if (!value || !value[0])
            return 0;

        uint32 length = 0;
        while (value[length])
            ++length;
        return GetSurfaceStableID(value, length);
    }

    struct SurfaceImplementationProfile
    {
        AnsiString profile_name;
        AnsiString parameter_schema_name;
        SurfaceProfileID profile_id = InvalidSurfaceProfileID;
        uint64 parameter_schema_id = 0;
        uint16 schema_version = SurfaceProfileSchemaVersion;
        uint16 quality_rank = 0;
    };

    struct SurfaceIntentDefinition
    {
        AnsiString intent_name;
        SurfaceIntentID intent_id = InvalidSurfaceIntentID;
        SurfaceProfileID preferred_profile_id = InvalidSurfaceProfileID;
    };

    struct SurfaceProfileDowngrade
    {
        SurfaceProfileID source_profile_id = InvalidSurfaceProfileID;
        SurfaceProfileID target_profile_id = InvalidSurfaceProfileID;
        uint16 selection_order = 0;
    };

    inline bool operator==(
        const SurfaceProfileDowngrade &lhs,
        const SurfaceProfileDowngrade &rhs) noexcept
    {
        return lhs.source_profile_id == rhs.source_profile_id
            && lhs.target_profile_id == rhs.target_profile_id
            && lhs.selection_order == rhs.selection_order;
    }

    struct MaterialSurfaceProfileProjection
    {
        SurfaceProfileID profile_id = InvalidSurfaceProfileID;
        SurfaceProjectionID projection_id = InvalidSurfaceProjectionID;
        uint16 projection_schema_version = SurfaceProfileSchemaVersion;
    };

    inline bool operator==(
        const MaterialSurfaceProfileProjection &lhs,
        const MaterialSurfaceProfileProjection &rhs) noexcept
    {
        return lhs.profile_id == rhs.profile_id
            && lhs.projection_id == rhs.projection_id
            && lhs.projection_schema_version
                == rhs.projection_schema_version;
    }

    inline MaterialSurfaceProfileProjection MakeMaterialSurfaceProfileProjection(
        const char *profile_name,
        const char *projection_name,
        const uint16 schema_version = SurfaceProfileSchemaVersion) noexcept
    {
        return {
            GetSurfaceStableID(profile_name),
            GetSurfaceStableID(projection_name),
            schema_version
        };
    }

    inline uint64 GetSurfaceImplementationProfileHash(
        const SurfaceImplementationProfile &profile) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(hash, profile.profile_id);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, profile.parameter_schema_id);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, profile.schema_version);
        return hgl::hash::FNV1aAppendValueBytes(hash, profile.quality_rank);
    }

    inline uint64 GetMaterialSurfaceProfileProjectionHash(
        const MaterialSurfaceProfileProjection &projection) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(hash, projection.profile_id);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, projection.projection_id);
        return hgl::hash::FNV1aAppendValueBytes(
            hash, projection.projection_schema_version);
    }
}
