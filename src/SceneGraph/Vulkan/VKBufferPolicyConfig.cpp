#include<hgl/graph/VKBufferPolicyConfig.h>
#include<fstream>
#include<sstream>
#include<algorithm>
#include<cctype>

VK_NAMESPACE_BEGIN

BufferPolicyReader::BufferPolicyReader()
{
}

BufferPolicyReader::~BufferPolicyReader()
{
}

std::string BufferPolicyReader::Trim(const std::string &str)
{
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start))
        start++;
    
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    
    return std::string(start, end + 1);
}

bool BufferPolicyReader::ParseEnum_Priority(const std::string &str, BufferPriority &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "CRITICAL") { out = BufferPriority::CRITICAL; return true; }
    if (trimmed == "HIGH") { out = BufferPriority::HIGH; return true; }
    if (trimmed == "NORMAL") { out = BufferPriority::NORMAL; return true; }
    if (trimmed == "LOW") { out = BufferPriority::LOW; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_UpdateRate(const std::string &str, BufferUpdateRate &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "PER_FRAME") { out = BufferUpdateRate::PER_FRAME; return true; }
    if (trimmed == "FREQUENT") { out = BufferUpdateRate::FREQUENT; return true; }
    if (trimmed == "BURST") { out = BufferUpdateRate::BURST; return true; }
    if (trimmed == "SPARSE") { out = BufferUpdateRate::SPARSE; return true; }
    if (trimmed == "RARE") { out = BufferUpdateRate::RARE; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_SubmitTiming(const std::string &str, BufferSubmitTiming &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "IMMEDIATE") { out = BufferSubmitTiming::IMMEDIATE; return true; }
    if (trimmed == "SAME_FRAME") { out = BufferSubmitTiming::SAME_FRAME; return true; }
    if (trimmed == "NEXT_FRAME_OK") { out = BufferSubmitTiming::NEXT_FRAME_OK; return true; }
    if (trimmed == "DEFERRED") { out = BufferSubmitTiming::DEFERRED; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_DropPolicy(const std::string &str, BufferDropPolicy &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "NEVER") { out = BufferDropPolicy::NEVER; return true; }
    if (trimmed == "DROP_OLD") { out = BufferDropPolicy::DROP_OLD; return true; }
    if (trimmed == "DROP_NEW") { out = BufferDropPolicy::DROP_NEW; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_DeadlinePolicy(const std::string &str, BufferDeadlinePolicy &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "NONE") { out = BufferDeadlinePolicy::NONE; return true; }
    if (trimmed == "SOFT") { out = BufferDeadlinePolicy::SOFT; return true; }
    if (trimmed == "HARD") { out = BufferDeadlinePolicy::HARD; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_PromotePolicy(const std::string &str, BufferPromotePolicy &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "NONE") { out = BufferPromotePolicy::NONE; return true; }
    if (trimmed == "AUTO_RAISE") { out = BufferPromotePolicy::AUTO_RAISE; return true; }
    if (trimmed == "FORCE_HIGH") { out = BufferPromotePolicy::FORCE_HIGH; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_MemoryPolicy(const std::string &str, BufferMemoryPolicy &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "REBAR") { out = BufferMemoryPolicy::REBAR; return true; }
    if (trimmed == "RING") { out = BufferMemoryPolicy::RING; return true; }
    if (trimmed == "STAGED") { out = BufferMemoryPolicy::STAGED; return true; }
    if (trimmed == "AUTO") { out = BufferMemoryPolicy::AUTO; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_CpuResident(const std::string &str, BufferCpuResident &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "KEEP") { out = BufferCpuResident::KEEP; return true; }
    if (trimmed == "RELEASE") { out = BufferCpuResident::RELEASE; return true; }
    if (trimmed == "AUTO") { out = BufferCpuResident::AUTO; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_SplitPolicy(const std::string &str, BufferSplitPolicy &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "NO_SPLIT") { out = BufferSplitPolicy::NO_SPLIT; return true; }
    if (trimmed == "ALLOW_SPLIT") { out = BufferSplitPolicy::ALLOW_SPLIT; return true; }
    if (trimmed == "PREFER_SPLIT") { out = BufferSplitPolicy::PREFER_SPLIT; return true; }
    return false;
}

bool BufferPolicyReader::ParseEnum_CommitPolicy(const std::string &str, BufferCommitPolicy &out)
{
    std::string trimmed = Trim(str);
    if (trimmed == "AUTO") { out = BufferCommitPolicy::Auto; return true; }
    if (trimmed == "STAGED_ONLY") { out = BufferCommitPolicy::StagedOnly; return true; }
    if (trimmed == "ALWAYS") { out = BufferCommitPolicy::Always; return true; }
    if (trimmed == "MANUAL") { out = BufferCommitPolicy::Manual; return true; }
    return false;
}

bool BufferPolicyReader::ParseSize(const std::string &str, VkDeviceSize &out)
{
    std::string trimmed = Trim(str);
    
    if (trimmed == "AUTO" || trimmed == "0")
    {
        out = 0;
        return true;
    }
    
    // Parse "8M", "256K", "1024" format
    char suffix = 0;
    if (!trimmed.empty() && (std::isalpha(trimmed.back())))
    {
        suffix = trimmed.back();
        trimmed.pop_back();
    }
    
    try {
        size_t value = std::stoull(trimmed);
        
        switch (suffix)
        {
            case 'M': case 'm': value *= (1024 * 1024); break;
            case 'K': case 'k': value *= 1024; break;
            case 0: break;  // bytes
            default: return false;
        }
        
        out = value;
        return true;
    }
    catch (...) {
        return false;
    }
}

bool BufferPolicyReader::ParseFrameCount(const std::string &str, uint32_t &out)
{
    std::string trimmed = Trim(str);
    
    if (trimmed == "AUTO")
    {
        out = 0;  // 0 means AUTO
        return true;
    }
    
    // Remove 'f' suffix if present
    if (!trimmed.empty() && (trimmed.back() == 'f' || trimmed.back() == 'F'))
        trimmed.pop_back();
    
    try {
        uint32_t value = std::stoul(trimmed);
        out = value;
        return true;
    }
    catch (...) {
        return false;
    }
}

bool BufferPolicyReader::LoadFromFile(const std::string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
        return false;
    
    policies.clear();
    std::string line;
    BufferPolicyConfig currentConfig;
    bool inCategory = false;
    
    while (std::getline(file, line))
    {
        // Remove comments
        size_t commentPos = line.find(';');
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);
        
        line = Trim(line);
        
        // Skip empty lines
        if (line.empty())
            continue;
        
        // Check for [Category] section header
        if (line.front() == '[' && line.back() == ']')
        {
            // Save previous config if exists
            if (inCategory && !currentConfig.name.empty())
            {
                policies[currentConfig.name] = currentConfig;
            }
            
            // Start new category
            currentConfig = BufferPolicyConfig();
            inCategory = true;
            continue;
        }
        
        // Parse key=value pairs
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos)
            continue;
        
        std::string key = Trim(line.substr(0, eqPos));
        std::string value = Trim(line.substr(eqPos + 1));
        
        // Map keys to fields
        if (key == "Name")
            currentConfig.name = value;
        else if (key == "Usage")
            currentConfig.usage = value;
        else if (key == "Priority")
            ParseEnum_Priority(value, currentConfig.priority);
        else if (key == "UpdateRate")
            ParseEnum_UpdateRate(value, currentConfig.updateRate);
        else if (key == "SubmitTiming")
            ParseEnum_SubmitTiming(value, currentConfig.submitTiming);
        else if (key == "MaxLatency")
            ParseFrameCount(value, currentConfig.maxLatency);
        else if (key == "BudgetGroup")
            currentConfig.budgetGroup = value;
        else if (key == "BudgetLimit")
            ParseSize(value, currentConfig.budgetLimit);
        else if (key == "Queueing")
        {
            std::string trimmedVal = Trim(value);
            currentConfig.queueing = (trimmedVal == "ENABLED" || trimmedVal == "true");
        }
        else if (key == "SplitPolicy")
            ParseEnum_SplitPolicy(value, currentConfig.splitPolicy);
        else if (key == "SplitChunk")
            ParseSize(value, currentConfig.splitChunk);
        else if (key == "DropPolicy")
            ParseEnum_DropPolicy(value, currentConfig.dropPolicy);
        else if (key == "DeadlinePolicy")
            ParseEnum_DeadlinePolicy(value, currentConfig.deadlinePolicy);
        else if (key == "Deadline")
            ParseFrameCount(value, currentConfig.deadline);
        else if (key == "PromotePolicy")
            ParseEnum_PromotePolicy(value, currentConfig.promotePolicy);
        else if (key == "PromoteRule")
            currentConfig.promoteRule = value;
        else if (key == "MemoryPolicy")
            ParseEnum_MemoryPolicy(value, currentConfig.memoryPolicy);
        else if (key == "CpuResident")
            ParseEnum_CpuResident(value, currentConfig.cpuResident);
        else if (key == "RingFrameCount")
            ParseFrameCount(value, currentConfig.ringFrameCount);
        else if (key == "StagedPersist")
            ParseEnum_CpuResident(value, currentConfig.stagedPersist);
        else if (key == "CommitPolicy")
            ParseEnum_CommitPolicy(value, currentConfig.commitPolicy);
        else if (key == "DevNotes")
            currentConfig.devNotes = value;
    }
    
    // Save last config if exists
    if (inCategory && !currentConfig.name.empty())
    {
        policies[currentConfig.name] = currentConfig;
    }
    
    file.close();
    return !policies.empty();
}

const BufferPolicyConfig* BufferPolicyReader::GetPolicyByName(const std::string &policyName) const
{
    auto it = policies.find(policyName);
    if (it != policies.end())
        return &it->second;
    return nullptr;
}

std::vector<std::string> BufferPolicyReader::GetPolicyNames() const
{
    std::vector<std::string> names;
    for (const auto &pair : policies)
        names.push_back(pair.first);
    return names;
}

VK_NAMESPACE_END
