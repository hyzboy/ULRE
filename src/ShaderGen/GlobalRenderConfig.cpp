// GlobalRenderConfig.cpp
// Implementation of global runtime configuration for quality level, lighting model, and sky/ambient model.

#include <hgl/mtl/GlobalRenderConfig.h>

namespace hgl::mtl {

GlobalRenderConfig& GlobalRenderConfig::Instance() {
    static GlobalRenderConfig instance;
    return instance;
}

GlobalRenderConfig::GlobalRenderConfig() = default;

void GlobalRenderConfig::SetQualityLevel(uint32_t level) {
    if (level < 1 || level > 10) {
        // Clamp to valid range
        level = (level < 1) ? 1 : 10;
    }
    if (quality_level_ != level) {
        quality_level_ = level;
        TriggerInvalidation();
    }
}

void GlobalRenderConfig::SetLightingModel(LightingModel model) {
    if (lighting_model_ != model) {
        lighting_model_ = model;
        TriggerInvalidation();
    }
}

void GlobalRenderConfig::SetSkyAmbientModel(SkyAmbientModel model) {
    if (sky_ambient_model_ != model) {
        sky_ambient_model_ = model;
        TriggerInvalidation();
    }
}

void GlobalRenderConfig::RegisterInvalidationCallback(std::function<void()> callback) {
    invalidation_callbacks_.push_back(std::move(callback));
}

void GlobalRenderConfig::TriggerInvalidation() {
    for (auto& cb : invalidation_callbacks_) {
        cb();
    }
}

} // namespace hgl::mtl
