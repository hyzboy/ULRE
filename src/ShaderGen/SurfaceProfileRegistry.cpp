#include <hgl/mtl/SurfaceProfileRegistry.h>

#include <hgl/mtl/MaterialRecipe.h>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetSurfaceProfileFailure(
            SurfaceProfileValidationDiagnostic &out_diagnostic,
            const SurfaceProfileValidationError error,
            const AnsiString &subject_id,
            const AnsiString &related_id = {},
            const uint32 item_index = 0) noexcept
        {
            out_diagnostic.error = error;
            out_diagnostic.subject_id = subject_id;
            out_diagnostic.related_id = related_id;
            out_diagnostic.item_index = item_index;
            return false;
        }

        bool IsValidIdentifier(const AnsiString &value) noexcept
        {
            if (value.IsEmpty())
                return false;

            const char *text = value.c_str();
            const int length = value.Length();
            for (int i = 0; i < length; ++i)
            {
                const char c = text[i];
                if ((c >= 'a' && c <= 'z')
                 || (c >= 'A' && c <= 'Z')
                 || (c >= '0' && c <= '9')
                 || c == '_'
                 || c == '.'
                 || c == '-')
                    continue;

                return false;
            }

            return true;
        }

        int FindProfileIndex(
            const SurfaceProfileRegistry &registry,
            const SurfaceProfileID profile_id) noexcept
        {
            for (int i = 0; i < registry.GetProfileCount(); ++i)
            {
                const SurfaceImplementationProfile *profile =
                    registry.GetProfileByIndex(i);
                if (profile && profile->profile_id == profile_id)
                    return i;
            }

            return -1;
        }

        bool VisitProfile(
            const SurfaceProfileRegistry &registry,
            const int profile_index,
            ValueArray<uint8> &visit_state,
            SurfaceProfileValidationDiagnostic &out_diagnostic) noexcept
        {
            if (visit_state[profile_index] == 2)
                return true;
            if (visit_state[profile_index] == 1)
                return false;

            visit_state[profile_index] = 1;
            const SurfaceImplementationProfile *profile =
                registry.GetProfileByIndex(profile_index);
            if (!profile)
                return false;

            for (int i = 0; i < registry.GetDowngradeCount(); ++i)
            {
                const SurfaceProfileDowngrade *edge =
                    registry.GetDowngradeByIndex(i);
                if (!edge || edge->source_profile_id != profile->profile_id)
                    continue;

                const int target_index =
                    FindProfileIndex(registry, edge->target_profile_id);
                if (target_index < 0)
                    continue;

                if (visit_state[target_index] == 1)
                {
                    const SurfaceImplementationProfile *target =
                        registry.FindProfile(edge->target_profile_id);
                    return SetSurfaceProfileFailure(
                        out_diagnostic,
                        SurfaceProfileValidationError::DowngradeCycle,
                        profile->profile_name,
                        target ? target->profile_name : AnsiString(),
                        static_cast<uint32>(i));
                }

                if (!VisitProfile(
                        registry,
                        target_index,
                        visit_state,
                        out_diagnostic))
                    return false;
            }

            visit_state[profile_index] = 2;
            return true;
        }

        template<typename T, typename Compare>
        void StableInsertionSort(
            ValueArray<const T *> &values,
            const Compare &compare)
        {
            for (int i = 1; i < values.GetCount(); ++i)
            {
                const T *value = values[i];
                int insert_at = i;
                while (insert_at > 0
                    && compare(value, values[insert_at - 1]) < 0)
                {
                    values[insert_at] = values[insert_at - 1];
                    --insert_at;
                }
                values[insert_at] = value;
            }
        }
    }

    const char *GetSurfaceProfileValidationErrorName(
        const SurfaceProfileValidationError error) noexcept
    {
        switch (error)
        {
        case SurfaceProfileValidationError::None: return "None";
        case SurfaceProfileValidationError::InvalidProfile: return "InvalidProfile";
        case SurfaceProfileValidationError::InvalidIntent: return "InvalidIntent";
        case SurfaceProfileValidationError::InvalidDowngrade: return "InvalidDowngrade";
        case SurfaceProfileValidationError::MissingProfile: return "MissingProfile";
        case SurfaceProfileValidationError::NonDecreasingQuality: return "NonDecreasingQuality";
        case SurfaceProfileValidationError::DuplicateDowngrade: return "DuplicateDowngrade";
        case SurfaceProfileValidationError::DuplicateDowngradeOrder: return "DuplicateDowngradeOrder";
        case SurfaceProfileValidationError::DowngradeCycle: return "DowngradeCycle";
        case SurfaceProfileValidationError::InvalidProjection: return "InvalidProjection";
        case SurfaceProfileValidationError::DuplicateProjection: return "DuplicateProjection";
        case SurfaceProfileValidationError::MissingPreferredProjection: return "MissingPreferredProjection";
        case SurfaceProfileValidationError::UnreachableProjection: return "UnreachableProjection";
        }

        return "Unknown";
    }

    bool SurfaceProfileRegistry::RegisterProfile(
        const SurfaceImplementationProfile &profile)
    {
        if (!IsValidIdentifier(profile.profile_name)
         || !IsValidIdentifier(profile.parameter_schema_name)
         || profile.profile_id == InvalidSurfaceProfileID
         || profile.parameter_schema_id == 0
         || profile.profile_id != GetSurfaceStableID(profile.profile_name)
         || profile.parameter_schema_id
                != GetSurfaceStableID(profile.parameter_schema_name)
         || profile.schema_version == 0
         || profile.schema_version > SurfaceProfileSchemaVersion
         || profiles_by_id.ContainsKey(profile.profile_name)
         || profiles_by_stable_id.ContainsKey(profile.profile_id))
            return false;

        SurfaceImplementationProfile *stored = profiles.Create();
        if (!stored)
            return false;

        *stored = profile;
        if (!profiles_by_id.Add(stored->profile_name, stored)
         || !profiles_by_stable_id.Add(stored->profile_id, stored))
        {
            profiles_by_id.DeleteByKey(stored->profile_name);
            profiles_by_stable_id.DeleteByKey(stored->profile_id);
            profiles.DeleteAt(profiles.GetCount() - 1);
            return false;
        }

        return true;
    }

    bool SurfaceProfileRegistry::RegisterIntent(
        const SurfaceIntentDefinition &intent)
    {
        if (!IsValidIdentifier(intent.intent_name)
         || intent.intent_id == InvalidSurfaceIntentID
         || intent.intent_id != GetSurfaceStableID(intent.intent_name)
         || !FindProfile(intent.preferred_profile_id)
         || intents_by_id.ContainsKey(intent.intent_name)
         || intents_by_stable_id.ContainsKey(intent.intent_id))
            return false;

        SurfaceIntentDefinition *stored = intents.Create();
        if (!stored)
            return false;

        *stored = intent;
        if (!intents_by_id.Add(stored->intent_name, stored)
         || !intents_by_stable_id.Add(stored->intent_id, stored))
        {
            intents_by_id.DeleteByKey(stored->intent_name);
            intents_by_stable_id.DeleteByKey(stored->intent_id);
            intents.DeleteAt(intents.GetCount() - 1);
            return false;
        }

        return true;
    }

    bool SurfaceProfileRegistry::RegisterDowngrade(
        const SurfaceProfileDowngrade &downgrade)
    {
        const SurfaceImplementationProfile *source =
            FindProfile(downgrade.source_profile_id);
        const SurfaceImplementationProfile *target =
            FindProfile(downgrade.target_profile_id);
        if (!source
         || !target
         || source == target
         || source->quality_rank <= target->quality_rank)
            return false;

        for (int i = 0; i < downgrades.GetCount(); ++i)
        {
            const SurfaceProfileDowngrade &existing = downgrades[i];
            if (existing.source_profile_id == downgrade.source_profile_id
             && existing.target_profile_id == downgrade.target_profile_id)
                return false;
            if (existing.source_profile_id == downgrade.source_profile_id
             && existing.selection_order == downgrade.selection_order)
                return false;
        }

        downgrades.Add(downgrade);
        return true;
    }

    const SurfaceImplementationProfile *SurfaceProfileRegistry::FindProfile(
        const char *profile_name) const
    {
        if (!profile_name || !profile_name[0])
            return nullptr;

        const SurfaceImplementationProfile *profile = nullptr;
        profiles_by_id.Get(AnsiString(profile_name), profile);
        return profile;
    }

    const SurfaceImplementationProfile *SurfaceProfileRegistry::FindProfile(
        const SurfaceProfileID profile_id) const
    {
        if (profile_id == InvalidSurfaceProfileID)
            return nullptr;

        const SurfaceImplementationProfile *profile = nullptr;
        profiles_by_stable_id.Get(profile_id, profile);
        return profile;
    }

    const SurfaceIntentDefinition *SurfaceProfileRegistry::FindIntent(
        const char *intent_name) const
    {
        if (!intent_name || !intent_name[0])
            return nullptr;

        const SurfaceIntentDefinition *intent = nullptr;
        intents_by_id.Get(AnsiString(intent_name), intent);
        return intent;
    }

    const SurfaceIntentDefinition *SurfaceProfileRegistry::FindIntent(
        const SurfaceIntentID intent_id) const
    {
        if (intent_id == InvalidSurfaceIntentID)
            return nullptr;

        const SurfaceIntentDefinition *intent = nullptr;
        intents_by_stable_id.Get(intent_id, intent);
        return intent;
    }

    const SurfaceImplementationProfile *
    SurfaceProfileRegistry::GetProfileByIndex(const int index) const noexcept
    {
        return index >= 0 && index < profiles.GetCount()
            ? profiles[index]
            : nullptr;
    }

    const SurfaceIntentDefinition *
    SurfaceProfileRegistry::GetIntentByIndex(const int index) const noexcept
    {
        return index >= 0 && index < intents.GetCount()
            ? intents[index]
            : nullptr;
    }

    const SurfaceProfileDowngrade *
    SurfaceProfileRegistry::GetDowngradeByIndex(const int index) const noexcept
    {
        return index >= 0 && index < downgrades.GetCount()
            ? &downgrades[index]
            : nullptr;
    }

    bool SurfaceProfileRegistry::CanDowngrade(
        const SurfaceProfileID source_profile_id,
        const SurfaceProfileID target_profile_id) const noexcept
    {
        const int source_index = FindProfileIndex(*this, source_profile_id);
        const int target_index = FindProfileIndex(*this, target_profile_id);
        if (source_index < 0 || target_index < 0)
            return false;
        if (source_index == target_index)
            return true;

        ValueArray<uint8> visited;
        ValueArray<int> pending;
        visited.Resize(GetProfileCount());
        pending.Resize(GetProfileCount());
        for (int i = 0; i < visited.GetCount(); ++i)
            visited[i] = 0;

        int pending_count = 1;
        pending[0] = source_index;
        while (pending_count > 0)
        {
            const int current = pending[--pending_count];
            if (visited[current])
                continue;
            visited[current] = 1;

            const SurfaceImplementationProfile *profile =
                GetProfileByIndex(current);
            if (!profile)
                continue;

            for (int i = 0; i < GetDowngradeCount(); ++i)
            {
                const SurfaceProfileDowngrade *edge =
                    GetDowngradeByIndex(i);
                if (!edge || edge->source_profile_id != profile->profile_id)
                    continue;

                const int next =
                    FindProfileIndex(*this, edge->target_profile_id);
                if (next == target_index)
                    return true;
                if (next >= 0
                 && !visited[next]
                 && pending_count < pending.GetCount())
                    pending[pending_count++] = next;
            }
        }

        return false;
    }

    bool SurfaceProfileRegistry::Validate(
        SurfaceProfileValidationDiagnostic &out_diagnostic) const noexcept
    {
        out_diagnostic = {};

        for (int i = 0; i < GetProfileCount(); ++i)
        {
            const SurfaceImplementationProfile *profile = GetProfileByIndex(i);
            if (!profile
             || !IsValidIdentifier(profile->profile_name)
             || !IsValidIdentifier(profile->parameter_schema_name)
             || profile->profile_id != GetSurfaceStableID(profile->profile_name)
             || profile->parameter_schema_id
                    != GetSurfaceStableID(profile->parameter_schema_name)
             || profile->schema_version == 0
             || profile->schema_version > SurfaceProfileSchemaVersion)
            {
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::InvalidProfile,
                    profile ? profile->profile_name : AnsiString(),
                    {},
                    static_cast<uint32>(i));
            }
        }

        for (int i = 0; i < GetIntentCount(); ++i)
        {
            const SurfaceIntentDefinition *intent = GetIntentByIndex(i);
            if (!intent
             || !IsValidIdentifier(intent->intent_name)
             || intent->intent_id != GetSurfaceStableID(intent->intent_name))
            {
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::InvalidIntent,
                    intent ? intent->intent_name : AnsiString(),
                    {},
                    static_cast<uint32>(i));
            }

            if (!FindProfile(intent->preferred_profile_id))
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::MissingProfile,
                    intent->intent_name,
                    {},
                    static_cast<uint32>(i));
        }

        for (int i = 0; i < GetDowngradeCount(); ++i)
        {
            const SurfaceProfileDowngrade *edge = GetDowngradeByIndex(i);
            const SurfaceImplementationProfile *source = edge
                ? FindProfile(edge->source_profile_id) : nullptr;
            const SurfaceImplementationProfile *target = edge
                ? FindProfile(edge->target_profile_id) : nullptr;
            if (!edge || !source || !target)
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::MissingProfile,
                    source ? source->profile_name : AnsiString(),
                    target ? target->profile_name : AnsiString(),
                    static_cast<uint32>(i));
            if (source == target)
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::InvalidDowngrade,
                    source->profile_name,
                    target->profile_name,
                    static_cast<uint32>(i));
            if (source->quality_rank <= target->quality_rank)
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::NonDecreasingQuality,
                    source->profile_name,
                    target->profile_name,
                    static_cast<uint32>(i));
        }

        ValueArray<uint8> visit_state;
        visit_state.Resize(GetProfileCount());
        for (int i = 0; i < visit_state.GetCount(); ++i)
            visit_state[i] = 0;

        for (int i = 0; i < GetProfileCount(); ++i)
        {
            if (visit_state[i] == 0
             && !VisitProfile(*this, i, visit_state, out_diagnostic))
                return false;
        }

        return true;
    }

    uint64 SurfaceProfileRegistry::GetStableHash() const noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, SurfaceProfileSchemaVersion);

        ValueArray<const SurfaceImplementationProfile *> sorted_profiles;
        for (int i = 0; i < GetProfileCount(); ++i)
            sorted_profiles.Add(GetProfileByIndex(i));
        StableInsertionSort(
            sorted_profiles,
            [](const SurfaceImplementationProfile *lhs,
               const SurfaceImplementationProfile *rhs)
            {
                return lhs->profile_id < rhs->profile_id
                    ? -1 : lhs->profile_id > rhs->profile_id ? 1 : 0;
            });

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(sorted_profiles.GetCount()));
        for (int i = 0; i < sorted_profiles.GetCount(); ++i)
        {
            const SurfaceImplementationProfile &profile = *sorted_profiles[i];
            hash = hgl::hash::FNV1aAppendValueBytes(hash, profile.profile_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, profile.parameter_schema_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, profile.schema_version);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, profile.quality_rank);
        }

        ValueArray<const SurfaceIntentDefinition *> sorted_intents;
        for (int i = 0; i < GetIntentCount(); ++i)
            sorted_intents.Add(GetIntentByIndex(i));
        StableInsertionSort(
            sorted_intents,
            [](const SurfaceIntentDefinition *lhs,
               const SurfaceIntentDefinition *rhs)
            {
                return lhs->intent_id < rhs->intent_id
                    ? -1 : lhs->intent_id > rhs->intent_id ? 1 : 0;
            });

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(sorted_intents.GetCount()));
        for (int i = 0; i < sorted_intents.GetCount(); ++i)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, sorted_intents[i]->intent_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, sorted_intents[i]->preferred_profile_id);
        }

        ValueArray<const SurfaceProfileDowngrade *> sorted_edges;
        for (int i = 0; i < GetDowngradeCount(); ++i)
            sorted_edges.Add(GetDowngradeByIndex(i));
        StableInsertionSort(
            sorted_edges,
            [](const SurfaceProfileDowngrade *lhs,
               const SurfaceProfileDowngrade *rhs)
            {
                if (lhs->source_profile_id != rhs->source_profile_id)
                    return lhs->source_profile_id < rhs->source_profile_id
                        ? -1 : 1;
                if (lhs->selection_order != rhs->selection_order)
                    return lhs->selection_order < rhs->selection_order ? -1 : 1;
                return lhs->target_profile_id < rhs->target_profile_id
                    ? -1 : lhs->target_profile_id > rhs->target_profile_id
                        ? 1 : 0;
            });

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(sorted_edges.GetCount()));
        for (int i = 0; i < sorted_edges.GetCount(); ++i)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, sorted_edges[i]->source_profile_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, sorted_edges[i]->target_profile_id);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, sorted_edges[i]->selection_order);
        }

        return hash;
    }

    void SurfaceProfileRegistry::Clear()
    {
        profiles_by_id.Clear();
        profiles_by_stable_id.Clear();
        intents_by_id.Clear();
        intents_by_stable_id.Clear();
        profiles.Clear();
        intents.Clear();
        downgrades.Clear();
    }

    bool ValidateMaterialSurfaceProfileProjections(
        const MaterialDefinition &definition,
        const SurfaceProfileRegistry &registry,
        SurfaceProfileValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};

        if (definition.surface_intent_id == InvalidSurfaceIntentID)
        {
            if (definition.surface_profile_projections.IsEmpty())
                return true;

            return SetSurfaceProfileFailure(
                out_diagnostic,
                SurfaceProfileValidationError::InvalidProjection,
                {});
        }

        const SurfaceIntentDefinition *intent =
            registry.FindIntent(definition.surface_intent_id);
        if (!intent)
            return SetSurfaceProfileFailure(
                out_diagnostic,
                SurfaceProfileValidationError::InvalidIntent,
                {});

        bool has_preferred_projection = false;
        for (int i = 0;
             i < definition.surface_profile_projections.GetCount();
             ++i)
        {
            const MaterialSurfaceProfileProjection &projection =
                definition.surface_profile_projections[i];
            const SurfaceImplementationProfile *profile =
                registry.FindProfile(projection.profile_id);
            if (!profile
             || projection.projection_id == InvalidSurfaceProjectionID
             || projection.projection_schema_version == 0
             || projection.projection_schema_version
                    > SurfaceProfileSchemaVersion)
            {
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::InvalidProjection,
                    intent->intent_name,
                    profile ? profile->profile_name : AnsiString(),
                    static_cast<uint32>(i));
            }

            for (int j = 0; j < i; ++j)
            {
                if (projection.profile_id
                    == definition.surface_profile_projections[j].profile_id)
                {
                    return SetSurfaceProfileFailure(
                        out_diagnostic,
                        SurfaceProfileValidationError::DuplicateProjection,
                        intent->intent_name,
                        profile->profile_name,
                        static_cast<uint32>(i));
                }
            }

            if (projection.profile_id == intent->preferred_profile_id)
                has_preferred_projection = true;
            else if (!registry.CanDowngrade(
                        intent->preferred_profile_id,
                        projection.profile_id))
            {
                return SetSurfaceProfileFailure(
                    out_diagnostic,
                    SurfaceProfileValidationError::UnreachableProjection,
                    intent->intent_name,
                    profile->profile_name,
                    static_cast<uint32>(i));
            }
        }

        if (!has_preferred_projection)
        {
            const SurfaceImplementationProfile *preferred =
                registry.FindProfile(intent->preferred_profile_id);
            return SetSurfaceProfileFailure(
                out_diagnostic,
                SurfaceProfileValidationError::MissingPreferredProjection,
                intent->intent_name,
                preferred ? preferred->profile_name : AnsiString());
        }

        return true;
    }
}
