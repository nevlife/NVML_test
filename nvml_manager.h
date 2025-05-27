#ifndef NVML_MANAGER_H
#define NVML_MANAGER_H

#include "nvml_types.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

class NVMLManager {
private:
    std::vector<GPUInfo> gpuDevices;
    std::vector<UnitInfo> unitDevices;
    std::atomic<bool> running;
    std::atomic<bool> initialized;
    
    // 모니터링 스레드
    std::thread monitorThread;
    std::thread eventThread;
    
    // 콜백 함수들
    std::function<void(const GPUMetrics&)> metricsCallback;
    std::function<void(const EventInfo&)> eventCallback;
    std::function<void(const std::vector<ProcessInfo>&)> processCallback;
    
    // 이벤트 처리
    nvmlEventSet_t eventSet;
    std::queue<EventInfo> eventQueue;
    std::mutex eventMutex;
    std::condition_variable eventCV;
    
    // 설정
    int monitoringInterval = 1000; // ms
    bool enableEventMonitoring = true;
    bool enableProcessMonitoring = true;

public:
    NVMLManager();
    ~NVMLManager();
    
    // 초기화 및 정리
    bool initialize();
    void shutdown();
    
    // GPU 정보 조회
    std::vector<GPUInfo> getGPUInfo();
    std::vector<UnitInfo> getUnitInfo();
    
    // 메트릭 조회
    GPUMetrics getGPUMetrics(unsigned int deviceIndex);
    std::vector<GPUMetrics> getAllGPUMetrics();
    
    // 프로세스 정보
    std::vector<ProcessInfo> getRunningProcesses(unsigned int deviceIndex);
    std::vector<ProcessInfo> getAllRunningProcesses();
    
    // BAR1 메모리 정보
    BAR1MemoryInfo getBAR1MemoryInfo(unsigned int deviceIndex);
    
    // vGPU 정보
    std::vector<VGPUInfo> getVGPUInfo(unsigned int deviceIndex);
    
    // 모니터링 제어
    void startMonitoring();
    void stopMonitoring();
    void setMonitoringInterval(int intervalMs);
    
    // 콜백 설정
    void setMetricsCallback(std::function<void(const GPUMetrics&)> callback);
    void setEventCallback(std::function<void(const EventInfo&)> callback);
    void setProcessCallback(std::function<void(const std::vector<ProcessInfo>&)> callback);
    
    // 이벤트 등록
    bool registerEvents(unsigned int deviceIndex, unsigned long long eventTypes);
    bool unregisterEvents(unsigned int deviceIndex);
    
    // 유틸리티 함수
    std::string getErrorString(nvmlReturn_t result);
    bool isDeviceValid(unsigned int deviceIndex);
    std::string getDriverVersion();
    std::string getNVMLVersion();
    std::string getCUDAVersion();

private:
    // 내부 함수들
    bool initializeDevices();
    bool initializeUnits();
    bool initializeEvents();
    void monitoringLoop();
    void eventLoop();
    void processEvents();
    GPUMetrics collectDeviceMetrics(const GPUInfo& gpu);
    std::vector<ProcessInfo> collectProcessInfo(nvmlDevice_t device);
    void handleEvent(const nvmlEventData_t& eventData);
    std::string eventTypeToString(unsigned long long eventType);
};

#endif // NVML_MANAGER_H