#pragma once

namespace hgl::graph
{
    /**
     * 材质数据路径标记：Arena+BDA 为唯一材质实例数据路径（W3.4 收尾完成）。
     *
     * 历史：迁移期(W2-W3.2)本函数读环境变量 ULRE_MATERIAL_ARENA 做双路径切换；
     * 旧路径（per-material 数据槽描述符 + 4B 行号表 + 全局纹理行表双轨）已于
     * W3.3 原子删除。保留本函数仅为避免触碰全部调用点——编译器将其折叠为
     * 常量，所有 `if (IsMaterialArenaBDAEnabled())` 分支的死代码由后续清理移除。
     */
    inline bool IsMaterialArenaBDAEnabled()
    {
        return true;
    }
}//namespace hgl::graph
