// GlobalRenderConfig.h
// Central global runtime configuration for quality level, lighting model, and sky/ambient model.
// Allows runtime switching and invalidation callbacks for shader cache/pipeline updates.

#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <hgl/mtl/LightingModel.h>
#include <hgl/mtl/SkyLight.h>

namespace hgl::graph::mtl {

/// Global render configuration state
class GlobalRenderConfig {
public:
    static GlobalRenderConfig& Instance();

    /// Get current quality level (1–10, where 10 is highest quality)
    uint32_t GetQualityLevel() const { return quality_level_; }

    /// Set quality level and trigger cache invalidation callbacks
    void SetQualityLevel(uint32_t level);

    /// Get current lighting model
    LightingModel GetLightingModel() const { return lighting_model_; }

    /// Set lighting model and trigger cache invalidation callbacks
    void SetLightingModel(LightingModel model);

    /// Get current sky/ambient model
    SkyLightAmbientModel GetSkyAmbientModel() const { return sky_ambient_model_; }

    /// Set sky/ambient model and trigger cache invalidation callbacks
    void SetSkyAmbientModel(SkyLightAmbientModel model);

    /// Register a callback to be invoked when any config changes that may invalidate shader cache
    void RegisterInvalidationCallback(std::function<void()> callback);

private:
    GlobalRenderConfig();

    void TriggerInvalidation();

    uint32_t quality_level_ = 10;  // Default to highest quality
    LightingModel lighting_model_ = LightingModel::BlinnPhong;
    SkyLightAmbientModel sky_ambient_model_ = SkyLightAmbientModel::Simple;

    std::vector<std::function<void()>> invalidation_callbacks_;
};

} // namespace hgl::graph::mtl
