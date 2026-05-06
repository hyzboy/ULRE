#pragma once

#include <string>

namespace hgl::graph::internal {

std::string BuildCompositorReadFailureMessage(
    const char *stage,
    const std::string &template_path,
    const std::string &file_path,
    const std::string &reason);

std::string BuildCompositorPreprocessFailureMessage(
    const char *stage,
    const std::string &template_path,
    const std::string &detail,
    const std::string &glsl_source);

} // namespace hgl::graph::internal