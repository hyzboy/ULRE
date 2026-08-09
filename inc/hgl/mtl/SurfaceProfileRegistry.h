#pragma once

#include <hgl/mtl/SurfaceProfile.h>
#include <hgl/type/ManagedArray.h>
#include <hgl/type/UnorderedMap.h>

namespace hgl::graph::mtl
{
    struct MaterialDefinition;

    enum class SurfaceProfileValidationError : uint8
    {
        None = 0,
        InvalidProfile,
        InvalidIntent,
        InvalidDowngrade,
        MissingProfile,
        NonDecreasingQuality,
        DuplicateDowngrade,
        DuplicateDowngradeOrder,
        DowngradeCycle,
        InvalidProjection,
        DuplicateProjection,
        MissingPreferredProjection,
        UnreachableProjection
    };

    struct SurfaceProfileValidationDiagnostic
    {
        SurfaceProfileValidationError error =
            SurfaceProfileValidationError::None;
        AnsiString subject_id;
        AnsiString related_id;
        uint32 item_index = 0;
    };

    const char *GetSurfaceProfileValidationErrorName(
        SurfaceProfileValidationError error) noexcept;

    class SurfaceProfileRegistry
    {
        ManagedArray<SurfaceImplementationProfile> profiles;
        ManagedArray<SurfaceIntentDefinition> intents;
        ValueArray<SurfaceProfileDowngrade> downgrades;
        UnorderedMap<AnsiString, const SurfaceImplementationProfile *>
            profiles_by_id;
        UnorderedMap<SurfaceProfileID, const SurfaceImplementationProfile *>
            profiles_by_stable_id;
        UnorderedMap<AnsiString, const SurfaceIntentDefinition *>
            intents_by_id;
        UnorderedMap<SurfaceIntentID, const SurfaceIntentDefinition *>
            intents_by_stable_id;

    public:
        bool RegisterProfile(const SurfaceImplementationProfile &profile);
        bool RegisterIntent(const SurfaceIntentDefinition &intent);
        bool RegisterDowngrade(const SurfaceProfileDowngrade &downgrade);

        const SurfaceImplementationProfile *FindProfile(
            const char *profile_id) const;
        const SurfaceImplementationProfile *FindProfile(
            SurfaceProfileID profile_id) const;
        const SurfaceIntentDefinition *FindIntent(
            const char *intent_id) const;
        const SurfaceIntentDefinition *FindIntent(
            SurfaceIntentID intent_id) const;

        int GetProfileCount() const noexcept { return profiles.GetCount(); }
        int GetIntentCount() const noexcept { return intents.GetCount(); }
        int GetDowngradeCount() const noexcept { return downgrades.GetCount(); }

        const SurfaceImplementationProfile *GetProfileByIndex(
            int index) const noexcept;
        const SurfaceIntentDefinition *GetIntentByIndex(
            int index) const noexcept;
        const SurfaceProfileDowngrade *GetDowngradeByIndex(
            int index) const noexcept;

        bool CanDowngrade(
            SurfaceProfileID source_profile_id,
            SurfaceProfileID target_profile_id) const noexcept;

        bool Validate(
            SurfaceProfileValidationDiagnostic &out_diagnostic) const noexcept;

        uint64 GetStableHash() const noexcept;
        void Clear();
    };

    bool ValidateMaterialSurfaceProfileProjections(
        const MaterialDefinition &definition,
        const SurfaceProfileRegistry &registry,
        SurfaceProfileValidationDiagnostic &out_diagnostic) noexcept;
}
