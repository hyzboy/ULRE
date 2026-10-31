// ULRE.ShaderRuntimeContract 的锚点翻译单元。
//
// 本目标下所有头都是 header-only，没有需要落 .cpp 的实现。保留这个 TU 有两个作用：
//   1. 让 add_library 有真实源文件（空静态库在部分工具链上是退化情形）
//   2. 充当「契约头必须自包含、且不得引入重依赖」的编译期哨兵 ——
//      任何契约头一旦偷偷拉进 GLSL 处理 / 文件 IO / 编译器插件，这里会最先炸。
#include <hgl/mtl/ShaderProgramKey.h>
#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/mtl/ShaderArtifactContract.h>
#include <hgl/mtl/MeshShaderLimits.h>
#include <hgl/mtl/ShaderCompilerProfileAPI.h>
#include <hgl/mtl/contract/ShaderGenContract.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/mtl/contract/ShaderGenPhysicalDeviceProfileAdapter.h>
#include <hgl/mtl/contract/ShaderGenPhysicalDeviceProfileJson.h>

#include <cstddef>

namespace
{
    // 把身份类型的字段布局钉死。烘焙端（ShaderGen / 离线工具）写出的 Manifest
    // 与运行时的读法必须对同一份结构解释一致；字段一旦增删或换序，这里立刻断编译，
    // 而不是留到「烘焙端写、运行端读」时静默错位。真要改布局，请同步升级
    // ShaderGenProfileTargetVersion.h 里的 MATERIAL_CODEGEN_GENERATION。
    static_assert(sizeof(hgl::graph::mtl::ShaderProgramKey) == 3 * sizeof(hgl::uint64),
                  "ShaderProgramKey 布局属于 Manifest 身份契约");
    static_assert(offsetof(hgl::graph::mtl::ShaderProgramKey, link_contract_hash)
                      == 2 * sizeof(hgl::uint64),
                  "ShaderProgramKey 字段顺序属于 Manifest 身份契约");
    static_assert(offsetof(hgl::graph::mtl::ShaderStageKey, definition_hash) == 8,
                  "ShaderStageKey 首字段之后必须紧跟 definition_hash");
    static_assert(offsetof(hgl::graph::mtl::ShaderStageKey, compiler_hash) == 32,
                  "ShaderStageKey 末字段必须是 compiler_hash");
}
