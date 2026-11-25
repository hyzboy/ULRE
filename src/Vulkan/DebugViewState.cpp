#include<hgl/vk/DebugViewState.h>
#include<hgl/sync/Mutex.h>
#include<hgl/log/Log.h>
#include<stdlib.h>

namespace hgl::graph
{
    namespace
    {
        using namespace mtl;

        struct Status
        {
            hgl::Mutex lock;
            DebugViewMode requested=DebugViewMode::Off;
            DebugViewMode applied=DebugViewMode::Off;
            const void *owner=nullptr;
            uint64_t target_id=0;
            uint32_t batches=0;
            uint32_t entities=0;
            const char *error=nullptr;
        };

        Status &SharedStatus()
        {
            static Status status;
            static const bool initialized=[]()
            {
                const char *name=::getenv("ULRE_DEBUG_VIEW");
                if(name && name[0])
                {
                    if(!ParseDebugViewMode(name,status.requested))
                        GLogWarning(u8"[DebugView] Invalid ULRE_DEBUG_VIEW: %s",name);
                }
                return true;
            }();
            (void)initialized;
            return status;
        }
    }

    bool DebugViewState::RequestMode(mtl::DebugViewMode mode)
    {
        if(static_cast<uint32_t>(mode)>=mtl::DebugViewModeCount) return false;
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        status.requested=mode;
        return true;
    }

    mtl::DebugViewMode DebugViewState::GetRequestedMode()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.requested;
    }

    mtl::DebugViewMode DebugViewState::GetAppliedMode()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.applied;
    }

    uint64_t DebugViewState::GetTargetID()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.target_id;
    }

    uint32_t DebugViewState::GetTrackedCount()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        return status.entities;
    }

    AnsiString DebugViewState::GetStatusJSON()
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        AnsiString json="{\"valid\":true,\"mode\":\"";
        json+=mtl::GetDebugViewModeName(status.applied);
        json+="\",\"requested_mode\":\"";
        json+=mtl::GetDebugViewModeName(status.requested);
        json+="\",\"target_id\":";
        json+=AnsiString::numberOf(status.target_id);
        json+=",\"batches\":";
        json+=AnsiString::numberOf(status.batches);
        json+=",\"tracked_entities\":";
        json+=AnsiString::numberOf(status.entities);
        if(status.error)
        {
            json+=",\"error\":\"";
            json+=status.error;
            json+="\"";
        }
        json+="}";
        return json;
    }

    void DebugViewState::SetStatus(const void *owner,mtl::DebugViewMode mode,uint64_t target_id,
                                   uint32_t batches,const char *error,uint32_t entities)
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        status.owner=owner;
        status.applied=mode;
        status.target_id=target_id;
        status.batches=batches;
        status.entities=entities;
        status.error=error;
    }

    void DebugViewState::ClearStatus(const void *owner,bool reset_requested)
    {
        Status &status=SharedStatus();
        hgl::MutexLock lock(&status.lock);
        if(status.owner!=owner) return;
        if(reset_requested) status.requested=mtl::DebugViewMode::Off;
        status.applied=mtl::DebugViewMode::Off;
        status.owner=nullptr;
        status.target_id=0;
        status.batches=0;
        status.entities=0;
        status.error=nullptr;
    }
}
