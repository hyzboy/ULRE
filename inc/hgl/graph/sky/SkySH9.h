#pragma once

#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>

namespace hgl
{
    namespace graph
    {
        // 本头会被无 `using namespace math` 的 TU 直接包含（与 LightRow.h 同款写法）
        using namespace math;

        /**
         * 天空 L2 球谐（SH9）系数的**解析合成器**。
         *
         * 落地位置在 `ambient_light_sh` 天光模块的输入端：该模块只读
         * `AmbientEnvInfo::sh[9]`，用 Ramamoorthi & Hanrahan 的解析卷积求漫反射辐照度：
         *
         *   E(n) = Σ cᵢ · Âₗ · Yᵢ(n)
         *
         * 其中 `Â₀ = 0.886227`、`Â₁ = 1.023327`、`Â₂(xy,yz,xz) = 0.858086`、
         * `Â₂(3z²-1) = 0.247708`、`Â₂(x²-y²) = 0.429043` 已写在 shader 里，
         * 所以本模块产出的 `cᵢ` 必须就是**标准归一化实球谐基**下的辐亮度系数
         * `cᵢ = ∫ L(ω)·Yᵢ(ω) dω`（`Y₀₀ = 0.282095`）。
         *
         * 下标约定（Z-up，与 `AmbientEnvInfo::sh`、`ambient_light_sh.glsl` 逐项一致）：
         *
         *   idx0 Y₀₀ ∝ 1            idx3 Y₁₁ ∝ x           idx6 Y₂₀ ∝ 3z²-1
         *   idx1 Y₁₋₁ ∝ y           idx4 Y₂₋₂ ∝ xy          idx7 Y₂₁ ∝ xz
         *   idx2 Y₁₀ ∝ z            idx5 Y₂₋₁ ∝ yz          idx8 Y₂₂ ∝ x²-y²
         *
         * 天空模型（三段拼接，全部对 z 轴回转对称 ⇒ 只有 idx0 / idx2 / idx6 非零）：
         *
         *   z ≥ 0（上半球）：L(z) = horizon + (zenith - horizon)·z   （地平线到天顶线性过渡）
         *   z &lt; 0（下半球）：L(z) = ground                            （地面恒定辐亮度）
         *
         * 三段在 z = 0 处**不连续**（horizon ≠ ground）—— 这符合物理直觉（地平线以下
         * 是地面而不是天空），但会让 SH9 在地平线附近出现 Gibbs 振铃。需要绝对平滑
         * 时应把 ground 调到接近 horizon（或用真实烘焙 IBL 贴图）。
         *
         * 太阳按「小立体角 δ 光源」的带限近似加入：`cᵢ += E_sun · Yᵢ(s)`，
         * 其中 `E_sun` 是该方向的**辐照度**（辐亮度 × 日盘立体角），因此日盘大小
         * 不影响 l ≤ 2 的系数，只影响视觉上的高光盘（那由天空 shader 负责）。
         */

        /// 标准归一化实球谐常数（与 `ambient_light_sh.glsl` 的基函数逐一对应）
        constexpr float kSkySH_Y00   = 0.282095f;   ///< Y₀₀
        constexpr float kSkySH_Y1    = 0.488603f;   ///< Y₁₋₁ / Y₁₀ / Y₁₁ 的方向系数
        constexpr float kSkySH_Y2xy  = 1.092548f;   ///< Y₂₋₂ / Y₂₋₁ / Y₂₁ 的方向系数
        constexpr float kSkySH_Y20   = 0.315392f;   ///< Y₂₀ = kSkySH_Y20·(3z²-1)
        constexpr float kSkySH_Y22   = 0.546274f;   ///< Y₂₂ = kSkySH_Y22·(x²-y²)

        /**
         * SH9 合成参数。
         *
         * 所有颜色都是**线性辐亮度**（不是 sRGB 编码值），因为 SH 系数按能量线性叠加。
         * 只有 `.rgb` 参与计算，`.a` 会被忽略。
         */
        struct SkySH9Params
        {
            Vector4f zenith_color  = Vector4f(0.42f, 0.58f, 0.92f, 1.0f);   ///< 天顶辐亮度
            Vector4f horizon_color = Vector4f(0.72f, 0.80f, 0.90f, 1.0f);   ///< 地平线辐亮度
            Vector4f ground_color  = Vector4f(0.25f, 0.26f, 0.24f, 1.0f);   ///< 下半球（地面）辐亮度

            /// 指向太阳的方向（与 `main_light.direction_intensity.xyz` 同约定）；`.z` 即仰角正弦
            Vector4f sun_direction = Vector4f(0.0f, 0.0f, 1.0f, 0.0f);

            /// 该方向上的太阳辐照度；`<= 0` 表示不叠加太阳项（纯渐变天空）
            Vector4f sun_color     = Vector4f(1.0f, 0.98f, 0.95f, 1.0f);

            float    sun_irradiance = 0.0f;

            /// 整体倍率（线性）。用于把美术给的颜色标定到渲染管线的亮度量级
            float    exposure = 1.0f;
        };

        /**
         * 把天空参数合成为 9 个 SH 辐亮度系数（`.a` 恒为 0）。
         *
         * 代价是几十次乘加，可逐帧调用（太阳移动时重算即可，不需每帧都算）。
         */
        inline void ComputeSkySH9(const SkySH9Params &p, Vector4f out_sh[9])
        {
            const float zr = p.zenith_color.r, zg = p.zenith_color.g, zb = p.zenith_color.b;
            const float hr = p.horizon_color.r, hg = p.horizon_color.g, hb = p.horizon_color.b;
            const float gr = p.ground_color.r, gg = p.ground_color.g, gb = p.ground_color.b;

            // 上半球斜率（zenith - horizon）
            const float dr = zr - hr, dg = zg - hg, db = zb - hb;

            // 2π 来自 dω = dz·dφ 对 φ 的积分；三个基函数的 z 积分结果如下：
            //   ∫L·Y₀₀dω = Y₀₀·2π·[ ground + horizon + Δ/2 ]
            //   ∫L·Y₁₀dω = Y₁ ·2π·[ -ground/2 + horizon/2 + Δ/3 ]
            //   ∫L·Y₂₀dω = Y₂₀·2π·[ Δ/4 ]        （常量项与 horizon 项的 ∫(3z²-1)dz 均为 0）
            constexpr float k2Pi = 6.28318530718f;

            const float s0 = kSkySH_Y00 * k2Pi;
            const float s1 = kSkySH_Y1   * k2Pi;
            const float s2 = kSkySH_Y20  * k2Pi;

            const float hd_r = hr * 0.5f, hd_g = hg * 0.5f, hd_b = hb * 0.5f;

            out_sh[0] = Vector4f(s0 * (gr + hr + dr * 0.5f),
                                 s0 * (gg + hg + dg * 0.5f),
                                 s0 * (gb + hb + db * 0.5f),
                                 0.0f);

            out_sh[1] = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            out_sh[2] = Vector4f(s1 * (-gr * 0.5f + hd_r + dr * (1.0f / 3.0f)),
                                 s1 * (-gg * 0.5f + hd_g + dg * (1.0f / 3.0f)),
                                 s1 * (-gb * 0.5f + hd_b + db * (1.0f / 3.0f)),
                                 0.0f);
            out_sh[3] = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            out_sh[4] = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            out_sh[5] = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            out_sh[6] = Vector4f(s2 * (dr * 0.25f),
                                 s2 * (dg * 0.25f),
                                 s2 * (db * 0.25f),
                                 0.0f);
            out_sh[7] = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            out_sh[8] = Vector4f(0.0f, 0.0f, 0.0f, 0.0f);

            // 太阳项：带限 δ 光源 ⇒ cᵢ += E_sun·Yᵢ(s)
            if (p.sun_irradiance > 0.0f)
            {
                float sx = p.sun_direction.x;
                float sy = p.sun_direction.y;
                float sz = p.sun_direction.z;

                const float len = sqrtf(sx * sx + sy * sy + sz * sz);

                if (len > 1e-6f)
                {
                    sx /= len;
                    sy /= len;
                    sz /= len;

                    const float er = p.sun_color.r * p.sun_irradiance;
                    const float eg = p.sun_color.g * p.sun_irradiance;
                    const float eb = p.sun_color.b * p.sun_irradiance;

                    const float y1y = kSkySH_Y1 * sy;
                    const float y1z = kSkySH_Y1 * sz;
                    const float y1x = kSkySH_Y1 * sx;

                    const float y2xy = kSkySH_Y2xy * (sx * sy);
                    const float y2yz = kSkySH_Y2xy * (sy * sz);
                    const float y20  = kSkySH_Y20  * (3.0f * sz * sz - 1.0f);
                    const float y2xz = kSkySH_Y2xy * (sx * sz);
                    const float y22  = kSkySH_Y22  * (sx * sx - sy * sy);

                    const float y0 = kSkySH_Y00;

                    out_sh[0].x += er * y0;   out_sh[0].y += eg * y0;   out_sh[0].z += eb * y0;
                    out_sh[1].x += er * y1y;  out_sh[1].y += eg * y1y;  out_sh[1].z += eb * y1y;
                    out_sh[2].x += er * y1z;  out_sh[2].y += eg * y1z;  out_sh[2].z += eb * y1z;
                    out_sh[3].x += er * y1x;  out_sh[3].y += eg * y1x;  out_sh[3].z += eb * y1x;
                    out_sh[4].x += er * y2xy; out_sh[4].y += eg * y2xy; out_sh[4].z += eb * y2xy;
                    out_sh[5].x += er * y2yz; out_sh[5].y += eg * y2yz; out_sh[5].z += eb * y2yz;
                    out_sh[6].x += er * y20;  out_sh[6].y += eg * y20;  out_sh[6].z += eb * y20;
                    out_sh[7].x += er * y2xz; out_sh[7].y += eg * y2xz; out_sh[7].z += eb * y2xz;
                    out_sh[8].x += er * y22;  out_sh[8].y += eg * y22;  out_sh[8].z += eb * y22;
                }
            }

            if (p.exposure != 1.0f)
            {
                for (int i = 0; i < 9; ++i)
                {
                    out_sh[i].x *= p.exposure;
                    out_sh[i].y *= p.exposure;
                    out_sh[i].z *= p.exposure;
                }
            }
        }

        /**
         * 按太阳仰角插值出天空三段色（夜晚 → 晨昏 → 白天）。
         *
         * @param elevation_sin 太阳方向 z 分量（指向太阳，Z-up）⇒ 仰角正弦，1 = 天顶，0 = 地平线，&lt;0 = 夜
         *
         * 色板是线性辐亮度；`kSkySunriseBand = 0.20` 以下的负仰角全按夜空处理，
         * `0.25` 以上全按白天处理，中间两段线性插值 —— 这样日落时的暖色带宽度可控，
         * 不会因为模型过于敏感而在两帧之间闪变。
         */
        inline void EvaluateSkyGradientForSun(const float elevation_sin,
                                              Vector4f &out_zenith,
                                              Vector4f &out_horizon,
                                              Vector4f &out_ground)
        {
            // 夜空：只留极弱的天光，避免"全黑 + 一盏主光"的塑料感
            const Vector4f night_zenith  (0.020f, 0.028f, 0.055f, 1.0f);
            const Vector4f night_horizon (0.035f, 0.045f, 0.075f, 1.0f);
            const Vector4f night_ground  (0.012f, 0.015f, 0.022f, 1.0f);

            // 晨昏：地平线暖橙、天顶偏冷，是日落色带的来源
            const Vector4f dusk_zenith   (0.22f, 0.26f, 0.42f, 1.0f);
            const Vector4f dusk_horizon  (0.85f, 0.42f, 0.20f, 1.0f);
            const Vector4f dusk_ground   (0.10f, 0.09f, 0.09f, 1.0f);

            // 白天：蓝天 + 亮地平线（地平线更亮是大气散射的典型观感）
            const Vector4f day_zenith    (0.42f, 0.58f, 0.92f, 1.0f);
            const Vector4f day_horizon   (0.72f, 0.80f, 0.90f, 1.0f);
            const Vector4f day_ground    (0.25f, 0.26f, 0.24f, 1.0f);

            float t;
            const Vector4f *from_zenith;
            const Vector4f *from_horizon;
            const Vector4f *from_ground;

            if (elevation_sin <= 0.0f)
            {
                from_zenith  = &night_zenith;
                from_horizon = &night_horizon;
                from_ground  = &night_ground;

                t = (elevation_sin + 0.20f) * (1.0f / 0.20f);   // [-0.20, 0] → [0, 1]

                if (t < 0.0f) t = 0.0f;

                out_zenith .x = from_zenith ->x + (dusk_zenith .x - from_zenith ->x) * t;
                out_zenith .y = from_zenith ->y + (dusk_zenith .y - from_zenith ->y) * t;
                out_zenith .z = from_zenith ->z + (dusk_zenith .z - from_zenith ->z) * t;
                out_zenith .w = 1.0f;

                out_horizon.x = from_horizon->x + (dusk_horizon.x - from_horizon->x) * t;
                out_horizon.y = from_horizon->y + (dusk_horizon.y - from_horizon->y) * t;
                out_horizon.z = from_horizon->z + (dusk_horizon.z - from_horizon->z) * t;
                out_horizon.w = 1.0f;

                out_ground .x = from_ground->x + (dusk_ground .x - from_ground->x) * t;
                out_ground .y = from_ground->y + (dusk_ground .y - from_ground->y) * t;
                out_ground .z = from_ground->z + (dusk_ground .z - from_ground->z) * t;
                out_ground .w = 1.0f;

                return;
            }

            t = elevation_sin * 4.0f;   // [0, 0.25] → [0, 1]

            if (t > 1.0f) t = 1.0f;

            out_zenith .x = dusk_zenith .x + (day_zenith .x - dusk_zenith .x) * t;
            out_zenith .y = dusk_zenith .y + (day_zenith .y - dusk_zenith .y) * t;
            out_zenith .z = dusk_zenith .z + (day_zenith .z - dusk_zenith .z) * t;
            out_zenith .w = 1.0f;

            out_horizon.x = dusk_horizon.x + (day_horizon.x - dusk_horizon.x) * t;
            out_horizon.y = dusk_horizon.y + (day_horizon.y - dusk_horizon.y) * t;
            out_horizon.z = dusk_horizon.z + (day_horizon.z - dusk_horizon.z) * t;
            out_horizon.w = 1.0f;

            out_ground .x = dusk_ground .x + (day_ground .x - dusk_ground .x) * t;
            out_ground .y = dusk_ground .y + (day_ground .y - dusk_ground .y) * t;
            out_ground .z = dusk_ground .z + (day_ground .z - dusk_ground .z) * t;
            out_ground .w = 1.0f;
        }

    }//namespace graph
}//namespace hgl
