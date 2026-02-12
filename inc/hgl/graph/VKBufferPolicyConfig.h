#pragma once

#include<hgl/graph/VKBuffer.h>
#include<string>
#include<unordered_map>

VK_NAMESPACE_BEGIN

/**
 * @brief BufferPolicyConfig holds the complete policy configuration for a buffer category
 *        Corresponds to one [Category] block in BufferPolicy.txt
 */
struct BufferPolicyConfig
{
    // Identification
    std::string name;                   // Category name (e.g., "CameraUBO")
    
    // Basic usage
    std::string usage;                  // "UBO", "SSBO", "VAB", "IBO", "TEX_TILE", "RAW"
    BufferPriority priority;
    BufferUpdateRate updateRate;
    BufferSubmitTiming submitTiming;
    
    // Timing and latency
    uint32_t maxLatency;                // Frames (or 0 for AUTO)
    
    // Budget and queueing
    std::string budgetGroup;            // Budget group name
    VkDeviceSize budgetLimit;           // Bytes (or 0 for AUTO)
    bool queueing;                      // Whether to participate in queue
    
    // Splitting and dropping
    BufferSplitPolicy splitPolicy;
    VkDeviceSize splitChunk;            // Bytes (or 0 for AUTO)
    BufferDropPolicy dropPolicy;
    
    // Deadlines and promotion
    BufferDeadlinePolicy deadlinePolicy;
    uint32_t deadline;                  // Frames (or 0 for AUTO)
    BufferPromotePolicy promotePolicy;
    std::string promoteRule;            // Expression like "latency>2f"
    
    // Memory strategies
    BufferMemoryPolicy memoryPolicy;
    BufferCpuResident cpuResident;
    
    // Optional settings for specific memory policies
    uint32_t ringFrameCount;            // For RING: cycle frames (default 3)
    BufferCpuResident stagedPersist;    // For STAGED: keep staging buffer
    BufferCommitPolicy commitPolicy;    // AUTO, STAGED_ONLY, ALWAYS, MANUAL
    
    // Developer notes
    std::string devNotes;
    
    // Constructors
    BufferPolicyConfig() 
        : priority(BufferPriority::NORMAL)
        , updateRate(BufferUpdateRate::RARE)
        , submitTiming(BufferSubmitTiming::DEFERRED)
        , maxLatency(2)
        , budgetGroup("GLOBAL")
        , budgetLimit(0)
        , queueing(true)
        , splitPolicy(BufferSplitPolicy::NO_SPLIT)
        , splitChunk(0)
        , dropPolicy(BufferDropPolicy::NEVER)
        , deadlinePolicy(BufferDeadlinePolicy::NONE)
        , deadline(0)
        , promotePolicy(BufferPromotePolicy::NONE)
        , memoryPolicy(BufferMemoryPolicy::AUTO)
        , cpuResident(BufferCpuResident::AUTO)
        , ringFrameCount(3)
        , stagedPersist(BufferCpuResident::AUTO)
        , commitPolicy(BufferCommitPolicy::Auto)
    {
    }
};

/**
 * @brief BufferPolicyReader reads and manages buffer policies from BufferPolicy.txt
 *        Provides lookup by policy name
 */
class BufferPolicyReader
{
public:
    BufferPolicyReader();
    ~BufferPolicyReader();
    
    /**
     * @brief Load policies from a BufferPolicy.txt format file
     * @param filepath Full path to BufferPolicy.txt
     * @return true if loading succeeded, false on error
     */
    bool LoadFromFile(const std::string &filepath);
    
    /**
     * @brief Get a policy configuration by name
     * @param policyName Name of the policy (e.g., "CameraUBO")
     * @return Pointer to BufferPolicyConfig, or nullptr if not found
     */
    const BufferPolicyConfig* GetPolicyByName(const std::string &policyName) const;
    
    /**
     * @brief Get all loaded policy names
     * @return Vector of policy names
     */
    std::vector<std::string> GetPolicyNames() const;
    
    /**
     * @brief Get total number of loaded policies
     */
    size_t GetPolicyCount() const { return policies.size(); }
    
    /**
     * @brief Clear all loaded policies
     */
    void Clear() { policies.clear(); }
    
private:
    std::unordered_map<std::string, BufferPolicyConfig> policies;
    
    // Helper functions for parsing
    static bool ParseEnum_Priority(const std::string &str, BufferPriority &out);
    static bool ParseEnum_UpdateRate(const std::string &str, BufferUpdateRate &out);
    static bool ParseEnum_SubmitTiming(const std::string &str, BufferSubmitTiming &out);
    static bool ParseEnum_DropPolicy(const std::string &str, BufferDropPolicy &out);
    static bool ParseEnum_DeadlinePolicy(const std::string &str, BufferDeadlinePolicy &out);
    static bool ParseEnum_PromotePolicy(const std::string &str, BufferPromotePolicy &out);
    static bool ParseEnum_MemoryPolicy(const std::string &str, BufferMemoryPolicy &out);
    static bool ParseEnum_CpuResident(const std::string &str, BufferCpuResident &out);
    static bool ParseEnum_SplitPolicy(const std::string &str, BufferSplitPolicy &out);
    static bool ParseEnum_CommitPolicy(const std::string &str, BufferCommitPolicy &out);
    
    // Helper to parse size strings like "8M", "256K", etc.
    static bool ParseSize(const std::string &str, VkDeviceSize &out);
    
    // Helper to parse frame count strings like "0f", "2f", "AUTO", etc.
    static bool ParseFrameCount(const std::string &str, uint32_t &out);
    
    // Helper to trim whitespace
    static std::string Trim(const std::string &str);
};

VK_NAMESPACE_END
