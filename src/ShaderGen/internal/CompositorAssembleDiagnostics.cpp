#include <hgl/shadergen/internal/CompositorAssembleDiagnostics.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <algorithm>

namespace hgl::graph::internal {

std::string BuildCompositorReadFailureMessage(
    const char *stage,
    const std::string &template_path,
    const std::string &file_path,
    const std::string &reason)
{
    std::string msg;
    msg.reserve(256 + reason.size());
    msg += "[CompositorAssembler] ";
    msg += stage;
    msg += " template load failed. template=";
    msg += template_path.empty() ? "<auto-route>" : template_path;
    msg += " file=";
    msg += file_path;
    msg += " reason=";
    msg += reason;
    return msg;
}

std::string BuildCompositorPreprocessFailureMessage(
    const char *stage,
    const std::string &template_path,
    const std::string &detail,
    const std::string &glsl_source)
{
    std::string msg;
    msg.reserve(384 + detail.size() + std::min<size_t>(glsl_source.size(), 2048));
    msg += "[CompositorAssembler] ";
    msg += stage;
    msg += " preprocess failed. template=";
    msg += template_path.empty() ? "<auto-route>" : template_path;
    msg += " detail=";
    msg += detail;
    msg += "\n[";
    msg += stage;
    msg += " GLSL first 80 lines]\n";
    msg += BuildGLSLPreviewFirstLines(glsl_source, 80);
    return msg;
}

} // namespace hgl::graph::internal
