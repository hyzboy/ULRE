#pragma once

#include<cstdlib>
#include<cwchar>

namespace hgl::graph
{
    /**
     * 材质数据双路径开关（W2 迁移期专用，W3.4 随旧路径一并删除）
     *
     * 环境变量 ULRE_MATERIAL_ARENA=1 / on 启用 Arena+BDA 路径；默认关闭走旧路径。
     *
     * 三处消费点必须取得同一值：
     *   1. ShaderGen GLSL 发射（buffer_reference 行声明 + MTL_ROW 宏）
     *   2. ECS 地址行表写入（mtl_data_addrs，8B/行）
     *   3. ResourceDomainManager::AllocateArrayAccessor 后端选择
     *
     * 头文件内联静态：无链接依赖（ShaderGen/SceneGraph/ECS 三方均可包含）；
     * 进程内只读一次环境变量。
     */
    inline bool IsMaterialArenaBDAEnabled()
    {
        //static const bool enabled=[]
        //{
        //    const wchar_t *env=_wgetenv(L"ULRE_MATERIAL_ARENA");

        //    return env!=nullptr
        //        && (wcscmp(env,L"1")==0
        //         || wcscmp(env,L"on")==0);
        //}();

        //return enabled;

        // 迁移期(W2-W3)双路径开关；W3.4 删 flag 时恢复此返回并内联删除。
        // 请勿硬编码——回归基线(flag off)依赖双态可切。
        static const bool enabled=[]
        {
            const wchar_t *env=_wgetenv(L"ULRE_MATERIAL_ARENA");

            return env!=nullptr
                && (wcscmp(env,L"1")==0
                 || wcscmp(env,L"on")==0);
        }();

        return enabled;
    }
}//namespace hgl::graph
