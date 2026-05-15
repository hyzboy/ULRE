#ifndef ULRE_SURFACE_ERROR_INDICATOR_GLSL
#define ULRE_SURFACE_ERROR_INDICATOR_GLSL

// ErrorIndicator Surface — 用于 FS 路由失败时的可视化诊断
//
// 显示红黑交替棋盘格，"黑"格用 RGB 编码 24 位 error_code，便于截屏反推。
//
// error_code 编码规则（由 C++ 侧 ErrorCodeRegistry::EncodeFSError() 生成）：
//   bit  0.. 7 = FSErrorReason (reason category)
//   bit  8..15 = surface_model id
//   bit 16..23 = missing tex_bits low 8 bits
//   (bit 24..31 = sampler_bits low 8，未编入颜色，仅存在于 uint32 数值中)
//
// FS 输出颜色规则：
//   checkerboard cell size ≈ 16 screen pixels
//   "红"格 = (1.0, 0.0, 0.0, 1.0)
//   "黑"格 = (R/255, G/255, B/255, 1.0)
//     其中 R = error_code & 0xFF
//          G = (error_code >> 8)  & 0xFF
//          B = (error_code >> 16) & 0xFF
//
// 反推步骤：
//   截屏 → 找一个"黑"格像素 → 读取 (R,G,B) →
//   error_code = uint(R*255) | (uint(G*255)<<8) | (uint(B*255)<<16) →
//   用 ErrorCodeRegistry::FormatFSError(error_code) 解析

// ULRE_ERROR_CODE 通过 C++ 侧装配时内联到 GLSL 常量中传入。
// 这里保留普通 const，避免在当前反射/预处理路径里触发 specialization constant 限制。
#ifndef ULRE_ERROR_CODE
const uint ULRE_ERROR_CODE = 0u;
#endif

SurfaceOutput EvalSurface(SurfaceInput si)
{
    // 16 像素网格棋盘（screen space）
    ivec2 cell = ivec2(floor(si.screenPos * 0.0625)); // 0.0625 = 1/16
    float checker = mod(float(cell.x + cell.y), 2.0);

    // 解码 error_code 为 RGB（各字节 / 255.0 映射到 [0,1]）
    float ec_r = float( ULRE_ERROR_CODE        & 0xFFu) / 255.0;
    float ec_g = float((ULRE_ERROR_CODE >>  8u) & 0xFFu) / 255.0;
    float ec_b = float((ULRE_ERROR_CODE >> 16u) & 0xFFu) / 255.0;

    // checker == 1.0 → "红"格；checker == 0.0 → "黑"格（编码颜色）
    vec3 color = mix(vec3(ec_r, ec_g, ec_b), vec3(1.0, 0.0, 0.0), checker);

    SurfaceOutput so;
    so.baseColor = color;
    so.alpha     = 1.0;
    so.normal    = si.worldNormal;
    so.metallic  = 0.0;
    so.roughness = 1.0;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    return so;
}

float EvalAlpha(SurfaceInput si)
{
    return 1.0;
}

#endif // ULRE_SURFACE_ERROR_INDICATOR_GLSL
