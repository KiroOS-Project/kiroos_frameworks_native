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

// KiroOS: the Vulkan 1.1 types (and VK_EXT_memory_budget / VK_EXT_memory_priority) the Android 7
// header (1.0.13) does not have, for the loader's Vulkan 1.1 layer (kiro_vk11.cpp). Values and
// layouts as in the Khronos Vulkan-Headers (vulkan_core.h).

#ifndef KIRO_VK11_TYPES_H
#define KIRO_VK11_TYPES_H

#include <vulkan/vulkan.h>

#ifndef VK_VERSION_1_1

#define KIRO_STYPE(v) static_cast<VkStructureType>(v)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES KIRO_STYPE(1000094000)
#define VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO KIRO_STYPE(1000157000)
#define VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO KIRO_STYPE(1000157001)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES KIRO_STYPE(1000083000)
#define VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS KIRO_STYPE(1000127000)
#define VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO KIRO_STYPE(1000127001)
#define VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO KIRO_STYPE(1000060000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES KIRO_STYPE(1000070000)
#define VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO KIRO_STYPE(1000070001)
#define VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2 KIRO_STYPE(1000146000)
#define VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2 KIRO_STYPE(1000146001)
#define VK_STRUCTURE_TYPE_IMAGE_SPARSE_MEMORY_REQUIREMENTS_INFO_2 KIRO_STYPE(1000146002)
#define VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2 KIRO_STYPE(1000146003)
#define VK_STRUCTURE_TYPE_SPARSE_IMAGE_MEMORY_REQUIREMENTS_2 KIRO_STYPE(1000146004)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 KIRO_STYPE(1000059000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 KIRO_STYPE(1000059001)
#define VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 KIRO_STYPE(1000059002)
#define VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2 KIRO_STYPE(1000059003)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2 KIRO_STYPE(1000059004)
#define VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 KIRO_STYPE(1000059005)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 KIRO_STYPE(1000059006)
#define VK_STRUCTURE_TYPE_SPARSE_IMAGE_FORMAT_PROPERTIES_2 KIRO_STYPE(1000059007)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SPARSE_IMAGE_FORMAT_INFO_2 KIRO_STYPE(1000059008)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_POINT_CLIPPING_PROPERTIES KIRO_STYPE(1000117000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES KIRO_STYPE(1000053001)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_PROPERTIES KIRO_STYPE(1000053002)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VARIABLE_POINTERS_FEATURES KIRO_STYPE(1000120000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES KIRO_STYPE(1000145001)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_PROPERTIES KIRO_STYPE(1000145002)
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 KIRO_STYPE(1000145003)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES KIRO_STYPE(1000156004)
#define VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_IMAGE_FORMAT_PROPERTIES KIRO_STYPE(1000156005)
#define VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO KIRO_STYPE(1000085000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO KIRO_STYPE(1000071000)
#define VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES KIRO_STYPE(1000071001)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO KIRO_STYPE(1000071002)
#define VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES KIRO_STYPE(1000071003)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES KIRO_STYPE(1000071004)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO KIRO_STYPE(1000112000)
#define VK_STRUCTURE_TYPE_EXTERNAL_FENCE_PROPERTIES KIRO_STYPE(1000112001)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO KIRO_STYPE(1000076000)
#define VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES KIRO_STYPE(1000076001)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES KIRO_STYPE(1000168000)
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_SUPPORT KIRO_STYPE(1000168001)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES KIRO_STYPE(1000063000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT KIRO_STYPE(1000237000)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT KIRO_STYPE(1000238000)
#define VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT KIRO_STYPE(1000238001)

#define VK_ERROR_FEATURE_NOT_PRESENT_KIRO static_cast<VkResult>(-8)
#define VK_MAX_DEVICE_GROUP_SIZE 32
#define VK_LUID_SIZE 8
#define VK_SUBGROUP_FEATURE_BASIC_BIT 0x00000001
#define VK_POINT_CLIPPING_BEHAVIOR_ALL_CLIP_PLANES 0

#define VK_EXT_MEMORY_BUDGET_EXTENSION_NAME "VK_EXT_memory_budget"
#define VK_EXT_MEMORY_BUDGET_SPEC_VERSION 1
#define VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME "VK_EXT_memory_priority"
#define VK_EXT_MEMORY_PRIORITY_SPEC_VERSION 1
#define VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME "VK_KHR_get_physical_device_properties2"
#define VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_SPEC_VERSION 2

// a non-dispatchable handle of the layer's own
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorUpdateTemplate)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSamplerYcbcrConversion)

typedef struct VkPhysicalDeviceFeatures2 {
    VkStructureType sType;
    void* pNext;
    VkPhysicalDeviceFeatures features;
} VkPhysicalDeviceFeatures2;

typedef struct VkPhysicalDeviceProperties2 {
    VkStructureType sType;
    void* pNext;
    VkPhysicalDeviceProperties properties;
} VkPhysicalDeviceProperties2;

typedef struct VkFormatProperties2 {
    VkStructureType sType;
    void* pNext;
    VkFormatProperties formatProperties;
} VkFormatProperties2;

typedef struct VkImageFormatProperties2 {
    VkStructureType sType;
    void* pNext;
    VkImageFormatProperties imageFormatProperties;
} VkImageFormatProperties2;

typedef struct VkPhysicalDeviceImageFormatInfo2 {
    VkStructureType sType;
    const void* pNext;
    VkFormat format;
    VkImageType type;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkImageCreateFlags flags;
} VkPhysicalDeviceImageFormatInfo2;

typedef struct VkQueueFamilyProperties2 {
    VkStructureType sType;
    void* pNext;
    VkQueueFamilyProperties queueFamilyProperties;
} VkQueueFamilyProperties2;

typedef struct VkPhysicalDeviceMemoryProperties2 {
    VkStructureType sType;
    void* pNext;
    VkPhysicalDeviceMemoryProperties memoryProperties;
} VkPhysicalDeviceMemoryProperties2;

typedef struct VkSparseImageFormatProperties2 {
    VkStructureType sType;
    void* pNext;
    VkSparseImageFormatProperties properties;
} VkSparseImageFormatProperties2;

typedef struct VkPhysicalDeviceSparseImageFormatInfo2 {
    VkStructureType sType;
    const void* pNext;
    VkFormat format;
    VkImageType type;
    VkSampleCountFlagBits samples;
    VkImageUsageFlags usage;
    VkImageTiling tiling;
} VkPhysicalDeviceSparseImageFormatInfo2;

typedef struct VkBindBufferMemoryInfo {
    VkStructureType sType;
    const void* pNext;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize memoryOffset;
} VkBindBufferMemoryInfo;

typedef struct VkBindImageMemoryInfo {
    VkStructureType sType;
    const void* pNext;
    VkImage image;
    VkDeviceMemory memory;
    VkDeviceSize memoryOffset;
} VkBindImageMemoryInfo;

typedef struct VkBufferMemoryRequirementsInfo2 {
    VkStructureType sType;
    const void* pNext;
    VkBuffer buffer;
} VkBufferMemoryRequirementsInfo2;

typedef struct VkImageMemoryRequirementsInfo2 {
    VkStructureType sType;
    const void* pNext;
    VkImage image;
} VkImageMemoryRequirementsInfo2;

typedef struct VkImageSparseMemoryRequirementsInfo2 {
    VkStructureType sType;
    const void* pNext;
    VkImage image;
} VkImageSparseMemoryRequirementsInfo2;

typedef struct VkMemoryRequirements2 {
    VkStructureType sType;
    void* pNext;
    VkMemoryRequirements memoryRequirements;
} VkMemoryRequirements2;

typedef struct VkSparseImageMemoryRequirements2 {
    VkStructureType sType;
    void* pNext;
    VkSparseImageMemoryRequirements memoryRequirements;
} VkSparseImageMemoryRequirements2;

typedef struct VkMemoryDedicatedRequirements {
    VkStructureType sType;
    void* pNext;
    VkBool32 prefersDedicatedAllocation;
    VkBool32 requiresDedicatedAllocation;
} VkMemoryDedicatedRequirements;

typedef struct VkDeviceQueueInfo2 {
    VkStructureType sType;
    const void* pNext;
    VkDeviceQueueCreateFlags flags;
    uint32_t queueFamilyIndex;
    uint32_t queueIndex;
} VkDeviceQueueInfo2;

typedef VkFlags VkDescriptorUpdateTemplateCreateFlags;
typedef enum VkDescriptorUpdateTemplateType {
    VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET = 0,
    VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkDescriptorUpdateTemplateType;

typedef struct VkDescriptorUpdateTemplateEntry {
    uint32_t dstBinding;
    uint32_t dstArrayElement;
    uint32_t descriptorCount;
    VkDescriptorType descriptorType;
    size_t offset;
    size_t stride;
} VkDescriptorUpdateTemplateEntry;

typedef struct VkDescriptorUpdateTemplateCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkDescriptorUpdateTemplateCreateFlags flags;
    uint32_t descriptorUpdateEntryCount;
    const VkDescriptorUpdateTemplateEntry* pDescriptorUpdateEntries;
    VkDescriptorUpdateTemplateType templateType;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineBindPoint pipelineBindPoint;
    VkPipelineLayout pipelineLayout;
    uint32_t set;
} VkDescriptorUpdateTemplateCreateInfo;

typedef struct VkDescriptorSetLayoutSupport {
    VkStructureType sType;
    void* pNext;
    VkBool32 supported;
} VkDescriptorSetLayoutSupport;

typedef struct VkPhysicalDeviceIDProperties {
    VkStructureType sType;
    void* pNext;
    uint8_t deviceUUID[VK_UUID_SIZE];
    uint8_t driverUUID[VK_UUID_SIZE];
    uint8_t deviceLUID[VK_LUID_SIZE];
    uint32_t deviceNodeMask;
    VkBool32 deviceLUIDValid;
} VkPhysicalDeviceIDProperties;

typedef VkFlags VkSubgroupFeatureFlags;
typedef struct VkPhysicalDeviceSubgroupProperties {
    VkStructureType sType;
    void* pNext;
    uint32_t subgroupSize;
    VkShaderStageFlags supportedStages;
    VkSubgroupFeatureFlags supportedOperations;
    VkBool32 quadOperationsInAllStages;
} VkPhysicalDeviceSubgroupProperties;

typedef struct VkPhysicalDevicePointClippingProperties {
    VkStructureType sType;
    void* pNext;
    uint32_t pointClippingBehavior;  // VkPointClippingBehavior
} VkPhysicalDevicePointClippingProperties;

typedef struct VkPhysicalDeviceMultiviewProperties {
    VkStructureType sType;
    void* pNext;
    uint32_t maxMultiviewViewCount;
    uint32_t maxMultiviewInstanceIndex;
} VkPhysicalDeviceMultiviewProperties;

typedef struct VkPhysicalDeviceProtectedMemoryProperties {
    VkStructureType sType;
    void* pNext;
    VkBool32 protectedNoFault;
} VkPhysicalDeviceProtectedMemoryProperties;

typedef struct VkPhysicalDeviceMaintenance3Properties {
    VkStructureType sType;
    void* pNext;
    uint32_t maxPerSetDescriptors;
    VkDeviceSize maxMemoryAllocationSize;
} VkPhysicalDeviceMaintenance3Properties;

typedef struct VkPhysicalDevice16BitStorageFeatures {
    VkStructureType sType;
    void* pNext;
    VkBool32 storageBuffer16BitAccess;
    VkBool32 uniformAndStorageBuffer16BitAccess;
    VkBool32 storagePushConstant16;
    VkBool32 storageInputOutput16;
} VkPhysicalDevice16BitStorageFeatures;

typedef struct VkPhysicalDeviceMultiviewFeatures {
    VkStructureType sType;
    void* pNext;
    VkBool32 multiview;
    VkBool32 multiviewGeometryShader;
    VkBool32 multiviewTessellationShader;
} VkPhysicalDeviceMultiviewFeatures;

typedef struct VkPhysicalDeviceVariablePointersFeatures {
    VkStructureType sType;
    void* pNext;
    VkBool32 variablePointersStorageBuffer;
    VkBool32 variablePointers;
} VkPhysicalDeviceVariablePointersFeatures;

typedef struct VkPhysicalDeviceProtectedMemoryFeatures {
    VkStructureType sType;
    void* pNext;
    VkBool32 protectedMemory;
} VkPhysicalDeviceProtectedMemoryFeatures;

typedef struct VkPhysicalDeviceSamplerYcbcrConversionFeatures {
    VkStructureType sType;
    void* pNext;
    VkBool32 samplerYcbcrConversion;
} VkPhysicalDeviceSamplerYcbcrConversionFeatures;

typedef struct VkPhysicalDeviceShaderDrawParametersFeatures {
    VkStructureType sType;
    void* pNext;
    VkBool32 shaderDrawParameters;
} VkPhysicalDeviceShaderDrawParametersFeatures;

typedef struct VkPhysicalDeviceMemoryBudgetPropertiesEXT {
    VkStructureType sType;
    void* pNext;
    VkDeviceSize heapBudget[VK_MAX_MEMORY_HEAPS];
    VkDeviceSize heapUsage[VK_MAX_MEMORY_HEAPS];
} VkPhysicalDeviceMemoryBudgetPropertiesEXT;

typedef struct VkPhysicalDeviceMemoryPriorityFeaturesEXT {
    VkStructureType sType;
    void* pNext;
    VkBool32 memoryPriority;
} VkPhysicalDeviceMemoryPriorityFeaturesEXT;

typedef struct VkMemoryPriorityAllocateInfoEXT {
    VkStructureType sType;
    const void* pNext;
    float priority;
} VkMemoryPriorityAllocateInfoEXT;

typedef struct VkPhysicalDeviceGroupProperties {
    VkStructureType sType;
    void* pNext;
    uint32_t physicalDeviceCount;
    VkPhysicalDevice physicalDevices[VK_MAX_DEVICE_GROUP_SIZE];
    VkBool32 subsetAllocation;
} VkPhysicalDeviceGroupProperties;

typedef struct VkExternalMemoryProperties {
    VkFlags externalMemoryFeatures;
    VkFlags exportFromImportedHandleTypes;
    VkFlags compatibleHandleTypes;
} VkExternalMemoryProperties;

typedef struct VkPhysicalDeviceExternalBufferInfo {
    VkStructureType sType;
    const void* pNext;
    VkBufferCreateFlags flags;
    VkBufferUsageFlags usage;
    uint32_t handleType;
} VkPhysicalDeviceExternalBufferInfo;

typedef struct VkExternalBufferProperties {
    VkStructureType sType;
    void* pNext;
    VkExternalMemoryProperties externalMemoryProperties;
} VkExternalBufferProperties;

typedef struct VkExternalImageFormatProperties {
    VkStructureType sType;
    void* pNext;
    VkExternalMemoryProperties externalMemoryProperties;
} VkExternalImageFormatProperties;

typedef struct VkPhysicalDeviceExternalFenceInfo {
    VkStructureType sType;
    const void* pNext;
    uint32_t handleType;
} VkPhysicalDeviceExternalFenceInfo;

typedef struct VkExternalFenceProperties {
    VkStructureType sType;
    void* pNext;
    VkFlags exportFromImportedHandleTypes;
    VkFlags compatibleHandleTypes;
    VkFlags externalFenceFeatures;
} VkExternalFenceProperties;

typedef struct VkPhysicalDeviceExternalSemaphoreInfo {
    VkStructureType sType;
    const void* pNext;
    uint32_t handleType;
} VkPhysicalDeviceExternalSemaphoreInfo;

typedef struct VkExternalSemaphoreProperties {
    VkStructureType sType;
    void* pNext;
    VkFlags exportFromImportedHandleTypes;
    VkFlags compatibleHandleTypes;
    VkFlags externalSemaphoreFeatures;
} VkExternalSemaphoreProperties;

typedef struct VkSamplerYcbcrConversionImageFormatProperties {
    VkStructureType sType;
    void* pNext;
    uint32_t combinedImageSamplerDescriptorCount;
} VkSamplerYcbcrConversionImageFormatProperties;

#endif  // VK_VERSION_1_1

#endif  // KIRO_VK11_TYPES_H
