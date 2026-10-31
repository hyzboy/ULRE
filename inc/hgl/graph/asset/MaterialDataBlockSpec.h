#pragma once

/**
 * 材质数据块规约（`*.material_data.toml` 的字段契约真源）。
 *
 * ── 为什么需要它 ────────────────────────────────────────────────────────────
 *
 * 配方（`MaterialRecipeSpec`）只描述**同一份材质定义**。批量示例（比如
 * `PBRSpheres` 的 10×10 metallic/roughness 网格）需要的是**几百行形状相同、
 * 取值不同的 SSBO 行**——这些行过去是示例里双重循环硬编码出来的，示例一多就
 * 变成一堆「手写数据表」，既不可编辑也无法被工具读。
 *
 * 数据块把「一堆行」变成**可授权文件**：
 *
 * ```toml
 * schema = 1
 * name   = "PBRSpheresGrid"
 * target = "PBRSurface"        # 决定落哪个池、行宽多少（见 GlobalSSBOTypes.h）
 *
 * [[rows]]
 * base_color = [0.35, 0.62, 0.31, 1.0]
 * metallic   = 0.0
 * roughness  = 0.05
 * ```
 *
 * ── 三种内容行合成一个行规约 ────────────────────────────────────────────────
 *
 * GPU 侧仍然有三套独立的内容行（`PBRSurfaceRow` 32B / `EmissiveSurfaceRow` 16B /
 * `TransmissionSurfaceRow` 16B），但**授权侧只用一个超集结构** `MaterialDataRowSpec`：
 * `target` 决定哪些键有意义，其余键写给本 target 用不到的行会被当作错误拒绝
 * （见 `MaterialDataBlockFile.cpp` 的 `ValidateRowKeysForTarget`），不会被静默忽略。
 *
 * 这与 `SceneMaterialSpec`（一个结构 + `type` 判别 + 超集字段）是同一种取舍：
 * 授权侧同构，运行时按 `target` 摊成物理行。
 *
 * 注意 `trans_color` 在授权侧是 `Color4f`（可读、可写 0..1），在 GPU 侧是
 * `TransmissionSurfaceRow::trans_color` 的 `uint32`（`packUnorm4x8()` 打包）。
 * 授权形态与物理形态的转换是编解码器的职责，不在规约里表达。
 *
 * 生成器：`src/Tools/AssetGen/asset_gen.py`（`[group.MaterialDataBlock]`）。
 */

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/color/Color4f.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/graph/ssbo/LitMaterialData.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    /**
     * 一行材质数据的**授权形态**（三种内容行的并集）。
     *
     * 缺省值与 `PBRSurfaceRow` / `EmissiveSurfaceRow` / `TransmissionSurfaceRow`
     * 的类内缺省值保持一致：文件里不写的键，落到 GPU 上必须与 C++ 直接构造该行
     * 得到的字节完全相同。
     */
    struct HGL_ASSET_STRUCT MaterialDataRowSpec
    {
        // ── PBRSurface 专用 ────────────────────────────────────────────────
        HGL_PROPERTY(EditAnywhere, Category = "PBR")
        Color4f base_color   = Color4f(1.0f);                        ///<基础颜色

        HGL_PROPERTY(EditAnywhere, Category = "PBR")
        float   metallic     = 0.0f;                                 ///<金属度

        HGL_PROPERTY(EditAnywhere, Category = "PBR")
        float   roughness    = 1.0f;                                 ///<粗糙度

        HGL_PROPERTY(EditAnywhere, Category = "PBR")
        float   normal_scale = ssbo::kDefaultLitMaterialNormalStrength;  ///<法线强度

        HGL_PROPERTY(EditAnywhere, Category = "PBR")
        float   fresnel      = ssbo::kDefaultLitMaterialFresnel;         ///<菲涅尔

        // ── EmissiveSurface 专用 ──────────────────────────────────────────
        HGL_PROPERTY(EditAnywhere, Category = "Emissive")
        Color4f color        = Color4f(1.0f);                        ///<自发光颜色

        // ── TransmissionSurface 专用 ──────────────────────────────────────
        HGL_PROPERTY(EditAnywhere, Category = "Transmission")
        Color4f trans_color  = Color4f(1.0f);                        ///<透射色（落盘时打包成 unorm4x8）
    };

    /**
     * 一个完整的数据块：**同一种**内容行的定长序列。
     *
     * `target` 只能是三个内容行类型之一（`GetGlobalSSBOTypeStructStride != 0`）；
     * `MaterialData`（组件行）**不在此列**——组件行说的是「引用哪一行内容」，
     * 其中的 `surface_ssbo_id` / `surface_data_index` 是运行期行号，不能写进文件。
     */
    struct HGL_ASSET_STRUCT MaterialDataBlockSpec
    {
        HGL_PROPERTY(EditAnywhere, Category = "Block")
        uint32 schema = 1;                                          ///<结构版本，只接受 1

        HGL_PROPERTY(EditAnywhere, Category = "Block")
        AnsiString name;                                            ///<块名（注册表去重 / 诊断）

        HGL_PROPERTY(EditAnywhere, Category = "Block")
        GlobalSSBOType target = GlobalSSBOType::PBRSurface;         ///<内容行类型（决定行宽与物理布局）

        HGL_PROPERTY(EditAnywhere, Category = "Block")
        ValueArray<MaterialDataRowSpec> rows;                       ///<行数据（顺序即池内行序）
    };
}//namespace hgl::graph
