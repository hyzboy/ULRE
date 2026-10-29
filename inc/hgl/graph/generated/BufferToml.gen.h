// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ubo/*.h、inc/hgl/graph/ssbo/*.h 的 HGL_UBO* / HGL_SSBO* 结构
//   说明：读写语义 = 键缺失保留 C++ 类内初始值；键存在但类型/长度不符即失败。
//         命名以 `_pad` / `reserved` 开头的字段是对齐填充，不进 TOML。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include <toml/toml.hpp>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/graph/ubo/AmbientEnvInfo.h>
#include <hgl/graph/ubo/ColorPalette.h>
#include <hgl/graph/ubo/GlobalAddresses.h>
#include <hgl/graph/ubo/LightRow.h>
#include <hgl/graph/ubo/ShadowInfo.h>
#include <hgl/graph/ubo/ViewportInfo.h>
#include <hgl/graph/ubo/WorldAddresses.h>

namespace hgl::graph::generated
{
    namespace detail
    {
        // 所有 helper 都遵守同一条约定：**键缺失不算错误**（保留 C++ 结构的类内初始值），
        // 键存在但类型/长度不对才算错误。这正是「未给的字段用行结构的默认值」的实现方式，
        // 也是本生成器不需要解析 C++ 初始化表达式的原因。
        inline const toml::value *Find(const toml::value &table, const char *key)
        {
            if (!table.is_table())
                return nullptr;
            const auto &t = table.as_table();
            const auto it = t.find(key);
            return it == t.end() ? nullptr : &it->second;
        }

        inline bool AsFloat(const toml::value &v, float &out)
        {
            if (v.is_floating()) { out = static_cast<float>(v.as_floating()); return true; }
            if (v.is_integer())  { out = static_cast<float>(v.as_integer());  return true; }
            return false;
        }

        template <typename T>
        inline bool AsUnsigned(const toml::value &v, T &out)
        {
            if (!v.is_integer()) return false;
            const auto raw = v.as_integer();
            if (raw < 0) return false;
            out = static_cast<T>(raw);
            return true;
        }

        template <typename T>
        inline bool AsSigned(const toml::value &v, T &out)
        {
            if (!v.is_integer()) return false;
            out = static_cast<T>(v.as_integer());
            return true;
        }

        inline bool AsFloatArray(const toml::value &v, float *out, const int count)
        {
            if (!v.is_array()) return false;
            const auto &a = v.as_array();
            if (static_cast<int>(a.size()) != count) return false;
            for (int i = 0; i < count; ++i)
                if (!AsFloat(a[size_t(i)], out[i])) return false;
            return true;
        }

        template <typename T>
        inline bool AsUnsignedArray(const toml::value &v, T *out, const int count)
        {
            if (!v.is_array()) return false;
            const auto &a = v.as_array();
            if (static_cast<int>(a.size()) != count) return false;
            for (int i = 0; i < count; ++i)
                if (!AsUnsigned(a[size_t(i)], out[i])) return false;
            return true;
        }

        template <typename T>
        inline bool AsSignedArray(const toml::value &v, T *out, const int count)
        {
            if (!v.is_array()) return false;
            const auto &a = v.as_array();
            if (static_cast<int>(a.size()) != count) return false;
            for (int i = 0; i < count; ++i)
                if (!AsSigned(a[size_t(i)], out[i])) return false;
            return true;
        }

        inline toml::value FromFloat(const float v)
        {
            return toml::value(static_cast<toml::value::floating_type>(v));
        }

        template <typename T>
        inline toml::value FromInteger(const T v)
        {
            return toml::value(static_cast<toml::value::integer_type>(v));
        }

        inline toml::value FromFloatArray(const float *src, const int count)
        {
            toml::array a;
            a.reserve(size_t(count));
            for (int i = 0; i < count; ++i)
                a.push_back(FromFloat(src[i]));
            return toml::value(std::move(a));
        }

        template <typename T>
        inline toml::value FromIntegerArray(const T *src, const int count)
        {
            toml::array a;
            a.reserve(size_t(count));
            for (int i = 0; i < count; ++i)
                a.push_back(FromInteger(src[i]));
            return toml::value(std::move(a));
        }
    } // namespace detail

    inline bool ReadAmbientEnvInfo(const toml::value &table, hgl::graph::AmbientEnvInfo &out);
    inline bool ReadViewportInfo(const toml::value &table, hgl::graph::ViewportInfo &out);
    inline bool ReadColorPalette(const toml::value &table, hgl::graph::ColorPalette &out);
    inline bool ReadShadowInfo(const toml::value &table, hgl::graph::ShadowInfo &out);
    inline bool ReadShadowCascadeInfo(const toml::value &table, hgl::graph::ShadowCascadeInfo &out);
    inline bool ReadGlobalAddresses(const toml::value &table, hgl::graph::GlobalAddresses &out);
    inline bool ReadWorldAddresses(const toml::value &table, hgl::graph::WorldAddresses &out);
    inline bool ReadLightRow(const toml::value &table, hgl::graph::LightRow &out);
    inline bool ReadPBRSurfaceRow(const toml::value &table, hgl::graph::ssbo::PBRSurfaceRow &out);
    inline bool ReadEmissiveSurfaceRow(const toml::value &table, hgl::graph::ssbo::EmissiveSurfaceRow &out);
    inline bool ReadTransmissionSurfaceRow(const toml::value &table, hgl::graph::ssbo::TransmissionSurfaceRow &out);
    inline bool ReadMaterialDataRow(const toml::value &table, hgl::graph::ssbo::MaterialDataRow &out);
    inline toml::value WriteAmbientEnvInfo(const hgl::graph::AmbientEnvInfo &in);
    inline toml::value WriteViewportInfo(const hgl::graph::ViewportInfo &in);
    inline toml::value WriteColorPalette(const hgl::graph::ColorPalette &in);
    inline toml::value WriteShadowInfo(const hgl::graph::ShadowInfo &in);
    inline toml::value WriteShadowCascadeInfo(const hgl::graph::ShadowCascadeInfo &in);
    inline toml::value WriteGlobalAddresses(const hgl::graph::GlobalAddresses &in);
    inline toml::value WriteWorldAddresses(const hgl::graph::WorldAddresses &in);
    inline toml::value WriteLightRow(const hgl::graph::LightRow &in);
    inline toml::value WritePBRSurfaceRow(const hgl::graph::ssbo::PBRSurfaceRow &in);
    inline toml::value WriteEmissiveSurfaceRow(const hgl::graph::ssbo::EmissiveSurfaceRow &in);
    inline toml::value WriteTransmissionSurfaceRow(const hgl::graph::ssbo::TransmissionSurfaceRow &in);
    inline toml::value WriteMaterialDataRow(const hgl::graph::ssbo::MaterialDataRow &in);

    inline bool ReadAmbientEnvInfo(const toml::value &table, hgl::graph::AmbientEnvInfo &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "env_tex"))
        {
            if (!detail::AsUnsignedArray(*v, &out.env_tex.x, 4)) return false;
        }
        return true;
    }

    inline bool ReadViewportInfo(const toml::value &table, hgl::graph::ViewportInfo &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "ortho_matrix"))
        {
            if (!detail::AsFloatArray(*v, &out.ortho_matrix[0][0], 16)) return false;
        }
        if (const toml::value *v = detail::Find(table, "canvas_resolution"))
        {
            if (!detail::AsUnsignedArray(*v, &out.canvas_resolution.x, 2)) return false;
        }
        if (const toml::value *v = detail::Find(table, "viewport_resolution"))
        {
            if (!detail::AsUnsignedArray(*v, &out.viewport_resolution.x, 2)) return false;
        }
        if (const toml::value *v = detail::Find(table, "inv_viewport_resolution"))
        {
            if (!detail::AsFloatArray(*v, &out.inv_viewport_resolution.x, 2)) return false;
        }
        return true;
    }

    inline bool ReadColorPalette(const toml::value &table, hgl::graph::ColorPalette &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "color"))
        {
            if (!detail::AsUnsignedArray(*v, out.color, 256)) return false;
        }
        return true;
    }

    inline bool ReadShadowInfo(const toml::value &table, hgl::graph::ShadowInfo &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "shadow_vp"))
        {
            if (!detail::AsFloatArray(*v, &out.shadow_vp[0][0], 16)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_params"))
        {
            if (!detail::AsFloatArray(*v, &out.shadow_params.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_map_size"))
        {
            if (!detail::AsFloatArray(*v, &out.shadow_map_size.x, 2)) return false;
        }
        if (const toml::value *v = detail::Find(table, "inv_shadow_map_size"))
        {
            if (!detail::AsFloatArray(*v, &out.inv_shadow_map_size.x, 2)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_tex"))
        {
            if (!detail::AsUnsignedArray(*v, &out.shadow_tex.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "csm_params"))
        {
            if (!detail::AsUnsignedArray(*v, &out.csm_params.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "cascades"))
        {
            if (!v->is_array()) return false;
            const auto &a = v->as_array();
            if (a.size() != 4) return false;
            for (size_t i = 0; i < 4; ++i)
                if (!ReadShadowCascadeInfo(a[i], out.cascades[i])) return false;
        }
        return true;
    }

    inline bool ReadShadowCascadeInfo(const toml::value &table, hgl::graph::ShadowCascadeInfo &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "shadow_vp"))
        {
            if (!detail::AsFloatArray(*v, &out.shadow_vp[0][0], 16)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_params"))
        {
            if (!detail::AsFloatArray(*v, &out.shadow_params.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_map_size"))
        {
            if (!detail::AsFloatArray(*v, &out.shadow_map_size.x, 2)) return false;
        }
        if (const toml::value *v = detail::Find(table, "inv_shadow_map_size"))
        {
            if (!detail::AsFloatArray(*v, &out.inv_shadow_map_size.x, 2)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_tex"))
        {
            if (!detail::AsUnsignedArray(*v, &out.shadow_tex.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "cascade_params"))
        {
            if (!detail::AsFloatArray(*v, &out.cascade_params.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "cache_origin"))
        {
            if (!detail::AsFloatArray(*v, &out.cache_origin.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "cache_offset"))
        {
            if (!detail::AsUnsignedArray(*v, &out.cache_offset.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "cache_valid_rect"))
        {
            if (!detail::AsUnsignedArray(*v, &out.cache_valid_rect.x, 4)) return false;
        }
        return true;
    }

    inline bool ReadGlobalAddresses(const toml::value &table, hgl::graph::GlobalAddresses &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "addr_mesh_draw_params_pool"))
        {
            if (!detail::AsUnsigned(*v, out.addr_mesh_draw_params_pool)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_geometry_aabb"))
        {
            if (!detail::AsUnsigned(*v, out.addr_geometry_aabb)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_pbr_surface"))
        {
            if (!detail::AsUnsigned(*v, out.addr_pbr_surface)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_emissive_surface"))
        {
            if (!detail::AsUnsigned(*v, out.addr_emissive_surface)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_transmission_surface"))
        {
            if (!detail::AsUnsigned(*v, out.addr_transmission_surface)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_color_palette"))
        {
            if (!detail::AsUnsigned(*v, out.addr_color_palette)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_viewport"))
        {
            if (!detail::AsUnsigned(*v, out.addr_viewport)) return false;
        }
        return true;
    }

    inline bool ReadWorldAddresses(const toml::value &table, hgl::graph::WorldAddresses &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "addr_camera_info"))
        {
            if (!detail::AsUnsigned(*v, out.addr_camera_info)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_shadow"))
        {
            if (!detail::AsUnsigned(*v, out.addr_shadow)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_entity_gpu"))
        {
            if (!detail::AsUnsigned(*v, out.addr_entity_gpu)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_l2w"))
        {
            if (!detail::AsUnsigned(*v, out.addr_l2w)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_entity_render_ids"))
        {
            if (!detail::AsUnsigned(*v, out.addr_entity_render_ids)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_lights"))
        {
            if (!detail::AsUnsigned(*v, out.addr_lights)) return false;
        }
        if (const toml::value *v = detail::Find(table, "addr_ambient_env"))
        {
            if (!detail::AsUnsigned(*v, out.addr_ambient_env)) return false;
        }
        return true;
    }

    inline bool ReadLightRow(const toml::value &table, hgl::graph::LightRow &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "type"))
        {
            if (!detail::AsUnsigned(*v, out.type)) return false;
        }
        if (const toml::value *v = detail::Find(table, "flags"))
        {
            if (!detail::AsUnsigned(*v, out.flags)) return false;
        }
        if (const toml::value *v = detail::Find(table, "direction_intensity"))
        {
            if (!detail::AsFloatArray(*v, &out.direction_intensity.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "color_angle"))
        {
            if (!detail::AsFloatArray(*v, &out.color_angle.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_kind"))
        {
            if (!detail::AsUnsigned(*v, out.shadow_kind)) return false;
        }
        if (const toml::value *v = detail::Find(table, "shadow_index"))
        {
            if (!detail::AsUnsigned(*v, out.shadow_index)) return false;
        }
        return true;
    }

    inline bool ReadPBRSurfaceRow(const toml::value &table, hgl::graph::ssbo::PBRSurfaceRow &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "base_color"))
        {
            if (!detail::AsFloatArray(*v, &out.base_color.x, 4)) return false;
        }
        if (const toml::value *v = detail::Find(table, "metallic"))
        {
            if (!detail::AsFloat(*v, out.metallic)) return false;
        }
        if (const toml::value *v = detail::Find(table, "roughness"))
        {
            if (!detail::AsFloat(*v, out.roughness)) return false;
        }
        if (const toml::value *v = detail::Find(table, "normal_scale"))
        {
            if (!detail::AsFloat(*v, out.normal_scale)) return false;
        }
        if (const toml::value *v = detail::Find(table, "fresnel"))
        {
            if (!detail::AsFloat(*v, out.fresnel)) return false;
        }
        return true;
    }

    inline bool ReadEmissiveSurfaceRow(const toml::value &table, hgl::graph::ssbo::EmissiveSurfaceRow &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "color"))
        {
            if (!detail::AsFloatArray(*v, &out.color.x, 4)) return false;
        }
        return true;
    }

    inline bool ReadTransmissionSurfaceRow(const toml::value &table, hgl::graph::ssbo::TransmissionSurfaceRow &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "trans_color"))
        {
            if (!detail::AsUnsigned(*v, out.trans_color)) return false;
        }
        return true;
    }

    inline bool ReadMaterialDataRow(const toml::value &table, hgl::graph::ssbo::MaterialDataRow &out)
    {
        if (!table.is_table())
            return false;
        if (const toml::value *v = detail::Find(table, "surface_type"))
        {
            if (!detail::AsUnsigned(*v, out.surface_type)) return false;
        }
        if (const toml::value *v = detail::Find(table, "surface_ssbo_id"))
        {
            if (!detail::AsUnsigned(*v, out.surface_ssbo_id)) return false;
        }
        if (const toml::value *v = detail::Find(table, "surface_data_index"))
        {
            if (!detail::AsUnsigned(*v, out.surface_data_index)) return false;
        }
        if (const toml::value *v = detail::Find(table, "authored_flags"))
        {
            if (!detail::AsUnsigned(*v, out.authored_flags)) return false;
        }
        return true;
    }

    inline toml::value WriteAmbientEnvInfo(const hgl::graph::AmbientEnvInfo &in)
    {
        toml::table t;
        t["env_tex"] = detail::FromIntegerArray(&in.env_tex.x, 4);
        return toml::value(std::move(t));
    }

    inline toml::value WriteViewportInfo(const hgl::graph::ViewportInfo &in)
    {
        toml::table t;
        t["ortho_matrix"] = detail::FromFloatArray(&in.ortho_matrix[0][0], 16);
        t["canvas_resolution"] = detail::FromIntegerArray(&in.canvas_resolution.x, 2);
        t["viewport_resolution"] = detail::FromIntegerArray(&in.viewport_resolution.x, 2);
        t["inv_viewport_resolution"] = detail::FromFloatArray(&in.inv_viewport_resolution.x, 2);
        return toml::value(std::move(t));
    }

    inline toml::value WriteColorPalette(const hgl::graph::ColorPalette &in)
    {
        toml::table t;
        t["color"] = detail::FromIntegerArray(in.color, 256);
        return toml::value(std::move(t));
    }

    inline toml::value WriteShadowInfo(const hgl::graph::ShadowInfo &in)
    {
        toml::table t;
        t["shadow_vp"] = detail::FromFloatArray(&in.shadow_vp[0][0], 16);
        t["shadow_params"] = detail::FromFloatArray(&in.shadow_params.x, 4);
        t["shadow_map_size"] = detail::FromFloatArray(&in.shadow_map_size.x, 2);
        t["inv_shadow_map_size"] = detail::FromFloatArray(&in.inv_shadow_map_size.x, 2);
        t["shadow_tex"] = detail::FromIntegerArray(&in.shadow_tex.x, 4);
        t["csm_params"] = detail::FromIntegerArray(&in.csm_params.x, 4);
        {
            toml::array a;
            a.reserve(4);
            for (size_t i = 0; i < 4; ++i)
                a.push_back(toml::value(WriteShadowCascadeInfo(in.cascades[i])));
            t["cascades"] = toml::value(std::move(a));
        }
        return toml::value(std::move(t));
    }

    inline toml::value WriteShadowCascadeInfo(const hgl::graph::ShadowCascadeInfo &in)
    {
        toml::table t;
        t["shadow_vp"] = detail::FromFloatArray(&in.shadow_vp[0][0], 16);
        t["shadow_params"] = detail::FromFloatArray(&in.shadow_params.x, 4);
        t["shadow_map_size"] = detail::FromFloatArray(&in.shadow_map_size.x, 2);
        t["inv_shadow_map_size"] = detail::FromFloatArray(&in.inv_shadow_map_size.x, 2);
        t["shadow_tex"] = detail::FromIntegerArray(&in.shadow_tex.x, 4);
        t["cascade_params"] = detail::FromFloatArray(&in.cascade_params.x, 4);
        t["cache_origin"] = detail::FromFloatArray(&in.cache_origin.x, 4);
        t["cache_offset"] = detail::FromIntegerArray(&in.cache_offset.x, 4);
        t["cache_valid_rect"] = detail::FromIntegerArray(&in.cache_valid_rect.x, 4);
        return toml::value(std::move(t));
    }

    inline toml::value WriteGlobalAddresses(const hgl::graph::GlobalAddresses &in)
    {
        toml::table t;
        t["addr_mesh_draw_params_pool"] = detail::FromInteger(in.addr_mesh_draw_params_pool);
        t["addr_geometry_aabb"] = detail::FromInteger(in.addr_geometry_aabb);
        t["addr_pbr_surface"] = detail::FromInteger(in.addr_pbr_surface);
        t["addr_emissive_surface"] = detail::FromInteger(in.addr_emissive_surface);
        t["addr_transmission_surface"] = detail::FromInteger(in.addr_transmission_surface);
        t["addr_color_palette"] = detail::FromInteger(in.addr_color_palette);
        t["addr_viewport"] = detail::FromInteger(in.addr_viewport);
        return toml::value(std::move(t));
    }

    inline toml::value WriteWorldAddresses(const hgl::graph::WorldAddresses &in)
    {
        toml::table t;
        t["addr_camera_info"] = detail::FromInteger(in.addr_camera_info);
        t["addr_shadow"] = detail::FromInteger(in.addr_shadow);
        t["addr_entity_gpu"] = detail::FromInteger(in.addr_entity_gpu);
        t["addr_l2w"] = detail::FromInteger(in.addr_l2w);
        t["addr_entity_render_ids"] = detail::FromInteger(in.addr_entity_render_ids);
        t["addr_lights"] = detail::FromInteger(in.addr_lights);
        t["addr_ambient_env"] = detail::FromInteger(in.addr_ambient_env);
        return toml::value(std::move(t));
    }

    inline toml::value WriteLightRow(const hgl::graph::LightRow &in)
    {
        toml::table t;
        t["type"] = detail::FromInteger(in.type);
        t["flags"] = detail::FromInteger(in.flags);
        t["direction_intensity"] = detail::FromFloatArray(&in.direction_intensity.x, 4);
        t["color_angle"] = detail::FromFloatArray(&in.color_angle.x, 4);
        t["shadow_kind"] = detail::FromInteger(in.shadow_kind);
        t["shadow_index"] = detail::FromInteger(in.shadow_index);
        return toml::value(std::move(t));
    }

    inline toml::value WritePBRSurfaceRow(const hgl::graph::ssbo::PBRSurfaceRow &in)
    {
        toml::table t;
        t["base_color"] = detail::FromFloatArray(&in.base_color.x, 4);
        t["metallic"] = detail::FromFloat(in.metallic);
        t["roughness"] = detail::FromFloat(in.roughness);
        t["normal_scale"] = detail::FromFloat(in.normal_scale);
        t["fresnel"] = detail::FromFloat(in.fresnel);
        return toml::value(std::move(t));
    }

    inline toml::value WriteEmissiveSurfaceRow(const hgl::graph::ssbo::EmissiveSurfaceRow &in)
    {
        toml::table t;
        t["color"] = detail::FromFloatArray(&in.color.x, 4);
        return toml::value(std::move(t));
    }

    inline toml::value WriteTransmissionSurfaceRow(const hgl::graph::ssbo::TransmissionSurfaceRow &in)
    {
        toml::table t;
        t["trans_color"] = detail::FromInteger(in.trans_color);
        return toml::value(std::move(t));
    }

    inline toml::value WriteMaterialDataRow(const hgl::graph::ssbo::MaterialDataRow &in)
    {
        toml::table t;
        t["surface_type"] = detail::FromInteger(in.surface_type);
        t["surface_ssbo_id"] = detail::FromInteger(in.surface_ssbo_id);
        t["surface_data_index"] = detail::FromInteger(in.surface_data_index);
        t["authored_flags"] = detail::FromInteger(in.authored_flags);
        return toml::value(std::move(t));
    }

} // namespace hgl::graph::generated
