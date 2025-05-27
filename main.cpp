#include "nvml_manager.h"
#include "nvml_field_queries.cpp"
#include "nvml_accounting.cpp"
#include "nvml_mig.cpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

void printGPUInfo(const std::vector<GPUInfo>& gpus) {
    std::cout << "\n=== GPU Information ===" << std::endl;
    for (const auto& gpu : gpus) {
        std::cout << "GPU " << gpu.index << ":" << std::endl;
        std::cout << "  Name: " << gpu.name << std::endl;
        std::cout << "  UUID: " << gpu.uuid << std::endl;
        std::cout << "  Serial: " << gpu.serial << std::endl;
        std::cout << "  PCI Bus ID: " << gpu.pciBusId << std::endl;
        std::cout << "  Architecture: " << gpu.architecture << std::endl;
        std::cout << "  CUDA Capability: " << gpu.cudaMajor << "." << gpu.cudaMinor << std::endl;
        std::cout << "  Total Memory: " << (gpu.totalMemory / 1024 / 1024) << " MB" << std::endl;
        std::cout << std::endl;
    }
}

void printGPUMetrics(const GPUMetrics& metrics, unsigned int gpuIndex) {
    std::cout << "GPU " << gpuIndex << " Metrics:" << std::endl;
    std::cout << "  GPU Utilization: " << metrics.gpuUtilization << "%" << std::endl;
    std::cout << "  Memory Utilization: " << metrics.memoryUtilization << "%" << std::endl;
    std::cout << "  Temperature: " << metrics.temperature << "°C" << std::endl;
    std::cout << "  Fan Speed: " << metrics.fanSpeed << "%" << std::endl;
    std::cout << "  Power Usage: " << metrics.powerUsage << "mW" << std::endl;
    std::cout << "  Memory Used: " << (metrics.memoryUsed / 1024 / 1024) << " MB / " 
              << (metrics.memoryTotal / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Graphics Clock: " << metrics.graphicsClock << " MHz" << std::endl;
    std::cout << "  Memory Clock: " << metrics.memoryClock << " MHz" << std::endl;
    std::cout << "  ECC Errors: Single=" << metrics.eccSingleBit 
              << ", Double=" << metrics.eccDoubleBit << std::endl;
    std::cout << std::endl;
}

void printProcessInfo(const std::vector<ProcessInfo>& processes) {
    if (processes.empty()) return;
    
    std::cout << "Running Processes:" << std::endl;
    for (const auto& proc : processes) {
        std::cout << "  PID " << proc.pid << " (" << proc.name << "): " 
                  << (proc.usedGpuMemory / 1024 / 1024) << " MB, Type: " 
                  << (proc.type == NVML_PROCESS_TYPE_COMPUTE ? "Compute" : "Graphics") << std::endl;
    }
    std::cout << std::endl;
}

void onMetricsUpdate(const GPUMetrics& metrics) {
    static unsigned int counter = 0;
    if (++counter % 10 == 0) { // 10초마다 출력
        std::cout << "\n[Real-time] ";
        printGPUMetrics(metrics, 0); // 첫 번째 GPU만 출력
    }
}

void onEventReceived(const EventInfo& event) {
    std::cout << "\n!!! GPU EVENT DETECTED !!!" << std::endl;
    std::cout << "Event: " << event.description << std::endl;
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::seconds>(
        event.timestamp.time_since_epoch()).count() << std::endl;
    std::cout << std::endl;
}

void onProcessUpdate(const std::vector<ProcessInfo>& processes) {
    static auto lastUpdate = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count() >= 30) {
        std::cout << "\n[Process Update] ";
        printProcessInfo(processes);
        lastUpdate = now;
    }
}

int main() {
    std::cout << "NVML GPU Monitoring System" << std::endl;
    std::cout << "=========================" << std::endl;
    
    // NVML 매니저 초기화
    NVMLManager manager;
    if (!manager.initialize()) {
        std::cerr << "Failed to initialize NVML" << std::endl;
        return -1;
    }
    
    // 시스템 정보 출력
    std::cout << "Driver Version: " << manager.getDriverVersion() << std::endl;
    std::cout << "NVML Version: " << manager.getNVMLVersion() << std::endl;
    std::cout << "CUDA Version: " << manager.getCUDAVersion() << std::endl;
    
    // GPU 정보 출력
    auto gpus = manager.getGPUInfo();
    printGPUInfo(gpus);
    
    if (gpus.empty()) {
        std::cout << "No GPUs found!" << std::endl;
        return -1;
    }
    
    // Field Queries 테스트
    std::cout << "\n=== Field Queries Test ===" << std::endl;
    NVMLFieldQueries fieldQueries;
    auto basicFields = fieldQueries.queryAllBasicFields(gpus[0].device);
    
    std::cout << "Basic Fields for GPU 0:" << std::endl;
    for (const auto& field : basicFields) {
        std::cout << "  " << field.first << ": ";
        switch (field.second.valueType) {
            case NVML_VALUE_TYPE_DOUBLE:
                std::cout << field.second.dVal;
                break;
            case NVML_VALUE_TYPE_UNSIGNED_INT:
                std::cout << field.second.uiVal;
                break;
            case NVML_VALUE_TYPE_UNSIGNED_LONG:
                std::cout << field.second.ulVal;
                break;
            case NVML_VALUE_TYPE_UNSIGNED_LONG_LONG:
                std::cout << field.second.ullVal;
                break;
            case NVML_VALUE_TYPE_SIGNED_LONG_LONG:
                std::cout << field.second.sllVal;
                break;
            default:
                std::cout << "Unknown type";
        }
        std::cout << std::endl;
    }
    
    // MIG 관리 테스트
    std::cout << "\n=== MIG Management Test ===" << std::endl;
    std::vector<nvmlDevice_t> devices;
    for (const auto& gpu : gpus) {
        devices.push_back(gpu.device);
    }
    
    NVMLMIGManager migManager(devices);
    if (migManager.isMIGModeEnabled(0)) {
        std::cout << "MIG Mode is enabled on GPU 0" << std::endl;
        auto migInstances = migManager.getAllGPUInstances(0);
        std::cout << "Found " << migInstances.size() << " MIG instances" << std::endl;
        
        for (const auto& instance : migInstances) {
            std::cout << "  Instance " << instance.instanceId 
                      << ": Memory=" << (instance.memorySize / 1024 / 1024) << "MB" << std::endl;
        }
    } else {
        std::cout << "MIG Mode is not enabled" << std::endl;
    }
    
    // Accounting 테스트
    std::cout << "\n=== Accounting Test ===" << std::endl;
    NVMLAccounting accounting(devices);
    
    if (accounting.isAccountingEnabled(0)) {
        std::cout << "Accounting is enabled on GPU 0" << std::endl;
        auto accountingStats = accounting.getRunningProcessStats(0);
        std::cout << "Found " << accountingStats.size() << " processes with accounting data" << std::endl;
        
        for (const auto& proc : accountingStats) {
            std::cout << "  PID " << proc.pid << " (" << proc.processName << "):" << std::endl;
            std::cout << "    Max Memory: " << (proc.maxMemoryUsage / 1024 / 1024) << " MB" << std::endl;
            std::cout << "    Runtime: " << (proc.time / 1000000) << " seconds" << std::endl;
            std::cout << "    Running: " << (proc.isRunning ? "Yes" : "No") << std::endl;
        }
    } else {
        std::cout << "Accounting is not enabled. Enabling..." << std::endl;
        if (accounting.enableAccounting(0)) {
            std::cout << "Accounting enabled successfully" << std::endl;
        } else {
            std::cout << "Failed to enable accounting" << std::endl;
        }
    }
    
    // 콜백 함수 설정
    manager.setMetricsCallback(onMetricsUpdate);
    manager.setEventCallback(onEventReceived);
    manager.setProcessCallback(onProcessUpdate);
    
    // 이벤트 등록
    for (size_t i = 0; i < gpus.size(); i++) {
        unsigned long long eventTypes = 
            nvmlEventTypeSingleBitEccError |
            nvmlEventTypeDoubleBitEccError |
            nvmlEventTypePState |
            nvmlEventTypeXidCriticalError;
        manager.registerEvents(i, eventTypes);
    }
    
    // 모니터링 시작
    std::cout << "\n=== Starting Real-time Monitoring ===" << std::endl;
    std::cout << "Press Enter to stop monitoring..." << std::endl;
    
    manager.setMonitoringInterval(1000); // 1초 간격
    manager.startMonitoring();
    
    // 초기 상태 출력
    for (size_t i = 0; i < gpus.size(); i++) {
        auto metrics = manager.getGPUMetrics(i);
        printGPUMetrics(metrics, i);
        
        auto processes = manager.getRunningProcesses(i);
        printProcessInfo(processes);
        
        auto bar1Info = manager.getBAR1MemoryInfo(i);
        if (bar1Info.bar1Total > 0) {
            std::cout << "BAR1 Memory - Total: " << (bar1Info.bar1Total / 1024 / 1024) 
                      << "MB, Used: " << (bar1Info.bar1Used / 1024 / 1024) 
                      << "MB, Free: " << (bar1Info.bar1Free / 1024 / 1024) << "MB" << std::endl;
        }
    }
    
    // 사용자 입력 대기
    std::cin.get();
    
    // 모니터링 중지
    std::cout << "\nStopping monitoring..." << std::endl;
    manager.stopMonitoring();
    
    std::cout << "Monitoring stopped. Goodbye!" << std::endl;
    return 0;
}