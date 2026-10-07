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

// KiroOS Vulkan 1.1 layer, inside the loader.
//
// The Mali-T760 driver of the Galaxy S6 family is Vulkan 1.0.26 and closed. The loader reports
// Vulkan 1.1.130 and provides the Vulkan 1.1 core commands on top of it: the "2" queries, bind /
// requirements 2, device groups of one device, descriptor update templates, maintenance 1-3,
// external-handle queries (none). The optional 1.1 features the hardware does not have stay off
// (16-bit storage, multiview, variable pointers, protected memory, YCbCr, draw parameters), and so
// do FP16 / INT8 arithmetic. It adds VK_EXT_memory_budget (usage tracked by the loader, budget from
// the free memory) and VK_EXT_memory_priority (accepted; the driver has no priorities), and keeps
// the pipeline cache of apps that do not keep one on the app's storage, so the shaders of a game
// are not compiled again at every start. persist.sys.kiroos.vk11=0 turns it all off.

#ifndef LIBVULKAN_KIRO_VK11_H
#define LIBVULKAN_KIRO_VK11_H

#include <vulkan/vulkan.h>

namespace vulkan {
namespace kiro {

bool Enabled();

// the loader's proc address lookups ask here before the driver
PFN_vkVoidFunction GetGlobalProcAddr(const char* name);
PFN_vkVoidFunction GetInstanceProcAddr(const char* name);
PFN_vkVoidFunction GetDeviceProcAddr(const char* name);

// api layer
void PatchProperties(VkPhysicalDeviceProperties* properties);
void OnDeviceCreated(VkPhysicalDevice physical_dev, VkDevice dev);
void OnDeviceDestroyed(VkDevice dev);
const VkMemoryAllocateInfo* SanitizeAllocateInfo(const VkMemoryAllocateInfo* info,
                                                 VkMemoryAllocateInfo* storage);
void OnAllocateMemory(VkDevice dev, const VkMemoryAllocateInfo* info, VkDeviceMemory memory);
void OnFreeMemory(VkDevice dev, VkDeviceMemory memory);
// returns the create info to use (seeded with the stored cache when the app gives no data);
// *seeded tells OnPipelineCacheCreated whether the cache is one the loader keeps
const VkPipelineCacheCreateInfo* SeedPipelineCache(VkDevice dev,
                                                   const VkPipelineCacheCreateInfo* info,
                                                   VkPipelineCacheCreateInfo* storage,
                                                   bool* seeded);
void OnPipelineCacheCreated(VkDevice dev, VkPipelineCache cache, bool seeded);
void OnPipelinesCreated(VkDevice dev, VkPipelineCache cache);
void OnPipelineCacheDestroy(VkDevice dev, VkPipelineCache cache);

// driver layer: what the 1.0 driver must not see
void SanitizeInstanceCreateInfo(VkInstanceCreateInfo* info, VkApplicationInfo* app_storage);
VkResult SanitizeDeviceCreateInfo(VkDeviceCreateInfo* info,
                                  VkPhysicalDeviceFeatures* features_storage);
uint32_t ExtraDeviceExtensionCount();
const VkExtensionProperties* ExtraDeviceExtensions();
uint32_t ExtraInstanceExtensionCount();
const VkExtensionProperties* ExtraInstanceExtensions();

}  // namespace kiro
}  // namespace vulkan

#endif  // LIBVULKAN_KIRO_VK11_H
