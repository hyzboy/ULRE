// GlobalRenderConfig.h
// Central global runtime configuration for quality level, lighting model, and sky/ambient model.
// Allows runtime switching and invalidation callbacks for shader cache/pipeline updates.

#pragma once
#include <cstdint>
#include <functional>
#include <vector>

namespace hgl::mtl {

/// Lighting model used globally
enum class LightingModel : uint8_t {
    Lambert = 0,
    BlinnPhong,
    PBR,
    COUNT
};

/// Sky/ambient lighting source model
enum class SkyAmbientModel : uint8_t {
    None = 0,       ///< No sky/ambient contribution
    Constant,       ///< Constant ambient color
    Hemisphere,     ///< Hemisphere ambient
    SkyAtmosphere,  ///< Atmosphere scattering sky
    Cubemap,        ///< Cubemap-based sky
    COUNT
};

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
    SkyAmbientModel GetSkyAmbientModel() const { return sky_ambient_model_; }

    /// Set sky/ambient model and trigger cache invalidation callbacks
    void SetSkyAmbientModel(SkyAmbientModel model);

    /// Register a callback to be invoked when any config changes that may invalidate shader cache
    void RegisterInvalidationCallback(std::function<void()> callback);

private:
    GlobalRenderConfig();

    void TriggerInvalidation();

    uint32_t quality_level_ = 10;  // Default to highest quality
    LightingModel lighting_model_ = LightingModel::PBR;
    SkyAmbientModel sky_ambient_model_ = SkyAmbientModel::SkyAtmosphere;

    std::vector<std::function<void()>> invalidation_callbacks_;
};

} // namespace hgl::mtl
