#include <hgl/graph/mtl/BuiltinHelpers.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <cstring>

namespace hgl::graph::mtl {
namespace builtin_helpers {

enum class BuiltinHelperId : uint8_t {
    TransformNormal,
    GetWorldPos,
    GetCameraPos,
};

struct HelperNameAlias {
    const char *name;
};

struct HelperMeta {
    BuiltinHelperId id;
    HelperNameAlias aliases[3];
};

static constexpr HelperMeta HELPER_META_TABLE[] = {
    {BuiltinHelperId::TransformNormal, {{"TransformNormal"}, {nullptr}, {nullptr}}},
    {BuiltinHelperId::GetWorldPos,     {{"GetWorldPos"}, {"GetWorldPosition"}, {nullptr}}},
    {BuiltinHelperId::GetCameraPos,    {{"GetCameraPos"}, {"GetCameraPosition"}, {nullptr}}},
};

static const char* GetStageBusinessCode(const ComposedMaterialDef &def, const char *shader_stage)
{
    if (strcmp(shader_stage, "VS") == 0)
        return def.vertex_business ? def.vertex_business->code : nullptr;

    if (strcmp(shader_stage, "FS") == 0)
        return def.fragment_business ? def.fragment_business->code : nullptr;

    return nullptr;
}

static bool IsHelperUsed(const char *business_code, const char *helper_name)
{
    if (!business_code || !helper_name)
        return false;

    char token[128];
    snprintf(token, sizeof(token), "%s(", helper_name);

    return std::strstr(business_code, token) != nullptr;
}

static bool IsHelperUsed(const char *business_code, const BuiltinHelperId helper_id)
{
    for (const auto &meta : HELPER_META_TABLE)
    {
        if (meta.id != helper_id)
            continue;

        for (const auto &alias : meta.aliases)
        {
            if (!alias.name)
                break;

            if (IsHelperUsed(business_code, alias.name))
                return true;
        }
    }

    return false;
}

static bool IsHelperExplicitlyRequired(
    const ComposedMaterialDef &def,
    const char *shader_stage,
    const BuiltinHelperId helper_id)
{
    const char **required_helpers = nullptr;
    uint32_t required_count = 0;

    // Collect all explicit and logic-driven required helpers
    std::vector<std::string> all_required_helpers;

    // Explicit (metadata-driven)
    if (strcmp(shader_stage, "VS") == 0) {
        if (def.vertex_required_helpers && def.vertex_required_helper_count > 0) {
            for (uint32_t i = 0; i < def.vertex_required_helper_count; ++i) {
                if (def.vertex_required_helpers[i])
                    all_required_helpers.emplace_back(def.vertex_required_helpers[i]);
            }
        }
    } else if (strcmp(shader_stage, "FS") == 0) {
        if (def.fragment_required_helpers && def.fragment_required_helper_count > 0) {
            for (uint32_t i = 0; i < def.fragment_required_helper_count; ++i) {
                if (def.fragment_required_helpers[i])
                    all_required_helpers.emplace_back(def.fragment_required_helpers[i]);
            }
        }
    }

    // Logic-driven (from ShaderLogic.h)
    for (const auto& helper : def.logic_required_helpers) {
        all_required_helpers.emplace_back(helper);
    }

    if (all_required_helpers.empty())
        return false;

    for (const auto& required_name : all_required_helpers) {
        if (required_name.empty())
            continue;
        for (const auto &meta : HELPER_META_TABLE) {
            if (meta.id != helper_id)
                continue;
            for (const auto &alias : meta.aliases) {
                if (!alias.name)
                    break;
                if (required_name == alias.name)
                    return true;
            }
        }
    }
    return false;
}

static bool HasDescriptor(const ComposedMaterialDef &def, const char *name)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i) {
        const auto &entry = def.descriptor_entries[i];
        if (entry.name && strcmp(entry.name, name) == 0) {
            return true;
        }
    }
    return false;
}

static AnsiString GenTransformNormal()
{
    return R"(
vec3 TransformNormal(vec3 local_normal) {
    return normalize(GetNormalMatrix() * local_normal);
}

)";
}

static AnsiString GenGetWorldPos(const char *shader_stage)
{
    if (strcmp(shader_stage, "VS") == 0 || strcmp(shader_stage, "FS") == 0) {
        return R"(
vec3 GetWorldPos() {
    return GetPosition3D().xyz;
}

vec3 GetWorldPosition() {
    return GetWorldPos();
}

)";
    }

    return AnsiString();
}

static AnsiString GenGetCameraPos(const ComposedMaterialDef &def)
{
    if (HasDescriptor(def, "CameraPos")) {
        return R"(
vec3 GetCameraPos() {
    return CameraPos;
}

vec3 GetCameraPosition() {
    return GetCameraPos();
}

)";
    }

    if (HasDescriptor(def, "CameraPosition")) {
        return R"(
vec3 GetCameraPos() {
    return CameraPosition;
}

vec3 GetCameraPosition() {
    return GetCameraPos();
}

)";
    }

    if (HasDescriptor(def, "ViewPos")) {
        return R"(
vec3 GetCameraPos() {
    return ViewPos;
}

vec3 GetCameraPosition() {
    return GetCameraPos();
}

)";
    }

    return R"(
vec3 GetCameraPos() {
    return vec3(0.0, 0.0, 0.0);
}

vec3 GetCameraPosition() {
    return GetCameraPos();
}

)";
}

AnsiString GenStageHelpers(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const char *shader_stage)
{
    AnsiString result;

    const char *business_code = GetStageBusinessCode(def, shader_stage);

    const bool need_transform_normal =
        IsHelperExplicitlyRequired(def, shader_stage, BuiltinHelperId::TransformNormal)
        || IsHelperUsed(business_code, BuiltinHelperId::TransformNormal);

    const bool need_get_world_pos =
        IsHelperExplicitlyRequired(def, shader_stage, BuiltinHelperId::GetWorldPos)
        || IsHelperUsed(business_code, BuiltinHelperId::GetWorldPos);

    const bool need_get_camera_pos =
        IsHelperExplicitlyRequired(def, shader_stage, BuiltinHelperId::GetCameraPos)
        || IsHelperUsed(business_code, BuiltinHelperId::GetCameraPos);

    if (!(need_transform_normal || need_get_world_pos || need_get_camera_pos)) {
        (void)key;
        return result;
    }

    result += "// Builtin helper library\n";

    if (need_transform_normal)
        result += GenTransformNormal();

    if (need_get_world_pos)
        result += GenGetWorldPos(shader_stage);

    if (need_get_camera_pos)
        result += GenGetCameraPos(def);

    // 预留：后续根据排列键选择复杂 helper（PBR、阴影、雾效等）
    (void)key;

    return result;
}

} // namespace builtin_helpers
} // namespace hgl::graph::mtl
