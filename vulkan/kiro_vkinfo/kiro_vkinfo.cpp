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

// KiroOS test tool (not in the ROM): checks the loader's Vulkan 1.1 layer on the phone.
//   adb push kiro_vkinfo /data/local/tmp && adb shell /data/local/tmp/kiro_vkinfo

#include <stdio.h>
#include <string.h>
#include <vector>

#include <vulkan/vulkan.h>
#include "../libvulkan/kiro_vk11_types.h"

#define CHECK(x)                                                           \
    do {                                                                   \
        VkResult r_ = (x);                                                 \
        if (r_ != VK_SUCCESS) {                                            \
            printf("FAIL %s = %d\n", #x, r_);                              \
            return 1;                                                      \
        }                                                                  \
    } while (0)

typedef VkResult (*PFN_EnumerateInstanceVersion)(uint32_t*);
typedef void (*PFN_GetProperties2)(VkPhysicalDevice, VkPhysicalDeviceProperties2*);
typedef void (*PFN_GetFeatures2)(VkPhysicalDevice, VkPhysicalDeviceFeatures2*);
typedef void (*PFN_GetMemoryProperties2)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2*);
typedef VkResult (*PFN_CreateTemplate)(VkDevice, const VkDescriptorUpdateTemplateCreateInfo*,
                                       const VkAllocationCallbacks*, VkDescriptorUpdateTemplate*);
typedef void (*PFN_UpdateWithTemplate)(VkDevice, VkDescriptorSet, VkDescriptorUpdateTemplate,
                                       const void*);
typedef void (*PFN_DestroyTemplate)(VkDevice, VkDescriptorUpdateTemplate,
                                    const VkAllocationCallbacks*);
typedef void (*PFN_GetBufferReq2)(VkDevice, const VkBufferMemoryRequirementsInfo2*,
                                  VkMemoryRequirements2*);
typedef VkResult (*PFN_BindBuffer2)(VkDevice, uint32_t, const VkBindBufferMemoryInfo*);

static void version(const char* what, uint32_t v) {
    printf("%s %u.%u.%u\n", what, v >> 22, (v >> 12) & 0x3ff, v & 0xfff);
}

static void budget(PFN_GetMemoryProperties2 get, VkPhysicalDevice pd, const char* when) {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT b = {};
    b.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    VkPhysicalDeviceMemoryProperties2 m = {};
    m.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    m.pNext = &b;
    get(pd, &m);
    for (uint32_t i = 0; i < m.memoryProperties.memoryHeapCount; i++) {
        printf("heap %u (%s): size %llu MB, budget %llu MB, usage %llu KB\n", i, when,
               (unsigned long long)(m.memoryProperties.memoryHeaps[i].size >> 20),
               (unsigned long long)(b.heapBudget[i] >> 20),
               (unsigned long long)(b.heapUsage[i] >> 10));
    }
}

int main() {
    // unbuffered: a crash still shows how far it got
    setvbuf(stdout, nullptr, _IONBF, 0);
    PFN_EnumerateInstanceVersion eiv = reinterpret_cast<PFN_EnumerateInstanceVersion>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (eiv) {
        uint32_t v = 0;
        eiv(&v);
        version("vkEnumerateInstanceVersion:", v);
    } else {
        printf("vkEnumerateInstanceVersion: missing\n");
    }

    uint32_t n = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> iext(n);
    vkEnumerateInstanceExtensionProperties(nullptr, &n, iext.data());
    printf("instance extensions:");
    for (auto& e : iext) printf(" %s", e.extensionName);
    printf("\n");

    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "kiro_vkinfo";
    app.apiVersion = (1u << 22) | (1u << 12);  // an app asking for Vulkan 1.1
    const char* inst_exts[] = {VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
    VkInstanceCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 1;
    ici.ppEnabledExtensionNames = inst_exts;
    VkInstance inst;
    CHECK(vkCreateInstance(&ici, nullptr, &inst));
    printf("instance with apiVersion 1.1: OK\n");

    n = 1;
    VkPhysicalDevice pd;
    vkEnumeratePhysicalDevices(inst, &n, &pd);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    printf("device: %s\n", props.deviceName);
    version("apiVersion:", props.apiVersion);
    version("driverVersion (raw):", props.driverVersion);

    auto get_props2 = reinterpret_cast<PFN_GetProperties2>(
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceProperties2"));
    auto get_feats2 = reinterpret_cast<PFN_GetFeatures2>(
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceFeatures2"));
    auto get_mem2 = reinterpret_cast<PFN_GetMemoryProperties2>(
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceMemoryProperties2"));
    printf("1.1 entry points: props2 %s, features2 %s, memory2 %s\n", get_props2 ? "yes" : "NO",
           get_feats2 ? "yes" : "NO", get_mem2 ? "yes" : "NO");
    if (!get_props2 || !get_feats2 || !get_mem2)
        return 1;

    VkPhysicalDeviceMaintenance3Properties m3 = {};
    m3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
    VkPhysicalDeviceSubgroupProperties sg = {};
    sg.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    sg.pNext = &m3;
    VkPhysicalDeviceProperties2 p2 = {};
    p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    p2.pNext = &sg;
    get_props2(pd, &p2);
    version("properties2 apiVersion:", p2.properties.apiVersion);
    printf("subgroup size %u ops 0x%x, maxPerSetDescriptors %u, maxMemoryAllocationSize %llu MB\n",
           sg.subgroupSize, sg.supportedOperations, m3.maxPerSetDescriptors,
           (unsigned long long)(m3.maxMemoryAllocationSize >> 20));

    VkPhysicalDeviceMemoryPriorityFeaturesEXT prio = {};
    prio.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
    VkPhysicalDeviceMultiviewFeatures mv = {};
    mv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES;
    mv.pNext = &prio;
    VkPhysicalDeviceFeatures2 f2 = {};
    f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    f2.pNext = &mv;
    get_feats2(pd, &f2);
    printf("features2: memoryPriority %u, multiview %u, shaderInt16 %u, shaderFloat64 %u\n",
           prio.memoryPriority, mv.multiview, f2.features.shaderInt16, f2.features.shaderFloat64);

    vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> dext(n);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, dext.data());
    printf("device extensions:");
    for (auto& e : dext) printf(" %s", e.extensionName);
    printf("\n");

    budget(get_mem2, pd, "before");

    // a device made the Vulkan 1.1 way: features through Features2, memory priority on
    float qp = 1.0f;
    VkDeviceQueueCreateInfo qci = {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qp;
    VkPhysicalDeviceMemoryPriorityFeaturesEXT want_prio = {};
    want_prio.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT;
    want_prio.memoryPriority = VK_TRUE;
    VkPhysicalDeviceFeatures2 want = {};
    want.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    want.pNext = &want_prio;
    const char* dev_exts[] = {VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
                              VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME};
    VkDeviceCreateInfo dci = {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &want;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 2;
    dci.ppEnabledExtensionNames = dev_exts;
    VkDevice dev;
    CHECK(vkCreateDevice(pd, &dci, nullptr, &dev));
    printf("device with Features2 + memory budget/priority: OK\n");

    // 64 MB with a priority
    VkMemoryPriorityAllocateInfoEXT mpa = {};
    mpa.sType = VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT;
    mpa.priority = 1.0f;
    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = &mpa;
    mai.allocationSize = 64u << 20;
    mai.memoryTypeIndex = 0;
    VkDeviceMemory mem;
    CHECK(vkAllocateMemory(dev, &mai, nullptr, &mem));
    budget(get_mem2, pd, "after 64 MB");

    // a uniform buffer bound with BindBufferMemory2, its requirements through the "2" query
    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = 256;
    bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkBuffer buf;
    CHECK(vkCreateBuffer(dev, &bci, nullptr, &buf));
    auto get_req2 = reinterpret_cast<PFN_GetBufferReq2>(
        vkGetDeviceProcAddr(dev, "vkGetBufferMemoryRequirements2"));
    auto bind2 = reinterpret_cast<PFN_BindBuffer2>(vkGetDeviceProcAddr(dev, "vkBindBufferMemory2"));
    VkMemoryDedicatedRequirements ded = {};
    ded.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
    VkBufferMemoryRequirementsInfo2 bri = {};
    bri.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    bri.buffer = buf;
    VkMemoryRequirements2 req = {};
    req.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    req.pNext = &ded;
    get_req2(dev, &bri, &req);
    VkBindBufferMemoryInfo bbi = {};
    bbi.sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
    bbi.buffer = buf;
    bbi.memory = mem;
    CHECK(bind2(dev, 1, &bbi));
    printf("buffer: requirements2 size %llu, dedicated %u, BindBufferMemory2 OK\n",
           (unsigned long long)req.memoryRequirements.size, ded.prefersDedicatedAllocation);

    // a descriptor set written through an update template
    VkDescriptorSetLayoutBinding lb = {};
    lb.binding = 0;
    lb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    lb.descriptorCount = 1;
    lb.stageFlags = VK_SHADER_STAGE_ALL;
    VkDescriptorSetLayoutCreateInfo lci = {};
    lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    lci.bindingCount = 1;
    lci.pBindings = &lb;
    VkDescriptorSetLayout layout;
    CHECK(vkCreateDescriptorSetLayout(dev, &lci, nullptr, &layout));
    VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    VkDescriptorPoolCreateInfo pci = {};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1;
    pci.poolSizeCount = 1;
    pci.pPoolSizes = &ps;
    VkDescriptorPool pool;
    CHECK(vkCreateDescriptorPool(dev, &pci, nullptr, &pool));
    VkDescriptorSetAllocateInfo dai = {};
    dai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dai.descriptorPool = pool;
    dai.descriptorSetCount = 1;
    dai.pSetLayouts = &layout;
    VkDescriptorSet set;
    CHECK(vkAllocateDescriptorSets(dev, &dai, &set));
    auto create_t = reinterpret_cast<PFN_CreateTemplate>(
        vkGetDeviceProcAddr(dev, "vkCreateDescriptorUpdateTemplate"));
    auto update_t = reinterpret_cast<PFN_UpdateWithTemplate>(
        vkGetDeviceProcAddr(dev, "vkUpdateDescriptorSetWithTemplate"));
    auto destroy_t = reinterpret_cast<PFN_DestroyTemplate>(
        vkGetDeviceProcAddr(dev, "vkDestroyDescriptorUpdateTemplate"));
    VkDescriptorUpdateTemplateEntry te = {};
    te.descriptorCount = 1;
    te.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    te.offset = 0;
    te.stride = sizeof(VkDescriptorBufferInfo);
    VkDescriptorUpdateTemplateCreateInfo tci = {};
    tci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
    tci.descriptorUpdateEntryCount = 1;
    tci.pDescriptorUpdateEntries = &te;
    tci.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
    tci.descriptorSetLayout = layout;
    VkDescriptorUpdateTemplate tmpl;
    CHECK(create_t(dev, &tci, nullptr, &tmpl));
    VkDescriptorBufferInfo dbi = {buf, 0, 256};
    update_t(dev, set, tmpl, &dbi);
    destroy_t(dev, tmpl, nullptr);
    printf("descriptor update template: OK\n");

    vkDestroyDescriptorPool(dev, pool, nullptr);
    vkDestroyDescriptorSetLayout(dev, layout, nullptr);
    vkDestroyBuffer(dev, buf, nullptr);
    vkFreeMemory(dev, mem, nullptr);
    budget(get_mem2, pd, "after free");
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    printf("ALL OK\n");
    return 0;
}
