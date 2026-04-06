#pragma once

#include"ISubWorldModule.h"

#include<memory>

namespace example::modules
{
    // Factory returning base interface so callers do not depend on concrete implementation.
    std::unique_ptr<ISubWorldModule> CreateSubWorldAnimatedGeometryModule();
}
