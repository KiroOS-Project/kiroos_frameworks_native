/*
 * Copyright 2026 KiroOS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// KiroOS Vulkan 1.1 layer inside the loader: see kiro_vk11.h.

#include "kiro_vk11.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <map>
#include <vector>

#include <cutils/properties.h>

#include "api.h"
#include "driver.h"
#include "kiro_vk11_types.h"

namespace vulkan {
namespace kiro {

namespace {

// Vulkan 1.1, header 130
constexpr uint32_t kApiVersion = (1u << 22) | (1u << 12) | 130u;

constexpr uint32_t kMaxPerSetDescriptors = 1024;
constexpr VkDeviceSize kMinMaxAllocation = 1ull << 30;
constexpr size_t kMaxCacheFile = 32u << 20;
constexpr auto kCacheSaveInterval = std::chrono::seconds(10);

// Beta 4.1: every Vulkan 1.1 core feature the layer provides also goes out under its KHR
// extension name, for the many apps and games written against 1.0 that look for the extension
// rather than the 1.1 core (same commands, see kProcs). Left out: the 1.1 extensions the Mali-T760
// cannot do (16-bit storage, multiview, variable pointers, sampler YCbCr, shader draw parameters,
// storage buffer storage class and relaxed block layout, which need the closed shader compiler),
// VK_KHR_maintenance2 (its structs would reach the 1.0 driver's image views) and
// VK_KHR_device_group (its swapchain commands are not implemented; groups of one device are).
const VkExtensionProperties kDeviceExtensions[] = {
    {VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, VK_EXT_MEMORY_BUDGET_SPEC_VERSION},
    {VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, VK_EXT_MEMORY_PRIORITY_SPEC_VERSION},
    {"VK_KHR_bind_memory2", 1},
    {"VK_KHR_dedicated_allocation", 3},
    {"VK_KHR_descriptor_update_template", 1},
    {"VK_KHR_external_fence", 1},
    {"VK_KHR_external_memory", 1},
    {"VK_KHR_external_semaphore", 1},
    {"VK_KHR_get_memory_requirements2", 1},
    {"VK_KHR_maintenance1", 2},
    {"VK_KHR_maintenance3", 1},
};
const VkExtensionProperties kInstanceExtensions[] = {
    {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
     VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION},
    {"VK_KHR_device_group_creation", 1},
    {"VK_KHR_external_fence_capabilities", 1},
    {"VK_KHR_external_memory_capabilities", 1},
    {"VK_KHR_external_semaphore_capabilities", 1},
};

struct BaseOut {
    VkStructureType sType;
    void* pNext;
};
struct BaseIn {
    VkStructureType sType;
    const void* pNext;
};

// handles as map keys, and the layer's own non-dispatchable handles
#if defined(__LP64__)
template <typename H>
uint64_t Key(H handle) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle));
}
template <typename H, typename T>
H ToHandle(T* object) {
    return reinterpret_cast<H>(object);
}
template <typename T, typename H>
T* FromHandle(H handle) {
    return reinterpret_cast<T*>(handle);
}
#else
template <typename H>
uint64_t Key(H handle) {
    return static_cast<uint64_t>(handle);
}
template <typename H, typename T>
H ToHandle(T* object) {
    return static_cast<H>(reinterpret_cast<uintptr_t>(object));
}
template <typename T, typename H>
T* FromHandle(H handle) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(handle));
}
#endif

struct DeviceInfo {
    VkPhysicalDevice physical_dev;
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceMemoryProperties memory;
};

struct Allocation {
    VkPhysicalDevice physical_dev;
    uint32_t heap;
    VkDeviceSize size;
};

struct CacheInfo {
    VkDevice dev;
    bool dirty;
    std::chrono::steady_clock::time_point saved;
};

struct UpdateTemplate {
    std::vector<VkDescriptorUpdateTemplateEntry> entries;
};

struct CacheFileHeader {
    char magic[4];
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t driver_version;
    uint8_t uuid[VK_UUID_SIZE];
    uint32_t size;
    uint32_t reserved;
};

std::mutex g_mutex;
std::map<VkDevice, DeviceInfo> g_devices;
std::map<uint64_t, Allocation> g_allocations;
std::map<VkPhysicalDevice, std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS>> g_usage;
std::map<uint64_t, CacheInfo> g_caches;

// ---- helpers

const BaseIn* NextIn(const void* p) {
    return static_cast<const BaseIn*>(p);
}

BaseOut* NextOut(void* p) {
    return static_cast<BaseOut*>(p);
}

bool HasStruct(const void* chain, VkStructureType type) {
    for (const BaseIn* s = NextIn(chain); s; s = NextIn(s->pNext)) {
        if (s->sType == type)
            return true;
    }
    return false;
}

const api::InstanceDispatchTable& Dispatch(VkPhysicalDevice physical_dev) {
    return api::GetData(physical_dev).dispatch;
}

const api::DeviceDispatchTable& Dispatch(VkDevice dev) {
    return api::GetData(dev).dispatch;
}

// the free memory the system can still give, for the memory budget
VkDeviceSize AvailableMemory() {
    FILE* f = fopen("/proc/meminfo", "re");
    if (!f)
        return 0;
    // MemAvailable is from Linux 3.14; the S6 kernel is 3.10: free + buffers + page cache
    char line[128];
    unsigned long long value = 0, available = 0, free_kb = 0, buffers = 0, cached = 0;
    bool has_available = false;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "MemAvailable: %llu kB", &value) == 1) {
            available = value;
            has_available = true;
        } else if (sscanf(line, "MemFree: %llu kB", &value) == 1) {
            free_kb = value;
        } else if (sscanf(line, "Buffers: %llu kB", &value) == 1) {
            buffers = value;
        } else if (sscanf(line, "Cached: %llu kB", &value) == 1) {
            cached = value;
        }
    }
    fclose(f);
    unsigned long long kb = has_available ? available : free_kb;
    if (!has_available) {
        kb = (kb > ULLONG_MAX - buffers) ? ULLONG_MAX : kb + buffers;
        kb = (kb > ULLONG_MAX - cached) ? ULLONG_MAX : kb + cached;
    }
    if (kb > (UINT64_MAX >> 10))
        return 0;
    return static_cast<VkDeviceSize>(kb) << 10;
}

// /data/data/<package>/cache/kiroos_vk_pipeline_cache.bin, or empty when the process is not an
// app with a cache dir it can write
const std::string& CachePath() {
    static std::string path;
    static std::once_flag once;
    std::call_once(once, [] {
        FILE* f = fopen("/proc/self/cmdline", "re");
        if (!f)
            return;
        char name[256] = {};
        size_t n = fread(name, 1, sizeof(name) - 1, f);
        fclose(f);
        if (n == 0)
            return;
        name[sizeof(name) - 1] = '\0';
        char* colon = strchr(name, ':');
        if (colon)
            *colon = '\0';
        if (!strchr(name, '.') || strchr(name, '/'))
            return;
        std::string dir = std::string("/data/data/") + name + "/cache";
        if (access(dir.c_str(), W_OK) != 0)
            return;
        path = dir + "/kiroos_vk_pipeline_cache.bin";
    });
    return path;
}

bool SameDevice(const CacheFileHeader& h, const VkPhysicalDeviceProperties& p) {
    return memcmp(h.magic, "KVK1", 4) == 0 && h.vendor_id == p.vendorID &&
           h.device_id == p.deviceID && h.driver_version == p.driverVersion &&
           memcmp(h.uuid, p.pipelineCacheUUID, VK_UUID_SIZE) == 0;
}

std::vector<uint8_t> LoadCache(const VkPhysicalDeviceProperties& props) {
    std::vector<uint8_t> data;
    const std::string& path = CachePath();
    if (path.empty())
        return data;
    FILE* f = fopen(path.c_str(), "rbe");
    if (!f)
        return data;
    CacheFileHeader h;
    if (fread(&h, sizeof(h), 1, f) == 1 && SameDevice(h, props) && h.size > 0 &&
        h.size <= kMaxCacheFile) {
        data.resize(h.size);
        if (fread(data.data(), 1, data.size(), f) != data.size())
            data.clear();
    }
    fclose(f);
    return data;
}

// takes the cache data now, writes it on its own thread (atomically, through a temp file)
void SaveCache(VkDevice dev, VkPipelineCache cache, const VkPhysicalDeviceProperties& props) {
    const std::string& path = CachePath();
    if (path.empty())
        return;
    size_t size = 0;
    const auto& d = Dispatch(dev);
    if (d.GetPipelineCacheData(dev, cache, &size, nullptr) != VK_SUCCESS || size == 0 ||
        size > kMaxCacheFile)
        return;
    std::vector<uint8_t> data(size);
    if (d.GetPipelineCacheData(dev, cache, &size, data.data()) != VK_SUCCESS)
        return;
    data.resize(size);
    CacheFileHeader h = {};
    memcpy(h.magic, "KVK1", 4);
    h.vendor_id = props.vendorID;
    h.device_id = props.deviceID;
    h.driver_version = props.driverVersion;
    memcpy(h.uuid, props.pipelineCacheUUID, VK_UUID_SIZE);
    h.size = static_cast<uint32_t>(size);
    std::string target = path;
    std::thread([h, target, bytes = std::move(data)] {
        std::string tmp = target + ".tmp";
        FILE* f = fopen(tmp.c_str(), "wbe");
        if (!f)
            return;
        bool ok = fwrite(&h, sizeof(h), 1, f) == 1 &&
                  fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        ok = (fclose(f) == 0) && ok;
        if (ok)
            rename(tmp.c_str(), target.c_str());
        else
            unlink(tmp.c_str());
    }).detach();
}

// ---- global

VKAPI_ATTR VkResult EnumerateInstanceVersion(uint32_t* pApiVersion) {
    *pApiVersion = kApiVersion;
    return VK_SUCCESS;
}

// ---- physical device

VKAPI_ATTR void GetPhysicalDeviceFeatures2(VkPhysicalDevice physical_dev,
                                           VkPhysicalDeviceFeatures2* features) {
    Dispatch(physical_dev).GetPhysicalDeviceFeatures(physical_dev, &features->features);
    for (BaseOut* s = NextOut(features->pNext); s; s = NextOut(s->pNext)) {
        if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES) {
            auto* f = reinterpret_cast<VkPhysicalDevice16BitStorageFeatures*>(s);
            f->storageBuffer16BitAccess = VK_FALSE;
            f->uniformAndStorageBuffer16BitAccess = VK_FALSE;
            f->storagePushConstant16 = VK_FALSE;
            f->storageInputOutput16 = VK_FALSE;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES) {
            auto* f = reinterpret_cast<VkPhysicalDeviceMultiviewFeatures*>(s);
            f->multiview = VK_FALSE;
            f->multiviewGeometryShader = VK_FALSE;
            f->multiviewTessellationShader = VK_FALSE;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES) {
            auto* f = reinterpret_cast<VkPhysicalDeviceVariablePointersFeatures*>(s);
            f->variablePointersStorageBuffer = VK_FALSE;
            f->variablePointers = VK_FALSE;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES) {
            reinterpret_cast<VkPhysicalDeviceProtectedMemoryFeatures*>(s)->protectedMemory =
                VK_FALSE;
        } else if (s->sType ==
                   VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES) {
            reinterpret_cast<VkPhysicalDeviceSamplerYcbcrConversionFeatures*>(s)
                ->samplerYcbcrConversion = VK_FALSE;
        } else if (s->sType ==
                   VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES) {
            reinterpret_cast<VkPhysicalDeviceShaderDrawParametersFeatures*>(s)
                ->shaderDrawParameters = VK_FALSE;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT) {
            reinterpret_cast<VkPhysicalDeviceMemoryPriorityFeaturesEXT*>(s)->memoryPriority =
                VK_TRUE;
        }
    }
}

VKAPI_ATTR void GetPhysicalDeviceProperties2(VkPhysicalDevice physical_dev,
                                             VkPhysicalDeviceProperties2* properties) {
    VkPhysicalDeviceProperties& p = properties->properties;
    Dispatch(physical_dev).GetPhysicalDeviceProperties(physical_dev, &p);
    PatchProperties(&p);
    VkPhysicalDeviceMemoryProperties mem;
    Dispatch(physical_dev).GetPhysicalDeviceMemoryProperties(physical_dev, &mem);
    VkDeviceSize largest_heap = kMinMaxAllocation;
    for (uint32_t i = 0; i < mem.memoryHeapCount && i < VK_MAX_MEMORY_HEAPS; i++)
        largest_heap = std::max(largest_heap, mem.memoryHeaps[i].size);

    for (BaseOut* s = NextOut(properties->pNext); s; s = NextOut(s->pNext)) {
        if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES) {
            auto* id = reinterpret_cast<VkPhysicalDeviceIDProperties*>(s);
            memcpy(id->deviceUUID, p.pipelineCacheUUID, VK_UUID_SIZE);
            memset(id->driverUUID, 0, VK_UUID_SIZE);
            memcpy(id->driverUUID, "KiroOS", 6);
            memcpy(id->driverUUID + 8, &p.driverVersion, sizeof(p.driverVersion));
            memcpy(id->driverUUID + 12, &p.vendorID, sizeof(p.vendorID));
            memset(id->deviceLUID, 0, VK_LUID_SIZE);
            id->deviceNodeMask = 0;
            id->deviceLUIDValid = VK_FALSE;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES) {
            // Midgard has no subgroup operations: the minimum Vulkan 1.1 asks for
            auto* sg = reinterpret_cast<VkPhysicalDeviceSubgroupProperties*>(s);
            sg->subgroupSize = 1;
            sg->supportedStages = VK_SHADER_STAGE_COMPUTE_BIT;
            sg->supportedOperations = VK_SUBGROUP_FEATURE_BASIC_BIT;
            sg->quadOperationsInAllStages = VK_FALSE;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES) {
            reinterpret_cast<VkPhysicalDevicePointClippingProperties*>(s)->pointClippingBehavior =
                VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES) {
            auto* mv = reinterpret_cast<VkPhysicalDeviceMultiviewProperties*>(s);
            mv->maxMultiviewViewCount = 0;
            mv->maxMultiviewInstanceIndex = 0;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES) {
            reinterpret_cast<VkPhysicalDeviceProtectedMemoryProperties*>(s)->protectedNoFault =
                VK_FALSE;
        } else if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES) {
            auto* m3 = reinterpret_cast<VkPhysicalDeviceMaintenance3Properties*>(s);
            m3->maxPerSetDescriptors = kMaxPerSetDescriptors;
            m3->maxMemoryAllocationSize = largest_heap;
        }
    }
}

VKAPI_ATTR void GetPhysicalDeviceFormatProperties2(VkPhysicalDevice physical_dev,
                                                   VkFormat format,
                                                   VkFormatProperties2* properties) {
    Dispatch(physical_dev)
        .GetPhysicalDeviceFormatProperties(physical_dev, format, &properties->formatProperties);
}

VKAPI_ATTR VkResult
GetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physical_dev,
                                        const VkPhysicalDeviceImageFormatInfo2* info,
                                        VkImageFormatProperties2* properties) {
    // no external memory: an image for an external handle type is not supported
    for (const BaseIn* s = NextIn(info->pNext); s; s = NextIn(s->pNext)) {
        if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO) {
            const uint32_t* handle_type = reinterpret_cast<const uint32_t*>(s + 1);
            if (*handle_type != 0)
                return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
    }
    VkResult result = Dispatch(physical_dev)
                          .GetPhysicalDeviceImageFormatProperties(
                              physical_dev, info->format, info->type, info->tiling,
                              info->usage, info->flags, &properties->imageFormatProperties);
    for (BaseOut* s = NextOut(properties->pNext); s; s = NextOut(s->pNext)) {
        if (s->sType == VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES) {
            auto* e = reinterpret_cast<VkExternalImageFormatProperties*>(s);
            memset(&e->externalMemoryProperties, 0, sizeof(e->externalMemoryProperties));
        } else if (s->sType ==
                   VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES) {
            reinterpret_cast<VkSamplerYcbcrConversionImageFormatProperties*>(s)
                ->combinedImageSamplerDescriptorCount = 1;
        }
    }
    return result;
}

VKAPI_ATTR void GetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physical_dev,
                                                        uint32_t* count,
                                                        VkQueueFamilyProperties2* properties) {
    const auto& d = Dispatch(physical_dev);
    if (!properties) {
        d.GetPhysicalDeviceQueueFamilyProperties(physical_dev, count, nullptr);
        return;
    }
    std::vector<VkQueueFamilyProperties> plain(*count);
    d.GetPhysicalDeviceQueueFamilyProperties(physical_dev, count, plain.data());
    for (uint32_t i = 0; i < *count; i++)
        properties[i].queueFamilyProperties = plain[i];
}

VKAPI_ATTR void GetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physical_dev,
                                                   VkPhysicalDeviceMemoryProperties2* properties) {
    VkPhysicalDeviceMemoryProperties& mem = properties->memoryProperties;
    Dispatch(physical_dev).GetPhysicalDeviceMemoryProperties(physical_dev, &mem);
    for (BaseOut* s = NextOut(properties->pNext); s; s = NextOut(s->pNext)) {
        if (s->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT)
            continue;
        auto* b = reinterpret_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(s);
        memset(b->heapBudget, 0, sizeof(b->heapBudget));
        memset(b->heapUsage, 0, sizeof(b->heapUsage));
        std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS> usage = {};
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_usage.find(physical_dev);
            if (it != g_usage.end())
                usage = it->second;
        }
        // the GPU shares the phone's RAM: what it may still take is what the system has free,
        // with a quarter kept for everything else
        const VkDeviceSize available = AvailableMemory() / 4 * 3;
        for (uint32_t i = 0; i < mem.memoryHeapCount && i < VK_MAX_MEMORY_HEAPS; i++) {
            const VkDeviceSize heap = mem.memoryHeaps[i].size;
            const VkDeviceSize used = std::min(usage[i], heap);
            b->heapUsage[i] = used;
            b->heapBudget[i] = (available >= heap - used) ? heap : used + available;
        }
    }
}

VKAPI_ATTR void GetPhysicalDeviceSparseImageFormatProperties2(
    VkPhysicalDevice physical_dev,
    const VkPhysicalDeviceSparseImageFormatInfo2* info,
    uint32_t* count,
    VkSparseImageFormatProperties2* properties) {
    const auto& d = Dispatch(physical_dev);
    if (!properties) {
        d.GetPhysicalDeviceSparseImageFormatProperties(physical_dev, info->format, info->type,
                                                       info->samples, info->usage,
                                                       info->tiling, count, nullptr);
        return;
    }
    std::vector<VkSparseImageFormatProperties> plain(*count);
    d.GetPhysicalDeviceSparseImageFormatProperties(physical_dev, info->format, info->type,
                                                   info->samples, info->usage, info->tiling,
                                                   count, plain.data());
    for (uint32_t i = 0; i < *count; i++)
        properties[i].properties = plain[i];
}

// no external buffers, fences or semaphores
VKAPI_ATTR void GetPhysicalDeviceExternalBufferProperties(
    VkPhysicalDevice,
    const VkPhysicalDeviceExternalBufferInfo*,
    VkExternalBufferProperties* properties) {
    memset(&properties->externalMemoryProperties, 0,
           sizeof(properties->externalMemoryProperties));
}

VKAPI_ATTR void GetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice,
                                                         const VkPhysicalDeviceExternalFenceInfo*,
                                                         VkExternalFenceProperties* properties) {
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalFenceFeatures = 0;
}

VKAPI_ATTR void GetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice,
    const VkPhysicalDeviceExternalSemaphoreInfo*,
    VkExternalSemaphoreProperties* properties) {
    properties->exportFromImportedHandleTypes = 0;
    properties->compatibleHandleTypes = 0;
    properties->externalSemaphoreFeatures = 0;
}

// every physical device is a group of one
VKAPI_ATTR VkResult EnumeratePhysicalDeviceGroups(VkInstance instance,
                                                  uint32_t* count,
                                                  VkPhysicalDeviceGroupProperties* groups) {
    const auto& d = api::GetData(instance).dispatch;
    uint32_t n = 0;
    VkResult result = d.EnumeratePhysicalDevices(instance, &n, nullptr);
    if (result != VK_SUCCESS)
        return result;
    if (!groups) {
        *count = n;
        return VK_SUCCESS;
    }
    std::vector<VkPhysicalDevice> devs(n);
    result = d.EnumeratePhysicalDevices(instance, &n, devs.data());
    if (result != VK_SUCCESS && result != VK_INCOMPLETE)
        return result;
    const uint32_t written = std::min(*count, n);
    for (uint32_t i = 0; i < written; i++) {
        groups[i].physicalDeviceCount = 1;
        memset(groups[i].physicalDevices, 0, sizeof(groups[i].physicalDevices));
        groups[i].physicalDevices[0] = devs[i];
        groups[i].subsetAllocation = VK_FALSE;
    }
    *count = written;
    return (written < n) ? VK_INCOMPLETE : VK_SUCCESS;
}

// ---- device

VKAPI_ATTR VkResult BindBufferMemory2(VkDevice dev,
                                      uint32_t count,
                                      const VkBindBufferMemoryInfo* infos) {
    const auto& d = Dispatch(dev);
    for (uint32_t i = 0; i < count; i++) {
        VkResult r = d.BindBufferMemory(dev, infos[i].buffer, infos[i].memory,
                                        infos[i].memoryOffset);
        if (r != VK_SUCCESS)
            return r;
    }
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult BindImageMemory2(VkDevice dev,
                                     uint32_t count,
                                     const VkBindImageMemoryInfo* infos) {
    const auto& d = Dispatch(dev);
    for (uint32_t i = 0; i < count; i++) {
        VkResult r =
            d.BindImageMemory(dev, infos[i].image, infos[i].memory, infos[i].memoryOffset);
        if (r != VK_SUCCESS)
            return r;
    }
    return VK_SUCCESS;
}

void FillDedicated(void* chain) {
    for (BaseOut* s = NextOut(chain); s; s = NextOut(s->pNext)) {
        if (s->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS) {
            auto* r = reinterpret_cast<VkMemoryDedicatedRequirements*>(s);
            r->prefersDedicatedAllocation = VK_FALSE;
            r->requiresDedicatedAllocation = VK_FALSE;
        }
    }
}

VKAPI_ATTR void GetBufferMemoryRequirements2(VkDevice dev,
                                             const VkBufferMemoryRequirementsInfo2* info,
                                             VkMemoryRequirements2* requirements) {
    Dispatch(dev).GetBufferMemoryRequirements(dev, info->buffer,
                                              &requirements->memoryRequirements);
    FillDedicated(requirements->pNext);
}

VKAPI_ATTR void GetImageMemoryRequirements2(VkDevice dev,
                                            const VkImageMemoryRequirementsInfo2* info,
                                            VkMemoryRequirements2* requirements) {
    Dispatch(dev).GetImageMemoryRequirements(dev, info->image, &requirements->memoryRequirements);
    FillDedicated(requirements->pNext);
}

VKAPI_ATTR void GetImageSparseMemoryRequirements2(
    VkDevice dev,
    const VkImageSparseMemoryRequirementsInfo2* info,
    uint32_t* count,
    VkSparseImageMemoryRequirements2* requirements) {
    const auto& d = Dispatch(dev);
    if (!requirements) {
        d.GetImageSparseMemoryRequirements(dev, info->image, count, nullptr);
        return;
    }
    std::vector<VkSparseImageMemoryRequirements> plain(*count);
    d.GetImageSparseMemoryRequirements(dev, info->image, count, plain.data());
    for (uint32_t i = 0; i < *count; i++)
        requirements[i].memoryRequirements = plain[i];
}

// maintenance1: a hint, nothing to give back on this driver
VKAPI_ATTR void TrimCommandPool(VkDevice, VkCommandPool, VkFlags) {}

VKAPI_ATTR void GetDeviceQueue2(VkDevice dev, const VkDeviceQueueInfo2* info, VkQueue* queue) {
    // there are no protected queues
    if (info->flags != 0) {
        *queue = VK_NULL_HANDLE;
        return;
    }
    // through the exported entry point: the loader sets the queue's dispatch there
    api::GetData(dev).dispatch.GetDeviceQueue(dev, info->queueFamilyIndex, info->queueIndex,
                                              queue);
}

VKAPI_ATTR VkResult CreateDescriptorUpdateTemplate(VkDevice,
                                                   const VkDescriptorUpdateTemplateCreateInfo* info,
                                                   const VkAllocationCallbacks*,
                                                   VkDescriptorUpdateTemplate* out) {
    if (info->templateType != VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET)
        return VK_ERROR_FEATURE_NOT_PRESENT_KIRO;
    auto* t = new (std::nothrow) UpdateTemplate();
    if (!t)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    t->entries.assign(info->pDescriptorUpdateEntries,
                      info->pDescriptorUpdateEntries + info->descriptorUpdateEntryCount);
    *out = ToHandle<VkDescriptorUpdateTemplate>(t);
    return VK_SUCCESS;
}

VKAPI_ATTR void DestroyDescriptorUpdateTemplate(VkDevice,
                                                VkDescriptorUpdateTemplate handle,
                                                const VkAllocationCallbacks*) {
    delete FromHandle<UpdateTemplate>(handle);
}

VKAPI_ATTR void UpdateDescriptorSetWithTemplate(VkDevice dev,
                                                VkDescriptorSet set,
                                                VkDescriptorUpdateTemplate handle,
                                                const void* data) {
    const UpdateTemplate* t = FromHandle<UpdateTemplate>(handle);
    if (!t || !data)
        return;
    const uint8_t* base = static_cast<const uint8_t*>(data);
    size_t images = 0, buffers = 0, views = 0;
    for (const auto& e : t->entries) {
        switch (e.descriptorType) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                buffers += e.descriptorCount;
                break;
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                views += e.descriptorCount;
                break;
            default:
                images += e.descriptorCount;
                break;
        }
    }
    // sized up front: the writes point into these arrays
    std::vector<VkDescriptorImageInfo> image_infos;
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    std::vector<VkBufferView> texel_views;
    image_infos.reserve(images);
    buffer_infos.reserve(buffers);
    texel_views.reserve(views);
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(t->entries.size());
    for (const auto& e : t->entries) {
        if (e.descriptorCount == 0)
            continue;
        VkWriteDescriptorSet w = {};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = e.dstBinding;
        w.dstArrayElement = e.dstArrayElement;
        w.descriptorCount = e.descriptorCount;
        w.descriptorType = e.descriptorType;
        for (uint32_t i = 0; i < e.descriptorCount; i++) {
            const uint8_t* src = base + e.offset + static_cast<size_t>(i) * e.stride;
            switch (e.descriptorType) {
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
                    VkDescriptorBufferInfo b;
                    memcpy(&b, src, sizeof(b));
                    if (i == 0)
                        w.pBufferInfo = buffer_infos.data() + buffer_infos.size();
                    buffer_infos.push_back(b);
                    break;
                }
                case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
                    VkBufferView v;
                    memcpy(&v, src, sizeof(v));
                    if (i == 0)
                        w.pTexelBufferView = texel_views.data() + texel_views.size();
                    texel_views.push_back(v);
                    break;
                }
                default: {
                    VkDescriptorImageInfo img;
                    memcpy(&img, src, sizeof(img));
                    if (i == 0)
                        w.pImageInfo = image_infos.data() + image_infos.size();
                    image_infos.push_back(img);
                    break;
                }
            }
        }
        writes.push_back(w);
    }
    if (!writes.empty())
        Dispatch(dev).UpdateDescriptorSets(dev, static_cast<uint32_t>(writes.size()),
                                           writes.data(), 0, nullptr);
}

VKAPI_ATTR void GetDescriptorSetLayoutSupport(VkDevice,
                                              const VkDescriptorSetLayoutCreateInfo* info,
                                              VkDescriptorSetLayoutSupport* support) {
    uint64_t total = 0;
    for (uint32_t i = 0; i < info->bindingCount; i++)
        total += info->pBindings[i].descriptorCount;
    support->supported = (total <= kMaxPerSetDescriptors) ? VK_TRUE : VK_FALSE;
}

// a device group of one device
VKAPI_ATTR void GetDeviceGroupPeerMemoryFeatures(VkDevice, uint32_t, uint32_t, uint32_t,
                                                 VkFlags* features) {
    *features = 0xF;  // copy src / dst, generic src / dst
}

VKAPI_ATTR void CmdSetDeviceMask(VkCommandBuffer, uint32_t) {}

VKAPI_ATTR void CmdDispatchBase(VkCommandBuffer cmd,
                                uint32_t base_x,
                                uint32_t base_y,
                                uint32_t base_z,
                                uint32_t x,
                                uint32_t y,
                                uint32_t z) {
    if (base_x != 0 || base_y != 0 || base_z != 0) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            ALOGW("vkCmdDispatchBase with a base: this GPU dispatches from 0");
        }
    }
    api::GetData(cmd).dispatch.CmdDispatch(cmd, x, y, z);
}

// samplerYcbcrConversion is not a feature of this GPU
VKAPI_ATTR VkResult CreateSamplerYcbcrConversion(VkDevice,
                                                 const void*,
                                                 const VkAllocationCallbacks*,
                                                 VkSamplerYcbcrConversion*) {
    return VK_ERROR_FEATURE_NOT_PRESENT_KIRO;
}

VKAPI_ATTR void DestroySamplerYcbcrConversion(VkDevice,
                                              VkSamplerYcbcrConversion,
                                              const VkAllocationCallbacks*) {}

struct Proc {
    const char* name;
    PFN_vkVoidFunction proc;
    bool device;
};

#define KIRO_PROC(name, fn, dev) \
    { name, reinterpret_cast<PFN_vkVoidFunction>(fn), dev }

// the Vulkan 1.1 commands and their KHR names; sorted by name
const Proc kProcs[] = {
    KIRO_PROC("vkBindBufferMemory2", BindBufferMemory2, true),
    KIRO_PROC("vkBindBufferMemory2KHR", BindBufferMemory2, true),
    KIRO_PROC("vkBindImageMemory2", BindImageMemory2, true),
    KIRO_PROC("vkBindImageMemory2KHR", BindImageMemory2, true),
    KIRO_PROC("vkCmdDispatchBase", CmdDispatchBase, true),
    KIRO_PROC("vkCmdDispatchBaseKHR", CmdDispatchBase, true),
    KIRO_PROC("vkCmdSetDeviceMask", CmdSetDeviceMask, true),
    KIRO_PROC("vkCmdSetDeviceMaskKHR", CmdSetDeviceMask, true),
    KIRO_PROC("vkCreateDescriptorUpdateTemplate", CreateDescriptorUpdateTemplate, true),
    KIRO_PROC("vkCreateDescriptorUpdateTemplateKHR", CreateDescriptorUpdateTemplate, true),
    KIRO_PROC("vkCreateSamplerYcbcrConversion", CreateSamplerYcbcrConversion, true),
    KIRO_PROC("vkDestroyDescriptorUpdateTemplate", DestroyDescriptorUpdateTemplate, true),
    KIRO_PROC("vkDestroyDescriptorUpdateTemplateKHR", DestroyDescriptorUpdateTemplate, true),
    KIRO_PROC("vkDestroySamplerYcbcrConversion", DestroySamplerYcbcrConversion, true),
    KIRO_PROC("vkEnumeratePhysicalDeviceGroups", EnumeratePhysicalDeviceGroups, false),
    KIRO_PROC("vkEnumeratePhysicalDeviceGroupsKHR", EnumeratePhysicalDeviceGroups, false),
    KIRO_PROC("vkGetBufferMemoryRequirements2", GetBufferMemoryRequirements2, true),
    KIRO_PROC("vkGetBufferMemoryRequirements2KHR", GetBufferMemoryRequirements2, true),
    KIRO_PROC("vkGetDescriptorSetLayoutSupport", GetDescriptorSetLayoutSupport, true),
    KIRO_PROC("vkGetDescriptorSetLayoutSupportKHR", GetDescriptorSetLayoutSupport, true),
    KIRO_PROC("vkGetDeviceGroupPeerMemoryFeatures", GetDeviceGroupPeerMemoryFeatures, true),
    KIRO_PROC("vkGetDeviceGroupPeerMemoryFeaturesKHR", GetDeviceGroupPeerMemoryFeatures, true),
    KIRO_PROC("vkGetDeviceQueue2", GetDeviceQueue2, true),
    KIRO_PROC("vkGetImageMemoryRequirements2", GetImageMemoryRequirements2, true),
    KIRO_PROC("vkGetImageMemoryRequirements2KHR", GetImageMemoryRequirements2, true),
    KIRO_PROC("vkGetImageSparseMemoryRequirements2", GetImageSparseMemoryRequirements2, true),
    KIRO_PROC("vkGetImageSparseMemoryRequirements2KHR", GetImageSparseMemoryRequirements2,
              true),
    KIRO_PROC("vkGetPhysicalDeviceExternalBufferProperties",
              GetPhysicalDeviceExternalBufferProperties, false),
    KIRO_PROC("vkGetPhysicalDeviceExternalBufferPropertiesKHR",
              GetPhysicalDeviceExternalBufferProperties, false),
    KIRO_PROC("vkGetPhysicalDeviceExternalFenceProperties",
              GetPhysicalDeviceExternalFenceProperties, false),
    KIRO_PROC("vkGetPhysicalDeviceExternalFencePropertiesKHR",
              GetPhysicalDeviceExternalFenceProperties, false),
    KIRO_PROC("vkGetPhysicalDeviceExternalSemaphoreProperties",
              GetPhysicalDeviceExternalSemaphoreProperties, false),
    KIRO_PROC("vkGetPhysicalDeviceExternalSemaphorePropertiesKHR",
              GetPhysicalDeviceExternalSemaphoreProperties, false),
    KIRO_PROC("vkGetPhysicalDeviceFeatures2", GetPhysicalDeviceFeatures2, false),
    KIRO_PROC("vkGetPhysicalDeviceFeatures2KHR", GetPhysicalDeviceFeatures2, false),
    KIRO_PROC("vkGetPhysicalDeviceFormatProperties2", GetPhysicalDeviceFormatProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceFormatProperties2KHR", GetPhysicalDeviceFormatProperties2,
              false),
    KIRO_PROC("vkGetPhysicalDeviceImageFormatProperties2",
              GetPhysicalDeviceImageFormatProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceImageFormatProperties2KHR",
              GetPhysicalDeviceImageFormatProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceMemoryProperties2", GetPhysicalDeviceMemoryProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceMemoryProperties2KHR", GetPhysicalDeviceMemoryProperties2,
              false),
    KIRO_PROC("vkGetPhysicalDeviceProperties2", GetPhysicalDeviceProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceProperties2KHR", GetPhysicalDeviceProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceQueueFamilyProperties2",
              GetPhysicalDeviceQueueFamilyProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceQueueFamilyProperties2KHR",
              GetPhysicalDeviceQueueFamilyProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceSparseImageFormatProperties2",
              GetPhysicalDeviceSparseImageFormatProperties2, false),
    KIRO_PROC("vkGetPhysicalDeviceSparseImageFormatProperties2KHR",
              GetPhysicalDeviceSparseImageFormatProperties2, false),
    KIRO_PROC("vkTrimCommandPool", TrimCommandPool, true),
    KIRO_PROC("vkTrimCommandPoolKHR", TrimCommandPool, true),
    KIRO_PROC("vkUpdateDescriptorSetWithTemplate", UpdateDescriptorSetWithTemplate, true),
    KIRO_PROC("vkUpdateDescriptorSetWithTemplateKHR", UpdateDescriptorSetWithTemplate, true),
};

#undef KIRO_PROC

const Proc* FindProc(const char* name) {
    if (!name)
        return nullptr;
    const Proc* end = kProcs + sizeof(kProcs) / sizeof(kProcs[0]);
    const Proc* p = std::lower_bound(
        kProcs, end, name, [](const Proc& a, const char* n) { return strcmp(a.name, n) < 0; });
    return (p != end && strcmp(p->name, name) == 0) ? p : nullptr;
}

// the 1.1 feature structs a device may be created with, and the ones the GPU cannot turn on
VkResult CheckFeatureStruct(const BaseIn* s) {
    auto any_true = [](const VkBool32* flags, size_t n) {
        for (size_t i = 0; i < n; i++) {
            if (flags[i])
                return true;
        }
        return false;
    };
    const VkBool32* flags = reinterpret_cast<const VkBool32*>(s + 1);
    switch (static_cast<uint32_t>(s->sType)) {
        case 1000083000:  // 16-bit storage
            return any_true(flags, 4) ? VK_ERROR_FEATURE_NOT_PRESENT_KIRO : VK_SUCCESS;
        case 1000053001:  // multiview
            return any_true(flags, 3) ? VK_ERROR_FEATURE_NOT_PRESENT_KIRO : VK_SUCCESS;
        case 1000120000:  // variable pointers
            return any_true(flags, 2) ? VK_ERROR_FEATURE_NOT_PRESENT_KIRO : VK_SUCCESS;
        case 1000145001:  // protected memory
        case 1000156004:  // sampler YCbCr conversion
        case 1000063000:  // shader draw parameters
            return any_true(flags, 1) ? VK_ERROR_FEATURE_NOT_PRESENT_KIRO : VK_SUCCESS;
        default:
            return VK_SUCCESS;
    }
}

}  // namespace

// ---- public

bool Enabled() {
    static const bool enabled = property_get_bool("persist.sys.kiroos.vk11", true);
    return enabled;
}

PFN_vkVoidFunction GetGlobalProcAddr(const char* name) {
    if (Enabled() && name && strcmp(name, "vkEnumerateInstanceVersion") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(EnumerateInstanceVersion);
    return nullptr;
}

PFN_vkVoidFunction GetInstanceProcAddr(const char* name) {
    if (!Enabled())
        return nullptr;
    if (name && strcmp(name, "vkEnumerateInstanceVersion") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(EnumerateInstanceVersion);
    const Proc* p = FindProc(name);
    return p ? p->proc : nullptr;
}

PFN_vkVoidFunction GetDeviceProcAddr(const char* name) {
    if (!Enabled())
        return nullptr;
    const Proc* p = FindProc(name);
    return (p && p->device) ? p->proc : nullptr;
}

void PatchProperties(VkPhysicalDeviceProperties* properties) {
    if (!Enabled() || !properties)
        return;
    const uint32_t v = properties->apiVersion;
    if ((v >> 22) == 1 && ((v >> 12) & 0x3ffu) == 0)
        properties->apiVersion = kApiVersion;
}

void OnDeviceCreated(VkPhysicalDevice physical_dev, VkDevice dev) {
    if (!Enabled())
        return;
    DeviceInfo info = {};
    info.physical_dev = physical_dev;
    const auto& d = Dispatch(physical_dev);
    d.GetPhysicalDeviceProperties(physical_dev, &info.properties);
    d.GetPhysicalDeviceMemoryProperties(physical_dev, &info.memory);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_devices[dev] = info;
}

void OnDeviceDestroyed(VkDevice dev) {
    if (!Enabled())
        return;
    std::vector<VkPipelineCache> dirty;
    VkPhysicalDeviceProperties props = {};
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto dit = g_devices.find(dev);
        if (dit == g_devices.end())
            return;
        props = dit->second.properties;
        for (auto it = g_caches.begin(); it != g_caches.end();) {
            if (it->second.dev == dev) {
                if (it->second.dirty) {
#if defined(__LP64__)
                    dirty.push_back(reinterpret_cast<VkPipelineCache>(
                        static_cast<uintptr_t>(it->first)));
#else
                    dirty.push_back(static_cast<VkPipelineCache>(it->first));
#endif
                }
                it = g_caches.erase(it);
            } else {
                ++it;
            }
        }
        // memory is freed before its device (valid usage): nothing of this device is left
        g_devices.erase(dit);
    }
    for (VkPipelineCache c : dirty)
        SaveCache(dev, c, props);
}

const VkMemoryAllocateInfo* SanitizeAllocateInfo(const VkMemoryAllocateInfo* info,
                                                 VkMemoryAllocateInfo* storage) {
    if (!Enabled() || !info || !info->pNext)
        return info;
    // the 1.1 / EXT structs the 1.0 driver does not know: priority, dedicated allocation, device
    // mask. The driver has no allocate-info structs of its own, so the chain is dropped whole.
    if (HasStruct(info->pNext, VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT) ||
        HasStruct(info->pNext, VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO) ||
        HasStruct(info->pNext, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO)) {
        *storage = *info;
        storage->pNext = nullptr;
        return storage;
    }
    return info;
}

void OnAllocateMemory(VkDevice dev, const VkMemoryAllocateInfo* info, VkDeviceMemory memory) {
    if (!Enabled() || !info)
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto dit = g_devices.find(dev);
    if (dit == g_devices.end())
        return;
    const VkPhysicalDeviceMemoryProperties& mem = dit->second.memory;
    if (info->memoryTypeIndex >= mem.memoryTypeCount)
        return;
    const uint32_t heap = mem.memoryTypes[info->memoryTypeIndex].heapIndex;
    if (heap >= VK_MAX_MEMORY_HEAPS)
        return;
    Allocation a = {dit->second.physical_dev, heap, info->allocationSize};
    g_allocations[Key(memory)] = a;
    auto& usage = g_usage[a.physical_dev];
    usage[heap] = (usage[heap] > UINT64_MAX - a.size) ? UINT64_MAX : usage[heap] + a.size;
}

void OnFreeMemory(VkDevice, VkDeviceMemory memory) {
    if (!Enabled() || memory == VK_NULL_HANDLE)
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_allocations.find(Key(memory));
    if (it == g_allocations.end())
        return;
    auto& usage = g_usage[it->second.physical_dev];
    VkDeviceSize& u = usage[it->second.heap];
    u = (u >= it->second.size) ? u - it->second.size : 0;
    g_allocations.erase(it);
}

const VkPipelineCacheCreateInfo* SeedPipelineCache(VkDevice dev,
                                                   const VkPipelineCacheCreateInfo* info,
                                                   VkPipelineCacheCreateInfo* storage,
                                                   bool* seeded) {
    *seeded = false;
    if (!Enabled() || !info || info->initialDataSize != 0 || CachePath().empty())
        return info;
    VkPhysicalDeviceProperties props;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto dit = g_devices.find(dev);
        if (dit == g_devices.end())
            return info;
        props = dit->second.properties;
    }
    // the app keeps no cache of its own: the loader keeps this one
    *seeded = true;
    static thread_local std::vector<uint8_t> data;
    data = LoadCache(props);
    if (data.empty())
        return info;
    *storage = *info;
    storage->initialDataSize = data.size();
    storage->pInitialData = data.data();
    return storage;
}

void OnPipelineCacheCreated(VkDevice dev, VkPipelineCache cache, bool seeded) {
    if (!Enabled() || !seeded)
        return;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_caches[Key(cache)] = {dev, false, std::chrono::steady_clock::now()};
}

void OnPipelinesCreated(VkDevice dev, VkPipelineCache cache) {
    if (!Enabled() || cache == VK_NULL_HANDLE)
        return;
    VkPhysicalDeviceProperties props;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_caches.find(Key(cache));
        if (it == g_caches.end())
            return;
        it->second.dirty = true;
        const auto now = std::chrono::steady_clock::now();
        if (now - it->second.saved < kCacheSaveInterval)
            return;
        auto dit = g_devices.find(dev);
        if (dit == g_devices.end())
            return;
        props = dit->second.properties;
        it->second.saved = now;
        it->second.dirty = false;
    }
    SaveCache(dev, cache, props);
}

void OnPipelineCacheDestroy(VkDevice dev, VkPipelineCache cache) {
    if (!Enabled() || cache == VK_NULL_HANDLE)
        return;
    bool dirty = false;
    VkPhysicalDeviceProperties props = {};
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_caches.find(Key(cache));
        if (it == g_caches.end())
            return;
        dirty = it->second.dirty;
        g_caches.erase(it);
        auto dit = g_devices.find(dev);
        if (dit == g_devices.end())
            return;
        props = dit->second.properties;
    }
    if (dirty)
        SaveCache(dev, cache, props);
}

void SanitizeInstanceCreateInfo(VkInstanceCreateInfo* info, VkApplicationInfo* app_storage) {
    if (!Enabled() || !info->pApplicationInfo)
        return;
    // a 1.0 driver must refuse an application that asks for 1.1: it gets 1.0 below the loader
    const uint32_t v = info->pApplicationInfo->apiVersion;
    if ((v >> 22) == 1 && ((v >> 12) & 0x3ffu) >= 1) {
        *app_storage = *info->pApplicationInfo;
        app_storage->apiVersion = (1u << 22);
        info->pApplicationInfo = app_storage;
    }
}

VkResult SanitizeDeviceCreateInfo(VkDeviceCreateInfo* info,
                                  VkPhysicalDeviceFeatures* features_storage) {
    if (!Enabled())
        return VK_SUCCESS;
    for (uint32_t i = 0; i < info->queueCreateInfoCount; i++) {
        if (info->pQueueCreateInfos[i].flags != 0)
            return VK_ERROR_FEATURE_NOT_PRESENT_KIRO;  // protected queues
    }
    bool has_kiro_structs = false;
    for (const BaseIn* s = NextIn(info->pNext); s; s = NextIn(s->pNext)) {
        const uint32_t type = static_cast<uint32_t>(s->sType);
        if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2) {
            has_kiro_structs = true;
            if (!info->pEnabledFeatures) {
                *features_storage =
                    reinterpret_cast<const VkPhysicalDeviceFeatures2*>(s)->features;
                info->pEnabledFeatures = features_storage;
            }
        } else if (type >= 1000000000u) {
            VkResult r = CheckFeatureStruct(s);
            if (r != VK_SUCCESS)
                return r;
            has_kiro_structs = true;
        }
    }
    // the 1.0 driver knows no device create struct (the loader's own were taken off before):
    // without the 1.1 / EXT ones the chain is empty
    if (has_kiro_structs)
        info->pNext = nullptr;
    return VK_SUCCESS;
}

uint32_t ExtraDeviceExtensionCount() {
    return Enabled() ? static_cast<uint32_t>(sizeof(kDeviceExtensions) /
                                             sizeof(kDeviceExtensions[0]))
                     : 0;
}

const VkExtensionProperties* ExtraDeviceExtensions() {
    return kDeviceExtensions;
}

uint32_t ExtraInstanceExtensionCount() {
    return Enabled() ? static_cast<uint32_t>(sizeof(kInstanceExtensions) /
                                             sizeof(kInstanceExtensions[0]))
                     : 0;
}

const VkExtensionProperties* ExtraInstanceExtensions() {
    return kInstanceExtensions;
}

}  // namespace kiro
}  // namespace vulkan
