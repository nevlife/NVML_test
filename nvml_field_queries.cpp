#include "nvml_manager.h"
#include <vector>
#include <map>

class NVMLFieldQueries {
private:
    std::map<nvmlFieldValue_t, std::string> fieldNames;
    
public:
    NVMLFieldQueries() {
        initializeFieldNames();
    }
    
    void initializeFieldNames() {
        // Field ID와 이름 매핑
        fieldNames[NVML_FI_DEV_NVML_VERSION] = "NVML Version";
        fieldNames[NVML_FI_DEV_CUDA_DRIVER_VERSION] = "CUDA Driver Version";
        fieldNames[NVML_FI_DEV_COUNT] = "Device Count";
        fieldNames[NVML_FI_DEV_SERIAL] = "Serial Number";
        fieldNames[NVML_FI_DEV_UUID] = "UUID";
        fieldNames[NVML_FI_DEV_MINOR_NUMBER] = "Minor Number";
        fieldNames[NVML_FI_DEV_OEM_INFOROM_VER] = "OEM InfoROM Version";
        fieldNames[NVML_FI_DEV_PCI_BUS_ID] = "PCI Bus ID";
        fieldNames[NVML_FI_DEV_POWER_USAGE] = "Power Usage";
        fieldNames[NVML_FI_DEV_POWER_LIMIT] = "Power Limit";
        fieldNames[NVML_FI_DEV_MAX_POWER_LIMIT] = "Max Power Limit";
        fieldNames[NVML_FI_DEV_MIN_POWER_LIMIT] = "Min Power Limit";
        fieldNames[NVML_FI_DEV_TEMPERATURE_GPU] = "GPU Temperature";
        fieldNames[NVML_FI_DEV_TEMPERATURE_MEMORY] = "Memory Temperature";
        fieldNames[NVML_FI_DEV_CLOCK_GRAPHICS] = "Graphics Clock";
        fieldNames[NVML_FI_DEV_CLOCK_SM] = "SM Clock";
        fieldNames[NVML_FI_DEV_CLOCK_MEM] = "Memory Clock";
        fieldNames[NVML_FI_DEV_CLOCK_VIDEO] = "Video Clock";
        fieldNames[NVML_FI_DEV_UTILIZATION_GPU] = "GPU Utilization";
        fieldNames[NVML_FI_DEV_UTILIZATION_MEMORY] = "Memory Utilization";
        fieldNames[NVML_FI_DEV_UTILIZATION_ENCODER] = "Encoder Utilization";
        fieldNames[NVML_FI_DEV_UTILIZATION_DECODER] = "Decoder Utilization";
        fieldNames[NVML_FI_DEV_MEMORY_TOTAL] = "Total Memory";
        fieldNames[NVML_FI_DEV_MEMORY_USED] = "Used Memory";
        fieldNames[NVML_FI_DEV_MEMORY_FREE] = "Free Memory";
        fieldNames[NVML_FI_DEV_ECC_SBE_VOL_TOTAL] = "Single Bit ECC Errors (Volatile)";
        fieldNames[NVML_FI_DEV_ECC_DBE_VOL_TOTAL] = "Double Bit ECC Errors (Volatile)";
        fieldNames[NVML_FI_DEV_ECC_SBE_AGG_TOTAL] = "Single Bit ECC Errors (Aggregate)";
        fieldNames[NVML_FI_DEV_ECC_DBE_AGG_TOTAL] = "Double Bit ECC Errors (Aggregate)";
        fieldNames[NVML_FI_DEV_RETIRED_SBE] = "Retired Pages (Single Bit)";
        fieldNames[NVML_FI_DEV_RETIRED_DBE] = "Retired Pages (Double Bit)";
        fieldNames[NVML_FI_DEV_RETIRED_PENDING] = "Pending Retired Pages";
        fieldNames[NVML_FI_DEV_PCIE_LINK_GEN_CURRENT] = "PCIe Link Generation";
        fieldNames[NVML_FI_DEV_PCIE_LINK_WIDTH_CURRENT] = "PCIe Link Width";
        fieldNames[NVML_FI_DEV_PCIE_TX_THROUGHPUT] = "PCIe TX Throughput";
        fieldNames[NVML_FI_DEV_PCIE_RX_THROUGHPUT] = "PCIe RX Throughput";
        fieldNames[NVML_FI_DEV_FAN_SPEED] = "Fan Speed";
        fieldNames[NVML_FI_DEV_PERFORMANCE_STATE] = "Performance State";
        fieldNames[NVML_FI_DEV_THROTTLE_REASONS_SUPPORTED] = "Supported Throttle Reasons";
        fieldNames[NVML_FI_DEV_THROTTLE_REASONS_CURRENT] = "Current Throttle Reasons";
        fieldNames[NVML_FI_DEV_POWER_STATE] = "Power State";
        fieldNames[NVML_FI_DEV_POWER_SOURCE] = "Power Source";
        fieldNames[NVML_FI_DEV_MEMORY_ERROR_TOTAL] = "Total Memory Errors";
        fieldNames[NVML_FI_DEV_COMPUTE_MODE] = "Compute Mode";
        fieldNames[NVML_FI_DEV_PERSISTENCE_MODE] = "Persistence Mode";
        fieldNames[NVML_FI_DEV_ACCOUNTING_MODE] = "Accounting Mode";
        fieldNames[NVML_FI_DEV_ACCOUNTING_BUFFER_SIZE] = "Accounting Buffer Size";
        fieldNames[NVML_FI_DEV_DRIVER_VERSION] = "Driver Version";
        fieldNames[NVML_FI_DEV_VBIOS_VERSION] = "VBIOS Version";
        fieldNames[NVML_FI_DEV_INFOROM_VERSION_IMG] = "InfoROM Image Version";
        fieldNames[NVML_FI_DEV_INFOROM_VERSION_OEM] = "InfoROM OEM Version";
        fieldNames[NVML_FI_DEV_INFOROM_VERSION_ECC] = "InfoROM ECC Version";
        fieldNames[NVML_FI_DEV_INFOROM_VERSION_PWR] = "InfoROM Power Version";
    }
    
    // 다중 필드 값을 한 번에 조회
    std::map<std::string, nvmlValue_t> queryMultipleFields(nvmlDevice_t device, 
                                                           const std::vector<nvmlFieldValue_t>& fieldIds) {
        std::map<std::string, nvmlValue_t> results;
        
        if (fieldIds.empty()) return results;
        
        std::vector<nvmlFieldValue_t> values(fieldIds.size());
        for (size_t i = 0; i < fieldIds.size(); i++) {
            values[i].fieldId = fieldIds[i];
        }
        
        unsigned int valCount = fieldIds.size();
        nvmlReturn_t result = nvmlDeviceGetFieldValues(device, valCount, values.data());
        
        if (result == NVML_SUCCESS) {
            for (size_t i = 0; i < values.size(); i++) {
                if (values[i].nvmlReturn == NVML_SUCCESS) {
                    std::string fieldName = fieldNames.count(values[i].fieldId) ? 
                                           fieldNames[values[i].fieldId] : 
                                           "Unknown Field " + std::to_string(values[i].fieldId);
                    results[fieldName] = values[i].value;
                }
            }
        }
        
        return results;
    }
    
    // 모든 기본 필드 조회
    std::map<std::string, nvmlValue_t> queryAllBasicFields(nvmlDevice_t device) {
        std::vector<nvmlFieldValue_t> basicFields = {
            NVML_FI_DEV_POWER_USAGE,
            NVML_FI_DEV_POWER_LIMIT,
            NVML_FI_DEV_TEMPERATURE_GPU,
            NVML_FI_DEV_CLOCK_GRAPHICS,
            NVML_FI_DEV_CLOCK_MEM,
            NVML_FI_DEV_UTILIZATION_GPU,
            NVML_FI_DEV_UTILIZATION_MEMORY,
            NVML_FI_DEV_MEMORY_TOTAL,
            NVML_FI_DEV_MEMORY_USED,
            NVML_FI_DEV_MEMORY_FREE,
            NVML_FI_DEV_FAN_SPEED,
            NVML_FI_DEV_PERFORMANCE_STATE
        };
        
        return queryMultipleFields(device, basicFields);
    }
    
    // 성능 관련 필드만 조회
    std::map<std::string, nvmlValue_t> queryPerformanceFields(nvmlDevice_t device) {
        std::vector<nvmlFieldValue_t> perfFields = {
            NVML_FI_DEV_UTILIZATION_GPU,
            NVML_FI_DEV_UTILIZATION_MEMORY,
            NVML_FI_DEV_UTILIZATION_ENCODER,
            NVML_FI_DEV_UTILIZATION_DECODER,
            NVML_FI_DEV_CLOCK_GRAPHICS,
            NVML_FI_DEV_CLOCK_SM,
            NVML_FI_DEV_CLOCK_MEM,
            NVML_FI_DEV_CLOCK_VIDEO,
            NVML_FI_DEV_THROTTLE_REASONS_CURRENT
        };
        
        return queryMultipleFields(device, perfFields);
    }
    
    // 메모리 관련 필드만 조회
    std::map<std::string, nvmlValue_t> queryMemoryFields(nvmlDevice_t device) {
        std::vector<nvmlFieldValue_t> memFields = {
            NVML_FI_DEV_MEMORY_TOTAL,
            NVML_FI_DEV_MEMORY_USED,
            NVML_FI_DEV_MEMORY_FREE,
            NVML_FI_DEV_ECC_SBE_VOL_TOTAL,
            NVML_FI_DEV_ECC_DBE_VOL_TOTAL,
            NVML_FI_DEV_ECC_SBE_AGG_TOTAL,
            NVML_FI_DEV_ECC_DBE_AGG_TOTAL,
            NVML_FI_DEV_RETIRED_SBE,
            NVML_FI_DEV_RETIRED_DBE,
            NVML_FI_DEV_RETIRED_PENDING
        };
        
        return queryMultipleFields(device, memFields);
    }
    
    // 전력 관련 필드만 조회
    std::map<std::string, nvmlValue_t> queryPowerFields(nvmlDevice_t device) {
        std::vector<nvmlFieldValue_t> powerFields = {
            NVML_FI_DEV_POWER_USAGE,
            NVML_FI_DEV_POWER_LIMIT,
            NVML_FI_DEV_MAX_POWER_LIMIT,
            NVML_FI_DEV_MIN_POWER_LIMIT,
            NVML_FI_DEV_POWER_STATE,
            NVML_FI_DEV_POWER_SOURCE,
            NVML_FI_DEV_TEMPERATURE_GPU,
            NVML_FI_DEV_TEMPERATURE_MEMORY,
            NVML_FI_DEV_FAN_SPEED
        };
        
        return queryMultipleFields(device, powerFields);
    }
    
    // PCIe 관련 필드만 조회
    std::map<std::string, nvmlValue_t> queryPCIeFields(nvmlDevice_t device) {
        std::vector<nvmlFieldValue_t> pcieFields = {
            NVML_FI_DEV_PCIE_LINK_GEN_CURRENT,
            NVML_FI_DEV_PCIE_LINK_WIDTH_CURRENT,
            NVML_FI_DEV_PCIE_TX_THROUGHPUT,
            NVML_FI_DEV_PCIE_RX_THROUGHPUT,
            NVML_FI_DEV_PCI_BUS_ID
        };
        
        return queryMultipleFields(device, pcieFields);
    }
};