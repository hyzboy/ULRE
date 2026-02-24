#include <hgl/graph/mtl/BuiltinHelpers.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <string.h>

namespace hgl::graph::mtl {
namespace builtin_helpers {

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

)";
    }

    if (HasDescriptor(def, "CameraPosition")) {
        return R"(
vec3 GetCameraPos() {
    return CameraPosition;
}

)";
    }

    if (HasDescriptor(def, "ViewPos")) {
        return R"(
vec3 GetCameraPos() {
    return ViewPos;
}

)";
    }

    return R"(
vec3 GetCameraPos() {
    return vec3(0.0, 0.0, 0.0);
}

)";
}

AnsiString GenStageHelpers(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const char *shader_stage)
{
    AnsiString result;

    result += "// Builtin helper library\n";
    result += GenTransformNormal();
    result += GenGetWorldPos(shader_stage);
    result += GenGetCameraPos(def);

    // 预留：后续根据排列键选择复杂 helper（PBR、阴影、雾效等）
    (void)key;

    return result;
}

} // namespace builtin_helpers
} // namespace hgl::graph::mtl
