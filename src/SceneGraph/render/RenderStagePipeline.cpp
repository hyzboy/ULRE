#include <algorithm>
#include <cstring>

#include<hgl/graph/RenderStagePipeline.h>

namespace hgl::graph
{
    void RenderStagePipeline::AddStage(RenderStage *stage)
    {
        if(!stage)
            return;

        if(std::find(stages.begin(),stages.end(),stage) != stages.end())
            return;

        stages.push_back(stage);
    }

    bool RenderStagePipeline::RemoveStage(RenderStage *stage)
    {
        auto it = std::find(stages.begin(),stages.end(),stage);
        if(it == stages.end())
            return false;

        stages.erase(it);
        return true;
    }

    bool RenderStagePipeline::RemoveStage(const char *stage_name)
    {
        if(!stage_name)
            return false;

        for(auto it = stages.begin(); it != stages.end(); ++it)
        {
            RenderStage *stage = *it;
            if(stage && stage->GetName() && std::strcmp(stage->GetName(),stage_name) == 0)
            {
                stages.erase(it);
                return true;
            }
        }

        return false;
    }

    void RenderStagePipeline::ClearStages()
    {
        stages.clear();
    }

    bool RenderStagePipeline::InsertStage(RenderStage *stage,size_t index)
    {
        if(!stage)
            return false;

        if(std::find(stages.begin(),stages.end(),stage) != stages.end())
            return false;

        if(index > stages.size())
            index = stages.size();

        stages.insert(stages.begin() + index,stage);
        return true;
    }

    bool RenderStagePipeline::InsertStageBefore(RenderStage *stage,RenderStage *before_stage)
    {
        auto it = std::find(stages.begin(),stages.end(),before_stage);
        if(it == stages.end())
            return false;

        return InsertStage(stage,static_cast<size_t>(std::distance(stages.begin(),it)));
    }

    bool RenderStagePipeline::InsertStageAfter(RenderStage *stage,RenderStage *after_stage)
    {
        auto it = std::find(stages.begin(),stages.end(),after_stage);
        if(it == stages.end())
            return false;

        return InsertStage(stage,static_cast<size_t>(std::distance(stages.begin(),it)) + 1);
    }

    bool RenderStagePipeline::InsertStageBefore(RenderStage *stage,const char *before_name)
    {
        if(!before_name)
            return false;

        for(size_t i=0;i<stages.size();++i)
        {
            RenderStage *current = stages[i];
            if(current && current->GetName() && std::strcmp(current->GetName(),before_name) == 0)
                return InsertStage(stage,i);
        }

        return false;
    }

    bool RenderStagePipeline::InsertStageAfter(RenderStage *stage,const char *after_name)
    {
        if(!after_name)
            return false;

        for(size_t i=0;i<stages.size();++i)
        {
            RenderStage *current = stages[i];
            if(current && current->GetName() && std::strcmp(current->GetName(),after_name) == 0)
                return InsertStage(stage,i + 1);
        }

        return false;
    }

    bool RenderStagePipeline::MoveStage(RenderStage *stage,size_t new_index)
    {
        auto it = std::find(stages.begin(),stages.end(),stage);
        if(it == stages.end())
            return false;

        if(new_index >= stages.size())
            new_index = stages.size() - 1;

        const size_t old_index = static_cast<size_t>(std::distance(stages.begin(),it));
        if(old_index == new_index)
            return true;

        RenderStage *saved = *it;
        stages.erase(it);

        if(new_index > stages.size())
            new_index = stages.size();

        stages.insert(stages.begin() + new_index,saved);
        return true;
    }

    bool RenderStagePipeline::MoveStage(const char *stage_name,size_t new_index)
    {
        if(!stage_name)
            return false;

        for(RenderStage *stage:stages)
        {
            if(stage && stage->GetName() && std::strcmp(stage->GetName(),stage_name) == 0)
                return MoveStage(stage,new_index);
        }

        return false;
    }

    bool RenderStagePipeline::ReplaceStage(const char *stage_name,RenderStage *new_stage)
    {
        if(!stage_name || !new_stage)
            return false;

        for(size_t i=0;i<stages.size();++i)
        {
            RenderStage *stage = stages[i];
            if(stage && stage->GetName() && std::strcmp(stage->GetName(),stage_name) == 0)
            {
                if(std::find(stages.begin(),stages.end(),new_stage) != stages.end())
                    return false;

                stages[i] = new_stage;
                return true;
            }
        }

        return false;
    }

    int RenderStagePipeline::GetStageIndex(RenderStage *stage) const
    {
        auto it = std::find(stages.begin(),stages.end(),stage);
        if(it == stages.end())
            return -1;

        return static_cast<int>(std::distance(stages.begin(),it));
    }

    int RenderStagePipeline::GetStageIndex(const char *stage_name) const
    {
        if(!stage_name)
            return -1;

        for(size_t i=0;i<stages.size();++i)
        {
            RenderStage *stage = stages[i];
            if(stage && stage->GetName() && std::strcmp(stage->GetName(),stage_name) == 0)
                return static_cast<int>(i);
        }

        return -1;
    }

    void RenderStagePipeline::Execute(RenderStageContext &context)
    {
        for(RenderStage *stage:stages)
        {
            if(!stage || !stage->IsEnabled())
                continue;

            stage->Prepare(context);
            stage->Execute(context);
        }
    }
}//namespace hgl::graph
