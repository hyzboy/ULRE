#pragma once

#include <hgl/mtl/MaterialProfileAsset.h>

#include "StandardVariantPolicy.h"

namespace hgl::graph::mtl
{

MaterialProfileAsset BuildBuiltinStandardDefaultProfile();

bool BuildStandardPolicyFromProfile(const MaterialProfileAsset &profile,
                                    const MaterialVariantKey &input_key,
                                    StandardVariantPolicyResult &out_policy,
                                    std::vector<std::string> &diagnostics);

} // namespace hgl::graph::mtl
