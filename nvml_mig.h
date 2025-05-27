#ifndef NVML_MIG_H
#define NVML_MIG_H

#include <nvml.h>
#include <vector>
#include <map>
#include <string>
#include <chrono>

// GPU metrics structure
struct GPUMetrics {
    std::chrono::system_clock::time_point timestamp;
    unsigned int gpuUtilization;
    unsigned int memoryUtilization;
    unsigned long long memoryUsed;
    unsigned long long memoryFree;
    unsigned long long memoryTotal;
    unsigned int powerUsage;
    unsigned int temperature;
};

struct MIGDeviceInfo {
    nvmlDevice_t migDevice;
    unsigned int instanceId;
    unsigned int computeInstanceId;
    nvmlDeviceAttributes_t attributes;
    std::string uuid;
    unsigned long long memorySize;
    unsigned int multiprocessorCount;
    unsigned int maxComputeInstances;
    unsigned int currentComputeInstances;
};

struct GPUInstanceProfile {
    unsigned int profileId;
    unsigned long long memorySizeMB;
    unsigned int multiprocessorCount;
    unsigned int maxComputeInstances;
    std::string name;
};

class NVMLMIGManager {
private:
    std::vector<nvmlDevice_t> parentDevices;
    std::map<nvmlDevice_t, std::vector<MIGDeviceInfo>> migInstances;
    
public:
    NVMLMIGManager(const std::vector<nvmlDevice_t>& devices);
    
    // Enable MIG mode
    bool enableMIGMode(unsigned int deviceIndex);
    
    // Disable MIG mode
    bool disableMIGMode(unsigned int deviceIndex);
    
    // Check if MIG mode is enabled
    bool isMIGModeEnabled(unsigned int deviceIndex);
    
    // Get available GPU Instance profiles
    std::vector<GPUInstanceProfile> getAvailableInstanceProfiles(unsigned int deviceIndex);
    
    // Create GPU Instance
    bool createGPUInstance(unsigned int deviceIndex, unsigned int profileId, unsigned int& instanceId);
    
    // Destroy GPU Instance
    bool destroyGPUInstance(unsigned int deviceIndex, unsigned int instanceId);
    
    // Get all GPU Instances
    std::vector<MIGDeviceInfo> getAllGPUInstances(unsigned int deviceIndex);
    
    // Create Compute Instance
    bool createComputeInstance(unsigned int deviceIndex, unsigned int gpuInstanceId, 
                              unsigned int profileId, unsigned int& computeInstanceId);
    
    // Get metrics from MIG device
    GPUMetrics getMIGDeviceMetrics(const MIGDeviceInfo& migDevice);
    
    // Get metrics from all MIG devices
    std::map<std::string, GPUMetrics> getAllMIGMetrics();
};

#endif // NVML_MIG_H 