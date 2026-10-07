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

// KiroOS test tool (not in the ROM): a Vulkan swapchain on a real surface through the system
// loader, as an app draws: 120 frames cleared to changing colors in a 512x512 square on top.

#include <stdio.h>
#include <time.h>
#include <vector>

#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>
#include <ui/PixelFormat.h>

#include <vulkan/vulkan.h>

using namespace android;

#define CHECK(x)                                                \
    do {                                                        \
        VkResult r_ = (x);                                      \
        if (r_ != VK_SUCCESS && r_ != VK_SUBOPTIMAL_KHR) {      \
            printf("FAIL %s = %d\n", #x, r_);                   \
            return 1;                                           \
        }                                                       \
    } while (0)

static double now_ms() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();
    const uint32_t size = 512;
    sp<SurfaceControl> sc = client->createSurface(String8("kiro_vkswap"), size, size,
                                                  PIXEL_FORMAT_RGBA_8888, 0);
    if (sc == nullptr || !sc->isValid()) {
        printf("FAIL surface\n");
        return 1;
    }
    SurfaceComposerClient::openGlobalTransaction();
    sc->setLayer(0x7FFFFFFF);
    sc->show();
    SurfaceComposerClient::closeGlobalTransaction();
    sp<Surface> surface = sc->getSurface();
    ANativeWindow* window = surface.get();

    const char* inst_exts[] = {VK_KHR_SURFACE_EXTENSION_NAME,
                               VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = (1u << 22) | (1u << 12);
    VkInstanceCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = inst_exts;
    VkInstance inst;
    CHECK(vkCreateInstance(&ici, nullptr, &inst));
    uint32_t n = 1;
    VkPhysicalDevice pd;
    vkEnumeratePhysicalDevices(inst, &n, &pd);

    VkAndroidSurfaceCreateInfoKHR asci = {};
    asci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    asci.window = window;
    VkSurfaceKHR vsurface;
    CHECK(vkCreateAndroidSurfaceKHR(inst, &asci, nullptr, &vsurface));

    float qp = 1.0f;
    VkDeviceQueueCreateInfo qci = {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qp;
    const char* dev_exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo dci = {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = dev_exts;
    VkDevice dev;
    CHECK(vkCreateDevice(pd, &dci, nullptr, &dev));
    VkQueue queue;
    vkGetDeviceQueue(dev, 0, 0, &queue);

    VkSurfaceCapabilitiesKHR caps;
    CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, vsurface, &caps));
    n = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, vsurface, &n, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(n);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pd, vsurface, &n, formats.data());
    printf("surface: %u formats, images %u-%u, usage 0x%x\n", n, caps.minImageCount,
           caps.maxImageCount, caps.supportedUsageFlags);

    VkSwapchainCreateInfoKHR sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = vsurface;
    sci.minImageCount = caps.minImageCount + 1;
    sci.imageFormat = formats[0].format;
    sci.imageColorSpace = formats[0].colorSpace;
    sci.imageExtent = {size, size};
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    VkSwapchainKHR swapchain;
    CHECK(vkCreateSwapchainKHR(dev, &sci, nullptr, &swapchain));
    n = 0;
    vkGetSwapchainImagesKHR(dev, swapchain, &n, nullptr);
    std::vector<VkImage> images(n);
    vkGetSwapchainImagesKHR(dev, swapchain, &n, images.data());
    printf("swapchain: %u images\n", n);

    VkCommandPoolCreateInfo cpci = {};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool;
    CHECK(vkCreateCommandPool(dev, &cpci, nullptr, &pool));
    VkCommandBufferAllocateInfo cbai = {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = pool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    CHECK(vkAllocateCommandBuffers(dev, &cbai, &cmd));
    VkSemaphoreCreateInfo semci = {};
    semci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore acquired, rendered;
    CHECK(vkCreateSemaphore(dev, &semci, nullptr, &acquired));
    CHECK(vkCreateSemaphore(dev, &semci, nullptr, &rendered));
    VkFenceCreateInfo fci = {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence;
    CHECK(vkCreateFence(dev, &fci, nullptr, &fence));

    const int frames = 120;
    const double t0 = now_ms();
    for (int f = 0; f < frames; f++) {
        uint32_t index;
        CHECK(vkAcquireNextImageKHR(dev, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE,
                                    &index));
        VkCommandBufferBeginInfo bi = {};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        CHECK(vkBeginCommandBuffer(cmd, &bi));
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageMemoryBarrier b = {};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = images[index];
        b.subresourceRange = range;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
        VkClearColorValue color = {};
        color.float32[0] = (f % 60) / 60.0f;
        color.float32[1] = 0.53f;
        color.float32[2] = 0.90f;
        color.float32[3] = 1.0f;
        vkCmdClearColorImage(cmd, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color,
                             1, &range);
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &b);
        CHECK(vkEndCommandBuffer(cmd));
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo si = {};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.waitSemaphoreCount = 1;
        si.pWaitSemaphores = &acquired;
        si.pWaitDstStageMask = &wait_stage;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmd;
        si.signalSemaphoreCount = 1;
        si.pSignalSemaphores = &rendered;
        CHECK(vkQueueSubmit(queue, 1, &si, fence));
        VkPresentInfoKHR pi = {};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &rendered;
        pi.swapchainCount = 1;
        pi.pSwapchains = &swapchain;
        pi.pImageIndices = &index;
        CHECK(vkQueuePresentKHR(queue, &pi));
        CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));
        CHECK(vkResetFences(dev, 1, &fence));
    }
    const double ms = now_ms() - t0;
    printf("presented %d frames in %.0f ms (%.1f fps)\n", frames, ms, frames * 1000.0 / ms);

    vkDeviceWaitIdle(dev);
    vkDestroyFence(dev, fence, nullptr);
    vkDestroySemaphore(dev, rendered, nullptr);
    vkDestroySemaphore(dev, acquired, nullptr);
    vkDestroyCommandPool(dev, pool, nullptr);
    vkDestroySwapchainKHR(dev, swapchain, nullptr);
    vkDestroyDevice(dev, nullptr);
    vkDestroySurfaceKHR(inst, vsurface, nullptr);
    vkDestroyInstance(inst, nullptr);
    sc->clear();
    printf("ALL OK\n");
    return 0;
}
