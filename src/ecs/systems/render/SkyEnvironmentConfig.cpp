#include<hgl/ecs/systems/render/SkyEnvironmentConfig.h>

namespace hgl::ecs
{
    const char *GetSkyModeName(SkyMode mode) noexcept
    {
        switch (mode)
        {
            case SkyMode::Atmosphere:      return "Atmosphere";
            case SkyMode::CubeMapIBL:      return "CubeMapIBL";
            case SkyMode::OctahedralIBL:   return "OctahedralIBL";
            case SkyMode::LambertDome:     return "LambertDome";
            case SkyMode::SHLightOnly:     return "SHLightOnly";
        }

        return "Unknown";
    }

    const char *GetSkyModeGeometryURI(SkyMode mode) noexcept
    {
        switch (mode)
        {
            // 全天球（只喂 POSITION，UV 由方向在几何/着色器侧推导）
            case SkyMode::Atmosphere:
            case SkyMode::CubeMapIBL:
            case SkyMode::OctahedralIBL:   return "geo:builtin:hex_sphere_sky";

            // 半球穹顶（顶点自带方位投影 UV，材质侧就是普通 2D 采样）
            case SkyMode::LambertDome:     return "geo:builtin:dome_64";

            case SkyMode::SHLightOnly:     return nullptr;
        }

        return nullptr;
    }

    bool SkyModeGeometryHasUV(SkyMode mode) noexcept
    {
        // 只有半球穹顶的 UV 是**几何事实**（按 Lambert Azimuthal 投影烘进顶点）；
        // 全天球三兄弟靠方向采样，声明 UV 反而要求几何提供它没有的流。
        return mode == SkyMode::LambertDome;
    }

    bool SkyEnvironmentConfig::IsSHZero() const
    {
        for (int i = 0; i < 9; ++i)
        {
            if (sh[i].x != 0.0f || sh[i].y != 0.0f || sh[i].z != 0.0f)
                return false;
        }

        return true;
    }
}//namespace hgl::ecs
